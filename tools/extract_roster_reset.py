"""Extract the original Reset confirmation only into private local assets."""
import argparse
from pathlib import Path
import struct

ADDRESS=0x800AEDD2
HEADER=struct.pack('<hhhBBBB',121,75,270,110,1,5,2)

def extract(data):
    start=ADDRESS-0x80015000
    if data[start:start+10]!=HEADER:
        raise ValueError('unexpected original Reset descriptor')
    end=start+10
    for _ in range(7):
        if end>=len(data) or data[end]!=1:
            raise ValueError('unexpected Reset alignment')
        next_end=data.find(b'\0',end+1,end+122)
        if next_end<0 or any(c<32 or c>126 for c in data[end+1:next_end]):
            raise ValueError('invalid Reset line')
        end=next_end+1
    return data[start:end]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('overlay',type=Path)
    parser.add_argument('--output',type=Path,default=Path('.local/assetpacks/reorder/reset.n97ui'))
    args=parser.parse_args()
    private=Path(__file__).resolve().parents[1]/'.local'
    if not args.output.resolve().is_relative_to(private.resolve()):
        parser.error('original text must stay under .local')
    packed=extract(args.overlay.read_bytes())
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(packed)
    print(f'RESET ASSET 0x{ADDRESS:08X}; {len(packed)} bytes -> {args.output}; local only')

if __name__=='__main__':
    main()
