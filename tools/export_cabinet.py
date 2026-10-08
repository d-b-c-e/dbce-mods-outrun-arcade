"""Export strictly replayed cabinet requests for software-only analysis.

Recorded min/max settings are retained. No retuning, device calls or physical
delivery claim. HUD speed is the original display argument, not SI velocity.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess

import replay_cabinet


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def export(source: Path, replayer: Path, output: Path):
    source, replayer, output = source.resolve(), replayer.resolve(), output.resolve()
    replay_cabinet.require(not output.exists(), "output already exists")
    binary = replayer.read_bytes()
    result = replay_cabinet.replay(source, replayer)
    raw = source.read_bytes()
    replay_cabinet.require(digest(raw) == result["sourceSha256"], "source changed after replay")
    replay_cabinet.require(digest(binary) == result["replayerSha256"] and replayer.read_bytes() == binary,
                           "replayer changed during replay")
    records = [json.loads(line, object_pairs_hook=replay_cabinet.unique) for line in raw.splitlines()]
    samples = [record["sample"] for record in records[1:-1]]
    first = samples[0]["channels"]
    settings = {key: first["tuning." + key] for key in ("maximum", "minimum", "holdMs")}
    replay_cabinet.require(settings["maximum"] > 0, "zero configured cap has no cap-occupancy comparison; replay remains valid")
    stream = io.StringIO(newline="")
    writer = csv.writer(stream, lineterminator="\n")
    writer.writerow(["elapsedSeconds", "gameUpdate", "hudSpeedKph", "nominalRequest", "epoch", "eligible",
                     "command", "step", "crashCounter", "skidCounter", "steeringAdjust"])
    for sample in samples:
        c = sample["channels"]
        # OHud::blit_speed(0x110CB6, car_increment >> 16), followed by HUD_KPH1/2.
        # Preserve that integer argument; do not use the unqualified raw fixed-point value as m/s.
        writer.writerow([sample["elapsedSeconds"], int(c["update"]), int(c["input.carIncrement"]) >> 16,
                         int(c["force.nominal"]) / 10000.0, 0, 1, int(c["force.command"]), int(c["force.step"]),
                         int(c["input.crashCounter"]), int(c["input.skidCounter"]), int(c["input.steeringAdjust"])])
    csv_bytes = stream.getvalue().encode("utf-8")
    limits = ("Original cabinet-command@2 nominal constant requests at the recorded minimum/maximum. "
              "Combined cabinet response, not isolated tyre steering. No Strength-50 conversion. "
              "All native calls were refused by the capture mute. Holds in the analyzer describe this "
              "calculated sample sequence, not accepted actuator commands, watchdog behavior or torque. "
              "Speed is the integer KPH HUD argument, not measured SI velocity. Different drives are "
              "unmatched workloads; this case cannot set a normalization gain.")
    receipt = {"schema": "cannonball.cabinet-export@1", "replay": result, "settings": settings,
               "rows": len(samples), "csvSha256": digest(csv_bytes), "physicalOutput": False,
               "normalizationQualified": False, "limitations": limits,
               "speedContract": "oinitengine.cpp: car_increment >> 16 to OHud::blit_speed, HUD_KPH1/2",
               "eligibility": "complete original producer replay and recorded mapper; not owner-workload acceptance"}
    root = Path(__file__).resolve().parents[1]
    inputs = {"source.jsonl": raw, "cabinet_replay.exe": binary,
              "export_cabinet.py": Path(__file__).read_bytes(),
              "replay_cabinet.py": Path(replay_cabinet.__file__).read_bytes(),
              "oinitengine.cpp": (root / "src/main/engine/oinitengine.cpp").read_bytes(),
              "ohud.cpp": (root / "src/main/engine/ohud.cpp").read_bytes()}
    receipt["artifacts"] = {name: digest(value) for name, value in inputs.items()}
    validation = (json.dumps(receipt, indent=2, allow_nan=False) + "\n").encode("utf-8")
    manifest = {"schema": "dbce.ffb-normalization", "version": 1, "series": [{
        "id": "cannonball-cabinet-recorded", "game": "CannonBall-SE", "model": "cabinet-command@2",
        "quantity": "constant-force-request", "evidence": "synthetic" if result["captureKind"] == "synthetic producer fixture" else "recorded-model-output",
        "settings": settings, "limitations": limits, "validation": "Strict original producer/state/mapper replay; validation.json",
        "artifacts": [{"path": "validation.json", "sha256": digest(validation)}] +
                     [{"path": name, "sha256": digest(value)} for name, value in inputs.items()],
        "format": "csv", "source": {"path": "cabinet.csv", "sha256": digest(csv_bytes)},
        "mapping": {"time": "elapsedSeconds", "speed": "hudSpeedKph", "value": "nominalRequest", "valid": "eligible", "epoch": "epoch"},
        "cap": min(10000, settings["maximum"]) / 10000.0,
        "maxGapSeconds": 0.1}]}
    # Validate everything before creating any result. A failure leaves no apparent completed export.
    output.mkdir(parents=True, exist_ok=False)
    for name, data in inputs.items():
        with (output / name).open("xb") as file:
            file.write(data)
    with (output / "cabinet.csv").open("xb") as file:
        file.write(csv_bytes)
    with (output / "validation.json").open("xb") as file:
        file.write(validation)
    # Published last; without this file an interrupted directory is not a comparison input.
    with (output / "comparison.json").open("x", encoding="utf-8", newline="\n") as file:
        json.dump(manifest, file, indent=2, allow_nan=False)
        file.write("\n")
    return receipt


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path, help="new private evidence directory")
    parser.add_argument("--replayer", type=Path, required=True)
    args = parser.parse_args()
    try:
        receipt = export(args.source, args.replayer, args.output)
        print(f"Exported {receipt['rows']} replay-verified cabinet rows; no physical normalization claim: {args.output}")
    except (ValueError, KeyError, TypeError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, "REFUSED: " + str(error) + "\n")
