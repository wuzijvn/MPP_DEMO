#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BASE="${1:-${PROJ_DIR}/logs/fault_matrix_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "${BASE}"
LOG_DIR="${BASE}/normal" "${PROJ_DIR}/scripts/run_09_enterprise_ffmpeg_pipeline_service.sh" | tee "${BASE}/normal_console.log" || true
INJECT_SEND_FAIL=1 LOG_DIR="${BASE}/inject_send_fail" "${PROJ_DIR}/scripts/run_09_enterprise_ffmpeg_pipeline_service.sh" | tee "${BASE}/inject_send_fail_console.log" || true
INJECT_RECEIVE_FAIL=1 LOG_DIR="${BASE}/inject_receive_fail" "${PROJ_DIR}/scripts/run_09_enterprise_ffmpeg_pipeline_service.sh" | tee "${BASE}/inject_receive_fail_console.log" || true
echo "[fault-matrix] base=${BASE}"
