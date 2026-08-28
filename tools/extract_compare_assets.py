"""Extract bounded private Compare labels/field IDs; no original text in source."""
from pathlib import Path
import argparse
import struct

BASE = 0x80015000
TABLES = ((0x800A56D0, 14), (0x800A5648, 17), (0x800A54C8, 24))
HEADERS = (0x800A47EC, 0x800A4810, 0x800A4818)
STARTERS = (0x80024C98, 0x80024CA4, 0x80024CB0, 0x80024CBC, 0x80024CC8)


def extract(data):
    def word(address):
        at = address - BASE
        if at < 0 or at+4 > len(data):
            raise ValueError('Compare pointer outside overlay')
        return struct.unpack_from('<I', data, at)[0]

    def string(address):
        at = address - BASE
        if at < 0 or at >= len(data):
            raise ValueError('Compare string outside overlay')
        end = data.find(b'\0', at, min(at+129, len(data)))
        if end <= at or any(c < 32 or c > 126 for c in data[at:end]):
            raise ValueError('invalid/oversized Compare string')
        raw = data[at:end]
        return struct.pack('<H', len(raw)) + raw

    # 3B26C type0 resolves object27's +34 table index through 8009AF40.
    index = word(0x800A5360+0x34)
    if index != 10 or word(0x8009AF40+index*4) != 0x8009D34C:
        raise ValueError('unexpected Compare layer-label table')
    texts = list(HEADERS) + [word(0x8009D34C+i*4) for i in range(4)] + list(STARTERS)
    # 4ECA8 maps team29 to name record31 in frontend states >5.
    texts.append(word(0x8009D598+31*4))
    out = bytearray(b'N97C'+struct.pack('<HH', 1, len(texts)))
    out += b''.join(string(p) for p in texts)
    for address, count in TABLES:
        out += struct.pack('<H', count)
        for i in range(count):
            field = word(address+i*8+4)
            if field != 0xFFFFFFFF and field > 66:
                raise ValueError('unsupported Compare field')
            out += struct.pack('<h', -1 if field == 0xFFFFFFFF else field)
            out += string(word(address+i*8))
    if len(out) > 8192:
        raise ValueError('Compare pack exceeds bound')
    return bytes(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('overlay', type=Path)
    parser.add_argument('--output', type=Path, default=Path('.local/assetpacks/reorder/compare.n97ui'))
    args = parser.parse_args()
    private = Path(__file__).resolve().parents[1]/'.local'
    if not args.output.resolve().is_relative_to(private.resolve()):
        parser.error('Compare assets must remain under repository .local')
    out = extract(args.overlay.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(out)
    print(f'COMPARE ASSET 13 texts + 55 field descriptors; bytes={len(out)} local-only -> {args.output}')


if __name__ == '__main__':
    main()
