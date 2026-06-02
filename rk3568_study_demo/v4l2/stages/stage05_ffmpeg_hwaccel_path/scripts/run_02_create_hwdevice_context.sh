#!/usr/bin/env bash
set -euo pipefail
HW_TYPE="${HW_TYPE:-drm}"
args=(--hw-type="${HW_TYPE}")

if [[ -n "${DEVICE:-}" ]]; then
  args+=(--device="${DEVICE}")
elif [[ "${HW_TYPE}" == "drm" ]]; then
  args+=(--device=/dev/dri/renderD128)
fi

./bin/02_create_hwdevice_context "${args[@]}"
