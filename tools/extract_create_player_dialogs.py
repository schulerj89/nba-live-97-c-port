"""Extract original Create Player Delete confirmations; output stays private."""
import argparse
from pathlib import Path
import struct

BASE = 0x80015000
ADDRESSES = (0x800AF352, 0x800AF3D6, 0x800AF460)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("overlay", type=Path)
    parser.add_argument("--output", type=Path,
                        default=Path(".local/assetpacks/create_player/delete.n97ui"))
    args = parser.parse_args()
    private = Path(__file__).resolve().parents[1] / ".local"
    if not args.output.resolve().is_relative_to(private.resolve()):
        parser.error("extracted game text must remain beneath .local")
    data = args.overlay.read_bytes()
    records = []
    expected = ((141, 75, 230, 100, 4), (130, 75, 250, 100, 4),
                (130, 70, 250, 110, 5))
    for address, shape in zip(ADDRESSES, expected):
        start = address - BASE
        x, y, width, height, style, lines, choices = struct.unpack_from("<hhhBBBB", data, start)
        if (x, y, width, height, lines) != shape or style != 1 or choices != 2:
            raise ValueError(f"unexpected Delete descriptor at {address:08X}")
        end = start + 10
        for _ in range(lines + choices):
            if data[end] != 1:
                raise ValueError("Delete dialog line is not centered")
            end = data.index(b"\0", end + 1) + 1
        raw = data[start:end]
        records.append(struct.pack("<II", address, len(raw)) + raw)
    packed = b"N97D\x01\0\x03\0" + b"".join(records)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(packed)
    print(f"CREATE PLAYER ASSET 3 original Delete dialogs -> {args.output} ({len(packed)} bytes; local only)")


if __name__ == "__main__":
    main()
