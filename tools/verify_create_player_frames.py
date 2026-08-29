#!/usr/bin/env python3
"""Check pixel-local Create Player animation evidence without game assets."""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path


def ppm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    parts = data.split(b"\n", 3)
    if len(parts) != 4 or parts[0] != b"P6" or parts[2] != b"255":
        raise RuntimeError(f"bad PPM: {path}")
    width, height = map(int, parts[1].split())
    if len(parts[3]) != width * height * 3:
        raise RuntimeError(f"truncated PPM: {path}")
    return width, height, parts[3]


def crop_digest(path: Path, box: tuple[int, int, int, int]) -> str:
    width, height, pixels = ppm(path)
    left, top, right, bottom = box
    if not (0 <= left < right <= width and 0 <= top < bottom <= height):
        raise RuntimeError("crop outside frame")
    crop = bytearray()
    for y in range(top, bottom):
        crop.extend(pixels[(y * width + left) * 3:(y * width + right) * 3])
    return hashlib.sha256(crop).hexdigest()


def changed(root: Path, a: str, b: str, box: tuple[int, int, int, int], label: str) -> None:
    if crop_digest(root / a, box) == crop_digest(root / b, box):
        raise RuntimeError(f"{label} crop did not change")
    print(f"CREATE-FRAME {label}: PASS crop={box} {a} != {b}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    args = parser.parse_args()
    changed(args.capture, "editor-first-required.ppm", "editor-selector-gold.ppm",
            (35, 78, 275, 116), "20-vblank-selector-tint")
    changed(args.capture, "editor-appearance-layer.ppm", "editor-model-motion-phase.ppm",
            (335, 40, 510, 220), "18-key/36-tick-zdom-mocap-preview")
    changed(args.capture, "editor-layer-scroll-enter.ppm", "editor-layer-scroll-mid.ppm",
            (35, 125, 275, 194), "six-vblank-scroll-enter-mid")
    changed(args.capture, "editor-layer-scroll-mid.ppm", "editor-layer-scroll-settled.ppm",
            (35, 125, 275, 194), "six-vblank-scroll-mid-settled")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
