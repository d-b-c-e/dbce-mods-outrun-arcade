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

## Recorded downstream Windows dependencies

These notices identify the retained packages used by the existing Release x64
build. They do not claim those DLLs were rebuilt here. Full hashes, ABI records,
source archive SHA512 values and vcpkg recipe/patch trees are recorded in
`tools/package/dependency-evidence.json`. All six retained DLL hashes match the
package SPDX records and the existing build receipt. Unknown compiler provenance,
incomplete corresponding sources and resource provenance remain in
`tools/package/unresolved-evidence.json`; this is review-only evidence.

| Component | Recorded version | Exact upstream source reference | Included notice |
|---|---|---|---|
| SDL2 | 2.32.8 | https://github.com/libsdl-org/SDL/tree/release-2.32.8 | `licenses/sdl2-LICENSE.txt` (Zlib) |
| tinyxml2 | 11.0.0 | https://github.com/leethomason/tinyxml2/tree/11.0.0 | `licenses/tinyxml2-LICENSE.txt` (Zlib) |
| mpg123 | 1.32.9, port revision 1 | https://sourceforge.net/projects/mpg123/files/mpg123/1.32.9/mpg123-1.32.9.tar.bz2/download | `licenses/mpg123-LICENSE.txt` (LGPL-2.1-or-later with project exceptions) |
| ANGLE | chromium_7258, port revision 1 | https://github.com/google/angle/tree/d9fc4a372074b1079c193c422fc4a180e79b6636 | `licenses/angle-LICENSE.txt` (BSD-3-Clause); transitive coverage remains unresolved |
| zlib | 1.3.1 | https://github.com/madler/zlib/tree/v1.3.1 | `licenses/zlib-LICENSE.txt` (Zlib) |

The five notice texts are copied byte-for-byte from the retained package
`share/<component>/copyright` files; their hashes match the corresponding SPDX
file identities. Verified recipe blobs at pinned vcpkg baseline
`b1b19307e2d2ec1eefbdb7ea069de7d4bcd31f01` identify their authoritative source
license files: SDL2/tinyxml2 `LICENSE.txt`, mpg123 `COPYING`, ANGLE `LICENSE`,
and zlib `LICENSE`. SPDX port download identifiers are Git trees, not commits.
Recipes and archive hashes identify sources; links are not a supplied or verified
complete corresponding-source distribution.

ANGLE also uses chromium third_party/zlib at
`4028ebf8710ee39d2286cb0f847f9b95c59f84d8`, WebKit build scripts at
`0742522b24152262b04913242cb0b3c48de92ba0`, EGL registry 2024-01-25 and OpenGL
registry 2024-02-10 (port revision 1). Nested source and per-file notice coverage
still require review. Standalone zlib1.dll's notice does not establish that coverage.

## Inherited code and asset evidence

Preserve `docs/license_mame.txt` and `docs/license_atari800.txt`. These attribution
notices state GPL-2.0-or-later; the complete referenced GPL v2 text is now included
as `licenses/GPL-2.0.txt`, retrieved from
https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt (SHA256
`edaef632cbb643e4e7a221717a6c441a4c1a7c918e6e4d56debc3d8739b233f6`).
Adding text does not decide compatibility with inherited engine/SE terms or
satisfy corresponding-source/relinking obligations by itself.

`src/main/windirent.h` attributes Toni Ronkko 1998-2019 and the MIT license.
`licenses/dirent-LICENSE.txt` preserves the full matching copyright/permission
notice from https://github.com/tronkko/dirent/blob/master/LICENSE, Git blob
`af04360668d8913aaeba7ae44cf71d5dc22639ea`. Its upstream source revision remains
unestablished; no new version is assigned to the inherited header.

The three Cannonball-Shader GLSL headers attribute James Pearce 2025; preserve
that attribution and existing SE license context. The bundled controller database
points to the former gabomdq repository, now mdqinc/SDL_GameControllerDB. Its
current upstream LICENSE is Zlib, blob `23abb73f2b67d1f5b5b9b4d6c949654d525173bb`,
but the bundled copy's historical source revision/license is not established.
No replacement notice is asserted for that copy. `tilemap.bin`, `tilepatch.bin`
and icon asset creation/rights provenance are unresolved. Preserve historical
assets; do not infer ROM-free redistribution clearance from tracked hashes.

## WheelFfb Windows transport candidate

`lib/toolkit` adds d-b-c-e's MIT-licensed WheelFfb native x64 library and C API
header from source `50ba139bcaee14aee080abe438d6beec4f6a2b47` of
`https://github.com/d-b-c-e/dbce-wheel-mod-toolkit`. Preserve the complete
`lib/toolkit/LICENSE` notice. Exact source/tag and file hashes are in
`lib/toolkit/VERSION.json`. This source candidate has not been added to the
historical release inventory or a published package; its transport tests do not
resolve the unrelated release provenance questions above.
