"""Gate native engine and pack code with unit-only gcov JSON counters."""

import gzip, json, subprocess, sys, tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
build = Path(sys.argv[1]).resolve()
for file in build.rglob("*.gcda"):
    file.unlink()
subprocess.run(["ctest", "--test-dir", build, "--output-on-failure"], check=True)
expected = {
    p.resolve() for folder in ("src", "hosts") for p in (root / folder).rglob("*.c")
}
lines = {}
branches = {}
with tempfile.TemporaryDirectory() as out:
    for i, meta in enumerate(build.rglob("*.gcno")):
        results = Path(out) / str(i)
        results.mkdir()
        subprocess.run(
            ["gcov", "--branch-probabilities", "--json-format", str(meta)],
            cwd=results,
            check=True,
            stdout=subprocess.DEVNULL,
        )
    for report in Path(out).rglob("*.gcov.json.gz"):
        with gzip.open(report, "rt") as source:
            data = json.load(source)
        for entry in data["files"]:
            file = (Path(data["current_working_directory"]) / entry["file"]).resolve()
            if file not in expected:
                continue
            counts = lines.setdefault(file, {})
            for line in entry["lines"]:
                n = line["line_number"]
                counts[n] = max(counts.get(n, 0), line["count"])
                outcomes = branches.setdefault(file, {})
                for index, branch in enumerate(line.get("branches", [])):
                    key = (n, index)
                    outcomes[key] = max(outcomes.get(key, 0), branch["count"])
assert set(lines) == expected, ("Missing coverage", expected - set(lines))
groups = {}
for file, counts in sorted(lines.items()):
    covered = sum(v > 0 for v in counts.values())
    total = len(counts)
    outcomes = branches[file]
    print(
        f"{file.relative_to(root)} {covered}/{total} lines, {sum(x > 0 for x in outcomes.values())}/{len(outcomes)} branch outcomes"
    )
    name = (
        ("offline" if file.name == "render.c" else "desktop")
        if file.parent.name == "hosts"
        else file.parent.name
    )
    group = groups.setdefault(name, [0, 0])
    group[0] += covered
    group[1] += total
for name, (hits, total) in groups.items():
    print(f"{name} unit coverage {100 * hits / total:.2f}%")
    assert hits * 100 >= 95 * total
summary = {
    "C/" + name: {"covered": hits, "total": total}
    for name, (hits, total) in groups.items()
}
(build / "line-coverage.json").write_text(json.dumps(summary, indent=2) + "\n")
