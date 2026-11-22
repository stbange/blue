"""Unit fixtures for the C pack compiler, recovery behavior and PCM codec."""
from copy import deepcopy
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

def main():
    binary=Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as temp:
        root=Path(temp);p=root/"pack";p.mkdir()
        def run(*args): subprocess.run([binary,*map(str,args)],check=True)
        def put(name,data): (p/name).write_text(json.dumps(data))
        manifest=dict(format=1,luaApi=1,presets={"test":"patch.json"},assets={"wave":"audio.wav"},script="voice.lua")
        doc={"format":1,"lanes":[{"source":"sample","root":69,"inserts":["comb"],"harmonics":[[1],[1,.4]]}],"sample":"wave","loop_start":0,"loop_end":3,"lfo":[.2,1,5],"seed":7}
        script="blue.lane(1,'sample',1)\nblue.control(2,'Space')\nblue.route(2,'density',3)"
        def fixture():
            put("pack.json",manifest);put("patch.json",doc);(p/"voice.lua").write_text(script);run("wave",p/"audio.wav")
        def check(good): run("pack",p,int(good))
        fixture();check(True)
        put("patch.json",{**doc,"format":0,"volume":.4});check(True)
        (p/"voice.lua").write_text("blue.at(0.01,2,0.75)");check(True)
        (p/"voice.lua").write_text("blue.at(-1,0,3)");check(False)
        (p/"patch.json").write_text('{"format":1,"lanes":[],"lanes":[]}');check(False)
        fixture()
        (root/"index.json").write_text('{"packs":["../escape","absent","pack"]}');run("catalog",root,1);run("catalog",p,0)
        for key,value in [('format',2),('lanes',[]),('lanes',[{}]*4),('envelope',[dict(time=0)]*129),('envelope',[dict(time=1)]),('envelope',[dict(time=0),dict(time=0)]),('loop',3),('sample','absent'),('lfo',[32]),('loop_end',0),('gain','one')]:
            case=dict(doc);case[key]=value;put('patch.json',case);check(False)
        for key,value in [('source','missing'),('harmonics',[[3]]),('harmonics',[[1]*17]),('inserts',['x']),('inserts',['bypass']*3),('root',-1),('mute','yes'),('solo',1)]:
            case=deepcopy(doc);case['lanes'][0][key]=value;put('patch.json',case);check(False)
        fixture()
        for text in ["invalid syntax ...","while true do end","local t={} for i=1,2000 do t[i]={"+','.join(['1']*100)+"} end","blue.route(9,'gain',1)","blue.lane(0,'fm',1)","blue.control(11,'bad')"]:
            (p/'voice.lua').write_text(text);check(False)
        fixture()
        for key,value in [('luaApi',3),('script','../escape.lua'),('script','bad:filename'),('script','absent.lua'),('format',None),('presets',{})]:
            case=dict(manifest);case[key]=value;put('pack.json',case);check(False)
        fixture();audio=p/'audio.wav';valid=audio.read_bytes()
        for index,data in [(0,b'bad!'),(20,b'\x03\x00'),(22,b'\x04\x00'),(24,b'\0'*4),(34,b'\x08\0'),(40,struct.pack('<I',0xffffffff))]:
            audio.write_bytes(valid[:index]+data+valid[index+len(data):]);check(False)
        audio.write_bytes(valid[:-1]);check(False);audio.unlink();check(False)
        audio.write_bytes(valid[:12]+b'JUNK'+struct.pack('<I',3)+b'abc\0'+valid[12:]);check(True)
        (p/'patch.json').write_text(' '*1048577);check(False)
        fixture();check(True)
if __name__=='__main__':main()
