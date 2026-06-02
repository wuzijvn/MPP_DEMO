#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUT_DIR="${OUT_DIR:-${PROJECT_DIR}/logs/run_default}"
"${PROJECT_DIR}/bin/07_enterprise_m2m_diagnostic_service" \
  --device="${DEVICE:-/dev/video0}" \
  --output-dir="${OUT_DIR}" \
  --inject="${INJECT:-none}" \
  --frames="${FRAMES:-12}" \
  --output-depth="${OUTPUT_DEPTH:-3}" \
  --capture-depth="${CAPTURE_DEPTH:-4}" \
  --min-decoded-frames="${MIN_DECODED_FRAMES:-6}" \
  --allowed-timeouts="${ALLOWED_TIMEOUTS:-0}" \
  --output-fourcc="${OUTPUT_FOURCC:-H264}" \
  --capture-fourcc="${CAPTURE_FOURCC:-NV12}" \
  --width="${WIDTH:-1280}" \
  --height="${HEIGHT:-720}" \
  ${EXTRA_ARGS:-}
