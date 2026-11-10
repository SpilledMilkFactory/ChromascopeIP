# M1.6: Validate the *installed* OpenCV MSVC static package with an actual
# consumer CMake project. The current core/CLI only needs still-image modules.
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
        'shared=OFF', 'static_crt=OFF', 'modules=core,imgproc,imgcodecs',
        'protobuf=OFF', 'windows_pthread_hook=ON', 'consumer_probe=PASS'
    )) {
        if ($fields -notcontains $expected) {
            throw "OpenCV provenance mismatch: $expected"
        }
    }
}
if (Test-Path -LiteralPath $scratch) {
    throw "Consumer probe directory already exists: $scratch"
}
$source = Join-Path $scratch 'source'
$build = Join-Path $scratch 'build'
New-Item -ItemType Directory -Path $source -Force | Out-Null

# This project is intentionally independent of ChromascopeIP. It must
# configure with the *installed* OpenCVConfig.cmake and link all three modules.
$cmakelists = @'
cmake_minimum_required(VERSION 3.24)
project(chromascope_opencv_consumer LANGUAGES CXX)
find_package(OpenCV 5.1.0 EXACT CONFIG REQUIRED COMPONENTS core imgproc imgcodecs)
add_executable(opencv_consumer main.cpp)
target_include_directories(opencv_consumer PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(opencv_consumer PRIVATE ${OpenCV_LIBS})
target_compile_features(opencv_consumer PRIVATE cxx_std_20)
'@
$cpp = @'
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <vector>
int main() {
    cv::Mat input(24, 24, CV_8UC3, cv::Scalar(0, 0, 255));
    cv::Mat gray;
    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    std::vector<unsigned char> bytes;
    if (gray.empty() || !cv::imencode(".png", input, bytes)) return 1;
    const cv::Mat decoded = cv::imdecode(bytes, cv::IMREAD_COLOR);
    if (decoded.empty() || decoded.rows != input.rows || decoded.cols != input.cols) return 2;
    return 0;
}
'@
Set-Content -LiteralPath (Join-Path $source 'CMakeLists.txt') -Value $cmakelists -Encoding utf8
Set-Content -LiteralPath (Join-Path $source 'main.cpp') -Value $cpp -Encoding utf8

Invoke-Checked cmake @('-S', $source, '-B', $build,
    '-G', 'Visual Studio 17 2022', '-A', 'x64',
    "-DOpenCV_DIR=$opencvDir",
    '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL')
Invoke-Checked cmake @('--build', $build, '--config', 'Release', '--parallel', "$Jobs")
$exe = Join-Path $build 'Release\opencv_consumer.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "OpenCV consumer executable missing: $exe"
}
Invoke-Checked -Program $exe -Arguments @()
Write-Host "PASS: OpenCV 5.1.0 MSVC static package configures, links and runs: $opencvDir"
