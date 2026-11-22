"""Compare audio from the two built targets, including occupied spectral bands."""

from pathlib import Path
import subprocess, sys, tempfile, wave
import numpy as np

native, dist = map(Path, sys.argv[1:])
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
    tmp = Path(d)
    for score in sorted((root / "content/performances").glob("*.score")):
        name = score.stem
        subprocess.run(
            [
                native,
                root / "content/packs/blue.factory",
                name,
                tmp / "native.wav",
                score,
            ],
            check=True,
        )
        subprocess.run(
            [
                "node",
                root / "tests/wasm_render.cjs",
                dist,
                name,
                score,
                tmp / "wasm.f32",
            ],
            check=True,
        )
        with wave.open(str(tmp / "native.wav")) as wav:
            a = (
                np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").astype(
                    float
                )
                / 32767
            )
        b = np.fromfile(tmp / "wasm.f32", dtype="<f4").astype(float)
        assert len(a) == len(b) and np.isfinite(b).all()
        rms = lambda x: np.sqrt(np.mean(x * x))
        rms_db = abs(20 * np.log10(rms(a) / rms(b)))
        assert rms_db <= 0.5, (name, rms_db)
        if name == "Calibration":
            assert np.max(abs(a - b)) <= 0.0001

        def bands(x):
            mono = x.reshape(-1, 2).mean(axis=1)
            windows = mono[: len(mono) // 4096 * 4096].reshape(-1, 4096)
            spec = abs(np.fft.rfft(windows * np.hanning(4096), axis=1)) / 1024
            return np.array(
                [
                    rms(spec[:, a:b])
                    for a, b in zip([1, 8, 32, 128, 512], [8, 32, 128, 512, 2049])
                ]
            )

        aa, bb = bands(a), bands(b)
        mask = aa > 1e-3
        diff = np.max(abs(20 * np.log10(aa[mask] / bb[mask]))) if mask.any() else 0
        assert diff <= 1, (name, diff)
        print(
            f"{name}: RMS delta {rms_db:.4f} dB, largest occupied band delta {diff:.4f} dB"
        )

# A dry analytic patch carries an exact sample reference. Each mutation is applied
# only to the browser-side input, exercising the same comparison used for parity.
import json

with tempfile.TemporaryDirectory() as temp:
    tmp = Path(temp)
    fixture = {
        "format": 1,
        "attack": 0,
        "release": 0,
        "sync": True,
        "bpm": 120,
        "loop": 0,
        "envelope": [{"time": 0, "value": 0.25}, {"time": 0.1, "value": 1}],
        "lanes": [
            {"source": "sine", "gain": 0.7},
            {"source": "ring", "gain": 0.2, "ratio": 0.5},
        ],
    }
    (tmp / "pack.json").write_text(
        json.dumps(
            {
                "format": 1,
                "luaApi": 1,
                "script": "voice.lua",
                "presets": {"Clock": "patch.json"},
            }
        )
    )
    (tmp / "voice.lua").write_text("")

    def put(doc, events):
        (tmp / "patch.json").write_text(json.dumps(doc))
        (tmp / "events.score").write_text(
            "48000 12000\n137 0 69 .8\n" + events + "\n10001 1 69 0\n"
        )

    def wasm():
        output = tmp / "wasm.f32"
        subprocess.run(
            [
                "node",
                root / "tests/wasm_render.cjs",
                dist,
                "Clock",
                tmp / "events.score",
                output,
                tmp,
            ],
            check=True,
        )
        return np.fromfile(output, dtype="<f4")

    put(fixture, "4800 9 0 90")
    subprocess.run(
        [native, tmp, "Clock", tmp / "reference.wav", tmp / "events.score"], check=True
    )
    with wave.open(str(tmp / "reference.wav")) as f:
        ref = np.frombuffer(f.readframes(f.getnframes()), dtype="<i2") / 32767.0

    def aligned(x):
        active = np.flatnonzero(abs(x) > 1e-4)
        return (
            active.size
            and np.array_equal(
                active[[0, -1]], np.flatnonzero(abs(ref) > 1e-4)[[0, -1]]
            )
            and np.max(abs(x - ref)) < 1e-4
        )

    assert aligned(wasm()), "aligned score differed at a sample boundary"
    for label, change, events in [
        ("detuned", fixture, "138 3 0 .2\n4800 9 0 90"),
        (
            "muted lane",
            {
                **fixture,
                "lanes": [fixture["lanes"][0], {**fixture["lanes"][1], "mute": True}],
            },
            "4800 9 0 90",
        ),
        ("late tempo", fixture, "4801 9 0 90"),
    ]:
        put(change, events)
        assert not aligned(wasm()), f"missed {label} mutation"
print("Detected detuning, a muted lane and a one-frame tempo error.")
