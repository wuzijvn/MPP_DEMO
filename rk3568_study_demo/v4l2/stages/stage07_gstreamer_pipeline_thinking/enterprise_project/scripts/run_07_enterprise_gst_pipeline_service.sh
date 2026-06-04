#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
STAGE_DIR="$(cd "${PROJECT_DIR}/.." && pwd)"
OUT_DIR="${OUTPUT_DIR:-${PROJECT_DIR}/logs/default_run}"

"${PROJECT_DIR}/build.sh" all >/dev/null
mkdir -p "${OUT_DIR}"

"${PROJECT_DIR}/bin/07_enterprise_gst_pipeline_service" \
  --mode="${MODE:-debug-caps}" \
  --scenario="${SCENARIO:-normal}" \
  --output-dir="${OUT_DIR}" \
  --backend-element="${BACKEND_ELEMENT:-avdec_h264_rkmpp}" \
  --frames="${FRAMES:-24}" \
  --width="${WIDTH:-320}" \
  --height="${HEIGHT:-240}" \
  --queue-depth="${QUEUE_DEPTH:-4}" \
  --slow-us="${SLOW_US:-0}" \
  --min-caps-mentions="${MIN_CAPS_MENTIONS:-0}" \
  --max-elapsed-ms="${MAX_ELAPSED_MS:-20000}" \
  ${REQUIRE_BACKEND:+--require-backend}

echo "enterprise_output_dir=${OUT_DIR}"
echo "stage_dir=${STAGE_DIR}"
