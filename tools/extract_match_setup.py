"""Extract the PRIVATE default control map used by FEONLY 80061674."""
import argparse
import hashlib
from pathlib import Path
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--overlay", type=Path, default=Path(".local/extracted/FEONLY.BIN"))
    parser.add_argument("--root", type=Path, default=Path(".local/assetpacks"))
    args = parser.parse_args()
    private = Path(__file__).resolve().parents[1] / ".local"
    target = args.root / "match_setup"
    if not target.resolve().is_relative_to(private.resolve()):
        parser.error("output must stay private under .local")
    overlay = args.overlay.read_bytes()
    def raw(address, size):
        offset = address - 0x80015000
        if offset < 0 or offset + size > len(overlay):
            raise ValueError("match data outside FEONLY")
        return overlay[offset:offset+size]
    def word(address):
        return struct.unpack("<I", raw(address, 4))[0]
    if word(0x80061718) != 0x3C06800C or word(0x8006171C) != 0x24C61CD8:
        raise ValueError("wrong default control address producer")
    if word(0x80021EE4) != 0 or word(0x80028858) != (0x0C000000 | ((0x80061674 >> 2) & 0x03FFFFFF)):
        raise ValueError("wrong cold frontend control initializer")
    controls = raw(0x800C1CD8, 59)
    target.mkdir(parents=True, exist_ok=True)
    (target / "controls.n97ctl").write_bytes(struct.pack("<4sHHI", b"N97C", 1, 59, 0x800C1CD8) + controls)
    print(f"MATCH PRIVATE:59 default control bytes; SHA256 {hashlib.sha256(controls).hexdigest()}")


if __name__ == "__main__":
    main()
