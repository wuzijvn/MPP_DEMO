#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
LOG_DIR="${LOG_DIR:-${PROJ_DIR}/logs/run_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "${LOG_DIR}"

"${PROJ_DIR}/build.sh"

args=(
  --input="${INPUT:-${PROJ_DIR}/../samples/sample.mp4}"
  --decoder="${DECODER:-h264_rkmpp}"
  --max-frames="${MAX_FRAMES:-120}"
  --print-every="${PRINT_EVERY:-20}"
  --log-dir="${LOG_DIR}"
)

if [[ -n "${HW_TYPE:-}" ]]; then
  args+=(--hw-type="${HW_TYPE}")
fi

if [[ -n "${DEVICE:-}" ]]; then
  args+=(--device="${DEVICE}")
fi

"${PROJ_DIR}/bin/09_enterprise_ffmpeg_hwaccel_service" "${args[@]}"
