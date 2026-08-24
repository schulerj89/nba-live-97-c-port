param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('ZSET1', 'ZSET4', 'ZSET7')]
    [string]$Pack
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$tool = Join-Path $repo '.local\tools\EA-Graphics-Manager'
$source = Join-Path $repo ".local\assetpacks\menu\$Pack.PSP"
$output = Join-Path $repo ".local\assetpacks\menu\$Pack-decoded"
if (-not (Test-Path -LiteralPath $source)) { throw "Missing private frontend pack: $source" }
if (-not (Test-Path -LiteralPath (Join-Path $tool 'src\EA_Image\ea_image_main.py'))) {
    throw 'EA Graphics Manager is required under .local\tools\EA-Graphics-Manager.'
}
$decode = @'
from pathlib import Path
from PIL import Image
from src.EA_Image.ea_image_main import EAImage
from reversebox.compression.compression_refpack import RefpackHandler
src = Path(r"__SOURCE__")
out = Path(r"__OUTPUT__")
out.mkdir(parents=True, exist_ok=True)
with src.open("rb") as stream:
    archive = EAImage()
    archive.set_ea_image_id(0)
    archive.parse_header(stream, str(src), src.name)
    archive.parse_directory(stream)
    archive.parse_bin_attachments(stream)
    logical_widths = {}
    for entry in archive.dir_entry_list:
        if entry.h_record_id == 0xC0:
            unpacked_size = len(RefpackHandler().decompress_data(entry.raw_data))
            if unpacked_size % entry.h_height:
                raise RuntimeError(f"Cannot derive scanline stride for {entry.tag}")
            stored_width = unpacked_size // entry.h_height * 2
            if stored_width < entry.h_width:
                raise RuntimeError(f"Stored scanline is shorter than {entry.tag}'s logical width")
            if stored_width != entry.h_width:
                logical_widths[entry.tag] = entry.h_width
                entry.h_width = stored_width
    archive.convert_images(None)
    for entry in archive.dir_entry_list:
        if entry.img_convert_data:
            image = Image.frombytes("RGBA", (entry.h_width, entry.h_height),
                                    bytes(entry.img_convert_data))
            logical_width = logical_widths.get(entry.tag)
            if logical_width is not None:
                image = image.crop((0, 0, logical_width, entry.h_height))
            image.save(out / (entry.tag + ".png"))
'@
$decode = $decode.Replace('__SOURCE__', $source).Replace('__OUTPUT__', $output)
Push-Location $tool
try {
    python -c $decode
    if ($LASTEXITCODE -ne 0) { throw "$Pack decode failed." }
} finally {
    Pop-Location
}
Write-Host "Decoded original $Pack.PSP sprites into the ignored local frontend pack."
