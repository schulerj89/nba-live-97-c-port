"""Extract source-selected Help descriptors into a small PRIVATE asset pack."""
import argparse
from pathlib import Path
import struct

BASE = 0x80015000
TABLE = 0x800B00E0
# State, descriptor index, expected record address. Text is never copied to code.
ROUTES = ((12, 0, 0x800B0F68), (12, 1, 0x800B102C),
          (35, 0, 0x800B2194), (36, 0, 0x800B22F0))
LIMIT = 16384


def record_at(data, address):
    at = address - BASE
    if at < 0 or at + 10 > len(data):
        raise ValueError('Help descriptor outside overlay')
    x, y, width, height, style, lines, choices = struct.unpack_from('<hhhBBBB', data, at)
    if not (0 <= x <= 246 and 0 <= y <= 110 and 20 <= width <= 512-x and
            10 <= height <= 240-y and style == 0 and 1 <= lines <= 16 and choices == 0):
        raise ValueError('unsupported Help geometry/style')
    end = at + 10
    for _ in range(lines):
        if end >= len(data) or data[end] not in (0, 1):
            raise ValueError('invalid Help alignment')
        extra = 2 if data[end] == 0 else 1
        if end + extra > len(data):
            raise ValueError('truncated Help alignment')
        if extra == 2 and data[end+1] >= width:
            raise ValueError('Help offset exceeds panel')
        end += extra
        text_end = data.find(b'\0', end, min(end+257, len(data)))
        if text_end < 0:
            raise ValueError('unterminated/oversized Help line')
        cursor = end
        while cursor < text_end:
            if data[cursor] == 0x1f:
                cursor += 1
                if cursor == text_end:
                    raise ValueError('truncated inline spacing')
            elif data[cursor] < 32:
                raise ValueError('unsupported inline control')
            cursor += 1
        end = text_end + 1
    return data[at:end]


def extract(data, routes=ROUTES):
    def pointer(address):
        at = address - BASE
        if at < 0 or at + 4 > len(data):
            raise ValueError('Help table outside overlay')
        return struct.unpack_from('<I', data, at)[0]
    records = []
    for state, index, expected in routes:
        table = pointer(TABLE + state * 4)
        address = pointer(table + index * 4)
        if address != expected:
            raise ValueError('unexpected original Help route')
        record = record_at(data, address)
        records.append(struct.pack('<BBHI', state, index, len(record), address) + record)
    packed = b'N97H' + struct.pack('<HH', 1, len(records)) + b''.join(records)
    if len(packed) > LIMIT:
        raise ValueError('Help pack exceeds bound')
    return packed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('overlay', type=Path)
    parser.add_argument('--output', type=Path, default=Path('.local/assetpacks/reorder/help.n97ui'))
    args = parser.parse_args()
    private = Path(__file__).resolve().parents[1] / '.local'
    if not args.output.resolve().is_relative_to(private.resolve()):
        parser.error('extracted Help text must remain under repository .local')
    packed = extract(args.overlay.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(packed)
    print(f'HELP ASSET 40FCC pointer routes -> {args.output}; records=4 bytes={len(packed)} local-only')


if __name__ == '__main__':
    main()
