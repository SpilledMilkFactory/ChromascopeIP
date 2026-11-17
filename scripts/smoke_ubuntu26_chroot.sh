#!/usr/bin/env bash
# M11.5: real dependency resolution and CLI processing in a *fresh* Ubuntu 26.04
# minbase chroot. Must run as root on Ubuntu 26.04 AMD64 with debootstrap.
set -Eeuo pipefail

if [[ "$#" -ne 1 || ! -f "$1" ]]; then
  echo 'usage: sudo scripts/smoke_ubuntu26_chroot.sh /path/to/ChromascopeIP-*-Ubuntu26.04-amd64.deb' >&2
  exit 2
fi
pkg="$(realpath -- "$1")"
source /etc/os-release
if [[ "$ID" != ubuntu || "$VERSION_ID" != 26.04 || "$(dpkg --print-architecture)" != amd64 ]]; then
  echo 'This clean-install gate requires Ubuntu 26.04 AMD64.' >&2
  exit 2
fi
if [[ "$(id -u)" -ne 0 ]]; then
  echo 'Run this clean-install smoke with root permission for chroot/debootstrap.' >&2
  exit 2
fi
command -v debootstrap >/dev/null || { echo 'Install debootstrap before running.' >&2; exit 2; }
command -v chroot >/dev/null || { echo 'Missing chroot command.' >&2; exit 2; }

work="$(mktemp -d -t chromascope-ubuntu2604-smoke-XXXXXXXX)"
trap 'rm -rf -- "$work"' EXIT
root="$work/resolute"
mkdir -p "$root"
debootstrap --variant=minbase --components=main,universe --include=apt,ca-certificates \
  resolute "$root" http://archive.ubuntu.com/ubuntu/
# The fresh minbase must resolve Ubuntu universe dependencies such as GLFW;
# Ubuntu archive pockets are explicit and independent of the CI host apt state.
cat > "$root/etc/apt/sources.list" <<'APT_SOURCES'
deb http://archive.ubuntu.com/ubuntu/ resolute main universe
deb http://archive.ubuntu.com/ubuntu/ resolute-updates main universe
deb http://security.ubuntu.com/ubuntu/ resolute-security main universe
APT_SOURCES
cp -- "$pkg" "$root/tmp/chromascopeip.deb"
# No host toolchain, OpenCV build prefix, or exported loader variables are
# present inside the new root. apt must resolve all external dependencies.
env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin DEBIAN_FRONTEND=noninteractive \
  chroot "$root" apt-get update
env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin DEBIAN_FRONTEND=noninteractive \
  chroot "$root" apt-get install -y --no-install-recommends /tmp/chromascopeip.deb
env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin \
  chroot "$root" dpkg-query -W chromascopeip
# Verify the actual files and links in the *installed* chroot, before ldd.
# RUNPATH is not transitive: the Ubuntu 26.04 package deliberately uses a
# private $ORIGIN DT_RPATH on the frontends and OpenCV modules.
private="$root/usr/lib/x86_64-linux-gnu/chromascopeip"
test -d "$private"
for module in core imgproc imgcodecs videoio dnn; do
  link="$private/libopencv_${module}.so.501"
  test -L "$link" && test -f "$link" || {
    echo "Missing installed private OpenCV SONAME link: $link" >&2
    exit 1
  }
done
show_loader_diagnostics() {
  echo '--- Installed private OpenCV libraries (including SONAME links) ---' >&2
  ls -la "$private" >&2 || :
  echo '--- Installed executables and program interpreters ---' >&2
  for object in "$root/usr/bin/chromascope-cli" "$root/usr/bin/chromascope-gui" \
                "$private"/libopencv_*.so.5.1.0; do
    [[ -f "$object" ]] || continue
    echo "Object: ${object#"$root"}" >&2
    readelf -l "$object" | grep 'Requesting program interpreter' >&2 || :
    readelf -d "$object" | grep -E '\((NEEDED|RPATH|RUNPATH|SONAME)\)' >&2 || :
  done
}
# A successful dpkg installation and readelf RPATH tag do not prove that the
# *real* Resolute loader resolves the installed libraries. Inspect the exact
# interpreter named by each executable and require videoio to resolve from
# the package-private directory before allowing the startup smoke to proceed.
cli_interpreter=''
for program in chromascope-cli chromascope-gui; do
  executable="$root/usr/bin/$program"
  interpreter="$(LC_ALL=C readelf -l "$executable" | sed -n 's/.*Requesting program interpreter: \([^]]*\)].*/\1/p')"
  if [[ -z "$interpreter" || "$interpreter" != /* || "$interpreter" == *'..'* || ! -e "$root$interpreter" ]]; then
    echo "Invalid or unavailable Resolute ELF interpreter for $program: $interpreter" >&2
    show_loader_diagnostics
    exit 1
  fi
  if [[ "$program" == chromascope-cli ]]; then
    cli_interpreter="$interpreter"
  fi
  if ! env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin \
       chroot "$root" "$interpreter" --list "/usr/bin/$program" \
       > "$work/$program.loader" 2>&1; then
    cat "$work/$program.loader" >&2
    show_loader_diagnostics
    echo "Real loader --list failed for $program" >&2
    exit 1
  fi
  echo "--- Resolute loader resolution: $program ---"
  cat "$work/$program.loader"
  # A generic loader success is not enough: confirm that OpenCV's critical
  # module is resolved from the exact package-private location, not from an
  # unexpected system installation. This is deliberately not LD_LIBRARY_PATH.
  # ld-linux prints the *un-normalized* $ORIGIN search path (e.g.,
  # /usr/bin/../lib/...). Compare real target files, not textual paths.
  # Only a package-private file is acceptable; a system OpenCV match fails.
  expected_videoio="$(realpath -e -- "$private/libopencv_videoio.so.501")"
  videoio_ok=false
  while read -r needed arrow location _rest; do
    if [[ "$needed" == 'libopencv_videoio.so.501' && "$arrow" == '=>' && "$location" == /* ]]; then
      if resolved="$(realpath -e -- "$root$location")" && \
         [[ "$resolved" == "$expected_videoio" ]]; then
        videoio_ok=true
      fi
    fi
  done < "$work/$program.loader"
  if [[ "$videoio_ok" != true ]]; then
    show_loader_diagnostics
    echo "Private OpenCV videoio did not resolve for $program" >&2
    exit 1
  fi
  if ! env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin \
       chroot "$root" /usr/bin/ldd "/usr/bin/$program" \
       > "$work/$program.ldd" 2>&1; then
    cat "$work/$program.ldd" >&2
    show_loader_diagnostics
    echo "ldd verification failed for $program" >&2
    exit 1
  fi
  if grep -F 'not found' "$work/$program.ldd" || \
     grep -F 'error while loading shared libraries' "$work/$program.ldd"; then
    cat "$work/$program.ldd" >&2
    show_loader_diagnostics
    echo "Unresolved runtime library reported by ldd for $program" >&2
    exit 1
  fi
done
# A debootstrap minbase chroot intentionally has no mounted /proc. glibc
# resolves the main executable's $ORIGIN using /proc/self/exe during ordinary
# kernel exec; in a proc-less chroot, the same executable can fail to locate
# its correct private RPATH even though --list succeeds (Debian bug #787227).
# Mounting procfs here would require SYS_ADMIN in GitHub's container. Instead,
# test BOTH: (1) native kernel exec from an unpacked .deb with the host's real
# procfs (no LD_LIBRARY_PATH), and (2) actual Resolute runtime execution against
# APT-installed dependencies through the executable's own Resolute interpreter.
# Neither check substitutes third-party binaries from the development prefix.
if [[ ! -r /proc/self/exe ]]; then
  echo 'Host /proc/self/exe is required for native $ORIGIN smoke verification.' >&2
  exit 1
fi
native_stage="$work/native-proc-stage"
mkdir -p "$native_stage"
dpkg-deb --extract "$pkg" "$native_stage"
native_cli="$native_stage/usr/bin/chromascope-cli"
native_private="$native_stage/usr/lib/x86_64-linux-gnu/chromascopeip"
test -x "$native_cli"
test -f "$native_private/libopencv_videoio.so.501"
if ! env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin \
     ldd "$native_cli" > "$work/native.ldd" 2>&1; then
  cat "$work/native.ldd" >&2
  echo 'Native packaged CLI ldd failed with host /proc mounted.' >&2
  exit 1
fi
native_videoio="$(realpath -e -- "$native_private/libopencv_videoio.so.501")"
native_videoio_ok=false
while read -r needed arrow location _rest; do
  if [[ "$needed" == 'libopencv_videoio.so.501' && "$arrow" == '=>' && "$location" == /* ]]; then
    if resolved="$(realpath -e -- "$location")" && [[ "$resolved" == "$native_videoio" ]]; then
      native_videoio_ok=true
    fi
  fi
done < "$work/native.ldd"
if [[ "$native_videoio_ok" != true ]]; then
  cat "$work/native.ldd" >&2
  echo 'Native packaged CLI did not resolve its own private videoio library.' >&2
  exit 1
fi
if ! env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin \
     "$native_cli" --help > "$work/native-help.txt" 2> "$work/native-help.err"; then
  cat "$work/native-help.err" >&2
  echo 'Native packaged CLI startup failed with host /proc mounted.' >&2
  exit 1
fi
grep -Fq 'Usage: chromascope-cli' "$work/native-help.txt"
echo 'PASS: ordinary kernel exec of extracted .deb with /proc, private OpenCV videoio verified.'

# In the clean Ubuntu 26.04 chroot, invoke the SAME installed binary via its
# actual Resolute ELF interpreter. This supplies its pathname without needing
# /proc/self/exe and exercises the installed APT dependencies and CLI logic.
# Do NOT report this as an ordinary kernel exec inside a proc-less chroot.
launch_resolute_cli() {
  env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin \
    chroot "$root" "$cli_interpreter" /usr/bin/chromascope-cli "$@"
}
if ! launch_resolute_cli --help \
     > "$work/help.txt" 2> "$work/help.err"; then
  cat "$work/help.err" >&2
  echo '--- Resolute CLI interpreter startup diagnostics (LD_DEBUG=libs) ---' >&2
  env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin LD_DEBUG=libs \
    chroot "$root" "$cli_interpreter" /usr/bin/chromascope-cli --help \
    > /dev/null 2> "$work/cli.lddebug" || :
  grep -E 'find library=libopencv_|search path=|trying file=.*(opencv|chromascopeip)' \
    "$work/cli.lddebug" | tail -n 120 >&2 || :
  echo '--- Last 80 loader messages ---' >&2
  tail -n 80 "$work/cli.lddebug" >&2 || :
  show_loader_diagnostics
  echo 'Installed CLI startup failed; Ubuntu 26.04 package is NOT qualified.' >&2
  exit 1
fi
grep -Fq 'Usage: chromascope-cli' "$work/help.txt"
launch_resolute_cli --list-profiles > "$work/profiles.txt"
# Create a wholly synthetic 24-bit BMP in the throwaway root. No photographic
# fixture/media distribution and no camera or inference-model access.
python3 - "$root/tmp/smoke.bmp" <<'PY'
import struct
import sys
from pathlib import Path
w, h = 320, 240
stride = ((w * 3 + 3) // 4) * 4
pixels = (bytes((20, 120, 210)) * w + b'\0' * (stride - w * 3)) * h
header = (b'BM' + struct.pack('<IHHI', 54 + len(pixels), 0, 0, 54)
          + struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, len(pixels), 2835, 2835, 0, 0))
Path(sys.argv[1]).write_bytes(header + pixels)
PY
launch_resolute_cli \
  --input /tmp/smoke.bmp --profile grayscale --json - > "$work/result.json"
test -s "$work/result.json"
# Installation never implicitly activates a camera or downloads a model.
test ! -e "$root/usr/share/chromascopeip/models/object_detection_nanodet_2022nov.onnx"
printf 'PASS Ubuntu 26.04 clean-install: apt dependencies, two Resolute ELF loaders, interpreter-driven CLI help/profiles/BMP, and native packaged CLI startup with host procfs.\n'
printf 'GUI window launch and real camera qualification remain manual M11.8 gates.\n'
