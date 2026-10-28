# Third-party notices

This project builds on the System 32 decompilation lineage by **cheviman** and on MAME/ymfm reference code.
The upstream copyright notices and license texts are kept in the files and in the `LICENSE-*` files at the
repository root. See [CREDITS.md](CREDITS.md) for the full credit list.

## cheviman / arcade-decomp

- **Author:** cheviman — <https://github.com/cheviman>
- **Toolkit:** [arcade-decomp](https://github.com/cheviman/arcade-decomp), BSD-3-Clause
- `tools/core/` is a copy of that toolkit (commit `140ce2d` at the time of writing).
- The V60/V70 and Z80 frontends, decompiler, translator, analysis and checks used to produce this port are
  his work. The System 32 runtime structure was adapted from the SegaSonic, OutRunners and Alien 3: The Gun
  decompilation projects in the same lineage.

## arcade-decomp and segasonic-decomp / outrunners-decomp

- `tools/core/` is a copy of [arcade-decomp](https://github.com/cheviman/arcade-decomp), BSD-3-Clause
  (commit `140ce2d` at the time of writing).
- `runtime/`, `ports/` and the reference harness were adapted from the System 32 decompilation lineage
  ([segasonic-decomp](https://github.com/cheviman/segasonic-decomp),
  [outrunners-decomp](https://github.com/cheviman/outrunners-decomp)) and from the Alien 3: The Gun
  recompilation project, BSD-3-Clause.
- License text: `LICENSE-arcade-decomp-BSD-3-Clause.txt`.

The files adapted from those repositories keep their original copyright headers. New files written for
this project are under PolyForm Noncommercial 1.0.0.

## MAME

`tools/reference/mame/`, `tools/reference/video/` and `tools/reference/sound/` contain files derived from
MAME (BSD-3-Clause). They are used by the reference tools (`s32ref`) and, as a documented fallback, by
the window/release sound board (`runtime/src/sound_reference.cpp`, `GA2_SOUND_REFERENCE=ON`) until the
native Z80 driver's command/timing fidelity is fixed. The main V60 game is native.

- License text: `LICENSE-MAME-BSD-3-Clause.txt`.
- Behaviour references: `sega/segas32.cpp`, `sega/segas32_v.cpp`, `sega/segas32_m.cpp`,
  `sega/315_5296.cpp`, `machine/eepromser.cpp`, `machine/upd4701.cpp`, `cpu/nec/v25.cpp`.

## ymfm

`tools/reference/ymfm/` is a copy of ymfm (BSD-3-Clause), used by the reference sound board and by the
window/release sound fallback (`GA2_SOUND_REFERENCE=ON`).

- License text: `LICENSE-ymfm-BSD-3-Clause.txt`.

## Game assets

No Sega ROM, graphics, audio, sample or extracted asset is part of this repository or of the source
release. The runtime requires the user's own MAME `ga2` set. The generated V60 C, decompiled sound C and
screenshots are derived from that set and must not be redistributed.
