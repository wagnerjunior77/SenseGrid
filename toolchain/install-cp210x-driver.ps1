param(
  [string]$RepoRoot = (Resolve-Path "$PSScriptRoot\..").Path
)

$ErrorActionPreference = "Stop"

$infPath = Join-Path $RepoRoot "driver\silabser.inf"
if (-not (Test-Path $infPath)) {
  throw "Driver INF not found at: $infPath"
}

$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
  [Security.Principal.WindowsBuiltInRole]::Administrator
)
if (-not $isAdmin) {
  Write-Warning "Driver install may need admin rights. If it fails, reopen VS Code as Administrator and rerun."
}

function Test-Cp210xInstalled {
  try {
    $out = pnputil /enum-drivers
    return ($out -match "silabser\.inf") -or ($out -match "CP210x")
  } catch {
    Write-Warning "Could not enumerate existing drivers: $_"
    return $false
  }
}

if (Test-Cp210xInstalled) {
  Write-Host "CP210x driver already present. Skipping install."
  exit 0
}

Write-Host "Installing CP210x driver from: $infPath"
$proc = Start-Process -FilePath "pnputil.exe" -ArgumentList @("/add-driver", $infPath, "/install", "/subdirs") `
  -Wait -PassThru -NoNewWindow

if ($proc.ExitCode -ne 0) {
  throw "pnputil exited with code $($proc.ExitCode). Run as admin and try again."
}

if (Test-Cp210xInstalled) {
  Write-Host "OK: CP210x driver installed."
} else {
  Write-Warning "pnputil reported success but driver is not detected. Check Device Manager."
}
