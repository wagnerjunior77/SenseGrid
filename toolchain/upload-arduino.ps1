param(
  [string]$CliPath,
  [string]$YamlPath,
  [string]$Fqbn,
  [string]$BuildPath = "$PSScriptRoot\\build\\SenseGrid",
  [string]$Port = "AUTO",
  [int]$DiscoveryTimeoutSec = 4
)

$ErrorActionPreference = "Stop"

function Test-RequiredPath {
  param([string]$Path, [string]$Label)
  if (-not $Path) { throw "$Label not provided." }
  if (-not (Test-Path -LiteralPath $Path)) { throw "$Label not found: $Path" }
  return (Resolve-Path -LiteralPath $Path).Path
}

function Is-AutoPort {
  param([string]$Value)
  if (-not $Value) { return $true }
  $v = $Value.Trim()
  if ($v -eq "") { return $true }
  return ($v.ToUpperInvariant() -eq "AUTO")
}

function Get-DetectedPorts {
  param([string]$CliPath, [string]$YamlPath, [int]$TimeoutSec)
  $timeout = "${TimeoutSec}s"
  $json = & $CliPath board list --json --config-file $YamlPath --discovery-timeout $timeout
  if (-not $json) { return @() }
  $data = $json | ConvertFrom-Json
  if (-not $data) { return @() }
  if (-not $data.detected_ports) { return @() }
  return @($data.detected_ports)
}

function Resolve-Port {
  param(
    [string]$CliPath,
    [string]$YamlPath,
    [string]$Fqbn,
    [string]$PortHint,
    [int]$TimeoutSec
  )

  if (-not (Is-AutoPort $PortHint)) { return $PortHint }
  if ($env:SENSEGRID_PORT -and $env:SENSEGRID_PORT.Trim() -ne "") {
    return $env:SENSEGRID_PORT.Trim()
  }

  $ports = Get-DetectedPorts -CliPath $CliPath -YamlPath $YamlPath -TimeoutSec $TimeoutSec
  if (-not $ports -or @($ports).Count -eq 0) {
    throw "No serial ports detected. Set SENSEGRID_PORT or sensegrid.serialPort."
  }

  $candidates = $ports

  if ($Fqbn) {
    $fqbnMatches = $ports | Where-Object {
      $_.matching_boards -and ($_.matching_boards | Where-Object { $_.fqbn -eq $Fqbn })
    }
    if ($fqbnMatches -and @($fqbnMatches).Count -gt 0) {
      $candidates = @($fqbnMatches)
    }
  }

  if (@($candidates).Count -eq 1) {
    return $candidates[0].port.address
  }

  $cp210x = $candidates | Where-Object { $_.port.properties -and $_.port.properties.vid -eq "0x10C4" }
  if (@($cp210x).Count -eq 1) {
    return $cp210x[0].port.address
  }

  $addresses = $candidates | ForEach-Object { $_.port.address }
  throw ("Multiple serial ports detected: " + ($addresses -join ", ") + ". Set SENSEGRID_PORT or sensegrid.serialPort.")
}

$CliPath = Test-RequiredPath -Path $CliPath -Label "arduino-cli.exe"
$YamlPath = Test-RequiredPath -Path $YamlPath -Label "arduino-cli.yaml"

if (-not $Fqbn) { throw "FQBN not provided." }
if (-not (Test-Path -LiteralPath $BuildPath)) {
  throw "Build path not found: $BuildPath. Run the build task first."
}
$BuildPath = (Resolve-Path -LiteralPath $BuildPath).Path

$resolvedPort = Resolve-Port -CliPath $CliPath -YamlPath $YamlPath -Fqbn $Fqbn -PortHint $Port -TimeoutSec $DiscoveryTimeoutSec
Write-Host "Using port: $resolvedPort"

$args = @(
  "upload",
  "--config-file", $YamlPath,
  "--fqbn", $Fqbn,
  "--build-path", $BuildPath,
  "--port", $resolvedPort
)

& $CliPath @args
exit $LASTEXITCODE
