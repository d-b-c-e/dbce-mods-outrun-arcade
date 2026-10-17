"""Focused producer boundaries; no compiler, executable or game is launched."""
import importlib.util
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('producer', Path(__file__).with_name('Private-WindowsStage.py'))
producer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(producer)


class ProducerBoundaries(unittest.TestCase):
    def archive(self, folder, rows):
        path = Path(folder) / 'fixture.tar'
        with tarfile.open(path, 'w') as tar:
            for name, data, link in rows:
                member = tarfile.TarInfo(name)
                if link:
                    member.type = tarfile.SYMTYPE
                    member.linkname = link
                    tar.addfile(member)
                else:
                    member.size = len(data)
                    tar.addfile(member, io.BytesIO(data))
        return path

    def test_actual_frozen_source_exports_exact_git_bytes_without_rom_directory(self):
        approved = producer.read_json(producer.PACKAGE / 'source-inventory.d128256.json')
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'source'
            producer.export_engine(root, approved)
            self.assertEqual(148, len(list(producer.files(root))))
            self.assertFalse((root / 'roms').exists())
            for row in approved['files']:
                self.assertEqual(row['blob'], producer.git_blob((root / row['path']).read_bytes()))

    def test_archive_traversal_refuses_instead_of_materializing(self):
        with tempfile.TemporaryDirectory() as folder:
            path = self.archive(folder, [('upstream/../../escape.txt', b'sentinel', None)])
            with self.assertRaisesRegex(ValueError, 'Unsafe archive'):
                list(producer.archive_members(path, 'fixture'))
            self.assertFalse((Path(folder).parent / 'escape.txt').exists())

    def test_windows_case_collision_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as folder:
            path = self.archive(folder, [('upstream/Header.h', b'A', None), ('upstream/header.h', b'B', None)])
            with self.assertRaisesRegex(ValueError, 'Case-colliding'):
                list(producer.archive_members(path, 'fixture'))

    def test_forbidden_media_empty_and_link_sources_are_explicit_exclusions(self):
        with tempfile.TemporaryDirectory() as folder:
            path = self.archive(folder, [('upstream/source.c', b'real source bytes', None),
                                        ('upstream/test.wav', b'audio fixture', None),
                                        ('upstream/empty.marker', b'', None),
                                        ('upstream/link.h', b'', '/outside/target')])
            rows = list(producer.archive_members(path, 'fixture'))
            self.assertEqual(b'real source bytes', rows[0][1])
            self.assertTrue(all(row[1] is None and row[2] for row in rows[1:]))

    def test_modified_input_and_unlisted_file_both_refuse(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            file = root / 'dependency.h'
            file.write_bytes(b'original input')
            expected = producer.inventory(root)
            file.write_bytes(b'tampered input')
            with self.assertRaisesRegex(ValueError, 'Input files changed'):
                producer.verify_files(root, expected)
            file.write_bytes(b'original input')
            (root / 'foreign.dll').write_bytes(b'unlisted')
            with self.assertRaisesRegex(ValueError, 'Input files changed'):
                producer.verify_files(root, expected)

    def test_review_inventory_cannot_substitute_current_engine(self):
        approved = producer.read_json(producer.PACKAGE / 'source-inventory.d128256.json')
        approved['commit'] = '870ed7ed365288ef780f9416edf2a01359260967'
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaisesRegex(ValueError, 'unchanged reviewed'):
                producer.export_engine(Path(folder), approved)

    def test_pinned_dependency_metadata_also_authenticates_header_bytes(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            header = root / 'include/component.h'
            header.parent.mkdir()
            header.write_bytes(b'approved header')
            metadata = root / 'share/component'
            metadata.mkdir(parents=True)
            producer.write_json(metadata / 'vcpkg.spdx.json', {'files': [{'fileName': './include/component.h',
                'SPDXID': 'SPDXRef-binary-file-0', 'checksums': [{'algorithm': 'SHA256', 'checksumValue': producer.digest(header.read_bytes())}]}]})
            (metadata / 'copyright').write_bytes(b'notice')
            (metadata / 'vcpkg_abi_info.txt').write_bytes(b'ABI identity')
            entry = {'name': 'component', **{field: producer.digest((metadata / name).read_bytes()) for name, field in
                [('vcpkg.spdx.json', 'spdxSHA256'), ('copyright', 'copyrightSHA256'), ('vcpkg_abi_info.txt', 'abiInfoSHA256')]}}
            producer.verify_dependency_metadata(root, {'dependencies': [entry]})
            header.write_bytes(b'tampered header')
            with self.assertRaisesRegex(ValueError, 'Dependency file differs'):
                producer.verify_dependency_metadata(root, {'dependencies': [entry]})
            (metadata / 'vcpkg.spdx.json').write_bytes(b'{}')
            with self.assertRaisesRegex(ValueError, 'Recorded dependency metadata differs'):
                producer.verify_dependency_metadata(root, {'dependencies': [entry]})


if __name__ == '__main__':
    unittest.main(verbosity=2)
