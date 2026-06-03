#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
"${STAGE_DIR}/bin/04_vm_vim2m_queue_loop" \
  --device="${DEVICE:-/dev/video0}" \
  --output="${OUTPUT:-RGBP}" \
  --capture="${CAPTURE:-RGBP}" \
  --width="${WIDTH:-640}" \
  --height="${HEIGHT:-480}" \
  --output-count="${OUTPUT_COUNT:-3}" \
  --capture-count="${CAPTURE_COUNT:-4}" \
  --loops="${LOOPS:-8}" \
  --timeout-ms="${TIMEOUT_MS:-1000}" \
  --verbose="${VERBOSE:-1}"
