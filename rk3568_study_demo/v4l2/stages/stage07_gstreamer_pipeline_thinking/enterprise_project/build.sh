#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"
INC_DIR="${SCRIPT_DIR}/include"
BIN_DIR="${SCRIPT_DIR}/bin"
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++11 -O2 -Wall -Wextra}"
LDFLAGS="${LDFLAGS:-}"
TARGET="${BIN_DIR}/07_enterprise_gst_pipeline_service"

build_all() {
  mkdir -p "${BIN_DIR}"
  local tmp="${TARGET}.tmp.$$"
  echo "[build] enterprise gst pipeline service"
  "${CXX}" ${CXXFLAGS} -I"${INC_DIR}" \
    "${SRC_DIR}/01_cli_config.cpp" \
    "${SRC_DIR}/02_state_machine.cpp" \
    "${SRC_DIR}/03_logger.cpp" \
    "${SRC_DIR}/04_metrics_sink.cpp" \
    "${SRC_DIR}/05_gate_evaluator.cpp" \
    "${SRC_DIR}/06_pipeline_service.cpp" \
    "${SRC_DIR}/07_enterprise_gst_pipeline_main.cpp" \
    -o "${tmp}" ${LDFLAGS}
  mv -f "${tmp}" "${TARGET}"
}

case "${1:-all}" in
  all)
    build_all
    ;;
  clean)
    rm -rf "${BIN_DIR}"
    echo "[clean] removed enterprise binaries"
    ;;
  *)
    echo "Usage: $0 [all|clean]" >&2
    exit 2
    ;;
esac
