# Windows wheel selection candidate — October 8

Source and offline build only; not installed or physically tested yet.

Open **Settings → Controls → Wheel output**. `Choose` cycles through None and
the attached force-capable device names, each with part of its instance GUID.
`Use choice after restart` selects that exact identity. `Output next start`
controls the existing force-enable setting. Use **Save settings** and restart
the game. `Refresh wheel list` refreshes discovery and resets the proposed
choice to None; it never chooses the first wheel for you.

The next wheel is stored in `controls.analog.haptic.device_guid` in the normal
config.xml. Empty, malformed, zero and disconnected identities refuse output;
there is no name/index/product fallback. Selecting a different identity or
changing enable stops and retires the current output for this process. It never
opens the newly selected wheel until the next launch admits foreground driving.
The normal input device/bindings and force strengths are not changed by selection.

Read-only device enumeration uses the pinned toolkit. It never calls force
initialization, acquisition, preferences, setters or exit guards. Its library
reference stays loaded until cleanup; freeing it while the actuator may still
exist would be unsafe. Cleanup closes the controller before releasing catalog
state. A refresh can discover a newly connected wheel, but applying a now-missing
choice is refused rather than silently selecting something else.

For developer tests, nonempty `FF_TARGET_GUID` overrides the saved identity.
An invalid override refuses even if the saved GUID is valid. `DBCE_FFB_MUTE=1`
still refuses the output initialization before loading an actuator. Opening
the picker may load the native library for read-only enumeration while muted;
DLL presence alone therefore does not prove force-device acquisition.

Sixteen CTest cases pass, including five new actual-Windows-adapter fake-sink
cases for saved identity, override/refusal, read-only enumeration and retirement.
The full Windows Release game builds; its existing unrelated renderer warnings
remain. The menu's rendering, config round trip and plain-launch use still need
a bounded muted runtime check. No gain, physical sign or normalization claim.
