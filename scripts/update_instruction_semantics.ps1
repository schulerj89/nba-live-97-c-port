$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$headless = Get-ChildItem -Path (Join-Path $env:USERPROFILE 'Downloads') `
    -Filter analyzeHeadless.bat -File -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $headless) { throw 'Could not find analyzeHeadless.bat under Downloads.' }

$projectDir = Join-Path $repo '.local\ghidra'
$projectName = 'nba97_feonly_evidence'
$projectFile = Join-Path $projectDir ($projectName + '.gpr')
if (-not (Test-Path -LiteralPath $projectFile)) {
    throw "Missing local Ghidra project: $projectFile. Run scripts/analyze_headless.ps1 first."
}

$output = Join-Path $repo 'config\decomp\instruction_semantics\view_rosters_original.json'
$outputDir = Split-Path -Parent $output
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$temporaryOutput = Join-Path $outputDir 'view_rosters_original.generated.tmp'
if (Test-Path -LiteralPath $temporaryOutput) {
    Remove-Item -LiteralPath $temporaryOutput -Force
}
$addresses = @(
    '0x8002FE58',
    '0x8005770C',
    '0x80057864',
    '0x80057CE4',
    '0x80059034',
    '0x800590B8',
    '0x800592C4',
    '0x80059610',
    '0x80059928',
    '0x8005A538',
    '0x8005FE14'
)

& $headless $projectDir $projectName -process 'FEONLY.BIN' -noanalysis `
    -postScript ExportFunctionSemanticsHeadless.py $temporaryOutput @addresses `
    -scriptPath (Join-Path $repo 'tools\ghidra')
if ($LASTEXITCODE -ne 0) { throw 'Instruction-semantic export failed.' }
if (-not (Test-Path -LiteralPath $temporaryOutput)) {
    throw 'Instruction-semantic export produced no fresh output.'
}
Move-Item -LiteralPath $temporaryOutput -Destination $output -Force

python (Join-Path $repo 'tools\verify_instruction_semantics.py')
if ($LASTEXITCODE -ne 0) { throw 'Instruction-semantic verification failed.' }
Write-Host 'Refreshed View Rosters instruction-semantic metadata.'
