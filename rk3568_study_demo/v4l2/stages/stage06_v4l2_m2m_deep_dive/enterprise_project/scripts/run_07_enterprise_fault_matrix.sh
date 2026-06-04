#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
STAMP="$(date -u +%Y%m%d_%H%M%S)"
BASE_OUT="${PROJECT_DIR}/logs/fault_matrix_${STAMP}"
mkdir -p "${BASE_OUT}"
BIN="${PROJECT_DIR}/bin/07_enterprise_m2m_diagnostic_service"
SUMMARY="${BASE_OUT}/summary.tsv"
printf "case\texit_code\tverdict\tmetrics\n" > "${SUMMARY}"

run_case() {
  local name="$1"
  shift
  local out="${BASE_OUT}/${name}"
  mkdir -p "${out}"
  set +e
  "${BIN}" --output-dir="${out}" "$@" > "${out}/stdout.log" 2>&1
  local rc=$?
  set -e
  local verdict="UNKNOWN"
  if [[ -f "${out}/enterprise_metrics.json" ]]; then
    verdict="$(sed -n 's/.*"verdict": "\([^"]*\)".*/\1/p' "${out}/enterprise_metrics.json" | head -n 1)"
  fi
  printf "%s\t%s\t%s\t%s\n" "${name}" "${rc}" "${verdict}" "${out}/enterprise_metrics.json" >> "${SUMMARY}"
}

common_vm=(
  --mode=vm-vim2m
  --device="${DEVICE:-/dev/video0}"
  --output-fourcc=RGBP
  --capture-fourcc=RGBP
  --width=640
  --height=480
)

run_case normal "${common_vm[@]}" --inject=none --frames=8 --min-decoded-frames=4
run_case timeout_recovered "${common_vm[@]}" --inject=timeout --allowed-timeouts=1 --frames=8 --min-decoded-frames=4
run_case bytesused_zero "${common_vm[@]}" --inject=bytesused_zero --frames=8 --min-decoded-frames=4
run_case unsupported_format "${common_vm[@]}" --inject=unsupported_format --min-decoded-frames=0
run_case source_change_recovered "${common_vm[@]}" --inject=source_change --frames=8 --min-decoded-frames=4
run_case source_change_no_reconfigure "${common_vm[@]}" --inject=source_change_no_reconfigure --frames=8 --min-decoded-frames=4
run_case rk_rkmpp_evidence --mode=rk-rkmpp --decoder="${DECODER:-h264_rkmpp}" --no-require-device

cat "${SUMMARY}"
echo "fault_matrix_dir=${BASE_OUT}"
