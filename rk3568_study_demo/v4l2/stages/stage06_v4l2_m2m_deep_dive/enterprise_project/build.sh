#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"
INC_DIR="${SCRIPT_DIR}/include"
BIN_DIR="${SCRIPT_DIR}/bin"
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++11 -O2 -Wall -Wextra}"
LDFLAGS="${LDFLAGS:-}"
TARGET="${BIN_DIR}/07_enterprise_m2m_diagnostic_service"

cmd="${1:-all}"
case "${cmd}" in
  all|07_enterprise_m2m_diagnostic_service)
    mkdir -p "${BIN_DIR}"
    echo "[build] enterprise m2m diagnostic service"
    "${CXX}" ${CXXFLAGS} -I"${INC_DIR}" \
      "${SRC_DIR}/01_cli_config.cpp" \
      "${SRC_DIR}/02_state_machine.cpp" \
      "${SRC_DIR}/03_logger.cpp" \
      "${SRC_DIR}/04_metrics_sink.cpp" \
      "${SRC_DIR}/05_gate_evaluator.cpp" \
      "${SRC_DIR}/06_m2m_diagnostic_service.cpp" \
      "${SRC_DIR}/07_enterprise_m2m_diagnostic_main.cpp" \
      -o "${TARGET}" ${LDFLAGS}
    echo "[build] wrote ${TARGET}"
    ;;
  clean)
    rm -rf "${BIN_DIR}"
    echo "[clean] removed enterprise bin"
    ;;
  *)
    echo "unknown build command: ${cmd}" >&2
    exit 1
    ;;
esac
