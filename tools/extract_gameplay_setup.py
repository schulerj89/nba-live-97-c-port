"""Extract verified private motion and period tables for native match setup.

No original bytes are embedded in public code. Preserve existing identical
outputs/timestamps and refuse conflicting or aliased paths before publishing.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib
from list_raw_cd_files import read_extent

ROOT=Path(__file__).resolve().parents[1]
PRIVATE=ROOT/'.local'
DISC_SHA='0bb357498e9465a940ef955921c46f790b1f73fd408c503c073dbcc0ec50753c'
GAME_SHA='d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0'
MOCAP_SHA='31ef711fb043c1d8b2ae22c15487af0fb32cf6b1fe86ccec65f50166db5fa559'

def build_period_pack(game):
    if len(game)!=1009196:
        raise ValueError('invalid GAMEONLY size')
    def span(address,count):return game[address-0x80015000:address-0x80015000+count]
    # Each formation occupies32 bytes (30 consumed). Source duration option is
    # an unchecked byte, so retain all256 possible word reads, not just five.
    # Adjacent data for unusual options is not relabeled as valid UI choices.
    payload=span(0x800b891c,64)+span(0x800b895c,1024)+span(0x800b8970,1024)
    assert len(payload)==2112
    return b'NBA97PER'+struct.pack('<III',1,len(payload),zlib.crc32(payload))+payload

def extract(disc,overlay,output):
    disc,overlay,output=Path(disc).resolve(),Path(overlay).resolve(),Path(output).resolve()
    names=['ZMOCAP.BIN','period_setup.bin','gameplay_setup.json']
    destinations=[(output/name).resolve() for name in names]
    if any(not p.is_relative_to(PRIVATE.resolve()) for p in destinations):
        raise ValueError('gameplay outputs must remain below repository .local')
    paths=[disc,overlay]+destinations
    for i,path in enumerate(paths):
        for other in paths[i+1:]:
            if path==other or (path.exists() and other.exists() and path.samefile(other)):
                raise ValueError('gameplay sources and outputs must not alias')
    if disc.stat().st_size!=726939696:
        raise ValueError('unsupported disc size')
    with disc.open('rb') as source:
        if hashlib.file_digest(source,'sha256').hexdigest()!=DISC_SHA:
            raise ValueError('unsupported disc hash')
        mocap=read_extent(source,250054,200044)
    if hashlib.sha256(mocap).hexdigest()!=MOCAP_SHA:
        raise ValueError('unsupported original motion resource')
    game=overlay.read_bytes()
    if hashlib.sha256(game).hexdigest()!=GAME_SHA:
        raise ValueError('unsupported GAMEONLY hash')
    pack=build_period_pack(game)
    manifest=dict(version=1,disc_sha256=DISC_SHA,game_sha256=GAME_SHA,
        mocap_sha256=MOCAP_SHA,period_pack_sha256=hashlib.sha256(pack).hexdigest(),
        formation_sources=['GAME:800B891C','GAME:800B893C'],
        duration_sources=['GAME:800B895C','GAME:800B8970'],duration_words_each=256,
        scope='Raw resource ownership only; no sampling, period execution or gameplay claim')
    prepared=[mocap,pack,(json.dumps(manifest,indent=2)+'\n').encode()]
    for path,data in zip(destinations,prepared):
        if path.exists() and path.read_bytes()!=data:
            raise ValueError('existing gameplay output differs; use a fresh private folder: '+str(path))
    output.mkdir(parents=True,exist_ok=True)
    for path,data in zip(destinations,prepared):
        if not path.exists():
            with path.open('xb') as target:target.write(data)
    return manifest

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--disc',type=Path,default=PRIVATE/'input/nba-live-97-slus-00267.bin')
    parser.add_argument('--overlay',type=Path,default=PRIVATE/'extracted/GAMEONLY.BIN')
    parser.add_argument('--output',type=Path,default=PRIVATE/'assetpacks/gameplay')
    args=parser.parse_args()
    try:manifest=extract(args.disc,args.overlay,args.output)
    except (ValueError,OSError) as error:parser.error(str(error))
    print(json.dumps(manifest,indent=2))
if __name__=='__main__':main()
