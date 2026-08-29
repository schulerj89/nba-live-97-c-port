#!/usr/bin/env python3
"""Extract the private Create Player model family from a raw PS1 image.

The output is deliberately restricted to .local/.  No copyrighted game data
or generated manifest is suitable for publication.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

from extract_raw_cd_file import extract
from list_raw_cd_files import USER_DATA_SIZE, read_extent, walk_directory


SHARED = {
    "ZDOMCSHD.BIN", "ZDOMHAIR.BIN", "ZDOMLTRS.BIN", "ZDOMPLYR.BIN",
    "ZDOMPSKN.BIN",
}


def iso_entries(disc_path: Path) -> dict[str, tuple[int, int, str]]:
    with disc_path.open("rb") as disc:
        pvd = read_extent(disc, 16, USER_DATA_SIZE)
        if pvd[1:6] != b"CD001":
            raise RuntimeError("sector 16 is not an ISO9660 primary volume descriptor")
        root = pvd[156:156 + pvd[156]]
        root_lba = struct.unpack_from("<I", root, 2)[0]
        root_size = struct.unpack_from("<I", root, 10)[0]
        result = {}
        for path, lba, size in walk_directory(disc, root_lba, root_size):
            result[Path(path).name.upper()] = (lba, size, path)
        return result


def wanted(name: str) -> bool:
    return name in SHARED or (
        name.startswith(("ZDOME", "ZDOMF", "ZDOMS")) and name.endswith(".BIN")
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("disc", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    local = (repo / ".local").resolve()
    if output != local and local not in output.parents:
        raise RuntimeError("private model output must remain beneath the ignored .local directory")

    entries = iso_entries(args.disc)
    selected = sorted((name, *entry) for name, entry in entries.items() if wanted(name))
    families = {prefix: sum(name.startswith(prefix) for name, *_ in selected)
                for prefix in ("ZDOME", "ZDOMF", "ZDOMS")}
    if len(SHARED.intersection(name for name, *_ in selected)) != len(SHARED):
        raise RuntimeError("disc is missing a required shared ZDOM model asset")
    if len(set(families.values())) != 1 or next(iter(families.values())) < 29:
        raise RuntimeError(f"incomplete or mismatched E/F/S team families: {families}")

    files = []
    for name, lba, size, source_path in selected:
        destination = output / name
        extract(args.disc, destination, lba, size)
        digest = hashlib.sha256(destination.read_bytes()).hexdigest()
        files.append({"name": name, "lba": lba, "size": size,
                      "sha256": digest, "source_path": source_path})
        print(f"CREATE-MODEL extracted {name} lba={lba} bytes={size} sha256={digest[:12]}")

    manifest = {"schema_version": 1, "private": True,
                "source": args.disc.name, "families": families, "files": files}
    (output / "manifest.local.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"CREATE-MODEL PASS {len(files)} private files -> {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
