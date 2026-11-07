"""Build the current commit and package it as one ROM-free zip.

This is OutRun Arcade's packager until the game moves to the toolkit's shared
packager (mod.json, standalone kind). It refuses uncommitted changes, builds
HEAD with CMake, copies the app (exe, DLLs, res/), notices, setup guide and the
engine source, writes release-manifest.json (SHA-256 per file) and zips the
folder with a .sha256 file beside it. It never publishes anything.

    python tools/package/Private-WindowsStage.py
    python tools/package/Private-WindowsStage.py --build-dir obj/package/build --out obj/package
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import zipfile

REPO = Path(__file__).resolve().parents[2]
EXE = 'cannonball-se.exe'
# The build must produce these beside the exe; any other DLL there ships too.
DLLS = ['SDL2.dll', 'tinyxml2.dll', 'libEGL.dll', 'libGLESv2.dll', 'mpg123.dll', 'zlib1.dll', 'WheelFfb.dll']
NOTICES = ['docs/license.txt', 'docs/CannonBall-SE-license.txt', 'docs/THIRD-PARTY-NOTICES.md', 'docs/LGPL-2.1.txt',
           'docs/license_mame.txt', 'docs/license_atari800.txt']
# ROMs never ship, not even in the source zip. Player data (scores, stats, a live
# config.xml) never ships either; res/config.xml is the engine's default template.
ROM = re.compile(r'^(epr|mpr|opr)-|\.rom$', re.I)
PLAYER = re.compile(r'^(hiscores.*|play_stats)\.|^config\.xml$', re.I)


def git(*args):
    return subprocess.check_output(['git', '-C', str(REPO), *args], text=True).strip()


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def refuse_rom_names(names, player_files=True):
    def bad(name):
        leaf = name.rsplit('/', 1)[-1]
        return ROM.search(leaf) or (player_files and PLAYER.search(leaf) and not name.startswith('res/'))
    found = [n for n in names if bad(n)]
    if found:
        raise SystemExit('Refusing to package ROM or player files: ' + ', '.join(found[:5]))


def vcpkg_toolchain(root):
    root = root or os.environ.get('VCPKG_ROOT')
    if not root:
        vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        found = subprocess.run([str(vswhere), '-latest', '-requires', 'Microsoft.VisualStudio.Component.Vcpkg',
                                '-property', 'installationPath'], capture_output=True, text=True).stdout.strip()
        if not found:
            raise SystemExit('No vcpkg found: pass --vcpkg-root or set VCPKG_ROOT (docs/Compiling-On-Windows.txt).')
        root = Path(found) / 'VC/vcpkg'
    return Path(root) / 'scripts/buildsystems/vcpkg.cmake'


def build(build_dir, cmake, vcpkg_root):
    cache = build_dir / 'CMakeCache.txt'
    if cache.exists():
        home = re.search(r'^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$', cache.read_text(encoding='utf-8', errors='replace'), re.M)
        if not home or Path(home[1]).resolve() != REPO.resolve():
            raise SystemExit(f'{build_dir} was configured for another source folder; use a new --build-dir.')
    else:
        subprocess.run([cmake, '-S', str(REPO), '-B', str(build_dir), '-A', 'x64',
                        '-DCMAKE_TOOLCHAIN_FILE=' + str(vcpkg_toolchain(vcpkg_root))], check=True)
    subprocess.run([cmake, '--build', str(build_dir), '--config', 'Release', '--target', 'cannonball-se', '--parallel'], check=True)
    return build_dir / 'Release'


def stage(release, destination, commit, version):
    """Assemble the app folder from a Release build output and the committed tree."""
    if destination.exists():
        raise SystemExit(f'{destination} already exists; remove it or package a new commit.')
    missing = [n for n in [EXE, *DLLS] if not (release / n).is_file()]
    if missing:
        raise SystemExit('The build output is missing: ' + ', '.join(missing))
    tracked = git('ls-tree', '-r', '--name-only', commit).splitlines()
    refuse_rom_names(tracked, player_files=False)
    destination.mkdir(parents=True)
    copies = [(release / EXE, EXE)] + [(p, p.name) for p in sorted(release.glob('*.dll'))]
    copies += [(REPO / n, n) for n in tracked if n.startswith('res/')]
    copies += [(REPO / n, 'licenses/' + Path(n).name) for n in NOTICES]
    copies += [(REPO / n, 'licenses/' + Path(n).name) for n in tracked if n.startswith('licenses/')]
    copies += [(REPO / 'docs/OUTRUN-ARCADE-SETUP.md', 'docs/OUTRUN-ARCADE-SETUP.md')]
    for source, relative in copies:
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    # The CannonBall license requires the complete modified source with the binary.
    source_zip = destination / 'source' / f'cannonball-se-{commit[:7]}-source.zip'
    source_zip.parent.mkdir()
    subprocess.run(['git', '-C', str(REPO), 'archive', '--format=zip', '-o', str(source_zip), commit], check=True)
    (destination / 'README.txt').write_text(
        f'OutRun Arcade (CannonBall-SE) {version}\n\n'
        'Runs the original OutRun arcade game (revision B) with wheel and force feedback support.\n'
        'ROMs are not included. Put your own OutRun revision B ROM files in a folder named roms\n'
        'beside cannonball-se.exe, then start cannonball-se.exe from this folder.\n'
        'Updating: copy these files over your existing folder. Your config.xml, scores and ROMs are kept.\n\n'
        'Setup guide: docs/OUTRUN-ARCADE-SETUP.md. Licenses: licenses/. Source: source/.\n'
        'Not for sale; see licenses/license.txt and licenses/CannonBall-SE-license.txt.\n'
        f'Source commit {commit}.\n', encoding='utf-8', newline='\r\n')
    files = sorted(p for p in destination.rglob('*') if p.is_file())
    refuse_rom_names([p.relative_to(destination).as_posix() for p in files])
    manifest = {'schema': 'dbce.release-manifest@1', 'id': 'outrun-arcade', 'name': 'OutRun Arcade (CannonBall-SE)',
                'version': version, 'commit': commit,
                'files': [{'path': p.relative_to(destination).as_posix(), 'bytes': p.stat().st_size, 'sha256': sha256(p)}
                          for p in files]}
    (destination / 'release-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    return manifest


def write_zip(folder):
    archive = folder.with_name(folder.name + '.zip')
    if archive.exists():
        raise SystemExit(f'{archive} already exists.')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        for p in sorted(folder.rglob('*')):
            if p.is_file():
                z.write(p, p.relative_to(folder).as_posix())
    digest = sha256(archive)
    archive.with_name(archive.name + '.sha256').write_text(f'{digest}  {archive.name}\n', encoding='utf-8')
    return archive, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--build-dir', type=Path, default=REPO / 'obj/package/build')
    parser.add_argument('--out', type=Path, default=REPO / 'obj/package')
    parser.add_argument('--vcpkg-root', type=Path)
    parser.add_argument('--cmake', default='cmake')
    args = parser.parse_args()
    if git('status', '--porcelain', '--untracked-files=normal'):
        raise SystemExit('Commit or stash your changes first: a package is built from a commit.')
    commit = git('rev-parse', 'HEAD')
    version = git('show', '-s', '--format=%cd', '--date=format:%Y.%m.%d', commit) + f' ({commit[:7]})'
    release = build(args.build_dir.resolve(), args.cmake, args.vcpkg_root)
    if git('status', '--porcelain', '--untracked-files=normal') or git('rev-parse', 'HEAD') != commit:
        raise SystemExit('The source changed during the build; package again.')
    folder = args.out.resolve() / f'OutRun Arcade {version}'
    manifest = stage(release, folder, commit, version)
    archive, digest = write_zip(folder)
    print(json.dumps({'zip': str(archive), 'sha256': digest, 'commit': commit, 'files': len(manifest['files'])}))


if __name__ == '__main__':
    main()
