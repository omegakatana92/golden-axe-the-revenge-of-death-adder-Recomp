# Building Golden Axe: The Revenge of Death Adder

## Requirements

- Python 3.10 or newer.
- CMake 3.20 or newer and Ninja.
- A C11 / C++17 compiler.
  - Windows: MSYS2 MinGW64 (`C:\msys64\mingw64`) with SDL2.
  - Linux: GCC or Clang, `libsdl2-dev`, `zlib1g-dev`.
- Your own MAME `ga2` set (World, Rev B). It is never committed.
- For the reference runner and translation: the MAME V60/Z80/video/sound cores in `tools/reference/`
  (BSD-3-Clause), built by CMake.

The repository intentionally contains no CMake file (owner request). `tools/write_cmake.py` writes
`CMakeLists.txt` in the build scripts; it is ignored by git and by the source archive.

## 1. Check the ROM set

```sh
python tools/verify_rom.py            # rom/ga2.zip or a folder
```

The verifier prints each file and its CRC-32/SHA-1 match. The set is 19 files; the V25 firmware is one of
them and is required.

## 2. Build the reference and write traces

The translator needs a trace of a real run to name every computed jump and handler. The reference runner
is MAME's V60 and Z80 cores driving the same board model the game uses.

```sh
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/mingw --target s32ref -j 8
build/bin/s32ref --timing cycles --rom rom \
  --script tests/controls/play_long.txt --frames 3000 \
  --trace build/ref/exec3000.txt --calls build/ref/called3000.txt \
  --transfers build/ref/transfers3000.txt --video-hashes build/ref/ref_video3000.txt
```

`tests/controls/play_long.txt` inserts a coin, starts player 1, selects Stern, walks right and attacks for
50 seconds. Add more scenarios for new code paths and pass every trace/calls file to the translator.

## 3. Translate the V60 program

```sh
python tools/recompile.py rom --trace build/ref/exec300.txt build/ref/exec3000.txt \
  --calls build/ref/called300.txt build/ref/called3000.txt
```

This writes `build/generated/program.h`, `program_NN.c` and `entries.c`. **These are the game's code
translated from your ROM. Do not redistribute them.**

If the headless build stops with

```
Jump from 105B3C to 10752F has no translated target
```

add `10752F` to the calls file and run the translator again. `build_windows.ps1` does this loop
automatically (up to 40 rounds).

## 4. Decompile the sound program

```sh
python tools/decompile_sound.py
```

Writes `src/sound/main.c` and `sound.h` (also generated from your ROM; do not redistribute) and the timing
model in `build/sound/timing.txt`.

## 5. Build the game

### Windows (MSYS2 MinGW64)

```sh
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/mingw --target GoldenAxe2 -j 8
```

or simply:

```powershell
powershell -ExecutionPolicy Bypass -File build_windows.ps1
```

The executable is `build/bin/GoldenAxe2.exe`; the script also copies SDL2.dll and documentation into
`release/Windows` and writes `release/GoldenAxe2-Windows-x64.zip`.

### Linux / Steam Deck

```sh
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DGA2_REFERENCE_TOOLS=OFF
cmake --build build/linux --target GoldenAxe2 ga2_headless -j "$(nproc)"
./build_appimage.sh
```

The AppImage script:

1. verifies the ROM,
2. builds the native ELF,
3. stages an AppDir with SDL2, the desktop file and icon,
4. runs `GoldenAxe2 --smoke-test --frames 600` with dummy video/audio,
5. packages with `appimagetool` and runs the resulting AppImage's own smoke test,
6. writes `release/SteamDeck/GoldenAxe2-SteamDeck-x86_64.AppImage` and `SHA256SUMS.txt`.

The AppImage is x86-64 and bundles SDL2 but not glibc; it is built on Ubuntu 22.04 (SteamOS 3's glibc is
newer) and does not need root.

## Running with a ROM

Place `ga2.zip` beside the executable, or in `rom/` beside it. On Linux the AppRun also checks the folder
that contains the AppImage and `~/.local/share/GoldenAxe2/`. `GA2_ROM` overrides the search:

```sh
GA2_ROM=/path/to/rom ./GoldenAxe2
```

## Tests

```sh
build/bin/ga2_headless --rom rom --script tests/controls/play_long.txt --frames 3000 \
  --video-hashes build/ref/native_video3000.txt
```

Compare with the reference:

```sh
diff build/ref/ref_video3000.txt build/ref/native_video3000.txt
```

`build/bin/GoldenAxe2 --smoke-test --frames 600 --rom rom` runs the windowed build with dummy SDL drivers
and exits after 600 frames; it is the startup/AppImage test.

## Troubleshooting

- **`No translated code starts at …` / `Jump … has no translated target`** — a new computed target; add
  it to the calls file and re-run the translator (the build scripts do this automatically).
- **`LDPR to privileged register`** — the core's V60 privileged-register range is 0-28; report the address
  and instruction. Golden Axe uses `ldpr #100000, #5` and the interrupt/timer registers only.
- **No SDL2** — the headless target still builds. Install `libsdl2-dev` (Linux) or the MSYS2
  `mingw-w64-x86_64-SDL2` package (Windows).
- **AppImage does not start on SteamOS** — run it from a terminal with `APPIMAGE_EXTRACT_AND_RUN=1` to see
  the error; check `ldd` inside the extracted AppDir. The bundled SDL2 is the only non-system library the
  game needs.
- **Sound state differs from MAME** — see `COMPATIBILITY.md`; the known cause is the two Z80 command
  dispatchers.

## Legal

The toolkit and runtime are BSD-3-Clause. New code is PolyForm Noncommercial 1.0.0. The ROM, generated
V60 C and decompiled sound C are not redistributable. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
