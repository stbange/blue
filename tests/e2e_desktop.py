"""Exercise the distributed SDL application through OS keyboard and pointer input."""
import argparse
import os
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
    parser=argparse.ArgumentParser()
    parser.add_argument("executable",type=Path)
    args=parser.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp); app=root/"Blue Instrument";app.mkdir()
        exe=app/args.executable.name;shutil.copy2(args.executable,exe)
        shutil.copytree(args.executable.parent/"content",app/"content")
        wav=root/"master.wav"
        child=subprocess.Popen([str(exe),"--record-to",str(wav)],cwd=root,env={**os.environ,"SDL_AUDIODRIVER":"dummy"})
        try:
            time.sleep(2)
            assert child.poll() is None
            if sys.platform.startswith("linux"):
                from Xlib import X, display
                connection=display.Display()
                for attempt in range(100):
                    windows=[w for w in connection.screen().root.query_tree().children if "Blue |" in str(w.get_wm_name())]
                    if windows:break
                    time.sleep(.1)
                assert windows, "Blue window did not appear"
                window=windows[-1]
                window.set_input_focus(X.RevertToParent,X.CurrentTime)
                connection.sync()
            w,h=ui.size();ui.click(w//2,h//2-160)
            ui.press("left");ui.press("f5");ui.press("right");ui.press("f9")
            ui.press("r");ui.keyDown("h");time.sleep(.35);ui.keyUp("h");time.sleep(.18);ui.press("r")
            for i in range(30):
                if wav.exists() and wav.stat().st_size>2048:break
                time.sleep(.1)
            with wave.open(str(wav)) as f:
                rate=f.getframerate();pcm=array("h",f.readframes(f.getnframes()))[::2]
            active=[i for i,x in enumerate(pcm) if abs(x)>64]
            assert active and active[-1]-active[0]>rate*.2
            signal=pcm[active[0]:active[-1]+1]
            freq=sum(a<0<=b for a,b in zip(signal,signal[1:]))*rate/len(signal)
            assert abs(freq-440)<10,(freq,len(signal))
            assert max(map(abs,pcm[-int(rate*.03):]))==0
            print(f"Desktop master recorded {len(pcm)} frames, {freq:.1f} Hz.")
            ui.press("escape");assert child.wait(timeout=10)==0
        finally:
            if child.poll() is None:child.terminate();child.wait(timeout=10)
if __name__=="__main__":main()
