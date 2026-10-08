from pathlib import Path
import copy
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest

recorder, replayer = map(lambda x: Path(x).resolve(), sys.argv[1:3])
del sys.argv[1:3]
spec = importlib.util.spec_from_file_location("replay_cabinet", Path(__file__).parents[1] / "tools/replay_cabinet.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class ReplayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="cannonball-replay-fixture-")
        cls.root = Path(cls.temp.name).resolve()
        run = subprocess.run([str(recorder), "complete"], cwd=cls.root, capture_output=True, text=True, timeout=15)
        if run.returncode:
            raise RuntimeError(run.stdout + run.stderr)
        cls.original = next(cls.root.rglob("source.jsonl"))
        cls.records = [json.loads(x) for x in cls.original.read_text().splitlines()]

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def changed(self, change):
        records = copy.deepcopy(self.records)
        change(records)
        # Recompute only envelope bytes so semantic mutations reach the original
        # C++ replayer instead of being caught just by the byte count.
        lines = [json.dumps(x, separators=(",", ":")) for x in records[:-1]]
        records[-1]["footer"]["counts"]["dataBytesWritten"] = sum(len(x.encode()) + 1 for x in lines)
        lines.append(json.dumps(records[-1], separators=(",", ":")))
        target = self.root / (self.id().split(".")[-1] + ".jsonl")
        target.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
        return target

    def refused(self, change):
        with self.assertRaises((ValueError, KeyError)):
            module.replay(self.changed(change), replayer)

    def test_actual_producer_writer_roundtrip(self):
        result = module.replay(self.original, replayer)
        self.assertEqual((result["status"], result["rows"], result["captureKind"]), ("passed", 8, "synthetic producer fixture"))
        self.assertFalse(result["normalizationQualified"])

    def test_source_identity(self):
        self.refused(lambda x: x[0]["metadata"]["properties"].update(producerSha256="0" * 64))

    def test_model_identity(self):
        self.refused(lambda x: x[0]["metadata"]["properties"].update(model="unknown"))

    def test_partial(self):
        self.refused(lambda x: x[-1]["footer"].update(completed=False))

    def test_unsafe_table_input(self):
        self.refused(lambda x: x[2]["sample"]["channels"].update({"input.motorInput": 0}))

    def test_missing_history(self):
        self.refused(lambda x: x[2]["sample"]["channels"].pop("before.steeringHistory1"))

    def test_gap(self):
        self.refused(lambda x: x[2]["sample"]["channels"].update(update=999))

    def test_mapper_output(self):
        self.refused(lambda x: x[2]["sample"]["channels"].update({"force.nominal": 10}))

    def test_nonfinite(self):
        self.refused(lambda x: x[2]["sample"]["channels"].update({"input.roadCurve": float("nan")}))

    def test_gameplay_interruption(self):
        self.refused(lambda x: x[2]["sample"]["channels"].update({"input.gameState": 1}))

    def test_unsupported_cabinet_movement(self):
        self.refused(lambda x: x[2]["sample"]["channels"].update({"before.movement": 1}))

    def test_plausible_but_wrong_producer_state(self):
        def change(x):
            x[2]["sample"]["channels"]["after.steeringHistory1"] += 1
            x[3]["sample"]["channels"]["before.steeringHistory1"] += 1
        target = self.changed(change)
        with self.assertRaisesRegex(ValueError, "producer mismatch row 1"):
            module.replay(target, replayer)

    def test_duplicate_key(self):
        target = self.root / "duplicate.jsonl"
        target.write_bytes(self.original.read_bytes().replace(b'"version":1', b'"version":1,"version":1', 1))
        with self.assertRaisesRegex(ValueError, "duplicate JSON key"):
            module.replay(target, replayer)

if __name__ == "__main__":
    unittest.main()
