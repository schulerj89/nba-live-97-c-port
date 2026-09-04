param(
    [switch]$SkipBuild,
    [ValidateSet('Debug','RelWithDebInfo')][string]$Configuration='Debug'
)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$cmake='C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
Push-Location $repo
try {
    if(-not $SkipBuild) {
        & "$PSScriptRoot/build.ps1" -Configuration $Configuration
        if($LASTEXITCODE) {throw 'Native application build failed'}
        & $cmake --build "$repo/build-windows" --config $Configuration --target nba97_game_main_tests --parallel
        if($LASTEXITCODE) {throw 'GAMEONLY 0x80029994 test build failed'}
    }
    $unit=Join-Path $repo "build-windows/$Configuration/nba97_game_main_tests.exe"
    & $unit
    if($LASTEXITCODE) {throw 'GAMEONLY 0x80029994 unit/composition tests failed'}

    $stamp=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
    # The existing capture owner enforces this private root and requires every
    # mutable store to live beside the fresh frame directory.
    $run=Join-Path $repo ".local/verification/team_select/game-entry-$stamp"
    New-Item -ItemType Directory -Path $run | Out-Null
    $frames=Join-Path $run 'frames'
    $trace=Join-Path $run 'trace.log'
    $exe=Join-Path $repo "build-windows/$Configuration/nba97_boot_decomp.exe"
    $captureArgs=@('--asset-root',"$repo/.local/assetpacks",'--capture-team-select',$frames,
        '--settings',"$run/settings.ini",'--profiles',"$run/profiles.n97sav",
        '--created-players',"$run/created.n97cpl",'--roster-save',"$run/rosters.n97rst",'--trace',$trace)
    & $exe @captureArgs *> "$run/stdout.log"
    if($LASTEXITCODE) {Get-Content "$run/stdout.log" -Tail 20;throw 'Frontend game-entry capture failed'}
    python tools/verify_game_entry_visual.py --frames $frames --trace $trace
    if($LASTEXITCODE) {throw 'Frontend game-entry visual receipt failed'}
    Write-Host "Evidence: $run"
} finally {
    Pop-Location
}
