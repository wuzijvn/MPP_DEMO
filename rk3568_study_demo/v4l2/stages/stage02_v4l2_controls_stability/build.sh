#!/usr/bin/env bash
set -euo pipefail

# Stage 02 build script
# Usage:
#   ./build.sh
#   ./build.sh all
#   ./build.sh list
#   ./build.sh clean
#   ./build.sh rebuild
#   ./build.sh stage02_v4l2_controls_stability_main
#
# 教学说明：
# 1) Stage02 比 Stage01 多线程与同步逻辑，因此默认链接 pthread；
# 2) 脚本保留 all/list/clean/rebuild，便于稳定性实验时频繁重编译；
# 3) 构建入口保持一致，降低阶段切换的心智成本。

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${SCRIPT_DIR}/bin"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++11 -O2 -Wall -Wextra}"
LDFLAGS="${LDFLAGS:- -lpthread}"
# LDFLAGS 默认带 -lpthread，因为 stage02 主程序使用 pthread_create/join。

list_sources() {
  # 列出当前阶段可编译源文件，避免输错目标名。
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
  # 编译单个 .cpp -> bin/同名可执行文件。
  local src="$1"
  local base out
  base="$(basename "${src}")"
  out="${BIN_DIR}/${base%.cpp}"
  mkdir -p "${BIN_DIR}"
  echo "[build] ${base} -> $(basename "${out}")"
  "${CXX}" ${CXXFLAGS} "${src}" -o "${out}" ${LDFLAGS}
}

build_all() {
  # 批量编译本目录全部 .cpp，适合阶段内多入口实验。
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
  # 兼容传入“带 .cpp”或“不带 .cpp”的目标名。
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
# 不传参数时默认 all，确保最短命令可获得完整构建结果。
case "${cmd}" in
  all)
    build_all
    ;;
  list)
    echo "Detected Stage02 sources:"
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
