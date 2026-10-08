# Analyze an original cabinet force recording

This is a software analysis bridge, not a new force model or physical
normalization. It retains the recorded cabinet minimum/maximum and refuses to
invent a Strength-50 equivalent. The cabinet law mixes steering, off-road, skid
and crash behavior; this export does not separate those components.

Build the original-producer replayer as described in
[CABINET-RECORDING.md](CABINET-RECORDING.md), then use a new private result folder:

```powershell
py -3 tools/export_cabinet.py <source.jsonl> <new-private-output> `
  --replayer build-check/ffb-tests/Release/cabinet_replay.exe
py -3 E:/Source/toolkits/dbce-wheel-mod-toolkit/tools/replay/force_normalization.py `
  <new-private-output>/comparison.json <new-private-report>
```

The first command requires the complete strict session, exact original producer
and mapper replay, and unchanged source/replayer hashes. It copies the original
capture, replayer, Python readers and public HUD source into the result and pins
them in `validation.json` and the comparison manifest. The manifest is written
last; an interrupted directory without it is not an analysis input. Existing
outputs are refused. Synthetic producer fixtures remain labelled synthetic.
Do not add them to an owner calibration corpus.

The CSV contains each update's nominal constant-force request divided by 10,000,
alongside time, cabinet command, step, observed counters and steering adjustment.
Speed is the integer argument `car_increment >> 16` passed to `OHud::blit_speed`
in `oinitengine.cpp`, with the original KPH labels. It is a HUD-domain measure,
not measured SI velocity. No raw fixed-point speed is treated as m/s.

The analyzer's holds describe the nominal calculation between sample timestamps.
They do not model native acceptance, recovery, watchdog expiry or delivered
torque: every recorded native request was refused by the mute. Its final sample
has no known duration, and gaps above 100 ms are excluded. The configured cap is
the recorded maximum limited to 10,000; a zero maximum is refused for this cap
comparison rather than assigning an invented cap. Zero observations with a
positive maximum remain valid.

## October 8 first original case

Case `6e6d13fa8ca64ea898e19dcd481e6079`, runtime 4336bd9, replays all 1,802 rows.
Source SHA256 `82B0FC01AF853DADAC596777AF11D87DC2F391055DB56C2498B72BBB7CDD4239`.
Recorded settings are maximum 8500, minimum 7000 and holdMs 20. No setting changed.
The first export and shared analysis are private at
`_archive/2026-10-08/cannonball-cabinet-export-1720`.

| HUD speed band | Seconds | Nominal RMS | Nominal absolute P95 | Peak |
|---|---:|---:|---:|---:|
| 0-5 KPH | 41.10 | 11.79% | 0.00% | 74.30% |
| 5-20 KPH | 1.60 | 0.00% | 0.00% | 0.00% |
| 20-60 KPH | 4.03 | 25.90% | 82.86% | 82.86% |
| 60+ KPH | 13.30 | 24.18% | 82.86% | 82.86% |

Only 77 of 1,802 updates have a nonzero nominal request. Over the entire sequence,
the analyzer assigns 4.27% of sampled time to magnitude at least 50% of unit range.
This describes sparse strong cabinet pulses, not physical duty or a weak gain.
The short automated drive is not representative cornering and does not justify
a multiplier against Art. A future independent steering/crash model requires
an explicit versioned adapter and preserved original replay.
