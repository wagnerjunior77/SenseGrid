#!/usr/bin/env bash
set -euo pipefail
PORT="${1:-/dev/ttyUSB0}"
BAUD="${2:-460800}"
ROOT="$(pwd)"
IDF_DIR="$ROOT/toolchain/esp-idf"
# shellcheck disable=SC1091
source "$IDF_DIR/export.sh"
idf.py set-target esp32s3
idf.py -p "$PORT" -b "$BAUD" flash monitor
