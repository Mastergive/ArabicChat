# ArabicChat — ClassiCube client plugin (Arabic / Persian / Urdu)

Type and read right-to-left text in the ClassiCube **client**, using a **system font** (Segoe UI by default).

## What's verified and what isn't
- `ft_text.c` (Linux backend): **compiled and rendered a mixed Arabic/Persian/Latin test line here** with FreeType + fontconfig (`test_ft.c`).
- `arabic_core.c` (UTF-8, Arabic joining, lam-alef, bidi, chunking): **compiled and unit-tested** (`test_core.c`, ASan/UBSan clean).
- `ArabicChat.c` + `gdi_text.c` (Windows): **written against the ClassiCube 1.3.8 plugin API but NOT compiled** — I had no headers/Windows toolchain here. Expect to fix a name or two; send me the compiler errors and I'll patch them.

## Use
- `/client arabic` opens the input bar. Type with your OS Arabic keyboard layout. **Enter** sends, **Esc** cancels, **Backspace** deletes.
- Incoming lines are drawn in a small overlay (top-left, under the stats). Stock chat still shows the raw bytes as gibberish for those lines — the plugin can't edit it.
- Options (in `options.txt`): `arabicchat-font` (default `Segoe UI` on Windows, `sans-serif` on Linux, i.e. whatever fontconfig picks; try `Noto Sans Arabic`, `DejaVu Sans`, `Tahoma`), `arabicchat-size` (20), `arabicchat-x` (10), `arabicchat-y` (140).

## Limits you should know
- **Both sides need the plugin** and the server must pass bytes >= 0x80 through unchanged (the classic protocol only carries 1 byte per char, so Arabic is tunnelled as UTF-8: ~30 letters per 64-byte line; longer messages are split into several lines).
- No paste (Ctrl+V) or cursor movement in the input bar yet; no bracket mirroring.
- Windows and Linux only. macOS would need a CoreText backend (`BuildTexture` is stubbed there).

## Build (Linux; needs freetype2 + fontconfig dev packages, e.g. `pacman -S freetype2 fontconfig`)
    gcc -O2 -fPIC -shared ArabicChat.c arabic_core.c ft_text.c -o ArabicChat.so $(pkg-config --cflags --libs freetype2 fontconfig)
Copy `ArabicChat.so` into ClassiCube's `plugins/` folder. `-I` the ClassiCube repo root if `src/...` includes are not found.

## Build (Windows, MinGW; needs the ClassiCube source `src/` folder next to these files and `ClassiCube.exe`)
    gcc -O2 -shared -s ArabicChat.c arabic_core.c gdi_text.c -o ArabicChat.dll -L . -lClassiCube -lgdi32
or with a ClassiCube import lib: put `ClassiCube.exe` in this folder and use `-L . -lClassiCube` as above
(see doc/plugin-dev.md in the ClassiCube repo). Copy `ArabicChat.dll` into ClassiCube's `plugins/` folder.

## Test the Linux text backend
    gcc test_ft.c ft_text.c arabic_core.c $(pkg-config --cflags --libs freetype2 fontconfig) -o test_ft && ./test_ft

## Test the core (any OS)
    gcc arabic_core.c test_core.c -o test_core && ./test_core
