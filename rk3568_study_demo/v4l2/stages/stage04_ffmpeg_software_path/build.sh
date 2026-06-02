#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"
INC_DIR="${SCRIPT_DIR}/include"
BIN_DIR="${SCRIPT_DIR}/bin"
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++11 -O2 -Wall -Wextra}"

FF_CFLAGS="$(pkg-config --cflags libavformat libavcodec libavutil libswscale 2>/dev/null || true)"
FF_LIBS="$(pkg-config --libs libavformat libavcodec libavutil libswscale 2>/dev/null || true)"

if [[ -z "${FF_LIBS}" ]]; then
  echo "[build] missing ffmpeg dev libs: libavformat/libavcodec/libavutil/libswscale" >&2
  echo "[build] ubuntu/debian install example:" >&2
  echo "  sudo apt-get install -y ffmpeg libavformat-dev libavcodec-dev libavutil-dev libswscale-dev" >&2
  exit 2
fi

mkdir -p "${BIN_DIR}"

build_one() {
  local src="$1"
  local out="$2"
  echo "[build] $(basename "${src}") -> $(basename "${out}")"
  "${CXX}" ${CXXFLAGS} ${FF_CFLAGS} -I"${INC_DIR}" "${src}" -o "${out}" ${FF_LIBS}
}

cmd="${1:-all}"
if [[ "${cmd}" == "all" ]]; then
  for src in "${SRC_DIR}"/*.cpp; do
    [[ -e "${src}" ]] || continue
    base="$(basename "${src}" .cpp)"
    build_one "${src}" "${BIN_DIR}/${base}"
  done
  echo "[build] done"
  exit 0
fi

src="${SRC_DIR}/${cmd}.cpp"
if [[ ! -f "${src}" ]]; then
  echo "[build] source not found: ${src}" >&2
  exit 1
fi
build_one "${src}" "${BIN_DIR}/${cmd}"
