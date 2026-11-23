"""Drive the packaged instrument on a nested origin and inspect its recorded master."""

import argparse
import os
import json
import shutil
import subprocess
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import tempfile
import threading
import wave
from array import array
from playwright.sync_api import expect, sync_playwright


def check_audio(path, expected=440):
    with wave.open(str(path)) as wav:
        assert wav.getnchannels() == 2
        rate = wav.getframerate()
        frames = array("h", wav.readframes(wav.getnframes()))[::2]
    active = [i for i, x in enumerate(frames) if abs(x) > 64]
    assert active and active[-1] - active[0] > rate * 0.12, (
        "silent or truncated capture"
    )
    samples = frames[active[0] : active[-1] + 1]
    crossings = [i for i, (a, b) in enumerate(zip(samples, samples[1:])) if a < 0 <= b]
    pitch = (len(crossings) - 1) * rate / (crossings[-1] - crossings[0])
    assert abs(pitch - expected) < 2, (pitch, expected)
    assert max(map(abs, frames[-int(rate * 0.025) :])) < 64, "stuck note"
    print(f"Captured {len(frames)} stereo frames at {rate} Hz, tone {pitch:.1f} Hz")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("site", type=Path)
    parser.add_argument("--browser", default="chromium")
    parser.add_argument("--executable")
    parser.add_argument("--iwer", type=Path)
    parser.add_argument("--native-package", type=Path)
    args = parser.parse_args()

    class PackServer(SimpleHTTPRequestHandler):
        missing_sample = False

        def do_GET(self):
            if self.missing_sample and self.path.endswith("/samples/glass.wav"):
                self.send_error(404)
            else:
                super().do_GET()

    server = ThreadingHTTPServer(
        ("127.0.0.1", 0), partial(PackServer, directory=str(args.site.resolve().parent))
    )
    threading.Thread(target=server.serve_forever, daemon=True).start()
    url = f"http://127.0.0.1:{server.server_port}/{args.site.name}/"
    with sync_playwright() as pw, tempfile.TemporaryDirectory() as tmp:
        browser = getattr(pw, args.browser).launch(
            executable_path=args.executable,
            headless=not os.getenv("BLUE_HEADED"),
            channel="chromium"
            if args.browser == "chromium" and not args.executable
            else None,
        )
        page = browser.new_page()
        evidence = Path(os.getenv("BLUE_EVIDENCE", tmp))
        evidence.mkdir(parents=True, exist_ok=True)
        page.context.tracing.start(screenshots=True, snapshots=True)
        expect.set_options(timeout=20000)
        page.on(
            "console",
            lambda m: (
                print("Console", m.type, m.text[:300], flush=True)
                if m.type in ("error", "warning")
                else None
            ),
        )
        page.on(
            "requestfailed",
            lambda r: print("Failed request", r.url, r.failure, flush=True),
        )
        page.on("pageerror", lambda err: print("Browser error", err, flush=True))
        if args.browser == "chromium":
            # Chromium can run its real-time audio graph against a silent output device in CI.
            page.add_init_script(
                "window.AudioContext=class extends AudioContext{constructor(options={}){super({...options,sinkId:{type:'none'}})}}"
            )
        if args.iwer:
            page.add_init_script(
                script=args.iwer.read_text()
                + "\nwindow.xrDevice=new IWER.XRDevice(IWER.metaQuest3);xrDevice.installRuntime({forceInstall:true});"
            )
        page.add_init_script(
            "Object.defineProperty(navigator,'requestMIDIAccess',{value:()=>Promise.reject(new DOMException('MIDI permission denied','NotAllowedError')),configurable:true})"
        )
        try:
            page.goto(url)
            expect(page.get_by_role("status")).to_contain_text("Patch ready")
            page.get_by_role("button", name="Calibration").click()
            expect(page.locator("h1")).to_have_text("Calibration")
            page.get_by_role("button", name="Start audio").click()
            expect(page.get_by_role("status")).to_have_text("Playing Calibration")
            for bank, preset in [
                ("formants", "Alder Choir"),
                ("littoral", "Pebble Choir"),
                ("nocturnes", "Polar Veil"),
                ("signals", "Quartz Steps"),
            ]:
                page.locator("#pack").select_option("packs/blue." + bank)
                expect(page.locator("#library button")).to_have_count(6)
                page.get_by_role("button", name=preset, exact=True).click()
                expect(page.get_by_role("status")).to_have_text("Playing " + preset)
            page.locator("#pack").select_option("packs/blue.soundstudy.tideline")
            expect(page.get_by_role("status")).to_have_text("Playing Tideline")
            page.locator("#import").set_input_files(
                Path(__file__).resolve().parents[1]
                / "examples/preset-library/littoral.zip"
            )
            expect(page.locator("#pack")).to_have_value("user/blue.littoral")
            page.get_by_role("button", name="Harbor Lights", exact=True).click()
            expect(page.get_by_role("status")).to_have_text("Playing Harbor Lights")
            page.locator("#pack").select_option("packs/blue.factory")
            page.get_by_role("button", name="Calibration").click()
            expect(page.get_by_role("status")).to_have_text("Playing Calibration")
            page.locator("#midi").click()
            expect(page.get_by_role("status")).to_contain_text("denied")
            page.get_by_role("button", name="Record output").click()
            page.keyboard.down("r")
            page.wait_for_timeout(280)
            page.keyboard.up("r")
            page.wait_for_timeout(160)
            with page.expect_download() as capture:
                page.get_by_role("button", name="Stop recording").click()
            wav = evidence / "flat.wav"
            capture.value.save_as(wav)
            check_audio(wav)
            assert page.locator("#keyboard button").count() == 36
            page.locator("#tuning").select_option(label="C Rast (24-TET)")
            expect(page.locator("#key-c span")).to_have_text("D♯2+50¢")
            page.get_by_text("Tuning builder", exact=True).click()
            page.locator("#tuning-root").fill("36")
            page.locator("#tuning-cents").fill("0, 200, 400, 500, 700, 950, 1100")
            page.get_by_role("button", name="Save custom tuning").click()
            expect(page.locator("#key-r span")).to_have_text("A4+50¢")
            page.get_by_role("button", name="Record output").click()
            page.keyboard.down("r")
            page.wait_for_timeout(310)
            page.keyboard.up("r")
            page.wait_for_timeout(160)
            with page.expect_download() as custom:
                page.get_by_role("button", name="Stop recording").click()
            custom_wav = evidence / "custom-tuning.wav"
            custom.value.save_as(custom_wav)
            check_audio(custom_wav, 440 * 2 ** (0.5 / 12))
            page.locator("#tuning").select_option(label="C major (C Dur)")
            page.get_by_text("Patch and Lua editor", exact=True).click()
            original = page.locator("#mapping").input_value()
            earlier = json.loads(page.locator("#document").input_value())
            earlier["format"] = 0
            earlier["volume"] = earlier.pop("gain", 0.22)
            page.locator("#document").fill(json.dumps(earlier))
            page.locator("#mapping").fill("this is invalid")
            page.get_by_role("button", name="Apply edits").click()
            expect(page.get_by_role("status")).to_contain_text("performance.lua")
            page.locator("#mapping").fill(original + "\nblue.route(4, 'gain', -1)")
            page.get_by_role("button", name="Apply edits").click()
            expect(page.get_by_role("status")).to_have_text("Playing Calibration")
            page.get_by_role("button", name="Save revision").click()
            expect(page.get_by_role("status")).to_contain_text("Saved")
            page.reload()
            expect(page.get_by_role("status")).to_contain_text("Patch ready")
            page.locator("#tuning").select_option(label="Custom")
            expect(page.locator("#key-r span")).to_have_text("A4+50¢")
            page.locator("#tuning").select_option(label="C major (C Dur)")
            page.get_by_role("button", name="Calibration").click()
            expect(page.locator("#mapping")).to_have_value(
                original + "\nblue.route(4, 'gain', -1)"
            )
            page.locator("#start").click()
            expect(page.get_by_role("status")).to_contain_text("Playing")
            PackServer.missing_sample = True
            page.get_by_role("button", name="Glass Tide").click()
            expect(page.get_by_role("status")).to_contain_text("glass.wav")
            PackServer.missing_sample = False
            page.get_by_role("button", name="Glass Tide").click()
            expect(page.get_by_role("status")).to_have_text("Playing Glass Tide")
            page.get_by_role("button", name="Calibration").click()
            expect(page.get_by_role("status")).to_have_text("Playing Calibration")
            page.get_by_text("Patch and Lua editor", exact=True).click()
            with page.expect_download() as archive:
                page.locator("#export").click()
            zipfile = evidence / "calibration.zip"
            archive.value.save_as(zipfile)
            if args.native_package:
                tools = args.native_package.resolve()
                content = Path(tmp) / "Desktop imported"
                exported = Path(tmp) / "desktop-calibration.zip"
                subprocess.run(
                    [tools / "blue-pack", "import", zipfile, content], check=True
                )
                pack = (
                    content
                    / json.loads((content / "index.json").read_text())["packs"][0]
                )
                subprocess.run(
                    [tools / "blue-pack", "export", pack, "Calibration", exported],
                    check=True,
                )
                native_wav = evidence / "desktop-render.wav"
                subprocess.run(
                    [tools / "blue-render", pack, "Calibration", native_wav], check=True
                )
                shutil.copy2(exported, evidence / exported.name)
                zipfile = exported
            page.locator("#import").set_input_files(zipfile)
            expect(page.locator("#pack")).to_have_value("user/blue.factory")
            expect(page.get_by_role("status")).to_have_text("Playing Calibration")
            expect(page.locator("#mapping")).to_have_value(
                original + "\nblue.route(4, 'gain', -1)"
            )
            page.locator("#m3").fill("0.75")
            page.get_by_role("button", name="Record output").click()
            page.keyboard.down("r")
            page.wait_for_timeout(320)
            page.keyboard.up("r")
            page.wait_for_timeout(190)
            with page.expect_download() as edited:
                page.get_by_role("button", name="Stop recording").click()
            dim = evidence / "edited.wav"
            edited.value.save_as(dim)

            def peak(path):
                with wave.open(str(path)) as stream:
                    return max(
                        map(abs, array("h", stream.readframes(stream.getnframes())))
                    )

            ratio = peak(dim) / peak(wav)
            assert 0.22 < ratio < 0.28, ("external Lua gain ratio", ratio)
            page.locator("#m3").fill("0")
            if args.iwer:
                page.get_by_role("button", name="Record output").click()
            page.locator("#xr").click()
            if args.iwer:
                expect(page.get_by_role("status")).to_contain_text("Trigger plays")
                page.evaluate(
                    "xrDevice.controllers.right.updateButtonValue('trigger',1)"
                )
                page.wait_for_timeout(500)
                page.evaluate("xrDevice.quaternion.set(0,1,0,0)")
                page.wait_for_timeout(500)
                page.evaluate(
                    "xrDevice.controllers.right.updateButtonValue('squeeze',1)"
                )
                page.wait_for_timeout(200)
                page.evaluate("xrDevice.controllers.right.position.set(.2,1.75,-.8)")
                page.wait_for_timeout(300)
                moved = float(page.locator("#m0").input_value())
                assert moved > 0.05, "Grip did not move the layer or macro"
                page.evaluate("xrDevice.controllers.right.connected=false")
                page.wait_for_timeout(250)
                page.evaluate("xrDevice.activeSession.end()")
                expect(page.get_by_role("status")).to_contain_text("Returned")
                assert abs(float(page.locator("#m0").input_value()) - moved) < 0.02
                page.wait_for_timeout(200)
                with page.expect_download() as output:
                    page.get_by_role("button", name="Stop recording").click()
                wav = evidence / "xr.wav"
                output.value.save_as(wav)
                with wave.open(str(wav)) as audio:
                    pcm = array("h", audio.readframes(audio.getnframes()))
                ratios = []
                for i in range(0, len(pcm) - 2048, 2048):
                    left = sum(x * x for x in pcm[i : i + 2048 : 2])
                    right = sum(x * x for x in pcm[i + 1 : i + 2048 : 2])
                    if left + right > 100000:
                        ratios.append((right - left) / (left + right))
                assert ratios and min(ratios) < -0.1 and max(ratios) > 0.1, (
                    "XR stereo response",
                    ratios,
                )
                assert max(map(abs, pcm[-1000:])) < 64, "XR note remained active"
            else:
                expect(page.get_by_role("status")).not_to_contain_text("Playing")
        finally:
            try:
                page.screenshot(path=evidence / "instrument.png", timeout=5000)
            finally:
                page.context.tracing.stop(path=evidence / "trace.zip")
        browser.close()
    server.shutdown()


if __name__ == "__main__":
    main()
