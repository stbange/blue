"""Compare a released-format document on stock and extension-enabled Wasm builds."""

from array import array
from pathlib import Path
import subprocess
import sys
import tempfile

stock, extended = map(Path, sys.argv[1:])
tests = Path(__file__).resolve().parent
pack = tests / "fixtures/node"
with tempfile.TemporaryDirectory() as tmp:
    out = Path(tmp) / "output.f32"
    for site in (stock, extended):
        result = subprocess.run(
            [
                "node",
                tests / "wasm_render.cjs",
                site,
                "Legacy",
                pack / "note.score",
                out,
                pack,
            ],
            text=True,
            capture_output=True,
        )
        if site == stock:
            assert result.returncode and "fixture.pulse.v1" in result.stderr, (
                result.stderr
            )
        else:
            assert result.returncode == 0, result.stderr
            pcm = array("f")
            pcm.frombytes(out.read_bytes())
            assert len(pcm) == 9600 and abs(max(pcm) - 0.2 * 0.5 * 2**-0.5) < 1e-5
            assert min(pcm) < -0.07 and max(map(abs, pcm[4800:])) == 0
print(
    "Stock build reports the dependency. The extension renders the v0 volume and note boundaries."
)
