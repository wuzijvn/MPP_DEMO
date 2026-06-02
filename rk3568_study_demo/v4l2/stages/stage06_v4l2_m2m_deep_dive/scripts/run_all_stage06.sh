#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
STAMP="$(date -u +%Y%m%d_%H%M%S)"
LOG_DIR="${STAGE_DIR}/logs/run_all_stage06_${STAMP}"
mkdir -p "${LOG_DIR}"

"${STAGE_DIR}/build.sh" all > "${LOG_DIR}/build.log" 2>&1
"${SCRIPT_DIR}/collect_env.sh" > "${LOG_DIR}/collect_env.log" 2>&1 || true

run_one() {
  local name="$1"
  local script="$2"
  echo "[run] ${name}"
  if "${script}" > "${LOG_DIR}/${name}.log" 2>&1; then
    echo "  PASS ${name}"
  else
    echo "  FAIL ${name} (see ${LOG_DIR}/${name}.log)"
    return 1
  fi
}

run_one 01_decoder_ioctl_sequence_map "${SCRIPT_DIR}/run_01_decoder_ioctl_sequence_map.sh"
SIMULATE="${SIMULATE:-1}" run_one 02_format_negotiation_probe "${SCRIPT_DIR}/run_02_format_negotiation_probe.sh"
run_one 03_mmap_buffer_lifecycle_sim "${SCRIPT_DIR}/run_03_mmap_buffer_lifecycle_sim.sh"
run_one 04_qbuf_dqbuf_poll_timeout_sim "${SCRIPT_DIR}/run_04_qbuf_dqbuf_poll_timeout_sim.sh"
run_one 05_source_change_eos_drain_sim "${SCRIPT_DIR}/run_05_source_change_eos_drain_sim.sh"
run_one 06_timeout_debug_report_template "${SCRIPT_DIR}/run_06_timeout_debug_report_template.sh"

echo "[run_all] logs: ${LOG_DIR}"
