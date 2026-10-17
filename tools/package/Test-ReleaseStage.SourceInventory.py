"""Full actual reviewed source + synthetic non-runnable runtime fixtures.

Invokes the production PowerShell verifier. This tests its identity/privacy
boundary, not a playable package, dependency completeness, or license clearance.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('producer', Path(__file__).with_name('Private-WindowsStage.py'))
producer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(producer)


class ActualSourceInventory(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.base = Path(cls.temporary.name)
        cls.original = cls.base / 'original'
        cls.original.mkdir()
        cls.stage = cls.original / 'stage'
        cls.stage.mkdir()
        cls.approved = producer.read_json(producer.PACKAGE / 'source-inventory.d128256.json')
        producer.export_engine(cls.stage / 'source', cls.approved)
        entries = []
        def add(path, role, data):
            destination = cls.stage / path
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
            entries.append({'path': path, 'role': role, 'bytes': len(data), 'sha256': producer.digest(data)})
        for row in cls.approved['files']:
            path = 'source/' + row['path']
            data = (cls.stage / path).read_bytes()
            entries.append({'path': path, 'role': 'source', 'bytes': len(data), 'sha256': producer.digest(data)})
        for name in producer.RUNTIME:
            add(name, 'runtime', ('synthetic non-runnable fixture: ' + name).encode())
        for name in producer.RESOURCES:
            add('res/' + name, 'resource', (cls.stage / 'source/res' / name).read_bytes())
        for name in producer.NOTICES:
            add('notices/' + name, 'notice', ('synthetic notice fixture: ' + name).encode())
        add('docs/OUTRUN-ARCADE-SETUP.md', 'documentation', b'synthetic fixture guide')
        dependencies = []
        for name in ['sdl2', 'tinyxml2', 'mpg123', 'angle', 'zlib']:
            source = 'dependency-source/' + name + '/source.txt'
            add(source, 'dependency-source', ('synthetic source fixture ' + name).encode())
            dependencies.append({'name': name, 'version': 'fixture-not-real', 'upstream': 'https://example.invalid/' + name,
                                 'source_paths': [source], 'notice_paths': ['notices/' + name + '-LICENSE.txt']})
        contract = {'schema': 'dbce-outrun-arcade-stage-v1', 'repository': 'd-b-c-e/dbce-mods-outrun-arcade',
                    'source': {'commit': producer.ENGINE, 'tree': producer.TREE, 'inventory': cls.approved['files']},
                    'build': {'source_commit': producer.ENGINE, 'architecture': 'x64', 'configuration': 'Release',
                              'compiler': 'synthetic fixture', 'sdk': 'synthetic fixture', 'configure_arguments': ['synthetic fixture']},
                    'dependencies': dependencies, 'files': entries}
        producer.write_json(cls.original / 'contract.json', contract)
        cls.runner = cls.base / 'Run-Verifier.ps1'
        cls.runner.write_text("param($Validator,$StageRoot,$ContractPath)\n$ErrorActionPreference='Stop'\n& $Validator -StageRoot $StageRoot -ContractPath $ContractPath | ConvertTo-Json -Depth 8\n")

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def fixture(self):
        destination = self.base / self._testMethodName
        shutil.copytree(self.original, destination)
        return destination, producer.read_json(destination / 'contract.json')

    def verify(self, root, contract, reject=None):
        producer.write_json(root / 'contract.json', contract)
        result = subprocess.run(['pwsh', '-NoProfile', '-File', str(self.runner), '-Validator', str(producer.PACKAGE / 'Test-ReleaseStage.ps1'),
                                 '-StageRoot', str(root / 'stage'), '-ContractPath', str(root / 'contract.json')], capture_output=True, text=True)
        if reject is None:
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            verdict = json.loads(result.stdout)
            self.assertTrue(verdict['verified'])
            self.assertTrue(verdict['reviewOnly'])
            self.assertIs(verdict['releaseReady'], False)
            self.assertEqual(7, len(verdict['unresolvedEvidence']))
        else:
            self.assertNotEqual(0, result.returncode)
            self.assertIn(reject, result.stdout + result.stderr)

    def extra(self, root, contract, path):
        target = root / 'stage' / path
        target.parent.mkdir(parents=True, exist_ok=True)
        data = b'synthetic forbidden-path sentinel'
        target.write_bytes(data)
        contract['files'].append({'path': path, 'role': 'documentation', 'bytes': len(data), 'sha256': producer.digest(data)})

    def test_full_actual_148_blob_source_passes_review_only(self):
        root, contract = self.fixture()
        self.verify(root, contract)

    def test_changed_source_bytes_fail_hash(self):
        root, contract = self.fixture()
        with (root / 'stage/source/src/main/roms.cpp').open('ab') as output:
            output.write(b'\nfixture mutation')
        self.verify(root, contract, 'size/hash mismatch')

    def test_rehashed_changed_source_still_fails_approved_blob(self):
        root, contract = self.fixture()
        path = 'source/src/main/roms.hpp'
        data = (root / 'stage' / path).read_bytes() + b'\nfixture mutation'
        (root / 'stage' / path).write_bytes(data)
        row = next(x for x in contract['files'] if x['path'] == path)
        row.update(bytes=len(data), sha256=producer.digest(data))
        self.verify(root, contract, 'source/resource Git blob mismatch')

    def test_raw_inventory_name_not_allowed_as_staged_artifact(self):
        root, contract = self.fixture()
        self.extra(root, contract, 'src/main/roms.cpp')
        self.verify(root, contract, 'private/ROM/archive path')

    def test_staged_prefix_not_allowed_in_inventory_context(self):
        root, contract = self.fixture()
        next(x for x in contract['source']['inventory'] if x['path'] == 'src/main/roms.cpp')['path'] = 'source/src/main/roms.cpp'
        self.verify(root, contract, 'private/ROM/archive path')

    def test_near_name_not_allowed(self):
        root, contract = self.fixture()
        self.extra(root, contract, 'source/src/main/roms.fake.cpp')
        self.verify(root, contract, 'private/ROM/archive path')

    def test_rom_bytes_path_not_allowed(self):
        root, contract = self.fixture()
        self.extra(root, contract, 'roms/epr-fixture.rom')
        self.verify(root, contract, 'private/ROM/archive path')

    def test_source_archive_not_allowed(self):
        root, contract = self.fixture()
        self.extra(root, contract, 'dependency-source/sdl2/source.tar.gz')
        self.verify(root, contract, 'private/ROM/archive path')

    def test_live_resource_config_not_allowed_as_artifact(self):
        root, contract = self.fixture()
        self.extra(root, contract, 'res/config.xml')
        self.verify(root, contract, 'personal configuration is disallowed')

    def test_wrong_source_role_still_rejected(self):
        root, contract = self.fixture()
        next(x for x in contract['files'] if x['path'] == 'source/src/main/roms.cpp')['role'] = 'documentation'
        self.verify(root, contract, 'missing corresponding source')

    def test_case_variant_is_not_an_exact_reviewed_path(self):
        root, contract = self.fixture()
        next(x for x in contract['files'] if x['path'] == 'source/src/main/roms.cpp')['path'] = 'source/src/main/ROMS.CPP'
        self.verify(root, contract, 'private/ROM/archive path')


if __name__ == '__main__':
    unittest.main(verbosity=2)
