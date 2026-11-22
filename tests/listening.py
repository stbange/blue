"""Render the four panel performances and match their integrated loudness."""
import argparse
import csv
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("renderer", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
args.output.mkdir(parents=True, exist_ok=True)


def measure(path):
    result = subprocess.run(
        ["ffmpeg", "-hide_banner", "-nostats", "-i", str(path), "-af",
         "loudnorm=print_format=json", "-f", "null", "-"],
        capture_output=True, text=True, check=True)
    data = json.loads(result.stderr[result.stderr.rindex("{"):result.stderr.rindex("}")+1])
    return float(data["input_i"]), float(data["input_tp"])


with tempfile.TemporaryDirectory() as tmp:
    levels = {}
    for name in ["Glass Tide", "Struck Wire", "Paper Cloud", "Step Current"]:
        source = Path(tmp) / (name + ".wav")
        subprocess.run([args.renderer.resolve(), root / "content/packs/blue.factory",
                        name, source, root / "content/performances" / (name + ".score")],
                       check=True)
        levels[name] = measure(source)
    # Preserve dynamics and leave at least 1 dB of true-peak headroom throughout.
    target = min(-23, *(loudness - peak - 1 for loudness, peak in levels.values()))
    measured = {}
    for name, (loudness, peak) in levels.items():
        out = args.output / (name + ".wav")
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i",
                        str(Path(tmp) / out.name), "-af", f"volume={target-loudness}dB",
                        "-c:a", "pcm_s24le", str(out)], check=True)
        matched, true_peak = measure(out)
        assert abs(matched-target) <= .25 and true_peak < 0, (name, matched, true_peak)
        measured[name] = {"integrated_LUFS": matched, "true_peak_dBTP": true_peak}
(args.output / "levels.json").write_text(json.dumps(measured, indent=2) + "\n")
with (args.output / "panel.csv").open("w", newline="") as file:
    sheet = csv.writer(file)
    sheet.writerow(["listener", "patch", "movement_1_to_5", "clarity_1_to_5",
                    "control_response_1_to_5", "observations"])
    for listener in range(1, 4):
        for name in measured:
            sheet.writerow([listener, name, "", "", "", ""])
print(json.dumps(measured, indent=2))
