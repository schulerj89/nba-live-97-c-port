import csv
from pathlib import Path
import tempfile
import unittest

import inspect_help_events as subject


class HelpEventInspection(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.directory = Path(self.temp.name)
        self.rows = []
        self.inputs = []
        self.frames = [dict(ns=0, help=0), dict(ns=10000, help=0)]
        self.modal = [0]*10
        self.add(0, 0)
        self.add(1, 0x20)
        while self.modal[0] == 1:
            self.add(3, 0)
        self.add(3, 0)
        self.add(2, 0x80)
        while self.modal[0] == 4:
            self.add(3, 0x80)
        self.add(3, 0)

    def tearDown(self):
        self.temp.cleanup()

    def add(self, op, raw):
        after, result = subject.expected_step(self.modal, op, raw, (121, 70, 270, 140))
        index = len(self.rows)
        row = dict(index=index, ns=index*100, next_frame=0 if not index else 1,
                   operation=op, raw=raw, result=result, notice=0)
        for prefix, modal in (('before_', self.modal), ('after_', after)):
            row.update({prefix+k: v for k, v in zip(subject.MODAL, modal)})
        row.update(dict(zip(subject.STATE, (4, 8, index*17, 3, 0, 0, after[0], 0, 0, 0, 0, 37, 37, -1, 0, 0))))
        row['slots_sha256'] = 'a'*64
        self.rows.append(row)
        if op == 1 or result == 2:
            self.inputs.append(dict(ns=row['ns']-1, next_frame=row['next_frame'], message=256, code=70 if op == 1 else 13))
        self.modal = after

    def inspect(self):
        with (self.directory / 'help_events.csv').open('w', newline='', encoding='ascii') as stream:
            writer = csv.DictWriter(stream, subject.COLUMNS)
            writer.writeheader()
            writer.writerows(self.rows)
        return subject.inspect(self.directory, dict(help_event_schema=1, help_events=len(self.rows)), self.frames, self.inputs)

    def test_between_paint_barrier_keeps_event_not_synthetic_frame(self):
        result = self.inspect()
        self.assertTrue(result['cycles'][0]['native_call_cycle_verified'])
        self.assertEqual(result['cycles'][0]['observed_boundary_phases'], [1, 2, 3, 4, 5, 0])
        self.assertEqual(len(self.frames), 2)
        self.assertFalse(result['original_parity'])
        self.assertFalse(result['cycles'][0]['all_between_event_mutations_verified'])

    def test_missing_barrier_event_breaks_chain(self):
        self.rows.pop(-2)
        for i, row in enumerate(self.rows): row['index'] = i
        with self.assertRaisesRegex(ValueError, 'chain'):
            self.inspect()

    def test_missing_return_remains_partial(self):
        self.rows.pop()
        self.frames[-1]['help'] = 5
        result = self.inspect()
        self.assertTrue(result['unfinished_open_cycle'])
        self.assertEqual(result['cycles'], [])

    def test_changed_geometry_result_or_video_phase_rejected(self):
        self.rows[2]['after_x'] += 1
        with self.assertRaisesRegex(ValueError, 'geometry'):
            self.inspect()
        self.rows[2]['after_x'] -= 1
        self.rows[2]['result'] = 1
        with self.assertRaisesRegex(ValueError, 'geometry'):
            self.inspect()
        self.rows[2]['result'] = 0
        self.frames[-1]['help'] = 5
        with self.assertRaisesRegex(ValueError, 'rendered'):
            self.inspect()

    def test_mask_and_time_and_boundary_rejected(self):
        baseline = self.rows[2].copy()
        for key, value in (('raw', 65536), ('ns', 99), ('ns', 10001), ('next_frame', 3), ('index', 99)):
            self.rows[2] = dict(baseline, **{key: value})
            with self.assertRaises(ValueError): self.inspect()

    def test_changed_slot_hash_or_parent_or_notice_cannot_pass(self):
        baseline = self.rows[2].copy()
        for key, value in (('slots_sha256', 'b'*64), ('top0', 1), ('notice', 1), ('child', 0x24)):
            self.rows[2] = dict(baseline, **{key: value})
            self.assertFalse(self.inspect()['cycles'][0]['native_call_cycle_verified'])

    def test_missing_or_wrong_input_cannot_pass(self):
        self.inputs[0]['code'] = 13
        self.assertFalse(self.inspect()['cycles'][0]['native_call_cycle_verified'])
        self.inputs = []
        self.assertFalse(self.inspect()['cycles'][0]['native_call_cycle_verified'])

    def test_fresh_nonzero_change_can_close_from_wait_within_one_call(self):
        # The C function enters READY then SHRINKING within the same call.
        # Do not fabricate an externally observed READY state in the sidecar.
        while self.rows[-1]['after_phase'] != 2: self.rows.pop()
        self.modal = [self.rows[-1]['after_'+k] for k in subject.MODAL]
        self.inputs = self.inputs[:1]
        self.add(2, 0x80)
        while self.modal[0] == 4: self.add(3, 0x80)
        self.add(3, 0)
        result = self.inspect()
        self.assertTrue(result['cycles'][0]['native_call_cycle_verified'])
        self.assertEqual(result['cycles'][0]['observed_boundary_phases'], [1, 2, 4, 5, 0])

    def test_unknown_schema_count_and_undeclared_file_rejected(self):
        self.inspect()
        for summary in ({}, dict(help_event_schema=True, help_events=1), dict(help_event_schema=1, help_events=10001),
                        dict(help_event_schema=1, help_events=0)):
            with self.assertRaises(ValueError): subject.inspect(self.directory, summary, self.frames, self.inputs)


if __name__ == '__main__':
    unittest.main()
