"""Extract original View Player no-facts warning into private local assets."""
import argparse
from pathlib import Path
import struct

ADDRESS=0x800AFE06
HEADER=struct.pack('<hhhBBBB',136,90,240,64,1,2,0)

def extract(data):
    start=ADDRESS-0x80015000
    if data[start:start+10]!=HEADER:
        raise ValueError('unexpected original no-facts descriptor')
    end=start+10
    for _ in range(2):
        if end>=len(data) or data[end]!=1:
            raise ValueError('unexpected no-facts alignment')
        stop=data.find(b'\0',end+1,end+122)
        if stop<0 or any(c<32 or c>126 for c in data[end+1:stop]):
            raise ValueError('invalid no-facts text')
        end=stop+1
    prompt=0x8002502C-0x80015000 # 40E20 adds this shared prompt, not in descriptor.
    stop=data.find(b'\0',prompt,min(prompt+121,len(data)))
    if stop<0 or any(c<32 or c>126 for c in data[prompt:stop]):
        raise ValueError('invalid no-facts continuation prompt')
    return data[start:end]+b'\1'+data[prompt:stop+1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('overlay',type=Path)
    parser.add_argument('--output',type=Path,default=Path('.local/assetpacks/player/no-facts.n97ui'))
    args=parser.parse_args()
    private=Path(__file__).resolve().parents[1]/'.local'
    if not args.output.resolve().is_relative_to(private.resolve()):
        parser.error('original text must stay under .local')
    packed=extract(args.overlay.read_bytes())
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(packed)
    print(f'PLAYER NOTICE ASSET 0x{ADDRESS:08X}; {len(packed)} bytes -> {args.output}; local only')

if __name__=='__main__':
    main()
