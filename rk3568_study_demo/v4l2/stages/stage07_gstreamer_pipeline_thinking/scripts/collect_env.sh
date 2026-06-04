#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUT_DIR="${OUT_DIR:-${STAGE_DIR}/logs/env_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "${OUT_DIR}"

{
  date
  uname -a
  command -v gst-launch-1.0 || true
  command -v gst-inspect-1.0 || true
  gst-launch-1.0 --version 2>/dev/null || true
  ffmpeg -hide_banner -version 2>/dev/null | sed -n '1,12p' || true
} >"${OUT_DIR}/env.txt" 2>&1

gst-inspect-1.0 2>/dev/null >"${OUT_DIR}/gst_inspect_all.txt" || true
gst-inspect-1.0 2>/dev/null | grep -Ei 'v4l2|vaapi|rkmpp|mpp|omx|h264|hevc|vp9|av1' >"${OUT_DIR}/gst_codec_backends.txt" || true
ls -l /dev/video* /dev/dri/* >"${OUT_DIR}/device_nodes.txt" 2>&1 || true
dmesg 2>/dev/null | grep -Ei 'vpu|mpp|rkvdec|hantro|v4l2|drm|dma|iommu|codec' | tail -200 >"${OUT_DIR}/dmesg_media_hints.txt" || true

echo "env_output_dir=${OUT_DIR}"
