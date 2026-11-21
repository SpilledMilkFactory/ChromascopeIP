# CMake stores certain -D paths in generated CMakeLists.txt files during
# compiler ABI detection (try_compile). Unescaped Windows \ path separators
# are not valid inside CMake quoted strings (e.g. D:\a => invalid escape).
# Use this one helper for the P5 preflight and the actual OpenCV bootstrap.
function ConvertTo-CMakePath {
    param([Parameter(Mandatory = $true)][string]$NativePath)
    if ([string]::IsNullOrWhiteSpace($NativePath)) {
        throw 'Cannot convert an empty path for CMake'
    }
    $fullPath = [IO.Path]::GetFullPath($NativePath)
    return $fullPath.Replace('\', '/')
}
