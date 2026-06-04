#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
LOG_ROOT="${STAGE_DIR}/logs/run_all_stage07_$(date +%Y%m%d_%H%M%S)"
mkdir -p "${LOG_ROOT}"

"${STAGE_DIR}/build.sh" all >"${LOG_ROOT}/build.log" 2>&1

status=0
for script in \
  run_01_gst_environment_probe.sh \
  run_02_caps_negotiation_raw_video.sh \
  run_03_queue_backpressure_latency.sh \
  run_04_gst_debug_log_capture.sh \
  run_05_link_failure_fault_injection.sh \
  run_06_ffmpeg_gstreamer_compare.sh \
  run_07_hardware_backend_probe.sh; do
  name="${script%.sh}"
  echo "[run] ${name}"
  if OUTPUT_DIR="${LOG_ROOT}/debug_demo" "${SCRIPT_DIR}/${script}" >"${LOG_ROOT}/${name}.log" 2>&1; then
    echo "  PASS ${name}"
  else
    echo "  FAIL ${name}"
    status=1
  fi
done

echo "log_root=${LOG_ROOT}"
exit "${status}"
