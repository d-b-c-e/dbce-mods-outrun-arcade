# Original cabinet force recording groundwork

October 8: offline fixture only. No runtime hook, recorder, gameplay writer,
install or physical output has been added in this change. STD-012 remains partial
and cross-game normalization remains pending.

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

All17 CTest cases pass, including the existing fake-native lifecycle suite.
The Windows-specific producer fixture does not change the Linux test contract.
