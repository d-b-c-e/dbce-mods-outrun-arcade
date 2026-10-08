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
| STD-004 | Consistent settings UX | unchecked |  |
| STD-005 | Camera numpad layout 8/2 9/3 4/6 7/1 +/- 5 | unchecked |  |
| STD-006 | Camera step sizes are settings | unchecked |  |
| STD-007 | Triple screens in one wide window | unchecked |  |
| STD-008 | Display changes: game applies once | unchecked |  |
| STD-009 | Dashboard telemetry matches the HUD | unchecked |  |
| STD-010 | Install the latest build for testing | unchecked |  |
| STD-021 | Art FFB reference | pending | Cabinet command and nominal constant request are not measured physical torque; no normalization workload yet. |
| STD-025 | Shared force model | partial | Cabinet game has no tyre model. Source candidate uses reviewed shared native 50ba139 transport; versioned cabinet arithmetic remains local, not AxleForceCurve. Not installed. |
| STD-026 | One player force model | partial | No model selector; explicit instance GUID still requires developer environment setting pending a player picker. |
| STD-027 | Independent strengths | pending | Cabinet law combines steering/crash/road; cannot relabel its overall gain as steering-only. |
