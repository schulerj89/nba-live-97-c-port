"""Asset-free adversarial accounting tests, not original execution evidence."""
import copy
import json
import unittest
from verify_release_players import CONFIG, calculate


class ReleaseAccountingTests(unittest.TestCase):
    def setUp(self):
        self.config = json.loads(CONFIG.read_text())

    def test_current_scope(self):
        result = calculate(self.config)
        self.assertEqual((result['accounted'], result['pending']), (208, 0))
        self.assertFalse(result['static_report_executes_tests'])

    def test_no_duplicate_missing_or_changed_owner(self):
        for change in ('duplicate', 'missing', 'count'):
            c = copy.deepcopy(self.config)
            if change == 'duplicate': c['functions'].append(c['functions'][0])
            elif change == 'missing': c['functions'].pop()
            else: c['functions'][0]['instruction_count'] += 1
            with self.subTest(change=change), self.assertRaises(ValueError): calculate(c)

    def test_no_overlap_or_gaps(self):
        for change in ('duplicate', 'missing', 'outside', 'count'):
            c = copy.deepcopy(self.config)
            blocks = c['functions'][0]['blocks']
            if change == 'duplicate': blocks.append(copy.deepcopy(blocks[0]))
            elif change == 'missing': blocks.pop()
            elif change == 'outside': blocks[0]['start'] = '0x80000000'
            else: blocks[0]['instruction_count'] += 1
            with self.subTest(change=change), self.assertRaises(ValueError): calculate(c)

    def test_no_unreviewed_credit(self):
        for owner in (0, 1):
            c = copy.deepcopy(self.config)
            function = c['functions'][owner]
            function['symbol'] = 'nba97_release_available'
            block = function['blocks'][-1]
            block.update(accounted=True, tests=c['core_tests'][:1], behavior='invented credit')
            with self.subTest(owner=owner), self.assertRaises(ValueError): calculate(c)

    def test_no_credit_without_test_or_source(self):
        for field, value in (('tests', []), ('tests', ['not_a_scenario']), ('behavior', ''), ('accounted', 1)):
            c = copy.deepcopy(self.config)
            c['functions'][-1]['blocks'][0][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError): calculate(c)
        self.config['functions'][-1]['symbol'] = 'missing_function'
        with self.assertRaises(ValueError): calculate(self.config)


if __name__ == '__main__':
    unittest.main()
