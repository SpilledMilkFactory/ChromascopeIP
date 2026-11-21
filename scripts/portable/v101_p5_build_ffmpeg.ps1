# P5 Windows engineering candidate only: pinned, restricted MSVC-built FFmpeg.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BuildRoot,
    [Parameter(Mandatory=$true)][string]$InstallPrefix
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if (-not $IsWindows -or -not [Environment]::Is64BitOperatingSystem) {
    throw 'P5 private FFmpeg build requires Windows x64'
}
$root=[IO.Path]::GetFullPath($BuildRoot)
$prefix=[IO.Path]::GetFullPath($InstallPrefix)
if ((Test-Path -LiteralPath $root) -or (Test-Path -LiteralPath $prefix)) {
    throw 'P5 private FFmpeg build roots must be new, empty paths'
}
$source=Join-Path $root 'source'
$build=Join-Path $root 'build'
New-Item -ItemType Directory -Path $root -Force | Out-Null
git -c core.autocrlf=false clone --depth 1 --branch n8.1.3 -- https://github.com/FFmpeg/FFmpeg.git $source
if ($LASTEXITCODE -ne 0) { throw 'FFmpeg clone failed' }
# Fail early if the Windows checkout is incomplete. The MSYS2 wrapper checks
# these paths again after translating them to its POSIX namespace.
foreach ($required in @('Makefile', 'configure', 'libavcodec/jpegxl_parser.c')) {
    if (-not (Test-Path -LiteralPath (Join-Path $source $required) -PathType Leaf)) {
        throw "Pinned FFmpeg source is incomplete: $required"
    }
}
python scripts/portable/v101_p5_private_ffmpeg.py prepare --source $source
if ($LASTEXITCODE -ne 0) { throw 'FFmpeg security backport failed' }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=(& $vswhere -latest -products '*' -version '[17.0,18.0)' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
if ($LASTEXITCODE -ne 0 -or -not $vs) { throw 'MSVC 2022 is unavailable' }
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvars64.bat'
$msysBash='C:\msys64\usr\bin\bash.exe'
if (-not (Test-Path -LiteralPath $msysBash -PathType Leaf)) {
    throw 'GitHub Windows runner lacks MSYS2 bash; cannot reproduce private FFmpeg'
}
$script=Join-Path $PSScriptRoot 'v101_p5_ffmpeg_msvc.sh'
$cmd=Join-Path $root 'run-msvc-ffmpeg.cmd'
# The build directories are created by the workflow and never contain untrusted
# command-line arguments. Keep the shell outside the repository source tree.
@(
    '@echo off',
    'setlocal',
    "call `"$vcvars`" || exit /b 1",
    'set "MSYS2_PATH_TYPE=inherit"',
    "`"$msysBash`" --noprofile --norc `"$script`" `"$source`" `"$build`" `"$prefix`"",
    'exit /b %ERRORLEVEL%'
) | Set-Content -LiteralPath $cmd -Encoding ascii
& cmd.exe /d /c $cmd
if ($LASTEXITCODE -ne 0) { throw "Private MSVC FFmpeg build failed: $LASTEXITCODE" }
python scripts/portable/v101_p5_private_ffmpeg.py verify --source $source `
    --components (Join-Path $build 'config_components.h') --prefix $prefix
if ($LASTEXITCODE -ne 0) { throw 'Private FFmpeg components/DLL verification failed' }
$legal=Join-Path $prefix 'share/chromascopeip'
New-Item -ItemType Directory -Path $legal -Force | Out-Null
$lgpl=Join-Path $source 'COPYING.LGPLv2.1'
if (-not (Test-Path -LiteralPath $lgpl -PathType Leaf)) {
    throw 'Reviewed FFmpeg source has no LGPL 2.1 license'
}
Copy-Item -LiteralPath $lgpl -Destination (Join-Path $legal 'COPYING.LGPLv2.1')
$stamp=Join-Path $prefix '.chromascope-ffmpeg-windows-p5.txt'
@(
    'ffmpeg=8.1.3',
    'commit=1041abdc962f4cc4f394aa8de9dc5236c0c3b9e7',
    'target=windows-x64-msvc2022',
    'shared=ON',
    'network=OFF',
    'hardware_acceleration=OFF',
    'jpegxl_parser=OFF',
    'h264_parser=ON',
    'security_components=PASS',
    'release_approved=NO'
) | Set-Content -LiteralPath $stamp -Encoding utf8
Write-Host "PASS: private Windows FFmpeg candidate at $prefix (NOT release approved)"
