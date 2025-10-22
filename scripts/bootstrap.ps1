<#  SenseGrid bootstrap (Windows)
    - detecta Python sem quebrar
    - instala Python se faltar (winget/choco)
    - cria venv em toolchain\.venv
    - clona esp-idf v5.5.1 em toolchain\esp-idf
    - instala toolchains e deps
#>

param([string]$IdfTag = "v5.5.1")
$ErrorActionPreference = "Stop"

function Get-Python {
  # tenta localizar um executavel de Python sem gerar erro
  if (Get-Command py -ErrorAction SilentlyContinue)      { return @{Exe="py";      Pre=@("-3")} }
  if (Get-Command python3 -ErrorAction SilentlyContinue) { return @{Exe="python3"; Pre=@()}   }
  if (Get-Command python -ErrorAction SilentlyContinue)  { return @{Exe="python";  Pre=@()}   }
  return @{Exe=$null; Pre=@()}
}

function Install-Python {
  Write-Host ">>> Python nao encontrado. Tentando instalar..."
  if (Get-Command winget -ErrorAction SilentlyContinue) {
    winget install -e --id Python.Python.3.11 --silent `
      --accept-source-agreements --accept-package-agreements
    return
  }
  if (Get-Command choco -ErrorAction SilentlyContinue) {
    choco install python --version=3.11.9 -y
    return
  }
  throw "Python nao encontrado e nao foi possivel instalar automaticamente (sem winget/choco). Instale manualmente Python 3.10+ e rode novamente."
}

function Ensure-Git {
  if (Get-Command git -ErrorAction SilentlyContinue) { return }
  Write-Host ">>> Git nao encontrado. Instalando..."
  if (Get-Command winget -ErrorAction SilentlyContinue) {
    winget install -e --id Git.Git --silent `
      --accept-source-agreements --accept-package-agreements
    return
  }
  if (Get-Command choco -ErrorAction SilentlyContinue) {
    choco install git -y
    return
  }
  throw "Git nao encontrado e sem instalador automatico. Instale o Git e rode de novo."
}

# --- paths
$Root         = (Resolve-Path -LiteralPath "$PSScriptRoot\..").Path
$ToolchainDir = Join-Path $Root "toolchain"
$VenvDir      = Join-Path $ToolchainDir ".venv"
$IdfDir       = Join-Path $ToolchainDir "esp-idf"
New-Item -ItemType Directory -Force -Path $ToolchainDir | Out-Null

# --- Python
$py = Get-Python
if (-not $py.Exe) {
  Install-Python
  $py = Get-Python
  if (-not $py.Exe) { throw "Python ainda indisponivel apos instalacao." }
}
Write-Host ">>> Usando Python: $($py.Exe) $($py.Pre -join ' ')"

# helper pra chamar python com prefixo (py -3 ...)
function Invoke-Py { param([Parameter(ValueFromRemainingArguments=$true)][string[]]$Args)
  & $py.Exe @($py.Pre) @Args
}

# garante pip/venv
Invoke-Py -m ensurepip --upgrade | Out-Null
Invoke-Py -m pip install --upgrade pip virtualenv | Out-Null

if (-not (Test-Path $VenvDir)) {
  Write-Host ">>> Criando venv em $VenvDir"
  Invoke-Py -m venv $VenvDir
}

$Vpy  = Join-Path $VenvDir "Scripts\python.exe"

# --- Git e ESP-IDF
Ensure-Git
if (-not (Test-Path $IdfDir)) {
  Write-Host ">>> Clonando esp-idf ($IdfTag) em $IdfDir"
  git clone --depth 1 --branch $IdfTag https://github.com/espressif/esp-idf.git $IdfDir
}

Write-Host ">>> Instalando requirements do esp-idf"
& $Vpy -m pip install --upgrade -r (Join-Path $IdfDir "requirements.txt")

Write-Host ">>> Instalando toolchains (idf_tools.py) — pode demorar"
& $Vpy (Join-Path $IdfDir "tools\idf_tools.py") --non-interactive install required

# --- smoke test do idf.py
$export = ". `"$IdfDir\export.ps1`""
Write-Host ">>> Testando idf.py --version"
powershell -NoProfile -Command "$export; idf.py --version"

Write-Host "`nOK! Ambiente pronto."
Write-Host "Proximo passo: Tasks → SenseGrid: Build (local toolchain)"
