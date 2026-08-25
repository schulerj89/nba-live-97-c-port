param(
    [string]$DiscImage
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $DiscImage) {
    $DiscImage = Join-Path $repo '.local\input\nba-live-97-slus-00267.bin'
}
if (-not (Test-Path -LiteralPath $DiscImage)) { throw "Missing private disc image: $DiscImage" }

$boot = Join-Path $repo '.local\assetpacks\boot'
$frontend = Join-Path $repo '.local\assetpacks\frontend'
$fonts = Join-Path $repo '.local\assetpacks\fonts'
$menu = Join-Path $repo '.local\assetpacks\menu'
$database = Join-Path $repo '.local\assetpacks\database'
New-Item -ItemType Directory -Force -Path $boot, $frontend, $fonts, $menu, $database | Out-Null
$extractor = Join-Path $repo 'tools\extract_raw_cd_file.py'
$feonly = Join-Path $repo '.local\extracted\FEONLY.BIN'
New-Item -ItemType Directory -Force -Path (Split-Path $feonly) | Out-Null
python $extractor $DiscImage $feonly --lba 56 --size 959960
python $extractor $DiscImage (Join-Path $boot 'ZLOADSCR.PSH') --lba 249235 --size 245812
python $extractor $DiscImage (Join-Path $boot 'ZLOADING.PSH') --lba 249232 --size 5784
python $extractor $DiscImage (Join-Path $frontend 'ZLEGAL.PSH') --lba 249114 --size 123460
python $extractor $DiscImage (Join-Path $frontend 'ZCPYRT97.PSH') --lba 235008 --size 245812
python $extractor $DiscImage (Join-Path $fonts 'ZFONT0.PSH') --lba 249009 --size 33016
python $extractor $DiscImage (Join-Path $fonts 'ZFONT1.PSH') --lba 249026 --size 31704
python $extractor $DiscImage (Join-Path $fonts 'ZFONT2.PSH') --lba 249042 --size 38256
python $extractor $DiscImage (Join-Path $menu 'ZFEMOCAP.BIN') --lba 248918 --size 22188
python $extractor $DiscImage (Join-Path $menu 'ZFEMODEL.BIN') --lba 248929 --size 44288
python $extractor $DiscImage (Join-Path $menu 'ZFEPLAYR.ART') --lba 248951 --size 73984
python $extractor $DiscImage (Join-Path $menu 'ZLOGOS.PSH') --lba 249370 --size 99848
python $extractor $DiscImage (Join-Path $menu 'ZTMPAL.PSH') --lba 267022 --size 21544
python $extractor $DiscImage (Join-Path $menu 'ZBPAL.PSH') --lba 234694 --size 17264
python $extractor $DiscImage (Join-Path $menu 'ZCURSOR.VB') --lba 235174 --size 60940
python $extractor $DiscImage (Join-Path $menu 'ZCURSOR.VH') --lba 235204 --size 1836
python $extractor $DiscImage (Join-Path $menu 'ZCARD.BIN') --lba 234703 --size 474240
python $extractor $DiscImage (Join-Path $menu 'Z1COOL.BIG') --lba 165811 --size 122580678
python $extractor $DiscImage (Join-Path $menu 'Z1COOL.IDX') --lba 225665 --size 19746
python $extractor $DiscImage (Join-Path $menu 'Z1PORT.BIG') --lba 225675 --size 13296378
python $extractor $DiscImage (Join-Path $menu 'Z1PORT.IDX') --lba 232168 --size 3970
python $extractor $DiscImage (Join-Path $menu 'ZTMENU1.CNK') --lba 252406 --size 8522396
python $extractor $DiscImage (Join-Path $menu 'ZSET1.PSP') --lba 251190 --size 342448
python $extractor $DiscImage (Join-Path $menu 'ZSET4.PSP') --lba 251683 --size 332084
python $extractor $DiscImage (Join-Path $menu 'ZSET7.PSP') --lba 252102 --size 323444
python $extractor $DiscImage (Join-Path $menu 'ZSET8.PSP') --lba 252260 --size 297432
if ($LASTEXITCODE -ne 0) { throw 'Private asset extraction failed.' }
python (Join-Path $repo 'tools\extract_roster_database.py') $feonly (Join-Path $database 'roster.n97db')
if ($LASTEXITCODE -ne 0) { throw 'Private roster database extraction failed.' }
& (Join-Path $PSScriptRoot 'decode_menu_assets.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Private Game Setup sprite decoding failed.' }
& (Join-Path $PSScriptRoot 'decode_frontend_pack.ps1') -Pack ZSET4
if ($LASTEXITCODE -ne 0) { throw 'Private Rosters sprite decoding failed.' }
& (Join-Path $PSScriptRoot 'decode_team_logos.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Private View Player team-logo decoding failed.' }
python (Join-Path $repo 'tools\decode_team_backgrounds.py') `
    (Join-Path $menu 'ZSET4.PSP') (Join-Path $menu 'ZTMPAL.PSH') `
    (Join-Path $menu 'ZSET4-team-backgrounds') `
    --ea-tool (Join-Path $repo '.local\tools\EA-Graphics-Manager')
if ($LASTEXITCODE -ne 0) { throw 'Private team roster background palette decoding failed.' }
& (Join-Path $PSScriptRoot 'decode_frontend_pack.ps1') -Pack ZSET7
if ($LASTEXITCODE -ne 0) { throw 'Private Users sprite decoding failed.' }
& (Join-Path $PSScriptRoot 'decode_frontend_pack.ps1') -Pack ZSET8
if ($LASTEXITCODE -ne 0) { throw 'Private View Player sprite decoding failed.' }
& (Join-Path $PSScriptRoot 'decode_card_assets.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Private Game Setup card decoding failed.' }
python (Join-Path $repo 'tools\decode_player_portraits.py') `
    (Join-Path $menu 'Z1PORT.IDX') (Join-Path $menu 'Z1PORT.BIG') `
    (Join-Path $menu 'Z1PORT-decoded') `
    --ea-tool (Join-Path $repo '.local\tools\EA-Graphics-Manager')
if ($LASTEXITCODE -ne 0) { throw 'Private View Player portrait decoding failed.' }
Write-Host 'Created local-only boot, frontend, font, menu, audio, and roster database packs from the original disc.'
