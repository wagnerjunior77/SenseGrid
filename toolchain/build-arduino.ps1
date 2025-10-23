param(
  [string]$SketchPath,
  [string]$YamlPath,
  [string]$CliPath,
  [string]$Fqbn,
  [string]$IncludeFile = "$PSScriptRoot\include-dirs.txt",
  [string]$BuildPath   = "$PSScriptRoot\build\SenseGrid",
  [switch]$ExportBinaries
)

$ErrorActionPreference = "Stop"

# Lê include-dirs.txt (ignora linhas vazias e comentários)
$incs = Get-Content -Raw $IncludeFile |
  Select-String -Pattern '^(?!\s*#).+\S' -AllMatches |
  ForEach-Object { $_.Matches.Value.Trim() }

# Resolve caminhos (normaliza barra)
$incsAbs = $incs | ForEach-Object { (Resolve-Path $_).Path -replace '\\','/' }
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

$args += $SketchPath

& $CliPath @args
exit $LASTEXITCODE
