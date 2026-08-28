#!/usr/bin/env python3
"""Inspect original desktop frame-step observations; never award parity credit.

No image transformation or automatic alignment. A declared visible rectangle
is compared within the original sequence only, not against native screenshots.
The full untouched desktop captures and debugger captures remain authoritative.
"""
import argparse
import hashlib
import json
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def consecutive_groups(values):
    groups = []
    for step, value in enumerate(values, 1):
        if not groups or value != values[step - 2]:
            groups.append([step])
        else:
            groups[-1].append(step)
    return groups


def inspect(manifest_path):
    from PIL import Image
    manifest_path = Path(manifest_path).resolve()
    root = manifest_path.parent
    manifest = json.loads(manifest_path.read_text())
    if manifest.get('kind') != 'original_desktop_frame_steps':
        raise ValueError('not an original desktop frame-step observation')
    roi = manifest['title_roi']
    if len(roi) != 4 or not all(type(x) is int for x in roi):
        raise ValueError('invalid title rectangle')
    values = []
    for number, observation in enumerate(manifest['steps'], 1):
        if observation['step'] != number:
            raise ValueError('missing/reordered frame steps')
        for field in ('screen', 'debugger'):
            item = observation[field]
            path = (root / item['file']).resolve()
            if path.parent != root or not path.is_file():
                raise ValueError('capture must be a direct file beside the manifest')
            if sha(path) != item['sha256']:
                raise ValueError('capture hash differs: ' + item['file'])
            with Image.open(path) as image:
                if list(image.size) != item['size']:
                    raise ValueError('capture dimensions differ')
                if field == 'screen':
                    l, t, r, b = roi
                    if not (0 <= l < r <= image.width and 0 <= t < b <= image.height):
                        raise ValueError('title rectangle outside capture')
                    # In-memory selection for equality only; no saved crop,
                    # resampling, offset search, color or brightness correction.
                    values.append(image.convert('RGB').crop(roi).tobytes())
    if len(values) < 3:
        raise ValueError('insufficient observations')
    groups = consecutive_groups(values)
    interior_lengths = [len(group) for group in groups[1:-1]]
    return {
        'status': 'observed',
        'manifest_sha256': sha(manifest_path),
        'steps': len(values),
        'title_roi': roi,
        'same_pixel_groups': groups,
        'complete_interior_hold_lengths': interior_lengths,
        'observed_four_step_holds': sum(n == 4 for n in interior_lengths),
        'all_complete_holds_four_steps': bool(interior_lengths) and all(n == 4 for n in interior_lengths),
        'title_roi_sha256': [hashlib.sha256(value).hexdigest() for value in values],
        'debugger_pc_and_cycles': 'manual_transcription_retained_in_manifest_not_OCR_verified',
        'native_pixel_parity': 'not_compared',
        'audio_parity': 'not_captured',
        'wall_clock_cadence': 'not_measured',
        'paired_scenario_credit': 0,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    report = inspect(args.manifest)
    rendered = json.dumps(report, indent=2) + '\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered)
    print(rendered, end='')


if __name__ == '__main__':
    main()
