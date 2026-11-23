"""Import the downloadable banks and render each installed preset on native and Wasm."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import wave

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
BANKS = ("formants", "littoral", "nocturnes", "signals")
EXAMPLES = ROOT / "examples/preset-library"


def inventory(root):
    return {p.relative_to(root): p.read_bytes() for p in root.rglob("*") if p.is_file()}


def band_power(audio):
    mono = audio.mean(axis=1)
    blocks = mono[: len(mono) // 4096 * 4096].reshape(-1, 4096)
    power = abs(np.fft.rfft(blocks * np.hanning(4096), axis=1) / 1024) ** 2
    edges = (1, 8, 32, 128, 512, 2049)
    return np.array([power[:, lo:hi].mean() for lo, hi in zip(edges, edges[1:])])


def check_long_hold(render, pack, name, tmp):
    rate = 24000
    score = tmp / "sustain.score"
    notes = (45, 52, 60)
    score.write_text(
        f"{rate} {50 * rate}\n"
        + "".join(f"{i * 1200} 0 {key} 0.75\n" for i, key in enumerate(notes))
        + "".join(f"{36 * rate} 1 {key} 0\n" for key in notes)
    )
    path = tmp / "sustain.wav"
    subprocess.run([render, pack, name, path, score], check=True)
    with wave.open(str(path)) as wav:
        audio = (
            np.frombuffer(wav.readframes(wav.getnframes()), "<i2").reshape(-1, 2)
            / 32767.0
        )
    # Compare the relative spectral shape after the attack has completed.
    shapes = []
    for start in (8, 14, 20, 26, 32):
        power = band_power(audio[start * rate : (start + 2) * rate])
        shapes.append(10 * np.log10(np.maximum(power[:3], 1e-12) / power[:3].sum()))
    movement = np.ptp(shapes, axis=0).max()
    assert movement > 1.5 and abs(audio).max() < 0.98, (name, movement)
    assert abs(audio[-rate:]).max() < 0.001, (name, "release failed to decay")


def check(native, web, evidence, tmp):
    index = json.loads((native / "content/index.json").read_bytes())["packs"]
    assert json.loads((web / "content/index.json").read_bytes())["packs"] == index
    assert index[:1] == ["packs/blue.factory"]
    assert "packs/blue.soundstudy.tideline" in index
    names = set(
        json.loads((native / "content/packs/blue.factory/pack.json").read_bytes())[
            "presets"
        ]
    )
    signatures = []
    for bank in BANKS:
        relative = "packs/blue." + bank
        assert relative in index, bank
        library = tmp / bank
        subprocess.run(
            [native / "blue-pack", "import", EXAMPLES / (bank + ".zip"), library],
            check=True,
        )
        assert json.loads((library / "index.json").read_bytes())["packs"] == [relative]
        imported, site = library / relative, web / "content" / relative
        assert (
            inventory(imported)
            == inventory(site)
            == inventory(native / "content" / relative)
        ), bank
        presets = json.loads((site / "pack.json").read_bytes())["presets"]
        assert len(presets) == 6 and not names.intersection(presets), bank
        names.update(presets)
        for name in presets:
            wav_path = evidence / (name + ".wav")
            raw = tmp / "web.f32"
            score = EXAMPLES / "audition.score"
            subprocess.run(
                [native / "blue-render", imported, name, wav_path, score], check=True
            )
            subprocess.run(
                ["node", ROOT / "tests/wasm_render.cjs", web, name, score, raw, site],
                check=True,
            )
            with wave.open(str(wav_path)) as stream:
                assert (
                    stream.getframerate(),
                    stream.getnchannels(),
                    stream.getnframes(),
                ) == (48000, 2, 480000)
                desktop = (
                    np.frombuffer(stream.readframes(480000), dtype="<i2").reshape(-1, 2)
                    / 32767.0
                )
            browser = np.fromfile(raw, dtype="<f4").reshape(-1, 2)
            assert desktop.shape == browser.shape and np.isfinite(browser).all(), name
            peak = np.abs(desktop).max()
            assert 0.04 < peak < 0.96 and np.abs(browser).max() < 0.96, (name, peak)
            assert np.all(np.mean(desktop[:300000] ** 2, axis=0) > 0.00002), name
            level = abs(10 * np.log10(np.mean(desktop**2) / np.mean(browser**2)))
            a, b = band_power(desktop), band_power(browser)
            occupied = a > 1e-6
            delta = max(abs(10 * np.log10(a[occupied] / b[occupied])))
            assert level <= 0.5 and delta <= 1.0, (name, level, delta)
            # Detect an accidentally duplicated preset even if its output gain changes.
            sig = desktop[:300000:16].mean(axis=1)
            sig /= np.linalg.norm(sig)
            for other, reference in signatures:
                assert abs(reference @ sig) < 0.99, (name, other)
            signatures.append((name, sig))
            if name in {"Hollow Consort", "Tidal Ink", "Polar Veil"}:
                check_long_hold(native / "blue-render", imported, name, tmp)
            print(
                f"{name}: peak {20 * np.log10(peak):.1f} dBFS, RMS delta {level:.3f} dB, band delta {delta:.3f} dB",
                flush=True,
            )


if __name__ == "__main__":
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("native", type=Path)
    cli.add_argument("web", type=Path)
    cli.add_argument("--save-renders", type=Path)
    args = cli.parse_args()
    with tempfile.TemporaryDirectory() as work:
        tmp = Path(work)
        evidence = args.save_renders or tmp
        evidence.mkdir(exist_ok=True, parents=True)
        check(args.native.resolve(), args.web.resolve(), evidence, tmp)
