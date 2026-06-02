#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
"${STAGE_DIR}/bin/03_mmap_buffer_lifecycle_sim" \
  --output-bufs="${OUTPUT_BUFS:-3}" \
  --capture-bufs="${CAPTURE_BUFS:-4}" \
  --coded-size="${CODED_SIZE:-262144}" \
  --frame-size="${FRAME_SIZE:-1382400}"
