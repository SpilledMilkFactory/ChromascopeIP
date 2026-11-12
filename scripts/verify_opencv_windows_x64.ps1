# M4.1: Validate the installed OpenCV MSVC static package with a standalone
# real consumer, including videoio and the native Media Foundation backend.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$InstallPrefix,
    [Parameter(Mandatory = $true)][string]$ScratchRoot,
    [ValidateRange(1, 32)][int]$Jobs = 2,
    [switch]$SkipStampCheck
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$expectedCommit = 'e4f78fbcf1a773fbed69e14500e8b734963eb585'
if (-not $IsWindows -or -not [Environment]::Is64BitOperatingSystem) {
    throw 'OpenCV consumer verification requires Windows x64 and MSVC 2022.'
}

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments = @())
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with code $LASTEXITCODE ($($Arguments -join ' '))"
    }
}

$install = [IO.Path]::GetFullPath($InstallPrefix)
$scratch = [IO.Path]::GetFullPath($ScratchRoot)
$opencvDir = Join-Path $install 'x64\vc17\staticlib'
foreach ($name in @('OpenCVConfig.cmake', 'OpenCVConfig-version.cmake',
                    'OpenCVModules.cmake', 'OpenCVModules-release.cmake')) {
    $path = Join-Path $opencvDir $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "OpenCV installed package is incomplete: $path"
    }
}
if (-not $SkipStampCheck) {
    $stamp = Join-Path $install '.chromascope-opencv-windows-build.txt'
    if (-not (Test-Path -LiteralPath $stamp -PathType Leaf)) {
        throw 'OpenCV installed provenance marker is missing.'
    }
    $fields = @(Get-Content -LiteralPath $stamp)
    foreach ($expected in @(
        "commit=$expectedCommit", 'opencv=5.1.0', 'architecture=x64',
        'toolchain=Visual Studio 17 2022', 'configuration=Release',
        'shared=OFF', 'static_crt=OFF', 'modules=core,imgproc,imgcodecs,videoio,dnn', 'video_backend=msmf',
        'protobuf=ON', 'dnn_backend=opencv_cpu', 'windows_pthread_hook=ON', 'consumer_probe=PASS'
    )) {
        if ($fields -notcontains $expected) {
            throw "OpenCV provenance mismatch: $expected"
        }
    }
}
if (Test-Path -LiteralPath $scratch) {
    throw "Consumer probe directory already exists: $scratch"
}
$build = Join-Path $scratch 'build'

# The independent consumer lives in source control (also registered as a
# CTest). It validates the installed package, PNG and MJPEG/AVI IO, and MSMF.
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\tests\quality\opencv_videoio_consumer'))
if (-not (Test-Path -LiteralPath (Join-Path $source 'CMakeLists.txt') -PathType Leaf)) {
    throw 'M4.1 independent OpenCV consumer project is missing.'
}

Invoke-Checked cmake @('-S', $source, '-B', $build,
    '-G', 'Visual Studio 17 2022', '-A', 'x64',
    "-DOpenCV_DIR=$opencvDir",
    '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL')
Invoke-Checked cmake @('--build', $build, '--config', 'Release', '--parallel', "$Jobs")
$exe = Join-Path $build 'Release\opencv_videoio_consumer.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "OpenCV consumer executable missing: $exe"
}
Invoke-Checked -Program $exe -Arguments @()
$dnnExe = Join-Path $build 'Release\opencv_dnn_consumer.exe'
if (-not (Test-Path -LiteralPath $dnnExe -PathType Leaf)) {
    throw "OpenCV DNN consumer executable missing: $dnnExe"
}
Invoke-Checked -Program $dnnExe -Arguments @()
Write-Host "PASS: OpenCV 5.1.0 MSVC static videoio package configures, links and runs: $opencvDir"
