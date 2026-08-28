import copy
import json
import unittest
from verify_sign_free_agent import CONFIG,calculate
class SignAccountingTests(unittest.TestCase):
    def setUp(self):self.c=json.loads(CONFIG.read_text())
    def test_baseline(self):self.assertEqual(calculate(self.c)['pending'],0)
    def test_missing_block(self):
        self.c['functions'][0]['blocks'].pop()
        with self.assertRaises(ValueError):calculate(self.c)
    def test_duplicate_block(self):
        self.c['functions'][0]['blocks'].append(copy.deepcopy(self.c['functions'][0]['blocks'][0]))
        with self.assertRaises(ValueError):calculate(self.c)
    def test_missing_evidence(self):
        self.c['functions'][0]['blocks'][0]['tests']=[]
        with self.assertRaises(ValueError):calculate(self.c)
    def test_changed_denominator(self):
        self.c['instruction_total']=188
        with self.assertRaises(ValueError):calculate(self.c)
    def test_unaccounted_remains_pending(self):
        b=self.c['functions'][0]['blocks'][0];b['accounted']=False
        self.assertEqual(calculate(self.c)['pending'],b['instruction_count'])
if __name__=='__main__':unittest.main()
