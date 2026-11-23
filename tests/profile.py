"""Record the optimized desktop baseline and stress profile with their input traces."""

import json
import platform
import resource
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

bench, out = map(Path, sys.argv[1:])
out.mkdir(parents=True, exist_ok=True)
root = Path(__file__).resolve().parents[1]
for label, seconds in [("baseline", 600), ("soak", 30)]:
    with tempfile.TemporaryDirectory() as scratch:
        raw = Path(scratch) / "capture.s16"
        args = [bench.resolve(), root / "content/packs/blue.factory", str(seconds), raw]
        if label == "soak":
            args.append("soak")
        result = subprocess.run(args, capture_output=True, text=True)
        (out / (label + ".txt")).write_text(
            result.stdout
            + result.stderr
            + f"maxrss_kib={resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss}\n"
        )
        assert raw.stat().st_size == seconds * 48000 * 4
        with wave.open(str(out / (label + ".wav")), "wb") as w, raw.open("rb") as f:
            w.setparams((2, 2, 48000, 0, "NONE", ""))
            while data := f.read(1 << 20):
                w.writeframesraw(data)
        trace = [{"frame": 0, "key": 44 + i, "velocity": 0.65} for i in range(16)]
        if label == "soak":
            trace.extend(
                {
                    "frame": 48000 * i,
                    "event": "reload, 512 macro values and retrigger 16 voices",
                }
                for i in range(1, seconds)
            )
        (out / (label + "-trace.json")).write_text(json.dumps(trace, indent=2))
        print(label, result.stdout, flush=True)
        if result.returncode:
            raise SystemExit(result.returncode)
(out / "machine.txt").write_text(
    platform.platform()
    + "\n"
    + Path("/proc/cpuinfo").read_text().split("\n\n")[0]
    + "\n"
)
