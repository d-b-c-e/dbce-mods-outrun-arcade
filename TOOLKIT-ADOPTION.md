# Toolkit standards adoption

Which entries of the wheel toolkit's standards ledger
(`E:\Source\toolkits\dbce-wheel-mod-toolkit\STANDARDS.md`) this mod has brought in.
Update a row in the same commit that adopts it. Statuses: `adopted`, `partial`,
`pending`, `n/a` (say why), `unchecked` (nobody has looked yet).
Seeded 2026-10-04 from what was verified that day; `unchecked` rows need a look.

| Standard | Title | Status | Notes |
|---|---|---|---|
| STD-001 | One mod per game | unchecked |  |
| STD-002 | Recording and playback from launch | unchecked |  |
| STD-003 | Normalized FFB strength | pending | Cabinet-command@2 retains the original nonzero law; Windows transport candidate only, no Art-matched force calibration. See docs/FFB-TOOLKIT-CANDIDATE.md. |
| STD-004 | Consistent settings UX | partial | Explicit saved wheel picker in Settings/Controls: October 8 muted menu/save/restart passed on 9fa3694. Physical output remains unqualified. See docs/WHEEL-SELECTION.md. |
| STD-005 | Camera numpad layout 8/2 9/3 4/6 7/1 +/- 5 | unchecked |  |
| STD-006 | Camera step sizes are settings | unchecked |  |
| STD-007 | Triple screens in one wide window | unchecked |  |
| STD-008 | Display changes: game applies once | unchecked |  |
| STD-009 | Dashboard telemetry matches the HUD | unchecked |  |
| STD-010 | Install the latest build for testing | unchecked |  |
| STD-021 | Art FFB reference | pending | Cabinet command and nominal constant request are not measured physical torque; no normalization workload yet. |
| STD-025 | Shared force model | partial | Cabinet game has no tyre model. Reviewed d2c1a00 uses shared native 50ba139 transport; versioned cabinet arithmetic remains local, not AxleForceCurve. Muted startup passed; previous install restored. |
| STD-026 | One player force model | partial | No model selector. Saved explicit wheel picker passed a muted two-launch round trip; environment override remains developer-only. Physical qualification pending. |
| STD-027 | Independent strengths | pending | Cabinet law combines steering/crash/road; cannot relabel its overall gain as steering-only. |
| STD-011 | Work lands on main | adopted | Changes committed/pushed to this fork's default master; upstream origin is not the owner fork. |
| STD-012 | Original route and signals | partial | Reviewed runtime4336bd9 recorded1802 original cabinet updates; strict standalone replay matched every command/state, observed mute -1 on all rows, normal exit0 and exact restoration. 31 CTest/14 strict/146347 producer checks. Gameplay playback and normalization workload remain pending; docs/CABINET-RECORDING.md. |
| STD-013 | Plain launch | partial | Existing install retained. Persisted explicit-GUID picker passed the October 8 muted menu/save/restart check; candidate restored afterward. Physical output remains pending. |
| STD-014 | Reciprocal review | adopted | Claude independently reviewed transport/recovery, rebuilt eleven cases and ran muted startup; findings addressed. |
| STD-015 | Surround and separate triples | unchecked | Wide cabinet rendering is not proof of three independently projected views. |
| STD-016 | Forza telemetry defaults | unchecked | Existing telemetry path not requalified by the force transport work. |
| STD-017 | Hide empty settings pages | unchecked | Player settings not audited here. |
| STD-018 | Online score guard | unchecked | Transport changes no driving physics; broader score paths not audited here. |
| STD-019 | Centre menus | unchecked | One wide menu frame does not establish the full menu/race matrix. |
| STD-020 | Standard feature checklist | partial | Ledger gaps made explicit October 8; unchecked rows still require feature evidence. |
| STD-022 | Triple selector | unchecked | No selector adoption claimed by native FFB work. |
| STD-023 | Panel/log frame rate | unchecked | No runtime qualification in this change. |
| STD-024 | On-screen frame rate | unchecked | No runtime qualification in this change. |
| STD-028 | Raw registry preservation | partial | October 8 muted test preserved ten owner files and restored binaries; it made no display/profile/registry override. A registry-changing test needs its own raw snapshot. |
