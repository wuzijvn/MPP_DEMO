#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="${1:-./logs/env_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "${OUT_DIR}"

echo "[collect_env] output=${OUT_DIR}"

{
  echo "timestamp=$(date -Iseconds)"
  echo "uname=$(uname -a)"
  echo "cwd=$(pwd)"
} > "${OUT_DIR}/env.txt"

ls -l /dev/video* > "${OUT_DIR}/dev_video.txt" 2>&1 || true
v4l2-ctl --list-devices > "${OUT_DIR}/v4l2_list_devices.txt" 2>&1 || true
v4l2-ctl -d /dev/video0 --all > "${OUT_DIR}/v4l2_video0_all.txt" 2>&1 || true
v4l2-ctl -d /dev/video0 --list-formats-ext > "${OUT_DIR}/v4l2_video0_formats.txt" 2>&1 || true
dmesg | grep -Ei "v4l2|video|codec|vpu|vim2m|vicodec|drm" > "${OUT_DIR}/dmesg_media.txt" 2>&1 || true

echo "[collect_env] done"
