"""Extract a bounded, PRIVATE state-3 text/layout/sprite pack from local assets."""
import argparse
from pathlib import Path
import struct
import sys
from extract_frontend_help import extract as extract_help
from decode_team_backgrounds import load_archive, indexed_pack, PALETTE_TAGS

BASE = 0x80015000
LABELS = (0x800C161C, 0x800C1625, 0x800C162F, 0x800C163D, 0x800C1646)

def extract_ui(data):
    def raw(address, size):
        at = address - BASE
        if at < 0 or at + size > len(data):
            raise ValueError('state-3 source outside FEONLY')
        return data[at:at+size]
    def u32(address): return struct.unpack('<I', raw(address, 4))[0]
    def string(address):
        text = raw(address, min(128, len(data)-(address-BASE))).split(b'\0')[0]
        if not text or len(text) == 128 or any(c < 32 or c > 126 for c in text):
            raise ValueError('invalid state-3 string')
        return text
    if (u32(0x8009333C), u32(0x800933EC)) != (0x80093C54, 0x80093D94):
        raise ValueError('unexpected state-3 layout route')
    if raw(0x800937D4, 9) != b'zset1.psp' or u32(0x8009AF5C) != 0x8009B048:
        raise ValueError('unexpected state-3 pack/logo route')
    texts = [string(0x80025998)] + [string(a+1) for a in LABELS]
    for team in range(31):
        address = u32(0x8009D598+team*4)
        if team in (11, 12): address += len(string(address))+1
        city = string(address)
        texts += [city, string(address+len(city)+1)]
    texts += [raw(0x8009B048+team*4, 4) for team in range(31)]
    packed = bytearray(struct.pack('<4s4H', b'N97S', 1, 31, 18, 5))
    packed += raw(0x800C73E4, 6*4)
    packed += raw(0x800A3234, 29*2)
    for text in texts: packed += struct.pack('<H', len(text)) + text
    for address in range(0x80093C54, 0x80093D74, 16):
        record=raw(address,16)
        y,x,_,z=struct.unpack_from('<4h',record)
        packed += struct.pack('<4h4s',x,y,z,record[13],record[8:12])
    return bytes(packed), texts[-31:]

def decode_sprite(entry, archive, palette=None):
    from PIL import Image
    from reversebox.compression.compression_refpack import RefpackHandler
    from src.EA_Image.common_ea_dir import get_palette_info_dto_from_dir_entry
    pixels = RefpackHandler().decompress_data(entry.raw_data) if entry.h_record_id & 0x80 else entry.raw_data
    kind = entry.h_record_id & 0x7f
    if kind not in (0x40, 0x41): raise ValueError(f'unsupported sprite encoding {entry.tag}: {kind:x}')
    if len(pixels) % entry.h_height: raise ValueError('partial sprite scanline')
    stride = len(pixels)//entry.h_height
    if stride*(2 if kind==0x40 else 1) < entry.h_width: raise ValueError('short sprite scanline')
    palette = palette or get_palette_info_dto_from_dir_entry(entry, archive).data
    colors = struct.unpack('<'+'H'*(len(palette)//2),palette)
    out=bytearray()
    for y in range(entry.h_height):
        for x in range(entry.h_width):
            byte=pixels[y*stride+(x//2 if kind==0x40 else x)]
            index=((byte>>(4*(x&1)))&15) if kind==0x40 else byte
            word=colors[index]
            out.extend([(((word>>(c*5))&31)<<3)|(((word>>(c*5))&31)>>2) for c in range(3)])
            # PS1 texel zero is transparent; 0x8000 remains an opaque black
            # texel in this non-semitransparent sprite slice. Never RGB-key.
            out.append(255 if word else 0)
    return Image.frombytes('RGBA',(entry.h_width,entry.h_height),bytes(out))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path('.local/assetpacks'))
    p.add_argument('--overlay',type=Path,default=Path('.local/extracted/FEONLY.BIN'))
    p.add_argument('--ea-tool',type=Path,default=Path('.local/tools/EA-Graphics-Manager'))
    a=p.parse_args()
    output=a.root/'team_select'
    private=Path(__file__).resolve().parents[1]/'.local'
    if not output.resolve().is_relative_to(private.resolve()): p.error('output must remain under .local')
    sys.path.insert(0,str(a.ea_tool.resolve()))
    from src.EA_Image.common_ea_dir import get_palette_info_dto_from_dir_entry
    from reversebox.compression.compression_refpack import RefpackHandler
    data=a.overlay.read_bytes()
    packed,logos=extract_ui(data)
    archive=load_archive(a.root/'menu'/'ZSET1.PSP')
    entries={e.tag:e for e in archive.dir_entry_list}
    palettes=load_archive(a.root/'menu'/'ZTMPAL.PSH')
    bank=[(e.tag,bytes(get_palette_info_dto_from_dir_entry(e,palettes).data[:320])) for e in palettes.dir_entry_list]
    strips=[]
    for tag in ('Bkga','Bkgb','Bkgc','Bkgd'):
        e=entries[tag]
        if (e.h_width,e.h_height,e.h_record_id&0x7f)!=(128,240,0x41): raise ValueError('wrong background')
        pixels=RefpackHandler().decompress_data(e.raw_data) if e.h_record_id&0x80 else e.raw_data
        strips.append((bytes(pixels),bytes(get_palette_info_dto_from_dir_entry(e,archive).data[320:512])))
    output.mkdir(parents=True,exist_ok=True)
    (output/'ui.n97select').write_bytes(packed)
    (output/'help.n97ui').write_bytes(extract_help(data,((3,0,0x800B0474),)))
    (output/'indexed.n97pal').write_bytes(indexed_pack(bank,strips))
    tags=['help','ba08','brte','brtf','brtg','brth','brle','brri','brbe','brbf','brbg','brbh','frmr','frml']
    # 31F48 applies context Pal0 to layout indices6..17; title5 and logos18/19
    # retain their own CLUTs. This is state3 data routing, not a blue tint.
    pal0=bytes(get_palette_info_dto_from_dir_entry(entries['Pal0'],archive).data)
    if len(pal0)!=32: raise ValueError('unexpected Pal0 size')
    for tag in tags+[t.decode('ascii') for t in logos]:
        decode_sprite(entries[tag],archive,pal0 if tag in tags[2:] else None).save(output/(tag+'.png'))
    print(f'TEAM SELECT private pack: {len(packed)} UI bytes; 31 team descriptors, 18 layout records, 45 sprites, 1 Help page, indexed background')

if __name__=='__main__': main()
