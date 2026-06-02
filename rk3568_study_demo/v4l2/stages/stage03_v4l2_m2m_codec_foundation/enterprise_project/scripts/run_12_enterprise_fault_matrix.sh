#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BASE_LOG_DIR="${1:-${PROJ_DIR}/logs/fault_matrix_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "${BASE_LOG_DIR}"

run_case() {
  local name="$1"
  shift
  local log_dir="${BASE_LOG_DIR}/${name}"
  mkdir -p "${log_dir}"

  echo "[fault-matrix] case=${name}"
  LOG_DIR="${log_dir}" "$@" \
    | tee "${log_dir}/console.log" || true
}

run_case normal "${PROJ_DIR}/scripts/run_12_enterprise_m2m_pipeline_service.sh"
REAL_SAMPLE="${REAL_SAMPLE:-${PROJ_DIR}/../samples/sample_720p_h264.annexb}"
if [[ -f "${REAL_SAMPLE}" ]]; then
  run_case real_annexb env INPUT_ANNEXB="${REAL_SAMPLE}" MAX_INPUT_CHUNKS=8 "${PROJ_DIR}/scripts/run_12_enterprise_m2m_pipeline_service.sh"
else
  echo "[fault-matrix] case=real_annexb skipped: sample not found (${REAL_SAMPLE})"
fi
run_case inject_timeout env INJECT_TIMEOUT=1 LOOPS=6 "${PROJ_DIR}/scripts/run_12_enterprise_m2m_pipeline_service.sh"
run_case inject_source_change env INJECT_SOURCE_CHANGE=1 LOOPS=8 "${PROJ_DIR}/scripts/run_12_enterprise_m2m_pipeline_service.sh"
run_case inject_dqbuf_eagain env INJECT_DQBUF_EAGAIN=1 LOOPS=8 "${PROJ_DIR}/scripts/run_12_enterprise_m2m_pipeline_service.sh"

echo "[fault-matrix] base_log_dir=${BASE_LOG_DIR}"
