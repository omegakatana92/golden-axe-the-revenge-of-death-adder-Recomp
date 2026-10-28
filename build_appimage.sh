#!/bin/sh
# Build Golden Axe: The Revenge of Death Adder for Linux x86-64 and package it as an AppImage.
#
# Run from the repository root in WSL/Linux:
#   ./build_appimage.sh
#
# Requirements: gcc, cmake, ninja, python3, SDL2 development files, curl (for appimagetool).
# The ROM must be in rom/ (or GA2_ROM must name its folder). Generated game C is kept out of git.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"
BUILD="$ROOT/build/linux"
BIN="$ROOT/build/bin"
APPDIR="$ROOT/build/AppDir"
TOOLS="$ROOT/build/tools"
ROM_DIR=${GA2_ROM:-$ROOT/rom}

say() { printf '\n== %s ==\n' "$*"; }

say "dependencies"
for tool in cmake ninja gcc g++ python3 pkg-config curl; do
    command -v "$tool" >/dev/null 2>&1 || { echo "missing $tool" >&2; exit 1; }
done
pkg-config --exists sdl2 || { echo "missing SDL2 development files (libsdl2-dev)" >&2; exit 1; }

say "ROM"
python3 tools/verify_rom.py "$ROM_DIR"

say "translated program"
if [ ! -f "$ROOT/build/generated/entries.c" ]; then
    python3 tools/recompile.py "$ROM_DIR" \
        --trace tools/traces/exec300.txt tools/traces/exec3000.txt \
        --calls tools/traces/called300.txt tools/traces/called3000.txt tools/calls_extra.txt
fi
if [ ! -f "$ROOT/src/sound/main.c" ]; then
    python3 tools/decompile_sound.py
fi
python3 tools/fix_sound_dispatch.py

say "configure and build"
python3 tools/write_cmake.py
cmake -S . -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DGA2_REFERENCE_TOOLS=ON -DGA2_SOUND_REFERENCE=ON
cmake --build "$BUILD" --target GoldenAxe2 ga2_headless -j "$(nproc)"

say "AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" \
         "$APPDIR/usr/share/applications" "$APPDIR/usr/share/icons/hicolor/256x256/apps" \
         "$APPDIR/usr/share/doc/goldenaxe2"
cp "$BIN/GoldenAxe2" "$APPDIR/usr/bin/GoldenAxe2"
cp "$BIN/ga2_headless" "$APPDIR/usr/bin/ga2_headless"
cp packaging/appimage/AppRun "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun"
cp packaging/appimage/goldenaxe2.desktop "$APPDIR/usr/share/applications/"
cp packaging/appimage/goldenaxe2.desktop "$APPDIR/"
cp packaging/appimage/goldenaxe2.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/"
cp packaging/appimage/goldenaxe2.png "$APPDIR/"
cp README.md LICENSE CREDITS.md THIRD_PARTY_NOTICES.md ROM_ANALYSIS.md BUILD_STATUS.md "$APPDIR/usr/share/doc/goldenaxe2/"

# SDL2 and the few libraries an AppImage may safely carry; libc and the graphics driver are the host's.
cp -L "$(pkg-config --variable=libdir sdl2)/libSDL2-2.0.so.0" "$APPDIR/usr/lib/" 2>/dev/null || \
    cp -L /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0 "$APPDIR/usr/lib/"

say "smoke test the staged application"
mkdir -p "$ROOT/build/appimage-test"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    "$APPDIR/usr/bin/GoldenAxe2" --smoke-test --frames 600 --rom "$ROM_DIR" \
    > "$ROOT/build/appimage-test/smoke.txt" 2>&1
status=$?
if [ "$status" -ne 0 ]; then
    echo "staged smoke test failed (status $status)" >&2
    cat "$ROOT/build/appimage-test/smoke.txt" >&2
    exit 1
fi
"$APPDIR/usr/bin/ga2_headless" --rom "$ROM_DIR" --frames 60 \
    > "$ROOT/build/appimage-test/headless.txt" 2>&1

say "appimagetool"
mkdir -p "$TOOLS"
if [ ! -x "$TOOLS/appimagetool" ]; then
    curl -L --fail -o "$TOOLS/appimagetool" \
        https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x "$TOOLS/appimagetool"
fi

say "package"
mkdir -p "$ROOT/release/SteamDeck"
OUT="$ROOT/release/SteamDeck/GoldenAxe2-SteamDeck-x86_64.AppImage"
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$TOOLS/appimagetool" "$APPDIR" "$OUT"
chmod +x "$OUT"

say "verify the AppImage"
mkdir -p "$ROOT/build/appimage-test/extract"
rm -rf "$ROOT/build/appimage-test/extract"/*
"$OUT" --appimage-extract >/dev/null 2>&1 || true
if [ -d squashfs-root ]; then
    rm -rf "$ROOT/build/appimage-test/extract/squashfs-root"
    mv squashfs-root "$ROOT/build/appimage-test/extract/"
fi
APPIMAGE_EXTRACT_AND_RUN=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    "$OUT" --smoke-test --frames 300 --rom "$ROM_DIR" \
    > "$ROOT/build/appimage-test/appimage.txt" 2>&1
status=$?
if [ "$status" -ne 0 ]; then
    echo "AppImage smoke test failed (status $status)" >&2
    cat "$ROOT/build/appimage-test/appimage.txt" >&2
    exit 1
fi
(cd "$ROOT/release/SteamDeck" && sha256sum "$(basename "$OUT")" > SHA256SUMS.txt)

say "done"
ls -lh "$OUT"
