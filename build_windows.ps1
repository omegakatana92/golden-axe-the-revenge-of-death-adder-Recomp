# Build Golden Axe: The Revenge of Death Adder on Windows with MSYS2 MinGW64.
#
# Usage (from a normal PowerShell, not the MSYS2 shell):
#   powershell -ExecutionPolicy Bypass -File build_windows.ps1
#
# The ROM stays in rom\ga2.zip. The generated V60 C and decompiled sound C are written under
# build\generated and src\sound; they are derived from your ROM and must not be published.

param(
    [switch]$NoIterate,
    [int]$Frames = 3000
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;$env:PATH"
$python = "C:\Users\omega\AppData\Local\Python\pythoncore-3.14-64\python.exe"
if (-not (Test-Path $python)) { $python = "python" }
$build = Join-Path $root "build\mingw"
$ref = Join-Path $root "build\ref"
$bin = Join-Path $root "build\bin"
New-Item -ItemType Directory -Force $ref | Out-Null

function Require($path, $what) {
    if (-not (Test-Path $path)) { throw "$what not found: $path" }
}

Push-Location $root
try {
    Write-Host "== dependencies =="
    Require "C:\msys64\mingw64\bin\gcc.exe" "MinGW64 gcc"
    Require "C:\msys64\mingw64\bin\g++.exe" "MinGW64 g++"
    Require "C:\msys64\mingw64\bin\SDL2.dll" "SDL2 runtime"
    Require "C:\msys64\mingw64\include\SDL2\SDL.h" "SDL2 headers"
    $cmake = (Get-Command cmake -ErrorAction SilentlyContinue)
    if (-not $cmake) { throw "cmake not found" }
    $ninja = (Get-Command ninja -ErrorAction SilentlyContinue)
    if (-not $ninja) { throw "ninja not found" }

    Write-Host "== ROM set =="
    & $python tools\verify_rom.py
    if ($LASTEXITCODE -ne 0) { throw "ROM verification failed" }

    Write-Host "== configure =="
    & $python tools\write_cmake.py
    if ($LASTEXITCODE -ne 0) { throw "CMake project generation failed" }
    cmake -S . -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe `
        -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

    Write-Host "== reference tools =="
    cmake --build $build --target s32ref -j 8
    if ($LASTEXITCODE -ne 0) { throw "reference build failed" }

    if (-not (Test-Path "$ref\exec3000.txt")) {
        Write-Host "== reference trace (3000 frames) =="
        $p = Start-Process -FilePath "$bin\s32ref.exe" -ArgumentList @(
            "--timing", "cycles", "--rom", "rom", "--script", "tests\controls\play_long.txt",
            "--frames", "3000", "--trace", "build\ref\exec3000.txt", "--calls", "build\ref\called3000.txt",
            "--transfers", "build\ref\transfers3000.txt", "--video-hashes", "build\ref\ref_video3000.txt"
        ) -Wait -PassThru -NoNewWindow -RedirectStandardOutput "$ref\reference_out.txt" -RedirectStandardError "$ref\reference_err.txt"
        if ($p.ExitCode -ne 0) { throw "reference run failed" }
    }
    if (-not (Test-Path "$ref\exec300.txt")) {
        $p = Start-Process -FilePath "$bin\s32ref.exe" -ArgumentList @(
            "--timing", "cycles", "--rom", "rom", "--frames", "300",
            "--trace", "build\ref\exec300.txt", "--calls", "build\ref\called300.txt"
        ) -Wait -PassThru -NoNewWindow -RedirectStandardOutput "$ref\reference_boot.txt" -RedirectStandardError "$ref\reference_boot_err.txt"
        if ($p.ExitCode -ne 0) { throw "reference boot run failed" }
    }

    if (-not (Test-Path "$ref\calls_extra.txt")) { New-Item -ItemType File "$ref\calls_extra.txt" | Out-Null }
    $seed = @(Get-Content "tools\calls_extra.txt" | Where-Object { $_ -match '^[0-9A-Fa-f]{6}$' })
    $extra = @(Get-Content "$ref\calls_extra.txt" | Where-Object { $_ -match '^[0-9A-Fa-f]{6}$' })
    @($seed + $extra) | Select-Object -Unique | Set-Content "$ref\calls_extra.txt"

    Write-Host "== translate =="
    $traces = @("$ref\exec300.txt", "$ref\exec3000.txt")
    $calls = @("$ref\called300.txt", "$ref\called3000.txt")
    $callFiles = @()
    foreach ($file in $calls) { $callFiles += @(Get-Content $file | Where-Object { $_ -match '^[0-9A-Fa-f]{6}$' }) }
    $callFiles += @(Get-Content "$ref\calls_extra.txt" | Where-Object { $_ -match '^[0-9A-Fa-f]{6}$' })
    $callFiles | Select-Object -Unique | Set-Content "$ref\calls_all.txt"
    & $python tools\recompile.py rom --trace @traces --calls "$ref\calls_all.txt"
    if ($LASTEXITCODE -ne 0) { throw "translation failed" }

    if (-not $NoIterate) {
        Write-Host "== cover computed jump targets (up to 40 rounds) =="
        for ($i = 0; $i -lt 40; $i++) {
            cmake --build $build --target ga2_headless -j 8 | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "headless build failed" }
            & "$bin\ga2_headless.exe" --rom rom --script tests\controls\play_long.txt --frames $Frames > "$ref\iterate_out.txt" 2>&1
            $match = Select-String -Path "$ref\iterate_out.txt" -Pattern 'Jump from [0-9A-Fa-f]{6} to ([0-9A-Fa-f]{6}) has no translated target'
            if (-not $match) {
                $match = Select-String -Path "$ref\iterate_out.txt" -Pattern 'No translated code starts at ([0-9A-Fa-f]{6})'
            }
            if (-not $match) {
                Write-Host "   translated program is complete for the tested path"
                break
            }
            $address = $match.Matches[0].Groups[1].Value.ToUpper()
            Write-Host "   computed target $address"
            Add-Content "$ref\calls_extra.txt" $address
            $callFiles += $address
            $callFiles | Select-Object -Unique | Set-Content "$ref\calls_all.txt"
            & $python tools\recompile.py rom --trace @traces --calls "$ref\calls_all.txt" | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "translation failed at $address" }
        }
    }

    Write-Host "== sound program =="
    & $python tools\decompile_sound.py
    if ($LASTEXITCODE -ne 0) { throw "sound decompilation failed" }
    & $python tools\fix_sound_dispatch.py
    if ($LASTEXITCODE -ne 0) { throw "sound dispatch fix failed" }

    Write-Host "== game =="
    cmake -S . -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build $build --target GoldenAxe2 -j 8
    if ($LASTEXITCODE -ne 0) { throw "game build failed" }

    Write-Host "== headless verification (3000 frames) =="
    $p = Start-Process -FilePath "$bin\ga2_headless.exe" -ArgumentList @(
        "--rom", "rom", "--script", "tests\controls\play_long.txt", "--frames", "3000",
        "--video-hashes", "build\ref\native_video3000.txt"
    ) -WorkingDirectory $root -Wait -PassThru -NoNewWindow -RedirectStandardOutput "$ref\headless_verify.txt" -RedirectStandardError "$ref\headless_verify_err.txt"
    Get-Content "$ref\headless_verify.txt" | Select-Object -Last 8 | ForEach-Object { Write-Host "   $_" }
    if ($p.ExitCode -ne 0) { throw "headless run failed (exit $($p.ExitCode))" }
    if (Test-Path "$ref\ref_video3000.txt") {
        $a = Get-Content "$ref\ref_video3000.txt"
        $b = Get-Content "$ref\native_video3000.txt"
        $diff = 0
        for ($i = 0; $i -lt [Math]::Min($a.Count, $b.Count); $i++) { if ($a[$i] -ne $b[$i]) { $diff++ } }
        Write-Host "   video frames compared: $($a.Count), differences: $diff"
    }

    Write-Host "== Windows startup smoke test (600 frames, dummy SDL drivers) =="
    $smokeBat = Join-Path $root "build\smoke-test.bat"
    @"
@echo off
start /wait "" "$bin\GoldenAxe2.exe" --smoke-test --frames 600 --rom rom
exit /b %ERRORLEVEL%
"@ | Set-Content -Encoding ASCII $smokeBat
    cmd /c "`"$smokeBat`"" > "$ref\windows_smoke.txt" 2>&1
    $smokeStatus = $LASTEXITCODE
    if ($smokeStatus -ne 0) {
        Get-Content "$ref\windows_smoke.txt" | Select-Object -Last 10 | ForEach-Object { Write-Host "   $_" }
        throw "Windows smoke test failed (exit $smokeStatus)"
    }

    Write-Host "== release =="
    $release = Join-Path $root "release\Windows"
    New-Item -ItemType Directory -Force $release | Out-Null
    Copy-Item "$bin\GoldenAxe2.exe" $release -Force
    Copy-Item "C:\msys64\mingw64\bin\SDL2.dll" $release -Force
    foreach ($dll in "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll") {
        $source = Join-Path $bin $dll
        if (-not (Test-Path $source)) { $source = "C:\msys64\mingw64\bin\$dll" }
        if (Test-Path $source) { Copy-Item $source $release -Force }
    }
    Copy-Item "$root\README.md", "$root\LICENSE", "$root\CREDITS.md", "$root\THIRD_PARTY_NOTICES.md" $release -Force
    @"
Golden Axe: The Revenge of Death Adder (native Windows build)
=============================================================

Put your own MAME ga2.zip (World, Rev B, 19 files) beside GoldenAxe2.exe, or in a rom\ folder beside it.
No ROM or game data is included in this folder.

Run GoldenAxe2.exe. Controls are in README.md. The game writes ga2.eeprom beside the executable.
"@ | Set-Content "$release\README-ROM.txt"
    $zip = Join-Path $root "release\GoldenAxe2-Windows-x64.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Compress-Archive -Path "$release\*" -DestinationPath $zip -Force
    Write-Host ""
    Write-Host "Output: $release\GoldenAxe2.exe"
    Write-Host "Archive: $zip"

    Write-Host "== source archive (no ROM, no generated game code) =="
    $stage = Join-Path $root "build\source-stage\GoldenAxe2"
    if (Test-Path (Join-Path $root "build\source-stage")) { Remove-Item (Join-Path $root "build\source-stage") -Recurse -Force }
    New-Item -ItemType Directory -Force $stage | Out-Null
    $items = @("README.md", "LICENSE", "CREDITS.md", "ROM_ANALYSIS.md", "BUILD_STATUS.md",
        "COMPATIBILITY.md", "TEST_RESULTS.md", "THIRD_PARTY_NOTICES.md", "symbols.txt", "sound_symbols.txt",
        "build_windows.ps1", "build_appimage.sh",
        "LICENSE-arcade-decomp-BSD-3-Clause.txt", "LICENSE-MAME-BSD-3-Clause.txt",
        "LICENSE-ymfm-BSD-3-Clause.txt", "runtime", "ports", "docs", "packaging", "tests",
        "tools\core", "tools\reference", "tools\ga2_rom.py", "tools\game_profile.py",
        "tools\verify_rom.py", "tools\recompile.py", "tools\decompile.py", "tools\decompile_sound.py",
        "tools\disassemble.py", "tools\check_structure.py", "tools\coverage.py", "tools\noise_report.py",
        "tools\write_cmake.py", "tools\calls_extra.txt", "tools\traces", "tools\fix_sound_dispatch.py")
    foreach ($item in $items) {
        $source = Join-Path $root $item
        if (Test-Path $source) {
            $target = Join-Path $stage $item
            $parent = Split-Path -Parent $target
            New-Item -ItemType Directory -Force $parent | Out-Null
            Copy-Item $source $target -Recurse -Force
        }
    }
    Get-ChildItem $stage -Recurse -Directory -Filter "__pycache__" | Remove-Item -Recurse -Force
    $sourceZip = Join-Path $root "release\GoldenAxe2-Source.zip"
    if (Test-Path $sourceZip) { Remove-Item $sourceZip -Force }
    Compress-Archive -Path "$stage\*" -DestinationPath $sourceZip -Force
    Write-Host "Source:  $sourceZip"
}
finally {
    Pop-Location
}
exit 0
