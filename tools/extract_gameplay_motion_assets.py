"""Extract verified private animation lookup, foot-offset and trig resources.

Original data stays below .local. Existing identical files retain timestamps;
conflicts and source/output aliases are refused before writing any output.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib
from extract_gameplay_setup import DISC_SHA, GAME_SHA, PRIVATE
from list_raw_cd_files import read_extent

FOOT_SHA='14389433d47a33d16d5bd4ee9e82b2ffdf85b85e7f8f7f8a1944f6df456d0061'
TRIG_SHA='c3b03a2581960f9b22f2f29fb52f30bf36eb87c3896c8e4f62c708b3c247f880'

def extract(disc,overlay,output):
    disc,overlay,output=Path(disc).resolve(),Path(overlay).resolve(),Path(output).resolve()
    names=['animation_maps.bin','ZHOTS.BIN','foot_trig.bin','gameplay_motion_assets.json']
    destinations=[(output/name).resolve() for name in names]
    if any(not p.is_relative_to(PRIVATE.resolve()) for p in destinations):
        raise ValueError('gameplay motion outputs must remain below repository .local')
    paths=[disc,overlay]+destinations
    for i,path in enumerate(paths):
        for other in paths[i+1:]:
            if path==other or (path.exists() and other.exists() and path.samefile(other)):
                raise ValueError('gameplay motion sources and outputs must not alias')
    if disc.stat().st_size!=726939696:
        raise ValueError('unsupported disc size')
    with disc.open('rb') as source:
        if hashlib.file_digest(source,'sha256').hexdigest()!=DISC_SHA:
            raise ValueError('unsupported disc hash')
        foot=read_extent(source,249065,25284)
    if hashlib.sha256(foot).hexdigest()!=FOOT_SHA:
        raise ValueError('unsupported original foot resource')
    game=overlay.read_bytes()
    if len(game)!=1009196 or hashlib.sha256(game).hexdigest()!=GAME_SHA:
        raise ValueError('unsupported GAMEONLY hash')
    trig=game[0xd6e30-0x15000:0xd6e30-0x15000+1028]
    if hashlib.sha256(trig).hexdigest()!=TRIG_SHA:
        raise ValueError('unsupported original foot trig window')
    # First remap lookup consumes unsigned4E; subsequent LH results are signed.
    # Retain all possible reads, including adjacent non-map data for raw indices.
    window=game[0xa850c-0x15000:0xd8590-0x15000]
    assert len(window)==0x30084
    maps=b'NBA97ANI'+struct.pack('<III',1,len(window),zlib.crc32(window))+window
    manifest=dict(version=1,disc_sha256=DISC_SHA,game_sha256=GAME_SHA,
        foot=dict(file='ZHOTS.BIN',lba=249065,size=25284,sha256=FOOT_SHA),
        trig=dict(file='foot_trig.bin',source='GAME:800D6E30',size=1028,sha256=TRIG_SHA),
        animation=dict(file='animation_maps.bin',source_start='GAME:800A850C',source_end_exclusive='GAME:800D8590',
            sha256=hashlib.sha256(maps).hexdigest(),lookup_words_each=65536,
            maps=['800B850C','800B8538','800B8564','800B8590','800B85BC','800B85E8','800B8614'],
            first_indices=[-32768,-32768,0,0,-32768,-32768,-32768]),
        scope='Original owned data only; no scene, possession or natural-entry claim')
    prepared=[maps,foot,trig,(json.dumps(manifest,indent=2)+'\n').encode()]
    for path,data in zip(destinations,prepared):
        if path.exists() and path.read_bytes()!=data:
            raise ValueError('existing motion output differs; use a fresh private folder: '+str(path))
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
