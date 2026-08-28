"""Validate native Help call-boundary evidence; never infer original execution."""
import csv
import re
from verify_reorder_reference import private_path, require

MODAL = ('phase', 'x', 'y', 'width', 'height', 'target_x', 'target_y', 'target_width', 'target_height', 'held')
STATE = ('boot', 'page', 'menu_ms', 'team', 'phase', 'child', 'help', 'cursor0', 'cursor1', 'top0', 'top1',
         'player0', 'player1', 'fact_variant', 'fact_flash', 'transition')
PARENT = tuple(k for k in STATE if k not in ('menu_ms', 'help', 'fact_variant', 'fact_flash'))
COLUMNS = ('index', 'ns', 'next_frame', 'operation', 'raw', 'result', 'notice') + tuple(
    prefix + key for prefix in ('before_', 'after_') for key in MODAL) + STATE + ('slots_sha256',)


def expected_step(before, operation, raw, target):
    """Independent arithmetic check of frontend_help.c, not a PSX comparison."""
    after = before.copy()
    result = 0
    if operation == 0:
        require(raw == 0, 'baseline has an input')
        return after, result
    if operation == 1:
        x, y, w, h = target
        require(before[0] == 0 and raw and 0 <= x <= 246 and 0 <= y <= 110 and
                w >= 20 and h >= 10 and x + w <= 512 and y + h <= 240, 'invalid Help open')
        return [1, 246, 110, 20, 10, x, y, w, h, raw], 1
    if operation == 3 and before[0] in (1, 4):
        if before[0] == 1:
            after[1:5] = [max(before[5], before[1] - 9), max(before[6], before[2] - 4),
                          min(before[7], before[3] + 18), min(before[8], before[4] + 8)]
            if after[1:5] == before[5:9]:
                after[0] = 2
        else:
            after[1:5] = [min(246, before[1] + 9), min(110, before[2] + 4),
                          max(20, before[3] - 18), max(10, before[4] - 8)]
            if after[1:5] == [246, 110, 20, 10]:
                after[0] = 5
        return after, result
    if before[0] == 5:
        if raw != before[9]:
            after[0], result = 0, 3
    else:
        if after[0] == 2 and raw != before[9]:
            after[0] = 3
        if after[0] == 3 and raw:
            after[0], after[9], result = 4, raw, 2
    return after, result


def key_mask(code):
    return {27: 0x100, 88: 0x100, 13: 0x80, 38: 1, 40: 2, 37: 8, 39: 4,
            70: 0x20, 72: 0x20, 112: 0x20, 68: 0x10, 83: 0x40}.get(code, 0x800)


def inspect(directory, summary, frames, inputs):
    if 'help_event_schema' not in summary:
        require('help_events' not in summary and not (directory / 'help_events.csv').exists(), 'undeclared Help evidence')
        return None
    require(type(summary['help_event_schema']) is int and summary['help_event_schema'] == 1, 'unknown Help event schema')
    count = summary.get('help_events')
    require(type(count) is int and 0 <= count <= 10000, 'invalid Help event count')
    path = private_path(directory, 'help_events.csv')
    require(path.stat().st_size <= 16 * 1024 * 1024, 'oversized Help event timeline')
    with path.open(newline='', encoding='ascii') as stream:
        reader = csv.DictReader(stream)
        require(reader.fieldnames == list(COLUMNS), 'wrong Help event columns')
        events = list(reader)
    require(len(events) == count, 'Help event count mismatch')
    parsed, prior, cycles, active = [], None, [], None
    for index, row in enumerate(events):
        require(set(row) == set(COLUMNS), 'malformed Help event row')
        require(re.fullmatch('[0-9a-f]{64}', row['slots_sha256']) is not None, 'invalid slot hash')
        e = {k: int(row[k]) for k in COLUMNS[:-1]}
        e['slots_sha256'] = row['slots_sha256']
        require(e['index'] == index and e['ns'] >= 0 and (prior is None or e['ns'] >= prior['ns']), 'Help event order/clock')
        boundary = e['next_frame']
        require(0 <= boundary <= len(frames), 'Help event frame boundary')
        require(not boundary or e['ns'] >= int(frames[boundary - 1]['ns']), 'Help event predates previous frame')
        require(boundary == len(frames) or e['ns'] <= int(frames[boundary]['ns']), 'Help event follows next frame')
        require(0 <= e['operation'] <= 3 and 0 <= e['raw'] <= 65535 and 0 <= e['result'] <= 3 and e['notice'] in (0, 1), 'Help event enum/mask')
        before, after = ([e[prefix + key] for key in MODAL] for prefix in ('before_', 'after_'))
        for modal in (before, after):
            require(0 <= modal[0] <= 5 and 0 <= modal[9] <= 65535 and
                    all(-32768 <= value <= 32767 for value in modal[1:9]), 'invalid Help modal values')
        require(e['help'] == after[0], 'event state/Help phase mismatch')
        if e['operation'] == 0:
            require(index == 0, 'baseline must be first')
        else:
            require(before != after or e['result'], 'unchanged Help call should not be logged')
        require((after, e['result']) == expected_step(before, e['operation'], e['raw'], after[5:9]), 'Help call result/geometry disagrees with controller')
        if prior is not None and not (e['operation'] == 1 and before[0] == prior['after_phase'] == 0):
            require(before == [prior['after_' + key] for key in MODAL], 'Help state chain is broken')
        if e['operation'] == 1:
            require(active is None, 'nested Help cycle')
            active = [e]
        elif active is not None:
            active.append(e)
        if active is not None and e['result'] == 3:
            first = active[0]
            close = next((event for event in active if event['result'] == 2), None)
            phases = []
            for event in active:
                if not phases or phases[-1] != event['after_phase']:
                    phases.append(event['after_phase'])
            parent = first['boot'] == 4 and first['page'] == 8 and first['phase'] in (0, 1) and first['child'] == 0
            stable = parent and all(not event['notice'] and all(event[k] == first[k] for k in PARENT) and
                                    event['slots_sha256'] == first['slots_sha256'] for event in active)
            def input_observed(event):
                return event is not None and any(int(i['message']) == 256 and int(i['next_frame']) == event['next_frame']
                    and int(i['ns']) <= event['ns'] and key_mask(int(i['code'])) == event['raw'] for i in inputs)
            complete = phases in ([1, 2, 3, 4, 5, 0], [1, 2, 4, 5, 0]) and close is not None
            cycles.append(dict(first_event=first['index'], return_event=e['index'], observed_boundary_phases=phases,
                parent_fields_and_slot_hash_unchanged=stable, slot_hash=first['slots_sha256'],
                open_key_observed=input_observed(first), close_key_observed=input_observed(close),
                native_call_cycle_verified=bool(complete and stable and input_observed(first) and input_observed(close)),
                all_between_event_mutations_verified=False, original_parity=False))
            active = None
        parsed.append(e)
        prior = e
    # Corroborate phases against actual submitted video, including several
    # state transitions sharing one next_frame. Do not manufacture extra frames.
    at = 0
    latest = None
    for index, frame in enumerate(frames):
        while at < len(parsed) and parsed[at]['next_frame'] <= index:
            latest = parsed[at]
            at += 1
        if latest is not None:
            require(int(frame['help']) == latest['after_phase'], 'rendered Help phase disagrees with events')
    return dict(status='native_help_events_valid', events=count, cycles=cycles,
                unfinished_open_cycle=active is not None, original_parity=False,
                timestamps_are_observation_times_not_original_ticks=True)
