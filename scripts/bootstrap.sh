#!/usr/bin/env bash
# SenseGrid bootstrap (Linux/macOS)
set -euo pipefail

IDF_TAG="${1:-v5.5.1}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOLCHAIN_DIR="$ROOT/toolchain"
VENV_DIR="$TOOLCHAIN_DIR/.venv"
IDF_DIR="$TOOLCHAIN_DIR/esp-idf"

mkdir -p "$TOOLCHAIN_DIR"

need_cmd() {
  command -v "$1" >/dev/null 2>&1
}

install_python() {
  echo ">>> Python nao encontrado. Tentando instalar..."
  if need_cmd apt-get; then
    sudo apt-get update && sudo apt-get install -y python3 python3-venv python3-pip git
  elif need_cmd dnf; then
    sudo dnf install -y python3 python3-venv python3-pip git
  elif need_cmd pacman; then
    sudo pacman -Sy --noconfirm python python-virtualenv python-pip git
  elif need_cmd brew; then
    brew install python@3.11 git
  else
    echo "Python nao encontrado e sem gerenciador suportado. Instale Python 3.10+ manualmente e rode de novo."
    exit 1
  fi
}

# 1) Python + venv
if ! need_cmd python3; then
  install_python
fi

python3 -m ensurepip --upgrade || true
python3 -m pip install --upgrade pip virtualenv

if [ ! -d "$VENV_DIR" ]; then
  echo ">>> Criando venv em $VENV_DIR"
  python3 -m venv "$VENV_DIR"
fi

# shellcheck disable=SC1090
source "$VENV_DIR/bin/activate"

# 2) ESP-IDF
if [ ! -d "$IDF_DIR" ]; then
  echo ">>> Clonando esp-idf ($IDF_TAG) em $IDF_DIR"
  git clone --depth 1 --branch "$IDF_TAG" https://github.com/espressif/esp-idf.git "$IDF_DIR"
fi

# 3) Requisitos
pip install --upgrade -r "$IDF_DIR/requirements.txt"

# 4) Toolchains
python "$IDF_DIR/tools/idf_tools.py" --non-interactive install required

# 5) Teste idf.py
# shellcheck disable=SC1091
. "$IDF_DIR/export.sh"
idf.py --version

echo
echo "OK! Ambiente pronto."
echo "Proximo passo: Tasks -> SenseGrid: Build (local toolchain)"
