#!/usr/bin/env python3
"""Extract one ISO9660 file from a raw 2352-byte/sector PS1 disc image.

The caller supplies the ISO LBA and byte size reported by ps1Analyzer.
Mode 2 Form 1 user data begins 24 bytes into each raw sector and contains
2048 bytes. The output path should normally be below the ignored `.local/`
directory.
"""

from __future__ import annotations

import argparse
from pathlib import Path


RAW_SECTOR_SIZE = 2352
USER_DATA_OFFSET = 24
USER_DATA_SIZE = 2048


def extract(source: Path, output: Path, lba: int, size: int,
            raw_sectors: bool = False) -> None:
    if raw_sectors:
        sector_count = (size + USER_DATA_SIZE - 1) // USER_DATA_SIZE
        output.parent.mkdir(parents=True, exist_ok=True)
        with source.open("rb") as disc, output.open("wb") as dst:
            disc.seek(lba * RAW_SECTOR_SIZE)
            remaining = sector_count * RAW_SECTOR_SIZE
            while remaining:
                chunk = disc.read(min(1024 * 1024, remaining))
                if not chunk:
                    raise EOFError("disc ended during raw-sector extraction")
                dst.write(chunk)
                remaining -= len(chunk)
        return

    remaining = size
    output.parent.mkdir(parents=True, exist_ok=True)

    with source.open("rb") as disc, output.open("wb") as dst:
        sector = lba
        while remaining:
            disc.seek(sector * RAW_SECTOR_SIZE + USER_DATA_OFFSET)
            chunk = disc.read(min(USER_DATA_SIZE, remaining))
            if not chunk:
                raise EOFError(f"disc ended while reading raw sector {sector}")
            dst.write(chunk)
            remaining -= len(chunk)
            sector += 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="raw 2352-byte-sector BIN/IMG")
    parser.add_argument("output", type=Path)
    parser.add_argument("--lba", type=int, required=True)
    parser.add_argument("--size", type=int, required=True)
    parser.add_argument(
        "--raw-sectors", action="store_true",
        help="preserve complete 2352-byte sectors (required by PSX STR/XA)")
    args = parser.parse_args()

    extract(args.source, args.output, args.lba, args.size, args.raw_sectors)
    kind = "raw-sector extent for" if args.raw_sectors else ""
    print(f"extracted {kind} {args.size} bytes from LBA {args.lba} to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
