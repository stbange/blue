"""Gate the exact first-party executable line counts across all shipped languages."""

import json
from pathlib import Path
import re
import subprocess
import sys


def assess(site, native, js, lua):
    expected = {"C/" + g for g in ("desktop", "offline", "web", "control", "engine")}
    assert set(native) == expected, (
        "Missing C coverage groups",
        expected - set(native),
    )
    files = {Path(k).resolve() for k in js if k != "total"}
    inventory = set(site.rglob("*.js"))
    assert files == inventory, ("JavaScript inventory", files ^ inventory)
    groups = {
        **native,
        "Web and XR JavaScript": js["total"]["lines"],
        "factory Lua": lua,
    }
    groups["All first-party"] = {
        k: sum(g[k] for g in groups.values()) for k in ("covered", "total")
    }
    failed = []
    for name, group in groups.items():
        hits, total = group["covered"], group["total"]
        assert 0 <= hits <= total and total > 0, (name, group)
        print(f"{name}: {hits}/{total} executable lines ({100 * hits / total:.2f}%)")
        if hits * 100 < 95 * total:
            failed.append(name)
    assert not failed, failed
    return groups


if __name__ == "__main__":
    build = Path(sys.argv[1]).resolve()
    site = Path(__file__).resolve().parents[1] / "hosts/web/site"
    native = json.loads((build / "line-coverage.json").read_text())
    js = json.loads(Path(sys.argv[2]).read_text())
    result = subprocess.check_output(
        ["ctest", "--test-dir", str(build), "-V", "-R", "^lua_unit$"], text=True
    )
    m = re.search(r"Lua total (\d+)/(\d+)", result)
    assert m, "Lua report missing"
    groups = assess(
        site, native, js, dict(zip(("covered", "total"), map(int, m.groups())))
    )
    (build / "all-lines.json").write_text(json.dumps(groups, indent=2) + "\n")
