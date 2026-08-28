from array import array
import csv
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import wave

import inspect_process_audio as subject


class ProcessAudioInspection(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.directory = self.root / '.local' / 'audio'
        self.directory.mkdir(parents=True)
        self.root_patch = patch.object(subject, 'ROOT', self.root)
        self.root_patch.start()
        self.report = dict(schema_version=1, kind='windows_process_mix', complete=True,
                           scope='include_current_process_tree', microphone=False, system_fallback=False,
                           windows_autoconvert_pcm=True, original_parity=False, sample_continuity_verified=False,
                           sample_rate=48000, channels=2, bits=16, target_pid=123,
                           qpc_origin_100ns=1000000, packets=2, sample_frames=960, nonzero_samples=1920,
                           requested_end_qpc_100ns=1200000, last_packet_end_qpc_100ns=1200100,
                           first_packet_qpc_100ns=1000100, last_packet_qpc_100ns=1100100,
                           position_gaps=1, discontinuities=0, timestamp_errors=0,
                           device_position_status='unavailable_all_zero')
        self.rows = [dict(packet=i, sample_offset=i*480, frames=480, device_position=0,
                          qpc_100ns=1000100+i*100000, flags=0) for i in range(2)]
        self.save()
        self.pcm = array('h', [1, -2]*960)
        with wave.open(str(self.directory / 'mixed.wav'), 'wb') as stream:
            stream.setparams((2, 2, 48000, 960, 'NONE', 'not compressed'))
            stream.writeframes(self.pcm.tobytes())

    def tearDown(self):
        self.root_patch.stop()
        self.temp.cleanup()

    def save(self):
        (self.directory / 'recording.json').write_text(json.dumps(self.report), encoding='utf-8')
        with (self.directory / 'packets.csv').open('w', newline='', encoding='ascii') as stream:
            writer = csv.DictWriter(stream, self.rows[0].keys())
            writer.writeheader()
            writer.writerows(self.rows)

    def test_valid_bytes_do_not_certify_sample_continuity(self):
        result = subject.inspect(self.directory, 1000000, [10000, 10010000])
        self.assertEqual(result['sample_frames'], 960)
        self.assertFalse(result['sample_continuity_verified'])
        self.assertFalse(result['original_parity'])
        self.assertEqual(result['video_boundary_coverage']['uncovered_frames'], [])
        self.assertEqual(result['peak_abs_sample'], 2)
        self.assertEqual(result['samples_above_one_lsb'], 960)

    def test_one_lsb_noise_is_not_mistaken_for_cue_strength(self):
        with wave.open(str(self.directory / 'mixed.wav'), 'wb') as stream:
            stream.setparams((2, 2, 48000, 960, 'NONE', 'not compressed'))
            stream.writeframes(array('h', [1, -1]*960).tobytes())
        result = subject.inspect(self.directory)
        self.assertEqual(result['nonzero_samples'], 1920)
        self.assertEqual(result['samples_above_one_lsb'], 0)
        self.assertEqual(result['peak_abs_sample'], 1)
        self.assertEqual(result['rms_sample'], 1)
        self.assertFalse(result['original_parity'])

    def test_selected_process_requires_explicit_pid_and_verified_identity(self):
        self.report.update(scope='include_selected_process_tree', target_executable_verified=True,
                           target_creation_filetime=1000000)
        self.save()
        with self.assertRaises(ValueError): subject.inspect(self.directory)
        with self.assertRaises(ValueError): subject.inspect(self.directory, selected_pid=124)
        result = subject.inspect(self.directory, selected_pid=123)
        self.assertEqual(result['capture_scope'], 'include_selected_process_tree')
        self.assertFalse(result['original_parity'])
        for key, value in [('target_executable_verified', False), ('target_creation_filetime', 0)]:
            old = self.report[key]; self.report[key] = value; self.save()
            with self.assertRaises(ValueError): subject.inspect(self.directory, selected_pid=123)
            self.report[key] = old

    def test_missing_or_ambiguous_packet_coverage_is_reported(self):
        result = subject.inspect(self.directory, 1000000, [0, 20010000])
        self.assertEqual(result['video_boundary_coverage']['uncovered_frames'], [0, 1])
        self.rows[1]['qpc_100ns'] -= 10
        self.report['last_packet_qpc_100ns'] -= 10
        self.report['last_packet_end_qpc_100ns'] -= 10
        self.save()
        result = subject.inspect(self.directory, 1000000, [10009500])
        self.assertEqual(result['video_boundary_coverage']['overlapping_packet_frames'], [0])

    def test_scope_clock_incomplete_and_fidelity_claims_rejected(self):
        baseline = self.report.copy()
        for key, value in [('microphone', True), ('system_fallback', True), ('complete', False),
                           ('scope', 'whole_system'), ('sample_continuity_verified', True),
                           ('original_parity', True), ('sample_frames', 50000000),
                           ('requested_end_qpc_100ns', 9999999)]:
            self.report = dict(baseline, **{key: value})
            self.save()
            with self.assertRaises(ValueError): subject.inspect(self.directory)
        self.report = baseline
        self.save()
        with self.assertRaises(ValueError): subject.inspect(self.directory, 999999, [0])

    def test_flags_counter_and_packet_order_rejected(self):
        baseline = [r.copy() for r in self.rows]
        for key, value in [('packet', 2), ('sample_offset', 481), ('qpc_100ns', 1),
                           ('flags', 1), ('flags', 4), ('frames', 100000)]:
            self.rows = [r.copy() for r in baseline]
            self.rows[1][key] = value
            self.save()
            with self.assertRaises(ValueError): subject.inspect(self.directory)

    def test_wave_truncation_and_signal_count_rejected(self):
        self.report['nonzero_samples'] = 1
        self.save()
        with self.assertRaises(ValueError): subject.inspect(self.directory)
        self.report['nonzero_samples'] = 1920
        self.save()
        path = self.directory / 'mixed.wav'
        path.write_bytes(path.read_bytes()[:-1])
        with self.assertRaises(ValueError): subject.inspect(self.directory)

    def test_silent_flag_requires_zero_pcm(self):
        self.rows[1]['flags'] = 2
        self.save()
        with self.assertRaises(ValueError): subject.inspect(self.directory)

    def test_duplicate_json_and_nonprivate_directory_rejected(self):
        path = self.directory / 'recording.json'
        path.write_text('{"schema_version":1,"schema_version":1}', encoding='utf-8')
        with self.assertRaises(ValueError): subject.inspect(self.directory)
        with self.assertRaises(ValueError): subject.inspect(self.root)


if __name__ == '__main__':
    unittest.main()
