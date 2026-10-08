# Windows cabinet force transport candidate — October 8

Source and offline tests only. **Not installed, physically tested or normalized.**
The installed executable and owner's settings stay unchanged. Linux evdev remains
the previous implementation and is not qualified by these checks.

The old Windows path could open the first wheel from a gamepad-rumble fallback,
ignore Acquire failure, submit constant parameters to a sine fallback and hold
force indefinitely. Its caller dropped OFF/CENTRE motor codes instead of releasing
the old output; a direct CENTRE call also had nonzero magnitude. Rumble modes were
passed as signed cabinet commands even when rumble was disabled.

The candidate replaces that private DirectInput implementation with the reviewed
41-export toolkit transport (`lib/toolkit/VERSION.json`, MIT notice beside it).
It requires an explicit **DirectInput instance GUID** in `FF_TARGET_GUID`; missing,
malformed, zero or disconnected selections refuse without selecting another wheel.
VID/PID, product GUID and first-device fallback are deliberately unsupported.
The current FFB configuration controls whether the selected identity is armed;
opening is deferred until foreground, unpaused driving. A refused initialization
no longer overwrites its saved enabled preference.
An eventual player device picker remains a release requirement.

`DBCE_FFB_MUTE=1` refuses before loading the force DLL and suppresses automatic
SDL haptic opening. Normal SDL rumble is limited to mapped gamepads; it never calls
the cabinet actuator. These are process environment settings for a diagnostic
launcher, not machine-wide changes. No physical test command is supplied here.

The production `CabinetForce` controller accepts zero before any nonzero command
and attempts zero and Stop independently at gate close, fault and cleanup. A
neutral motor code during active driving sends an acknowledged zero without
Stop/Start chatter. Known transient INPUTLOST, NOTACQUIRED, NOTEXCLUSIVEACQUIRED
or INCOMPLETEEFFECT refusals allow at most twenty retries, spaced by 100 ms,
within two foreground seconds. Every refused force is followed by attempted
zero/stop, and a later accepted zero must precede resumed nonzero output.
Neutral-only acceptance never resets a persistent failure episode or its failed
nonzero-attempt count. Accepted neutral demand pauses its time budget, so a long
straight does not disable an otherwise responsive device. A later failed neutral
resumes that budget. Inactive time pauses only after the gate successfully
acknowledges release; otherwise the device is retired. Invalid commands, other errors and exhausted
recovery latch unavailable until restart. The device is never automatically
reopened or replaced. It never calls StartEffect. Pause, menus, non-driving game
states and background release the output. After a valid gate reopens, another
accepted zero precedes the next force. The toolkit binds an owned process window, stops on
focus loss and uses a hold watchdog. Legacy force_duration was ignored on Windows;
it now requests a stale-producer timeout clamped to 100–500 ms, not pulse duration.
Cleanup attempts zero, stop and Free independently before unloading.

`cabinet-command@2` preserves the old nonzero integer seven-step mapping and signs,
including the 10,000 nominal clamp. It corrects release/delivery behavior; it does
not infer tyre forces or convert cabinet codes to Art-equivalent torque. Steering,
crash and road share the original cabinet law: STD-027's separate controls and a
producer recording contract remain future work. Do not rename the existing gain
to steering-only, or call this physically normalized.

Offline validation:

```powershell
cmake -S tests -B build-check/ffb-tests -G "Visual Studio 17 2022" -A x64
cmake --build build-check/ffb-tests --config Release
ctest --test-dir build-check/ffb-tests -C Release --output-on-failure
cmake --build build-check --config Release
```

Eleven CTest cases cover all motor codes against the original arithmetic, invalid
settings, partial initialization, accepted-neutral ordering, foreground handback,
refused output, every zero/stop acknowledgement combination and cleanup. The
adapter fixture compiles the actual Windows adapter with fake toolkit and window
calls; it neither loads a DLL nor opens hardware. The existing full-game x64
Release configuration also builds. No launch or installed-file changes occur.
The mapping/controller executable makes 315 assertions. Removing only the
initial accepted-zero call makes two production fixture cases fail; restoring
the original bytes returns all eleven cases to passing. Private negative-control
log: `build-check/ffb-negative.log`.

Claude independently reviewed the first candidate and rebuilt its ten cases.
His recovery/startup findings led to the bounded recovery and deferred open above.
The revised cases cover a transient failure followed by success, repeated nonzero
refusals despite accepted zero, inactive time excluded from the budget, and an
unacknowledged gate-close stop retiring the device rather than pausing forever.
An earlier configure-only hash check missed an incremental-build tamper; the
new always-run verification/staging target refuses that case and accepts the
restored bytes. Evidence: `build-check/ffb-pin-negative.log` and
`build-check/ffb-pin-restored.log`. Header attributes preserve the pinned raw bytes
in fresh checkouts. Claude's follow-up independently passed eleven cases and
found that neutral-only success could still exhaust recovery. The correction
adds a three-second accepted-neutral interval followed by successful force,
repeated rejected nonzero requests separated by accepted neutral (still bounded),
and failed neutral resuming the deadline. All eleven cases and the full Release
build pass. The correction awaits peer follow-up before a muted run.

The pinned native's GetLastHResult updates only on failed DirectInput HRESULTs;
a refused send caused by an absent effect can retain an older HRESULT. Such a
refusal may therefore enter transient recovery on stale classification. Recovery
still applies the same time/attempt bound, never treats that refusal as an
accepted command, and never opens another device. Exact refusal-reason reporting
needs a future native contract, not an inferred success from the cached code.

Before packaging: independent review, producer recording/normalization contract,
player device selection and release inventory/notice adoption. Then a bounded
muted startup with exact owner-state restoration; physical sign, release and feel
remain owner-attended. The shared native version number alone does not establish
the retained-identity fix: use the committed source and binary hashes.
