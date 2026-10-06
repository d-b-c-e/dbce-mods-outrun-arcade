# Telemetry (DBCE STD-016)

CannonBall sends the Forza Horizon 4/5 "Data Out" packet (324 bytes, UDP) every game tick (30 Hz), so
SimHub's Forza Horizon profile, dashboards and bass shakers work as with the other DBCE racing mods. It is
**on by default**, to `127.0.0.1:8000`; change or disable it in `config.xml`:

```xml
<telemetry enabled="1">
    <host>127.0.0.1</host>
    <port>8000</port>
</telemetry>
```

Layout: `dbce-wheel-mod-toolkit` `Dbce.Wheel.Telemetry.ForzaPacket` (byte 323 = 0x52 marks a DBCE mod).
Source: `src/main/telemetry/forza.cpp`, called from `main.cpp` after each engine tick.

| Forza field | From the engine |
|---|---|
| IsRaceOn | in game, including the start countdown (`GS_START1`..`GS_INGAME`); attract mode and menus are off |
| Speed, velocity z | `car_increment >> 16` km/h (the HUD speed), as m/s |
| Current/max/idle RPM | HUD rev units (`revs >> 16`) x 25: the 20-bar counter reads 0-8000; idle 1000 |
| Gear | 1 low, 2 high (`oinputs.gear`) |
| Accel, brake | the player's pedal input, 0-255 |
| Steer | the player's wheel input, -127..127 (attract mode's AI does not use it, so it reads 0 there) |
| Surface rumble | 1 on the wheels that left the road (`oferrari.wheel_state`), for shakers |
| Distance, race time, current lap time | accumulated while in game; reset in attract mode |
| Lap number | stage (0-4) |
| Position x / z | lateral road position / distance |
| Drivetrain, cylinders | RWD, 12 (Testarossa) |

Checked 2026-10-06 in attract mode with a windowed test copy: 1,436 packets in 48 s (30 Hz), all 324 bytes
with the sentinel, speed to 281 km/h, revs to 6,050 rpm, gear 1 to 2 at about 160 km/h.