#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"
INC_DIR="${SCRIPT_DIR}/include"
BIN_DIR="${SCRIPT_DIR}/bin"
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++11 -O2 -Wall -Wextra}"
LDFLAGS="${LDFLAGS:-}"

list_sources() {
  local found=0
  for src in "${SRC_DIR}"/*.cpp; do
    [[ -e "${src}" ]] || continue
    found=1
    echo "  - $(basename "${src}")"
  done
  if [[ "${found}" -eq 0 ]]; then
    echo "  (no .cpp files found in src/)"
  fi
}

build_one() {
  local src="$1"
  local base out
  base="$(basename "${src}")"
  out="${BIN_DIR}/${base%.cpp}"
  mkdir -p "${BIN_DIR}"
  echo "[build] ${base} -> $(basename "${out}")"
  "${CXX}" ${CXXFLAGS} -I"${INC_DIR}" "${src}" -o "${out}" ${LDFLAGS}
}

build_all() {
  local found=0
  for src in "${SRC_DIR}"/*.cpp; do
    [[ -e "${src}" ]] || continue
    found=1
    build_one "${src}"
  done
  if [[ "${found}" -eq 0 ]]; then
    echo "[build] no .cpp files in ${SRC_DIR}"
  fi
}

resolve_one() {
  local name="$1"
  local src
  if [[ "${name}" == *.cpp ]]; then
    src="${SRC_DIR}/${name}"
  else
    src="${SRC_DIR}/${name}.cpp"
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
    echo "Detected Stage03 sources (src/):"
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
