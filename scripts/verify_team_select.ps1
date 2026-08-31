param(
    [switch]$SkipBuild,
    [ValidateSet('Debug','RelWithDebInfo')][string]$Configuration='Debug',
    [string]$OriginalRanks
)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
function SaveFingerprint {
    $result=@{}
    foreach($folder in @('.local/saves','.local/config')) {
        $path=Join-Path $repo $folder
        if(Test-Path -LiteralPath $path) {
            Get-ChildItem -LiteralPath $path -File -Recurse | ForEach-Object {
                $result[$_.FullName]=((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash+':'+$_.Length+':'+$_.LastWriteTimeUtc.Ticks)
            }
        }
    }
    return $result
}
$before=SaveFingerprint
Push-Location $repo
try {
    if(-not $SkipBuild) { & "$PSScriptRoot/build.ps1" -Configuration $Configuration -AllTargets; if($LASTEXITCODE) {throw 'Build failed'} }
    $exe=Join-Path $repo "build-windows/$Configuration/nba97_boot_decomp.exe"
    foreach($test in @('team_select','team_select_poll','team_ratings','user_setup','user_setup_session','user_setup_visibility','user_profiles','match_controls','match_snapshot','frontend_help','frontend_help_presentation','win32_keyboard')) {
        & (Join-Path $repo ("build-windows/{0}/nba97_{1}_tests.exe" -f $Configuration,$test))
        if($LASTEXITCODE) {throw "$test failed"}
    }
    & (Join-Path $repo "build-windows/$Configuration/nba97_frontend_help_tests.exe") "$repo/.local/assetpacks"
    if($LASTEXITCODE) {throw 'Original-font Help pixel checks failed'}
    python tools/verify_team_select.py
    if($LASTEXITCODE) {throw 'Team Select metadata failed'}
    $stamp=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
    $root=Join-Path $repo ".local/verification/team_select/run-$stamp"
    New-Item -ItemType Directory -Path $root | Out-Null
    # Refuse unsafe defaults before stores open; preserve all real save families.
    $guard=& $exe --capture-team-select "$root/unsafe/frames" --trace "$root/guard.log" 2>&1
    if($LASTEXITCODE -eq 0 -or ($guard -join [Environment]::NewLine) -notmatch 'isolated run directory and explicit saves') {throw 'Capture isolation guard failed'}
    foreach($name in @('first','second')) {
        $run=Join-Path $root $name
        New-Item -ItemType Directory -Path $run | Out-Null
        $captureArgs=@('--asset-root',"$repo/.local/assetpacks",'--capture-team-select',"$run/frames",
            '--settings',"$run/settings.ini",'--profiles',"$run/profiles.n97sav",
            '--created-players',"$run/created.n97cpl",'--roster-save',"$run/rosters.n97rst",'--trace',"$run/trace.log")
        & $exe @captureArgs *> "$run/stdout.log"
        if($LASTEXITCODE) {Get-Content "$run/stdout.log" -Tail 12;throw "$name capture failed"}
    }
    $verifyArgs=@('tools/verify_team_select.py','--first',"$root/first/frames",'--second',"$root/second/frames")
    if($OriginalRanks) {$verifyArgs+=@('--original-ranks',$OriginalRanks)}
    python @verifyArgs
    if($LASTEXITCODE) {throw 'Team Select scenario verification failed'}
    Write-Host "Evidence: $root"
} finally {
    Pop-Location
    $after=SaveFingerprint
    if($before.Count -ne $after.Count) {throw 'Real save/config file set changed'}
    foreach($key in $before.Keys) {
        if(-not $after.ContainsKey($key) -or $before[$key] -ne $after[$key]) {throw "Real save/config file changed: $key"}
    }
    Write-Host 'TEAM SAVE SAFETY PASS: real save/config files byte-identical, timestamps unchanged'
}
