# Original cabinet force recording groundwork

October 8: runtime recorder and standalone replay candidate, not installed or
live-qualified. The original producer fixture is independently reviewed. The
runtime at cb01492 passed independent source review before a rig capture.
The subsequent observed-delivery addition below awaits its narrow follow-up review. STD-012
remains partial; gameplay playback and cross-game normalization remain pending.

The original `OOutputs` deluxe-cabinet producer is stateful. It combines steering,
crash/skid and off-road motor tables, then the Windows `motor_output` boundary
sends a command and seven-step force index. A separate `cabinet-command@2`
mapper produces a nominal constant request. These are neither tyre forces nor
measured wheel torque; replacing them with AxleForceCurve would invent inputs.

`tests/extract_cabinet_producer.py` extracts the unchanged constructor/init and
motor methods from production `ooutputs.cpp` at build time. The fixture supplies
engine observations and a fake output sink; there is no game, SDL, ROM or native
force dependency. This avoids maintaining a second approximation of the model.

The baseline passes140,866 checks across12,000 synthetic rows, replayed without
per-row resets both from the initial checkpoint and from row7,311. It also checks
known neutral/sign cases and the original eight-step crash waveform. One
important original behavior is preserved: at low speed, when `was_small_change`
is false, `done()` can retain the preceding motor command. A symmetric sign test
must not silently change that history-dependent behavior.

## Contract to carry into an original recorder

Record at the existing `MODE_FFEEDBACK` producer boundary, once per motor update,
before output gating. Preserve the existing Windows command stream, including
explicit zero for codes0/8. Record actual update identity and monotonic time;
nominal30 Hz is not a timing measurement.

- Inputs: game state; crash and skid counters; car increment; road curve;
  wheel state; lateral car difference; adjusted steering; motor input.
- State before/after: hardware command, motor enabled, motor position change,
  motor control/movement, centred flag, movement latch, speed/curve table indices,
  vibration-table counter, small-change latch and all three steering-history
  values (14 scalars).
- Output: actual command/step pair and qualified nominal mapper request, with
  maximum/minimum/hold settings, source identity and admission domain.

The existing Windows mute preserves the configured haptic flag, so the engine
can still run the producer while the force transport refuses initialization.
A runtime recorder must independently attest that mute and also block rumble and
telemetry delivery during unattended capture. It must buffer rows, finalize away
from the motor update, preserve ordered calls and reject interruption/overflow,
unsupported modes or incomplete data. Strict offline replay must validate the
envelope, input ranges, first checkpoint and subsequent state continuity before
running any table lookup. The replay executable must never link a physical sink.

Only after a live original case replays exactly should independent steering,
crash and road strengths change composition. Keep the legacy recording contract
readable, record the new composition version, and demonstrate each independent
gain before claiming STD-027 or comparing with Art's reference.

Run the offline Windows suite:

```powershell
cmake -S tests -B build-check/ffb-tests
cmake --build build-check/ffb-tests --config Release
ctest --test-dir build-check/ffb-tests -C Release --output-on-failure
```

The initial 17 CTest cases passed, including the existing fake-native lifecycle suite.
The Windows-specific producer fixture does not change the Linux test contract.

## Runtime candidate

The Windows producer now copies its14-field state before/after the existing
motor call, and observes its actual command/step request. A preallocated8192-row
buffer enforces tick continuity, state continuity, finite typed domains and a
maximum120-second duration. No allocation or file I/O occurs inside that interval.
The original arithmetic remains unchanged. The low-speed retained-command
behavior is preserved. A read-only state accessor serves recording; the friend
writer exists only in the standalone test/replay executable.

The capture key is a startup process choice. Its presence blocks DirectInput
initialization/force sends, SDL haptic opening/rumble and telemetry socket opening,
including when the capture request itself is invalid. Saved settings are not
changed. Haptic calculation mode must already be configured; real-cabinet mode
is refused. The renderer, controls and engine remain the game's own.

The outer frame checks the shared lease (250 ms, plus every seal), ends on a
driving interruption, and writes after the buffer stops. Process cleanup releases
devices before finalizing a partial recording. A stopped/failed capture has no
completed session footer. Both source and outcome use non-replacing publication.
An existing result or temporary file is refused. No second capture is armed in
the same process. A runner still owns launch admission, exact owner backup,
normal close and restoration.

The shared vendored `dbce.wheel.session@1` writer is pinned separately under
`lib/toolkit/session`; the native41-export pin is unchanged. Metadata names the
producer and mapper source hashes, configured maximum/minimum/hold, muted
admission, uncalibrated increment units and original versus synthetic capture
kind. Nominal force is the existing mapper's calculation, not a claimed native
command or measured torque.

The standalone `cabinet_replay.exe` uses the unchanged source-extracted producer
and a fake sink. `tools/replay_cabinet.py` validates the shared envelope, exact
channel inventory, footer/counts, compiled producer/mapper identity and integer
domains before replay. It restores only the first checkpoint, then requires all
subsequent inputs, requests and states to match. Its summary preserves capture
kind and reports normalizationQualified=false.

```powershell
# Diagnostic child environment only; never persist these as owner settings.
# The runner supplies a new lowercase 32-hex id and its exact shared lease token.
$env:DBCE_CANNONBALL_CAPTURE = '<new-id>'
$env:DBCE_CANNONBALL_CAPTURE_SECONDS = '60'
$env:DBCE_CANNONBALL_LEASE = '<owned-token>'
# Launch the checked candidate through the supervised runner, enter an offline
# game and drive with muted outputs. This example does not launch it for you.

py -3 tools/replay_cabinet.py '<result>/source.jsonl' --replayer build-check/ffb-tests/Release/cabinet_replay.exe
```

Results are under `%LOCALAPPDATA%/Dbce/StagePlayback/cannonball-force/<id>`.
Source-only validation now passes 31 CTest cases, including 14 strict reader/replay
cases and real shared-writer file tests with a controlled clock. These cover
lease loss before sealing, early interruption, nested calls, occupied outputs,
invalid arming, native mute, malformed/tampered rows and unsafe table inputs.
Claude's baseline suggestions are adopted: unrecorded members are poisoned
before both stateful replays; steering stays in[-127,127]; nonzero cabinet-only
motor movement is refused in the recording contract. No physical/live result
is implied by these fixtures. The full Release game build also passes. Current
producer checks total 146,347; the older count above names the reviewed baseline.
Both build entry points verify the shared writer against its recorded SHA-256.

## Observed mute result, before the first original capture

Every Windows producer call now records the actual return from
`forcefeedback::set` as `delivery.result`, after preserving its existing command
and step. A complete row requires exactly one request followed by exactly one
delivery result of -1. An unset result, accepted call (0), ordering error or
missing callback stops the capture without a completed footer. The startup mute
path returns -1 before loading the force DLL; this is observed software refusal,
not a measurement of the actuator. Non-Windows delivery is unchanged.

The private signal contract now has that required channel; no original gameplay
captures predate this addition. The strict Python reader and original C++ replay
both refuse a changed result. Runtime writer tests include accepted-output and
early-delivery faults. Full Release build, 31 CTest cases, 14 strict reader tests
and 146,347 producer assertions pass. Evidence: `build-check/cabinet-delivery-*`.

`source.jsonl` is independently validated through its complete footer, counts,
source identities and original producer replay. The standalone reader does not
consume sibling `outcome.txt`; do not describe it as verifying that file.
