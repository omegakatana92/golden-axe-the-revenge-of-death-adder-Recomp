# Compatibility and known issues

## Working

- Boot, Sega logo, title, attract mode, coin/start, player select, stage 1 gameplay.
- V60 program translated to C and timed with the board; no V60 interpreter.
- System 32 video: tilemaps, text, bitmap, sprites, palette and mixer.
- 4-player PCB input path: P1/P2 on the 315-5296, P3/P4 and start 3/4 on the i8255 at `0xC00060`.
- V25 protection DPRAM initialisation from the player's firmware (no V25 execution).
- EEPROM saving/loading.
- SDL2 window, fullscreen, keyboard and up to four gamepads.
- Native Windows x64 and Linux x86-64 builds; Linux AppImage packages and launches.

## Known limitations

1. **Sound fidelity.** The native Z80 driver is decompiled and runs; audio is produced and sustained. The
   two command dispatchers `sound_0473` and `sound_1094` needed a post-pass
   (`tools/fix_sound_dispatch.py`) to follow the continuation address the original pushes on the stack,
   because the decompiler's CFG cannot see it. With that pass the game is no longer silent, but the sound
   board's sample/state hashes still differ from MAME's Z80 on some frames (235 of 300 in the first
   scripted run). The window/release build therefore uses the **reference sound board** by default
   (`GA2_SOUND_REFERENCE=ON`): the original Z80 program on MAME's Z80 core with MAME's RF5C68 and ymfm's
   YM3438. That backend is emulated and reference-accurate in level/content; it produces exactly 800
   samples per 60 Hz frame at 48 kHz. The native backend remains selectable with
   `-DGA2_SOUND_REFERENCE=OFF`. The long-term fix is a Z80 return-address model in the decompiler.
2. **State-hash transients.** Video is frame-exact on the 3,000-frame runs, but work RAM/video RAM state
   hashes differ on 172 frames of the scripted run. These are off-screen or scratch values; no visible
   difference was found.
3. **Full-game validation.** The run covers the first 50 seconds with one character. Later stages, bosses,
   the ending and 2/3/4-player sessions have not been played to completion.
4. **Analog board.** MAME lists an A/D board (`837-7968`) for the hardware family, but `ga2`'s inputs are
   digital; no analog control is exposed because the game does not read one. The runtime retains the
   trackball/ADC code for other System 32 profiles but ga2 does not use it.
5. **Screenshots.** None are shipped: frames contain copyrighted graphics and the repository stays
   ROM-free.

## Inputs verified

- Keyboard P1: WASD + JKL, coin 5, start 1.
- Gamepad P1: D-pad/left stick, A/B/X, Start, Back (coin), RT/LT.
- The headless reference scripts use the same input bits and produce the expected player-select and
  gameplay screens.
- P3/P4 (i8255) are implemented but not yet covered by a scripted 4-player test; this is the next input
  validation item.

## Not claimed

- This is not a MAME replacement and does not run every System 32 game. It is a per-game translation.
- No ROM, BIOS, graphics, audio or extracted asset is distributed.
- No DRM or access-control bypass is implemented; the protection MCU's own reset behaviour is reproduced
  from the user's firmware.
