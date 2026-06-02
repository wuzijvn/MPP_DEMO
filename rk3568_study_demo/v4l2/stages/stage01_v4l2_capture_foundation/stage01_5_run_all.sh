#!/usr/bin/env bash
# Stage01.5 one-click runner
#
# 作用：
# 1) 按 Stage01.5 runbook 的 A~E 依次执行；
# 2) 每个 case 保留独立日志；
# 3) 自动提取关键指标，生成 summary.csv + summary.md。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="${SCRIPT_DIR}/bin/stage01_v4l2_capture_main"
EXTRACTOR="${SCRIPT_DIR}/stage01_5_extract_metrics.sh"

DEV="/dev/video10"
OUT_DIR="${SCRIPT_DIR}/outputs_stage01_5"
FRAMES=120
WARMUP=3
FPS=30
TIMEOUT_MS=2000
REQ_BUFS_BASE=4
LOG_EVERY=20
DO_BUILD=0

usage() {
    cat <<USAGE
Usage:
  $0 [options]

Options:
  --dev=PATH           default: ${DEV}
  --out-dir=DIR        default: ${OUT_DIR}
  --frames=N           default: ${FRAMES}
  --warmup=N           default: ${WARMUP}
  --fps=N              default: ${FPS}
  --timeout-ms=N       default: ${TIMEOUT_MS}
  --log-every=N        default: ${LOG_EVERY}
  --build              rebuild binary before run
  -h, --help

Examples:
  $0 --build
  $0 --dev=/dev/video10 --frames=80
USAGE
}

for a in "$@"; do
    case "$a" in
        --dev=*) DEV="${a#*=}" ;;
        --out-dir=*) OUT_DIR="${a#*=}" ;;
        --frames=*) FRAMES="${a#*=}" ;;
        --warmup=*) WARMUP="${a#*=}" ;;
        --fps=*) FPS="${a#*=}" ;;
        --timeout-ms=*) TIMEOUT_MS="${a#*=}" ;;
        --log-every=*) LOG_EVERY="${a#*=}" ;;
        --build) DO_BUILD=1 ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "unknown option: $a" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ "$DO_BUILD" -eq 1 ]]; then
    "${SCRIPT_DIR}/build.sh" stage01_v4l2_capture_main
fi

if [[ ! -x "$BIN" ]]; then
    echo "binary not found: $BIN" >&2
    echo "hint: run ./build.sh stage01_v4l2_capture_main" >&2
    exit 2
fi

if [[ ! -x "$EXTRACTOR" ]]; then
    echo "extractor not found: $EXTRACTOR" >&2
    exit 2
fi

RUN_TAG="$(date -u +%Y%m%d_%H%M%S)"
RUN_DIR="${OUT_DIR}/stage01_5_${RUN_TAG}"
LOG_DIR="${RUN_DIR}/logs"
SUMMARY_CSV="${RUN_DIR}/summary.csv"
SUMMARY_MD="${RUN_DIR}/summary.md"

mkdir -p "$LOG_DIR"

run_case() {
    local label="$1"
    shift
    local log_path="${LOG_DIR}/${label}.log"

    echo "[run] ${label}"
    echo "cmd: $*" > "$log_path"
    echo >> "$log_path"

    local rc=0
    set +e
    "$@" >> "$log_path" 2>&1
    rc=$?
    set -e

    echo "[done] ${label} rc=${rc}"

    local csv_row
    csv_row="$(${EXTRACTOR} --csv --label="${label}" "$log_path")"
    echo "${csv_row},${rc}" >> "$SUMMARY_CSV"
}

# CSV header
cat > "$SUMMARY_CSV" <<'HDR'
case,fps,select_timeout,dq_fail,dq_eagain,requeue_fail,requeue_skipped,bytes_min,bytes_max,bytes_avg,dq_interval_avg_ms,v4l2_interval_avg_ms,active_fmt,ret
HDR

# A baseline
run_case "A_baseline_640x480_yuyv_buf4" \
    "$BIN" "$DEV" 640 480 /tmp/a_base.yuyv /tmp/a_base.ppm "$FRAMES" \
    "--warmup=${WARMUP}" "--fps=${FPS}" "--pixfmt=YUYV" "--timeout-ms=${TIMEOUT_MS}" \
    "--req-bufs=${REQ_BUFS_BASE}" "--no-save" "--log-every=${LOG_EVERY}"

# B req-bufs compare
for b in 2 4 8; do
    run_case "B_reqbufs_${b}_640x480_yuyv" \
        "$BIN" "$DEV" 640 480 /tmp/b_${b}.yuyv /tmp/b_${b}.ppm "$FRAMES" \
        "--warmup=${WARMUP}" "--fps=${FPS}" "--pixfmt=YUYV" "--timeout-ms=${TIMEOUT_MS}" \
        "--req-bufs=${b}" "--no-save" "--log-every=${LOG_EVERY}"
done

# C pixfmt compare (same resolution)
for pf in YUYV MJPG; do
    run_case "C_pixfmt_${pf}_640x480" \
        "$BIN" "$DEV" 640 480 "/tmp/c_${pf}.yuyv" "/tmp/c_${pf}.ppm" "$FRAMES" \
        "--warmup=${WARMUP}" "--fps=${FPS}" "--pixfmt=${pf}" "--timeout-ms=${TIMEOUT_MS}" \
        "--req-bufs=${REQ_BUFS_BASE}" "--no-save" "--log-every=${LOG_EVERY}"
done

# D resolution compare (same format)
for sz in 640x480 1280x720; do
    w="${sz%x*}"
    h="${sz#*x}"
    run_case "D_res_${sz}_yuyv" \
        "$BIN" "$DEV" "$w" "$h" "/tmp/d_${sz}.yuyv" "/tmp/d_${sz}.ppm" "$FRAMES" \
        "--warmup=${WARMUP}" "--fps=${FPS}" "--pixfmt=YUYV" "--timeout-ms=${TIMEOUT_MS}" \
        "--req-bufs=${REQ_BUFS_BASE}" "--no-save" "--log-every=${LOG_EVERY}"
done

# E fault injection: skip-requeue
run_case "E_inject_skip_requeue_f20" \
    "$BIN" "$DEV" 640 480 /tmp/e_skip.yuyv /tmp/e_skip.ppm 80 \
    "--warmup=${WARMUP}" "--fps=${FPS}" "--pixfmt=YUYV" "--timeout-ms=${TIMEOUT_MS}" \
    "--req-bufs=${REQ_BUFS_BASE}" "--inject=skip-requeue" "--inject-frame=20" \
    "--no-save" "--log-every=${LOG_EVERY}"

# 生成 markdown 预览
{
    echo "# Stage01.5 Summary"
    echo
    echo "- run_tag: ${RUN_TAG} (UTC)"
    echo "- device: ${DEV}"
    echo "- frames(default): ${FRAMES}"
    echo "- warmup: ${WARMUP}"
    echo "- req_fps: ${FPS}"
    echo "- timeout_ms: ${TIMEOUT_MS}"
    echo "- log_every: ${LOG_EVERY}"
    echo
    echo "## Table"
    echo
    echo "| case | fps | timeout | dq_fail | dq_eagain | rq_fail | rq_skipped | bytes(min/max/avg) | dq_avg_ms | v4l2_avg_ms | ret |"
    echo "|---|---:|---:|---:|---:|---:|---:|---|---:|---:|---:|"
    awk -F',' 'NR>1 {
        case_name=$1
        fps=$2
        to=$3
        dqf=$4
        dqe=$5
        rqf=$6
        rqs=$7
        bmin=$8
        bmax=$9
        bavg=$10
        dqavg=$11
        vavg=$12
        ret=$14
        printf("| %s | %s | %s | %s | %s | %s | %s | %s/%s/%s | %s | %s | %s |\n", case_name, fps, to, dqf, dqe, rqf, rqs, bmin, bmax, bavg, dqavg, vavg, ret)
    }' "$SUMMARY_CSV"
    echo
    echo "## Raw"
    echo
    echo "- logs: ${LOG_DIR}"
    echo "- summary.csv: ${SUMMARY_CSV}"
} > "$SUMMARY_MD"

echo

echo "[done] summary csv: ${SUMMARY_CSV}"
echo "[done] summary md : ${SUMMARY_MD}"
echo "[done] logs       : ${LOG_DIR}"
