"""Exercise the distributed SDL application through OS keyboard and pointer input."""

import argparse
import os
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import wave
from array import array
import pyautogui as ui


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        app = root / "Blue Instrument"
        app.mkdir()
        exe = app / args.executable.name
        shutil.copy2(args.executable, exe)
        shutil.copytree(args.executable.parent / "content", app / "content")
        wav = root / "master.wav"

        trace = []
        log = (root / "diagnostics.txt").open("w")

        def snapshot():
            if os.getenv("BLUE_EVIDENCE"):
                from PIL import ImageGrab

                ImageGrab.grab().save(root / "instrument.png")

        def start(preset):
            return subprocess.Popen(
                [
                    str(exe),
                    "--record-to",
                    str(wav),
                    "--preset",
                    preset,
                    "--user-dir",
                    str(root),
                ],
                cwd=root,
                stdout=log,
                stderr=log,
                env={**os.environ, "SDL_AUDIODRIVER": "dummy"},
            )

        child = start("Calibration")
        try:
            time.sleep(2)
            assert child.poll() is None
            if sys.platform.startswith("linux"):
                from Xlib import X, display

                connection = display.Display()
                for attempt in range(100):
                    windows = [
                        w
                        for w in connection.screen().root.query_tree().children
                        if "Blue |" in str(w.get_wm_name())
                    ]
                    if windows:
                        break
                    time.sleep(0.1)
                assert windows, "Blue window did not appear"
                window = windows[-1]
                window.set_input_focus(X.RevertToParent, X.CurrentTime)
                connection.sync()
            keys = ui
            if sys.platform == "win32":
                import ctypes

                user = ctypes.windll.user32
                for _ in range(100):
                    handle = user.FindWindowW(None, "Blue | Calibration")
                    if handle:
                        break
                    time.sleep(0.1)
                assert handle, "Blue window did not appear"

                class WindowKeys:
                    codes = {
                        "left": 37,
                        "right": 39,
                        "f5": 116,
                        "f9": 120,
                        "escape": 27,
                        "[": 219,
                        "]": 221,
                    }

                    def post(self, key, up):
                        code = self.codes.get(
                            key, ord(key.upper()) if len(key) == 1 else 0
                        )
                        scan = user.MapVirtualKeyW(code, 0)
                        flags = (
                            1
                            | (scan << 16)
                            | (1 << 24 if key in ("left", "right") else 0)
                            | (0xC0000000 if up else 0)
                        )
                        user.PostMessageW(handle, 0x101 if up else 0x100, code, flags)

                    def keyDown(self, key):
                        self.post(key, False)

                    def keyUp(self, key):
                        self.post(key, True)

                    def press(self, key):
                        self.keyDown(key)
                        time.sleep(0.04)
                        self.keyUp(key)
                        time.sleep(0.05)

                keys = WindowKeys()
            else:
                w, h = ui.size()
                ui.click(w // 2, h // 2 - 160)
            original_keys = keys

            class TracedKeys:
                def __getattr__(self, action):
                    def send(key):
                        trace.append(
                            {
                                "monotonic_s": time.monotonic(),
                                "action": action,
                                "key": key,
                            }
                        )
                        getattr(original_keys, action)(key)

                    return send

            keys = TracedKeys()
            snapshot()
            for _ in range(4):
                keys.press("]")
            keys.press("f5")
            for _ in range(30):
                if (root / "controls.txt").exists():
                    break
                time.sleep(0.1)
            assert (root / "controls.txt").is_file(), "No performance state was saved"
            assert float((root / "controls.txt").read_text().split()[2]) == 0.5
            keys.press("right")
            time.sleep(0.3)
            if sys.platform.startswith("linux"):
                assert window.get_wm_name() == "Blue | Alder Choir"
            if sys.platform == "win32":
                assert user.FindWindowW(None, "Blue | Alder Choir")
            keys.press("f9")
            time.sleep(0.3)
            if sys.platform.startswith("linux"):
                assert window.get_wm_name() == "Blue | Calibration", (
                    window.get_wm_name()
                )
            if sys.platform == "win32":
                assert user.FindWindowW(None, "Blue | Calibration"), (
                    "Saved preset was not recalled"
                )
            keys.press("r")
            keys.keyDown("h")
            time.sleep(0.15)
            # Reload a maximum-size PCM bank while the held calibration voice plays.
            factory = app / "content/packs/blue.factory"
            patch_file = factory / "presets/calibration.json"
            old_patch = patch_file.read_bytes()
            with wave.open(str(factory / "samples/glass.wav"), "wb") as large:
                large.setparams((1, 2, 48000, 0, "NONE", ""))
                large.writeframes(bytes(65536 * 2))
            patch = json.loads(old_patch)
            patch["sample"] = "glass"
            patch_file.write_text(json.dumps(patch))
            keys.press("z")
            time.sleep(0.25)
            patch_file.write_bytes(old_patch)
            keys.keyUp("h")
            time.sleep(0.9)
            keys.press("r")
            for i in range(30):
                if wav.exists() and wav.stat().st_size > 2048:
                    break
                time.sleep(0.1)
            with wave.open(str(wav)) as f:
                rate = f.getframerate()
                pcm = array("h", f.readframes(f.getnframes()))[::2]
            active = [i for i, x in enumerate(pcm) if abs(x) > 64]
            assert active and active[-1] - active[0] > rate * 0.2
            signal = pcm[active[0] : active[-1] + 1]
            window_size = rate // 200
            assert all(
                max(map(abs, signal[i : i + window_size])) > 64
                for i in range(0, len(signal) - window_size, window_size)
            ), "Gap during live bank staging"
            freq = (
                sum(a < 0 <= b for a, b in zip(signal, signal[1:])) * rate / len(signal)
            )
            assert abs(freq - 440) < 10, (freq, len(signal))
            tail = max(map(abs, pcm[-int(rate * 0.03) :]))
            assert tail < 64, (tail, len(pcm), active[-1], rate)
            print(f"Desktop master recorded {len(pcm)} frames, {freq:.1f} Hz.")
            # Reload a loose mapping into the same executable. Recall the saved half-trim.
            baseline = root / "original.wav"
            wav.replace(baseline)
            script = app / "content/packs/blue.factory/scripts/performance.lua"
            with script.open("a") as mapping:
                mapping.write("\nblue.route(2, 'gain', -1.75)\n")
            keys.press("escape")
            assert child.wait(timeout=10) == 0
            child = start("Glass Tide")
            time.sleep(1.6)
            assert child.poll() is None
            if sys.platform == "win32":
                for attempt in range(120):
                    handle = user.FindWindowW(None, "Blue | Glass Tide")
                    if handle:
                        break
                    time.sleep(0.1)
                assert handle, child.poll()
            elif sys.platform.startswith("linux"):
                for attempt in range(120):
                    names = [
                        (w, w.get_wm_name())
                        for w in connection.screen().root.query_tree().children
                    ]
                    windows = [w for w, n in names if n == "Blue | Glass Tide"]
                    if windows:
                        break
                    time.sleep(0.1)
                assert windows, ([n for w, n in names], child.poll())
                window = windows[-1]
                window.set_input_focus(X.RevertToParent, X.CurrentTime)
                connection.sync()
            else:
                w, h = ui.size()
                ui.click(w // 2, h // 2 - 160)
            document = app / "content/packs/blue.factory/presets/calibration.json"
            missing = root / "temporarily-missing.json"
            document.rename(missing)
            keys.press("f9")
            time.sleep(0.25)
            if sys.platform == "win32":
                assert user.FindWindowW(None, "Blue | Glass Tide")
            elif sys.platform.startswith("linux"):
                assert window.get_wm_name() == "Blue | Glass Tide"
            missing.rename(document)
            keys.press("f9")
            time.sleep(0.5)
            keys.press("r")
            keys.keyDown("h")
            time.sleep(0.35)
            keys.keyUp("h")
            time.sleep(0.3)
            keys.press("r")
            for _ in range(50):
                if wav.exists() and wav.stat().st_size > 2048:
                    break
                time.sleep(0.1)
            with wave.open(str(wav)) as f:
                edited = array("h", f.readframes(f.getnframes()))
            ratio = max(map(abs, edited)) / max(map(abs, pcm))
            assert 0.22 < ratio < 0.28, ("reloaded gain ratio", ratio)
            keys.press("escape")
            assert child.wait(timeout=10) == 0
        finally:
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=10)
            log.close()
            (root / "input.json").write_text(json.dumps(trace, indent=2))
            if os.getenv("BLUE_EVIDENCE"):
                evidence = Path(os.environ["BLUE_EVIDENCE"])
                evidence.mkdir(parents=True, exist_ok=True)
                for f in (
                    wav,
                    root / "original.wav",
                    root / "controls.txt",
                    root / "diagnostics.txt",
                    root / "input.json",
                    root / "instrument.png",
                ):
                    if f.exists():
                        shutil.copy2(f, evidence)


if __name__ == "__main__":
    main()
