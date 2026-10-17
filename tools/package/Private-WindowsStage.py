"""ROM-free private producer for the frozen d128256 review snapshot.

prepare exports exact reviewed Git blobs and snapshots locally pinned dependency
inputs. build invokes existing CMake, never the game. stage assembles an expanded
tree and invokes the unchanged review-only verifier with an external contract.
No downloads, installs, public archives, release clearance or engine edits.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tarfile

REPO = Path(__file__).resolve().parents[2]
PACKAGE = REPO / 'tools/package'
ENGINE = 'd128256afa2c9e7ac4dc164125c30e95a7d1ab3c'
TREE = '559f22a398640674b30c1fe6950f185644486b33'
RUNTIME = ['cannonball-se.exe', 'SDL2.dll', 'tinyxml2.dll', 'libEGL.dll', 'libGLESv2.dll', 'mpg123.dll', 'zlib1.dll']
RESOURCES = ['gamecontrollerdb.txt', 'tilemap.bin', 'tilepatch.bin', 'Cannonball-Shader-Vertex.glsl', 'Cannonball-Shader-Fragment.glsl', 'Cannonball-Shader-Fragment-Fast.glsl']
NOTICES = ['license.txt', 'CannonBall-SE-license.txt', 'THIRD-PARTY-NOTICES.md', 'LGPL-2.1.txt', 'license_mame.txt', 'license_atari800.txt', 'GPL-2.0.txt', 'dirent-LICENSE.txt', 'sdl2-LICENSE.txt', 'tinyxml2-LICENSE.txt', 'mpg123-LICENSE.txt', 'angle-LICENSE.txt', 'zlib-LICENSE.txt']


def digest(data, algorithm='sha256'):
    return hashlib.new(algorithm, data).hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def git(*args):
    return subprocess.check_output(['git', '-C', str(REPO), *args])


def reject_links(path):
    path = Path(os.path.abspath(path))
    for item in [path, *path.parents]:
        try:
            attributes = getattr(item.lstat(), 'st_file_attributes', 0)
        except FileNotFoundError:
            attributes = 0
        if item.is_symlink() or attributes & 0x400:
            raise ValueError('Reparse/link input refused: ' + str(item))


def files(root):
    reject_links(root)
    for path in sorted(root.rglob('*')):
        reject_links(path)
        if path.is_file():
            yield path


def inventory(root):
    return [{'path': f.relative_to(root).as_posix(), 'bytes': f.stat().st_size,
             'sha256': digest(f.read_bytes())} for f in files(root)]


def verify_files(root, expected):
    if inventory(root) != expected:
        raise ValueError('Input files changed: ' + str(root))


def verify_dependency_metadata(root, evidence):
    for component in evidence['dependencies']:
        name = component['name']
        for filename, field in [('vcpkg.spdx.json', 'spdxSHA256'), ('vcpkg_abi_info.txt', 'abiInfoSHA256'), ('copyright', 'copyrightSHA256')]:
            path = root / 'share' / name / filename
            reject_links(path)
            if digest(path.read_bytes()) != component[field].lower():
                raise ValueError('Recorded dependency metadata differs: ' + name + '/' + filename)
        spdx = read_json(root / 'share' / name / 'vcpkg.spdx.json')
        for row in spdx['files']:
            if not row['SPDXID'].startswith('SPDXRef-binary-file-'):
                continue
            relative = PurePosixPath(row['fileName'].removeprefix('./'))
            if relative.is_absolute() or '..' in relative.parts or '\\' in str(relative) or ':' in str(relative):
                raise ValueError('Unsafe SPDX file path')
            # vcpkg removes package-root CONTROL/BUILD_INFO during installation.
            # Check the selected compiler/linker/runtime/CMake input subtrees.
            if not relative.parts or relative.parts[0] not in ['include', 'lib', 'bin', 'share', 'debug']:
                continue
            path = root / relative
            reject_links(path)
            checksum = next(x['checksumValue'] for x in row['checksums'] if x['algorithm'] == 'SHA256')
            if digest(path.read_bytes()) != checksum:
                raise ValueError('Dependency file differs from pinned SPDX: ' + str(relative))


def git_blob(data):
    return digest(b'blob ' + str(len(data)).encode() + b'\0' + data, 'sha1')


def export_engine(destination, approved):
    if approved['commit'] != ENGINE or approved['tree'] != TREE or len(approved['files']) != 148:
        raise ValueError('Expected unchanged reviewed 148-blob engine inventory')
    if git('rev-parse', ENGINE + '^{tree}').decode().strip() != TREE:
        raise ValueError('Engine tree identity mismatch')
    tracked = {}
    for record in git('ls-tree', '-rz', ENGINE).split(b'\0'):
        if not record:
            continue
        metadata, name = record.split(b'\t', 1)
        mode, kind, sha = metadata.decode().split()
        tracked[name.decode()] = (mode, kind, sha)
    hashes = [item['blob'] for item in approved['files']]
    result = subprocess.check_output(['git', '-C', str(REPO), 'cat-file', '--batch'],
                                     input=('\n'.join(hashes) + '\n').encode())
    offset = 0
    for item in approved['files']:
        relative = item['path']
        # These two approved C++ files expose a contradiction in the frozen
        # verifier's ROM-name rule. Export raw reviewed source accurately; do
        # not bypass or modify the verifier when later staging it.
        if relative not in ['src/main/roms.cpp', 'src/main/roms.hpp']:
            check_path('source/' + relative)
        mode, kind, sha = tracked[relative]
        if sha != item['blob'] or kind != 'blob' or mode not in ['100644', '100755']:
            raise ValueError('Reviewed source path/blob differs')
        end = result.index(b'\n', offset)
        blob, blob_kind, length = result[offset:end].decode().split()
        start = end + 1
        end = start + int(length)
        data = result[start:end]
        if blob != sha or blob_kind != 'blob' or result[end:end+1] != b'\n':
            raise ValueError('Malformed Git batch output')
        offset = end + 1
        if git_blob(data) != item['blob']:
            raise ValueError('Git blob bytes differ')
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    if offset != len(result):
        raise ValueError('Unexpected extra Git batch output')


def check_path(relative):
    # Match the frozen verifier's privacy/Windows filename boundaries.
    if not relative or re.search(r'(^/|\\|:|[<>"|?*\x00-\x1F]|//|/$)', relative):
        raise ValueError('Unsafe path: ' + relative)
    reviewed_source = relative in ['source/src/main/roms.cpp', 'source/src/main/roms.hpp']
    for part in relative.split('/'):
        if part in ['.', '..', ''] or part.endswith((' ', '.')):
            raise ValueError('Unsafe path: ' + relative)
        source_token = reviewed_source and part in ['roms.cpp', 'roms.hpp']
        if ((not source_token and re.search(r'^(roms|run|userdata|saves|\.git|\.env|credentials)(\.|$)', part, re.I)) or
                re.search(r'^(epr|mpr)[-_]|^(hiscores.*|play_stats)\.|\.(zip|7z|rar|tar|gz|mp3|wav|ym|rom|ini|log)$|^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)', part, re.I)):
            raise ValueError('Frozen verifier excludes path: ' + relative)
    if re.search(r'(^|/)config\.xml$', relative, re.I) and relative not in ['defaults/config.xml', 'source/res/config.xml']:
        raise ValueError('Configuration outside reviewed defaults')


def archive_members(archive, component):
    """Yield regular source files; record verifier exclusions without renaming.

    Traversal and ambiguous Windows paths fail. Audio/archive/ini test fixtures
    and symlinks stay excluded and explicitly prevent a source-completeness claim.
    """
    seen = set()
    with tarfile.open(archive) as tar:
        members = tar.getmembers()
        roots = {PurePosixPath(m.name).parts[0] for m in members if PurePosixPath(m.name).parts}
        strip_root = len(roots) == 1 and any('/' in m.name for m in members)
        for member in members:
            raw = PurePosixPath(member.name)
            if raw.is_absolute() or '..' in raw.parts or '\\' in member.name:
                raise ValueError('Unsafe archive member: ' + member.name)
            if member.isdir():
                continue
            parts = raw.parts[1:] if strip_root else raw.parts
            if not parts:
                continue
            relative = 'dependency-source/' + component + '/upstream/' + '/'.join(parts)
            if relative.casefold() in seen:
                raise ValueError('Case-colliding archive source: ' + member.name)
            seen.add(relative.casefold())
            reason = None
            if not member.isfile():
                reason = 'Nonregular/link member not materialized'
            elif member.size == 0:
                reason = 'Empty upstream marker cannot satisfy frozen positive-byte file contract'
            else:
                try:
                    check_path(relative)
                except ValueError as error:
                    reason = str(error)
            yield relative, (tar.extractfile(member).read() if reason is None else None), reason, member.name


def run_command(args, log):
    with log.open('w', encoding='utf-8') as output:
        output.write(json.dumps([str(x) for x in args]) + '\n')
        output.flush()
        subprocess.run([str(x) for x in args], stdout=output, stderr=subprocess.STDOUT, check=True)


def checked_state(root):
    state = read_json(root / 'prepare.json')
    for name, sha in state['producerFiles'].items():
        if digest((REPO / name).read_bytes()) != sha:
            raise ValueError('Producer/policy input changed: ' + name)
    verify_files(root / 'engine-source', state['engineFiles'])
    verify_files(root / 'dependencies', state['dependencyFiles'])
    for row in state['archives']:
        reject_links(Path(row['input']))
        if digest(Path(row['input']).read_bytes(), 'sha512') != row['sha512']:
            raise ValueError('Cached source archive changed: ' + row['name'])
    return state


def prepare(args, root):
    if root.exists() or git('status', '--porcelain', '--untracked-files=normal').strip():
        raise ValueError('Prepare requires clean committed producer and a new run directory')
    plan = read_json(PACKAGE / 'private-windows-inputs.json')
    evidence = read_json(PACKAGE / 'dependency-evidence.json')
    approved = read_json(PACKAGE / 'source-inventory.d128256.json')
    if plan['engineCommit'] != ENGINE or plan['engineTree'] != TREE or plan['releaseReady'] is not False or plan['reviewOnly'] is not True:
        raise ValueError('Input plan must retain frozen engine and review-only policy')
    reject_links(args.dependency_root)
    reject_links(args.download_cache)
    dep = args.dependency_root.resolve()
    cache = args.download_cache.resolve()
    reject_links(dep)
    reject_links(cache)
    for row in evidence['runtime']:
        if row['name'] != 'cannonball-se.exe' and digest((dep / 'bin' / row['name']).read_bytes()) != row['sha256'].lower():
            raise ValueError('Retained dependency binary identity differs: ' + row['name'])
    verify_dependency_metadata(dep, evidence)
    archives = []
    for row in plan['archives']:
        archive = cache / row['archive']
        reject_links(archive)
        if digest(archive.read_bytes(), 'sha512') != row['sha512']:
            raise ValueError('Upstream archive does not match recorded recipe: ' + row['name'])
        archives.append(dict(row, input=str(archive)))
    # Validate all selected inputs before creating the new run.
    root.mkdir(parents=True)
    export_engine(root / 'engine-source', approved)
    # Imported CMake targets validate their debug paths even for Release builds.
    # Snapshot those inputs too; only the six pinned release DLLs enter the stage.
    for directory in ['include', 'lib', 'bin', 'share', 'debug']:
        for source in files(dep / directory):
            target = root / 'dependencies' / source.relative_to(dep)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
    verify_dependency_metadata(root / 'dependencies', evidence)
    producer_names = ['tools/package/Private-WindowsStage.py', 'tools/package/Build-PrivateWindowsStage.ps1',
                      'tools/package/private-windows-inputs.json', 'tools/package/dependency-evidence.json',
                      'tools/package/Test-ReleaseStage.ps1', 'tools/package/source-inventory.d128256.json',
                      'tools/package/unresolved-evidence.json']
    state = {'schema': 'dbce-outrun-private-prepare-v1', 'producerCommit': git('rev-parse', 'HEAD').decode().strip(),
             'engineCommit': ENGINE, 'engineTree': TREE, 'reviewOnly': True, 'releaseReady': False,
             'producerFiles': {n: digest((REPO / n).read_bytes()) for n in producer_names},
             'engineFiles': inventory(root / 'engine-source'), 'dependencyFiles': inventory(root / 'dependencies'),
             'dependencyOrigin': str(dep), 'archives': archives,
             'dependencyCompilerProvenance': 'Historical DLL compilers remain missing; engine compiler receipt does not cover DLLs.'}
    write_json(root / 'prepare.json', state)
    print(json.dumps({'prepared': True, 'engineSource': ENGINE, 'producerSource': state['producerCommit'], 'releaseReady': False}))


def build(args, root):
    state = checked_state(root)
    if (root / 'build').exists() or (root / 'build-receipt.json').exists():
        raise ValueError('Preserve previous build; prepare a new run')
    deps = root / 'dependencies'
    command = [args.cmake, '-S', root / 'engine-source', '-B', root / 'build', '-G', 'Visual Studio 17 2022',
               '-A', 'x64', '-T', 'v143,version=14.44.35207', '-DCMAKE_SYSTEM_VERSION=10.0.26100.0',
               '-DCMAKE_TOOLCHAIN_FILE:FILEPATH=', '-DVCPKG_MANIFEST_INSTALL=OFF',
               '-DCMAKE_PREFIX_PATH=' + str(deps), '-DCMAKE_INCLUDE_PATH=' + str(deps / 'include'),
               '-DCMAKE_LIBRARY_PATH=' + str(deps / 'lib'), '-DMPG123_LIBRARY=' + str(deps / 'lib/mpg123.lib'),
               '-DMPG123_INCLUDE_DIR=' + str(deps / 'include'), '-DWITH_MP3=ON']
    run_command(command, root / 'configure.log')
    build_command = [args.cmake, '--build', root / 'build', '--config', 'Release', '--target', 'cannonball-se', '--parallel', '2']
    run_command(build_command, root / 'build.log')
    # CMake configure/build must not alter the exported source or private dep snapshot.
    checked_state(root)
    compiler_files = list((root / 'build/CMakeFiles').glob('*/CMakeCXXCompiler.cmake'))
    if len(compiler_files) != 1:
        raise ValueError('Ambiguous compiler identification')
    compiler = compiler_files[0].read_text()
    version = re.search(r'set\(CMAKE_CXX_COMPILER_VERSION "([^"]+)"', compiler)[1]
    compiler_exe = Path(re.search(r'set\(CMAKE_CXX_COMPILER "([^"]+)"', compiler)[1])
    project = (root / 'build/cannonball-se.vcxproj').read_text(encoding='utf-8-sig')
    sdk = re.search(r'<WindowsTargetPlatformVersion>([^<]+)', project)[1]
    exe = root / 'build/Release/cannonball-se.exe'
    data = exe.read_bytes()
    pe = int.from_bytes(data[60:64], 'little')
    if data[:2] != b'MZ' or data[pe:pe+6] != b'PE\0\0\x64\x86':
        raise ValueError('Expected actual Windows x64 engine output')
    receipt = {'schema': 'dbce-outrun-private-build-v1', 'producerCommit': state['producerCommit'],
               'source_commit': ENGINE, 'source_tree': TREE, 'architecture': 'x64', 'configuration': 'Release',
               'compiler': 'MSVC ' + version, 'compilerPath': str(compiler_exe), 'compilerSha256': digest(compiler_exe.read_bytes()),
               'sdk': sdk, 'configure_arguments': [str(x) for x in command[1:]], 'build_arguments': [str(x) for x in build_command[1:]],
               'cmakeVersion': subprocess.check_output([args.cmake, '--version'], text=True).splitlines()[0],
               'prepareSha256': digest((root / 'prepare.json').read_bytes()), 'engineSha256': digest(data),
               'evidence': {str(f.relative_to(root)): digest(f.read_bytes()) for f in [root / 'configure.log', root / 'build.log', root / 'build/CMakeCache.txt', compiler_files[0]]},
               'reusedEngineBinary': False, 'dependencyCompilerProvenance': state['dependencyCompilerProvenance'], 'reviewOnly': True, 'releaseReady': False}
    write_json(root / 'build-receipt.json', receipt)
    print(json.dumps({'built': True, 'engineSha256': receipt['engineSha256'], 'reviewOnly': True, 'releaseReady': False}))


def stage(args, root):
    state = checked_state(root)
    receipt = read_json(root / 'build-receipt.json')
    exe = root / 'build/Release/cannonball-se.exe'
    if receipt['source_commit'] != ENGINE or receipt['prepareSha256'] != digest((root / 'prepare.json').read_bytes()) or receipt['engineSha256'] != digest(exe.read_bytes()):
        raise ValueError('Build receipt/input/output identity differs')
    for name, sha in receipt['evidence'].items():
        if digest((root / name).read_bytes()) != sha:
            raise ValueError('Build evidence changed')
    destination = root / 'stage'
    if destination.exists() or (root / 'contract.json').exists():
        raise ValueError('Preserve previous stage/contract; prepare a new run')
    destination.mkdir()
    entries = []
    def add(relative, role, data):
        check_path(relative)
        target = destination / relative
        if target.exists():
            raise ValueError('Duplicate stage path: ' + relative)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        entries.append({'path': relative, 'role': role, 'bytes': len(data), 'sha256': digest(data)})
    for name in RUNTIME:
        source = exe if name == RUNTIME[0] else root / 'dependencies/bin' / name
        add(name, 'runtime', source.read_bytes())
    for item in state['engineFiles']:
        add('source/' + item['path'], 'source', (root / 'engine-source' / item['path']).read_bytes())
    for name in RESOURCES:
        add('res/' + name, 'resource', (root / 'engine-source/res' / name).read_bytes())
    for name in NOTICES:
        source = ('docs/' if name in NOTICES[:6] else 'licenses/') + name
        add('notices/' + name, 'notice', git('show', state['producerCommit'] + ':' + source))
    add('docs/OUTRUN-ARCADE-SETUP.md', 'documentation', git('show', state['producerCommit'] + ':docs/OUTRUN-ARCADE-SETUP.md'))
    dependencies = []
    exclusions = []
    evidence = read_json(PACKAGE / 'dependency-evidence.json')
    for row in state['archives']:
        paths = []
        for relative, data, reason, original in archive_members(row['input'], row['name']):
            if reason:
                exclusions.append({'component': row['name'], 'archivePath': original, 'reason': reason})
            else:
                add(relative, 'dependency-source', data)
                paths.append(relative)
        recipe = evidence['sourceRecipes']['ports'][row['name']]
        for field, name in [('manifest', 'vcpkg.json'), ('recipe', 'portfile.cmake')]:
            relative = 'dependency-source/' + row['name'] + '/recipe/' + name
            add(relative, 'dependency-source', recipe[field]['content'].encode('utf-8'))
            paths.append(relative)
        dependencies.append({'name': row['name'], 'version': row['version'], 'upstream': row['upstream'], 'source_paths': paths,
                             'notice_paths': ['notices/' + row['name'] + '-LICENSE.txt']})
    for name in ['prepare.json', 'build-receipt.json']:
        add('provenance/' + name, 'documentation', (root / name).read_bytes())
    add('provenance/dependency-evidence.json', 'documentation', (PACKAGE / 'dependency-evidence.json').read_bytes())
    add('provenance/unresolved-evidence.json', 'documentation', (PACKAGE / 'unresolved-evidence.json').read_bytes())
    source_limits = {'reviewOnly': True, 'releaseReady': False, 'completeCorrespondingSourcesReviewed': False,
                     'excludedArchiveMembers': exclusions,
                     'remaining': 'Unpatched upstream archives plus recorded port recipes; recipe patches, ANGLE Chromium zlib/WebKit build support, transitive notice/source completeness and DLL compiler history remain unresolved. All seven frozen holds remain. No source-completeness or license-compatibility claim.'}
    add('provenance/source-coverage-limits.json', 'documentation', (json.dumps(source_limits, indent=2) + '\n').encode())
    approved = read_json(PACKAGE / 'source-inventory.d128256.json')
    contract = {'schema': 'dbce-outrun-arcade-stage-v1', 'repository': 'd-b-c-e/dbce-mods-outrun-arcade',
                'source': {'commit': ENGINE, 'tree': TREE, 'inventory': approved['files']}, 'build': receipt,
                'dependencies': dependencies, 'files': entries}
    write_json(root / 'contract.json', contract)
    run_command([args.pwsh, '-NoProfile', '-File', PACKAGE / 'Test-ReleaseStage.ps1', '-StageRoot', destination,
                 '-ContractPath', root / 'contract.json'], root / 'verifier.log')
    write_json(root / 'stage-receipt.json', {'schema': 'dbce-outrun-private-stage-v1', 'producerCommit': state['producerCommit'],
               'engineCommit': ENGINE, 'engineTree': TREE, 'files': len(entries), 'contractSha256': digest((root / 'contract.json').read_bytes()),
               'verifierSha256': digest((PACKAGE / 'Test-ReleaseStage.ps1').read_bytes()), 'verifierLogSha256': digest((root / 'verifier.log').read_bytes()),
               'excludedUpstreamFixtureFiles': len(exclusions), 'reviewOnly': True, 'releaseReady': False,
               'verificationScope': 'File identity and declared coverage only; producer-generated external contract is retained for review, not public release approval.'})
    print(json.dumps({'staged': True, 'files': len(entries), 'reviewOnly': True, 'releaseReady': False, 'stage': str(destination)}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=['prepare', 'build', 'stage'])
    parser.add_argument('--run-root', type=Path, required=True)
    parser.add_argument('--dependency-root', type=Path)
    parser.add_argument('--download-cache', type=Path)
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--pwsh', default='pwsh')
    args = parser.parse_args()
    root = Path(os.path.abspath(args.run_root))
    reject_links(root)
    if not root.is_relative_to(REPO / 'obj/private-stage') or root == REPO / 'obj/private-stage':
        raise ValueError('Run root must be a new named directory under this producer worktree obj/private-stage')
    if args.phase == 'prepare' and (args.dependency_root is None or args.download_cache is None):
        parser.error('prepare requires explicit dependency-root and download-cache')
    {'prepare': prepare, 'build': build, 'stage': stage}[args.phase](args, root)


if __name__ == '__main__':
    main()
