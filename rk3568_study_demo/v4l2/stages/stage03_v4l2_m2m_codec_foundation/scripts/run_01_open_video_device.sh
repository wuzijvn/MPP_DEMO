#!/usr/bin/env bash
set -euo pipefail
./bin/01_open_video_device --dev="${DEV:-/dev/video0}" --verbose
