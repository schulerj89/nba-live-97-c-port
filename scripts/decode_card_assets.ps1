$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$tool = Join-Path $repo '.local\tools\EA-Graphics-Manager'
$source = Join-Path $repo '.local\assetpacks\menu\ZCARD.BIN'
$output = Join-Path $repo '.local\assetpacks\menu\ZCARD-decoded'
if (-not (Test-Path -LiteralPath $source)) { throw "Missing private card pack: $source" }
if (-not (Test-Path -LiteralPath (Join-Path $tool 'src\EA_Image\ea_image_main.py'))) {
    throw 'EA Graphics Manager is required under .local\tools\EA-Graphics-Manager to decode the private ZCARD image records.'
}
$decode = @'
from io import BytesIO
from pathlib import Path
from PIL import Image
from src.EA_Image.ea_image_main import EAImage

src = Path(r"__SOURCE__")
out = Path(r"__OUTPUT__")
out.mkdir(parents=True, exist_ok=True)
data = src.read_bytes()
record_size = 0x1380
if len(data) % record_size:
    raise RuntimeError("ZCARD.BIN is not an integral number of 0x1380-byte records")
for index in range(len(data) // record_size):
    record = data[index * record_size:(index + 1) * record_size]
    stream = BytesIO(record)
    archive = EAImage()
    archive.set_ea_image_id(0)
    archive.parse_header(stream, str(src), f"card-{index:02d}.shpp")
    archive.parse_directory(stream)
    archive.parse_bin_attachments(stream)
    # These PS1 PAL8 portraits are 69 logical pixels wide but every row is
    # stored on a 70-byte boundary. Decode at the physical stride so rows do
    # not shear diagonally, then crop back to the directory width.
    logical_widths = {}
    for entry in archive.dir_entry_list:
        if entry.h_record_id == 0x41 and entry.h_width & 1:
            logical_widths[entry.tag] = entry.h_width
            entry.h_width += 1
    archive.convert_images(None)
    images = [entry for entry in archive.dir_entry_list if entry.img_convert_data]
    if len(images) != 1:
        raise RuntimeError(f"card {index} decoded {len(images)} images")
    entry = images[0]
    image = Image.frombytes("RGBA", (entry.h_width, entry.h_height),
                            bytes(entry.img_convert_data))
    logical_width = logical_widths.get(entry.tag)
    if logical_width is not None:
        image = image.crop((0, 0, logical_width, entry.h_height))
    image.save(out / f"card_{index:02d}.png")
print(f"decoded {len(data) // record_size} private card images")
'@
$decode = $decode.Replace('__SOURCE__', $source).Replace('__OUTPUT__', $output)
Push-Location $tool
try {
    python -c $decode
    if ($LASTEXITCODE -ne 0) { throw 'ZCARD image decode failed.' }
} finally {
    Pop-Location
}
Write-Host 'Decoded original ZCARD.BIN portraits into the ignored local menu asset pack.'
