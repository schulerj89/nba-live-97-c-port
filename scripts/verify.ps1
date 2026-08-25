param(
    [switch]$SkipBuild,
    [switch]$SkipBehavior,
    [switch]$RequireMatching
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $repo
try {
    Write-Host '[1/8] Validating committed progress metadata'
    python tools/report_progress.py --check
    if ($LASTEXITCODE -ne 0) { throw 'Progress metadata verification failed.' }

    Write-Host '[2/8] Validating recovered C ownership and PS1 match candidates'
    $arguments = @('tools/verify_recovery.py')
    if ($RequireMatching) { $arguments += '--require-matching' }
    & python $arguments
    if ($LASTEXITCODE -ne 0) { throw 'Recovered C verification failed.' }

    Write-Host '[3/8] Validating committed instruction-semantic accounting'
    python tools/verify_instruction_semantics.py --check
    if ($LASTEXITCODE -ne 0) { throw 'Instruction-semantic metadata verification failed.' }

    if ($SkipBuild) {
        Write-Host '[4/8] Native build skipped by request'
    } else {
        Write-Host '[4/8] Building the native C/C++ application'
        & (Join-Path $PSScriptRoot 'build.ps1')
        if ($LASTEXITCODE -ne 0) { throw 'Native build verification failed.' }
    }

    if ($SkipBehavior) {
        Write-Host '[5/8] Asset-backed behavioral self-test skipped by request'
        Write-Host '[6/8] Native semantic checkpoint verification skipped with behavioral test'
        Write-Host '[7/8] View Rosters fidelity measurement skipped with behavioral test'
        Write-Host '[8/8] Progress regeneration skipped with behavioral test'
    } else {
        Write-Host '[5/8] Running asset-backed behavioral self-test'
        $exe = Join-Path $repo 'build-windows\Debug\nba97_boot_decomp.exe'
        if (-not (Test-Path -LiteralPath $exe)) { throw "Missing native test executable: $exe" }
        & $exe --self-test --trace '.local/logs/recovery_verification.log'
        if ($LASTEXITCODE -ne 0) { throw 'Native behavioral self-test failed.' }

        Write-Host '[6/8] Verifying native semantic checkpoints against original function scope'
        python tools/verify_instruction_semantics.py --require-native
        if ($LASTEXITCODE -ne 0) { throw 'Native instruction-semantic verification failed.' }

        $referenceRoot = Join-Path $repo '.local\verification\view_rosters\references'
        $hasRosterReferences =
            (Test-Path -LiteralPath (Join-Path $referenceRoot 'team_chicago_initial.png')) -and
            (Test-Path -LiteralPath (Join-Path $referenceRoot 'player_chicago_initial.png'))
        if ($hasRosterReferences) {
            Write-Host '[7/8] Capturing and measuring View Rosters fidelity'
            & $exe --capture-view-rosters '.local/verification/view_rosters/native' `
                --trace '.local/logs/view_rosters_capture.log'
            if ($LASTEXITCODE -ne 0) { throw 'View Rosters deterministic capture failed.' }
            python tools/verify_view_rosters.py --behavior-pass --require-references
            if ($LASTEXITCODE -ne 0) { throw 'View Rosters fidelity verification failed.' }
            Write-Host '[8/8] Regenerating progress from measured fidelity and instruction semantics'
            python tools/report_progress.py
            if ($LASTEXITCODE -ne 0) { throw 'Measured progress regeneration failed.' }
            python tools/report_progress.py --check
            if ($LASTEXITCODE -ne 0) { throw 'Measured progress verification failed.' }
        } else {
            Write-Host '[7/8] View Rosters fidelity skipped: local original references are absent'
            Write-Host '[8/8] Regenerating progress from instruction-semantic verification'
            python tools/report_progress.py
            if ($LASTEXITCODE -ne 0) { throw 'Measured progress regeneration failed.' }
            python tools/report_progress.py --check
            if ($LASTEXITCODE -ne 0) { throw 'Measured progress verification failed.' }
        }
    }

    Write-Host 'VERIFICATION PASS'
    Write-Host 'Metadata: current; C recovery ownership: verified; instruction semantics: tiered and validated; native behavior and local fidelity: tested when available.'
    if (-not $RequireMatching) {
        Write-Host 'Exact PS1 matching is reported separately and is not implied by this pass.'
    }
} finally {
    Pop-Location
}
