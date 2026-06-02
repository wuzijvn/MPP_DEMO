#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
"${STAGE_DIR}/bin/04_qbuf_dqbuf_poll_timeout_sim" \
  --frames="${FRAMES:-8}" \
  --timeout-at="${TIMEOUT_AT:-5}" \
  --recover="${RECOVER:-1}" \
  --output-depth="${OUTPUT_DEPTH:-3}" \
  --capture-depth="${CAPTURE_DEPTH:-4}"
