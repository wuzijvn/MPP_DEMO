#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"
INC_DIR="${SCRIPT_DIR}/include"
BIN_DIR="${SCRIPT_DIR}/bin"
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++11 -O2 -Wall -Wextra}"

mkdir -p "${BIN_DIR}"

sources=(
  "${SRC_DIR}/01_cli_config.cpp"
  "${SRC_DIR}/02_logger.cpp"
  "${SRC_DIR}/03_metrics_sink.cpp"
  "${SRC_DIR}/04_v4l2_pipeline_service.cpp"
  "${SRC_DIR}/05_enterprise_pipeline_main.cpp"
)

for s in "${sources[@]}"; do
  if [[ ! -f "${s}" ]]; then
    echo "[build] missing source: ${s}" >&2
    exit 1
  fi
done

out="${BIN_DIR}/12_enterprise_m2m_pipeline_service"
echo "[build] $(basename "${out}")"
"${CXX}" ${CXXFLAGS} -I"${INC_DIR}" "${sources[@]}" -o "${out}"

echo "[build] done"
