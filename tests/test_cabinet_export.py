from pathlib import Path
import copy
import csv
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

recorder, replayer = map(lambda x: Path(x).resolve(), sys.argv[1:3])
del sys.argv[1:3]
sys.path.insert(0, str(Path(__file__).parents[1] / "tools"))
import export_cabinet as module


class ExportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cannonball-export-fixture-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        subprocess.run([str(recorder), "complete"], cwd=self.root, check=True, capture_output=True, timeout=15)
        self.source = next(self.root.rglob("source.jsonl"))
        self.output = self.root / "export"

    def change(self, mutate):
        records = [json.loads(x) for x in self.source.read_bytes().splitlines()]
        mutate(records)
        lines = [json.dumps(x, separators=(",", ":")) for x in records[:-1]]
        records[-1]["footer"]["counts"]["dataBytesWritten"] = sum(len(x.encode()) + 1 for x in lines)
        lines.append(json.dumps(records[-1], separators=(",", ":")))
        self.source.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")

    def test_actual_producer_export_and_hashes(self):
        receipt = module.export(self.source, replayer, self.output)
        self.assertEqual(receipt["rows"], 8)
        self.assertFalse(receipt["physicalOutput"])
        self.assertFalse(receipt["normalizationQualified"])
        manifest = json.loads((self.output / "comparison.json").read_text())
        s = manifest["series"][0]
        self.assertEqual(s["evidence"], "synthetic")
        self.assertNotIn("strength", s["settings"])
        for item in s["artifacts"] + [s["source"]]:
            self.assertEqual(hashlib.sha256((self.output / item["path"]).read_bytes()).hexdigest(), item["sha256"])
        with (self.output / "cabinet.csv").open(newline="") as f:
            rows = list(csv.DictReader(f))
        original = [json.loads(x)["sample"] for x in self.source.read_bytes().splitlines()[1:-1]]
        for exported, sample in zip(rows, original, strict=True):
            c = sample["channels"]
            self.assertEqual(float(exported["elapsedSeconds"]), sample["elapsedSeconds"])
            self.assertEqual(int(exported["hudSpeedKph"]), int(c["input.carIncrement"]) >> 16)
            self.assertEqual(float(exported["nominalRequest"]), c["force.nominal"] / 10000)
            self.assertEqual(int(exported["gameUpdate"]), c["update"])

    def test_original_label_retained_without_owner_acceptance(self):
        self.change(lambda r: r[0]["metadata"]["properties"].update(captureKind="original gameplay observation"))
        receipt = module.export(self.source, replayer, self.output)
        self.assertEqual(json.loads((self.output / "comparison.json").read_text())["series"][0]["evidence"], "recorded-model-output")
        self.assertFalse(receipt["normalizationQualified"])

    def test_does_not_overwrite(self):
        self.output.mkdir()
        (self.output / "sentinel").write_text("keep")
        with self.assertRaisesRegex(ValueError, "output already exists"):
            module.export(self.source, replayer, self.output)
        self.assertEqual((self.output / "sentinel").read_text(), "keep")
        self.assertEqual(len(list(self.output.iterdir())), 1)

    def test_partial_refused_before_output(self):
        self.change(lambda r: r[-1]["footer"].update(completed=False))
        with self.assertRaisesRegex(ValueError, "incomplete"):
            module.export(self.source, replayer, self.output)
        self.assertFalse(self.output.exists())

    def test_force_tamper_reaches_production_refusal(self):
        self.change(lambda r: r[2]["sample"]["channels"].update({"force.nominal": 50}))
        with self.assertRaisesRegex(ValueError, "original producer replay refused"):
            module.export(self.source, replayer, self.output)
        self.assertFalse(self.output.exists())

    def test_source_change_after_replay_refused(self):
        real = module.replay_cabinet.replay
        def changed(source, executable):
            result = real(source, executable)
            source.write_bytes(source.read_bytes() + b"\n")
            return result
        with patch.object(module.replay_cabinet, "replay", changed):
            with self.assertRaisesRegex(ValueError, "source changed after replay"):
                module.export(self.source, replayer, self.output)
        self.assertFalse(self.output.exists())


    def test_zero_configured_cap_refused_without_inventing_one(self):
        # A known replay result isolates the post-replay analysis boundary.
        self.change(lambda r: [x["sample"]["channels"].update({"tuning.maximum": 0, "tuning.minimum": 0,
                                                             "force.nominal": 0}) for x in r[1:-1]])
        result = {"sourceSha256": hashlib.sha256(self.source.read_bytes()).hexdigest(),
                  "replayerSha256": hashlib.sha256(replayer.read_bytes()).hexdigest()}
        with patch.object(module.replay_cabinet, "replay", return_value=result):
            with self.assertRaisesRegex(ValueError, "zero configured cap"):
                module.export(self.source, replayer, self.output)
        self.assertFalse(self.output.exists())

    def test_changed_replayer_refused(self):
        executable = self.root / "replayer.exe"
        executable.write_bytes(replayer.read_bytes())
        real = module.replay_cabinet.replay
        def changed(source, path):
            result = real(source, path)
            path.write_bytes(path.read_bytes() + b"changed")
            return result
        with patch.object(module.replay_cabinet, "replay", changed):
            with self.assertRaisesRegex(ValueError, "replayer changed during replay"):
                module.export(self.source, executable, self.output)
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
