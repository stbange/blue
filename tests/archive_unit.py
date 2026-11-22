"""Pack reader and writer fixtures, including rollback when a later entry is bad."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from zipfile import ZipFile
import warnings

tool, factory = map(Path, sys.argv[1:])
with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    def run(*args, valid=True):
        rc=subprocess.run([tool,*map(str,args)]).returncode
        assert (rc==0)==valid,(args,rc)
    archive=root/"sound.zip"
    folder=root/"portable library"
    run("export",factory,"Calibration",archive)
    run("export",factory,"Calibration",archive,valid=False)
    run("import",archive,folder)
    with ZipFile(archive) as z:
        entries={n:z.read(n) for n in z.namelist()}
    assert (folder/"packs/instrument/samples/glass.wav").read_bytes()==(factory/"samples/glass.wav").read_bytes()
    run("import",archive,folder,valid=False)
    run("import",root/"missing.zip",root/"missing",valid=False)
    run("bad",valid=False)
    run("export",factory,"z"*200,root/"unused",valid=False)
    run("export",factory,"unknown",root/"bad",valid=False)
    for name in ["../escape", "absolute:/file", "a//b","a/", "NUL.wav", "com1.foo", "bad./x"]:
        with ZipFile(archive,"w") as z:z.writestr(name,b"abc")
        run("import",archive,root/"wrong",valid=False)
        assert not (root/"wrong").exists()
    with ZipFile(archive,"w") as z:z.writestr("packs/nested/unused",b"x")
    run("import",archive,root/"wrong",valid=False)
    assert not (root/"wrong").exists()
    with warnings.catch_warnings():
        warnings.simplefilter("ignore",UserWarning)
        with ZipFile(archive,"w") as z:
            z.writestr("p/test",b"first");z.writestr("p/test",b"second")
    run("import",archive,root/"wrong",valid=False)
    assert not (root/"wrong").exists()
    with ZipFile(archive,"w") as z:z.writestr("huge.bin",b"x"*(2*1024*1024+1))
    run("import",archive,root/"wrong",valid=False)
    copy=root/"local"
    shutil.copytree(factory,copy)
    (copy/"samples/glass.wav").unlink()
    run("export",copy,"Calibration",root/"incomplete.zip",valid=False)
    assert not (root/"incomplete.zip").exists()
