"""Check the unrounded first-party executable line counts across the shipped languages."""
import json
from pathlib import Path
import re
import subprocess
import sys

build = Path(sys.argv[1]).resolve()
site = Path(__file__).resolve().parents[1] / "hosts/web/site"
groups = json.loads((build / "line-coverage.json").read_text())
js = json.loads(Path(sys.argv[2]).read_text())
js_files = {Path(k).resolve() for k in js if k != "total"}
assert js_files == set(site.rglob("*.js")), ("JavaScript inventory", js_files ^ set(site.rglob("*.js")))
groups["Web and XR JavaScript"] = js["total"]["lines"]
lua = subprocess.check_output(["ctest", "--test-dir", str(build), "-V", "-R", "^lua_unit$"], text=True)
match = re.search(r"Lua total (\d+)/(\d+)", lua)
assert match, "Lua coverage report missing"
groups["factory Lua"] = dict(zip(["covered","total"],map(int,match.groups())))
groups["All first-party"] = {column:sum(g[column] for g in groups.values())
                             for column in ("covered","total")}
failed = []
for name, data in groups.items():
    hits, total = data["covered"], data["total"]
    print(f"{name}: {hits}/{total} executable lines ({100*hits/total:.2f}%)")
    if hits*100 < 95*total:
        failed.append(name)
assert not failed, failed
