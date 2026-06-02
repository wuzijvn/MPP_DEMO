#!/usr/bin/env bash
set -euo pipefail
./bin/02_querycap_enum_formats --dev="${DEV:-/dev/video0}" --mplane="${MPLANE:-0}" --verbose
