import copy
import json
import unittest
import verify_reset_rosters as audit


class ResetLedger(unittest.TestCase):
    def setUp(self):
        self.config = json.loads(audit.CONFIG.read_text())

    def test_reviewed_scope(self):
        report = audit.calculate(self.config)
        self.assertEqual((report['total'], report['accounted'], report['pending']), (237, 52, 185))
        self.assertFalse(report['static_report_executes_tests'])

    def test_reject_changed_denominator_duplicate_owner_and_missing_block(self):
        for edit in ('total', 'duplicate', 'block'):
            config = copy.deepcopy(self.config)
            if edit == 'total': config['instruction_total'] -= 1
            elif edit == 'duplicate': config['functions'].append(config['functions'][0])
            else: config['functions'][0]['blocks'].pop()
            with self.assertRaises(ValueError): audit.calculate(config)

    def test_reject_unreviewed_credit(self):
        config = copy.deepcopy(self.config)
        owner = config['functions'][0]
        owner['symbol'] = 'nba97_reset_enabled'
        owner['blocks'][0].update(accounted=True, behavior='invented credit', tests=config['core_tests'])
        with self.assertRaises(ValueError): audit.calculate(config)

    def test_reject_wrong_symbol_or_missing_evidence(self):
        for edit in ('symbol', 'tests', 'behavior', 'boolean', 'overlap'):
            config = copy.deepcopy(self.config)
            owner = next(f for f in config['functions'] if f['address'] == '0x80057C48')
            block = owner['blocks'][0]
            if edit == 'symbol': owner['symbol'] = 'nba97_reset_open'
            elif edit == 'tests': block['tests'] = []
            elif edit == 'behavior': block['behavior'] = ''
            elif edit == 'boolean': block['accounted'] = 1
            else: owner['blocks'].append(copy.deepcopy(block))
            with self.assertRaises(ValueError): audit.calculate(config)


if __name__ == '__main__':
    unittest.main()
