Param(
  [string]$OutDir = "$(Join-Path $PSScriptRoot '.')"
)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$dest = Join-Path $PSScriptRoot "arduino-cli.exe"
if (Test-Path $dest) {
  Write-Host "arduino-cli.exe já existe em $dest"
  & $dest version
  exit 0
}

$apiUrl = "https://api.github.com/repos/arduino/arduino-cli/releases/latest"
Write-Host "Consultando $apiUrl ..."
$release = Invoke-RestMethod -UseBasicParsing -Uri $apiUrl

$asset = $release.assets | Where-Object { $_.name -match 'Windows_64bit\.zip$' } | Select-Object -First 1
if (-not $asset) {
  # fallback mais amplo
  $asset = $release.assets | Where-Object { $_.name -match 'Windows.*64.*\.zip$' } | Select-Object -First 1
}
if (-not $asset) {
  throw "Não encontrei asset Windows 64-bit no release mais recente."
}

$zipPath = Join-Path $PSScriptRoot "arduino-cli-win64.zip"
Write-Host "Baixando: $($asset.name)"
Invoke-WebRequest -UseBasicParsing -Uri $asset.browser_download_url -OutFile $zipPath

Write-Host "Extraindo para $PSScriptRoot ..."
Expand-Archive -Path $zipPath -DestinationPath $PSScriptRoot -Force

# Algumas versões extraem direto o exe na raiz; se vier em subpasta, mova
$exe = Get-ChildItem -Path $PSScriptRoot -Recurse -Filter "arduino-cli.exe" | Select-Object -First 1
if (-not $exe) {
  throw "arduino-cli.exe não encontrado após extração."
}

if ($exe.FullName -ne $dest) {
  Move-Item -Force -Path $exe.FullName -Destination $dest
}

Remove-Item -Force $zipPath -ErrorAction SilentlyContinue
Write-Host "OK: arduino-cli.exe instalado em $dest"
& $dest version
