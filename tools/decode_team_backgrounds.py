#!/usr/bin/env python3
"""Recover NBA Live 97's runtime-patched frontend background layers.

FEONLY FUN_8002FDA4 loads each 160-colour ZTMPAL team palette, and
FUN_8002FE58 replaces colours 0..159 of every ZSET4 Bkg texture while retaining
its local colours 160..255. Recreate that mixed 256-colour palette without
publishing either the source assets or decoded output.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

PALETTE_TAGS = tuple(t + 'P' for t in (
    'atl bos cha chi cle dal den det gol hou ind lac lal mia mil min nwj nwy '
    'orl phi pho por sac san sea tor uta van was xea xwe zc1 zc2').split())


def indexed_pack(palettes, strips):
    """Versioned raw CLUT/index pack; no RGB reconstruction or palette guessing.

    16-byte header, 33 ordered (tag,160-word) palettes, then four strips:
    128*240 index bytes followed by 96 local CLUT words. Little endian.
    """
    if tuple(tag for tag, _ in palettes) != PALETTE_TAGS or len(strips) != 4:
        raise ValueError('unexpected original palette/strip order')
    out = bytearray(struct.pack('<4s6H', b'N97P', 1, 4, 33, 128, 240, 0))
    for tag, palette in palettes:
        if len(palette) != 320:
            raise ValueError('team palette must contain 160 original words')
        out += tag.encode('ascii') + palette
    for indices, local in strips:
        if len(indices) != 128*240 or len(local) != 192:
            raise ValueError('wrong indexed strip/local palette size')
        out += indices + local
    return bytes(out)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("zset4", type=Path)
    parser.add_argument("ztmpal", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--ea-tool", type=Path, required=True)
    return parser.parse_args()


def load_archive(path: Path):
    from src.EA_Image.ea_image_main import EAImage

    with path.open("rb") as stream:
        archive = EAImage()
        archive.set_ea_image_id(0)
        archive.parse_header(stream, str(path), path.name)
        archive.parse_directory(stream)
        archive.parse_bin_attachments(stream)
    return archive


def main() -> int:
    args = parse_args()
    private = Path(__file__).resolve().parents[1] / '.local'
    if not args.output.resolve().is_relative_to(private.resolve()):
        raise ValueError('decoded backgrounds must remain under repository .local')
    sys.path.insert(0, str(args.ea_tool.resolve()))

    from PIL import Image
    from reversebox.compression.compression_refpack import RefpackHandler
    from src.EA_Image.common_ea_dir import get_palette_info_dto_from_dir_entry
    from src.EA_Image.ea_image_decoder import decode_image_data_by_entry_type
    from src.EA_Image.dto import PaletteInfoDTO

    zset4 = load_archive(args.zset4)
    palettes = load_archive(args.ztmpal)
    background_tags = {f"Bkg{suffix}" for suffix in "abcdefgh"}
    backgrounds = {entry.tag: entry for entry in zset4.dir_entry_list
                   if entry.tag in background_tags}
    if len(backgrounds) != 8:
        raise RuntimeError("ZSET4 does not contain all eight frontend background strips")

    raw_palettes = [(e.tag, bytes(get_palette_info_dto_from_dir_entry(e, palettes).data[:320]))
                    for e in palettes.dir_entry_list]
    raw_strips = []
    for tag in ('Bkga', 'Bkgb', 'Bkgc', 'Bkgd'):
        e = backgrounds[tag]
        if (e.h_width, e.h_height, e.h_record_id & 0x7f) != (128, 240, 0x41):
            raise ValueError('unsupported indexed background encoding')
        pixels = RefpackHandler().decompress_data(e.raw_data) if e.h_record_id & 0x80 else e.raw_data
        local = get_palette_info_dto_from_dir_entry(e, zset4).data[320:512]
        raw_strips.append((bytes(pixels), bytes(local)))
    packed = indexed_pack(raw_palettes, raw_strips)

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'indexed.n97pal').write_bytes(packed)
    print(f'PALETTE ASSET raw CLUT/index pack bytes={len(packed)} teams=33 strips=4 local-only')
    for palette_entry in palettes.dir_entry_list:
        if not palette_entry.tag.endswith("P") or len(palette_entry.tag) != 4:
            continue
        team_palette = get_palette_info_dto_from_dir_entry(palette_entry, palettes)
        if len(team_palette.data) < 160 * 2:
            raise RuntimeError(
                f"{palette_entry.tag} has {len(team_palette.data)} palette bytes; expected at least 320")
        team_dir = args.output / palette_entry.tag[:3]
        team_dir.mkdir(parents=True, exist_ok=True)
        for tag, entry in backgrounds.items():
            image_data = entry.raw_data
            if entry.h_record_id & 0x80:
                image_data = RefpackHandler().decompress_data(image_data)
            expected_size = entry.h_width * entry.h_height
            if len(image_data) != expected_size:
                raise RuntimeError(
                    f"{tag} has {len(image_data)} indexed pixels; expected {expected_size}")
            local_palette = get_palette_info_dto_from_dir_entry(entry, zset4)
            if len(local_palette.data) < 256 * 2:
                raise RuntimeError(
                    f"{tag} has {len(local_palette.data)} local palette bytes; expected 512")
            mixed_palette = PaletteInfoDTO(
                entry_id=local_palette.entry_id,
                data=team_palette.data[:160 * 2] + local_palette.data[160 * 2:256 * 2],
                swizzle_flag=False,
            )
            rgba = decode_image_data_by_entry_type(
                entry.h_record_id & 0x7f, image_data, mixed_palette, entry)
            if rgba is None:
                raise RuntimeError(f"failed to decode {tag} with mixed {palette_entry.tag} palette")
            image = Image.frombytes("RGBA", (entry.h_width, entry.h_height), bytes(rgba))
            image.save(team_dir / f"{tag}.png")

    print(f"decoded {len(palettes.dir_entry_list)} runtime-patched frontend palettes -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
