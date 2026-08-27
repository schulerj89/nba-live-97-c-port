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
        self.assertEqual(report['instructions'], dict(accounted=875, total=875, pending=0, percent=100.0))
        self.assertEqual(report['instruction_slices']['interaction'], dict(accounted=166, total=166, pending=0))
        self.assertEqual(report['instruction_slices']['screen'], dict(accounted=413, total=413, pending=0))
        self.assertEqual(report['next_slice'], 'feedback')
        self.assertEqual(report['fully_accounted_functions'], 10)
        self.assertFalse(report['tests_executed_by_static_report'])
        for row in report['functions']:
            if row['address'] in {'0x800556B0', '0x800558E0'}:
                self.assertEqual(row['accounted'], row['total'])
                self.assertTrue(row['block_evidence'])

    def test_shared_core_requires_compaction_contract(self):
        self.config['shared_contracts'] = [c for c in self.config['shared_contracts'] if c['address'] != '0x800555F4']
        with self.assertRaisesRegex(ValueError, 'unresolved call contract'):
            ledger.calculate(self.config, self.inventory)

    def test_shared_block_mapping_cannot_be_omitted(self):
        del self.config['functions'][7]['block_evidence']['0x80055710']
        with self.assertRaisesRegex(ValueError, 'per-block evidence'):
            ledger.calculate(self.config, self.inventory)

    def test_shared_block_tests_must_belong_to_owner(self):
        self.config['functions'][7]['block_evidence']['0x80055710']['tests'] = ['swap_exact']
        with self.assertRaisesRegex(ValueError, 'owner-declared tests'):
            ledger.calculate(self.config, self.inventory)

    def test_selection_credit_requires_callee_contract(self):
        self.config['selection_contracts'] = [c for c in self.config['selection_contracts']
                                              if c['address'] != '0x80040A1C']
        with self.assertRaisesRegex(ValueError, 'unresolved call contract'):
            ledger.calculate(self.config, self.inventory)

    def test_contract_cannot_hide_integration_boundary(self):
        self.config['selection_contracts'][0]['integration_pending'] = ''
        with self.assertRaisesRegex(ValueError, 'integration boundary'):
            ledger.calculate(self.config, self.inventory)

    def test_complete_selection_does_not_complete_feature(self):
        report = ledger.calculate(self.config, self.inventory)
        self.assertEqual(report['instruction_slices']['interaction']['pending'], 0)
        self.assertEqual(report['feature_acceptance'], 'pending')
        self.assertTrue(all(c['integration_pending'] for c in report['selection_contracts']))

    def test_dialog_extraction_is_bounded(self):
        from extract_reorder_dialogs import extract
        import struct
        data = bytearray(0xB1000 - 0x15000)
        # Synthetic descriptors, not original copyrighted message text.
        record = struct.pack('<hhhBBBB', 146, 106, 220, 65, 1, 2, 0) + b'\x01test\0\x01message\0'
        for address in (0x800AFFFA, 0x800AFC22):
            at = address - 0x80015000
            data[at:at+len(record)] = record
        packed = extract(data)
        self.assertEqual(packed, b'N97D\x01\0\0\0' + (struct.pack('<I', len(record)) + record) * 2)
        data[0x800AFFFA - 0x80015000 + 7] = 0
        with self.assertRaisesRegex(ValueError, 'unexpected original'):
            extract(data)

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
        self.config['functions'][0]['accounted_blocks'] = []
        self.config['functions'][0]['pending'] = ''
        with self.assertRaisesRegex(ValueError, 'pending-work'):
            ledger.calculate(self.config, self.inventory)

    def test_screen_credit_requires_engine_contract(self):
        self.config['screen_contracts'] = [c for c in self.config['screen_contracts']
                                         if c['address'] != '0x8003D930']
        with self.assertRaisesRegex(ValueError, 'unresolved call contract'):
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
