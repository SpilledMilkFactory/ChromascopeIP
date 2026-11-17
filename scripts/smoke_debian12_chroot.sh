#!/usr/bin/env bash
# M11.2: real dependency resolution and CLI processing in a *fresh* Debian 12
# minbase chroot. Must run as root on Debian 12 AMD64 with debootstrap.
set -Eeuo pipefail

if [[ "$#" -ne 1 || ! -f "$1" ]]; then
  echo 'usage: sudo scripts/smoke_debian12_chroot.sh /path/to/ChromascopeIP-*-Debian12-amd64.deb' >&2
  exit 2
fi
pkg="$(realpath -- "$1")"
source /etc/os-release
if [[ "$ID" != debian || "$VERSION_ID" != 12 || "$(dpkg --print-architecture)" != amd64 ]]; then
  echo 'This clean-install gate requires Debian 12 AMD64.' >&2
  exit 2
fi
if [[ "$(id -u)" -ne 0 ]]; then
  echo 'Run this clean-install smoke with root permission for chroot/debootstrap.' >&2
  exit 2
fi
command -v debootstrap >/dev/null || { echo 'Install debootstrap before running.' >&2; exit 2; }
command -v chroot >/dev/null || { echo 'Missing chroot command.' >&2; exit 2; }

work="$(mktemp -d -t chromascope-deb12-smoke-XXXXXXXX)"
trap 'rm -rf -- "$work"' EXIT
root="$work/bookworm"
mkdir -p "$root"
debootstrap --variant=minbase --include=apt,ca-certificates \
  bookworm "$root" http://deb.debian.org/debian
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
# RUNPATH is not transitive: the Debian 12 package deliberately uses a
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
  echo '--- Installed private OpenCV libraries ---' >&2
  ls -la "$private" >&2 || :
  echo '--- Installed ELF dynamic search tags ---' >&2
  for object in "$root/usr/bin/chromascope-cli" "$root/usr/bin/chromascope-gui" \
                "$private"/libopencv_*.so.5.1.0; do
    [[ -f "$object" ]] || continue
    echo "Object: $object" >&2
    readelf -d "$object" | grep -E '\((NEEDED|RPATH|RUNPATH|SONAME)\)' >&2 || :
  done
}
for program in chromascope-cli chromascope-gui; do
  if ! env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin \
       chroot "$root" /usr/bin/ldd "/usr/bin/$program" \
       > "$work/$program.ldd" 2>&1; then
    cat "$work/$program.ldd" >&2
    show_loader_diagnostics
    echo "Dynamic-loader verification failed for $program" >&2
    exit 1
  fi
  if grep -F 'not found' "$work/$program.ldd"; then
    cat "$work/$program.ldd" >&2
    show_loader_diagnostics
    echo "Unresolved runtime library in $program" >&2
    exit 1
  fi
done
env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin \
  chroot "$root" /usr/bin/chromascope-cli --help > "$work/help.txt"
grep -Fq 'Usage: chromascope-cli' "$work/help.txt"
env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin \
  chroot "$root" /usr/bin/chromascope-cli --list-profiles > "$work/profiles.txt"
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
env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin \
  chroot "$root" /usr/bin/chromascope-cli \
    --input /tmp/smoke.bmp --profile grayscale --json - > "$work/result.json"
test -s "$work/result.json"
# Installation never implicitly activates a camera or downloads a model.
test ! -e "$root/usr/share/chromascopeip/models/object_detection_nanodet_2022nov.onnx"
printf 'PASS Debian 12 isolated clean-install: apt dependencies, two ELF loaders, CLI help/profiles, synthetic BMP processing.\n'
printf 'GUI window launch and real camera qualification remain manual M11.8 gates.\n'
