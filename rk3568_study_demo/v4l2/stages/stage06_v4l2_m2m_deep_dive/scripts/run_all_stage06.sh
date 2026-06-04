#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
STAMP="$(date -u +%Y%m%d_%H%M%S)"
LOG_DIR="${STAGE_DIR}/logs/run_all_stage06_${STAMP}"
mkdir -p "${LOG_DIR}"

"${STAGE_DIR}/build.sh" all-with-enterprise > "${LOG_DIR}/build.log" 2>&1
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

run_one 01_vm_vim2m_device_discovery "${SCRIPT_DIR}/run_01_vm_vim2m_device_discovery.sh"
run_one 02_vm_vim2m_format_negotiation "${SCRIPT_DIR}/run_02_vm_vim2m_format_negotiation.sh"
run_one 03_vm_vim2m_mmap_lifecycle "${SCRIPT_DIR}/run_03_vm_vim2m_mmap_lifecycle.sh"
run_one 04_vm_vim2m_queue_loop "${SCRIPT_DIR}/run_04_vm_vim2m_queue_loop.sh"
run_one 05_vm_vim2m_fault_injection "${SCRIPT_DIR}/run_05_vm_vim2m_fault_injection.sh"
run_one 07_enterprise_vm_vim2m_real_queue "${STAGE_DIR}/enterprise_project/scripts/run_07_enterprise_m2m_diagnostic_service.sh"

if [[ "${RUN_RK_TRACK:-0}" == "1" ]]; then
  run_one 06_rk_board_rkmpp_hardware_path "${SCRIPT_DIR}/run_06_rk_board_rkmpp_hardware_path.sh"
fi

echo "[run_all] logs: ${LOG_DIR}"
