<#  SenseGrid bootstrap (Windows)
    - instala Python se faltar (winget/choco)
    - cria venv em toolchain\.venv
    - clona esp-idf v5.5.1 em toolchain\esp-idf
    - instala toolchains e deps
#>

param(
  [string]$IdfTag = "v5.5.1"
)

$ErrorActionPreference = "Stop"

function Find-Python {
  Write-Host ">>> Procurando Python..."
  $candidates = @(
    { & py -3 --version 2>$null; if ($LASTEXITCODE -eq 0) { return "py -3" } },
    { & python3 --version 2>$null; if ($LASTEXITCODE -eq 0) { return "python3" } },
    { & python --version 2>$null; if ($LASTEXITCODE -eq 0) { return "python" } }
  )
  foreach ($probe in $candidates) {
    $cmd = & $probe
    if ($cmd) { return $cmd }
  }
  return $null
}

function Install-Python {
  Write-Host ">>> Python nao encontrado. Tentando instalar..."
  # tenta winget
  if (Get-Command winget -ErrorAction SilentlyContinue) {
    winget install -e --id Python.Python.3.11 --accept-source-agreements --accept-package-agreements
    return
  }
  # tenta chocolatey
  if (Get-Command choco -ErrorAction SilentlyContinue) {
    choco install python --version=3.11.9 -y
    return
  }
  throw "Python nao encontrado e nao foi possivel instalar automaticamente (sem winget/choco). Instale manualmente o Python 3.10+ e rode o bootstrap novamente."
}

$Root = (Resolve-Path -LiteralPath "$PSScriptRoot\..").Path
$ToolchainDir = Join-Path $Root "toolchain"
$VenvDir = Join-Path $ToolchainDir ".venv"
$IdfDir = Join-Path $ToolchainDir "esp-idf"

New-Item -ItemType Directory -Force -Path $ToolchainDir | Out-Null

# 1) Python + pip + venv
$py = Find-Python
if (-not $py) {
  Install-Python
  $py = Find-Python
}
Write-Host ">>> Usando Python: $py"

# garante pip
& $py -m ensurepip --upgrade 2>$null | Out-Null
& $py -m pip install --upgrade pip virtualenv

# cria venv (se nao existir)
if (-not (Test-Path $VenvDir)) {
  Write-Host ">>> Criando venv em $VenvDir"
  & $py -m venv $VenvDir
}

$Vpy = Join-Path $VenvDir "Scripts\python.exe"
$Vpip = "$Vpy -m pip"

# 2) ESP-IDF
if (-not (Test-Path $IdfDir)) {
  Write-Host ">>> Clonando esp-idf ($IdfTag) em $IdfDir"
  git clone --depth 1 --branch $IdfTag https://github.com/espressif/esp-idf.git $IdfDir
}

# 3) Requisitos Python do IDF
Write-Host ">>> Instalando requirements.txt"
& $Vpy -m pip install --upgrade -r (Join-Path $IdfDir "requirements.txt")

# 4) Toolchains (compiladores/SDK)
Write-Host ">>> Instalando toolchains (idf_tools.py)... isso pode demorar"
& $Vpy (Join-Path $IdfDir "tools\idf_tools.py") --non-interactive install required

# 5) Teste idf.py
$export = ". `"$IdfDir\export.ps1`""
Write-Host ">>> Testando idf.py --version"
powershell -NoProfile -Command "$export; idf.py --version"

Write-Host "`nOK! Ambiente pronto."
Write-Host "Proximo passo: Tasks → SenseGrid: Build (local toolchain)"
