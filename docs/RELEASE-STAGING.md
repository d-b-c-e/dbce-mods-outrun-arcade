# OutRun Arcade: verify a private ROM-free release stage

This is the downstream d-b-c-e OutRun arcade fork. Its build and stage identities
must be recorded independently of J1mbo's upstream releases. A verified stage is
not a public release, hardware acceptance or permission to redistribute assets.

## Prepare and review the contract before staging

Use PowerShell 7. Keep the reviewed JSON contract and source inventory outside the
stage. Do not generate the trusted contract from an installed game or accept a
manifest shipped inside an untrusted package. The verifier is read-only: it does
not create/copy/launch/install anything, connect to a device, or use the network.

The contract schema is `dbce-outrun-arcade-stage-v1` and repository is
`d-b-c-e/dbce-mods-outrun-arcade`. Record:

- `source.commit`, `source.tree` and `source.inventory`: exact reviewed engine
  snapshot, with every included public source path and Git blob SHA. The accompanying
  `tools/package/source-inventory.d128256.json` pins current canonical master
  `d128256afa2c9e7ac4dc164125c30e95a7d1ab3c`, tree
  `559f22a398640674b30c1fe6950f185644486b33`, and 148 non-ROM tracked files.
  Its only excluded tracked path is `roms/roms.txt` (ROM-list instructions, not a ROM).
  Stage those files under `source/` without changing bytes. Update/review the external
  inventory explicitly for another source snapshot; never shorten it to make a stage pass.
- `build.source_commit`, `build.architecture` (`x64`), `build.configuration`
  (`Release`), `build.compiler`, `build.sdk` and nonempty `build.configure_arguments`.
  If a previously verified build is reused for a documentation-only source revision,
  record its actual build commit, and independently verify all runtime/build inputs
  are unchanged. Do not relabel an old executable as newly built from current master.
- `files`: a complete explicit array of `{path, role, bytes, sha256}` entries.
  Use lowercase SHA256, positive byte counts and `/`-separated relative paths.
  Runtime filenames are exactly `cannonball-se.exe`, `SDL2.dll`, `tinyxml2.dll`,
  `libEGL.dll`, `libGLESv2.dll`, `mpg123.dll` and `zlib1.dll` at the stage root.
  Hash the independently approved build outputs, not files supplied by a recipient.
  Required runtime resources are `res/gamecontrollerdb.txt`, `res/tilemap.bin`,
  `res/tilepatch.bin` and the three `res/Cannonball-Shader-*.glsl` files from the
  reviewed source inventory. They are checked against source Git blob identity too;
  this gate accepts no replacement private asset merely by changing its manifest hash.
- `dependencies`: entries for `sdl2`, `tinyxml2`, `mpg123`, `angle` and `zlib`, each
  with `name`, exact `version`, `upstream`, nonempty `source_paths` and `notice_paths`.
  Every source/notice path must occur in `files` with role `dependency-source`/`notice`.
  Supply complete corresponding component sources as the inherited license requires;
  the presence of one declared file does not prove component-source completeness.

Other allowed roles are `resource`, `source`, `documentation` and `default-config`.
The package must include `docs/OUTRUN-ARCADE-SETUP.md` and the six notices below.
Resources, dependency source coverage and third-party license obligations need
independent review; hashes identify approved bytes but do not establish ownership
or legal completeness. The verifier never contacts GitHub to authenticate the
snapshot: approval of the external contract/inventory is its trust boundary.

## Exact stage contents and privacy boundaries

Include these notices under `notices/`: `license.txt`, `CannonBall-SE-license.txt`,
`THIRD-PARTY-NOTICES.md`, `LGPL-2.1.txt`, `license_mame.txt`, `license_atari800.txt`.
Include applicable dependency notices and corresponding sources too. Preserve
noncommercial distribution and full-source obligations. Where static LGPL linking
requires relinkable objects or another compliant mechanism, arrange it before release.
The verifier's success does not replace that license review.

Never stage ROMs, user music, installed configs, scores, statistics, logs or credentials.
Do not copy `roms/`, `run/`, `.git/` or an installed game folder wholesale. The gate
rejects extra files, hash/size changes, private-state paths, ROM-shaped filenames,
archives, traversal, duplicate Windows paths and junctions/symlinks/reparse points.
Archives are deliberately excluded: this version verifies an expanded file tree,
so no opaque source/runtime ZIP can hide unchecked contents. These checks supplement
an independently reviewed exact allowlist; they cannot identify a private asset that
an approver mistakenly allowed under an innocuous filename.

Only a reviewed fresh-install default may be included as `defaults/config.xml`;
the upstream default is also permitted at `source/res/config.xml`. Never overwrite
an existing live configuration, ROM directory, custom music or save data. This tool
contains no installation/update command. Use the standalone setup guide and preserve
the installed executable/configuration identities and working-directory behavior.

## Verify without running the game

```powershell
pwsh -NoProfile -File ./tools/package/Test-ReleaseStage.ps1 `
  -StageRoot C:/private-staging/OutRunArcade `
  -ContractPath C:/reviewed-contracts/outrun-arcade.json
```

Success returns the source/build commits, verified file count and hashes of the
external contract and source inventory. Any rejection must be resolved by checking
the source/stage evidence; do not relax an allowlist to absorb personal files.
The command does not execute `cannonball-se.exe` or prove runtime compatibility.
Keep runtime/device checks separately authorized. Publish nothing automatically.

For the fixture-only regression checks:

```powershell
pwsh -NoProfile -File ./tools/package/Test-ReleaseStage.Tests.ps1 `
  -FixtureRoot C:/temporary-tests/outrun-release-stage
```

Fixtures contain synthetic text even for executable/DLL filenames. They are neither
runnable binaries nor game assets. Passing them verifies rejection/identity rules,
not a complete actual downstream release. Fixture contracts and inventories must
never be used for real release approval.

## Current evidence and remaining release work

The verified Release x64 build and four-notice CMake install remain valid evidence.
Current master's later changes are documentation-only; no new build or runtime test
is claimed here. CMake install currently stages notices, not a complete runnable
package. The build's vcpkg toolchain stages the seven runtime files, while runtime
resources, six inherited notices, full engine/component sources and an approved
source/build/artifact contract still require deliberate assembly and review.

This candidate adds an offline validation gate and instructions only. No actual
release contract is supplied because complete dependency sources/notice coverage and
asset provenance have not been assembled and reviewed. No ROM/private asset is
downloaded or included; no public binary release is made.
