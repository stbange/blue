"""Replace physical assets in a relocated pack and measure both production renderers."""
from array import array
from hashlib import sha256
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import wave

native, web = [Path(x).resolve() for x in sys.argv[1:]]
node_driver = Path(__file__).with_name("wasm_render.cjs").resolve()
binaries = [native, web/"audio.wasm", web/"author.wasm"]
def digests():
    return [sha256(p.read_bytes()).digest() for p in binaries]

with tempfile.TemporaryDirectory() as tmp:
    temp = Path(tmp)
    pack = temp / "Moved Blue instrument"
    pack.mkdir()
    (pack/"pack.json").write_text(json.dumps({"id":"test.loose","format":1,"luaApi":1,
        "presets":{"Test":"patch.json"},"script":"route.lua","assets":{"osc":"source.wav"}}))
    (pack/"patch.json").write_text(json.dumps({"format":1,"sample":"osc","attack":0,
        "macros":[.5],"lanes":[{"source":"sample","root":69,"gain":1}]}))
    score = temp/"note.score"
    score.write_text("48000 48000\n0 0 69 1\n")
    previous = {}
    hashes = digests()
    for freq, amount in [(220,0),(440,-1)]:
        with wave.open(str(pack/"source.wav"), "wb") as stream:
            stream.setparams((1,2,48000,0,"NONE",""))
            pcm=array("h",(int(16000*math.sin(i*math.tau*freq/48000)) for i in range(48000)))
            stream.writeframes(pcm.tobytes())
        (pack/"route.lua").write_text(f'blue.route(1,"gain",{amount})\n')
        subprocess.run([native,pack,"Test",temp/"out.wav",score],cwd=temp,check=True)
        subprocess.run(["node",node_driver,web,"Test",score,temp/"out.f32",pack],cwd=temp,check=True)
        with wave.open(str(temp/"out.wav")) as stream:
            a=array("h");a.frombytes(stream.readframes(48000))
        b=array("f");b.frombytes((temp/"out.f32").read_bytes())
        for platform,output in [("native",a),("Wasm",b)]:
            end=output[-48000::2]
            hz=sum(x<0<=y for x,y in zip(end,end[1:]))*2
            assert abs(hz-freq)<=2,(platform,hz)
            peak=max(map(abs,end))
            if platform in previous:
                assert .49 < peak/previous[platform] < .51,(platform,peak)
            previous[platform]=peak
        assert hashes==digests()
print("Native and Wasm followed the replaced PCM and Lua mapping at unchanged binary digests.")
