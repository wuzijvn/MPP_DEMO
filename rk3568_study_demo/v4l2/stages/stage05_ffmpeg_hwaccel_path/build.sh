#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"
INC_DIR="${SCRIPT_DIR}/include"
BIN_DIR="${SCRIPT_DIR}/bin"
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++11 -O2 -Wall -Wextra}"

if [[ -n "${FFMPEG_PREFIX:-}" ]]; then
  export PKG_CONFIG_PATH="${FFMPEG_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
fi

FF_CFLAGS="$(pkg-config --cflags libavformat libavcodec libavutil libswresample libswscale 2>/dev/null || true)"
FF_LIBS="$(pkg-config --libs libavformat libavcodec libavutil libswresample libswscale 2>/dev/null || true)"

if [[ -n "${FFMPEG_PREFIX:-}" ]]; then
  FF_LIBS="${FF_LIBS} -Wl,-rpath,${FFMPEG_PREFIX}/lib -Wl,-rpath-link,${FFMPEG_PREFIX}/lib"

  # Mirror the board-side ffmpeg wrapper: Rockchip FFmpeg may keep libva beside
  # the FFmpeg install instead of in the system loader path.
  extra_va_libs_added=0
  for extra_dir in "${FFMPEG_PREFIX}/extra-va-libs" "${FFMPEG_PREFIX}/extra-libs"; do
    if [[ -d "${extra_dir}" ]]; then
      FF_LIBS="${FF_LIBS} -L${extra_dir} -Wl,-rpath,${extra_dir} -Wl,-rpath-link,${extra_dir}"
      if [[ "${extra_va_libs_added}" -eq 0 && -e "${extra_dir}/libva.so" && -e "${extra_dir}/libva-drm.so" ]]; then
        FF_LIBS="${FF_LIBS} -Wl,--no-as-needed -lva-drm -lva -lswresample -lswscale -Wl,--as-needed"
        extra_va_libs_added=1
      fi
    fi
  done
fi

if [[ -z "${FF_LIBS}" ]]; then
  echo "[build] missing ffmpeg dev libs: libavformat/libavcodec/libavutil" >&2
  echo "[build] ubuntu/debian install example:" >&2
  echo "  sudo apt-get install -y ffmpeg libavformat-dev libavcodec-dev libavutil-dev pkg-config" >&2
  echo "[build] rockchip ffmpeg example:" >&2
  echo "  FFMPEG_PREFIX=/opt/rockchip/ffmpeg-rockchip ./build.sh all" >&2
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
