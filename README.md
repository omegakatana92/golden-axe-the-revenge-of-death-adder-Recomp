# Golden Axe: The Revenge of Death Adder — native recompilation

A native Windows / Linux port of Sega's 1992 arcade game **Golden Axe: The Revenge of Death Adder**
(World, Rev B, MAME set `ga2`, Sega System 32). The V60 program is translated ahead of time into C, the
Z80 sound driver is decompiled into C, and a System 32 board model, the video hardware and the
YM3438/RF5C68 sound chips run natively. No emulator, MAME, Wine or Proton process is used at runtime.

This is an independent reverse-engineering project. It is not affiliated with, endorsed by or sponsored by
Sega. *Golden Axe* is a trademark of Sega. No ROM or extracted game asset is included in this repository or
in its source releases.

## What works

- Boot, region warning, Sega logo, title screen, attract mode, coin/start, character select and gameplay.
- Native V60 C generated from the player's own ROM (`tools/recompile.py`), timed like MAME's V60 core
  (8 board cycles per instruction). Video hashes match the cycle-accurate MAME reference on every frame of
  a 3,000-frame (50-second) coin/start/play scenario and a 3,000-frame attract-mode loop; see [TEST_RESULTS.md](TEST_RESULTS.md).
- System 32 video: four tilemaps, text and bitmap layers, two sprite buffers and the mixer.
- The 4-player PCB: P1/P2 through the 315-5296, P3/P4 through the i8255 at `0xC00060`.
- The V25 protection MCU is modelled, not emulated: its reset routine only fills the 4 KB DPRAM with a
  sprite table and a wake-up string, then halts. The port performs the same initialisation from the
  player's MCU ROM (`docs/PROTECTION.md`).
- Sound: the Z80 program is decompiled to C (`src/sound/`), running on the board's timing with the
  YM3438 and RF5C68 libraries. The driver runs and produces audio; a small number of command-dispatch
  paths still differ from MAME's Z80 state and are listed in [COMPATIBILITY.md](COMPATIBILITY.md).
- Keyboard and gamepad input for four players, fullscreen/windowed presentation with the original 4:3
  aspect ratio.
- The EEPROM (settings/bookkeeping) is saved beside the executable on Windows or in the user folder on
  Linux.

Full stage progression, every boss, the ending and long four-player sessions are still being validated.
See [COMPATIBILITY.md](COMPATIBILITY.md) and [TEST_RESULTS.md](TEST_RESULTS.md) for exactly what has been
run.

## The ROM set

You need the MAME **`ga2`** set (Golden Axe: The Revenge of Death Adder, World, Rev B). The port accepts
the original `ga2.zip` or a folder with its 19 files; every file is checked by CRC-32 and SHA-1. The set is
only read, never modified.

```
rom/ga2.zip        # or the loose files
```

Alternatively put `ga2.zip` beside the executable. The application searches:

1. `ga2.zip` beside the executable,
2. a `rom/` folder beside it (the zip or loose files).

The V25 protection firmware `epr-14468-02.u3` is part of the set and is read at startup; no MCU emulation
is used.

## Playing

| Action | Player 1 | Player 2 | Player 3 | Player 4 |
|---|---|---|---|---|
| Move | W A S D | arrows | T F G H | Numpad 8 4 5 6 |
| Attack / jump / magic | J K L | Numpad 1 2 3 | U I O | Numpad 7 9 3 |
| Start | 1 | 2 | 3 | 4 |
| Coin | 5 | 6 | 7 | 8 |

| System | Key |
|---|---|
| Test menu | F2 |
| Service | 9 |
| Full screen | F11 or Alt+Enter |
| Smooth / sharp pixels | F3 |
| Quit | Escape |

Gamepads (Xbox layout, Steam Deck built-in controls, arcade sticks): left stick or D-pad moves, A/B/X are
the three buttons, Start starts, Back inserts a coin; RT/LT also attack/jump. Up to four controllers are
assigned in the order they are connected (player 1 first). Steam Deck's built-in controls need no extra
mapping: the AppImage reads them through SDL's game controller API.

## Releases

- `release/Windows/GoldenAxe2.exe` — Windows x86-64, SDL2.dll and documentation included. Put your own
  `ga2.zip` beside it.
- `release/SteamDeck/GoldenAxe2-SteamDeck-x86_64.AppImage` — Linux x86-64 AppImage with SDL2 bundled. Put
  your own `ga2.zip` beside it and run it directly (no Wine, Proton or MAME).

The release folders intentionally contain no ROM or extracted game data. The generated V60 C and the
decompiled sound C are built from your own set and are not part of a public source release.

## Building

The repository carries no CMake file: `tools/write_cmake.py` writes `CMakeLists.txt` when a build starts,
and both build scripts do that automatically. The source archive has no ROM, no generated game code and no
CMake file.

### Windows 10/11 (MSYS2 MinGW64)

```powershell
powershell -ExecutionPolicy Bypass -File build_windows.ps1
```

The script verifies the ROM, builds the reference tools, writes the traces, translates the V60 program,
decompiles the sound driver, builds the game and packages `release/Windows`.

### Linux / Steam Deck

```sh
sudo apt install build-essential cmake ninja-build python3 libsdl2-dev zlib1g-dev
./build_appimage.sh
```

Output: `release/SteamDeck/GoldenAxe2-SteamDeck-x86_64.AppImage`. The script builds the native ELF,
stages an AppDir with SDL2, smoke-tests the staged application and the final AppImage, and writes
`SHA256SUMS.txt`.

See [docs/BUILDING.md](docs/BUILDING.md) for details and troubleshooting.

## Layout

```
runtime/              System 32 board model, video, sound board and chips (BSD-3-Clause lineage)
ports/sdl2/           the window, input and audio port
ports/headless/       headless runner used for reference comparisons
tools/core/           arcade-decomp, the shared V60/Z80 decompiler and analysis (BSD-3-Clause)
tools/reference/      MAME V60/Z80/video/RF5C68 and ymfm, reference tools only (BSD-3-Clause)
tools/ga2_rom.py      the 'ga2' set, its checksums and the board's region layout
tools/recompile.py    V60 -> C translation scaffold
tools/decompile_sound.py  Z80 sound driver -> C (src/sound/)
symbols.txt           names and entry points used by the translator
sound_symbols.txt     names and entry points used by the sound decompiler
src/sound/            decompiled sound program (generated from your ROM; not committed)
build/generated/      translated V60 program (generated from your ROM; not committed)
tests/controls/       scripted inputs for the reference runner
docs/                 analysis, protection, compatibility and build notes
```

## How it works

- **V60 code.** `tools/recompile.py` disassembles the program recursively from the reset vector, the
  interrupt vectors and the calls/transfers observed in a reference run, then writes one C function per
  original function with the V60's registers, flags and memory operations preserved. It is an ahead-of-time
  translation, not an interpreter.
- **Timing.** MAME's V60 core charges 8 board cycles per instruction. Every translated instruction calls
  `v60_tick()`, which advances the System 32 board by 8 cycles and runs any timer, VBLANK start/end or
  sprite-list event that falls due. The game's wait loop at `0x133505` is ordinary code: its polling
  iterations take time, the VBLANK handler sets its flag, and the loop leaves exactly as on the board.
- **Handlers that replace return addresses.** Golden Axe pushes a handler address and later executes
  `rsr` to enter it. The generated code follows the V60 stack: when a callee returns to a different
  translated function, that function runs and the C frames unwind to the matching continuation.
- **Video.** The board model's memories are drawn by `runtime/src/video.c`, whose output is compared pixel
  for pixel with MAME's System 32 renderer in the reference runner.
- **Protection.** The V25 ROM's reset routine is a fixed DPRAM initialiser; `docs/PROTECTION.md` documents
  the disassembly and the native replacement. No protected code is bypassed or patched.
- **Sound.** The Z80 driver is decompiled to C. `runtime/src/sound_cpu.c` gives each original basic block
  its board time; the YM3438 and RF5C68 are native C libraries.
- **Data.** The translated program still reads its data tables, graphics, samples and sound program from
  the board's memories, exactly where the original reads them. That is why the ROM set is required at
  runtime even though the CPU code is native.

## Legal

- New project code is under the **PolyForm Noncommercial License 1.0.0**; see [LICENSE](LICENSE).
- `runtime/`, `tools/core/`, `tools/reference/` and the decompiled code retain their BSD-3-Clause notices
  and licenses. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
- No Sega ROM data, graphics, sound samples or extracted assets are distributed here. The generated C in
  `build/generated/` and `src/sound/` is derived from the user's ROM and must not be redistributed.
