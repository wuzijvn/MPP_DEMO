#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"
INC_DIR="${SCRIPT_DIR}/include"
BIN_DIR="${SCRIPT_DIR}/bin"
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++11 -O2 -Wall -Wextra}"

FF_CFLAGS="$(pkg-config --cflags libavformat libavcodec libavutil 2>/dev/null || true)"
FF_LIBS="$(pkg-config --libs libavformat libavcodec libavutil 2>/dev/null || true)"

if [[ -z "${FF_LIBS}" ]]; then
  echo "[build] missing ffmpeg dev libs" >&2
  exit 2
fi

mkdir -p "${BIN_DIR}"
"${CXX}" ${CXXFLAGS} ${FF_CFLAGS} -I"${INC_DIR}" \
  "${SRC_DIR}/01_enterprise_pipeline_main.cpp" \
  -o "${BIN_DIR}/09_enterprise_ffmpeg_pipeline_service" ${FF_LIBS}

echo "[build] done"
