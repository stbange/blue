"""Gate native engine and pack code with unit-only gcov JSON counters."""
import gzip,json,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
build=Path(sys.argv[1]).resolve()
for file in build.rglob("*.gcda"):file.unlink()
subprocess.run(["ctest","--test-dir",build,"--output-on-failure"],check=True)
expected={p.resolve() for folder in ("src/engine","src/control","hosts/web") for p in (root/folder).glob("*.c")}
lines={}
with tempfile.TemporaryDirectory() as out:
    for meta in build.rglob("*.gcno"):
        subprocess.run(["gcov","--json-format",str(meta)],cwd=out,check=True,stdout=subprocess.DEVNULL)
    for report in Path(out).glob("*.gcov.json.gz"):
        with gzip.open(report,"rt") as source:data=json.load(source)
        for entry in data["files"]:
            file=(Path(data["current_working_directory"])/entry["file"]).resolve()
            if file not in expected:continue
            counts=lines.setdefault(file,{})
            for line in entry["lines"]:
                n=line["line_number"];counts[n]=max(counts.get(n,0),line["count"])
assert set(lines)==expected,("Missing coverage",expected-set(lines))
groups={}
for file,counts in sorted(lines.items()):
    covered=sum(v>0 for v in counts.values());total=len(counts)
    print(f"{file.relative_to(root)} {covered}/{total} lines")
    group=groups.setdefault(file.parent.name,[0,0]);group[0]+=covered;group[1]+=total
for name,(hits,total) in groups.items():
    print(f"{name} unit coverage {100*hits/total:.2f}%")
    assert hits*100>=95*total
