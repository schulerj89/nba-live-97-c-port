"""Extract bounded Trade text/notice/preference packs. All output is private."""
import argparse
from pathlib import Path
import struct
from extract_frontend_help import extract as extract_help

BASE=0x80015000
HELP=((13,0,0x800B10D0),(13,1,0x800B11AC),(35,0,0x800B2194),(36,0,0x800B22F0))
TEXTS=(0x8002655C,0x80026574,0x80026588,0x8002659C,
       0x80026508,0x8002650C,0x80026510,0x80026514,0x80026518,0x8002651C,0x8002502C,0x80024E60,
       0x800264EC,0x800264F8)
DIALOGS=(0x800AEBB2,0x800AECBE,0x800AFC22,0x800AF4F8,0x800AEE88,0x800AEEF6)

def cstring(data, at):
    if at<0 or at>=len(data): raise ValueError('text outside overlay')
    end=data.find(b'\0',at,min(at+257,len(data)))
    if end<0 or any(c<32 or c>126 for c in data[at:end]): raise ValueError('invalid original text')
    return data[at:end+1]

def extract(data, sign=False):
    records=[]
    for address in TEXTS + ((0x8009D83A,) if sign else ()):
        records.append((address,cstring(data,address-BASE)))
    for address in DIALOGS + ((0x800AED20,0x800AEC72,0x800AED88) if sign else ()):
        at=address-BASE
        if at<0 or at+10>len(data): raise ValueError('descriptor outside overlay')
        x,y,w,h,style,lines,choices=struct.unpack_from('<hhhBBBB',data,at)
        if not (0<=x<=246 and 0<=y<=110 and 20<=w<=512-x and
                10<=h<=240-y and style==1 and 1<=lines<=8 and choices in (0,2)):
            raise ValueError('unsupported Trade descriptor')
        end=at+10
        for _ in range(lines+choices):
            if end>=len(data) or data[end]!=1: raise ValueError('unsupported alignment')
            end+=1+len(cstring(data,end+1))
        records.append((address,data[at:end]))
    at=0x800265AC-BASE
    preference=data[at:at+25]
    if len(preference)!=25 or any(x>4 for x in preference): raise ValueError('invalid position preference table')
    records.append((0x800265AC,preference))
    return b'N97T'+struct.pack('<HH',1,len(records))+b''.join(
        struct.pack('<II',a,len(b))+b for a,b in records)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('overlay',type=Path)
    parser.add_argument('--output',type=Path,default=Path('.local/assetpacks/trade'))
    parser.add_argument('--sign',action='store_true',help='Sign descriptors/Help in a separate private pack')
    args=parser.parse_args()
    private=Path(__file__).resolve().parents[1]/'.local'
    if not args.output.resolve().is_relative_to(private.resolve()): parser.error('output must remain under .local')
    if args.sign and args.output==Path('.local/assetpacks/trade'):
        args.output=Path('.local/assetpacks/sign')
    data=args.overlay.read_bytes(); packed=extract(data,args.sign)
    routes=((14,0,0x800B1270),(14,1,0x800B1350))+HELP[2:] if args.sign else HELP
    help_pack=extract_help(data,routes)
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'ui.n97trade').write_bytes(packed)
    (args.output/'help.n97ui').write_bytes(help_pack)
    print(f'TRADE ASSETS ui={len(packed)} help={len(help_pack)} bytes; original text/geometry/preferences; private only')
if __name__=='__main__': main()
