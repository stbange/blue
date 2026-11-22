"""Build a single-bank import for existing desktop and web installations."""

import argparse
import json
import re
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED


def package(pack, destination):
    manifest = json.loads((pack / "pack.json").read_text())
    if not re.fullmatch(r"[a-zA-Z0-9_-][a-zA-Z0-9_.-]*", manifest["id"]):
        raise ValueError("Invalid pack ID")
    folder = "packs/" + manifest["id"]
    files = {
        "pack.json",
        manifest["script"],
        *manifest["presets"].values(),
        *manifest.get("assets", {}).values(),
    }

    def entry(archive, path, data):
        info = ZipInfo(path)
        info.external_attr = 0o100644 << 16
        archive.writestr(info, data, compress_type=ZIP_DEFLATED)

    contents = {}
    for name in sorted(files):
        if not re.fullmatch(r"[a-zA-Z0-9_./-]+", name) or any(
            part in ("", ".", "..") for part in name.split("/")
        ):
            raise ValueError(f"Invalid pack path: {name}")
        path = (pack / name).resolve()
        path.relative_to(pack.resolve())
        contents[name] = path.read_bytes()

    with ZipFile(destination, "w") as archive:
        entry(archive, "index.json", json.dumps({"packs": [folder]}).encode())
        for name, data in contents.items():
            entry(archive, folder + "/" + name, data)


if __name__ == "__main__":
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("pack", type=Path)
    cli.add_argument("destination", type=Path)
    args = cli.parse_args()
    package(args.pack, args.destination)
