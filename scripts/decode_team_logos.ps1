$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$tool = Join-Path $repo '.local\tools\EA-Graphics-Manager'
$source = Join-Path $repo '.local\assetpacks\menu\ZLOGOS.PSH'
$output = Join-Path $repo '.local\assetpacks\menu\ZLOGOS-decoded'
if (-not (Test-Path -LiteralPath $source)) { throw "Missing private team-logo pack: $source" }
if (-not (Test-Path -LiteralPath (Join-Path $tool 'src\EA_Image\ea_image_main.py'))) {
    throw 'EA Graphics Manager is required under .local\tools\EA-Graphics-Manager to decode the private ZLOGOS pack.'
}
$decode = @'
from pathlib import Path
from PIL import Image
from src.EA_Image.ea_image_main import EAImage
src = Path(r"__SOURCE__")
out = Path(r"__OUTPUT__")
out.mkdir(parents=True, exist_ok=True)
with src.open("rb") as stream:
    archive = EAImage()
    archive.set_ea_image_id(0)
    archive.parse_header(stream, str(src), src.name)
    archive.parse_directory(stream)
    archive.parse_bin_attachments(stream)
    archive.convert_images(None)
    for entry in archive.dir_entry_list:
        # The visible 44x48 records use a padded four-character tag ("chi ").
        # Palette helper records end in P and are not rendered directly.
        tag = entry.tag.strip().lower()
        if not entry.img_convert_data or tag.endswith("p") or entry.h_width != 44 or entry.h_height != 48:
            continue
        image = Image.frombytes("RGBA", (entry.h_width, entry.h_height),
                                bytes(entry.img_convert_data))
        image.save(out / (tag + "L.png"))
'@
$decode = $decode.Replace('__SOURCE__', $source).Replace('__OUTPUT__', $output)
Push-Location $tool
try {
    python -c $decode
    if ($LASTEXITCODE -ne 0) { throw 'EA Graphics Manager ZLOGOS decode failed.' }
} finally {
    Pop-Location
}
Write-Host 'Decoded original ZLOGOS.PSH sprites into the ignored local menu asset pack.'
