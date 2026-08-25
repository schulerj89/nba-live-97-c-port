$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$tool = Join-Path $repo '.local\tools\EA-Graphics-Manager'
$source = Join-Path $repo '.local\assetpacks\menu\ZSET1.PSP'
$output = Join-Path $repo '.local\assetpacks\menu\ZSET1-decoded'
if (-not (Test-Path -LiteralPath $source)) { throw "Missing private menu pack: $source" }
if (-not (Test-Path -LiteralPath (Join-Path $tool 'src\EA_Image\ea_image_main.py'))) {
    throw 'EA Graphics Manager is required under .local\tools\EA-Graphics-Manager to decode the private ZSET1 sprite pack.'
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
    # ZSET1's compressed 4-bit PS1 sprites store a padded scanline stride. This
    # affects even widths too (for example, c00a is 94 logical pixels but 96
    # stored pixels); ignoring it shears every row into diagonal ribbons.
    # Derive the stored width from RefPack output, decode at that stride, then
    # crop to the directory dimensions below.
    logical_widths = {}
    for entry in archive.dir_entry_list:
        # Both compressed PAL4 (C0, two pixels/byte) and PAL8 (C1, one
        # pixel/byte) records use a padded scanline stride.
        if entry.h_record_id in (0xC0, 0xC1):
            unpacked_size = len(RefpackHandler().decompress_data(entry.raw_data))
            if unpacked_size % entry.h_height:
                raise RuntimeError(f"Cannot derive scanline stride for {entry.tag}")
            pixels_per_byte = 2 if entry.h_record_id == 0xC0 else 1
            stored_width = unpacked_size // entry.h_height * pixels_per_byte
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
    if ($LASTEXITCODE -ne 0) { throw 'EA Graphics Manager decode failed.' }
} finally {
    Pop-Location
}
Write-Host 'Decoded original ZSET1.PSP sprites into the ignored local menu asset pack.'
