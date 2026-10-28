# ROM analysis — Golden Axe: The Revenge of Death Adder

## Identification

| Field | Value |
|---|---|
| Game | Golden Axe: The Revenge of Death Adder |
| MAME set | `ga2` — World, Rev B (parent set: none) |
| Manufacturer | Sega |
| Release | 1992 |
| Board | Sega System 32, 4-player PCB `833-8933-02` |
| Sub board | `834-8529-02` (V25 protection CPU) |
| A/D board | `837-7968` listed by MAME; this game's inputs are digital (P1/P2 on the 315-5296, P3/P4 on the i8255) |
| Main CPU | NEC V60, 16,107,950 Hz (32.2159 MHz / 2) |
| Protection CPU | NEC V25, 10,000,000 Hz, with a 4 KB dual-port RAM |
| Sound CPU | Zilog Z80, 8,053,975 Hz (32.2159 MHz / 4) |
| Sound chips | 2 × YM3438 (OPN2C) at 8,053,975 Hz, 1 × Ricoh RF5C68 at 12,500,000 Hz |
| Video | System 32 raster, 416 × 224, 60 Hz, 262 lines, 6,539,520 Hz pixel clock |
| Palette | 16,384 entries (`0x4000`) |
| Work RAM | 64 KB at `0x200000`, mirrored to `0x2FFFFF` |
| Video RAM | 128 KB at `0x300000`, mirrored to `0x31FFFF` |
| Sprite RAM | 128 KB at `0x400000`, mirrored to `0x41FFFF` |
| Shared RAM | 8 KB at `0x700000` (the Z80 sees it at `0xE000-0xFFFF`) |
| DPRAM | 4 KB at `0xA00000` (V25 side `0x10000-0x10FFF`) |

All clock and display values are from MAME 0.289 `sega/segas32.cpp` and `mame ga2 -listxml`.

## ROM set

The supplied archive is the complete MAME `ga2` set: **19 files, 8,594,070 bytes**. Every file was matched
by CRC-32 and SHA-1; see `tools/verify_rom.py` and the table below.

| File | Size | CRC-32 | SHA-1 | Region / load |
|---|---:|---|---|---|
| `epr-14961b.ic17` | 0x020000 | `d9cd8885` | `dc9d1f01…` | main program, `0x000000`, ×4 |
| `epr-14958b.ic8` | 0x020000 | `0be324a3` | `5e5f4575…` | main program, `0x080000`, ×4 |
| `epr-15148b.ic18` | 0x040000 | `c477a9fd` | `a9d60f80…` | main program, `0x100000`, even bytes, ×2 |
| `epr-15147b.ic9` | 0x040000 | `1bb676ea` | `125ffd13…` | main program, `0x100001`, odd bytes, ×2 |
| `epr-14945.ic36` | 0x010000 | `4781d4cb` | `bd1b774b…` | Z80 program, `0x000000`, ×16 |
| `mpr-14944.ic35` | 0x100000 | `fd4d4b86` | `e14b9cd6…` | sound bank, `0x100000` |
| `mpr-14943.ic34` | 0x100000 | `24d40333` | `38faf8f3…` | sound bank, `0x200000` |
| `mpr-14942.ic24` | 0x100000 | `a89b0e90` | `e14c6241…` | sound bank, `0x300000` |
| `epr-14468-02.u3` | 0x010000 | `77634daa` | `339169d1…` | V25 protection firmware |
| `mpr-14948.ic14` | 0x200000 | `75050d4a` | `51d6bc99…` | tiles, even bytes |
| `mpr-14947.ic5` | 0x200000 | `b53e62f4` | `5aa0f198…` | tiles, odd bytes |
| `mpr-14949.ic32` | 0x200000 | `152c716c` | `448d16ea…` | sprites, base 0, lane 0 |
| `mpr-14951.ic30` | 0x200000 | `fdb1a534` | `3126b595…` | sprites, base 0, lane 1 |
| `mpr-14953.ic28` | 0x200000 | `33bd1c15` | `4e16562e…` | sprites, base 0, lane 2 |
| `mpr-14955.ic26` | 0x200000 | `e42684aa` | `12e0f18a…` | sprites, base 0, lane 3 |
| `mpr-14950.ic31` | 0x200000 | `15fd0026` | `e918984b…` | sprites, base 0x800000, lane 0 |
| `mpr-14952.ic29` | 0x200000 | `96f96613` | `4c980886…` | sprites, base 0x800000, lane 1 |
| `mpr-14954.ic27` | 0x200000 | `39b2ac9e` | `74f4c81d…` | sprites, base 0x800000, lane 2 |
| `mpr-14956.ic25` | 0x200000 | `298fca50` | `16e09b19…` | sprites, base 0x800000, lane 3 |

The full SHA-1 values are in `tools/ga2_rom.py` and printed by `tools/verify_rom.py`.

## Region layout

### V60 program (`mainpcb:maincpu`, 2 MB)

```
0x000000-0x07FFFF  epr-14961b.ic17 repeated four times (128 KB → 512 KB)
0x080000-0x0FFFFF  epr-14958b.ic8  repeated four times (128 KB → 512 KB)
0x100000-0x1FFFFF  epr-15148b.ic18 / epr-15147b.ic9 interleaved, each decoded twice
0xF00000-0xFFFFFF  first 1 MB of the same region (the V60's reset/vector mirror)
```

The V60 starts at `0xFFFFFFF0`, which the board mirrors to `0xFFFFF0`. The reset code is
`D6 F3 06 05` (`jmp 0x100506`). At `0x100506` it runs `ldpr #100000, #5` (SBR = `0x100000`), so the
interrupt vector table is the 256-entry table at `0x100000`, not `0xBE000` (that is Alien 3's table).
Vectors `0x40-0x44` are the five System 32 sources; vector 2 is the default/error handler.

### Sound (`mainpcb:soundcpu`, 4 MB)

```
0x000000-0x0FFFFF  epr-14945.ic36 repeated sixteen times (64 KB → 1 MB)
0x100000-0x1FFFFF  mpr-14944.ic35
0x200000-0x2FFFFF  mpr-14943.ic34
0x300000-0x3FFFFF  mpr-14942.ic24
```

The Z80 program decodes `0x0000-0x9FFF`; `0xA000-0xBFFF` is a banked window into the 3 MB of samples;
`0xC000-0xDFFF` is the RF5C68's 8 KB register/window region; `0xE000-0xFFFF` is the shared RAM.

### Tiles (`mainpcb:tiles`, 4 MB)

Two 2 MB chips interleaved (`mpr-14948.ic14` even, `mpr-14947.ic5` odd): 32,768 16×16 tiles at 4 bpp.

### Sprites (`mainpcb:sprites`, 16 MB)

Eight 2 MB chips, four per 8 MB base (`0x000000`, `0x800000`), spread over 64-bit words as MAME's
`ROM_LOAD64_WORD`. Every 32-bit word is big-endian in the sprite ROM layout.

## Dependencies

- **No parent set or BIOS.** MAME's `GAME(1992, ga2, 0, …)` has no parent; the 19 files above are the
  complete requirement.
- **No CHD, disk, NVRAM or sample file.** The game's EEPROM is a 93C46-family serial device with no
  factory image in the set; the port starts it blank (all ones), which makes the game use default
  settings, and saves changes beside the executable.
- **The V25 firmware is required.** It supplies the DPRAM's sprite-protection table and wake-up string.
  The port reads those bytes from `epr-14468-02.u3`; it does not execute the V25.

## Toolchain assessment

`arcade-decomp` supports the NEC V60/V70 and the Z80: the exact CPUs this board uses. The V25 is **not**
supported by the toolkit, and is the only unsupported CPU on the board. Its firmware was analysed and
found to be a reset-time DPRAM initialiser that then halts; the native port models that initialisation
directly instead of interpreting V25 instructions. There is no unsupported graphics, sound or input chip:
the 315-5296, i8255, YM3438 and RF5C68 are all implemented in the runtime.

Consequently the game is **not** running through an emulated CPU at runtime: the V60 program and the Z80
program are C, and the only non-native component would have been the V25, which is replaced by its
verified initialisation behaviour (`docs/PROTECTION.md`).

## Checksums

```
19/19 files of the 'ga2' set
```

Run `python tools/verify_rom.py` to reproduce this report against any copy of the set.
