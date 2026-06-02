#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
STAMP="$(date -u +%Y%m%d_%H%M%S)"
OUT_DIR="${OUT_DIR:-${STAGE_DIR}/logs/rk_board_reality_${STAMP}}"
mkdir -p "${OUT_DIR}"

summary="${OUT_DIR}/summary.txt"
: > "${summary}"

log_section() {
  local title="$1"
  printf '\n==== %s ====\n' "${title}" | tee -a "${summary}"
}

log_section "device nodes"
ls -l /dev/video* /dev/dri/* 2>&1 | tee "${OUT_DIR}/device_nodes.txt" | tee -a "${summary}" >/dev/null || true

log_section "v4l2 devices"
if command -v v4l2-ctl >/dev/null 2>&1; then
  v4l2-ctl --list-devices 2>&1 | tee "${OUT_DIR}/v4l2_list_devices.txt" | tee -a "${summary}" >/dev/null || true
else
  echo "v4l2-ctl not installed" | tee "${OUT_DIR}/v4l2_list_devices.txt" | tee -a "${summary}" >/dev/null
fi

log_section "ffmpeg rkmpp/v4l2 decoders"
if command -v ffmpeg >/dev/null 2>&1; then
  ffmpeg -hide_banner -decoders 2>/dev/null | grep -Ei 'rkmpp|v4l2m2m|h264|hevc|vp8|vp9|av1' \
    | tee "${OUT_DIR}/ffmpeg_codec_backends.txt" | tee -a "${summary}" >/dev/null || true
else
  echo "ffmpeg not installed" | tee "${OUT_DIR}/ffmpeg_codec_backends.txt" | tee -a "${summary}" >/dev/null
fi

log_section "dmesg media hints"
dmesg 2>/dev/null | grep -Ei 'rkvdec|rkvenc|mpp|vpu|v4l2|codec|h264|hevc|drm|dma|iommu|firmware|timeout|reset' \
  | tail -n 200 | tee "${OUT_DIR}/dmesg_media_hints.txt" | tee -a "${summary}" >/dev/null || true

# 判定 1：是否存在真实字符设备形式的 V4L2 M2M codec 节点。
# 注意：有些 RK BSP 会出现 /dev/video-dec0 这种名字，但如果不是字符设备，不能按 V4L2 ioctl 节点使用。
m2m_candidate_count=0
if command -v v4l2-ctl >/dev/null 2>&1; then
  while IFS= read -r node; do
    [[ -c "${node}" ]] || continue
    if v4l2-ctl -d "${node}" --all 2>/dev/null | grep -Eq 'Video Memory-to-Memory|Video M2M|VIDEO_M2M|M2M'; then
      m2m_candidate_count=$((m2m_candidate_count + 1))
      echo "M2M candidate: ${node}" >> "${OUT_DIR}/m2m_candidates.txt"
    fi
  done < <(ls /dev/video* 2>/dev/null || true)
fi

# 判定 2：FFmpeg 是否有 RKMPP decoder，这是当前板端更可能的真实硬解入口。
rkmpp_decoder_count=0
if [[ -s "${OUT_DIR}/ffmpeg_codec_backends.txt" ]]; then
  rkmpp_decoder_count="$(grep -Ec '_(rkmpp)[[:space:]]' "${OUT_DIR}/ffmpeg_codec_backends.txt" || true)"
fi

log_section "verdict"
if [[ "${m2m_candidate_count}" -gt 0 ]]; then
  echo "v4l2_m2m_status=AVAILABLE" | tee -a "${summary}"
else
  echo "v4l2_m2m_status=NOT_FOUND" | tee -a "${summary}"
fi

if [[ "${rkmpp_decoder_count}" -gt 0 ]]; then
  echo "rkmpp_status=AVAILABLE" | tee -a "${summary}"
else
  echo "rkmpp_status=NOT_FOUND" | tee -a "${summary}"
fi

if [[ "${m2m_candidate_count}" -eq 0 && "${rkmpp_decoder_count}" -gt 0 ]]; then
  cat <<'MSG' | tee -a "${summary}"
recommended_learning_path=RKMPP_REAL_PATH_PLUS_V4L2_M2M_CONCEPT
meaning=当前板端没有可用 V4L2 M2M codec 字符设备；真实硬解验证应优先使用 FFmpeg h264_rkmpp/hevc_rkmpp。Stage06 的 ioctl/queue 内容按概念和模拟方式学习，不要强行对 rkisp/camera 节点跑 codec ioctl。
MSG
elif [[ "${m2m_candidate_count}" -gt 0 ]]; then
  echo "recommended_learning_path=V4L2_M2M_REAL_DEVICE_AVAILABLE" | tee -a "${summary}"
else
  echo "recommended_learning_path=NO_HW_CODEC_BACKEND_FOUND_NEED_BSP_CHECK" | tee -a "${summary}"
fi

echo "reality_check_logs=${OUT_DIR}"
