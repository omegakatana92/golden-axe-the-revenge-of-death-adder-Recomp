# Build status

Updated: 2026-10-28

## Where the project lives

The requested project directory `C:\Users\omega\Documents\system 32\ga2.zip` is the ROM archive itself, not
a directory. The project was created at:

```
C:\Users\omega\Documents\Default Project\GoldenAxe2-Recomp
```

with the supplied ROM copied to `GoldenAxe2-Recomp\rom\ga2.zip`.

## Completed

- **Phase 1 — toolkit inspection.** Read `arcade-decomp`, its V60/V70 and Z80 frontends, the decompiler,
  the recompiler, the analysis/check tools, and the `segasonic-decomp` / `outrunners-decomp` System 32
  references. Identified the supported CPUs and the missing V25.
- **Phase 2 — ROM analysis.** `ROM_ANALYSIS.md`: full 19-file hash table, System 32 board, V60/V25/Z80,
  video, sound, memory map, region layout, no parent/BIOS, V25 dependency.
- **Phase 3 — project.** Runtime, ports, reference tools, decompiler core, game ROM module, symbol files,
  build scripts, packaging, docs and tests.
- **Phase 4 — translation.** V60 reset/vector analysis (SBR `0x100000`), 3,000-frame trace, 1,988
  translated functions / 132,357 instruction instances, no untranslated instructions. Added the V60 string
  instructions, `clrtlb`, push/return continuations and negative-index branch tables to the core.
- **Phase 5 — graphics/audio.** System 32 video is frame-exact against MAME on 3,000 frames. The Z80
  driver is decompiled and runs; the two command dispatchers remain a known fidelity issue.
- **Phase 6 — controls.** Keyboard P1-P4, Xbox/Steam Deck gamepad mapping, 4-player i8255 path.
- **Phase 7 — Windows.** `GoldenAxe2.exe` built with MinGW64, SDL2.dll packaged, smoke test 600 frames.
- **Phase 8 — Steam Deck.** Linux x86-64 ELF and `GoldenAxe2-SteamDeck-x86_64.AppImage` built on Ubuntu
  22.04, smoke-tested, SDL2 bundled, desktop file and icon.
- **Phase 9 — tests.** ROM verification, headless reference/native comparisons, startup tests, AppImage
  launch tests, build scripts' error reporting.
- **Phase 10 — debugging.** Documented: lazy `mov`/`ldpr` (no), V60 `clrtlb`, string instructions,
  push/return handler chains, negative branch tables, C declaration portability, SDL audio guard.
- **Phase 11 — licensing.** PolyForm Noncommercial for new code; BSD-3-Clause notices preserved for
  arcade-decomp, MAME and ymfm; no ROM in source or release.
- **Phase 12 — release prep.** `release/Windows` + zip, `release/SteamDeck` + AppImage + SHA256SUMS.
  Nothing has been committed or pushed to GitHub.

## Verified

| Item | Status |
|---|---|
| ROM set 19/19 by CRC-32 and SHA-1 | pass |
| Windows exe starts and runs 600 frames (`--smoke-test`) | pass |
| Linux ELF starts and runs 600 frames | pass |
| Headless 3,000-frame run | pass, 2,999 frames completed, clean exit |
| Video hashes vs MAME reference, 3,000 frames | 0 differences |
| Windows release zip / AppImage contents | no ROM, no generated C |
| AppImage final launch (300 frames) | pass |

## Remaining issues

1. **Sound command dispatchers** (`sound_0473`, `sound_1094`): the decompiled C does not reproduce the
   pushed continuation after a table handler returns; sound state diverges from frame 24. This is the
   highest-priority remaining defect. It needs a Z80 return-address/continuation model in the decompiler.
2. **State hash transients**: 172/3,000 frames differ in non-visible state. Investigate the first
   divergence (frame 24, off-screen video RAM) if bit-exact state is required.
3. **Full-game coverage**: stages after the first, bosses, ending and 4-player sessions are not yet
   validated.
4. **Physical Steam Deck test**: AppImage and gamepad path were exercised with dummy SDL drivers, not on
   Deck hardware.
5. **P3/P4 scripted input test**: the i8255 path is implemented but not yet covered by a four-player
   reference scenario.

## Published

- Repository: <https://github.com/omegakatana92/golden-axe-the-revenge-of-death-adder-Recomp>
- Release: <https://github.com/omegakatana92/golden-axe-the-revenge-of-death-adder-Recomp/releases/tag/v0.1.0>
- Assets: Windows x64 zip, Steam Deck AppImage, source zip, SHA256SUMS.
- The repository contains no ROM, BIOS, generated V60 C, decompiled sound C, CMake file or build tree.

## Not done

- Full-game playthrough, four-player session on physical hardware, and the sound-dispatcher fix remain.
- No release artifact was uploaded anywhere except the project's own GitHub release page.

## Rebuild commands

```powershell
powershell -ExecutionPolicy Bypass -File build_windows.ps1
```

```sh
./build_appimage.sh
```

Both scripts verify the ROM, regenerate the translated C, build, run the tests and package the release.
