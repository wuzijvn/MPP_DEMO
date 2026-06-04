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
  [[ "${found}" -eq 1 ]] || echo "  (no .cpp files found)"
}

build_one() {
  local src="$1"
  local base out tmp
  base="$(basename "${src}" .cpp)"
  out="${BIN_DIR}/${base}"
  tmp="${out}.tmp.$$"
  mkdir -p "${BIN_DIR}"
  echo "[build] ${base}.cpp -> ${base}"
  "${CXX}" ${CXXFLAGS} -I"${INC_DIR}" "${src}" -o "${tmp}" ${LDFLAGS}
  mv -f "${tmp}" "${out}"
}

build_all() {
  local found=0
  for src in "${SRC_DIR}"/*.cpp; do
    [[ -e "${src}" ]] || continue
    found=1
    build_one "${src}"
  done
  [[ "${found}" -eq 1 ]] || echo "[build] no sources"
}

cmd="${1:-all}"
case "${cmd}" in
  all)
    build_all
    ;;
  enterprise)
    "${SCRIPT_DIR}/enterprise_project/build.sh" all
    ;;
  all-with-enterprise)
    build_all
    "${SCRIPT_DIR}/enterprise_project/build.sh" all
    ;;
  clean)
    rm -rf "${BIN_DIR}"
    "${SCRIPT_DIR}/enterprise_project/build.sh" clean || true
    echo "[clean] removed stage07 binaries"
    ;;
  list)
    list_sources
    ;;
  *)
    src="${SRC_DIR}/${cmd}.cpp"
    if [[ ! -f "${src}" ]]; then
      echo "[error] source not found: ${src}" >&2
      list_sources >&2
      exit 1
    fi
    build_one "${src}"
    ;;
esac
