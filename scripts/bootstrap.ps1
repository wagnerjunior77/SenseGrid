#Requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Run([string]$exe, [string[]]$argv) {
  $pretty = ($exe + " " + ($argv -join " ")).Trim()
  Write-Host ">>> $pretty"
  $prevPath=$env:PATH; $prevCI=$env:CI; $prevGA=$env:GITHUB_ACTIONS; $prevMSYS=$env:MSYSTEM
  try {
    foreach ($k in @("CI","GITHUB_ACTIONS","MSYSTEM")) { Remove-Item "Env:$k" -ErrorAction SilentlyContinue }
    $env:PATH = ($env:PATH -split ";" | Where-Object { $_ -and ($_ -notmatch "\\msys64\\") -and ($_ -notmatch "Git\\usr\\bin") }) -join ";"
    & $exe @argv
    if ($LASTEXITCODE -ne 0) { throw "Falha ao executar: $pretty (exit $LASTEXITCODE)" }
  } finally {
    $env:PATH=$prevPath
    if ($null -ne $prevCI)   { $env:CI=$prevCI }             else { Remove-Item Env:CI -ErrorAction SilentlyContinue }
    if ($null -ne $prevGA)   { $env:GITHUB_ACTIONS=$prevGA } else { Remove-Item Env:GITHUB_ACTIONS -ErrorAction SilentlyContinue }
    if ($null -ne $prevMSYS) { $env:MSYSTEM=$prevMSYS }      else { Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue }
  }
  $true
}

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$RepoRoot     = (Resolve-Path "$PSScriptRoot\..").Path
$ToolchainDir = Join-Path $RepoRoot "toolchain"
$LocalPyDir   = Join-Path $ToolchainDir "py311"
$LocalPyExe   = Join-Path $LocalPyDir "python.exe"
$VenvDir      = Join-Path $ToolchainDir ".venv"
$IdfDir       = Join-Path $ToolchainDir "esp-idf"

New-Item -ItemType Directory -Force -Path $ToolchainDir | Out-Null

Run "git" @("--version")

function Resolve-Python311 {
  $candidates = @($LocalPyExe,"py","python3","python")
  foreach ($c in $candidates) {
    try { $ver = & $c --version 2>$null; if ($LASTEXITCODE -eq 0 -and $ver -match "Python 3\.11") { return (Get-Command $c -ErrorAction Stop).Source } } catch {}
  }
  $default = "$env:LocalAppData\Programs\Python\Python311\python.exe"
  if (Test-Path $default) { return $default }
  return $null
}

function Install-Python311-Embedded {
  $zipUrl = "https://www.python.org/ftp/python/3.11.9/python-3.11.9-embed-amd64.zip"
  $zipPath = Join-Path $ToolchainDir "python-3.11.9-embed-amd64.zip"
  Write-Host ">>> Baixando Python 3.11 (embeddable)..."
  Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath
  if (Test-Path $LocalPyDir) { Remove-Item $LocalPyDir -Recurse -Force -ErrorAction SilentlyContinue }
  New-Item -ItemType Directory -Force -Path $LocalPyDir | Out-Null
  Expand-Archive -Path $zipPath -DestinationPath $LocalPyDir -Force
  Remove-Item $zipPath -Force
  $pth = Join-Path $LocalPyDir "python311._pth"
  if (Test-Path $pth) {
    $content = Get-Content $pth -Raw
    if ($content -notmatch "import site") { Add-Content -Path $pth -Value "import site" }
  }
  $gp = Join-Path $LocalPyDir "get-pip.py"
  Write-Host ">>> Instalando pip no Python embutido..."
  Invoke-WebRequest -Uri "https://bootstrap.pypa.io/get-pip.py" -OutFile $gp
  & $LocalPyExe $gp
  if ($LASTEXITCODE -ne 0) { throw "Falha ao instalar pip no Python embutido (exit $LASTEXITCODE)" }
  & $LocalPyExe -m pip install --upgrade pip
  return $LocalPyExe
}

function Ensure-Python311 {
  $py = Resolve-Python311
  if ($py) { return $py }
  try {
    $url = "https://www.python.org/ftp/python/3.11.9/python-3.11.9-amd64.exe"
    $installer = Join-Path $ToolchainDir "python-3.11.9-amd64.exe"
    Write-Host ">>> Baixando Python 3.11.9..."
    Invoke-WebRequest -Uri $url -OutFile $installer
    Write-Host ">>> Instalando Python 3.11.9 local em $LocalPyDir"
    New-Item -ItemType Directory -Force -Path $LocalPyDir | Out-Null
    & $installer /quiet InstallAllUsers=0 PrependPath=0 Include_pip=1 Include_test=0 Include_launcher=0 Shortcuts=0 AssociateFiles=0 SimpleInstall=1 TargetDir="$LocalPyDir"
    $code = $LASTEXITCODE
    Remove-Item $installer -Force -ErrorAction SilentlyContinue
    if ($code -eq 0 -and (Test-Path $LocalPyExe)) { return $LocalPyExe }
    Write-Host ">>> Instalador .exe falhou (exit $code). Usando pacote embutido..."
    return Install-Python311-Embedded
  } catch {
    Write-Host ">>> Erro com instalador .exe: $_"
    Write-Host ">>> Usando pacote embutido..."
    return Install-Python311-Embedded
  }
}

$pyExe = Ensure-Python311
Write-Host ">>> Usando Python: $pyExe"

# venv
if (-not (Test-Path (Join-Path $VenvDir "Scripts\python.exe"))) {
  Write-Host ">>> Criando venv em $VenvDir"
  try { Run $pyExe @("-m","venv","$VenvDir") }
  catch {
    Write-Host ">>> venv nativo indisponível; usando virtualenv"
    Run $pyExe @("-m","pip","install","--upgrade","virtualenv")
    Run $pyExe @("-m","virtualenv","$VenvDir")
  }
}
$Vpy = (Join-Path $VenvDir "Scripts\python.exe")

# ESP-IDF v5.5.1
if (-not (Test-Path (Join-Path $IdfDir "tools\idf_tools.py"))) {
  Write-Host ">>> Clonando esp-idf (v5.5.1) em $IdfDir"
  Run "git" @("clone","--depth","1","--branch","v5.5.1","https://github.com/espressif/esp-idf.git","$IdfDir")
} else {
  Write-Host ">>> esp-idf ja existe: $IdfDir"
}

# Variáveis de ambiente do IDF (antes de chamar idf_tools.py)
$env:IDF_PATH = $IdfDir
$env:IDF_PYTHON_ENV_PATH = $VenvDir
$env:IDF_GITHUB_ASSETS = "dl.espressif.com/github_assets"

$IdfTools = Join-Path $IdfDir "tools\idf_tools.py"
$ReqCore  = Join-Path $IdfDir "tools\requirements\requirements.core.txt"
$ConsFile = Join-Path $env:USERPROFILE ".espressif\espidf.constraints.v5.5.txt"

# >>> Instala as dependências do IDF DENTRO do seu venv
$installed = $false
try {
  Run $Vpy @("$IdfTools","install-python-env","--python",$Vpy)
  $installed = $true
} catch {
  Write-Host ">>> install-python-env falhou; tentando pip direto..."
}

if (-not $installed) {
  if (-not (Test-Path $ConsFile)) {
    # tenta gerar constraints (agora que IDF_PYTHON_ENV_PATH está setado)
    try { Run $Vpy @("$IdfTools","check-python-dependencies") } catch {}
  }
  if (Test-Path $ConsFile) {
    Run $Vpy @("-m","pip","install","--upgrade","--force-reinstall","-r",$ReqCore,"-c",$ConsFile)
  } else {
    Run $Vpy @("-m","pip","install","--upgrade","--force-reinstall","-r",$ReqCore)
  }
}

# Toolchains para ESP32-S3
Run $Vpy @("$IdfTools","--non-interactive","install","--targets","esp32s3")

Write-Host ">>> PRONTO."
Write-Host ">>> Proximo passo: Tasks -> SenseGrid: Build (local IDF)"
