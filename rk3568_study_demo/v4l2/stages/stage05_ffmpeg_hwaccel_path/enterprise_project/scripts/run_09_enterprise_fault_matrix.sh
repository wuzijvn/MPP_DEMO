#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BASE_DIR="${BASE_DIR:-${PROJ_DIR}/logs/fault_matrix_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "${BASE_DIR}"

"${PROJ_DIR}/build.sh"

run_case() {
  local name="$1"
  shift
  local dir="${BASE_DIR}/${name}"
  mkdir -p "${dir}"
  local args=(
    --input="${INPUT:-${PROJ_DIR}/../samples/sample.mp4}"
    --decoder="${DECODER:-h264_rkmpp}"
    --max-frames="${MAX_FRAMES:-80}"
    --print-every="${PRINT_EVERY:-20}"
    --log-dir="${dir}"
  )

  if [[ -n "${HW_TYPE:-}" ]]; then
    args+=(--hw-type="${HW_TYPE}")
  fi

  if [[ -n "${DEVICE:-}" ]]; then
    args+=(--device="${DEVICE}")
  fi

  set +e
  "${PROJ_DIR}/bin/09_enterprise_ffmpeg_hwaccel_service" "${args[@]}" "$@" > "${dir}/stdout.log" 2>&1
  local rc=$?
  set -e
  echo "${name},rc=${rc}" | tee -a "${BASE_DIR}/summary.csv"
}

echo "case,exit_code" > "${BASE_DIR}/summary.csv"
run_case normal
run_case inject_device_fail --inject-device-create-fail
run_case inject_force_sw --inject-force-sw-fallback
run_case inject_transfer_fail --inject-transfer-fail
run_case inject_missing_hwfmt --inject-missing-hwfmt

echo "[fault_matrix] output=${BASE_DIR}"
