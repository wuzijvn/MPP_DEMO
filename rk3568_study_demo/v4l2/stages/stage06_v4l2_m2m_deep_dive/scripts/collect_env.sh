#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
STAMP="$(date -u +%Y%m%d_%H%M%S)"
OUT_DIR="${STAGE_DIR}/logs/env_${STAMP}"
mkdir -p "${OUT_DIR}"
{
  uname -a || true
  echo ""
  g++ --version 2>/dev/null | head -n 2 || true
} > "${OUT_DIR}/env.txt"
ls -l /dev/video* 2>/dev/null > "${OUT_DIR}/dev_video.txt" || true
if command -v v4l2-ctl >/dev/null 2>&1; then
  v4l2-ctl --list-devices > "${OUT_DIR}/v4l2_list_devices.txt" 2>&1 || true
else
  echo "v4l2-ctl not installed" > "${OUT_DIR}/v4l2_list_devices.txt"
fi
dmesg 2>/dev/null | grep -Ei "v4l2|video|m2m|vpu|codec|rkvdec|rkvenc|mpp|dma|iommu|timeout|reset|firmware" > "${OUT_DIR}/dmesg_media.txt" || true
echo "[collect_env] wrote ${OUT_DIR}"
