"""Focused checks of the packager's staging and zip steps, using a fake build output.

    python tools/package/Test-PrivateWindowsStage.py
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import zipfile

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('packager', HERE / 'Private-WindowsStage.py')
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)
REPO = packager.REPO


def expect_exit(action, text):
    try:
        action()
    except SystemExit as error:
        assert text in str(error), error
        return
    raise AssertionError('expected refusal: ' + text)


def main():
    (REPO / 'obj').mkdir(exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='packager-test-', dir=REPO / 'obj'))
    try:
        release = work / 'Release'
        release.mkdir()
        for name in [packager.EXE, *packager.DLLS]:
            (release / name).write_bytes(b'fake ' + name.encode())
        commit = packager.git('rev-parse', 'HEAD')

        expect_exit(lambda: packager.stage(work / 'missing', work / 'none', commit, 'test'), 'missing')
        folder = work / 'OutRun Arcade test'
        manifest = packager.stage(release, folder, commit, 'test')
        paths = {f['path'] for f in manifest['files']}
        required = {packager.EXE, *packager.DLLS, 'res/tilemap.bin', 'res/tilepatch.bin', 'res/gamecontrollerdb.txt',
                    'res/config.xml', 'licenses/license.txt', 'licenses/CannonBall-SE-license.txt',
                    'docs/OUTRUN-ARCADE-SETUP.md', 'README.txt', f'source/cannonball-se-{commit[:7]}-source.zip'}
        assert required <= paths, required - paths
        assert 'config.xml' not in paths and not any(p.startswith('roms/') for p in paths), paths
        for f in manifest['files']:
            data = (folder / f['path']).read_bytes()
            assert hashlib.sha256(data).hexdigest() == f['sha256'] and len(data) == f['bytes'], f['path']
        assert manifest['commit'] == commit
        assert json.loads((folder / 'release-manifest.json').read_text(encoding='utf-8')) == manifest
        with zipfile.ZipFile(folder / f'source/cannonball-se-{commit[:7]}-source.zip') as z:
            assert 'src/main/directx/cabinet_recording.cpp' in z.namelist()
        expect_exit(lambda: packager.stage(release, folder, commit, 'test'), 'already exists')

        archive, digest = packager.write_zip(folder)
        assert hashlib.sha256(archive.read_bytes()).hexdigest() == digest
        assert archive.with_name(archive.name + '.sha256').read_text(encoding='utf-8') == f'{digest}  {archive.name}\n'
        with zipfile.ZipFile(archive) as z:
            assert set(z.namelist()) == paths | {'release-manifest.json'}

        expect_exit(lambda: packager.refuse_rom_names(['roms/epr-10187.88']), 'epr-10187.88')
        expect_exit(lambda: packager.refuse_rom_names(['opr-10185.11']), 'opr-10185.11')
        expect_exit(lambda: packager.refuse_rom_names(['config.xml']), 'config.xml')
        expect_exit(lambda: packager.refuse_rom_names(['hiscores.xml']), 'hiscores.xml')
        packager.refuse_rom_names(['res/config.xml', 'src/main/roms.cpp', 'roms/README.md'])
        packager.refuse_rom_names(['tests/fixtures/config.xml'], player_files=False)
    finally:
        shutil.rmtree(work)
    print('PASS packager staging checks')


if __name__ == '__main__':
    main()
