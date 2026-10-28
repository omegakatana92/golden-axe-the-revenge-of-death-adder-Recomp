# The V25 protection MCU

Golden Axe: The Revenge of Death Adder is listed by MAME as *"protected via a custom V25 with encrypted
code"*. That is the only component on the board whose CPU family `arcade-decomp` does not support (V60,
V70 and Z80 are). This note records what the V25 actually does, how it was verified, and why the native
port models its initialisation instead of running its instructions.

## Hardware wiring

- V25 at 10 MHz on sub board `834-8529-02`, program at `0x00000-0x0FFFF` and `0xF0000-0xFFFFF`.
- 4 KB dual-port RAM (`MB8421`-class) at the V25's `0x10000-0x1FFFF`; the V60 reads the other side at
  `0xA00000-0xA00FFF`, 8-bit data on the low byte lane.
- No interrupt line is wired to the V25. The V60 cannot reset or interrupt it; the MCU only runs its
  power-on routine.

## Encryption and startup

MAME's `decrypt_protrom` unscrambles the 64 KB firmware's address lines:

```
rom[i] = temp[bitswap<16>(i, 14, 11, 15, 12, 13, 4, 3, 7, 5, 10, 2, 8, 9, 6, 1, 0)]
```

The V25 then starts in its secure mode: every opcode fetch is translated through
`ga2_opcode_table[256]` (`src/mame/sega/segas32_m.cpp`). A trace of the real device (MAME 0.289, debugger
`trace` on `mainpcb:mcu`) shows:

```
FFFF0: br      0000:0400h
00400: di
00401: mov     aw,1000h
00404: mov     ss,aw
00406: mov     sp,800h
00409: call    424h
...
00411: mov     aw,1000h
00414: mov     ds1,aw
00416: mov     ix,0h
00419: call    54Dh          ; clear all 4 KB of DPRAM
...
0041C: call    504h          ; copy 48 bytes from ROM 0x51D to DPRAM 0x080
0041F: call    4DBh          ; copy 16 bytes from ROM 0x4F4 to DPRAM 0x000
00422: br      422h          ; halt forever
```

The first call at `0x424` copies nine 3-byte records from ROM `0x1000` into the upper 4 KB of its own
address space (`0xF0000 + …`), which is ROM on the real board and has no effect on the DPRAM. It does not
alter the values below. After `0x422` the V25 never runs again; MAME's trace shows its PC fixed at
`0x422` for the rest of the run.

## The DPRAM image

After reset the V60 side contains:

```
0x000: 0A 00 C5 00 11 00 11 00 18 00 18 00 1F 00 C6 00
0x080: "wake up! GOLDEN AXE The Revenge of Death-Adder! "
```

The 16 bytes at 0x000 are the sprite-protection table MAME's old simulation described as
`{0x0a,0, 0xc5,0, 0x11,0, 0x11,0, 0x18,0, 0x18,0, 0x1f,0, 0xc6,0}`. The string at 0x080 is the
wake-up check the V60 reads at `0xA00100` during boot. Both are copied from the player's
`epr-14468-02.u3` at runtime, so the source tree contains no game data:

```c
for (i = 0; i < 16; i++)  dpram[i]      = game_mcu[0x4F4 + i];
for (i = 0; i < 48; i++)  dpram[0x80+i] = game_mcu[0x51D + i];
```

(`game_mcu` is the address-decoded firmware built by `tools/ga2_rom.py:decrypt_mcu`.)

## Why this is not a bypass

The port does **not** patch the V60 program, skip a check, or fabricate a different answer. It reproduces
the observable behaviour of a hardware device whose complete firmware execution is a fixed memory
initialisation followed by a halt. The V60 still reads and validates the same table and string from the
same addresses; if the player's MCU ROM is missing or wrong, the set verification fails. Running a V25
interpreter would execute the same stores and then spin forever, so it would add no behaviour and would
make the runtime depend on an emulated CPU — exactly what this project avoids.

If a future revision of the game is found whose V25 does respond to DPRAM commands, this model is not
valid for it; the project only claims `ga2` World Rev B (and the US/Japan revisions use the same
firmware and init routine, but their main programs have not been validated here).
