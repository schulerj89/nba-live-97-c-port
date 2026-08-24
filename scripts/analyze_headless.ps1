$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$headless = Get-ChildItem -Path (Join-Path $env:USERPROFILE 'Downloads') `
    -Filter analyzeHeadless.bat -File -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $headless) { throw 'Could not find analyzeHeadless.bat under Downloads.' }

$projectDir = Join-Path $repo '.local\ghidra'
$scriptPath = Join-Path $repo 'tools\ghidra'
$boot = Join-Path $repo '.local\input\nba-live-97-slus-00267.bin.boot.exe'
$feonly = Join-Path $repo '.local\extracted\FEONLY.BIN'
if (-not (Test-Path -LiteralPath $boot)) { throw "Missing private boot EXE: $boot" }
if (-not (Test-Path -LiteralPath $feonly)) { throw "Missing private FEONLY overlay: $feonly" }
New-Item -ItemType Directory -Force -Path $projectDir | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'

& $headless $projectDir "nba97_boot_$stamp" -import $boot -loader BinaryLoader `
    -processor 'MIPS:LE:32:default' -loader-baseAddr 0x801DF800 `
    -preScript PreparePS1Raw.py 0x801E3508 `
    -postScript ReportReferencesHeadless.py (Join-Path $projectDir 'boot_asset_refs.txt') `
    0x801E0008 0x801E0024 0x801E0040 -scriptPath $scriptPath
if ($LASTEXITCODE -ne 0) { throw 'Boot analysis failed.' }

& $headless $projectDir "nba97_feonly_$stamp" -import $feonly -loader BinaryLoader `
    -processor 'MIPS:LE:32:default' -loader-baseAddr 0x80015000 `
    -preScript PreparePS1Raw.py 0x8007B79C `
    -postScript ReportReferencesHeadless.py (Join-Path $projectDir 'feonly_asset_refs.txt') `
    0x80024914 0x80024920 0x80024B38 0x8002D768 0x8002DFB4 0x80036684 `
    -scriptPath $scriptPath
if ($LASTEXITCODE -ne 0) { throw 'FEONLY analysis failed.' }
Write-Host 'Recovered boot/frontend asset references under .local/ghidra.'
