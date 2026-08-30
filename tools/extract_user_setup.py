"""Extract the PRIVATE state5 layout, text, alphabet, Help and unique sprites."""
import argparse
from pathlib import Path
import struct
import sys
from extract_frontend_help import extract as extract_help
from extract_team_select import decode_sprite
from decode_team_backgrounds import load_archive

BASE=0x80015000

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path('.local/assetpacks'))
    p.add_argument('--overlay',type=Path,default=Path('.local/extracted/FEONLY.BIN'))
    p.add_argument('--ea-tool',type=Path,default=Path('.local/tools/EA-Graphics-Manager'))
    a=p.parse_args()
    out=a.root/'user_setup'
    private=Path(__file__).resolve().parents[1]/'.local'
    if not out.resolve().is_relative_to(private.resolve()): p.error('output must stay private under .local')
    data=a.overlay.read_bytes()
    def raw(address,size):
        at=address-BASE
        if at<0 or at+size>len(data): raise ValueError('state5 data outside overlay')
        return data[at:at+size]
    def u32(address): return struct.unpack('<I',raw(address,4))[0]
    def string(address):
        text=raw(address,128).split(b'\0',1)[0]
        if not 0<len(text)<128 or any(c<32 or c>126 for c in text): raise ValueError('invalid state5 string')
        return text
    if (u32(0x80093344),u32(0x800933F4))!=(0x80094444,0x80094674): raise ValueError('wrong state5 layout route')
    if raw(0x80093804,9)!=b'zset1.psp': raise ValueError('wrong state5 archive')
    alphabet=raw(u32(0x80098170),68)
    if len(set(alphabet))!=68 or any(c<32 or c>126 for c in alphabet): raise ValueError('invalid editor alphabet')
    b=bytearray(struct.pack('<4s4H',b'N97U',1,35,8,68))
    b+=alphabet+raw(0x80098174,32)+raw(0x80021EA6,8)
    for address in (0x80024B8C,0x80024B98):
        text=string(address);b+=struct.pack('<H',len(text))+text
    for address in range(0x80094444,0x80094674,16):
        r=raw(address,16);y,x,visible,z=struct.unpack_from('<4h',r)
        b+=struct.pack('<4h4s',x,y,z,r[13],r[8:12])
    sys.path.insert(0,str(a.ea_tool.resolve()))
    archive=load_archive(a.root/'menu/ZSET1.PSP')
    entries={e.tag:e for e in archive.dir_entry_list}
    out.mkdir(parents=True,exist_ok=True)
    (out/'ui.n97users').write_bytes(b)
    (out/'help.n97ui').write_bytes(extract_help(data,((5,0,0x800B0640),(5,1,0x800B0720))))
    dialogs=bytearray(struct.pack('<4sHH',b'N97M',1,4))
    for address in (0x800B008E,0x800B002C,0x800AFFAC,0x800AFF4C):
        header=raw(address,10)
        if header[7]!=1 or header[8]!=3 or header[9] not in (0,2): raise ValueError('wrong User Setup modal')
        end=address+10
        for _ in range(header[8]+header[9]):
            if raw(end,1)!=b'\1': raise ValueError('unexpected modal alignment')
            end+=1
            while raw(end,1)!=b'\0':
                if not 32<=raw(end,1)[0]<=126: raise ValueError('invalid modal text')
                end+=1
            end+=1
        blob=raw(address,end-address)
        dialogs+=struct.pack('<IH',address,len(blob))+blob
    continuation=string(0x8002502C)
    dialogs+=struct.pack('<H',len(continuation))+continuation
    dialogs+=raw(0x80021DA0,1)
    (out/'dialogs.n97ui').write_bytes(dialogs)
    # Shared border/frame/logo resources remain owned by Team Select's pack.
    for tag in ('hel1','hel2','ba39','cnt3','cnt2','cnt1'):
        decode_sprite(entries[tag],archive).save(out/(tag+'.png'))
    print(f'USER SETUP PRIVATE:35 layout records,8 colors,68 editor glyphs,2 Help pages,4 dialogs,6 unique sprites; {len(b)} UI bytes')

if __name__=='__main__': main()
