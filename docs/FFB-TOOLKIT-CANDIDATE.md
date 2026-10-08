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
The current FFB configuration still controls whether initialization is attempted;
a refused initialization no longer overwrites its saved enabled preference.
An eventual player device picker remains a release requirement.

`DBCE_FFB_MUTE=1` refuses before loading the force DLL and suppresses automatic
SDL haptic opening. Normal SDL rumble is limited to mapped gamepads; it never calls
the cabinet actuator. These are process environment settings for a diagnostic
launcher, not machine-wide changes. No physical test command is supplied here.

The production `CabinetForce` controller accepts zero before any nonzero command,
attempts zero and Stop independently, and latches unavailable after a failed send
or invalid command. It never calls StartEffect, automatically reopens or retries
a nonzero request after failure. Off, neutral, pause, menus, non-driving game states
and background release the output. After a valid gate reopens, another accepted
zero precedes the next force. The toolkit binds an owned process window, stops on
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

Ten CTest cases cover all motor codes against the original arithmetic, invalid
settings, partial initialization, accepted-neutral ordering, foreground handback,
refused output, every zero/stop acknowledgement combination and cleanup. The
adapter fixture compiles the actual Windows adapter with fake toolkit and window
calls; it neither loads a DLL nor opens hardware. The existing full-game x64
Release configuration also builds. No launch or installed-file changes occur.
The mapping/controller executable makes 160 assertions. Removing only the
initial accepted-zero call makes two production fixture cases fail; restoring
the original bytes returns all ten cases to passing. Private negative-control
log: `build-check/ffb-negative.log`.

Before packaging: independent review, producer recording/normalization contract,
player device selection and release inventory/notice adoption. Then a bounded
muted startup with exact owner-state restoration; physical sign, release and feel
remain owner-attended. The shared native version number alone does not establish
the retained-identity fix: use the committed source and binary hashes.
