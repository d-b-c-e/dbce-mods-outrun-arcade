# Triple screens (DBCE STD-007, STD-015)

OutRun is a sprite-scaling game, so there are no 3D cameras to turn per screen. The triple mode renders
the road, scenery, traffic and backgrounds over a **48:9 internal picture** (1196 x 224, or 2392 x 448 with
hires), which fills 7680 x 1440 exactly: the centre screen shows the original view and the sides show what
lies beyond it. The HUD stays on the centre screen.

| `<video><widescreen>` | 4:3 display | 16:9 display | Surround (one 7680 display) | Three separate monitors |
|---|---|---|---|---|
| 0 | 4:3 | 4:3 | 4:3 | 4:3 on the primary |
| 1 (on) | 16:9 | 16:9 | **48:9** | **48:9, one borderless window across all three** |
| 2 (triple) | 48:9 | 48:9 | 48:9 | 48:9 across all three |

The in-game Video menu cycles WIDESCREEN between OFF, ON and TRIPLE. On three separate monitors (equal
heights, side by side, none of them Surround) the game opens a borderless window over their combined bounds
instead of desktop fullscreen on the primary display; no display mode is changed (`src/main/sdl2/span.cpp`).
Window mode keeps the chosen width.

Engine changes for widths over 404: sprite x is signed through the renderer and the culling bounds; the
MAME-era wrap for flipped sprites (`xpos < 0x80`) is skipped when the left screen makes such x visible; and
the 1024-pixel background tilemap is drawn at every repeat that falls on screen. Narrower modes are unchanged.

Checked 2026-10-06 in attract mode (test copy): 48:9 windowed for 90 s without a fault (the first build
crashed at 17 s: unsigned sprite x overran the frame buffer), and fullscreen on "Sim Racing" as one
7680 x 1440 window at (-2560,0).

Known issues: at the far left the sea's edge on the first stage shows steps; in a few frames the start-line
clouds end in a straight edge where the original background has no tiles. A CRT shape/warp setting bends the
whole 48:9 picture like one large tube; turn `crt_shape` off for flat triples if preferred.