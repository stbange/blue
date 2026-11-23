"""Regressions for missing inventories and unrounded coverage thresholds."""

from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory
import subprocess
import sys
from coverage_total import assess

with TemporaryDirectory() as tmp, redirect_stdout(StringIO()):
    root = Path(tmp)
    full = {"covered": 100, "total": 100}
    native = {
        "C/" + g: full for g in ["desktop", "offline", "web", "control", "engine"]
    }
    js = {"total": {"lines": full}}

    def rejects(run):
        try:
            run()
        except AssertionError:
            return
        raise RuntimeError("Expected a failing coverage gate")

    assert assess(root, native, js, full)["All first-party"]["covered"] == 700
    rejects(
        lambda: assess(
            root, {**native, "C/web": {"covered": 949, "total": 1000}}, js, full
        )
    )
    rejects(
        lambda: assess(
            root, {k: v for k, v in native.items() if k != "C/web"}, js, full
        )
    )
    (root / "unloaded.js").touch()
    rejects(lambda: assess(root, native, js, full))
    mapping = (
        Path(__file__).resolve().parents[1]
        / "content/packs/blue.factory/scripts/performance.lua"
    )
    extra = root / "untested.lua"
    extra.write_text("local a=1\na=a+2\nreturn a\n")
    result = subprocess.run(
        [
            Path(sys.argv[1]).resolve(),
            Path(__file__).with_name("lua_unit.lua"),
            mapping,
            mapping,
            extra,
        ],
        capture_output=True,
        text=True,
    )
    assert result.returncode and "untested.lua: 0/" in result.stdout, result.stdout
