# Credits

## cheviman — arcade-decomp

This project would not exist without **[cheviman](https://github.com/cheviman)** and
**[arcade-decomp](https://github.com/cheviman/arcade-decomp)**, the shared toolkit that provides:

- the NEC V60/V70 decoder, disassembler, lifter and C translation scaffold,
- the Z80 decoder, analysis, lift, timing model and C writer,
- the decompiler: IR, data flow, signatures, control-flow structuring, variables, pointer types and the C
  writer,
- the static analysis, coverage and structure-checking tools,
- the System 32 reference lineage used by
  [segasonic-decomp](https://github.com/cheviman/segasonic-decomp) and
  [outrunners-decomp](https://github.com/cheviman/outrunners-decomp).

`tools/core/` is his BSD-3-Clause work, kept with its original notices. The System 32 board model, video
renderer and sound-board structure in `runtime/` were adapted from that same lineage. The translation of
Golden Axe's V60 program and Z80 sound driver into C is built directly on his tools.

## MAME

**MAMEdev and contributors** wrote the reference CPU, video and I/O implementations used by `s32ref` to
validate this port: the NEC V60 core, Z80 core, System 32 video renderer, 315-5296 I/O chip, serial EEPROM,
RF5C68 and the System 32 board wiring. These reference files remain under MAME's BSD-3-Clause license and
are not linked into the game executable.

## ymfm

**Aaron Giles and ymfm contributors** wrote the YM3438/OPN2 model used by the reference sound board
(`tools/reference/ymfm/`, BSD-3-Clause).

## System 32 decompilation lineage

The earlier System 32 recompilation work (SegaSonic and OutRunners by cheviman, and the Alien 3: The Gun
port whose runtime structure this project adapted) established the board model, timing approach and
reference-comparison method used here.

## Third-party notices

Full license texts and copyright notices are in:

- `LICENSE-arcade-decomp-BSD-3-Clause.txt`
- `LICENSE-MAME-BSD-3-Clause.txt`
- `LICENSE-ymfm-BSD-3-Clause.txt`
- `THIRD_PARTY_NOTICES.md`
