#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUT_DIR="${OUT_DIR:-${PROJECT_DIR}/logs/run_default}"
args=(
  --mode="${MODE:-vm-vim2m}"
  --device="${DEVICE:-/dev/video0}"
  --output-dir="${OUT_DIR}"
  --inject="${INJECT:-none}"
  --frames="${FRAMES:-8}"
  --output-depth="${OUTPUT_DEPTH:-3}"
  --capture-depth="${CAPTURE_DEPTH:-4}"
  --min-decoded-frames="${MIN_DECODED_FRAMES:-4}"
  --allowed-timeouts="${ALLOWED_TIMEOUTS:-0}"
  --timeout-ms="${TIMEOUT_MS:-1000}"
  --output-fourcc="${OUTPUT_FOURCC:-RGBP}"
  --capture-fourcc="${CAPTURE_FOURCC:-RGBP}"
  --width="${WIDTH:-640}"
  --height="${HEIGHT:-480}"
  --decoder="${DECODER:-h264_rkmpp}"
)

if [[ -n "${INPUT:-}" ]]; then
  args+=(--input="${INPUT}")
fi

if [[ -n "${EXTRA_ARGS:-}" ]]; then
  # shellcheck disable=SC2206
  extra_args=( ${EXTRA_ARGS} )
  args+=("${extra_args[@]}")
fi

"${PROJECT_DIR}/bin/07_enterprise_m2m_diagnostic_service" "${args[@]}"
