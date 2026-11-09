# OutRun Arcade (CannonBall-SE) — agent guide

Portfolio rules: `E:\Source\dbce-project-mgmt\HOW-WE-WORK.md` (read first; it replaces older process, lease, review and testing instructions). Current focus: `E:\Source\dbce-project-mgmt\NOW.md`. Feature status lives in the portfolio grid (`python E:\Source\dbce-project-mgmt\tools\portfolio_grid.py plan outrunarcade`), not in this file. Grid id: `outrunarcade`.

## Project

Fork of CannonBall-SE (J1mbo; itself a fork of Chris White's CannonBall), the OutRun arcade (revision B) engine, with DBCE wheel/FFB additions. Remotes: `origin` = upstream `J1mbo/cannonball-se` (never push there), `fork` = `d-b-c-e/cannonball-se` (PUBLIC; the owner pushes). Licenses: upstream CannonBall non-commercial terms plus CannonBall-SE terms (not for sale), see `docs/license.txt` and `docs/CannonBall-SE-license.txt`.

## Build

- Windows: open in Visual Studio and use the `windows-release` CMake preset (`docs/Compiling-On-Windows.txt`); vcpkg installs dependencies on first configure.
- Linux/Pi: `./install.sh`.
- Package the current commit (ROM-free zip): `python tools/package/Private-WindowsStage.py`, see `docs/PRIVATE-WINDOWS-STAGE.md`.

## Test

- `tests/CMakeLists.txt` builds the cabinet force, profile device, inject and toolkit adapter tests; run them with `ctest`.
- Python tests: `tests/test_cabinet_export.py`, `tests/test_cabinet_replay.py`.
- Original ROMs go in `roms/` (git-ignored). Back up `config.xml` and saves in the game folder before a run and restore them after.

## Install / run on the rig

`tools/Install-LaunchBox.ps1 -BuildDir obj/package/build/Release -Label <label>` installs the exe and DLLs into `E:\Source\tools\launchbox\Launchbox-Racing\Games\Windows\OutRun (Cannonball-SE)` with a backup (`-Restore <backup>` undoes it). The Stream Deck and LaunchBox start it through `Launch-Cannonball.bat`, which sets the working folder.

## Shared code

Toolkit pieces are in `lib/toolkit` (checked by `tools/Verify-Toolkit.cmake`). Change shared code in the toolkit and re-sync; never edit the vendored copy.

## Project-specific rules

- ROMs are never committed; the user supplies OutRun revision B.
- Windows wheel force feedback needs an explicit wheel instance GUID (see `docs/WHEEL-SELECTION.md`).
- Distribution is non-commercial, with credits and license files preserved.

## Key docs

- `README.md`, `docs/OUTRUN-ARCADE-SETUP.md`
- `docs/WHEEL-SELECTION.md`, `docs/CABINET-RECORDING.md`
- `docs/2026-11-03-f6-panel-gap.md`
- `TOOLKIT-ADOPTION.md`


<!-- BEGIN project-file-location -->
## Project files and temporary work

Keep project-related research, evidence, scripts, packages, and other working files inside the relevant project folder whenever practical. Keep disposable or machine-specific outputs git-ignored when they do not belong in version control; never ignore files that should be committed merely to hide them. When a project folder is unsuitable, use `E:\Source\temp\<project>` instead of creating scattered drive-root folders. Record the location of material needed by later work and update references when relocating it.

Clean up temporary files and disposable copies promptly once they are no longer useful. Preserve active work, user data, settings, saves, required evidence, backups, rollback artifacts, and worktrees until their retention or replacement is established. Follow existing project, rig, and data-loss safeguards; do not remove files merely because they are old or untracked.
<!-- END project-file-location -->
