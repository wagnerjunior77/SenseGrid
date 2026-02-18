param(
  [string]$ToolsRoot = "$PSScriptRoot\..\toolchain\arduino-data\packages\esp32\tools"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ToolsRoot)) {
  Write-Host "Tools root not found: $ToolsRoot"
  exit 0
}

# Tools nao usados pelo ESP32-C3 (RISC-V)
$remove = @(
  "esp-x32",
  "xtensa-esp-elf-gdb",
  "esp-xs2",
  "esp-xs3"
)

foreach ($name in $remove) {
  $path = Join-Path $ToolsRoot $name
  if (Test-Path $path) {
    Write-Host "Removing $path"
    Remove-Item -Recurse -Force $path
  } else {
    Write-Host "Skip (not found): $path"
  }
}

Write-Host "Prune complete."
