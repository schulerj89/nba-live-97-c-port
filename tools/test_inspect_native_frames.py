import csv
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import inspect_native_frames as subject


class NativeRecordingInspection(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.directory = self.root / '.local' / 'fixture'
        self.directory.mkdir(parents=True)
        self.root_patch = patch.object(subject, 'ROOT', self.root)
        self.root_patch.start()
        self.summary = dict(schema_version=1, kind='native_presentations', video_complete=True,
                            audio_captured=False, reference_ready=False, frame_rate=None,
                            written=2, submitted=2, inputs=1)
        self.save_summary()
        self.rows = [dict(index=0, ns=100, help=0), dict(index=1, ns=100000100, help=1)]
        self.save_rows()
        self.input_rows = [dict(ns=500, next_frame=1, message=256, code=70, data=1)]
        self.save_inputs()
        for index in range(2):
            (self.directory / f'{index:05d}.ppm').write_bytes(b'P6\n512 240\n255\n' + bytes([1, 2, index]) * (512 * 240))

    def tearDown(self):
        self.root_patch.stop()
        self.temp.cleanup()

    def save_summary(self):
        (self.directory / 'recording.json').write_text(json.dumps(self.summary), encoding='utf-8')

    def save_rows(self):
        with (self.directory / 'frames.csv').open('w', newline='', encoding='ascii') as f:
            writer = csv.DictWriter(f, ['index', 'ns', 'help'])
            writer.writeheader()
            writer.writerows(self.rows)

    def save_inputs(self):
        with (self.directory / 'inputs.csv').open('w', newline='', encoding='ascii') as f:
            writer = csv.DictWriter(f, ['ns', 'next_frame', 'message', 'code', 'data'])
            writer.writeheader()
            writer.writerows(self.input_rows)

    def test_pixels_and_variable_time_do_not_award_reference_credit(self):
        result = subject.inspect(self.directory)
        self.assertEqual(result['unique_decoded_frames'], 2)
        self.assertEqual(result['interval_ms'], dict(min=100, median=100, max=100))
        self.assertFalse(result['reference_ready'])
        self.assertFalse(result['audio_captured'])
        self.assertFalse(result['original_pair_compared'])

    def test_partial_or_relabelled_summaries_rejected(self):
        baseline = self.summary.copy()
        for key, value in [('video_complete', False), ('written', 3), ('submitted', 1),
                           ('reference_ready', True), ('frame_rate', [60, 1]), ('audio_captured', True)]:
            with self.subTest(key=key):
                self.summary = dict(baseline, **{key: value})
                self.save_summary()
                with self.assertRaises(ValueError):
                    subject.inspect(self.directory)

    def test_frame_order_and_clock_rejected(self):
        for field, value in [('index', 3), ('ns', 100), ('ns', 99)]:
            with self.subTest(field=field, value=value):
                self.rows[1] = dict(index=1, ns=100000100, help=1)
                self.rows[1][field] = value
                self.save_rows()
                with self.assertRaises(ValueError):
                    subject.inspect(self.directory)

    def test_corrupt_and_unindexed_frames_rejected(self):
        frame = self.directory / '00001.ppm'
        original = frame.read_bytes()
        frame.write_bytes(original[:-1])
        with self.assertRaises(ValueError):
            subject.inspect(self.directory)
        frame.write_bytes(original)
        (self.directory / '00002.ppm').write_bytes(original)
        with self.assertRaises(ValueError):
            subject.inspect(self.directory)

    def test_input_boundary_rejected(self):
        for ns, boundary in [(99, 1), (100000101, 1), (500, -1), (500, 3)]:
            self.input_rows[0].update(ns=ns, next_frame=boundary)
            self.save_inputs()
            with self.assertRaises(ValueError):
                subject.inspect(self.directory)

    def test_not_private_rejected(self):
        with self.assertRaises(ValueError):
            subject.inspect(self.root)

    def test_duplicate_summary_and_unknown_phase_rejected(self):
        path = self.directory / 'recording.json'
        path.write_text('{"written":2,"written":3}', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            subject.inspect(self.directory)
        self.save_summary()
        self.rows[1]['help'] = 6
        self.save_rows()
        with self.assertRaisesRegex(ValueError, 'unknown Help'):
            subject.inspect(self.directory)

    def test_capture_limit_validation(self):
        for limit in (0, 1, 6001, True, '600'):
            self.summary['frame_limit'] = limit
            self.save_summary()
            with self.assertRaises(ValueError):
                subject.inspect(self.directory)
        self.summary['frame_limit'] = 3600
        self.save_summary()
        self.assertEqual(subject.inspect(self.directory)['frames'], 2)


class ObservedHelpRoundTrips(unittest.TestCase):
    def setUp(self):
        self.rows = [dict(help=phase, boot=4, page=8, team=3, phase=0, child=0,
                          cursor0=0, cursor1=0, top0=0, top1=0, player0=37, player1=37,
                          transition=0) for phase in (0, 1, 1, 2, 3, 3, 4, 5, 0, 0)]
        self.inputs = [dict(message=256, next_frame=1, code=70),
                       dict(message=256, next_frame=6, code=13)]

    def result(self):
        return subject.help_round_trips(self.rows, self.inputs)[0]

    def test_complete_cycle_and_no_fidelity_credit(self):
        result = self.result()
        self.assertTrue(result['observed_parent_round_trip'])
        self.assertEqual(result['return_frame'], 8)
        self.assertFalse(result['all_roster_slots_verified'])
        self.assertFalse(result['original_parity'])

    def test_partial_capture_or_missing_barrier_is_not_complete(self):
        original = self.rows.copy()
        for rows in (original[:7], original[1:], original[:7] + original[8:]):
            self.rows = rows
            self.assertFalse(self.result()['phase_cycle_complete'])
            self.assertFalse(self.result()['observed_parent_round_trip'])

    def test_transient_parent_change_or_missing_snapshot_is_not_preserved(self):
        self.rows[4]['top0'] = 1
        self.assertFalse(self.result()['parent_snapshot_unchanged'])
        self.rows[4]['top0'] = 0
        del self.rows[4]['player1']
        self.assertFalse(self.result()['observed_parent_round_trip'])

    def test_child_help_is_not_parent_proof(self):
        for row in self.rows:
            row['child'] = 0x24
        self.assertFalse(self.result()['observed_parent_round_trip'])

    def test_absent_wrong_or_late_input_does_not_corroborate(self):
        original = [event.copy() for event in self.inputs]
        for field, value in (('message', 257), ('code', 13), ('next_frame', 2)):
            self.inputs = [event.copy() for event in original]
            self.inputs[0][field] = value
            self.assertFalse(self.result()['observed_parent_round_trip'])
        self.inputs = original[:1]
        self.assertFalse(self.result()['observed_parent_round_trip'])

    def test_multiple_cycles_are_separate(self):
        rows = self.rows + self.rows
        inputs = self.inputs + [dict(event, next_frame=event['next_frame'] + 10) for event in self.inputs]
        result = subject.help_round_trips(rows, inputs)
        self.assertEqual(len(result), 2)
        self.assertTrue(all(cycle['observed_parent_round_trip'] for cycle in result))


if __name__ == '__main__':
    unittest.main()
