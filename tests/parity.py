"""Compare audio from the two built targets, including occupied spectral bands."""
from pathlib import Path
import subprocess,sys,tempfile,wave
import numpy as np
native,dist=map(Path,sys.argv[1:])
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
    tmp=Path(d)
    for score in sorted((root/"content/performances").glob("*.score")):
        name=score.stem
        subprocess.run([native,root/"content/packs/blue.factory",name,tmp/"native.wav",score],check=True)
        subprocess.run(["node",root/"tests/wasm_render.cjs",dist,name,score,tmp/"wasm.f32"],check=True)
        with wave.open(str(tmp/"native.wav")) as wav:
            a=np.frombuffer(wav.readframes(wav.getnframes()),dtype="<i2").astype(float)/32767
        b=np.fromfile(tmp/"wasm.f32",dtype="<f4").astype(float)
        assert len(a)==len(b) and np.isfinite(b).all()
        rms=lambda x: np.sqrt(np.mean(x*x))
        rms_db=abs(20*np.log10(rms(a)/rms(b)))
        assert rms_db<=.5,(name,rms_db)
        if name=="Calibration":assert np.max(abs(a-b))<=.0001
        def bands(x):
            mono=x.reshape(-1,2).mean(axis=1)
            windows=mono[:len(mono)//4096*4096].reshape(-1,4096)
            spec=abs(np.fft.rfft(windows*np.hanning(4096),axis=1))/1024
            return np.array([rms(spec[:,a:b]) for a,b in zip([1,8,32,128,512],[8,32,128,512,2049])])
        aa,bb=bands(a),bands(b);mask=aa>1e-3
        diff=np.max(abs(20*np.log10(aa[mask]/bb[mask]))) if mask.any() else 0
        assert diff<=1,(name,diff)
        print(f"{name}: RMS delta {rms_db:.4f} dB, largest occupied band delta {diff:.4f} dB")
