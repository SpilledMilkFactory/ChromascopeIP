# M1.6: Build the pinned OpenCV 5.1.0 source for Windows x64 / MSVC 2022.
# OpenCV static libraries use the dynamic MSVC runtime (/MD), matching the app.
# Usage: pwsh -File scripts/bootstrap_opencv_windows_x64.ps1 `
#   -InstallPrefix C:\opencv-5.1.0 -BuildRoot C:\opencv-build -Jobs 2
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$InstallPrefix,
    [Parameter(Mandatory = $true)][string]$BuildRoot,
    [ValidateRange(1, 32)][int]$Jobs = 2,
    [string]$P5PrivateFFmpegPrefix = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$expectedCommit = 'e4f78fbcf1a773fbed69e14500e8b734963eb585'

# CMake re-serializes these paths into compiler TryCompile CMakeLists files.
# Always pass CMake-style forward slashes, not native Windows backslashes.
. (Join-Path $PSScriptRoot 'portable/v101_p5_cmake_paths.ps1')

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

Invoke-Native git @('-c', 'core.autocrlf=false', 'clone', '--depth', '1', '--branch', '5.1.0',
    '--', 'https://github.com/opencv/opencv.git', $source)
$actualCommit = (& git -C $source rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -ne $expectedCommit) {
    throw "Unexpected OpenCV source commit: $actualCommit (expected $expectedCommit)"
}

# Use OpenCV's native post-dependency-detection hook to remove the invalid
# POSIX pthread import from the MSVC static package. Keep Windows concurrency
# enabled. Do not patch the immutable upstream source checkout.
$hookDir = Join-Path $PSScriptRoot 'opencv_windows_hooks'
$hook = Join-Path $hookDir 'POST_DETECT_DEPENDENCIES.cmake'
if (-not (Test-Path -LiteralPath $hook -PathType Leaf)) {
    throw "Missing validated OpenCV Windows dependency hook: $hook"
}

# Only the P5 engineering build uses the security-reviewed parser-enabled
# FFmpeg backend. Historical Windows builds keep their original MSMF policy.
$p5Args = @('-DWITH_FFMPEG=OFF')
if ($P5PrivateFFmpegPrefix) {
    $ffRoot = [IO.Path]::GetFullPath($P5PrivateFFmpegPrefix)
    if (-not (Test-Path -LiteralPath (Join-Path $ffRoot '.chromascope-ffmpeg-windows-p5.txt') -PathType Leaf)) {
        throw 'P5 requires the same-job, qualified private FFmpeg prefix'
    }
    python (Join-Path $PSScriptRoot 'portable/v101_p5_prepare_opencv.py') $source
    if ($LASTEXITCODE -ne 0) { throw 'Private OpenCV HLS security source transformation failed' }
    $modules = ConvertTo-CMakePath (Join-Path $PSScriptRoot 'portable/cmake')
    $p5Args = @(
        '-DWITH_FFMPEG=ON',
        '-DOPENCV_FFMPEG_SKIP_DOWNLOAD=ON',
        '-DOPENCV_FFMPEG_ENABLE_LIBAVDEVICE=OFF',
        '-DOPENCV_FFMPEG_USE_FIND_PACKAGE=ChromascopePrivateFFmpeg',
        "-DCHROMASCOPE_PRIVATE_FFMPEG_PREFIX=$(ConvertTo-CMakePath $ffRoot)",
        "-DCMAKE_MODULE_PATH=$modules"
    )
}

Invoke-Native cmake ( @(
    '-S', $source, '-B', $build,
    '-G', 'Visual Studio 17 2022', '-A', 'x64',
    "-DCMAKE_INSTALL_PREFIX=$(ConvertTo-CMakePath $install)",
    "-DOPENCV_CMAKE_HOOKS_DIR=$(ConvertTo-CMakePath $hookDir)",
    '-DBUILD_LIST=core,imgproc,imgcodecs,videoio,dnn',
    '-DBUILD_SHARED_LIBS=OFF',
    '-DBUILD_WITH_STATIC_CRT=OFF',
    # M5.2: DNN's ONNX parser needs Protobuf. Use the internal pinned
    # OpenCV dependency build rather than a floating external package.
    '-DWITH_PROTOBUF=ON', '-DBUILD_PROTOBUF=ON',
    '-DBUILD_TESTS=OFF', '-DBUILD_PERF_TESTS=OFF',
    '-DBUILD_EXAMPLES=OFF', '-DBUILD_DOCS=OFF',
    '-DBUILD_opencv_apps=OFF', '-DBUILD_JAVA=OFF',
    '-DBUILD_opencv_python2=OFF', '-DBUILD_opencv_python3=OFF',
    '-DWITH_IPP=OFF', '-DWITH_OPENCL=OFF',
    '-DWITH_GSTREAMER=OFF',
    '-DWITH_MSMF=ON', '-DWITH_MSMF_DXVA=OFF', '-DWITH_DSHOW=ON',
    '-DWITH_OPENEXR=OFF', '-DWITH_CUDA=OFF'
) + $p5Args )
Invoke-Native cmake @('--build', $build, '--config', 'Release', '--parallel', "$Jobs")
Invoke-Native cmake @('--install', $build, '--config', 'Release')

$configs = @(Get-ChildItem -LiteralPath $install -Recurse -File -Filter 'OpenCVConfig.cmake')
if ($configs.Count -eq 0) { throw 'OpenCVConfig.cmake was not installed.' }
# Verify the installed CMake package using a separate MSVC consumer before
# declaring the installation reusable. This detects broken imported targets
# (including missing third-party archives) and codec link/runtime problems.
$verifier = Join-Path $PSScriptRoot 'verify_opencv_windows_x64.ps1'
& $verifier -InstallPrefix $install -ScratchRoot (Join-Path $root 'consumer-probe') -SkipStampCheck -Jobs $Jobs `
    -P5PrivateFFmpegPrefix $P5PrivateFFmpegPrefix

# Write the provenance marker only after install + consumer verification pass.
$metadata = @(
    "commit=$expectedCommit",
    'opencv=5.1.0',
    'architecture=x64',
    'toolchain=Visual Studio 17 2022',
    'configuration=Release',
    'shared=OFF',
    'static_crt=OFF',
    'modules=core,imgproc,imgcodecs,videoio,dnn',
    $(if ($P5PrivateFFmpegPrefix) {'video_backend=msmf+ffmpeg-private'} else {'video_backend=msmf'}),
    $(if ($P5PrivateFFmpegPrefix) {'private_hls_capture=65017:1'} else {'private_hls_capture=OFF'}),
    'protobuf=ON',
    'dnn_backend=opencv_cpu',
    'windows_pthread_hook=ON',
    'consumer_probe=PASS'
) -join "`n"
Set-Content -LiteralPath $stamp -Value $metadata -Encoding utf8
Write-Host "Installed OpenCV 5.1.0 ($expectedCommit) at $install"
Write-Host "Configuration(s): $($configs.FullName -join ', ')"
