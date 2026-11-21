"""Check the measurable musical briefs and analytic source fixtures."""
from pathlib import Path
import json,subprocess,sys,tempfile,wave
import numpy as np
root=Path(__file__).resolve().parents[1]
exe=Path(sys.argv[1]).resolve()
factory=root/"content/packs/blue.factory"
with tempfile.TemporaryDirectory() as td:
    work=Path(td)
    def render(folder,name,score):
        subprocess.run([exe,folder,name,work/"out.wav",score],check=True)
        with wave.open(str(work/"out.wav")) as f:
            return np.frombuffer(f.readframes(f.getnframes()),dtype="<i2").reshape(-1,2).mean(axis=1)/32767
    def spectrum(x):return abs(np.fft.rfft(x*np.hanning(len(x))))
    def centroid(x):
        a=spectrum(x);return np.average(np.fft.rfftfreq(len(x),1/48000),weights=a)
    for name in ["Glass Tide","Struck Wire","Paper Cloud","Step Current"]:
        y=render(factory,name,root/"content/performances"/(name+".score"))
        if name=="Glass Tide":
            ratio=centroid(y[10*48000:12*48000])/centroid(y[48000:3*48000])
            assert ratio>=1.25,(name,ratio)
        elif name=="Struck Wire":
            def color(x):
                a=spectrum(x);freq=np.fft.rfftfreq(len(x),1/48000)
                return np.linalg.norm(a[freq>800])/np.linalg.norm(a[(freq>420)&(freq<460)])
            delta=20*np.log10(color(y[800:4800])/color(y[48000:52000]));assert delta>=6,delta
        elif name=="Paper Cloud":
            assert np.std(y[48000:96000])>.001 and np.isfinite(y).all()
        else:
            phase=np.abs(y[48000:48000*5]).reshape(-1,24000)
            assert np.corrcoef(phase[1],phase[2])[0,1]>.95
    (work/"pack.json").write_text(json.dumps(dict(format=1,luaApi=1,presets={"test":"patch.json"},script="mapping.lua")))
    (work/"mapping.lua").write_text("")
    score=work/"notes.score";score.write_text("48000 96000\n0 0 69 1\n")
    (work/"patch.json").write_text(json.dumps(dict(format=1,lanes=[dict(source="ring",ratio=.25,gain=1)])))
    y=render(work,"test",score);fft=spectrum(y[48000:]);assert min(fft[330],fft[550])>100*fft[440]
    print("Four musical briefs and analytic ring-modulation spectrum passed.")
