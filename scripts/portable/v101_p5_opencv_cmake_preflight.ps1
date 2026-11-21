# Run before the expensive FFmpeg build. Exercises Visual Studio's CMake
# compiler detection and nested try_compile using the SAME path conversion as
# the real OpenCV P5 bootstrap, without cloning or building dependencies.
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$ScratchRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not $IsWindows -or -not [Environment]::Is64BitOperatingSystem) {
    throw 'The P5 CMake path preflight requires Windows x64'
}
. (Join-Path $PSScriptRoot 'v101_p5_cmake_paths.ps1')
$moduleDir = Join-Path $PSScriptRoot 'cmake'
$hookDir = Join-Path (Split-Path -Parent $PSScriptRoot) 'opencv_windows_hooks'
foreach ($dir in @($moduleDir, $hookDir)) {
    if (-not (Test-Path -LiteralPath $dir -PathType Container)) {
        throw "Missing P5 CMake directory: $dir"
    }
}
$scratch = [IO.Path]::GetFullPath($ScratchRoot)
if (Test-Path -LiteralPath $scratch) { throw "P5 preflight directory already exists: $scratch" }
$source = Join-Path $scratch 'source'
$build = Join-Path $scratch 'build'
New-Item -ItemType Directory -Path $source -Force | Out-Null
@'
cmake_minimum_required(VERSION 3.20)
project(ChromascopeP5PathPreflight LANGUAGES C CXX)
foreach(_var IN ITEMS CMAKE_MODULE_PATH OPENCV_CMAKE_HOOKS_DIR
                      CHROMASCOPE_PRIVATE_FFMPEG_PREFIX CMAKE_INSTALL_PREFIX)
  string(FIND "${${_var}}" "\\" _backslash)
  if(NOT _backslash EQUAL -1)
    message(FATAL_ERROR "Unnormalized CMake path in ${_var}: ${${_var}}")
  endif()
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/p5probe.cpp" "int main() { return 0; }\n")
try_compile(_p5_ok "${CMAKE_CURRENT_BINARY_DIR}/nested-probe"
            "${CMAKE_CURRENT_BINARY_DIR}/p5probe.cpp"
            OUTPUT_VARIABLE _p5_output)
if(NOT _p5_ok)
  message(FATAL_ERROR "Nested P5 CMake compiler probe failed: ${_p5_output}")
endif()
message(STATUS "P5 Windows CMake paths and nested MSVC try_compile: PASS")
'@ | Set-Content -LiteralPath (Join-Path $source 'CMakeLists.txt') -Encoding utf8
$modulePath = ConvertTo-CMakePath $moduleDir
$hookPath = ConvertTo-CMakePath $hookDir
$ffmpegPath = ConvertTo-CMakePath (Join-Path $scratch 'private-ffmpeg')
$installPath = ConvertTo-CMakePath (Join-Path $scratch 'opencv-install')
& cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64 `
    "-DCMAKE_MODULE_PATH=$modulePath" `
    "-DOPENCV_CMAKE_HOOKS_DIR=$hookPath" `
    "-DCHROMASCOPE_PRIVATE_FFMPEG_PREFIX=$ffmpegPath" `
    "-DCMAKE_INSTALL_PREFIX=$installPath"
if ($LASTEXITCODE -ne 0) { throw 'P5 Windows CMake path / nested try_compile preflight failed' }
Write-Host 'PASS: P5 Windows OpenCV CMake path propagation preflight'
