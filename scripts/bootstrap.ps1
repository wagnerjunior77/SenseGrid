# scripts/bootstrap.ps1
$ErrorActionPreference = "Stop"
$IDF_TAG = "v5.5.1"
$ROOT = (Get-Location).Path
$TC_DIR = Join-Path $ROOT "toolchain"
$IDF_DIR = Join-Path $TC_DIR "esp-idf"
$PY_DIR = Join-Path $TC_DIR "py"

Write-Host "==> SenseGrid bootstrap (Windows) - IDF $IDF_TAG" -ForegroundColor Cyan
if (-not (Get-Command python -ErrorAction SilentlyContinue)) { throw "Python 3 nao encontrado no PATH" }
if (-not (Get-Command git -ErrorAction SilentlyContinue))    { throw "git nao encontrado no PATH" }

New-Item -ItemType Directory -Force $TC_DIR | Out-Null

if (-not (Test-Path $IDF_DIR)) {
  Write-Host "Clonando esp-idf $IDF_TAG..."
  git clone --depth 1 --branch $IDF_TAG https://github.com/espressif/esp-idf.git $IDF_DIR
}

if (-not (Test-Path $PY_DIR)) {
  Write-Host "Criando venv Python..."
  python -m venv $PY_DIR
}
$env:VIRTUAL_ENV = $PY_DIR
$env:Path = "$PY_DIR\Scripts;$env:Path"

pip install --upgrade pip wheel setuptools
pip install -r (Join-Path $IDF_DIR "requirements.txt")

Write-Host "Instalando toolchains (esp32s3)..."
python (Join-Path $IDF_DIR "tools\idf_tools.py") --non-interactive install --targets esp32s3
python (Join-Path $IDF_DIR "tools\idf_tools.py") --non-interactive install-python-env

Write-Host "OK. Rode a task: SenseGrid: Build (local toolchain)" -ForegroundColor Green
