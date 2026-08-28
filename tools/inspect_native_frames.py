"""Inspect passive native presentation recordings; never award reference credit."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import statistics

from PIL import Image
import inspect_process_audio
import inspect_help_events
from verify_reorder_reference import private_path, read_json

ROOT = Path(__file__).resolve().parents[1]


def help_round_trips(rows, inputs):
    """Report observed parent-Help cycles, not full state or original fidelity.

    Phase numbers come from recovered/frontend_help.h. Missing presentation
    phases stay unverified; never insert an unseen barrier frame.
    """
    fields = ('boot', 'page', 'team', 'phase', 'child', 'cursor0', 'cursor1',
              'top0', 'top1', 'player0', 'player1', 'transition')
    transitions = [i for i, row in enumerate(rows)
                   if not i or row['help'] != rows[i - 1]['help']]
    result = []
    for offset, start in enumerate(transitions):
        if int(rows[start]['help']) != 1:
            continue
        following = transitions[offset:]
        end_offset = next((n for n, i in enumerate(following)
                           if int(rows[i]['help']) == 0), None)
        cycle = following if end_offset is None else following[:end_offset + 1]
        phases = [int(rows[i]['help']) for i in cycle]
        complete = start > 0 and int(rows[start - 1]['help']) == 0 and phases == [1, 2, 3, 4, 5, 0]
        end = cycle[-1] if end_offset is not None else len(rows) - 1
        snapshot_rows = rows[max(0, start - 1):end + 1]
        available = all(all(field in row for field in fields) for row in snapshot_rows)
        baseline = {field: int(snapshot_rows[0][field]) for field in fields} if available else None
        # Page8 is the native Re-order host; child cards have separate state
        # not fully represented here. Do not award this parent-only check to them.
        parent = available and baseline['boot'] == 4 and baseline['page'] == 8 and baseline['child'] == 0 and baseline['phase'] in (0, 1)
        unchanged = parent and all(all(int(row[field]) == baseline[field] for field in fields) for row in snapshot_rows)
        close = next((i for i in cycle if int(rows[i]['help']) == 4), None)
        def key_at(frame, allowed=None):
            return any(int(event['message']) == 256 and int(event['next_frame']) == frame
                       and (allowed is None or int(event['code']) in allowed) for event in inputs)
        opened = key_at(start, (70, 72, 112))  # F/H/F1; actual WM_KEYDOWN boundary.
        closed = close is not None and key_at(close)
        result.append(dict(start_frame=start, return_frame=end if end_offset is not None else None,
                           observed_phases=phases, phase_cycle_complete=complete,
                           parent_snapshot=baseline, parent_snapshot_unchanged=bool(unchanged),
                           opening_key_observed=opened, closing_key_observed=closed,
                           observed_parent_round_trip=bool(complete and unchanged and opened and closed),
                           all_roster_slots_verified=False, original_parity=False))
    return result


def inspect(directory):
    directory = directory.resolve(strict=True)
    directory.relative_to((ROOT / '.local').resolve(strict=True))
    summary_path = private_path(directory, 'recording.json')
    if summary_path.stat().st_size > 16384:
        raise ValueError('oversized recording summary')
    summary = read_json(summary_path)
    if not isinstance(summary, dict):
        raise ValueError('recording summary must be an object')
    if (summary.get('schema_version') != 1 or summary.get('kind') != 'native_presentations'
            or summary.get('video_complete') is not True or summary.get('reference_ready') is not False
            or summary.get('frame_rate') is not None or type(summary.get('audio_captured')) is not bool):
        raise ValueError('not a completed passive native-video recording')
    count = summary.get('written')
    limit = summary.get('frame_limit', 600)
    if type(limit) is not int or not 1 <= limit <= 6000 or type(count) is not int or not 1 <= count <= limit or summary.get('submitted') != count:
        raise ValueError('invalid submitted/written frame counts')
    timeline = private_path(directory, 'frames.csv')
    if timeline.stat().st_size > 1024 * 1024:
        raise ValueError('oversized timeline')
    with timeline.open(newline='', encoding='ascii') as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) != count:
        raise ValueError('frame timeline is incomplete')
    timestamps, hashes, help_transitions = [], [], []
    last_help = None
    for index, row in enumerate(rows):
        if int(row['index']) != index:
            raise ValueError('missing/reordered frame index')
        timestamp = int(row['ns'])
        if timestamp < 0 or (timestamps and timestamp <= timestamps[-1]):
            raise ValueError('invalid presentation timestamp')
        timestamps.append(timestamp)
        path = private_path(directory, f'{index:05d}.ppm')
        if path.stat().st_size != len(b'P6\n512 240\n255\n') + 512 * 240 * 3:
            raise ValueError('wrong frame extent')
        with Image.open(path) as frame:
            if frame.format != 'PPM' or frame.size != (512, 240) or frame.mode != 'RGB':
                raise ValueError('wrong frame format')
            hashes.append(hashlib.sha256(frame.tobytes()).hexdigest())
        phase = int(row['help'])
        if not 0 <= phase <= 5:
            raise ValueError('unknown Help phase')
        if phase != last_help:
            help_transitions.append({'frame': index, 'ns': timestamp, 'phase': phase})
            last_help = phase
    if len(list(directory.glob('*.ppm'))) != count:
        raise ValueError('unindexed frame files')
    input_path = private_path(directory, 'inputs.csv')
    if input_path.stat().st_size > 16 * 1024 * 1024:
        raise ValueError('oversized input timeline')
    with input_path.open(newline='', encoding='ascii') as stream:
        inputs = list(csv.DictReader(stream))
    if len(inputs) != summary.get('inputs') or len(inputs) > 100000:
        raise ValueError('input count mismatch')
    prior = -1
    for row in inputs:
        ns, next_frame = int(row['ns']), int(row['next_frame'])
        if ns < prior or not 0 <= next_frame <= count:
            raise ValueError('invalid input order/frame boundary')
        # Input is observed before the next presentation, not assigned to the
        # nearest frame after the fact. Preserve sub-frame timing.
        if next_frame and ns < timestamps[next_frame - 1]:
            raise ValueError('input predates its preceding presentation')
        if next_frame < count and ns > timestamps[next_frame]:
            raise ValueError('input follows its declared next presentation')
        prior = ns
    deltas = [(b - a) / 1000000 for a, b in zip(timestamps, timestamps[1:])]
    audio = None
    if summary.get('audio_requested') or summary['audio_captured']:
        if summary.get('audio_requested') is not True or summary['audio_captured'] is not True or summary.get('audio_complete') is not True:
            raise ValueError('requested process audio is missing/incomplete')
        if summary.get('clock') != 'qpc_100ns_since_start_scaled_to_ns':
            raise ValueError('mixed capture does not use the shared QPC clock')
        audio = inspect_process_audio.inspect(directory / 'audio', summary.get('qpc_origin_100ns'), timestamps)
    return {
        'status': 'native_video_artifacts_valid',
        'frames': count,
        'unique_decoded_frames': len(set(hashes)),
        'span_seconds': (timestamps[-1] - timestamps[0]) / 1000000000,
        'interval_ms': {'min': min(deltas), 'median': statistics.median(deltas),
                        'max': max(deltas)} if deltas else None,
        'help_phase_transitions': help_transitions,
        'help_round_trips': help_round_trips(rows, inputs),
        'help_events': inspect_help_events.inspect(directory, summary, rows, inputs),
        'input_events': len(inputs),
        'audio_captured': summary['audio_captured'],
        'audio': audio,
        'original_pair_compared': False,
        'reference_ready': False,
        'scanout_verified': False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--require-help-roundtrip', action='store_true',
                        help='require one fully observed native parent-Help cycle; not original parity')
    parser.add_argument('--require-help-events', action='store_true',
                        help='require a verified native parent-Help call cycle; no synthesized frames')
    args = parser.parse_args()
    try:
        result = inspect(args.directory)
        print(json.dumps(result, indent=2))
        if args.require_help_roundtrip and not any(cycle['observed_parent_round_trip'] for cycle in result['help_round_trips']):
            raise ValueError('no complete observed parent-Help round trip')
        if args.require_help_events and not any(cycle['native_call_cycle_verified'] for cycle in (result['help_events'] or {}).get('cycles', [])):
            raise ValueError('no verified native parent-Help call cycle')
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f'NATIVE RECORDING INCOMPLETE: {error}')
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
