$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$headless = Get-ChildItem (Join-Path $env:USERPROFILE 'Downloads') -Filter analyzeHeadless.bat -File -Recurse |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $headless) { throw 'Ghidra headless was not found under Downloads.' }
$config = Get-Content (Join-Path $repo 'config/decomp/reorder_rosters.json') -Raw | ConvertFrom-Json
$addresses = @($config.functions | ForEach-Object { $_.address })
$output = Join-Path $repo ('.local/ghidra/reorder_inventory_' + [guid]::NewGuid().ToString('N') + '.json')
& $headless (Join-Path $repo '.local/ghidra') nba97_feonly_evidence -process FEONLY.BIN -noanalysis -readOnly `
    -postScript ExportFunctionSemanticsHeadless.py $output @addresses -scriptPath (Join-Path $repo 'tools/ghidra')
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $output)) { throw 'Fresh Ghidra export failed.' }
python (Join-Path $repo 'tools/verify_reorder_rosters.py') --import-inventory $output
if ($LASTEXITCODE -ne 0) { throw 'Re-order inventory validation failed.' }
