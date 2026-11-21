#!/usr/bin/env bash
# P5 fail-fast proof that the selected GNU Make uses a POSIX shell and does
# not corrupt FFmpeg's MSVC dependency generator's awk/sed backslashes.
# This runs the real tools; it never compiles FFmpeg or changes its sources.
set -euo pipefail
if [[ $# != 1 || ! -x "$1" ]]; then
    echo 'Usage: v101_p5_msys_make_preflight.sh <absolute GNU Make executable>' >&2
    exit 64
fi
make_bin="$1"
case "$make_bin" in /*) ;; *) echo 'Make executable must be absolute' >&2; exit 64 ;; esac
version="$("$make_bin" --version | head -n 1)"
case "$version" in 'GNU Make '*) ;; *) echo "Wrong Make implementation: $version" >&2; exit 1 ;; esac
if [[ "$(uname -s)" == MSYS_NT-* ]]; then
    if [[ "$make_bin" != /usr/bin/make && "$make_bin" != /usr/bin/make.exe ]]; then
        echo "Windows-native Make is unsupported for FFmpeg/MSVC: $make_bin" >&2
        exit 1
    fi
fi
probe_dir="$(mktemp -d)"
trap 'rm -rf -- "$probe_dir"' EXIT
# The AWK rule intentionally mirrors the MSVC dependency conversion in
# FFmpeg's pinned configure. GNU Make must keep both backslashes in /\/.
# The sed rule catches analogous quoting damage without invoking a compiler.
cat > "$probe_dir/Makefile" <<'MAKEFILE'
.PHONY: all
all:
	@printf '%s\n' 'Note: including file: C:\headers\video.h' | awk '/including/ { sub(/^.*file: */, ""); gsub(/\\/, "/"); if (!match($$0, / /)) print "probe.o:", $$0 }' > "$(OUT)/awk.d"
	@printf '%s\n' 'C:\headers\video.h' | sed 's|\\|/|g' > "$(OUT)/sed.txt"
MAKEFILE
"$make_bin" -r -f "$probe_dir/Makefile" "SHELL=/usr/bin/sh" "OUT=$probe_dir" all
if [[ "$(cat "$probe_dir/awk.d")" != 'probe.o: C:/headers/video.h' ||
      "$(cat "$probe_dir/sed.txt")" != 'C:/headers/video.h' ]]; then
    echo 'Make/awk/sed quoting preflight failed: non-POSIX tool or shell' >&2
    exit 1
fi
printf 'PASS P5 POSIX GNU Make + awk/sed quoting: %s\n' "$make_bin"
