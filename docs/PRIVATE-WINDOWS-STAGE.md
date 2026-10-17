# Private ROM-free Windows build and expanded stage

This producer builds the exact reviewed engine snapshot
`d128256afa2c9e7ac4dc164125c30e95a7d1ab3c`, tree
`559f22a398640674b30c1fe6950f185644486b33`, from its 148 approved Git blobs.
The producer's commit is recorded separately. It does **not** package current
master's later engine/FFB/controls changes, reuse or relabel an old executable,
change the installed game, or establish new force/runtime acceptance.

The result is an expanded **private, review-only stage**, not a public release.
`releaseReady=false` and all seven unresolved evidence issues remain. This is
a repeatable source/input/build recipe with receipts; byte-identical results
across two independent builds have not been demonstrated.

## Prepare, build, stage

Use PowerShell 7 and a clean committed producer worktree. Python 3.11+ is
required; only its standard library is used. Paths below are explicit retained
inputs, not discovery or permission to overwrite them. Every run must use a new
named directory below this worktree's `obj/private-stage/`.

```powershell
$run = Join-Path $PWD 'obj/private-stage/run-01'
$producer = './tools/package/Build-PrivateWindowsStage.ps1'
& $producer -Phase prepare -RunRoot $run `
  -DependencyRoot 'E:/Source/games/dbce-mods-outrun-arcade/build/vcpkg_installed/x64-windows' `
  -DownloadCache "$env:LOCALAPPDATA/vcpkg/downloads"

# Heavy build/stage batch: obtain Kobe's grant and the existing rig lease first.
# No game or executable produced by the build is launched.
& $producer -Phase build -RunRoot $run
& $producer -Phase stage -RunRoot $run
```

`-PythonExe`, `-CMakeExe` and `-PwshExe` select explicit tool executables when
needed. No dependency install, network fetch, ROM input, ZIP/public publication,
game launch, device/input, or owner install is implemented. Build uses existing
CMake with Visual Studio 2022 v143/14.44.35207, Windows SDK10.0.26100.0 and two
parallel jobs. It disables external toolchain/manifest installation and points
CMake at the private dependency snapshot, including the explicit release mpg123
library. Imported CMake targets also require their debug files to exist; those
are snapshotted for import validation, but only the six pinned release DLLs enter
the stage. Compiler/SDK/CMake identity and exact configure/build arguments are
read from the actual build and retained, not assigned to historical DLLs.

Preparation exports raw Git blobs without checkout newline conversion or the
excluded `roms/roms.txt`. It authenticates the retained release DLLs, SPDX/ABI/
copyright metadata and selected dependency input files against the existing
dependency evidence. Headers, libraries, binaries and CMake package files are
copied into a private input snapshot and checked again. Cached upstream archive
SHA512 values must match their recorded source recipes. A later phase refuses
changed source, dependency snapshot, archive, producer or policy inputs.

The build receipt identifies the new engine output and records log/cache/compiler
hashes. Staging consumes that receipt, not an installed game. It copies exact
engine source/resources, the seven runtime files, existing notices/setup guide,
expanded pinned upstream component sources and recorded vcpkg recipes. An
external `contract.json` describes the complete stage; the unchanged source
inventory and release policy are used by `Test-ReleaseStage.ps1`.

The narrowly authorized verifier correction separates inventory context from
artifact context: exact `src/main/roms.cpp`, `roms.hpp` and `res/config.xml` are
valid reviewed inventory names; their staged source counterparts retain role
and Git-blob checks. Actual ROM paths, near names, source archives and a live
`res/config.xml` remain rejected. No inventory or unresolved issue is relaxed.

## Receipts and unresolved coverage

The run contains `prepare.json`, `build-receipt.json`, `stage-receipt.json`, an
external `contract.json`, raw configure/build/verifier logs and `stage/`.
Stage `provenance/` carries the source/build/dependency evidence and all seven
unresolved issues. The external contract is producer-generated from identified
inputs/outputs and retained for review; its file-identity verdict is not an
independent permission or public release approval.

Expanded component sources are the pinned **unpatched upstream archives plus
recorded port recipes**. They are not claimed to be complete patched corresponding
sources. The frozen stage policy cannot carry audio/archive/INI fixtures, empty
markers or links; excluded upstream members are individually recorded in
`provenance/source-coverage-limits.json`. Recipe patches, ANGLE's separate Chromium
zlib/WebKit inputs, transitive notices/source coverage and historical DLL compiler
attestation remain unresolved. Tile/icon/controller-db provenance and inherited
license review remain separate holds. The producer never clears these issues.

A failed run is retained. Fix its concrete cause and use a new run rather than
overwriting artifacts. Source preparation and focused verifier/producer checks
do not establish a successful engine build; actual build/stage results must be
recorded separately. Current master870ed7e and accepted installed FFB remain
outside this frozen-snapshot packaging lane.

Focused checks (no compiler/game execution):

```powershell
python tools/package/Test-PrivateWindowsStage.py
python tools/package/Test-ReleaseStage.SourceInventory.py
./tools/package/Test-ReleaseStage.Tests.ps1 -FixtureRoot obj/private-stage/verifier-fixtures
```

The actual-source verifier fixture uses all 148 real reviewed source blobs and
synthetic non-runnable runtime/dependency files. Its pass proves the context,
identity and privacy checks, not playable packaging or complete licensing.

Standards comparison found existing pending/unchecked adoption and missing
STD029/030/031/032/036/037 entries. They remain leads; this packaging task makes
no unrelated runtime/UI/force adoption or installation claim.

## Actual private run, October 17

Fresh Kobe grant241322 ran reviewed producer commit
`0c05f3febf9518ac718d9ac7ea2af25be000f7a2` once in this isolated worktree's
`obj/private-stage/run-01`. Prepare, native build and expanded stage all exited0.
MSVC19.44.35229.0, toolset14.44.35207, SDK10.0.26100.0, CMake4.4.3 produced a
new x64 Release engine (not reused and never executed), SHA256
`73c222c4f5c215e6e3afcbf58dce4cdda9859f318510ecfec606660d88dfb34b`.

Production review-only verifier passed6582files:148exact frozen Git source
blobs,7runtime files,6resources,13notices,6documentation/provenance files and
6402dependency-source/recipe files. Thirteen excluded upstream fixture members
are individually recorded. Contract SHA256
`8651865d1217f8d5f190b11f5927b088052bd3798282db19e61b7110550ccc4c`;
stage-receipt SHA256
`f52491e83c4b7e9f4cab4233fe721921d3c4d84a5a365d8ce6eb0b8ff86711f3`;
build-receipt SHA256
`2c1ed462d5526d32e6876adc7aec2230e4402d6040389a7396101f8fa05c940c`.
Raw configure/build/verifier logs and complete input/output receipts remain
beside the stage. Frozen d128256 source/tree559f22a and source inventory/evidence
JSONs are unchanged. This proves one reproducible recipe execution, not two-build
bit identity, playable behavior, patched-source completeness or release rights.

Shared custody23:50:50.1916007Z–23:54:14.8792703Z (204.69s). All39tracked process
identities were quiescent with a safe census and no remaining scoped children;
the exact lease token was released. Hub runner/result/logs:
`artifacts/cannonball-private-vunu-20261017/build-01/`.
Runner A9ED617B0776EE199811B07518D2C87B9CDCA52D4374BA298C86822177E124AE
adds the disclosed phase-start budget guard to the prepared runner: full180s
phase plus30s closure must fit within600s. No automatic retry, executable/game
run, UI, device, network, installation or owner-state change occurred. All seven
holds remain open and `releaseReady=false`.
