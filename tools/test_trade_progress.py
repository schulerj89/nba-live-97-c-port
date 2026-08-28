import json
import unittest
from verify_trade_rosters import CONFIG, ROOT, calculate, markdown


class TradeProgressTests(unittest.TestCase):
    def setUp(self):
        self.config = json.loads(CONFIG.read_text(encoding="utf-8"))
        self.inventory = json.loads((ROOT / self.config["inventory"]).read_text(encoding="utf-8"))

    def test_narrow_scope_and_no_execution_claim(self):
        result = calculate(self.config, self.inventory)
        self.assertEqual(result["instructions"], {"accounted": 39, "total": 39, "pending": 0, "percent": 100.0})
        self.assertFalse(result["tests_executed_by_static_report"])
        self.assertIn("now opens a native screen", markdown(result))
        self.assertIn("does not automatically earn", markdown(result))

    def test_cannot_reduce_denominator(self):
        self.inventory["functions"][0]["size_bytes"] -= 4
        with self.assertRaises(ValueError):
            calculate(self.config, self.inventory)

    def test_cannot_overlap_blocks(self):
        blocks = self.inventory["functions"][0]["blocks"]
        blocks[1]["start"] = blocks[0]["start"]
        with self.assertRaises(ValueError):
            calculate(self.config, self.inventory)

    def test_credit_requires_known_test(self):
        self.config["functions"][0]["tests"] = ["invented_test"]
        with self.assertRaises(ValueError):
            calculate(self.config, self.inventory)

    def test_duplicate_credit_rejected(self):
        owner = self.config["functions"][0]
        owner["accounted_blocks"].append(owner["accounted_blocks"][0])
        with self.assertRaises(ValueError):
            calculate(self.config, self.inventory)

    def test_whole_wrapper_credit_requires_constructor_contract(self):
        self.config['screen_contracts']=[]
        with self.assertRaises(ValueError):
            calculate(self.config, self.inventory)

    def test_handoff_requires_both_indirect_callbacks(self):
        for index in (0,1):
            config=json.loads(json.dumps(self.config))
            del config['selection_contracts'][index]
            with self.assertRaises(ValueError):calculate(config,self.inventory)

    def test_each_credited_block_needs_own_evidence(self):
        for address in self.config['functions'][0]['accounted_blocks']:
            config=json.loads(json.dumps(self.config))
            del config['functions'][0]['block_evidence'][address]
            with self.assertRaises(ValueError):calculate(config,self.inventory)


if __name__ == "__main__":
    unittest.main()
