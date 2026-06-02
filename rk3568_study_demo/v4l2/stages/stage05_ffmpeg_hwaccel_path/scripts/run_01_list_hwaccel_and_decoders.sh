#!/usr/bin/env bash
set -euo pipefail
./bin/01_list_hwaccel_and_decoders --decoder="${DECODER:-h264_rkmpp}"
