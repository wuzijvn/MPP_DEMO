#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
"${STAGE_DIR}/bin/04_gst_debug_log_capture" \
  --output-dir="${OUTPUT_DIR:-${STAGE_DIR}/logs/debug_demo}" \
  --frames="${FRAMES:-6}"
