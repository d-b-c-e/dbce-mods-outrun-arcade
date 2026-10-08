"""Strict shared-session reader followed by original C++ producer replay.

The replayer has a fake sink and never links a wheel library. This does not
launch a game, drive hardware, or turn a synthetic case into an owner recording.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess

INPUTS = "gameState crashCounter skidCounter carIncrement roadCurve wheelState carXDiff steeringAdjust motorInput".split()
STATE = "command enabled positionChange control movement centred movementLatch speedIndex curveIndex counter smallChange steeringHistory1 steeringHistory2 steeringHistory3".split()
CHANNELS = {"update", "force.command", "force.step", "force.nominal", "tuning.maximum", "tuning.minimum", "tuning.holdMs", "delivery.muted", "delivery.result"}
CHANNELS |= {"input." + x for x in INPUTS} | {prefix + x for prefix in ("before.", "after.") for x in STATE}

def require(ok, message):
    if not ok:
        raise ValueError(message)

def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "duplicate JSON key: " + key)
        result[key] = value
    return result

def integer(value):
    require(type(value) in (int, float) and math.isfinite(value) and value == int(value), "non-integral channel")
    return int(value)

def prepare(source: Path, identity: list[str]):
    require(source.stat().st_size <= 64 * 1024 * 1024, "capture too large")
    raw = source.read_bytes()
    require(raw.endswith(b"\n"), "unterminated final record")
    lines = raw.splitlines()
    require(4 <= len(lines) <= 8194, "record count")
    require(all(0 < len(x) <= 256 * 1024 for x in lines), "record length")
    records = [json.loads(x, object_pairs_hook=unique) for x in lines]
    header, footer = records[0], records[-1]
    require(header.get("kind") == "metadata" and header.get("schema") == "dbce.wheel.session" and type(header.get("version")) is int and header.get("version") == 1, "session schema")
    meta = header["metadata"]
    require(meta.get("game") == "CannonBall-SE", "game identity")
    props = meta["properties"]
    require(props.get("contract") == "cannonball.cabinet-signal@1" and props.get("model") == "cabinet-command@2", "model contract")
    require([props.get("producerSha256"), props.get("mapperSha256")] == identity, "compiled producer/mapper identity differs")
    require(props.get("physicalOutput") == "false" and props.get("telemetryDelivery") == "false", "capture delivery domain")
    require(props.get("captureKind") in ("original gameplay observation", "synthetic producer fixture"), "capture kind")
    require(type(props["durationSeconds"]) is str and props["durationSeconds"].isdigit(), "duration representation")
    duration = int(props["durationSeconds"])
    require(1 <= duration <= 120, "duration")
    require(footer.get("kind") == "footer" and footer["footer"].get("completed") is True and footer["footer"].get("stopReason") == "stopped", "incomplete recording")
    samples = records[1:-1]
    counts = footer["footer"]["counts"]
    require(all(type(x) is int and x >= 0 for x in counts.values()), "count representation")
    require(counts["writtenSamples"] == counts["acceptedSamples"] == len(samples), "sample count mismatch")
    for key in ("acceptedMarkers", "writtenMarkers", "droppedSamples", "droppedMarkers", "queueFullCount", "contentionCount", "invalidCount", "limitCount", "errorCount", "rejectedAfterStop"):
        require(counts[key] == 0, "nonzero count: " + key)
    require(counts["dataBytesWritten"] == sum(len(x) + 1 for x in lines[:-1]), "written byte count mismatch")
    protocol, nominal, last = [], [], -1.0
    for index, record in enumerate(samples):
        require(record.get("kind") == "sample", "unexpected record kind")
        sample = record["sample"]
        require(type(sample["sequence"]) is int and sample["sequence"] == index, "sample sequence")
        elapsed = sample["elapsedSeconds"]
        require(type(elapsed) in (int, float) and math.isfinite(elapsed) and elapsed > last and elapsed >= 0, "sample time")
        micro = round(elapsed * 1000000)
        require(abs(micro / 1000000 - elapsed) <= 1e-10, "time is not in recorded microseconds")
        last = elapsed
        c = sample["channels"]
        require(set(c) == CHANNELS, "missing/unknown channels")
        require(integer(c["delivery.muted"]) == 1, "unmuted row")
        require(integer(c["delivery.result"]) == -1, "force call not refused by mute guard")
        fields = [micro, integer(c["update"])]
        fields += [integer(c["input." + key]) for key in INPUTS]
        fields += [integer(c[prefix + key]) for prefix in ("before.", "after.") for key in STATE]
        fields += [integer(c[key]) for key in ("force.command", "force.step", "force.nominal", "delivery.result", "tuning.maximum", "tuning.minimum", "tuning.holdMs")]
        require(len(fields) == 46, "internal protocol count")
        protocol.append("frame " + " ".join(map(str, fields)))
        nominal.append(integer(c["force.nominal"]))
    require(last == footer["footer"]["elapsedSeconds"], "footer time")
    protocol.append("end " + str(len(samples)))
    return duration, "\n".join(protocol) + "\n", {
        "schema": "cannonball.cabinet-replay@1", "sourceSha256": hashlib.sha256(raw).hexdigest(),
        "producerSha256": identity[0], "mapperSha256": identity[1], "rows": len(samples),
        "elapsedSeconds": last, "nominalMinimum": min(nominal), "nominalMaximum": max(nominal),
        "nonzeroNominalRows": sum(x != 0 for x in nominal),
        "domain": "original cabinet commands; nominal mapper calculation, no physical delivery",
        "captureKind": props["captureKind"],
        "normalizationQualified": False,
    }

def replay(source: Path, executable: Path):
    identity = subprocess.check_output([str(executable), "--identity"], text=True, timeout=15).split()
    require(len(identity) == 2 and all(len(x) == 64 and all(c in "0123456789abcdef" for c in x) for x in identity), "invalid replayer identity")
    duration, protocol, result = prepare(source, identity)
    run = subprocess.run([str(executable), str(duration)], input=protocol, text=True, capture_output=True, timeout=30)
    require(run.returncode == 0, "original producer replay refused: " + run.stderr.strip())
    result["status"] = "passed"
    result["replayerSha256"] = hashlib.sha256(executable.read_bytes()).hexdigest()
    result["replay"] = run.stdout.strip()
    return result

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--replayer", type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(replay(args.source.resolve(), args.replayer.resolve()), indent=2))
    except (ValueError, KeyError, TypeError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, "REFUSED: " + str(error) + "\n")
