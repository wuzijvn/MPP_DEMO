#!/usr/bin/env bash
# Stage01.5 log metric extractor
#
# 用途：
# 1) 从单个 stage01 日志提取你关心的核心指标；
# 2) 统一成固定字段，便于横向对比；
# 3) 降低“手工抄日志”出错概率。
#
# 支持两种输出：
# - pretty（默认）：key=value，适合人读
# - csv            ：单行 CSV，适合拼汇总表

set -euo pipefail

usage() {
    cat <<'USAGE'
Usage:
  stage01_5_extract_metrics.sh [--csv] [--label=NAME] LOG_FILE

Options:
  --csv           output one CSV row
  --label=NAME    case label (default: log filename without .log)
  -h, --help

Examples:
  ./stage01_5_extract_metrics.sh ./outputs/logs/base.log
  ./stage01_5_extract_metrics.sh --csv --label=reqbuf_4 ./outputs/logs/reqbuf_4.log
USAGE
}

MODE="pretty"
LABEL=""
LOG_FILE=""

for a in "$@"; do
    case "$a" in
        --csv)
            MODE="csv"
            ;;
        --label=*)
            LABEL="${a#*=}"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --*)
            echo "unknown option: $a" >&2
            usage >&2
            exit 1
            ;;
        *)
            if [[ -n "$LOG_FILE" ]]; then
                echo "only one LOG_FILE is allowed" >&2
                usage >&2
                exit 1
            fi
            LOG_FILE="$a"
            ;;
    esac
done

if [[ -z "$LOG_FILE" ]]; then
    usage >&2
    exit 1
fi

if [[ ! -f "$LOG_FILE" ]]; then
    echo "log file not found: $LOG_FILE" >&2
    exit 2
fi

if [[ -z "$LABEL" ]]; then
    bname="$(basename "$LOG_FILE")"
    LABEL="${bname%.log}"
fi

# 从“段落标题下一行”里取 key=value 的 value。
# 例如：
# select:
#   calls=20 ready=20 timeout=0 ...
extract_key_from_section_next_line() {
    local section_title_regex="$1"
    local key="$2"
    awk -v sec="$section_title_regex" -v key="$key" '
        $0 ~ sec {
            if (getline > 0) {
                for (i = 1; i <= NF; ++i) {
                    if ($i ~ ("^" key "=")) {
                        split($i, a, "=")
                        gsub(/[^0-9.]/, "", a[2])
                        print a[2]
                        exit
                    }
                }
            }
        }
    ' "$LOG_FILE"
}

# active format snapshot 下的下一行。
active_fmt="$(awk '
    /active format snapshot:/ {
        if (getline > 0) {
            gsub(/^ +/, "", $0)
            print $0
            exit
        }
    }
' "$LOG_FILE")"

fps="$(awk '/fps\(actual dq_ok \/ duration\):/ {print $NF; exit}' "$LOG_FILE")"

select_timeout="$(extract_key_from_section_next_line "^select:" "timeout")"
dq_fail="$(extract_key_from_section_next_line "^dqbuf:" "fail")"
dq_eagain="$(extract_key_from_section_next_line "^dqbuf:" "eagain")"
requeue_fail="$(extract_key_from_section_next_line "^qbuf\(requeue\):" "fail")"
requeue_skipped="$(extract_key_from_section_next_line "^qbuf\(requeue\):" "skipped")"

bytes_min="$(awk '
    /^bytesused:/ {
        if (getline > 0) {
            for (i = 1; i <= NF; ++i) {
                if ($i ~ /^min=/) { split($i, a, "="); gsub(/[^0-9.]/, "", a[2]); print a[2]; exit }
            }
        }
    }
' "$LOG_FILE")"

bytes_max="$(awk '
    /^bytesused:/ {
        if (getline > 0) {
            for (i = 1; i <= NF; ++i) {
                if ($i ~ /^max=/) { split($i, a, "="); gsub(/[^0-9.]/, "", a[2]); print a[2]; exit }
            }
        }
    }
' "$LOG_FILE")"

bytes_avg="$(awk '
    /^bytesused:/ {
        if (getline > 0) {
            for (i = 1; i <= NF; ++i) {
                if ($i ~ /^avg=/) { split($i, a, "="); gsub(/[^0-9.]/, "", a[2]); print a[2]; exit }
            }
        }
    }
' "$LOG_FILE")"

dq_interval_avg="$(awk '
    /^dq host interval\(ms\):/ {
        if (getline > 0) {
            for (i = 1; i <= NF; ++i) {
                if ($i ~ /^avg=/) { split($i, a, "="); gsub(/[^0-9.]/, "", a[2]); print a[2]; exit }
            }
        }
    }
' "$LOG_FILE")"

v4l2_interval_avg="$(awk '
    /^v4l2 timestamp interval\(ms\):/ {
        if (getline > 0) {
            for (i = 1; i <= NF; ++i) {
                if ($i ~ /^avg=/) { split($i, a, "="); gsub(/[^0-9.]/, "", a[2]); print a[2]; exit }
            }
        }
    }
' "$LOG_FILE")"

# 统一空值显示，避免表格错位。
normalize_or_na() {
    if [[ -z "$1" ]]; then
        echo "NA"
    else
        echo "$1"
    fi
}

active_fmt="$(normalize_or_na "$active_fmt")"
fps="$(normalize_or_na "$fps")"
select_timeout="$(normalize_or_na "$select_timeout")"
dq_fail="$(normalize_or_na "$dq_fail")"
dq_eagain="$(normalize_or_na "$dq_eagain")"
requeue_fail="$(normalize_or_na "$requeue_fail")"
requeue_skipped="$(normalize_or_na "$requeue_skipped")"
bytes_min="$(normalize_or_na "$bytes_min")"
bytes_max="$(normalize_or_na "$bytes_max")"
bytes_avg="$(normalize_or_na "$bytes_avg")"
dq_interval_avg="$(normalize_or_na "$dq_interval_avg")"
v4l2_interval_avg="$(normalize_or_na "$v4l2_interval_avg")"

if [[ "$MODE" == "csv" ]]; then
    # CSV 中 active_fmt 可能含空格，使用双引号包裹。
    # 同时把双引号转义成两个双引号。
    active_fmt_csv="${active_fmt//\"/\"\"}"
    echo "${LABEL},${fps},${select_timeout},${dq_fail},${dq_eagain},${requeue_fail},${requeue_skipped},${bytes_min},${bytes_max},${bytes_avg},${dq_interval_avg},${v4l2_interval_avg},\"${active_fmt_csv}\""
else
    cat <<OUT
case=${LABEL}
fps=${fps}
select_timeout=${select_timeout}
dq_fail=${dq_fail}
dq_eagain=${dq_eagain}
requeue_fail=${requeue_fail}
requeue_skipped=${requeue_skipped}
bytes_min=${bytes_min}
bytes_max=${bytes_max}
bytes_avg=${bytes_avg}
dq_interval_avg_ms=${dq_interval_avg}
v4l2_interval_avg_ms=${v4l2_interval_avg}
active_fmt=${active_fmt}
OUT
fi
