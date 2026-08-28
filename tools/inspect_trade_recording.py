"""Validate one bounded live Trade Down/C/X scenario, not original equivalence."""
import argparse
import csv
import json
from pathlib import Path

from inspect_native_frames import inspect as inspect_frames

FIELDS = ('boot', 'page', 'team', 'phase', 'child', 'help', 'cursor0', 'cursor1',
          'top0', 'top1', 'player0', 'player1', 'transition')


def validate_selection_cancel(rows, inputs):
    """Input boundaries must explain every parent-state change; never guess offsets.

    Start recording on the first row of a settled, occupied Trade pair. Press
    Down, C, X separately, allowing a presentation between actions, then stop.
    V1 records only the left team; right-team and full-roster parity are unproven.
    """
    def require(condition, reason):
        if not condition:
            raise ValueError(reason)

    states = [{field: int(row[field]) for field in FIELDS} for row in rows]
    require(bool(states), 'no Trade frames')
    base = states[0]
    require(base['boot'] == 4 and base['page'] == 9 and base['phase'] == 0 and
            base['child'] == 0 and base['help'] == 0 and base['transition'] == 0,
            'scenario must start in settled Trade FIRST, without child or modal')
    require(0 <= base['team'] < 29 and
            all(base[k] == 0 for k in ('cursor0', 'cursor1', 'top0', 'top1')) and
            all(0 <= base[k] < 493 for k in ('player0', 'player1')) and
            base['player0'] != base['player1'], 'invalid occupied first-row fixture')
    keys = [e for e in inputs if int(e['message']) == 256 and int(e['code']) != 120]
    require([int(e['code']) for e in keys] == [40, 67, 88], 'expected exactly Down, C, X key-downs')
    require(not any(int(e['message']) in (8, 513, 514) for e in inputs),
            'focus loss or mouse button input interrupts this keyboard scenario')
    boundaries = [int(e['next_frame']) for e in keys]
    require(0 < boundaries[0] < boundaries[1] < boundaries[2] < len(states),
            'each action needs a separate observed before/after presentation')
    for n, event in enumerate(keys):
        end_ns = int(keys[n+1]['ns']) if n+1 < len(keys) else int(rows[-1]['ns'])
        require(not (int(event['data']) & (1 << 30)), 'autorepeat is not a fresh press')
        require(any(int(e['message']) == 257 and int(e['code']) == int(event['code']) and
                    int(event['ns']) <= int(e['ns']) <= end_ns for e in inputs),
                'missing key release before next action/final presentation')
    moved = states[boundaries[0]]
    require(0 <= moved['player0'] < 493 and moved['player0'] not in (base['player0'], base['player1']),
            'Down did not select a different occupied player (possibly stale recorder state)')
    after_down = dict(base, cursor0=1, player0=moved['player0'])
    expected = [base, after_down, dict(after_down, phase=1), after_down]
    edges = [0, *boundaries, len(states)]
    for segment, state in enumerate(expected):
        require(all(s == state for s in states[edges[segment]:edges[segment+1]]),
                f'unexpected/missing controller transition in segment {segment}')
    return dict(status='native_trade_selection_cancel_observed',
                input_frames=dict(zip(('down', 'pick', 'cancel'), boundaries)),
                initial=base, returned=states[-1],
                right_team_verified=False, full_roster_unchanged_verified=False,
                original_parity=False, timing_parity=False)


def inspect(directory):
    artifacts = inspect_frames(directory)  # Bounds, PPMs, clocks, input boundaries.
    with (directory / 'frames.csv').open(newline='', encoding='ascii') as stream:
        rows = list(csv.DictReader(stream))
    with (directory / 'inputs.csv').open(newline='', encoding='ascii') as stream:
        inputs = list(csv.DictReader(stream))
    return dict(artifacts=artifacts, scenario=validate_selection_cancel(rows, inputs))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.directory), indent=2))
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f'TRADE LIVE SCENARIO NOT VERIFIED: {error}')
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
