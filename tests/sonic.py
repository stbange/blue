"""Check the measurable musical briefs and analytic source fixtures."""

from pathlib import Path
import json, subprocess, sys, tempfile, wave
import numpy as np

root = Path(__file__).resolve().parents[1]
exe = Path(sys.argv[1]).resolve()
factory = root / "content/packs/blue.factory"
with tempfile.TemporaryDirectory() as td:
    work = Path(td)

    def render(folder, name, score):
        subprocess.run([exe, folder, name, work / "out.wav", score], check=True)
        with wave.open(str(work / "out.wav")) as f:
            return (
                np.frombuffer(f.readframes(f.getnframes()), dtype="<i2")
                .reshape(-1, 2)
                .mean(axis=1)
                / 32767
            )

    def spectrum(x):
        return abs(np.fft.rfft(x * np.hanning(len(x))))

    def centroid(x):
        a = spectrum(x)
        return np.average(np.fft.rfftfreq(len(x), 1 / 48000), weights=a)

    for name in ["Glass Tide", "Struck Wire", "Paper Cloud", "Step Current"]:
        y = render(factory, name, root / "content/performances" / (name + ".score"))
        if name == "Glass Tide":
            ratio = centroid(y[10 * 48000 : 12 * 48000]) / centroid(
                y[48000 : 3 * 48000]
            )
            assert ratio >= 1.25, (name, ratio)
        elif name == "Struck Wire":

            def color(x):
                a = spectrum(x)
                freq = np.fft.rfftfreq(len(x), 1 / 48000)
                return np.linalg.norm(a[freq > 800]) / np.linalg.norm(
                    a[(freq > 420) & (freq < 460)]
                )

            delta = 20 * np.log10(color(y[800:4800]) / color(y[48000:52000]))
            assert delta >= 6, delta
        elif name == "Paper Cloud":
            assert np.std(y[48000:96000]) > 0.001 and np.isfinite(y).all()
        else:
            phase = np.abs(y[48000 : 48000 * 5]).reshape(-1, 24000)
            assert np.corrcoef(phase[1], phase[2])[0, 1] > 0.95
    (work / "pack.json").write_text(
        json.dumps(
            dict(
                format=1, luaApi=1, presets={"test": "patch.json"}, script="mapping.lua"
            )
        )
    )
    (work / "mapping.lua").write_text("")
    score = work / "notes.score"
    score.write_text("48000 96000\n0 0 69 1\n")
    (work / "patch.json").write_text(
        json.dumps(dict(format=1, lanes=[dict(source="ring", ratio=0.25, gain=1)]))
    )
    y = render(work, "test", score)
    fft = spectrum(y[48000:])
    assert min(fft[330], fft[550]) > 100 * fft[440]
    lanes = [
        dict(source="sine", gain=0.12, pan=-0.5),
        dict(source="fm", gain=0.1, pan=0.3),
        dict(source="ring", gain=0.08, pan=0.8),
    ]

    def mix(layers):
        (work / "patch.json").write_text(json.dumps(dict(format=1, lanes=layers)))
        return render(work, "test", score)

    combined = mix(lanes)
    isolated = []
    for i in range(3):
        isolated.append(mix([{**lane, "solo": i == j} for j, lane in enumerate(lanes)]))
    assert np.max(abs(combined - sum(isolated))) < 4 / 32767
    assert (
        np.max(
            abs(
                mix([{**lane, "mute": j == 1} for j, lane in enumerate(lanes)])
                - isolated[0]
                - isolated[2]
            )
        )
        < 3 / 32767
    )
    # Isolate the wet return in time. A 125 ms A4 burst has finished before
    # the first quarter-note repeat at 120 BPM.
    score.write_text("48000 96000\n0 0 69 0.7\n6000 1 69 0\n")
    tone = dict(source="sine", gain=0.5, send=1)

    def returns(lane, **overrides):
        patch = dict(
            format=1,
            attack=0,
            release=0,
            bpm=120,
            delay=1,
            feedback=0,
            reverb=0,
            lanes=[lane],
        )
        patch.update(overrides)
        (work / "patch.json").write_text(json.dumps(patch))
        return render(work, "test", score)

    dry = returns(tone)
    assert np.flatnonzero(abs(dry[22000:]) > 1e-4)[0] + 22000 == 24001
    shifted = returns(tone, echo_pitch=12)
    window = shifted[24200:28600]
    dominant = np.argmax(spectrum(window)) * 48000 / len(window)
    assert abs(12 * np.log2(dominant / 880)) <= 1, (dominant, "granular echo")
    for insert in ["lowpass", "bandpass", "highpass", "shape", "comb"]:
        tone = dict(source="sine", gain=1, inserts=[insert], cutoff=600)
        x = returns(tone)
        assert np.isfinite(x).all() and np.std(x[2000:5000]) > 0.001
    # An impulse leaves the delay line but recirculates between the two stereo
    # room taps. Check timings and geometric decay directly in each channel.
    score.write_text("48000 16000\n0 0 69 1\n2 1 69 0\n")
    returns(dict(source="sine", gain=1, space=1, pan=-1), reverb=0.5)
    with wave.open(str(work / "out.wav")) as f:
        stereo = np.frombuffer(f.readframes(f.getnframes()), dtype="<i2").reshape(-1, 2)
    assert stereo[1, 0] > 0 and stereo[1, 1] == 0
    assert abs(int(stereo[4094, 1]) - int(stereo[1, 0])) < 2
    assert abs(int(stereo[7347, 0]) - int(stereo[1, 0]) * 0.5) < 2
    print(
        "Factory briefs, synthesis spectra, lane summation, echo, room taps and decay passed."
    )
