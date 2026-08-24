$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$drive = $repo.Substring(0, 1).ToLowerInvariant()
$tail = $repo.Substring(2).Replace('\', '/')
$linuxRepo = "/mnt/$drive$tail"

& wsl.exe -d Ubuntu -- bash -lc "cd '$linuxRepo' && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel"
if ($LASTEXITCODE -ne 0) { throw 'WSL compatibility build failed.' }
Write-Host 'Built build/nba97_boot_decomp (WSL compatibility target)'
