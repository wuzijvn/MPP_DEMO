#!/usr/bin/env bash
set -euo pipefail
args=(
  --input="${INPUT:-./samples/sample.mp4}"
  --decoder="${DECODER:-h264_rkmpp}"
  --max-frames="${MAX_FRAMES:-16}"
)

if [[ -n "${HW_TYPE:-}" ]]; then
  args+=(--hw-type="${HW_TYPE}")
fi

if [[ -n "${DEVICE:-}" ]]; then
  args+=(--device="${DEVICE}")
fi

./bin/04_hwframe_vs_swframe "${args[@]}"
