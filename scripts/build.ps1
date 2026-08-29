param(
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release')]
    [string]$Configuration = 'RelWithDebInfo',
    [switch]$AllTargets
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw "Missing Visual Studio CMake: $cmake" }

& $cmake -S $repo -B (Join-Path $repo 'build-windows') -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'Native Windows configuration failed.' }
$buildArgs = @('--build', (Join-Path $repo 'build-windows'), '--config', $Configuration)
if (-not $AllTargets) { $buildArgs += @('--target', 'nba97_boot_decomp') }
$buildArgs += '--parallel'
& $cmake @buildArgs
if ($LASTEXITCODE -ne 0) { throw 'Native Windows build failed.' }
Write-Host "Built build-windows\$Configuration\nba97_boot_decomp.exe"
