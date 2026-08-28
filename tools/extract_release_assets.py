"""Release-specific original UI records, extracted ONLY into a private pack."""
import argparse
import struct
from pathlib import Path
from extract_trade_assets import extract as extract_shared, HELP, BASE
from extract_frontend_help import extract as extract_help

# 57084 refusal descriptors; state17 Help route from original B00E0 table.
DIALOGS = (0x800AEB54, 0x800AEBEA, 0x800AEC1E)
ROUTES = ((17, 0, 0x800B152C),) + HELP[2:]
# Public structural facts only: x/y/tag, not image bytes or original records.
LAYOUT = ((0,0,'Bkga'),(128,0,'Bkgb'),(256,0,'Bkgc'),(384,0,'Bkgd'),
          (235,217,'help'),(140,10,'ba23'),
          (0,5,'brte'),(128,5,'brtf'),(256,5,'brtg'),(384,5,'brth'),
          (0,65,'brle'),(476,65,'brri'),(0,185,'brbe'),(128,185,'brbf'),
          (256,185,'brbg'),(384,185,'brbh'),(30,15,'frml'),(368,15,'frmr'),
          (54,22,'dflt'),(386,22,'dflt'),(40,16,'110p'),(370,16,'111p'))


def validate_layout(data):
    table_at = 0x80093330-BASE+17*4
    if len(data)<table_at+4 or struct.unpack_from('<I',data,table_at)[0]!=0x80097104:
        raise ValueError('unexpected Release layout pointer')
    at = 0x80097104-BASE
    for index, (x,y,tag) in enumerate(LAYOUT):
        record = at+index*16
        if len(data)<record+16:
            raise ValueError('truncated Release layout')
        actual_y,actual_x = struct.unpack_from('<hh',data,record)
        if (actual_x,actual_y)!=(x,y) or data[record+8:record+12]!=tag.encode('ascii'):
            raise ValueError('Release layout geometry/tag mismatch at '+str(index))


def extract(data):
    validate_layout(data)
    return (extract_shared(data, extra_texts=(0x8009D83A,), extra_dialogs=DIALOGS),
            extract_help(data, ROUTES))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('overlay', type=Path)
    parser.add_argument('--output', type=Path, default=Path('.local/assetpacks/release'))
    args = parser.parse_args()
    private = Path(__file__).resolve().parents[1] / '.local'
    if not args.output.resolve().is_relative_to(private.resolve()):
        parser.error('output must remain under .local')
    ui, help_pack = extract(args.overlay.read_bytes())
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output/'ui.n97trade').write_bytes(ui)
    (args.output/'help.n97ui').write_bytes(help_pack)
    print(f'RELEASE ASSETS ui={len(ui)} help={len(help_pack)} bytes; state17 one-stage Help, '
          'three original refusal descriptors; 22 layout records verified, ba23 at140,10; private only')


if __name__ == '__main__':
    main()
