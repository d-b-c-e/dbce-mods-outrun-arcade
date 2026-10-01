# Third-Party Notices

This project includes or interfaces with third-party components. Copies of their
licenses are included where required.

---

## Blargg SNES NTSC Video Filter

- **Component:** `snes_ntsc`
- **Author:** Shay Green ('Blargg') <gblargg@gmail.com>
- **License:** GNU Lesser General Public License (LGPL) — see `LGPL-2.1.txt`
- **Docs:** `Blargg-NTSC-Filter-Concepts-and-Implementation.txt`

### Notes

This library provides composite-video style NTSC filtering. When statically linking
LGPL code into binaries, ensure recipients can relink with a modified version (for
example, by providing relinkable object files); when dynamically linking, ensure users
can swap in a compatible modified library. Preserve copyright and license notices.

For the full terms, see `LGPL-2.1.txt`.

