#!/usr/bin/env bash
set -Eeuo pipefail

# Native Ubuntu host wrapper for a Debian 12 build. Source is mounted read-only;
# all generated data remains in one operator-owned output directory.
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"
output="${CHROMASCOPE_P2_OUTPUT:-${HOME}/ChromascopeIP-v101-P2}"
sample="${CHROMASCOPE_P2_SAMPLE:-${HOME}/ChromascopeIP-M125-Parser-AB/private-media/sample.ts}"
jobs="${CHROMASCOPE_P2_JOBS:-2}"
engine="${CHROMASCOPE_P2_CONTAINER_ENGINE:-}"
if [[ -z "${engine}" ]]; then
    if command -v podman >/dev/null 2>&1; then engine=podman
    elif command -v docker >/dev/null 2>&1; then engine=docker
    else echo 'Install Docker or Podman first (Debian 12 build is required).' >&2; exit 2
    fi
fi
if [[ "${engine}" != docker && "${engine}" != podman ]]; then
    echo 'CHROMASCOPE_P2_CONTAINER_ENGINE must be docker or podman' >&2
    exit 2
fi
if ! command -v "${engine}" >/dev/null 2>&1; then
    echo "Container engine not found: ${engine}" >&2; exit 2
fi
if [[ ! "${jobs}" =~ ^[0-9]+$ ]] || (( 10#${jobs} < 1 || 10#${jobs} > 16 )); then
    echo 'CHROMASCOPE_P2_JOBS must be 1..16' >&2; exit 2
fi
if [[ ! -f "${sample}" || -L "${sample}" ]]; then
    echo "Missing private 276-frame sample or symlinked sample: ${sample}" >&2
    exit 2
fi
if [[ -L "${output}" ]]; then
    echo 'Symlink output directory refused' >&2; exit 2
fi
mkdir -p "${output}"
output="$(cd "${output}" && pwd -P)"
sample="$(realpath "${sample}")"
image='chromascope-v101-p2-debian12:local'
"${engine}" build -t "${image}" -f "${repo}/scripts/portable/Dockerfile.debian12-p2" "${repo}/scripts/portable"
image_id="$("${engine}" image inspect --format '{{.Id}}' "${image}")"
if [[ "${image_id}" =~ ^[a-f0-9]{64}$ ]]; then image_id="sha256:${image_id}"; fi
[[ "${image_id}" =~ ^sha256:[a-f0-9]{64}$ ]] || { echo 'Container image identifier missing' >&2; exit 2; }

"${engine}" run --rm --init --cap-drop=ALL --security-opt=no-new-privileges \
    --user "$(id -u):$(id -g)" \
    --mount "type=bind,source=${repo},target=/project,readonly" \
    --mount "type=bind,source=${output},target=/out" \
    --mount "type=bind,source=${sample},target=/private-media/sample.ts,readonly" \
    --env HOME=/out \
    --env "CHROMASCOPE_P2_IMAGE_ID=${image_id}" \
    "${image}" python3 /project/scripts/portable/v101_p2_build.py \
        --out /out --sample /private-media/sample.ts --jobs "${jobs}"
