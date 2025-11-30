param(
  [string]$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path
)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$staging = Join-Path $RepoRoot "toolchain/arduino-data/staging/packages"
New-Item -ItemType Directory -Force -Path $staging | Out-Null

$tools = @(
  @{
    Name    = "mdns-discovery";
    Archive = "mdns-discovery_v1.0.12_Windows_64bit.zip";
    Bundle  = Join-Path $RepoRoot "toolchain/bundled-tools/mdns-discovery_v1.0.12_Windows_64bit.zip";
    Urls    = @(
      "https://downloads.arduino.cc/discovery/mdns-discovery/mdns-discovery_v1.0.12_Windows_64bit.zip"
    );
  }
)

function Get-WithRetry([string]$url, [string]$dest) {
  $max = 3
  for ($i = 1; $i -le $max; $i++) {
    try {
      Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $dest -TimeoutSec 600
      return $true
    } catch {
      Write-Warning ("Tentativa {0} falhou para {1} - {2}" -f $i, $url, $_.Exception.Message)
      Start-Sleep -Seconds ([Math]::Min(5*$i, 20))
    }
  }
  return $false
}

foreach ($t in $tools) {
  $dest = Join-Path $staging $t.Archive
  if (Test-Path $dest) {
    Write-Host "$($t.Name) ja esta em cache: $dest"
    continue
  }

  if ($t.Bundle -and (Test-Path $t.Bundle)) {
    Write-Host "Copiando bundle local para $dest"
    Copy-Item -Force -Path $t.Bundle -Destination $dest
    continue
  }

  $done = $false
  foreach ($url in $t.Urls) {
    Write-Host "Baixando $($t.Name) de $url ..."
    if (Get-WithRetry $url $dest) { $done = $true; break }
  }

  if (-not $done) {
    throw "Falha ao obter $($t.Name). Verifique conectividade ou baixe manualmente para $dest"
  }
}

Write-Host "OK: ferramentas de discovery prontas em $staging"
