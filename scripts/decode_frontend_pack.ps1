param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('ZSET1', 'ZSET4', 'ZSET7', 'ZSET8')]
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
from src.EA_Image.common_ea_dir import get_palette_info_dto_from_dir_entry
from src.EA_Image.ea_image_decoder import decode_image_data_by_entry_type
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
        # RefPack-compressed PS1 PAL4/PAL8 records retain their padded source
        # scanline.  C0 stores two pixels per byte; C1 stores one.  The old
        # decoder corrected C0 only, shearing every odd-width C1 row (notably
        # the 103px team marks and 87px NBA/EA header logos).
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
    # FEONLY FUN_80031F48 reads screen 0x10's descriptor byte 0x16 and
    # supplies ZSET4's Pal0 CLUT to layout records 6..16.  Their indexed
    # pixels remain local to each sprite; only the 16-colour PS1 palette is
    # shared.  Recreate that runtime operation here so the ignored PNG pack
    # contains the exact border/frame colours instead of their embedded red
    # authoring palettes.
    pal0_targets = set()
    if src.stem.upper() == "ZSET4":
        pal0_targets = {
            "brte", "brtf", "brtg", "brth", "brle", "brri",
            "brbe", "brbf", "brbg", "brbh", "frml", "frmr",
        }
    elif src.stem.upper() == "ZSET8":
        # State 0x24's layout routes its A-D frame records through ZSET8
        # Pal0. Their embedded red authoring CLUT is not the runtime result.
        pal0_targets = {
            "brta", "brtb", "brtc", "brtd", "brle", "brri",
            "brba", "brbb", "brbc", "brbd",
        }
    pal0 = None
    if pal0_targets:
        pal0_entry = next((entry for entry in archive.dir_entry_list
                           if entry.tag == "Pal0"), None)
        if pal0_entry is None:
            raise RuntimeError("ZSET4 is missing FEONLY's shared Pal0 palette")
        pal0 = get_palette_info_dto_from_dir_entry(pal0_entry, archive)
        if len(pal0.data) != 32:
            raise RuntimeError(f"Pal0 has {len(pal0.data)} bytes; expected 16 BGR555 colours")
    # FUN_800399C4 marks unavailable type-2 menu objects with state bit 0x80.
    # The frontend renderer resolves that state through ZSET4's `red1` palette
    # carrier. Its lower indices form the disabled red ramp while its upper
    # indices preserve the plate's grey/white highlights and labels. Reset
    # (c06d) and Player Injuries (c14d) therefore reuse their normal indexed
    # artwork; they are not separate replacement pictures in the archive.
    # Preserve those exact local-only runtime variants for the native compositor.
    disabled_variants = {}
    if src.stem.upper() == "ZSET4":
        red1_entry = next((entry for entry in archive.dir_entry_list
                           if entry.tag == "red1"), None)
        if red1_entry is None:
            raise RuntimeError("ZSET4 is missing FEONLY's disabled red1 palette carrier")
        red1 = get_palette_info_dto_from_dir_entry(red1_entry, archive)
        if len(red1.data) != 32:
            raise RuntimeError(f"red1 has {len(red1.data)} bytes; expected 16 BGR555 colours")
        disabled_variants = {"c06d": (red1, "c06r"),
                             "c14d": (red1, "c14r")}
    for entry in archive.dir_entry_list:
        converted = entry.img_convert_data
        if pal0 is not None and entry.tag in pal0_targets:
            image_data = entry.raw_data
            if entry.h_record_id & 0x80:
                image_data = RefpackHandler().decompress_data(image_data)
            converted = decode_image_data_by_entry_type(
                entry.h_record_id & 0x7f, image_data, pal0, entry)
            if converted is None:
                raise RuntimeError(f"Failed to decode {entry.tag} through shared Pal0")
        if converted:
            image = Image.frombytes("RGBA", (entry.h_width, entry.h_height),
                                    bytes(converted))
            logical_width = logical_widths.get(entry.tag)
            if logical_width is not None:
                image = image.crop((0, 0, logical_width, entry.h_height))
            image.save(out / (entry.tag + ".png"))
        variant = disabled_variants.get(entry.tag)
        if variant is not None:
            palette, output_tag = variant
            image_data = entry.raw_data
            if entry.h_record_id & 0x80:
                image_data = RefpackHandler().decompress_data(image_data)
            converted = decode_image_data_by_entry_type(
                entry.h_record_id & 0x7f, image_data, palette, entry)
            if converted is None:
                raise RuntimeError(f"Failed to decode {entry.tag} through red1 CLUT")
            image = Image.frombytes("RGBA", (entry.h_width, entry.h_height),
                                    bytes(converted))
            logical_width = logical_widths.get(entry.tag)
            if logical_width is not None:
                image = image.crop((0, 0, logical_width, entry.h_height))
            image.save(out / (output_tag + ".png"))
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
