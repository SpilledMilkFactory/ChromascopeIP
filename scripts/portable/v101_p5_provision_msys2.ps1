# P5-only hosted-runner setup. MSYS2 utilities execute FFmpeg build recipes;
# the VS 2022 vcvars64 environment still supplies the C compiler and linker.
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not $IsWindows -or -not [Environment]::Is64BitOperatingSystem) {
    throw 'P5 MSYS2 provisioning requires the hosted Windows x64 runner'
}

# Use the same GitHub runner MSYS2 installation as v101_p5_build_ffmpeg.ps1.
# Do not accept Git-for-Windows, MinGW or ambient PATH aliases of these tools.
$msysBin = 'C:\msys64\usr\bin'
$pacman = Join-Path $msysBin 'pacman.exe'
$bash = Join-Path $msysBin 'bash.exe'
foreach ($required in @($pacman, $bash)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Missing hosted MSYS2 prerequisite: $required"
    }
}

# FFmpeg n8.1.3 documents MSYS2's regular GNU Make for its POSIX recipes.
# A stock GitHub runner may instead expose /mingw64/bin/make; that does NOT
# establish that /usr/bin/make exists. Explicitly install the MSYS packages.
# --needed leaves already installed packages untouched. Do not perform a
# system-wide partial -Sy update or replace MSVC with a MinGW compiler.
& $pacman -S --needed --noconfirm make gawk sed diffutils
if ($LASTEXITCODE -ne 0) { throw "MSYS2 build-tool installation failed: $LASTEXITCODE" }

foreach ($name in @('make.exe', 'awk.exe', 'sed.exe', 'sh.exe', 'cygpath.exe', 'diff.exe')) {
    $path = Join-Path $msysBin $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required POSIX build tool not installed at $path"
    }
}
# Record actual resolved package versions; the hosted image is mutable.
& $pacman -Q make gawk sed diffutils
if ($LASTEXITCODE -ne 0) { throw 'MSYS2 installed-package provenance check failed' }
Write-Host 'PASS: hosted MSYS2 POSIX build tools provisioned; FFmpeg compiler remains MSVC'
