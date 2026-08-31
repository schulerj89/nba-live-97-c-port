#!/usr/bin/env python3
"""Decode the original local-only View Player portrait archive.

Z1PORT.IDX begins with a little-endian logical record count followed by
`(size, offset)` pairs. The count excludes the reserved physical record zero,
which is the original fallback image; FEONLY player id N below the count uses
physical record N+1. Each existing record in Z1PORT.BIG is a
standalone SHPP archive containing a 180x156 action photo.
The source archive and generated PNGs remain under `.local/` and are never
distributed by the port.
"""

from __future__ import annotations

import argparse
import io
import struct
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("index", type=Path)
    parser.add_argument("archive", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--ea-tool", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    sys.path.insert(0, str(args.ea_tool.resolve()))
    from PIL import Image
    from src.EA_Image.ea_image_main import EAImage

    index = args.index.read_bytes()
    archive = args.archive.read_bytes()
    if len(index) < 4:
        raise RuntimeError("Z1PORT index is truncated")
    count = struct.unpack_from("<I", index)[0]
    # 800310D8: player<count selects player+1, otherwise reserved record0.
    # The native extractor formerly omitted the last original physical record.
    physical_count = count + 1
    if len(index) < 4 + physical_count * 8:
        raise RuntimeError("Z1PORT index table is truncated")
    args.output.mkdir(parents=True, exist_ok=True)

    decoded = 0
    for player_id in range(physical_count):
        size, offset = struct.unpack_from("<II", index, 4 + player_id * 8)
        if size == 0:
            continue
        if offset + size > len(archive):
            raise RuntimeError(f"player {player_id} portrait exceeds Z1PORT.BIG")
        stream = io.BytesIO(archive[offset:offset + size])
        image_archive = EAImage()
        image_archive.set_ea_image_id(0)
        # EA Graphics Manager consults the supplied path only for an outer
        # file-size sanity value; parsing itself uses this bounded stream.
        image_archive.parse_header(stream, str(args.archive), f"player_{player_id:03d}.shpp")
        image_archive.parse_directory(stream)
        image_archive.parse_bin_attachments(stream)
        image_archive.convert_images(None)
        entry = next((item for item in image_archive.dir_entry_list
                      if item.img_convert_data), None)
        if entry is None:
            raise RuntimeError(f"player {player_id} SHPP contains no image")
        image = Image.frombytes("RGBA", (entry.h_width, entry.h_height),
                                bytes(entry.img_convert_data))
        if image.size != (180, 156):
            raise RuntimeError(
                f"player {player_id} portrait is {image.width}x{image.height}; expected 180x156")
        image.save(args.output / f"player_{player_id:03d}.png")
        decoded += 1

    print(f"decoded {decoded}/{physical_count} original View Player portraits -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
