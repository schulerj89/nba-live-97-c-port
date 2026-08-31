"""Extract private cursor pitch data and shared RNG seed from local FEONLY."""
import argparse
import hashlib
from pathlib import Path

BASE = 0x80015000
# Full source-owner hashes validate this audited overlay without publishing code.
OWNERS = {
    0x8002F124: (84, "d1dc276e243564ca1f3ba4de1749251eb4f4859689cbad1527ec25771554c83d"),
    0x80072048: (524, "8bda9777abae51d4012ffefc0a747e6071e9ae373aad4bf13df2434e0e73c2b4"),
    0x8007A538: (208, "f514526972c3a347227f6492ed6a07e8c2696918dcc95628095703efc4bdbe93"),
}


def extract(data):
    def checked(address, size, digest):
        value = data[address-BASE:address-BASE+size]
        if len(value) != size or hashlib.sha256(value).hexdigest() != digest:
            raise ValueError(f"unsupported FEONLY source at {address:08X}")
        return value
    for address, (size, digest) in OWNERS.items():
        checked(address, size, digest)
    pitch = checked(0x800C6D60, 256,
                    "24e944a9313649312e7251afb205894bcf816f2e8005ae004e5a30a5806baa15")
    seed = checked(0x800C73E4, 24,
                   "13eb516294955f2da4796105e1ecbea3780d3effefe20f5d2ae69aabcad79bde")
    return pitch, seed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--overlay", type=Path, default=Path(".local/extracted/FEONLY.BIN"))
    parser.add_argument("--output", type=Path, default=Path(".local/assetpacks/menu/zcursor_pitch.bin"))
    args = parser.parse_args()
    private = (Path(__file__).resolve().parents[1] / ".local").resolve()
    source_path = args.overlay.resolve()
    pitch_path = args.output.resolve()
    seed_path = (args.output.parent / "frontend_rng.bin").resolve()
    if any(not path.is_relative_to(private) for path in (pitch_path, seed_path)):
        parser.error("both cursor data outputs must remain under the repository's .local directory")
    paths = (source_path, pitch_path, seed_path)
    for index, path in enumerate(paths):
        for other in paths[index+1:]:
            if path == other or (path.exists() and other.exists() and path.samefile(other)):
                parser.error("source, pitch output and seed output must be distinct files")
    data, seed = extract(source_path.read_bytes())
    pitch_path.parent.mkdir(parents=True, exist_ok=True)
    seed_path.parent.mkdir(parents=True, exist_ok=True)
    pitch_path.write_bytes(data)
    seed_path.write_bytes(seed)
    print(f"Private cursor pitch table: {len(data)} bytes -> {pitch_path}; shared seed: {len(seed)} bytes -> {seed_path}")


if __name__ == "__main__":
    main()
