#!/usr/bin/env python3
"""List ISO9660 files in a raw 2352-byte/sector PlayStation disc image."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


RAW_SECTOR_SIZE = 2352
USER_DATA_OFFSET = 24
USER_DATA_SIZE = 2048


def read_extent(disc, lba: int, size: int) -> bytes:
    data = bytearray()
    for sector in range((size + USER_DATA_SIZE - 1) // USER_DATA_SIZE):
        disc.seek((lba + sector) * RAW_SECTOR_SIZE + USER_DATA_OFFSET)
        data.extend(disc.read(USER_DATA_SIZE))
    return bytes(data[:size])


def walk_directory(disc, lba: int, size: int, prefix: str = ""):
    directory = read_extent(disc, lba, size)
    offset = 0
    while offset < len(directory):
        record_size = directory[offset]
        if record_size == 0:
            offset = ((offset // USER_DATA_SIZE) + 1) * USER_DATA_SIZE
            continue
        record = directory[offset:offset + record_size]
        extent_lba = struct.unpack_from("<I", record, 2)[0]
        extent_size = struct.unpack_from("<I", record, 10)[0]
        flags = record[25]
        name_size = record[32]
        raw_name = record[33:33 + name_size]
        offset += record_size
        if raw_name in (b"\x00", b"\x01"):
            continue
        name = raw_name.decode("ascii", errors="replace").split(";", 1)[0]
        path = f"{prefix}/{name}" if prefix else name
        if flags & 0x02:
            yield from walk_directory(disc, extent_lba, extent_size, path)
        else:
            yield path, extent_lba, extent_size


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("disc", type=Path)
    parser.add_argument("--match", default="", help="case-insensitive name fragment")
    args = parser.parse_args()
    with args.disc.open("rb") as disc:
        pvd = read_extent(disc, 16, USER_DATA_SIZE)
        if pvd[1:6] != b"CD001":
            raise RuntimeError("sector 16 is not an ISO9660 primary volume descriptor")
        root = pvd[156:156 + pvd[156]]
        root_lba = struct.unpack_from("<I", root, 2)[0]
        root_size = struct.unpack_from("<I", root, 10)[0]
        for path, lba, size in walk_directory(disc, root_lba, root_size):
            if args.match.lower() in path.lower():
                print(f"{lba:6d} {size:10d} {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
