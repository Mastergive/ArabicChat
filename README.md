# ArabicChat — ClassiCube client plugin (Arabic / Persian / Urdu)

Type and read right-to-left text in the ClassiCube **client**, using a **system font** (Segoe UI by default).

## What's verified and what isn't
- `arabic_core.c` (UTF-8, Arabic joining, lam-alef, bidi, chunking): **compiled and unit-tested** (`test_core.c`, ASan/UBSan clean).
- `ArabicChat.c` + `gdi_text.c`: **written against the ClassiCube 1.3.8 plugin API but NOT compiled** — I had no headers/Windows toolchain here. Expect to fix a name or two; send me the compiler errors and I'll patch them.

## Use
- `/client arabic` opens the input bar. Type with your OS Arabic keyboard layout. **Enter** sends, **Esc** cancels, **Backspace** deletes.
- Incoming lines are drawn in a small overlay (top-left, under the stats). Stock chat still shows the raw bytes as gibberish for those lines — the plugin can't edit it.
- Options (in `options.txt`): `arabicchat-font` (default `Segoe UI`; try `Tahoma`, `Arial`), `arabicchat-size` (20), `arabicchat-x` (10), `arabicchat-y` (140).

## Limits you should know
- **Both sides need the plugin** and the server must pass bytes >= 0x80 through unchanged (the classic protocol only carries 1 byte per char, so Arabic is tunnelled as UTF-8: ~30 letters per 64-byte line; longer messages are split into several lines).
- No paste (Ctrl+V) or cursor movement in the input bar yet; no bracket mirroring.
- Windows only for now. Linux/macOS need a different text backend (`BuildTexture` is stubbed).

## Build (Windows, MinGW; needs the ClassiCube source `src/` folder next to these files and `ClassiCube.exe`)
    gcc -O2 -shared -s ArabicChat.c arabic_core.c gdi_text.c -o ArabicChat.dll -L . -lClassiCube -lgdi32
or with a ClassiCube import lib: put `ClassiCube.exe` in this folder and use `-L . -lClassiCube` as above
(see doc/plugin-dev.md in the ClassiCube repo). Copy `ArabicChat.dll` into ClassiCube's `plugins/` folder.

## Test the core (any OS)
    gcc arabic_core.c test_core.c -o test_core && ./test_core
