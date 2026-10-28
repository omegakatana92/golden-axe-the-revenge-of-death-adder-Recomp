# Program analysis and implementation notes

## What the runtime is

```
V60 C (build/generated/)  ─┐
                           ├─ runtime/src/v60.c ─ runtime/src/machine.c ─ runtime/src/io.c
Z80 C (src/sound/)        ─┘                         │
                                                     ├─ runtime/src/video.c
                                                     ├─ runtime/src/sound.c / sound_chips.c
                                                     └─ runtime/src/rf5c68.c / ym3438.c
```

`runtime/src/machine.c` is the System 32 bus: work RAM, video RAM, sprite RAM, palette, mixer, shared RAM,
DPRAM, the interrupt controller, timers and VBLANK. `runtime/src/io.c` is the 315-5296, the 93C46 EEPROM,
the optional trackball/ADC devices and the 4-player PCB's i8255. `runtime/src/video.c` draws the four
tilemaps, text/bitmap layers and the two sprite buffers. The sound files run the decompiled Z80 program
against the YM3438/RF5C68 models.

## Address map used by the translated code

| Range | Device |
|---|---|
| `0x000000-0x1FFFFF` | V60 program (mirrored at `0xF00000-0xFFFFFF`) |
| `0x200000-0x20FFFF` | work RAM (mirrored through `0x2FFFFF`) |
| `0x300000-0x31FFFF` | video RAM |
| `0x400000-0x41FFFF` | sprite RAM |
| `0x500000-0x50000F` | sprite control |
| `0x600000-0x60FFFF` | palette |
| `0x610000-0x61007F` | mixer |
| `0x700000-0x701FFF` | shared RAM with the Z80 |
| `0xA00000-0xA00FFF` | V25 DPRAM (8-bit low lane) |
| `0xC00000-0xC0001F` | 315-5296 |
| `0xC00060-0xC00067` | i8255 (P3/P4) |
| `0xD00000` | interrupt controller / timers |
| `0xD80000-0xDFFFFF` | random number generator |

## Translation

`tools/recompile.py` uses the shared core's recursive V60 disassembler. It decodes from the reset vector,
the vector table at `0x100000` (SBR is loaded with `0x100000` at `0x100506`) and every call seen in the
reference trace. The output is one C function per original function; instructions preserve the V60's
registers, flags, memory access widths and ordering.

Two game-specific patterns were added to the core while bringing the game up:

1. **Calls whose callee replaces its return address.** The game uses `push #handler` followed by `rsr`
   (or a callee that pops a different address) to chain handlers. The generated call site checks the
   V60's actual return address with `v60_returned_to()`. When it differs, the runtime runs that translated
   function and the C frames unwind to the matching continuation. `symbols.txt` also marks the
   `push #imm` destinations as continuations so the generated function has a `goto` label for them.
2. **Branch tables with negative indices.** At `0x105B3C` the V60 does `jmp 0x107538[PC](R3)`; a negative
   `R3` lands before the printed table base (`0x10752F`). The analysis now includes the run of fixed-size
   `br` entries immediately before a table base.

The unsupported V60 instructions encountered were `skpcu.h` / `schcu.h` (search strings), `cmpc.h`
(compare strings) and `clrtlb` (a no-op on MAME because no TLB is modelled). They are implemented by
`v60_search_bytes`, `v60_search_words`, `v60_compare_bytes`, `v60_compare_words` and a no-op in
`runtime/src/v60.c`; these follow MAME's `opSEARCHUB/H` and `opCMPSTRB/H` flag and R27/R28 results.

## Frame timing

MAME's V60 charges 8 board cycles per instruction. Every generated instruction calls `v60_tick()`, which
advances the board by 8 cycles and runs timers, VBLANK start/end and the delayed sprite-list event when
they fall due. The game's wait loop is ordinary code:

```
133505: test1 #0, 2C08[R25]     ; bit 0 of the byte at 0x20AC08
13350B: bne   133505
13350D: rsr                      ; frame done
```

`symbols.txt` names `133505 frame_wait_loop` and `13350D frame_wait_done`. With cycle timing, the loop's
polling takes real board time; the VBLANK handler clears the flag, and the generated program leaves the
loop exactly when the original does. The headless and SDL ports both use `s32_board.frame_timing = false`
and let `v60_tick()` drive the board.

## Video

`runtime/src/video.c` is the shared System 32 renderer. It reads the same video/sprite/palette/mixer
memories the original does and draws a 416×224 (or 320×224 when the game selects it) picture. The
reference tool `tools/reference/video/s32video.cpp` is MAME's renderer; `s32ref --video-hashes` and the
headless port's `--video-hashes` produce one FNV-1a hash per frame. A 3,000-frame comparison is exact
(`TEST_RESULTS.md`).

The SDL port presents the picture at the cabinet's 4:3 aspect by default; F4 switches to square pixels
(the native 416×224 pixel grid).

## Inputs

- P1/P2: 315-5296 ports A/B; service/test/coins/starts on port E.
- P3/P4: i8255 ports A/B at `0xC00060`/`0xC00062`; starts 3/4 on port C bits 0/1.
- The i8255 is a standard mode-0 device: the V60 writes the control word at `0xC00066` (`0x93` during
  boot, then its input configuration) and reads the ports. The runtime implements set/reset and the
  direction bits; unconnected bits read as 1.

## Sound

The Z80 program is decompiled by `tools/decompile_sound.py`. Its entry points are reset `0x0000` and
interrupt `0x0038`; the main loop is `0x0075`; four jump tables were identified and declared in
`sound_symbols.txt`:

```
110E  jumps sound_command_table 32
10AC  via   sound_command_table
04A8  jumps sound_table_04A8 32
049E  via   sound_table_04A8
09A0  jumps sound_table_09A0 128
075B  via   sound_table_09A0
```

The decompiler's stack check flags the two command dispatchers `sound_0473` and `sound_1094` because they
push a return address and jump through a table whose handlers return to it; the CFG cannot see a return
address that only the stack knows. They are allowed to be emitted, and the generated C is built. Their
behaviour is the remaining sound-fidelity work item (`COMPATIBILITY.md`).

`runtime/src/sound_cpu.c` gives each original basic block its board time; the YM3438 and RF5C68 are native
C libraries. `s32ref --sound-timing build/sound/timing.txt` runs MAME's Z80 with the same per-block timing
model, so any difference is a code/control-flow difference, not a clock difference.

## Symbols

`symbols.txt` currently names only what the scaffold and analysis need: the vector table, the wait loop
and its end. All other functions are `sub_ADDRESS` until they are reversed. This is deliberate: the
translation is the game's own code, not a hand-written reimplementation.

## Reference vs native state

Video hashes match every compared frame. Work RAM and the sound board's shared RAM differ on a small
number of transient frames. The 3,000-frame run has a matching picture on all frames; the state
differences are being reduced (see `COMPATIBILITY.md` and `BUILD_STATUS.md`).
