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
runs its init and main loop, and the first 23 frames' sample and state hashes match.

The two command dispatchers (`sound_0473`, `sound_1094`) push a continuation address and jump through a
table whose handlers return to it. The decompiler's CFG cannot see a return address that only the Z80 stack
holds, so the raw generated C returned after one handler and never advanced the command stream; the game
was effectively silent. `tools/fix_sound_dispatch.py` rewrites only those two generated functions to run
the original continuation (increment DE, store the stream pointer for `sound_0473`, loop), matching the
Z80's pushed return path.

With the fix, the native game produces sustained audio in gameplay. Measured through SDL's disk audio
driver for a 900-frame autoplay run: **peak 4270, RMS 540** over 13.5 seconds (the first 900 frames); the
reference play run measures peak 5708 / RMS 986 over the same music. The sound board's per-frame sample and
state hashes still differ on 235 of 300 frames, so the remaining issue is command/timing fidelity, not
silence. This is documented in `COMPATIBILITY.md`.

### Window/release sound backend (sound fix)

Because the native Z80 command/timing fidelity is still incomplete, the window and release builds now
use the **reference sound board** by default (`GA2_SOUND_REFERENCE=ON`): the original Z80 program on
MAME's Z80 core, MAME's RF5C68 and ymfm's YM3438, through `runtime/src/sound_reference.cpp`. It is
emulated, and documented as such.

Verified with SDL's dummy drivers and the same 900-frame autoplay sequence:

```text
audio: dummy, 48000 Hz, 2 channels, 1024 samples
raw:   2,880,000 bytes = 720,000 stereo frames = 15.0 s = 800 samples/frame
peak:  5666    RMS: 1108.6
reference (s32ref --sound-wav, same script): peak 5947, RMS 1128.0
```

The remaining sample-level difference comes from the V60 translation's cycle timing, not the sound
board. The native decompiled Z80 backend remains selectable with `-DGA2_SOUND_REFERENCE=OFF`.

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
