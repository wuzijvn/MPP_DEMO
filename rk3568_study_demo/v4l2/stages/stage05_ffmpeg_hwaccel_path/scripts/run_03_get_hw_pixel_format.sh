#!/usr/bin/env bash
set -euo pipefail
args=(--decoder="${DECODER:-h264_rkmpp}")

if [[ -n "${HW_TYPE:-}" ]]; then
  args+=(--hw-type="${HW_TYPE}")
fi

./bin/03_get_hw_pixel_format "${args[@]}"
