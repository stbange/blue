"""Exercise the distributed Tideline pack with the native and Wasm engines."""

from pathlib import Path
import filecmp
import subprocess
import sys
import tempfile
import wave

import numpy as np


def band_levels(audio):
    mono = audio.mean(axis=1)
    blocks = mono[: len(mono) // 4096 * 4096].reshape(-1, 4096)
    power = abs(np.fft.rfft(blocks * np.hanning(4096), axis=1) / 1024) ** 2
    edges = (1, 8, 32, 128, 512, 2049)
    return np.array([power[:, lo:hi].mean() for lo, hi in zip(edges, edges[1:])])


def brightness_range(audio, rate):
    mono = audio.mean(axis=1)
    hz = np.fft.rfftfreq(2 * rate, 1 / rate)
    body = (hz >= 90) & (hz < 1000)
    levels = []
    for start in (5, 13, 22, 32, 43, 49):
        block = mono[start * rate : (start + 2) * rate]
        power = abs(np.fft.rfft(block * np.hanning(len(block)))) ** 2
        levels.append(10 * np.log10(power[hz >= 1200].sum() / power[body].sum()))
    return np.ptp(levels)


def main():
    native, web = map(Path, sys.argv[1:])
    study = Path(__file__).resolve().parent
    score = study / "held-chord.score"
    with tempfile.TemporaryDirectory() as tmp:
        temp = Path(tmp)
        library = temp / "library"
        subprocess.run(
            [native / "blue-pack", "import", study / "Tideline.zip", library],
            check=True,
        )
        (pack,) = library.glob("packs/*")
        for source, name in [(study / "pack", "loose"), (pack, "imported")]:
            subprocess.run(
                [
                    native / "blue-render",
                    source,
                    "Tideline",
                    temp / f"{name}.wav",
                    score,
                ],
                check=True,
            )
        assert filecmp.cmp(temp / "imported.wav", temp / "loose.wav", shallow=False)
        subprocess.run(
            [
                "node",
                study.parents[1] / "tests/wasm_render.cjs",
                web,
                "Tideline",
                score,
                temp / "web.f32",
                pack,
            ],
            check=True,
        )
        with wave.open(str(temp / "imported.wav")) as wav:
            rate = wav.getframerate()
            assert (rate, wav.getnchannels(), wav.getsampwidth(), wav.getnframes()) == (
                48000,
                2,
                2,
                62 * 48000,
            )
            desktop = (
                np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").reshape(
                    -1, 2
                )
                / 32767.0
            )
        browser = (
            np.fromfile(temp / "web.f32", dtype="<f4").reshape(-1, 2).astype(float)
        )
        assert desktop.shape == browser.shape and np.isfinite(browser).all()
        assert (
            0.01 < abs(desktop).max() < 1 and abs(desktop[-rate // 4 :]).max() < 0.003
        )
        level_diff = abs(10 * np.log10(np.mean(desktop**2) / np.mean(browser**2)))
        a, b = band_levels(desktop), band_levels(browser)
        occupied = a > 1e-6
        spectral_diff = max(abs(10 * np.log10(a[occupied] / b[occupied])))
        movement = brightness_range(desktop, rate)
        print(
            f"Native/Wasm RMS delta {level_diff:.4f} dB, band delta {spectral_diff:.4f} dB, held-chord brightness range {movement:.1f} dB"
        )
        assert level_diff <= 0.5 and spectral_diff <= 1 and movement >= 6


if __name__ == "__main__":
    main()
