param(
    [switch]$SkipBuild,
    [string]$NavigationOriginalTrace,
    [string]$WrapOriginalTrace,
    [switch]$RequireOriginal
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $repo
try {
    python tools/verify_roster_scenarios.py --check-config
    if ($LASTEXITCODE -ne 0) { throw 'Roster scenario contract validation failed.' }

    if (-not $SkipBuild) {
        & (Join-Path $PSScriptRoot 'build.ps1')
        if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
    }

    $exe = Join-Path $repo 'build-windows\Debug\nba97_boot_decomp.exe'
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "Missing native executable: $exe"
    }
    & $exe --self-test --trace '.local/logs/view_rosters_semantic_workflow.log'
    if ($LASTEXITCODE -ne 0) { throw 'Native View Rosters scenario capture failed.' }

    $arguments = @('tools/verify_roster_scenarios.py', '--require-native')
    if ($NavigationOriginalTrace) {
        $arguments += '--original'
        $arguments += "view_rosters_navigation=$NavigationOriginalTrace"
    }
    if ($WrapOriginalTrace) {
        $arguments += '--original'
        $arguments += "view_player_wrap=$WrapOriginalTrace"
    }
    if ($RequireOriginal) { $arguments += '--require-original' }
    & python $arguments
    if ($LASTEXITCODE -ne 0) { throw 'View Rosters semantic comparison failed.' }

    Write-Host 'VIEW ROSTERS SEMANTIC VERIFICATION PASS'
    if ($RequireOriginal) {
        Write-Host 'All required local original traces and all native scenarios verified.'
    } elseif (-not $NavigationOriginalTrace -or -not $WrapOriginalTrace) {
        Write-Host 'Native state contract is verified; original equivalence remains partial until both local no$psx traces are supplied.'
    }
} finally {
    Pop-Location
}
