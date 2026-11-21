# Build only the restricted HTTPS transport library, not the curl executable.
# This is intentionally isolated from the accepted private FFmpeg/OpenCV build.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BuildRoot,
    [Parameter(Mandatory = $true)][string]$InstallPrefix
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$commit = '3163ec065643ba20709f190111f42c3e1fe95273'
$tag = 'curl-8_23_0'
$source = Join-Path $BuildRoot 'source'
$build = Join-Path $BuildRoot 'build'
if (Test-Path -LiteralPath $BuildRoot) {
    throw 'Refusing to reuse an existing private curl build directory'
}
New-Item -ItemType Directory -Path $BuildRoot -Force | Out-Null
& git clone --depth 1 --branch $tag -- https://github.com/curl/curl.git $source
if ($LASTEXITCODE -ne 0) { throw 'Private curl source checkout failed' }
$head = (& git -C $source rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -ne $commit) {
    throw "Private curl source commit mismatch: $head"
}
# Do not use OpenSSL/MinGW or ship standalone curl.exe. Restrict compiled
# protocols and optional third-party dependencies to the reviewed HTTPS path.
& cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64 `
    "-DCMAKE_INSTALL_PREFIX=$InstallPrefix" `
    '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>DLL' `
    '-DBUILD_SHARED_LIBS=OFF' '-DBUILD_STATIC_LIBS=ON' `
    '-DBUILD_CURL_EXE=OFF' '-DBUILD_TESTING=OFF' `
    '-DBUILD_EXAMPLES=OFF' '-DBUILD_LIBCURL_DOCS=OFF' `
    '-DBUILD_MISC_DOCS=OFF' '-DCURL_USE_SCHANNEL=ON' `
    '-DCURL_USE_OPENSSL=OFF' '-DCURL_USE_LIBPSL=OFF' `
    '-DCURL_USE_LIBSSH2=OFF' '-DUSE_NGHTTP2=OFF' '-DUSE_NGTCP2=OFF' `
    '-DCURL_ZLIB=OFF' '-DCURL_BROTLI=OFF' '-DCURL_ZSTD=OFF' `
    '-DUSE_LIBIDN2=OFF' '-DCURL_CA_NATIVE=ON' `
    '-DCURL_DISABLE_CA_SEARCH=ON' `
    '-DCURL_DISABLE_DICT=ON' '-DCURL_DISABLE_FILE=ON' `
    '-DCURL_DISABLE_FTP=ON' '-DCURL_DISABLE_GOPHER=ON' `
    '-DCURL_DISABLE_IMAP=ON' '-DCURL_DISABLE_IPFS=ON' `
    '-DCURL_DISABLE_LDAP=ON' '-DCURL_DISABLE_MQTT=ON' `
    '-DCURL_DISABLE_POP3=ON' '-DCURL_DISABLE_RTSP=ON' `
    '-DCURL_DISABLE_SMTP=ON' '-DCURL_DISABLE_TELNET=ON' `
    '-DCURL_DISABLE_TFTP=ON' '-DCURL_DISABLE_WEBSOCKETS=ON' `
    '-DCURL_ENABLE_EXPORT_TARGET=ON' '-DENABLE_UNICODE=ON'
if ($LASTEXITCODE -ne 0) { throw 'Private curl/MSVC configuration failed' }
$cache = Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt') -ErrorAction Stop
foreach ($required in @(
    'CURL_USE_SCHANNEL:BOOL=ON', 'CURL_USE_OPENSSL:BOOL=OFF',
    'BUILD_SHARED_LIBS:BOOL=OFF', 'BUILD_STATIC_LIBS:BOOL=ON',
    'BUILD_CURL_EXE:BOOL=OFF', 'CURL_CA_NATIVE:BOOL=ON',
    'CURL_DISABLE_FTP:BOOL=ON', 'CURL_DISABLE_FILE:BOOL=ON',
    'CURL_DISABLE_LDAP:BOOL=ON', 'CURL_DISABLE_WEBSOCKETS:BOOL=ON',
    'USE_NGHTTP2:BOOL=OFF', 'CURL_USE_LIBPSL:BOOL=OFF')) {
    if ($cache -notcontains $required) { throw "Private curl configuration mismatch: $required" }
}
& cmake --build $build --config Release --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Private curl/MSVC compilation failed' }
& cmake --install $build --config Release
if ($LASTEXITCODE -ne 0) { throw 'Private curl installation failed' }
$config = Join-Path $InstallPrefix 'lib/cmake/CURL/CURLConfig.cmake'
$header = Join-Path $InstallPrefix 'include/curl/curl.h'
$license = Join-Path $source 'COPYING'
foreach ($path in @($config, $header, $license)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Private curl output missing: $path"
    }
}
$lib = @(Get-ChildItem -LiteralPath (Join-Path $InstallPrefix 'lib') -File -Filter '*.lib' |
    Where-Object { $_.Name -match '^(?:libcurl|libcurl_a|curl)\.lib$' })
if ($lib.Count -ne 1) { throw 'Expected exactly one private MSVC static curl library' }
if (Get-ChildItem -LiteralPath $InstallPrefix -Recurse -File -Filter '*.dll') {
    throw 'Private curl unexpectedly installed a dynamically linked DLL'
}
$legal = Join-Path $InstallPrefix 'share/chromascopeip'
New-Item -ItemType Directory -Path $legal -Force | Out-Null
Copy-Item -LiteralPath $license -Destination (Join-Path $legal 'curl-COPYING')
@(
    'curl=8.23.0'
    "commit=$commit"
    'target=windows-x64-msvc2022'
    'tls=schannel'
    'linkage=static'
    'protocols=http,https'
) | Set-Content -LiteralPath (Join-Path $InstallPrefix '.chromascope-curl-windows-p5.txt') -Encoding ascii
Write-Host 'PASS: pinned private MSVC curl / Schannel static HTTPS transport'
