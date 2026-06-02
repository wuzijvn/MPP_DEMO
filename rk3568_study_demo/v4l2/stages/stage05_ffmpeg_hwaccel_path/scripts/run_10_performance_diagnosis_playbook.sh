#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/bin/04_hwframe_vs_swframe"
PLAYBOOK_BIN="${ROOT_DIR}/bin/10_performance_diagnosis_playbook"
INPUT="${INPUT:-${ROOT_DIR}/samples/sample.mp4}"
MAX_FRAMES="${MAX_FRAMES:-120}"
LOOPS="${LOOPS:-5}"
LOG_DIR="${LOG_DIR:-${ROOT_DIR}/logs/10_performance_diagnosis_$(date +%Y%m%d_%H%M%S)}"

mkdir -p "${LOG_DIR}"

if [[ ! -x "${BIN}" ]]; then
  echo "[demo10] missing binary: ${BIN}" >&2
  echo "[demo10] build first: ./build.sh 04_hwframe_vs_swframe" >&2
  exit 2
fi

if [[ ! -f "${INPUT}" ]]; then
  echo "[demo10] input not found: ${INPUT}" >&2
  exit 3
fi

if [[ -x "${PLAYBOOK_BIN}" ]]; then
  "${PLAYBOOK_BIN}" | tee "${LOG_DIR}/00_playbook.log"
fi

TIME_MODE="gnu"
if [[ ! -x "/usr/bin/time" ]]; then
  TIME_MODE="bash"
fi

SUMMARY_CSV="${LOG_DIR}/summary.csv"
cat > "${SUMMARY_CSV}" <<'EOF'
case,run_id,cmd_status,real_sec,user_sec,sys_sec,cpu_pct,maxrss_kb,decoder,hw_type,hw_frames,wrapper_frames,sw_fallback_frames,fallback,verdict
EOF

run_case() {
  local case_name="$1"
  local decoder="$2"
  local hw_type="$3"
  local run_id="$4"
  local case_dir="${LOG_DIR}/${case_name}_run${run_id}"
  local output_file="${case_dir}/output.log"
  local time_file="${case_dir}/time.log"
  local dmesg_file="${case_dir}/dmesg_media.log"
  local status_file="${case_dir}/status.txt"

  mkdir -p "${case_dir}"

  local args=(
    "--input=${INPUT}"
    "--decoder=${decoder}"
    "--max-frames=${MAX_FRAMES}"
  )

  if [[ -n "${hw_type}" ]]; then
    args+=("--hw-type=${hw_type}")
  fi

  local cmd_status=0
  if [[ "${TIME_MODE}" == "gnu" ]]; then
    set +e
    /usr/bin/time -f "real_sec=%e user_sec=%U sys_sec=%S cpu_pct=%P maxrss_kb=%M" -o "${time_file}" \
      "${BIN}" "${args[@]}" >"${output_file}" 2>&1
    cmd_status=$?
    set -e
  else
    # bash 内置 time 模式：无 cpu_pct/maxrss，仅提供 real/user/sys
    set +e
    {
      TIMEFORMAT='real_sec=%R user_sec=%U sys_sec=%S cpu_pct=N/A maxrss_kb=N/A'
      time "${BIN}" "${args[@]}" 2>&1
    } >"${output_file}" 2>"${time_file}"
    cmd_status=$?
    set -e
  fi

  printf "%s\n" "${cmd_status}" > "${status_file}"

  # 抓取与媒体相关的内核日志快照（权限不足时写明原因）
  if dmesg | grep -Ei "vpu|mpp|v4l2|drm|iommu|dma|codec|firmware|timeout|reset" > "${dmesg_file}" 2>/dev/null; then
    :
  else
    echo "dmesg snapshot unavailable (permission or no match)" > "${dmesg_file}"
  fi

  # 解析 time 指标
  local real_sec user_sec sys_sec cpu_pct maxrss_kb
  real_sec="$(sed -n 's/.*real_sec=\([^ ]*\).*/\1/p' "${time_file}" | tail -n 1)"
  user_sec="$(sed -n 's/.*user_sec=\([^ ]*\).*/\1/p' "${time_file}" | tail -n 1)"
  sys_sec="$(sed -n 's/.*sys_sec=\([^ ]*\).*/\1/p' "${time_file}" | tail -n 1)"
  cpu_pct="$(sed -n 's/.*cpu_pct=\([^ ]*\).*/\1/p' "${time_file}" | tail -n 1)"
  maxrss_kb="$(sed -n 's/.*maxrss_kb=\([^ ]*\).*/\1/p' "${time_file}" | tail -n 1)"

  # 解析 demo04 summary / verdict
  local summary_line verdict_line
  summary_line="$(grep -F "[demo04] summary" "${output_file}" | tail -n 1 || true)"
  verdict_line="$(grep -F "[demo04] verdict=" "${output_file}" | tail -n 1 || true)"

  local decoder_val hw_type_val hw_frames_val wrapper_frames_val sw_fallback_frames_val fallback_val verdict_val
  decoder_val="$(echo "${summary_line}" | sed -n 's/.*decoder=\([^ ]*\).*/\1/p')"
  hw_type_val="$(echo "${summary_line}" | sed -n 's/.*hw_type=\([^ ]*\).*/\1/p')"
  hw_frames_val="$(echo "${summary_line}" | sed -n 's/.*hw_frames=\([0-9]\+\).*/\1/p')"
  wrapper_frames_val="$(echo "${summary_line}" | sed -n 's/.*wrapper_frames=\([0-9]\+\).*/\1/p')"
  sw_fallback_frames_val="$(echo "${summary_line}" | sed -n 's/.*sw_fallback_frames=\([0-9]\+\).*/\1/p')"
  fallback_val="$(echo "${summary_line}" | sed -n 's/.*fallback=\([0-9]\+\).*/\1/p')"
  verdict_val="${verdict_line##*=}"

  : "${decoder_val:=N/A}"
  : "${hw_type_val:=N/A}"
  : "${hw_frames_val:=0}"
  : "${wrapper_frames_val:=0}"
  : "${sw_fallback_frames_val:=0}"
  : "${fallback_val:=1}"
  : "${verdict_val:=UNKNOWN}"
  : "${real_sec:=N/A}"
  : "${user_sec:=N/A}"
  : "${sys_sec:=N/A}"
  : "${cpu_pct:=N/A}"
  : "${maxrss_kb:=N/A}"

  echo "${case_name},${run_id},${cmd_status},${real_sec},${user_sec},${sys_sec},${cpu_pct},${maxrss_kb},${decoder_val},${hw_type_val},${hw_frames_val},${wrapper_frames_val},${sw_fallback_frames_val},${fallback_val},${verdict_val}" >> "${SUMMARY_CSV}"
}

echo "[demo10] log_dir=${LOG_DIR}"
echo "[demo10] input=${INPUT}"
echo "[demo10] max_frames=${MAX_FRAMES}"
echo "[demo10] loops=${LOOPS}"
echo "[demo10] time_mode=${TIME_MODE}"
echo "[demo10] running cases..."

for i in $(seq 1 "${LOOPS}"); do
  run_case "sw_baseline_h264" "h264" "" "${i}"
  run_case "rkmpp_wrapper_default" "h264_rkmpp" "" "${i}"
done

echo "[demo10] summary: ${SUMMARY_CSV}"
echo "[demo10] quick view:"
if command -v column >/dev/null 2>&1; then
  column -s, -t "${SUMMARY_CSV}"
else
  cat "${SUMMARY_CSV}"
fi
echo "[demo10] PASS"
