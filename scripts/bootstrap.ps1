#Requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Run([string]$exe, [string[]]$argv) {
  $pretty = ($exe + " " + ($argv -join " ")).Trim()
  Write-Host ">>> $pretty"

  # Salva ambiente
  $prevPath = $env:PATH
  $prevCI = $env:CI; $prevGA = $env:GITHUB_ACTIONS; $prevMSYS = $env:MSYSTEM
  try {
    # Remove variáveis que atrapalham subprocessos
    foreach ($k in @("CI","GITHUB_ACTIONS","MSYSTEM")) { Remove-Item "Env:$k" -ErrorAction SilentlyContinue }

    # PATH sem MSYS2 e sem Git\usr\bin
    $env:PATH = ($env:PATH -split ";" | Where-Object {
      $_ -and ($_ -notmatch "\\msys64\\") -and ($_ -notmatch "Git\\usr\\bin")
    }) -join ";"

    # Executa
    & $exe @argv
    $code = $LASTEXITCODE
    if ($code -ne 0) { throw "Falha ao executar: $pretty (exit $code)" }
  }
  finally {
    # Restaura ambiente
    $env:PATH = $prevPath
    if ($null -ne $prevCI)  { $env:CI = $prevCI }            else { Remove-Item Env:CI -ErrorAction SilentlyContinue }
    if ($null -ne $prevGA)  { $env:GITHUB_ACTIONS = $prevGA } else { Remove-Item Env:GITHUB_ACTIONS -ErrorAction SilentlyContinue }
    if ($null -ne $prevMSYS){ $env:MSYSTEM = $prevMSYS }      else { Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue }
  }
  $true
}

# --- Checagens básicas ---
Run "git" @("--version")

function Resolve-Python311 {
  $candidates = @("py","python3","python")
  foreach ($c in $candidates) {
    try {
      $ver = & $c --version 2>$null
      if ($LASTEXITCODE -eq 0 -and $ver -match "Python 3\.11") {
        return (Get-Command $c -ErrorAction Stop).Source
      }
    } catch {}
  }
  $default = "$env:LocalAppData\Programs\Python\Python311\python.exe"
  if (Test-Path $default) { return $default }
  return $null
}

$pyExe = Resolve-Python311
if (-not $pyExe) {
  Write-Host "Python 3.11 nao encontrado."
  Write-Host "Instale: https://www.python.org/downloads/release/python-3119/ (marque Add to PATH) e rode novamente."
  throw "Python 3.11 ausente"
}
Write-Host ">>> Usando Python: $pyExe"

$RepoRoot     = (Resolve-Path "$PSScriptRoot\..").Path
$ToolchainDir = Join-Path $RepoRoot "toolchain"
$VenvDir      = Join-Path $ToolchainDir ".venv"
$IdfDir       = Join-Path $ToolchainDir "esp-idf"

New-Item -ItemType Directory -Force -Path $ToolchainDir | Out-Null

# --- venv ---
if (-not (Test-Path (Join-Path $VenvDir "Scripts\python.exe"))) {
  Write-Host ">>> Criando venv em $VenvDir"
  Run $pyExe @("-m","venv","$VenvDir")
}
$Vpy = (Join-Path $VenvDir "Scripts\python.exe")

# --- ESP-IDF v5.5.1 ---
if (-not (Test-Path (Join-Path $IdfDir "tools\idf_tools.py"))) {
  Write-Host ">>> Clonando esp-idf (v5.5.1) em $IdfDir"
  Run "git" @("clone","--depth","1","--branch","v5.5.1","https://github.com/espressif/esp-idf.git","$IdfDir")
} else {
  Write-Host ">>> esp-idf ja existe: $IdfDir"
}

# --- Requirements mínimos (core) COM CONSTRAINTS ---
$ReqCore  = Join-Path $IdfDir "tools\requirements\requirements.core.txt"
$ConsFile = Join-Path $env:USERPROFILE ".espressif\espidf.constraints.v5.5.txt"

# Se a constraints ainda não existir, gera uma (uma vez) via check-python-dependencies
if (-not (Test-Path $ConsFile)) {
  Run $Vpy @((Join-Path $IdfDir "tools\idf_tools.py"), "check-python-dependencies")
}

if (Test-Path $ConsFile) {
  Run $Vpy @("-m","pip","install","--upgrade","--force-reinstall","-r","$ReqCore","-c","$ConsFile")
} else {
  # Fallback sem constraints (só se a linha acima não criar); export.bat depois avisa e dá o caminho
  Run $Vpy @("-m","pip","install","--upgrade","--force-reinstall","-r","$ReqCore")
}

# --- Mirrors e sanitização do ambiente do IDF ---
$env:IDF_PATH = $IdfDir
# Sem "https://", o idf_tools aceita mirrors:
$env:IDF_GITHUB_ASSETS = "dl.espressif.com/github_assets"

# --- Instala toolchains direto pelo idf_tools.py ---
$IdfTools = Join-Path $IdfDir "tools\idf_tools.py"
Run $Vpy @("$IdfTools","--non-interactive","install","--targets","esp32s3")

Write-Host ">>> PRONTO."
Write-Host ">>> Proximo passo: Tasks -> SenseGrid: Build (local IDF)"
