# M1.6: Build the pinned OpenCV 5.1.0 source for Windows x64 / MSVC 2022.
# OpenCV static libraries use the dynamic MSVC runtime (/MD), matching the app.
# Usage: pwsh -File scripts/bootstrap_opencv_windows_x64.ps1 `
#   -InstallPrefix C:\opencv-5.1.0 -BuildRoot C:\opencv-build -Jobs 2
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$InstallPrefix,
    [Parameter(Mandatory = $true)][string]$BuildRoot,
    [ValidateRange(1, 32)][int]$Jobs = 2
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$expectedCommit = 'e4f78fbcf1a773fbed69e14500e8b734963eb585'

function Invoke-Native {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE (args: $($Arguments -join ' '))"
    }
}

if (-not $IsWindows) { throw 'This bootstrap requires Windows and MSVC 2022.' }
if (-not [Environment]::Is64BitOperatingSystem) { throw 'Windows x64 is required.' }
$install = [IO.Path]::GetFullPath($InstallPrefix)
$root = [IO.Path]::GetFullPath($BuildRoot)
$stamp = Join-Path $install '.chromascope-opencv-windows-build.txt'

# Fail closed on an incomplete/stale install: never silently reuse different flags.
if (Test-Path -LiteralPath $install) {
    throw "Install directory already exists: $install. Remove it explicitly before bootstrapping."
}
if (Test-Path -LiteralPath $root) {
    throw "Build root already exists: $root. Remove it explicitly before bootstrapping."
}
New-Item -ItemType Directory -Path $root -Force | Out-Null
$source = Join-Path $root 'opencv'
$build = Join-Path $root 'build'

Invoke-Native git @('clone', '--depth', '1', '--branch', '5.1.0',
    '--', 'https://github.com/opencv/opencv.git', $source)
$actualCommit = (& git -C $source rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -ne $expectedCommit) {
    throw "Unexpected OpenCV source commit: $actualCommit (expected $expectedCommit)"
}

Invoke-Native cmake @(
    '-S', $source, '-B', $build,
    '-G', 'Visual Studio 17 2022', '-A', 'x64',
    "-DCMAKE_INSTALL_PREFIX=$install",
    '-DBUILD_LIST=core,imgproc,imgcodecs',
    '-DBUILD_SHARED_LIBS=OFF',
    '-DBUILD_WITH_STATIC_CRT=OFF',
    '-DBUILD_TESTS=OFF', '-DBUILD_PERF_TESTS=OFF',
    '-DBUILD_EXAMPLES=OFF', '-DBUILD_DOCS=OFF',
    '-DBUILD_opencv_apps=OFF', '-DBUILD_JAVA=OFF',
    '-DBUILD_opencv_python2=OFF', '-DBUILD_opencv_python3=OFF',
    '-DWITH_IPP=OFF', '-DWITH_OPENCL=OFF',
    '-DWITH_OPENEXR=OFF', '-DWITH_CUDA=OFF'
)
Invoke-Native cmake @('--build', $build, '--config', 'Release', '--parallel', "$Jobs")
Invoke-Native cmake @('--install', $build, '--config', 'Release')

$configs = @(Get-ChildItem -LiteralPath $install -Recurse -File -Filter 'OpenCVConfig.cmake')
if ($configs.Count -eq 0) { throw 'OpenCVConfig.cmake was not installed.' }
# Write a marker only when the entire source build and installation pass.
$metadata = @(
    "commit=$expectedCommit",
    'opencv=5.1.0',
    'architecture=x64',
    'toolchain=Visual Studio 17 2022',
    'configuration=Release',
    'shared=OFF',
    'static_crt=OFF',
    'modules=core,imgproc,imgcodecs'
) -join "`n"
Set-Content -LiteralPath $stamp -Value $metadata -Encoding utf8
Write-Host "Installed OpenCV 5.1.0 ($expectedCommit) at $install"
Write-Host "Configuration(s): $($configs.FullName -join ', ')"
