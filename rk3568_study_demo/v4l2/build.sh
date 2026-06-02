#!/usr/bin/env bash
set -euo pipefail

# Unified build script for all V4L2 demos in this directory.
#
# Usage:
#   ./build.sh                # build all *.cpp
#   ./build.sh all            # same as above
#   ./build.sh list           # list detected source files
#   ./build.sh clean          # remove bin/
#   ./build.sh rebuild        # clean + build all
#   ./build.sh xxx            # build one file: xxx.cpp (or pass xxx.cpp)
#
# Optional env:
#   CXX=g++ CXXFLAGS="-std=c++11 -O2 -Wall -Wextra"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${SCRIPT_DIR}/bin"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++11 -O2 -Wall -Wextra}"
LDFLAGS="${LDFLAGS:-}"

list_sources() {
  local found=0
  for src in "${SCRIPT_DIR}"/*.cpp; do
    [[ -e "${src}" ]] || continue
    found=1
    echo "  - $(basename "${src}")"
  done
  if [[ "${found}" -eq 0 ]]; then
    echo "  (no .cpp files found)"
  fi
}

build_one() {
  local src="$1"
  local base out
  base="$(basename "${src}")"
  out="${BIN_DIR}/${base%.cpp}"
  mkdir -p "${BIN_DIR}"
  echo "[build] ${base} -> $(basename "${out}")"
  "${CXX}" ${CXXFLAGS} "${src}" -o "${out}" ${LDFLAGS}
}

build_all() {
  local found=0
  for src in "${SCRIPT_DIR}"/*.cpp; do
    [[ -e "${src}" ]] || continue
    found=1
    build_one "${src}"
  done
  if [[ "${found}" -eq 0 ]]; then
    echo "[build] no .cpp files in ${SCRIPT_DIR}"
  fi
}

resolve_one() {
  local name="$1"
  local src
  if [[ "${name}" == *.cpp ]]; then
    src="${SCRIPT_DIR}/${name}"
  else
    src="${SCRIPT_DIR}/${name}.cpp"
  fi
  if [[ ! -f "${src}" ]]; then
    echo "[error] source not found: ${src}" >&2
    echo "[hint] available sources:" >&2
    list_sources >&2
    exit 1
  fi
  build_one "${src}"
}

cmd="${1:-all}"
case "${cmd}" in
  all)
    build_all
    ;;
  list)
    echo "Detected V4L2 sources:"
    list_sources
    ;;
  clean)
    rm -rf "${BIN_DIR}"
    echo "[clean] removed ${BIN_DIR}"
    ;;
  rebuild)
    rm -rf "${BIN_DIR}"
    echo "[clean] removed ${BIN_DIR}"
    build_all
    ;;
  *)
    resolve_one "${cmd}"
    ;;
esac

