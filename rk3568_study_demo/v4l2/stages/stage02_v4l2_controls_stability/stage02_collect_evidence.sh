#!/usr/bin/env bash
# Stage02 evidence collector
#
# 目的：
# 1) 采集用户态 summary + v4l2-ctl 信息 + dmesg 片段；
# 2) 形成可复盘证据包，便于做“用户态现象 <-> 驱动线索”映射。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="${SCRIPT_DIR}/bin/stage02_v4l2_controls_stability_main"

DEV="/dev/video10"
OUT_DIR="${SCRIPT_DIR}/outputs_stage02_evidence"
DURATION=30
EXTRA_ARGS=()

usage() {
  cat <<USAGE
Usage:
  $0 [options] [-- extra stage02 args]

Options:
  --dev=PATH          default: ${DEV}
  --duration-sec=N    default: ${DURATION}
  --out-dir=DIR       default: ${OUT_DIR}
  -h, --help

Examples:
  $0 --dev=/dev/video10 --duration-sec=30
  $0 --dev=/dev/video10 -- --queue-depth=8 --queue-policy=block --writer-delay-ms=20
USAGE
}

PASS_EXTRA=0
for a in "$@"; do
  if [[ "$PASS_EXTRA" -eq 1 ]]; then
    EXTRA_ARGS+=("$a")
    continue
  fi
  case "$a" in
    --) PASS_EXTRA=1 ;;
    --dev=*) DEV="${a#*=}" ;;
    --duration-sec=*) DURATION="${a#*=}" ;;
    --out-dir=*) OUT_DIR="${a#*=}" ;;
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

if [[ ! -x "$BIN" ]]; then
  echo "binary not found: $BIN" >&2
  echo "hint: ./build.sh stage02_v4l2_controls_stability_main" >&2
  exit 2
fi

TAG="$(date -u +%Y%m%d_%H%M%S)"
RUN_DIR="${OUT_DIR}/evidence_${TAG}"
mkdir -p "$RUN_DIR"

INFO_TXT="${RUN_DIR}/env_info.txt"
V4L2_ALL="${RUN_DIR}/v4l2_all.txt"
V4L2_FMT="${RUN_DIR}/v4l2_formats.txt"
V4L2_CTRLS="${RUN_DIR}/v4l2_ctrls.txt"
DMESG_BEFORE="${RUN_DIR}/dmesg_before.txt"
DMESG_AFTER="${RUN_DIR}/dmesg_after.txt"
RUN_LOG="${RUN_DIR}/stage02_run.log"
SUMMARY_TXT="${RUN_DIR}/summary_extract.txt"

{
  echo "date_utc=$(date -u '+%Y-%m-%d %H:%M:%S')"
  echo "dev=${DEV}"
  uname -a | sed 's/^/uname=/'
} > "$INFO_TXT"

if command -v v4l2-ctl >/dev/null 2>&1; then
  v4l2-ctl -d "$DEV" --all > "$V4L2_ALL" 2>&1 || true
  v4l2-ctl -d "$DEV" --list-formats-ext > "$V4L2_FMT" 2>&1 || true
  v4l2-ctl -d "$DEV" -L > "$V4L2_CTRLS" 2>&1 || true
else
  echo "v4l2-ctl not found" > "$V4L2_ALL"
fi

dmesg | tail -n 300 > "$DMESG_BEFORE" || true

CMD=("$BIN" "$DEV" 640 480 "--duration-sec=${DURATION}" "--dump-every=0" "--no-save")
CMD+=("${EXTRA_ARGS[@]}")

{
  echo "cmd: ${CMD[*]}"
  echo
  "${CMD[@]}"
} > "$RUN_LOG" 2>&1 || true

dmesg | tail -n 300 > "$DMESG_AFTER" || true

awk '
  /================ stage02 summary ================/ {p=1}
  p==1 {print}
  /===============================================/ {if (p==1) exit}
' "$RUN_LOG" > "$SUMMARY_TXT"

echo "[done] evidence dir: $RUN_DIR"
echo "[done] run log     : $RUN_LOG"
echo "[done] summary     : $SUMMARY_TXT"
