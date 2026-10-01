# OutRun Arcade (CannonBall-SE): standalone setup and releases

This product runs the original OutRun arcade game through the CannonBall-SE engine.
It keeps Chris White's CannonBall lineage and James Pearce's SE enhancements.
OutRun 2006: Coast 2 Coast and OutRun arcade SP SDX are separate products.

## Product identity

The product display name is **OutRun Arcade (CannonBall-SE)**. The canonical
repository is [d-b-c-e/dbce-mods-outrun-arcade](https://github.com/d-b-c-e/dbce-mods-outrun-arcade).
The in-place rename retained repository ID 1319808619, the native fork relationship,
branches, tags, history and public visibility. Old repository URLs redirect to this name.
Keep `cannonball-se.exe`, `config.xml`,
resource paths and existing installation directories compatible. A repository rename
does not require renaming the installed executable or moving user data.

One engine executable already carries wheel input and FFB. The LaunchBox batch file
is an optional launcher for that executable, not a second mod. Setup must work
without LaunchBox, the wheel toolkit, or the triple-screen optimizer.

## Source and availability

As checked on 2026-10-01, the fork's default branch `master` is at `086591c`.
The Windows FFB direction/range fix is on `fix/windows-ffb-direction-and-force-range`
at `422d1d370f84d605e1602fcc6056f240081301c3`, matching the local source checkout's HEAD.
Build that branch when evaluating the Windows FFB candidate; selecting `master`
does not include the fix. The fork currently has no published GitHub releases.
Upstream releases remain upstream artifacts and do not establish fork acceptance.

SDL wheel/gamepad input, SDL rumble and a Windows DirectInput force backend exist.
Current hardware acceptance is still separate from source presence. Widescreen
output exists, but calibrated simultaneous triple-screen views are unverified.
Telemetry export, session recording, recorded-force analysis and driving-input
playback are unverified. The game's play-count/statistics files are not session recordings.

## Windows setup

1. Build the selected fork source using [the Windows guide](Compiling-On-Windows.txt).
   Building and playing are separate steps; setup verification need not start a game.
2. For a fresh installation, place the executable and its matching runtime DLLs in
   one writable installation directory. Include the required engine resources from
   `res/`, preserving the resource paths configured by the engine. Inspect the actual
   build output instead of assuming a source archive is an installable binary package.
3. Supply your own legally held original OutRun revision B ROM set privately.
   ROMs must never enter a public source or release package.
4. Start from the installation directory. Relative `config.xml`, ROM, resource and
   save paths depend on the working directory. `-cfgfile` can select a configuration,
   but does not by itself relocate other relative paths.
5. Configure wheel/gamepad controls through the existing game menu. SDL joystick
   indices can change when devices change; do not distribute a user's `pad_id`
   or assume that SDL and DirectInput indices have identical ordering.

The existing LaunchBox launcher changes to `Games/Windows/OutRun (Cannonball-SE)`
before starting `cannonball-se.exe`. Keep that installation path and the stock
CannonBall fallback usable. A shortcut should set the same working directory.

## Existing installations and settings

Back up the selected configuration and its configured `data.savepath` before an
upgrade. Preserve `config.xml`, `play_stats.xml`, `hiscores*.xml`, custom music,
ROM/resource paths and any local launcher settings. Keep user-owned ROMs and music
private. Distribute a clearly marked default configuration for fresh installs only;
never overwrite an existing configuration, scores or statistics as part of an update.
Record the old executable/build identity so rollback restores matching binaries
without undoing user settings. Do not automatically move installations during naming cleanup.

## One release per game

A future fork release should identify its own version, exact source revision and
upstream base, and contain one runnable engine payload with one setup guide.
Keep historical upstream versions, branches and artifacts traceable. A documentation
change supplies no new runtime capability and does not justify a tested-binary claim.

Before release, verify the staged package, not the live installation:

- Confirm the executable's matching DLLs and configured resource paths; check all
  documentation links and include an upgrade/rollback note.
- Exclude ROMs, personal configurations, scores, statistics and user music. Do not
  package the local `roms/`, `run/` or installed game directory wholesale.
- Include complete corresponding modified source and component sources as required
  by [the CannonBall license](license.txt); preserve [SE terms](CannonBall-SE-license.txt).
- Include [third-party notices](THIRD-PARTY-NOTICES.md), [LGPL-2.1](LGPL-2.1.txt) and
  applicable component notices. Supply relinkable objects or another compliant
  mechanism where static LGPL linking requires it.
- Keep distribution noncommercial and preserve credits. Existing CMake license
  installation entries alone are not proof that a complete release package is compliant.
- Reconcile current owner work before promoting the FFB branch to the default branch;
  preserve the source checkout ownership guard until an approved owner resolves it.

The repository rename is complete. This documentation does not publish a release.
