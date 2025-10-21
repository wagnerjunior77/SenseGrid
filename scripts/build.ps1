# scripts/build.ps1
$ErrorActionPreference = "Stop"
$ROOT = (Get-Location).Path
$IDF_DIR = Join-Path $ROOT "toolchain\esp-idf"
$EXPORT = Join-Path $IDF_DIR "export.ps1"
if (-not (Test-Path $EXPORT)) { throw "export.ps1 nao encontrado. Rode Bootstrap primeiro." }

. $EXPORT
idf.py --version
idf.py set-target esp32s3
if (Test-Path "sdkconfig") { Remove-Item "sdkconfig" -Force }
idf.py build
idf.py size
