"""Check bounded native plate pixels against authored private assets, not goldens.

This checks the fixed state3/state5 entry captures. It deliberately avoids title,
text, animated palette, unavailable texture columns and PS1 raster-edge claims.
"""
import argparse
import json
from pathlib import Path
import struct

from PIL import Image


def require(ok, message):
    if not ok:
        raise ValueError(message)


class Pack:
    def __init__(self, path, header):
        self.data = path.read_bytes()
        require(self.data[:12] == header, f"{path}: unsupported asset header")
        self.at = 12

    def take(self, count):
        require(self.at + count <= len(self.data), "truncated plate asset pack")
        result = self.data[self.at:self.at + count]
        self.at += count
        return result

    def text(self):
        return self.take(struct.unpack('<H', self.take(2))[0]).decode('ascii')

    def layout(self, count):
        result = [struct.unpack('<4h4s', self.take(12)) for _ in range(count)]
        require(self.at == len(self.data), "trailing plate asset pack data")
        return result


def source_layouts(assets):
    team = Pack(assets / 'team_select/ui.n97select',
                struct.pack('<4s4H', b'N97S', 1, 31, 18, 5))
    team.take(24 + 58)  # Six-word RNG seed and 29 rating adjustments.
    for _ in range(6 + 62):
        team.text()
    tags = [team.text() for _ in range(31)]
    require(tags[3] == 'chiR', "team3 source logo is no longer Chicago chiR")
    team_layout = team.layout(18)
    user = Pack(assets / 'user_setup/ui.n97users',
                struct.pack('<4s4H', b'N97U', 1, 35, 8, 68))
    user.take(68 + 32 + 8)  # Alphabet, colors and initial assignments.
    user.text()
    user.text()
    user_layout = user.layout(35)
    # Original 80093330 state3/state5 descriptor routes through 80031F48.
    for layout, depth in ((team_layout, 2), (user_layout, 8)):
        require(layout[16] == (368, 15, depth, 0, b'frmr') and
                layout[17] == (30, 15, depth, 0, b'frml'),
                "source foreground frame coordinates/depths changed")
    require(user_layout[33] == (370, 16, 9, 8, b'blnk') and
            user_layout[34] == (40, 16, 9, 8, b'blnk'),
            "source state5 logo coordinates/depths changed")
    require(user_layout[10] == (0, 65, 13, 0, b'brle') and
            user_layout[11] == (476, 65, 13, 0, b'brri'),
            "source state5 outer border coordinates/depths changed")
    return team_layout, user_layout


def ppm(path):
    parts = path.read_bytes().split(b'\n', 3)
    require(parts[:3] == [b'P6', b'512 240', b'255'] and len(parts) == 4 and
            len(parts[3]) == 512 * 240 * 3, f"{path}: expected bounded 512x240 P6")
    return parts[3]


def asset(assets, tag, size):
    path = assets / 'team_select' / (tag + '.png')
    with Image.open(path) as original:
        require(original.size == size, f"{path}: source dimensions changed")
        image = original.convert('RGBA')
        require(set(image.getchannel('A').getdata()) <= {0, 255},
                f"{path}: unsupported intermediate alpha")
        return image


def pixels_equal(frame, samples, label, minimum):
    require(len(samples) >= minimum, f"{label}: insufficient safe authored samples")
    for x, y, expected in samples:
        offset = (y * 512 + x) * 3
        actual = tuple(frame[offset:offset + 3])
        require(actual == expected,
                f"{label}: pixel ({x},{y}) expected {expected}, got {actual}")
    return len(samples)


def frame_samples(image, x, y):
    # Above the first row (y66) avoids all retained names/markers and their
    # topology history. The title is horizontally separate from both frames.
    return [(x + xx, y + yy, image.getpixel((xx, yy))[:3])
            for yy in range(50) for xx in range(image.width)
            if image.getpixel((xx, yy))[3] == 255]


def inside_home(x, y):
    # Strict integer interior of the original 80093714 shape. No boundary
    # sample or replication of the native floating-point triangle helper.
    points = ((20, 10), (106, 0), (106, 58), (0, 60))
    return all((b[0] - a[0]) * (y - a[1]) -
               (b[1] - a[1]) * (x - a[0]) > 0
               for a, b in zip(points, points[1:] + points[:1]))


def sample_sets(assets):
    right = asset(assets, 'frmr', (120, 62))
    left = asset(assets, 'frml', (120, 62))
    chicago = asset(assets, 'chiR', (103, 60))
    # 34A5C adds the same offsets to XY/UV: visible (x,y) selects exactly
    # image(x,y), without rescaling. Column103 and beyond are not asserted.
    logo = [(370 + x, 16 + y, chicago.getpixel((x, y))[:3])
            for y in range(49) for x in range(chicago.width)
            if inside_home(x, y) and chicago.getpixel((x, y))[3] == 255
            and right.getpixel((x + 2, y + 1))[3] == 0]
    borders = []
    for tag, size, origin, safe_x in (
            ('brle', (40, 130), 0, range(26)),
            ('brri', (36, 130), 476, range(13, 36))):
        image = asset(assets, tag, size)
        # Outside plate/title/name regions and above the lower border.
        borders.extend((origin + x, 65 + y, image.getpixel((x, y))[:3])
                       for y in range(20, 106) for x in safe_x
                       if image.getpixel((x, y))[3] == 255)
    return {'right_frame': (frame_samples(right, 368, 15), 500),
            'left_frame': (frame_samples(left, 30, 15), 500),
            'chicago_identity': (logo, 1000),
            'outer_borders': (borders, 500)}


def verify(frames, assets):
    source_layouts(assets)
    samples = sample_sets(assets)
    states = json.loads((frames / 'states.json').read_text(encoding='utf-8-sig'))
    results = {}
    for name, page in (('entry', 'Team Select'), ('user-setup-entry', 'User Setup')):
        matches = [state for state in states if state['id'] == name]
        require(len(matches) == 1, f"{name}: missing or duplicated capture state")
        state = matches[0]
        require(state['page'] == page and state['home'] == 3,
                f"{name}: expected {page} with Chicago at home")
        require(state.get('shown_home', 3) == 3 and state.get('help', 0) == 0 and
                state.get('user_help', 0) == 0 and state.get('dialog', 0) == 0,
                f"{name}: expected unobscured Chicago entry presentation")
        frame = ppm(frames / (name + '.ppm'))
        results[name] = {
            region: pixels_equal(frame, points, f'{name}/{region}', minimum)
            for region, (points, minimum) in samples.items()
            if region != 'outer_borders' or name == 'user-setup-entry'}
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frames', required=True, type=Path)
    parser.add_argument('--assets', type=Path, default=Path('.local/assetpacks'))
    args = parser.parse_args()
    try:
        result = verify(args.frames, args.assets)
    except (OSError, ValueError, KeyError, struct.error) as error:
        parser.exit(1, f'FRONTEND PLATE FAIL: {error}\n')
    print('FRONTEND PLATE PASS: ' + json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
