#!/usr/bin/env bash
set -euo pipefail

APP="./bin/07_streamon_streamoff"
DEV="${DEV:-/dev/video0}"
IN_FOURCC="${IN_FOURCC:-RGB3}"
OUT_FOURCC="${OUT_FOURCC:-RGB3}"
WIDTH="${WIDTH:-64}"
HEIGHT="${HEIGHT:-64}"
OUT_COUNT="${OUT_COUNT:-2}"
CAP_COUNT="${CAP_COUNT:-2}"
MPLANE="${MPLANE:-0}"

"${APP}" \
  --dev="${DEV}" \
  --in-fourcc="${IN_FOURCC}" \
  --out-fourcc="${OUT_FOURCC}" \
  --width="${WIDTH}" \
  --height="${HEIGHT}" \
  --out-count="${OUT_COUNT}" \
  --cap-count="${CAP_COUNT}" \
  --mplane="${MPLANE}" \
  --verbose
