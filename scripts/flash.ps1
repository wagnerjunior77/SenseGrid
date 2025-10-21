param([string]$Port="COM5",[string]$Baud="460800")
$ErrorActionPreference = "Stop"
$ROOT = (Get-Location).Path
$IDF_DIR = Join-Path $ROOT "toolchain\esp-idf"
$EXPORT = Join-Path $IDF_DIR "export.ps1"
if (-not (Test-Path $EXPORT)) { throw "export.ps1 nao encontrado. Rode Bootstrap primeiro." }
. $EXPORT
idf.py set-target esp32s3
idf.py -p $Port -b [int]$Baud flash monitor
