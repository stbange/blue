"""Give the native editor unit suite its own writable copy of the content."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory() as folder:
    root = Path(folder)
    content = root / "content"
    shutil.copytree(sys.argv[2], content)
    subprocess.run([sys.argv[1], content, root / "recording.wav", root], check=True)
