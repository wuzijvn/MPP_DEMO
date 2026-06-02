#!/usr/bin/env bash
set -euo pipefail

APP="./bin/06_qbuf_dqbuf_ownership"
DEV="${DEV:-/dev/video0}"
IN_FOURCC="${IN_FOURCC:-RGB3}"
OUT_FOURCC="${OUT_FOURCC:-RGB3}"
WIDTH="${WIDTH:-64}"
HEIGHT="${HEIGHT:-64}"
OUT_COUNT="${OUT_COUNT:-2}"
CAP_COUNT="${CAP_COUNT:-2}"
MAX_LOOPS="${MAX_LOOPS:-4}"
TIMEOUT_MS="${TIMEOUT_MS:-300}"
MPLANE="${MPLANE:-0}"

"${APP}" \
  --dev="${DEV}" \
  --in-fourcc="${IN_FOURCC}" \
  --out-fourcc="${OUT_FOURCC}" \
  --width="${WIDTH}" \
  --height="${HEIGHT}" \
  --out-count="${OUT_COUNT}" \
  --cap-count="${CAP_COUNT}" \
  --max-loops="${MAX_LOOPS}" \
  --timeout-ms="${TIMEOUT_MS}" \
  --mplane="${MPLANE}" \
  --verbose
