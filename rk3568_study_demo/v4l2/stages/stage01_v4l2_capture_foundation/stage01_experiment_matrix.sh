#!/usr/bin/env bash
# Stage01 experiment runner:
#   批量执行“分辨率 x pixfmt”矩阵实验，自动产出日志、trace CSV、汇总报告。
#
# 目的：
# 1) 形成可复盘证据链（命令 + 原始日志 + 指标摘要）
# 2) 快速对比“请求值 vs 生效值”与吞吐/稳定性变化
# 3) 为后续 ffmpeg/gstreamer/VPU 适配准备输入基线
#
# 学习建议：
# 1) 第一次先只跑一个 case，看清 report 结构；
# 2) 再跑全矩阵，对比不同 pixfmt 的 bytesused/fps/timeout；
# 3) 最后开启 skip-requeue 注入，观察“队列被破坏”后的症状。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="${SCRIPT_DIR}/bin/stage01_v4l2_capture_main"

DEV="/dev/video0"
OUT_DIR="${SCRIPT_DIR}/outputs"
FRAMES=300
WARMUP=3
FPS=30
TIMEOUT_MS=2000
REQ_BUFS=4
LOG_EVERY=100
DO_BUILD=0
DUMP_FORMATS=0
INJECT_SKIP_REQUEUE=1
# 上面这些默认值是“教学友好”配置，不一定是所有设备最优值。

usage() {
    cat <<EOF
Usage:
  $0 [options]

Options:
  --dev=PATH            default: ${DEV}
  --out-dir=DIR         default: ${OUT_DIR}
  --frames=N            default: ${FRAMES}
  --warmup=N            default: ${WARMUP}
  --fps=N               default: ${FPS}
  --timeout-ms=N        default: ${TIMEOUT_MS}
  --req-bufs=N          default: ${REQ_BUFS}
  --log-every=N         default: ${LOG_EVERY}
  --build               build binary before running
  --dump-formats        add --dump-formats in first case
  --no-skip-inject      do not run skip-requeue fault injection
  -h, --help

Examples:
  $0 --dev=/dev/video0 --build
  $0 --dev=/dev/video10 --frames=120 --fps=25 --out-dir=./outputs
EOF
}

for a in "$@"; do
    # 统一使用 --key=value 风格，便于脚本自动生成命令。
    case "$a" in
        --dev=*) DEV="${a#*=}" ;;
        --out-dir=*) OUT_DIR="${a#*=}" ;;
        --frames=*) FRAMES="${a#*=}" ;;
        --warmup=*) WARMUP="${a#*=}" ;;
        --fps=*) FPS="${a#*=}" ;;
        --timeout-ms=*) TIMEOUT_MS="${a#*=}" ;;
        --req-bufs=*) REQ_BUFS="${a#*=}" ;;
        --log-every=*) LOG_EVERY="${a#*=}" ;;
        --build) DO_BUILD=1 ;;
        --dump-formats) DUMP_FORMATS=1 ;;
        --no-skip-inject) INJECT_SKIP_REQUEUE=0 ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "unknown option: ${a}" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ "${DO_BUILD}" -eq 1 ]]; then
    # 可选：先构建再跑，避免手动切换。
    "${SCRIPT_DIR}/build.sh" stage01_v4l2_capture_main
fi

if [[ ! -x "${BIN}" ]]; then
    echo "binary not found or not executable: ${BIN}" >&2
    echo "hint: run ./build.sh stage01_v4l2_capture_main" >&2
    exit 1
fi

RUN_TAG="$(date -u +%Y%m%d_%H%M%S)"
# 每次运行单独目录，避免不同实验结果互相覆盖。
RUN_DIR="${OUT_DIR}/stage01_matrix_${RUN_TAG}"
LOG_DIR="${RUN_DIR}/logs"
TRACE_DIR="${RUN_DIR}/traces"
PREVIEW_DIR="${RUN_DIR}/preview"
REPORT_MD="${RUN_DIR}/report.md"

mkdir -p "${LOG_DIR}" "${TRACE_DIR}" "${PREVIEW_DIR}"

# 矩阵集合：你可以按设备能力删减。
PIXFMTS=("YUYV" "NV12" "MJPG")
SIZES=("640x480" "1280x720")

cat > "${REPORT_MD}" <<EOF
# Stage01 Matrix Report

- run_tag: ${RUN_TAG} (UTC)
- device: ${DEV}
- frames: ${FRAMES}
- warmup: ${WARMUP}
- req_fps: ${FPS}
- timeout_ms: ${TIMEOUT_MS}
- req_bufs: ${REQ_BUFS}
- log_every: ${LOG_EVERY}

## Summary Table

| case | req | pixfmt | ret | active_fmt | fps | timeout | dq_fail | requeue_fail | skipped | trace_csv |
|---|---|---|---:|---|---:|---:|---:|---:|---:|---|
EOF

extract_key() {
    # 从 summary 文本里提取形如 key=123 的数值。
    # 用于组装 markdown 表格。
    local key="$1"
    local log="$2"
    local out
    out="$(awk -v k="${key}" '
        $0 ~ k {
            for (i = 1; i <= NF; ++i) {
                if ($i ~ (k "=")) {
                    split($i, a, "=")
                    gsub(/[^0-9.]/, "", a[2])
                    print a[2]
                }
            }
        }' "${log}" | tail -n1)"
    echo "${out:-NA}"
}

extract_fps() {
    # 提取 "fps(actual dq_ok / duration): X" 里的 X。
    local log="$1"
    local out
    out="$(awk '/fps\(actual dq_ok \/ duration\):/ {print $NF}' "${log}" | tail -n1)"
    echo "${out:-NA}"
}

extract_active_fmt_line() {
    # 提取 active format snapshot 的核心一行，
    # 用于报告里直观对比“请求值 vs 生效值”。
    local log="$1"
    local out
    out="$(awk '
        /active format snapshot:/ {want=1; next}
        want==1 && /width=/ {print; exit}
    ' "${log}" | sed -e 's/^ *//' -e 's/|/\\|/g')"
    if [[ -z "${out}" ]]; then
        echo "NA"
    else
        echo "${out}"
    fi
}

run_case() {
    # 执行一个“分辨率 + pixfmt”组合，并把关键指标写入 report。
    local width="$1"
    local height="$2"
    local pixfmt="$3"
    local tag="${width}x${height}_${pixfmt}"
    local log="${LOG_DIR}/${tag}.log"
    local trace="${TRACE_DIR}/${tag}.csv"
    local raw="${PREVIEW_DIR}/${tag}.yuyv"
    local ppm="${PREVIEW_DIR}/${tag}.ppm"

    local cmd=("${BIN}"
               "${DEV}" "${width}" "${height}" "${raw}" "${ppm}" "${FRAMES}"
               "--warmup=${WARMUP}"
               "--fps=${FPS}"
               "--pixfmt=${pixfmt}"
               "--timeout-ms=${TIMEOUT_MS}"
               "--req-bufs=${REQ_BUFS}"
               "--log-every=${LOG_EVERY}"
               "--trace-csv=${trace}")

    # 仅首个 case 打一次格式枚举，避免日志膨胀。
    # 枚举信息主要用于“设备能力摸底”，不需要每个 case 都重复。
    if [[ "${DUMP_FORMATS}" -eq 1 && ! -f "${RUN_DIR}/.dumped_formats_once" ]]; then
        cmd+=("--dump-formats")
        : > "${RUN_DIR}/.dumped_formats_once"
    fi

    # 当前预览导出仅支持 YUYV：
    # 其他格式仍可做采集/统计/trace，只是不转 PPM。
    if [[ "${pixfmt}" != "YUYV" ]]; then
        cmd+=("--no-save")
    fi

    echo "[run] ${tag}"
    echo "cmd: ${cmd[*]}" > "${log}"
    echo >> "${log}"

    local rc=0
    set +e
    # case 级别允许失败（例如某些格式不支持），所以临时关闭 set -e。
    "${cmd[@]}" >> "${log}" 2>&1
    rc=$?
    set -e

    local fps timeout dq_fail requeue_fail skipped active_fmt
    fps="$(extract_fps "${log}")"
    timeout="$(extract_key "timeout" "${log}")"
    dq_fail="$(extract_key "fail" "${log}")"
    requeue_fail="$(awk '
        /qbuf\(requeue\):/ {want=1; next}
        want==1 && /ok=/ {
            for (i = 1; i <= NF; ++i) {
                if ($i ~ /^fail=/) {
                    split($i, a, "=")
                    gsub(/[^0-9]/, "", a[2])
                    print a[2]
                    exit
                }
            }
        }
    ' "${log}" | tail -n1)"
    skipped="$(awk '
        /qbuf\(requeue\):/ {want=1; next}
        want==1 && /ok=/ {
            for (i = 1; i <= NF; ++i) {
                if ($i ~ /^skipped=/) {
                    split($i, a, "=")
                    gsub(/[^0-9]/, "", a[2])
                    print a[2]
                    exit
                }
            }
        }
    ' "${log}" | tail -n1)"
    active_fmt="$(extract_active_fmt_line "${log}")"
    if [[ -z "${requeue_fail}" ]]; then requeue_fail="NA"; fi
    if [[ -z "${skipped}" ]]; then skipped="NA"; fi

    echo "| ${tag} | ${width}x${height} | ${pixfmt} | ${rc} | ${active_fmt} | ${fps} | ${timeout} | ${dq_fail} | ${requeue_fail} | ${skipped} | ${trace} |" >> "${REPORT_MD}"
}

for s in "${SIZES[@]}"; do
    w="${s%x*}"
    h="${s#*x}"
    for f in "${PIXFMTS[@]}"; do
        run_case "${w}" "${h}" "${f}"
    done
done

if [[ "${INJECT_SKIP_REQUEUE}" -eq 1 ]]; then
    # 额外故障注入 case：
    # 从指定帧开始不回队，观察 timeout/dq_fail/requeue_skipped 变化。
    tag="inject_skip_requeue"
    log="${LOG_DIR}/${tag}.log"
    trace="${TRACE_DIR}/${tag}.csv"
    raw="${PREVIEW_DIR}/${tag}.yuyv"
    ppm="${PREVIEW_DIR}/${tag}.ppm"
    cmd=("${BIN}"
         "${DEV}" "640" "480" "${raw}" "${ppm}" "${FRAMES}"
         "--warmup=${WARMUP}"
         "--fps=${FPS}"
         "--pixfmt=YUYV"
         "--timeout-ms=${TIMEOUT_MS}"
         "--req-bufs=${REQ_BUFS}"
         "--log-every=20"
         "--inject=skip-requeue"
         "--inject-frame=30"
         "--trace-csv=${trace}")
    echo "[run] ${tag}"
    echo "cmd: ${cmd[*]}" > "${log}"
    echo >> "${log}"
    rc=0
    set +e
    "${cmd[@]}" >> "${log}" 2>&1
    rc=$?
    set -e
    fps="$(extract_fps "${log}")"
    timeout="$(extract_key "timeout" "${log}")"
    dq_fail="$(extract_key "fail" "${log}")"
    requeue_fail="$(awk '
        /qbuf\(requeue\):/ {want=1; next}
        want==1 && /ok=/ {
            for (i = 1; i <= NF; ++i) {
                if ($i ~ /^fail=/) {
                    split($i, a, "=")
                    gsub(/[^0-9]/, "", a[2])
                    print a[2]
                    exit
                }
            }
        }
    ' "${log}" | tail -n1)"
    skipped="$(awk '
        /qbuf\(requeue\):/ {want=1; next}
        want==1 && /ok=/ {
            for (i = 1; i <= NF; ++i) {
                if ($i ~ /^skipped=/) {
                    split($i, a, "=")
                    gsub(/[^0-9]/, "", a[2])
                    print a[2]
                    exit
                }
            }
        }
    ' "${log}" | tail -n1)"
    active_fmt="$(extract_active_fmt_line "${log}")"
    if [[ -z "${requeue_fail}" ]]; then requeue_fail="NA"; fi
    if [[ -z "${skipped}" ]]; then skipped="NA"; fi
    echo "| ${tag} | 640x480 | YUYV | ${rc} | ${active_fmt} | ${fps} | ${timeout} | ${dq_fail} | ${requeue_fail} | ${skipped} | ${trace} |" >> "${REPORT_MD}"
fi

cat >> "${REPORT_MD}" <<EOF

## Notes

- ret=0：达到目标帧数；ret=2：采集中途提前结束（常见于超时/注入）。
- 请优先关注：
  1) active format snapshot 是否与请求一致
  2) fps 与 timeout/dq_fail 的关联
  3) trace CSV 中 host_delta_ms/v4l2_delta_ms 是否稳定
  4) buffer flags 分布是否出现 ERROR

## Raw Artifacts

- logs: ${LOG_DIR}
- traces: ${TRACE_DIR}
- preview: ${PREVIEW_DIR}
EOF

echo
echo "[done] report: ${REPORT_MD}"
echo "[done] logs  : ${LOG_DIR}"
echo "[done] traces: ${TRACE_DIR}"
echo "[done] preview: ${PREVIEW_DIR}"
