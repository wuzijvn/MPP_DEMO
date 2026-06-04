#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT="${OUTPUT_ROOT:-${PROJECT_DIR}/logs/fault_matrix_$(date +%Y%m%d_%H%M%S)}"
BIN="${PROJECT_DIR}/bin/07_enterprise_gst_pipeline_service"

"${PROJECT_DIR}/build.sh" all >/dev/null
mkdir -p "${ROOT}"

run_case() {
  local name="$1"
  shift
  local out="${ROOT}/${name}"
  mkdir -p "${out}"
  echo "[case] ${name}"
  if "${BIN}" --output-dir="${out}" "$@" >"${out}/stdout.log" 2>&1; then
    echo "PASS ${name}" | tee "${out}/status.txt"
  else
    echo "FAIL ${name}" | tee "${out}/status.txt"
  fi
}

run_case normal_debug_caps \
  --mode=debug-caps --scenario=normal --frames=16 --min-caps-mentions=0

run_case caps_failure_expected \
  --mode=raw-basic --scenario=caps-failure --frames=8

run_case missing_element_expected \
  --mode=raw-basic --scenario=missing-element --frames=8

run_case slow_queue_observation \
  --mode=raw-basic --scenario=slow-queue --frames=20 --slow-us=3000 --queue-depth=3 --max-elapsed-ms=20000

run_case hardware_probe_rkmpp \
  --mode=hardware-candidate --scenario=hardware-probe --frames=8 --backend-element="${BACKEND_ELEMENT:-avdec_h264_rkmpp}" --require-backend

{
  echo "| case | status | gate_pass | failure_layer | elapsed_ms | reason |"
  echo "| --- | --- | --- | --- | --- | --- |"
  for d in "${ROOT}"/*; do
    [[ -d "${d}" ]] || continue
    name="$(basename "${d}")"
    status="$(cat "${d}/status.txt" 2>/dev/null || echo UNKNOWN)"
    if [[ -f "${d}/enterprise_metrics.json" ]]; then
      gate="$(grep -o '"gate_pass": [^,]*' "${d}/enterprise_metrics.json" | head -1 | sed 's/.*: //')"
      layer="$(grep -o '"failure_layer": "[^"]*"' "${d}/enterprise_metrics.json" | head -1 | sed 's/.*: "//;s/"$//')"
      elapsed="$(grep -o '"elapsed_ms": [0-9]*' "${d}/enterprise_metrics.json" | head -1 | sed 's/.*: //')"
      reason="$(grep -o '"gate_reason": "[^"]*"' "${d}/enterprise_metrics.json" | head -1 | sed 's/.*: "//;s/"$//')"
    else
      gate="missing"
      layer="missing"
      elapsed="missing"
      reason="metrics missing"
    fi
    echo "| ${name} | ${status} | ${gate} | ${layer} | ${elapsed} | ${reason} |"
  done
} | tee "${ROOT}/summary.md"

echo "fault_matrix_output_dir=${ROOT}"
