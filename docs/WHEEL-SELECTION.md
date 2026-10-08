# Windows wheel selection candidate — October 8

Source candidate with a muted menu/save/restart check; not physically tested.

Open **Settings → Controls → Wheel output**. `Choose` cycles through None and
the attached force-capable device names, each with part of its instance GUID.
`Use choice after restart` selects that exact identity. `Output next start`
controls the existing force-enable setting. Use **Save settings** and restart
the game. `Refresh wheel list` refreshes discovery and starts on the saved
wheel if it is present. A missing saved wheel leaves the proposal unchosen;
Apply then asks for a choice. Clearing a saved wheel requires explicitly
cycling to None. It never chooses the first wheel for you.

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
remain. No gain, physical sign or normalization claim.

Claude's independent source review passed for a muted menu/config check. It
confirmed native enumeration does not replace the active actuator identity and
cleanup releases the controller before the catalog. The unchosen state above
addresses its usability finding: Apply must not silently clear a disconnected
saved wheel.

## Muted runtime check, October 8, 13:47–13:56 Central

Candidate `9fa3694`, executable SHA-256
`6EB4D56395BF2D89997925D6A7E95E8F82FA399DE85569EDEA4656E291380F71`,
was temporarily staged into the existing Stream Deck target under the shared rig
lease. Both ordinary executable launches had `DBCE_FFB_MUTE=1`, with no
`FF_TARGET_GUID`. Existing display and force strengths were retained. Only the
SDL input index temporarily pointed to the virtual pad for menu navigation.

Fresh window frames preceded each button press. Settings → Controls → Wheel
output discovered the MOZA R12 by name and GUID. Applying it updated Next wheel;
Save and return wrote the exact full GUID. After normal close (exit 0), a second
launch displayed the same saved wheel and preselected it in Choose. All rows fit
the menu. The font displayed square brackets poorly; the following source-only
label correction removes them without changing the identity or selection logic.

The second launch also closed normally (exit 0). The virtual pad stopped and
all ten original root files, including config and statistics, were restored with
exact inventory, hashes and timestamps; temporary payloads were removed. The old
installed executable remains. Evidence is private at
`E:/Source/_archive/2026-10-08/cannonball-wheel-20261008-134734`.
An earlier prelaunch attempt rejected the owner's nonstandard XML comments;
it launched no game and restored its ten files exactly. The runner now replaces
only the temporary pad index as text, without rewriting the owner's XML grammar.

This proves menu discovery, save and reload while muted. It does not qualify
device acquisition, forces, disconnect recovery or physical feel. Those require
an attended check after a reviewed packaged installation.
