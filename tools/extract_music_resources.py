"""Extract the verified NTSC-U five CNK resources and source music filename slots.

Outputs stay private. Existing identical outputs are retained byte-for-byte;
different existing outputs are refused, never silently overwritten. XA media
are a separate subsystem and are not decoded or relabeled here.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT / '.local'
DISC_SIZE = 726939696
DISC_SHA256 = '0bb357498e9465a940ef955921c46f790b1f73fd408c503c073dbcc0ec50753c'
OVERLAY_SHA256 = '14904a5644a517f3799a8ac0b5a5b010a2f57752cf1c9ff64ac97e9d3d32a94c'
SLOT_OFFSET, SLOT_BYTES = 0x93568 - 0x15000, 16 * 13
SLOT_SHA256 = '7e9457b330b38dbc81cc1bcfefb0141f866af79ec1eda9d397855e70bf2daf40'
# Disc extents/checksums are metadata; no audio or original table is embedded.
TRACKS = (
    ('ZTMENU1.CNK', 252406, 8522396, '29bdee20fc33093b077b42d9bbe59d23558747b5f5b63ac0c574f8703144f4ff'),
    ('ZTMENU2.CNK', 256568, 8175484, '1bd8ed36abce8ea492eb606f3ff43bfbc7b9e13598fc460138a1ea1e364fad24'),
    ('ZTMENU3.CNK', 260560, 6578508, '96c5e4452cccaf59242b117edc96b2fb1c9b0b4e2be13e65c661494f81e63ab8'),
    ('ZTMENU4.CNK', 263773, 6652444, '14baee8db5592b541119f9aba898436b0fdde02f900b3041fb511c9c9cf651e7'),
    ('ZTPAUSE.CNK', 267033, 10693852, 'bd41a6dafc5f1bd84c7fb5c596130a5109cfbcdadb20d5357e50d98bc7c3bd5c'),
)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def inspect_cnk(data):
    if len(data) < 128 or data[:4] != b'SCHl' or data[12:16] != b'PATl' or data[68:72] != b'TMxl':
        raise ValueError('unsupported fixed SCHl/PATl/TMxl header')
    header = struct.unpack_from('<I', data, 4)[0]
    if not 128 <= header <= len(data) or data[72:76] != bytes((0, 16, 2, 6)):
        raise ValueError('unsupported CNK header version/format')
    rate, samples = struct.unpack_from('<H', data, 78)[0], struct.unpack_from('<I', data, 84)[0]
    if rate != 44100 or samples == 0:
        raise ValueError('unsupported CNK sample rate/count')
    offset, blocks, available, ended = header, 0, 0, False
    while offset < len(data):
        if offset + 8 > len(data):
            raise ValueError('truncated CNK chunk header')
        tag, size = data[offset:offset+4], struct.unpack_from('<I', data, offset+4)[0]
        if size < 8 or offset + size > len(data):
            raise ValueError('invalid CNK chunk extent')
        if tag == b'SCEl':
            ended = True
            if offset + size != len(data):
                raise ValueError('unexpected data after CNK terminator')
        elif tag == b'SCDl':
            if size < 48 or (size - 16) % 32:
                raise ValueError('invalid stereo ADPCM payload extent')
            blocks += 1
            available += (size - 16) // 32 * 28
        elif tag != b'SCCl':
            raise ValueError('unsupported CNK chunk tag')
        offset += size
    if not ended or available < samples or not blocks:
        raise ValueError('missing CNK terminator or sample payload')
    return dict(sample_rate=rate, sample_frames=samples, channels=2,
                data_blocks=blocks, duration_seconds=samples/rate)


def extract_music(disc, overlay, output):
    disc, overlay, output = Path(disc).resolve(), Path(overlay).resolve(), Path(output).resolve()
    names = [row[0] for row in TRACKS] + ['music_slots.bin', 'music_routing.json']
    outputs = [(output/name).resolve() for name in names]
    private = PRIVATE.resolve()
    if any(not path.is_relative_to(private) for path in outputs):
        raise ValueError('music outputs must remain under the repository .local directory')
    paths = [disc, overlay] + outputs
    for i, path in enumerate(paths):
        for other in paths[i+1:]:
            if path == other or (path.exists() and other.exists() and path.samefile(other)):
                raise ValueError('music sources and outputs must be distinct files')
    if disc.stat().st_size != DISC_SIZE:
        raise ValueError('unsupported disc size')
    with disc.open('rb') as source:
        if hashlib.file_digest(source, 'sha256').hexdigest() != DISC_SHA256:
            raise ValueError('unsupported disc SHA256')
    data = overlay.read_bytes()
    if digest(data) != OVERLAY_SHA256:
        raise ValueError('unsupported FEONLY SHA256')
    slots = data[SLOT_OFFSET:SLOT_OFFSET+SLOT_BYTES]
    if len(slots) != SLOT_BYTES or digest(slots) != SLOT_SHA256:
        raise ValueError('unsupported music slot table')
    slot_names = []
    for offset in range(0, SLOT_BYTES, 13):
        slot = slots[offset:offset+13]
        if b'\0' not in slot:
            raise ValueError('unterminated music filename')
        name = slot.split(b'\0', 1)[0].decode('ascii')
        if name not in names[:len(TRACKS)]:
            raise ValueError('unrecognized source music filename')
        slot_names.append(name)
    prepared, tracks = {}, []
    with disc.open('rb') as source:
        for name, lba, size, expected in TRACKS:
            track = bytearray()
            for sector in range((size+2047)//2048):
                source.seek((lba+sector)*2352+24)
                block = source.read(min(2048, size-len(track)))
                if not block:
                    raise ValueError('truncated music extent')
                track.extend(block)
            if len(track) != size or digest(track) != expected:
                raise ValueError('unsupported music resource SHA256: '+name)
            info = inspect_cnk(track)
            tracks.append(dict(file=name, lba=lba, bytes=size, sha256=expected, **info))
            prepared[name] = track
    manifest = dict(version=1, disc_sha256=DISC_SHA256, overlay_sha256=OVERLAY_SHA256,
                    initial='ZTMENU1.CNK', pause='ZTPAUSE.CNK', slot_count=16, slot_stride=13,
                    slot_source='FEONLY:80093568', slot_sha256=SLOT_SHA256, slots=slot_names,
                    tracks=tracks, scope='CNK resources and selector table only; no XA or playback claim')
    prepared['music_slots.bin'] = slots
    prepared['music_routing.json'] = (json.dumps(manifest, indent=2)+'\n').encode()
    # Validate every collision before publishing anything; preserve real or
    # previously verified bytes rather than letting extraction replace them.
    for name, path in zip(names, outputs):
        if path.exists() and path.read_bytes() != prepared[name]:
            raise ValueError('existing music output differs; use a fresh private directory: '+str(path))
    output.mkdir(parents=True, exist_ok=True)
    for name, path in zip(names, outputs):
        if not path.exists():
            with path.open('xb') as target:
                target.write(prepared[name])
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--disc', type=Path, default=PRIVATE/'input/nba-live-97-slus-00267.bin')
    parser.add_argument('--overlay', type=Path, default=PRIVATE/'extracted/FEONLY.BIN')
    parser.add_argument('--output', type=Path, default=PRIVATE/'assetpacks/menu')
    args = parser.parse_args()
    try:
        manifest = extract_music(args.disc, args.overlay, args.output)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"Extracted/verified {len(manifest['tracks'])} private CNK tracks and16 source filename slots in {args.output}")


if __name__ == '__main__':
    main()
