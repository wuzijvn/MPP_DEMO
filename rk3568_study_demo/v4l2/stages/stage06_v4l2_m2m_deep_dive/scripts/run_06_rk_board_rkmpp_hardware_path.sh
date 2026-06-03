#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUT_DIR="${OUT_DIR:-${STAGE_DIR}/logs/rk_rkmpp_probe}"
"${STAGE_DIR}/bin/06_rk_board_rkmpp_hardware_path" \
  --output-dir="${OUT_DIR}" \
  --input="${INPUT:-}" \
  --decoder="${DECODER:-h264_rkmpp}" \
  --require-rkmpp="${REQUIRE_RKMPP:-0}"
