"""Extract two original Re-order message descriptors; output MUST stay private."""
import argparse
from pathlib import Path
import struct


def extract(data):
    records = []
    for address in (0x800AFFFA, 0x800AFC22):
        offset = address - 0x80015000
        x, y, width, height, style, lines, choices = struct.unpack_from('<hhhBBBB', data, offset)
        if style != 1 or choices or not 1 <= lines <= 4:
            raise ValueError('unexpected original message descriptor')
        end = offset + 10
        for _ in range(lines):
            if data[end] != 1:
                raise ValueError('expected centered original dialog line')
            end = data.index(b'\0', end + 1) + 1
        record = data[offset:end]
        records.append(struct.pack('<I', len(record)) + record)
    return b'N97D\x01\0\0\0' + b''.join(records)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('overlay', type=Path)
    parser.add_argument('--output', type=Path, default=Path('.local/assetpacks/reorder/dialogs.n97ui'))
    args = parser.parse_args()
    private_root = Path(__file__).resolve().parents[1] / '.local'
    if not args.output.resolve().is_relative_to(private_root.resolve()):
        parser.error('extracted game text must remain under the repository .local directory')
    packed = extract(args.overlay.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(packed)
    # Separate optional screen pack: keep the tested two-message v1 format
    # stable. 80056254's confirmation has four body lines and two choices.
    data = args.overlay.read_bytes()
    start = 0x800AF4F8 - 0x80015000
    if data[start:start+10] != struct.pack('<hhhBBBB', 141, 80, 230, 100, 1, 4, 2):
        raise ValueError('unexpected original discard confirmation')
    end = start + 10
    for _ in range(6):
        if data[end] != 1:
            raise ValueError('unexpected discard alignment')
        end = data.index(b'\0', end+1)+1
    args.output.with_name('discard.n97ui').write_bytes(data[start:end])
    print(f'REORDER ASSET original modal descriptors -> {args.output} ({len(packed)} bytes; local only)')


if __name__ == '__main__':
    main()
