param(
    [string]$DiscImage
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $DiscImage) {
    $DiscImage = Join-Path $repo '.local\input\nba-live-97-slus-00267.bin'
}
if (-not (Test-Path -LiteralPath $DiscImage)) { throw "Missing private disc image: $DiscImage" }

$intro = Join-Path $repo '.local\assetpacks\intro'
$raw = Join-Path $intro 'Z0ZTITLE.raw.xa'
$index = Join-Path $intro 'Z0ZTITLE.jpsxdec.idx'
$playback = Join-Path $intro 'Z0ZTITLE.avi'
$decoded = Join-Path $intro 'decoded'
$generated = Join-Path $decoded 'Z0ZTITLE.raw.xa[0].avi'
New-Item -ItemType Directory -Force -Path $intro, $decoded | Out-Null

if (-not (Test-Path -LiteralPath $raw)) {
    python (Join-Path $repo 'tools\extract_raw_cd_file.py') $DiscImage $raw `
        --lba 146888 --size 23947264 --raw-sectors
    if ($LASTEXITCODE -ne 0) { throw 'Raw Z0ZTITLE.XA extraction failed.' }
}

$toolRoot = Join-Path $repo '.local\tools\jpsxdec_v2.1-beta'
$zip = Join-Path $repo '.local\tools\jpsxdec_v2.1-beta.zip'
$jar = Join-Path $toolRoot 'jpsxdec_v2.1-beta\jpsxdec.jar'
$expectedHash = 'E11787A2E05B6A4B6EE07972439E50DA9E6798F0CA505E748F4969B3D91CD45B'
if (-not (Test-Path -LiteralPath $jar)) {
    New-Item -ItemType Directory -Force -Path (Split-Path $zip) | Out-Null
    Invoke-WebRequest `
        -Uri 'https://github.com/m35/jpsxdec/releases/download/v2.1/jpsxdec_v2.1-beta.zip' `
        -OutFile $zip
    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash
    if ($actualHash -ne $expectedHash) { throw "jPSXdec archive hash mismatch: $actualHash" }
    Expand-Archive -LiteralPath $zip -DestinationPath $toolRoot -Force
}

if (-not (Test-Path -LiteralPath $index)) {
    Push-Location $intro
    java -jar $jar -f $raw -x $index
    $indexCode = $LASTEXITCODE
    Pop-Location
    if ($indexCode -ne 0) { throw 'jPSXdec indexing failed.' }
}
if (-not (Test-Path -LiteralPath $generated) -and -not (Test-Path -LiteralPath $playback)) {
    Push-Location $decoded
    java -jar $jar -x $index -i 0 -vidfmt avi:mjpg -quality high -psxav
    $decodeCode = $LASTEXITCODE
    Pop-Location
    if ($decodeCode -ne 0) { throw 'jPSXdec title-movie decode failed.' }
}
if (-not (Test-Path -LiteralPath $playback)) {
    Copy-Item -LiteralPath $generated -Destination $playback
}
Write-Host "Prepared private recovered intro movie: $playback"
