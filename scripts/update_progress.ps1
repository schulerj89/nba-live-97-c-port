$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
python (Join-Path $repo 'tools\report_progress.py')
if ($LASTEXITCODE -ne 0) { throw 'Progress report generation failed.' }
Write-Host 'Updated reports/progress.json and docs/progress.md, docs/progress.html, and docs/progress.svg.'
