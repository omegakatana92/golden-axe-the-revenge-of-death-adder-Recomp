# Windows packaging

`build_windows.ps1` builds and stages this folder. The release folder contains:

```
release/Windows/GoldenAxe2.exe
release/Windows/SDL2.dll
release/Windows/README.md
release/Windows/README-ROM.txt
release/Windows/LICENSE
release/Windows/THIRD_PARTY_NOTICES.md
release/GoldenAxe2-Windows-x64.zip
```

It intentionally contains no `ga2.zip`, EEPROM image, generated V60 C or decompiled sound C. The player
supplies their own MAME `ga2` set beside the executable or in `rom\`.

The executable is a native x86-64 Windows GUI program built with MSYS2 MinGW64; it has no console window
and links SDL2 dynamically (the DLL is beside it). It does not use Wine, MAME or any emulator process.
