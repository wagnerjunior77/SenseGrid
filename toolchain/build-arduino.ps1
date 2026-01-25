param(
  [string]$SketchPath,
  [string]$YamlPath,
  [string]$CliPath,
  [string]$Fqbn,
  [string]$IncludeFile = "$PSScriptRoot\include-dirs.txt",
  [string]$BuildPath   = "$PSScriptRoot\build\SenseGrid",
  [switch]$ExportBinaries,
  [switch]$Clean
)

$ErrorActionPreference = "Stop"

# Lê include-dirs.txt (ignora linhas vazias e comentários)
$incs = Get-Content $IncludeFile |
  Where-Object { $_ -notmatch '^\s*#' -and $_.Trim() -ne '' } |
  ForEach-Object { $_.Trim() }

# Resolve caminhos (normaliza barra) e ignora entradas inexistentes
$incsExisting = $incs | Where-Object { Test-Path $_ -PathType Container }
$incsMissing = $incs | Where-Object { -not (Test-Path $_ -PathType Container) }
if ($incsMissing) {
  Write-Warning ("Include path(s) not found: " + ($incsMissing -join ', '))
}
$incsAbs = $incsExisting | ForEach-Object { (Resolve-Path $_).Path -replace '\\','/' }
$flags = '-DCONFIG_NIMBLE_CPP_IDF=1 ' + (($incsAbs | ForEach-Object { '-I' + $_ }) -join ' ')

# Garante pasta de build
New-Item -ItemType Directory -Force $BuildPath | Out-Null

# Monta args do arduino-cli
$args = @(
  'compile',
  '--config-file', $YamlPath,
  '--fqbn', $Fqbn,
  '--build-property', "build.extra_flags=$flags",
  '--build-path', $BuildPath
)

if ($ExportBinaries) { $args += '--export-binaries' }
if ($Clean) { $args += '--clean' }

$args += $SketchPath

& $CliPath @args
exit $LASTEXITCODE
