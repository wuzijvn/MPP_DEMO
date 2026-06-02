#!/usr/bin/env bash
set -euo pipefail
OUT_DIR="${1:-./logs/env_$(date +%Y%m%d_%H%M%S)}"
mkdir -p "${OUT_DIR}"
{
  echo "timestamp=$(date -Iseconds)"
  echo "uname=$(uname -a)"
  echo "cwd=$(pwd)"
} > "${OUT_DIR}/env.txt"
which ffmpeg > "${OUT_DIR}/which_ffmpeg.txt" 2>&1 || true
which ffprobe > "${OUT_DIR}/which_ffprobe.txt" 2>&1 || true
ffmpeg -version > "${OUT_DIR}/ffmpeg_version.txt" 2>&1 || true
ffprobe -version > "${OUT_DIR}/ffprobe_version.txt" 2>&1 || true
pkg-config --modversion libavformat libavcodec libavutil libswscale > "${OUT_DIR}/pkg_config_ffmpeg.txt" 2>&1 || true
dmesg | grep -Ei "ffmpeg|video|v4l2|drm|codec|vpu" > "${OUT_DIR}/dmesg_media.txt" 2>&1 || true
echo "[collect_env] output=${OUT_DIR}"
