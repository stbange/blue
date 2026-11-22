"""Check the installed content tree against the repository and inspect the binaries."""
from pathlib import Path
import argparse
import hashlib
import json

parser = argparse.ArgumentParser()
parser.add_argument("directory", type=Path)
parser.add_argument("--web", action="store_true")
args = parser.parse_args()
source = Path(__file__).resolve().parents[1] / "content"
installed = args.directory / "content"


def inventory(root):
    return {p.relative_to(root): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob("*") if p.is_file()}


expected = inventory(source)
assert inventory(installed) == expected, "Packaged loose content differs from source"
programs = list(args.directory.glob("*.wasm")) if args.web else [
    args.directory / ("Blue.exe" if (args.directory / "Blue.exe").exists() else "Blue")]
assert programs
for path in programs:
    binary = path.read_bytes()
    for relative in expected:
        asset = (source / relative).read_bytes()
        if len(asset) >= 64:
            assert asset not in binary, f"{relative} is embedded in {path.name}"
print(json.dumps({str(p): digest for p, digest in expected.items()}, indent=2))
