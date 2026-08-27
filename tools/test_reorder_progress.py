import copy
import unittest
from unittest.mock import patch
from pathlib import Path
import subprocess
import tempfile
import verify_reorder_rosters as ledger


class ReorderProgressTests(unittest.TestCase):
    def setUp(self):
        self.config = ledger.read(ledger.CONFIG)
        self.inventory = ledger.read(ledger.ROOT / self.config['inventory'])

    def test_small_scope_and_no_fragment_inflation(self):
        report = ledger.calculate(self.config, self.inventory)
        self.assertEqual(report['instructions'], dict(accounted=36, total=875, pending=839, percent=4.11))
        self.assertEqual(report['instruction_slices']['interaction'], dict(accounted=14, total=166, pending=152))
        self.assertEqual(report['next_slice'], 'screen')
        self.assertEqual(report['fully_accounted_functions'], 1)
        self.assertFalse(report['tests_executed_by_static_report'])
        for row in report['functions']:
            if row['address'] in {'0x800556B0', '0x800558E0'}:
                self.assertEqual(row['accounted'], 0)

    def test_duplicate_credit_rejected(self):
        item = self.config['functions'][0]
        item['accounted_blocks'].append(item['accounted_blocks'][0])
        with self.assertRaisesRegex(ValueError, 'duplicate accounted'):
            ledger.calculate(self.config, self.inventory)

    def test_missing_basis_rejected(self):
        self.config['functions'][0]['basis'] = ''
        with self.assertRaisesRegex(ValueError, 'basis'):
            ledger.calculate(self.config, self.inventory)

    def test_unknown_test_rejected(self):
        self.config['functions'][0]['tests'] = ['invented_pass']
        with self.assertRaisesRegex(ValueError, 'declared tests'):
            ledger.calculate(self.config, self.inventory)

    def test_denominator_changes_rejected(self):
        self.inventory['functions'][0]['instruction_count'] -= 1
        with self.assertRaisesRegex(ValueError, 'denominator'):
            ledger.calculate(self.config, self.inventory)

    def test_overlapping_original_blocks_rejected(self):
        function = next(f for f in self.inventory['functions'] if len(f['blocks']) > 1)
        function['blocks'][1] = copy.deepcopy(function['blocks'][0])
        with self.assertRaisesRegex(ValueError, 'duplicate original block'):
            ledger.calculate(self.config, self.inventory)

    def test_pending_explanation_required(self):
        self.config['functions'][1]['pending'] = ''
        with self.assertRaisesRegex(ValueError, 'pending-work'):
            ledger.calculate(self.config, self.inventory)

    def test_failed_run_cannot_leave_green_evidence(self):
        with tempfile.TemporaryDirectory() as temp, patch.object(ledger, 'ROOT', Path(temp)):
            with patch.object(ledger.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1, '', '')):
                with self.assertRaisesRegex(ValueError, 'fresh native test failed'):
                    ledger.run_tests(Path('unused.exe'), None, self.config)
            evidence = ledger.read(Path(temp) / '.local/reports/reorder_rosters_run.json')
            self.assertEqual(evidence['status'], 'failed')
            self.assertEqual(evidence['passed'], [])

    def test_launch_failure_is_not_success(self):
        with tempfile.TemporaryDirectory() as temp, patch.object(ledger, 'ROOT', Path(temp)):
            with patch.object(ledger.subprocess, 'run', side_effect=OSError('test launch failure')):
                with self.assertRaises(OSError):
                    ledger.run_tests(Path('unused.exe'), None, self.config)
            evidence = ledger.read(Path(temp) / '.local/reports/reorder_rosters_run.json')
            self.assertEqual(evidence['status'], 'launch_failed')


if __name__ == '__main__':
    unittest.main()
