param(
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release')]
    [string]$Configuration = 'RelWithDebInfo'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe = Join-Path $repo "build-windows\$Configuration\nba97_boot_decomp.exe"
if (-not (Test-Path -LiteralPath $exe)) { throw "Missing native build: $exe" }
Push-Location $repo
& $exe
$code = $LASTEXITCODE
Pop-Location
exit $code
