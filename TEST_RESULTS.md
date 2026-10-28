# Test results

All runs use the supplied `ga2` set (World, Rev B), verified 19/19 by CRC-32 and SHA-1. The reference is
`s32ref`, which runs the original V60/Z80 code on MAME's cores against the same System 32 board model. The
native program is the C written by `tools/recompile.py`; the native sound is the C written by
`tools/decompile_sound.py`.

## Summary

| Test | Frames | Video hash differences | State hash differences | Result |
|---|---:|---:|---:|---|
| Boot / attract | 300 | 0 | 2 | picture exact every frame |
| Coin, start, character select, 15 s play | 900 | 0 | 32 | picture exact every frame |
| Coin, start, character select, 50 s play | 3,000 | 0 | 172 | picture exact every frame |
| Attract mode, full loop | 3,000 | 0 | not measured | picture exact every frame |
| Scripted reference sound (first 300 frames) | 300 | — | sample hash differs on 236 frames | driver runs; see caveat |

The video comparison is the strongest end-to-end check: `s32ref --video-hashes` draws every frame with
MAME's System 32 renderer, and the native headless port draws the same frame with `runtime/src/video.c`.
A zero difference means the V60 wrote the same video/sprite/palette/mixer state at the frame boundary and
the renderer produced identical pixels for all 3,000 frames.

State differences are transient and do not affect the picture. They include off-screen video RAM and
scratch values. The work-RAM dump at the first differing frame (24) differs only at `0x20FFA8` and
`0x20FFF8`, inside the V60 stack region that the state hash excludes; the hash difference at that frame is
from video RAM not used by the visible picture. They are recorded here rather than hidden.

## Commands

```sh
# ROM
python tools/verify_rom.py

# Reference picture and state
build/bin/s32ref --timing cycles --rom rom --script tests/controls/play_long.txt --frames 3000 \
  --video-hashes build/ref/ref_video3000.txt --hashes build/ref/ref_hashes3000.txt

# Native
build/bin/ga2_headless --rom rom --script tests/controls/play_long.txt --frames 3000 \
  --video-hashes build/ref/native_video3000.txt --hashes build/ref/native_hashes3000.txt

# compare
diff build/ref/ref_video3000.txt build/ref/native_video3000.txt
```

Observed:

```
state diffs=172 of 3000
video diffs=0 of 3000
```

## Sound

`s32ref --sound-hashes --sound-timing build/sound/timing.txt` and
`ga2_headless --sound-hashes` both run the Z80 with the same per-block time. The decompiled driver starts,
runs its init and main loop, and the first 23 frames' sample and state hashes match. From frame 24 the
sample hash differs; the board's shared RAM shows different driver variables (`E000-E008`, `E060-E061`).

The cause is known: the sound program's two command dispatchers (`sound_0473`, `sound_1094`) push a
continuation address and jump through a table whose handlers return to it. The decompiler's CFG cannot see
a return address that only the Z80 stack holds, so those two functions' generated C does not update the
command-stream pointer the way the original does. All other 73 decompiled functions are emitted normally.
This is a correctness issue, not a crash: the game runs and audio is produced. It is listed in
`COMPATIBILITY.md` and `BUILD_STATUS.md` as the remaining sound work.

## Startup and packaging

| Check | Result |
|---|---|
| Windows `GoldenAxe2.exe --smoke-test --frames 600 --rom rom` | exit 0, 600 frames |
| Linux ELF `GoldenAxe2 --smoke-test --frames 600 --rom rom` | exit 0, 600 frames |
| Linux headless `ga2_headless --rom rom --frames 60` | 59 frames, clean exit |
| Staged AppDir smoke test (600 frames) | exit 0 |
| Final AppImage smoke test (300 frames) | exit 0 |
| AppImage `--appimage-extract` inspection | contains SDL2, executable, desktop file, icon; no ROM |
| Windows zip contents | exe, SDL2.dll, README, licenses; no ROM |

The AppImage was built on Ubuntu 22.04 (glibc 2.35) for SteamOS 3 compatibility. It bundles SDL2; the
Steam Deck's built-in controller is read through SDL's game controller API.

## Not yet verified

- Full stage progression, all bosses, the ending, and 2/3/4-player sessions beyond the scripted run.
- Sound command dispatch parity (known issue above).
- Frame-perfect input response on physical Steam Deck hardware (the AppImage and gamepad path were tested
  with SDL's dummy video driver and a virtual controller only in this environment).
- Windows real hardware boot beyond the smoke test (the process starts, runs 600 frames and exits 0).

No screenshot is included in this repository: a frame from the game contains copyrighted graphics, and the
project keeps its source tree ROM-free. Run the commands above locally to produce your own frames
(`--video-dump N --video-dir DIR`).
