#!/usr/bin/env bash
set -euo pipefail
IDF_TAG="v5.5.1"
ROOT="$(pwd)"
TC_DIR="$ROOT/toolchain"
IDF_DIR="$TC_DIR/esp-idf"
PY_DIR="$TC_DIR/py"

command -v git >/dev/null || { echo "git nao encontrado"; exit 1; }
command -v python3 >/dev/null || { echo "python3 nao encontrado"; exit 1; }

mkdir -p "$TC_DIR"
[ -d "$IDF_DIR" ] || git clone --depth 1 --branch "$IDF_TAG" https://github.com/espressif/esp-idf.git "$IDF_DIR"

[ -d "$PY_DIR" ] || python3 -m venv "$PY_DIR"
# shellcheck disable=SC1091
source "$PY_DIR/bin/activate"

pip install --upgrade pip wheel setuptools
pip install -r "$IDF_DIR/requirements.txt"

python "$IDF_DIR/tools/idf_tools.py" --non-interactive install --targets esp32s3
python "$IDF_DIR/tools/idf_tools.py" --non-interactive install-python-env
echo "OK. Rode a task: SenseGrid: Build (local toolchain)"
