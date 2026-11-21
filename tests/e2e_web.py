"""Drive the packaged instrument on a nested origin and inspect its recorded master."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import tempfile
import threading
import wave
from array import array
from playwright.sync_api import expect, sync_playwright

def check_audio(path):
    with wave.open(str(path)) as wav:
        assert wav.getnchannels() == 2
        rate = wav.getframerate()
        frames = array("h", wav.readframes(wav.getnframes()))[::2]
    active = [i for i,x in enumerate(frames) if abs(x)>64]
    assert active and active[-1]-active[0] > rate*.12, "silent or truncated capture"
    samples = frames[active[0]:active[-1]+1]
    pitch = sum(a < 0 <= b for a,b in zip(samples,samples[1:]))*rate/len(samples)
    assert 425 < pitch < 455, pitch
    assert max(map(abs, frames[-int(rate*.025):])) < 64, "stuck note"
    print(f"Captured {len(frames)} stereo frames at {rate} Hz, tone {pitch:.1f} Hz")

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("site",type=Path)
    parser.add_argument("--browser",default="chromium")
    parser.add_argument("--executable")
    parser.add_argument("--iwer",type=Path)
    args=parser.parse_args()
    server=ThreadingHTTPServer(("127.0.0.1",0),partial(SimpleHTTPRequestHandler,directory=str(args.site.resolve().parent)))
    threading.Thread(target=server.serve_forever,daemon=True).start()
    url=f"http://127.0.0.1:{server.server_port}/{args.site.name}/"
    with sync_playwright() as pw, tempfile.TemporaryDirectory() as tmp:
        browser=getattr(pw,args.browser).launch(executable_path=args.executable)
        page=browser.new_page()
        page.on("pageerror",lambda err: print("Browser error",err,flush=True))
        if args.iwer:
            page.add_init_script(script=args.iwer.read_text()+"\nwindow.xrDevice=new IWER.XRDevice(IWER.metaQuest3);xrDevice.installRuntime({forceInstall:true});")
        page.goto(url)
        expect(page.get_by_role("status")).to_contain_text("Patch ready")
        page.get_by_role("button",name="Calibration").click()
        expect(page.locator("h1")).to_have_text("Calibration")
        page.get_by_role("button",name="Start audio").click()
        expect(page.get_by_role("status")).to_have_text("Playing Calibration")
        page.get_by_role("button",name="Record output").click()
        page.keyboard.down("h");page.wait_for_timeout(280);page.keyboard.up("h")
        page.wait_for_timeout(160)
        with page.expect_download() as capture:page.get_by_role("button",name="Stop recording").click()
        wav=Path(tmp)/"capture.wav";capture.value.save_as(wav);check_audio(wav)
        page.locator("summary").click()
        original=page.locator("#mapping").input_value()
        page.locator("#mapping").fill("this is invalid")
        page.get_by_role("button",name="Apply edits").click()
        expect(page.get_by_role("status")).to_contain_text("performance.lua")
        page.locator("#mapping").fill(original+"\nblue.route(4, 'gain', -1)")
        page.get_by_role("button",name="Apply edits").click()
        expect(page.get_by_role("status")).to_have_text("Playing Calibration")
        page.get_by_role("button",name="Save revision").click()
        expect(page.get_by_role("status")).to_contain_text("Saved")
        page.reload();page.get_by_role("button",name="Calibration").click()
        expect(page.locator("#mapping")).to_have_value(original+"\nblue.route(4, 'gain', -1)")
        page.locator("#start").click();expect(page.get_by_role("status")).to_contain_text("Playing")
        page.locator("#xr").click()
        if args.iwer:
            expect(page.get_by_role("status")).to_contain_text("Trigger plays")
            page.evaluate("xrDevice.controllers.right.updateButtonValue('trigger',1)")
            page.wait_for_timeout(300)
            page.evaluate("xrDevice.controllers.right.updateButtonValue('trigger',0)")
            page.evaluate("xrDevice.activeSession.end()")
            expect(page.get_by_role("status")).to_contain_text("Returned")
        else:expect(page.get_by_role("status")).not_to_contain_text("Playing")
        browser.close()
    server.shutdown()
if __name__=="__main__":main()
