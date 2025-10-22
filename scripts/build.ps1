param([string]$Target="esp32s3")
$ErrorActionPreference = "Stop"

$Root      = (Resolve-Path -LiteralPath "$PSScriptRoot\..").Path
$Toolchain = Join-Path $Root "toolchain"
$EnvHint   = Join-Path $Toolchain "env_path.ps1"
if (Test-Path $EnvHint) { . $EnvHint }
if (-not $env:SENSEGRID_VENV) { $env:SENSEGRID_VENV = (Join-Path $Toolchain ".venv") }

$Idf = Join-Path $Toolchain "esp-idf"

# Build usando o ambiente do esp-idf exportado
$cmd = "& { . '$Idf\export.ps1'; idf.py --version; idf.py set-target $Target; Remove-Item -Force -ErrorAction SilentlyContinue sdkconfig; idf.py build }"
powershell -NoProfile -ExecutionPolicy Bypass -Command $cmd
