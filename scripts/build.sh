#!/usr/bin/env bash
set -euo pipefail
ROOT="$(pwd)"
IDF_DIR="$ROOT/toolchain/esp-idf"
# shellcheck disable=SC1091
source "$IDF_DIR/export.sh"
idf.py --version
idf.py set-target esp32s3
rm -f sdkconfig
idf.py build
idf.py size
