#!/usr/bin/env bash
set -euo pipefail

APP="./bin/04_try_set_format"
DEV="${DEV:-/dev/video0}"
IN_FOURCC="${IN_FOURCC:-RGB3}"
OUT_FOURCC="${OUT_FOURCC:-RGB3}"
WIDTH="${WIDTH:-64}"
HEIGHT="${HEIGHT:-64}"
MPLANE="${MPLANE:-0}"

"${APP}" \
  --dev="${DEV}" \
  --in-fourcc="${IN_FOURCC}" \
  --out-fourcc="${OUT_FOURCC}" \
  --width="${WIDTH}" \
  --height="${HEIGHT}" \
  --mplane="${MPLANE}" \
  --verbose
