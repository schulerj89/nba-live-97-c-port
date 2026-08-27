#!/usr/bin/env python3
"""Decode local Z2PORT small portraits (80056494 -> 80030D14).

Record zero is the original fallback; player N uses record N+1. No game
bytes are embedded here. PS1 PAL8 rows have an even byte stride, including
uncompressed 0x41 records: the logical 87x51 image occupies 88x51 bytes.
"""
import argparse
import io
import struct
import sys
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('index', type=Path)
    p.add_argument('archive', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--ea-tool', type=Path, required=True)
    args = p.parse_args()
    private = Path(__file__).resolve().parents[1] / '.local'
    if not args.output.resolve().is_relative_to(private.resolve()):
        p.error('decoded game portraits must remain under the ignored .local directory')
    sys.path.insert(0, str(args.ea_tool.resolve()))
    from PIL import Image
    from src.EA_Image.ea_image_main import EAImage
    from src.EA_Image.common_ea_dir import get_palette_info_dto_from_dir_entry
    from src.EA_Image.ea_image_decoder import decode_image_data_by_entry_type
    from reversebox.compression.compression_refpack import RefpackHandler
    idx, big = args.index.read_bytes(), args.archive.read_bytes()
    if len(idx) < 4:
        raise ValueError('truncated Z2PORT index')
    count, = struct.unpack_from('<I', idx)
    if count != 493 or len(idx) < 4 + count * 8:
        raise ValueError('unexpected Z2PORT index')
    args.output.mkdir(parents=True, exist_ok=True)
    decoded = 0
    for record in range(count):
        size, offset = struct.unpack_from('<II', idx, 4 + record * 8)
        if not size:
            continue
        if offset + size > len(big):
            raise ValueError('Z2PORT record outside archive')
        stream = io.BytesIO(big[offset:offset+size])
        archive = EAImage()
        archive.set_ea_image_id(0)
        archive.parse_header(stream, str(args.archive), 'portrait.shpp')
        archive.parse_directory(stream)
        archive.parse_bin_attachments(stream)
        entry, = archive.dir_entry_list
        if (entry.h_width, entry.h_height) != (87, 51) or entry.h_record_id & 0x7f != 0x41:
            raise ValueError('unexpected small portrait format/dimensions')
        raw = entry.raw_data
        if entry.h_record_id & 0x80:
            raw = RefpackHandler().decompress_data(raw)
        if len(raw) != 88 * 51:
            raise ValueError(f'portrait {record} padded stride mismatch: {len(raw)}')
        palette = get_palette_info_dto_from_dir_entry(entry, archive)
        entry.h_width = 88
        rgba = decode_image_data_by_entry_type(0x41, raw, palette, entry)
        image = Image.frombytes('RGBA', (88, 51), bytes(rgba)).crop((0, 0, 87, 51))
        image.save(args.output / f'player_{record:03d}.png')
        decoded += 1
    print(f'Z2PORT: {decoded}/{count} original 87x51 portraits decoded locally')


if __name__ == '__main__':
    main()
