#!/usr/bin/env bash
set -euo pipefail

# Stage 01 build script
# Usage:
#   ./build.sh                 # build all *.cpp in this stage
#   ./build.sh all
#   ./build.sh list
#   ./build.sh clean
#   ./build.sh rebuild
#   ./build.sh stage01_v4l2_capture_main
#
# 教学说明：
# 1) 这是“单阶段局部编译脚本”，不依赖上层工程构建系统；
# 2) 你可以先在这里快速迭代 demo，再迁移到 CMake/Yocto/Buildroot；
# 3) 保留 list/clean/rebuild 子命令，便于日常调试与 CI 脚本接入。

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${SCRIPT_DIR}/bin"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++11 -O2 -Wall -Wextra}"
LDFLAGS="${LDFLAGS:-}"
# 说明：
# - CXX/CXXFLAGS/LDFLAGS 都允许环境变量覆盖；
# - 方便你在不改脚本的情况下切换编译器/优化级别/额外库。

list_sources() {
  # 列出本目录可编译源文件，帮助确认“目标名到底叫什么”。
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
  # 把单个 .cpp 编译到 bin/ 下同名可执行文件。
  # 例如 stage01_v4l2_capture_main.cpp -> bin/stage01_v4l2_capture_main
  local src="$1"
  local base out
  base="$(basename "${src}")"
  out="${BIN_DIR}/${base%.cpp}"
  mkdir -p "${BIN_DIR}"
  echo "[build] ${base} -> $(basename "${out}")"
  "${CXX}" ${CXXFLAGS} "${src}" -o "${out}" ${LDFLAGS}
}

build_all() {
  # 批量编译当前 stage 目录下所有 .cpp。
  # 这对“一个阶段多个实验入口”的组织方式很方便。
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
  # 允许两种输入：
  # 1) 带后缀：stage01_v4l2_capture_main.cpp
  # 2) 不带后缀：stage01_v4l2_capture_main
  #
  # 这样用起来更顺手，也更适合在脚本里拼接目标名。
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
# 默认 all：让“直接 ./build.sh”也能得到可运行二进制。
case "${cmd}" in
  all)
    build_all
    ;;
  list)
    echo "Detected Stage01 sources:"
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
