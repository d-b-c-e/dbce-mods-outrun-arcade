# Windows package (current commit, ROM-free)

`tools/package/Private-WindowsStage.py` builds the current commit and packages it
as one zip. It stays the packager until OutRun Arcade moves to the toolkit's shared
packager (`mod.json`, standalone kind). It replaces the old frozen producer, which
could only package engine commit d128256 from a fixed 148-file inventory.

```powershell
python tools/package/Private-WindowsStage.py
```

What it does:

1. Refuses uncommitted changes, so the package always matches one commit.
2. Builds `cannonball-se` (Release x64) in `obj/package/build`. The first run
   configures CMake with vcpkg (`--vcpkg-root`, `VCPKG_ROOT`, or the copy that
   ships with Visual Studio) and can take a while; later runs are incremental.
3. Copies the app into `obj/package/OutRun Arcade <date> (<commit>)/`:
   `cannonball-se.exe` and the DLLs beside it (SDL2, ANGLE, mpg123, tinyxml2,
   zlib, WheelFfb), `res/`, `licenses/`, the setup guide, a short `README.txt`,
   and the engine source for that commit as `source/*.zip` (the CannonBall
   license requires the modified source with the binary).
4. Refuses ROM files anywhere, and player files (a live `config.xml`, scores,
   play statistics) outside `res/`. Without a `config.xml` the game creates one
   on first start, so copying a package over an install keeps the player's
   settings, scores and ROMs.
5. Writes `release-manifest.json` (commit, version, SHA-256 of every file), then
   the zip and a `.sha256` file beside it.

Options: `--build-dir`, `--out`, `--vcpkg-root`, `--cmake`. Focused check (no
compiler, no game): `python tools/package/Test-PrivateWindowsStage.py`.

To install a build on the rig, use `tools/Install-LaunchBox.ps1` (it backs up the
replaced files and writes the receipt Wheelkit reads).

## Before a public release (owner)

Publishing needs the owner. These questions from the old review are still open:

- Dependency sources: the zip carries the engine source but not the SDL2, ANGLE,
  mpg123, tinyxml2 and zlib sources or vcpkg patches. ANGLE's nested
  third-party notices (Chromium zlib and others) are unreviewed.
- Asset origins: `res/tilemap.bin`, `res/tilepatch.bin`, `res/cannonball.ico`,
  `res/icon.png`, and the historical license of `res/gamecontrollerdb.txt`.
- How the noncommercial CannonBall and SE terms combine with the LGPL
  obligations of mpg123 (source and relinking).
