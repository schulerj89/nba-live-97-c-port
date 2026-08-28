"""Fail-closed host evidence checks, using synthetic CLI text, never game assets."""
import unittest
from verify_trade_screen import validate_summary

SUMMARY = '''TRADE-HOST-VERIFY   PASS
TRADE-MATRIX   PASS 6525
TRADE-CANCEL-VERIFY   PASS
TRADE-DRAFT-CHILDREN PASS
TRADE-SYNC-VERIFY PASS
TRADE-QUIRK-VERIFY PASS 4
TRADE-TRANSFER-VERIFY PASS
TRADE-RECORD-VERIFY PASS
TRADE-SAVE-FAILED injected
TRADE-COMMIT generation=1
ROSTER-RESET-COMMIT restored
TRADE-CUE event=1 raw=1 cue=3
TRADE-CUE event=1 raw=2 cue=4
TRADE-CUE event=2 raw=8 cue=2
TRADE-CUE event=2 raw=4 cue=1
TRADE-CUE event=3 raw=2048 cue=6
TRADE-CUE event=4 raw=2048 cue=6
TRADE-CUE event=5 raw=256 cue=10
TRADE-CUE event=10 raw=16 cue=6
TRADE-CUE event=11 raw=64 cue=6
TRADE-MATRIX-OUTCOMES swap=4000 transfer=2000 both-empty=500 minimum=25;
'''

class TradeHostEvidenceTests(unittest.TestCase):
    def test_exact_outcome_counts(self):
        self.assertEqual(validate_summary(SUMMARY),dict(swap=4000,transfer=2000,both_empty=500,minimum=25))

    def test_each_required_marker(self):
        for line in SUMMARY.splitlines():
            with self.subTest(line=line),self.assertRaises(ValueError):
                validate_summary(SUMMARY.replace(line,'',1))

    def test_wrong_totals_or_missing_success_classes(self):
        for changed in (SUMMARY.replace('swap=4000','swap=3999'),
                        SUMMARY.replace('swap=4000 transfer=2000','swap=0 transfer=6000'),
                        SUMMARY.replace('swap=4000 transfer=2000','swap=6000 transfer=0'),
                        SUMMARY.replace('minimum=25','minimum=-25')):
            with self.subTest(changed=changed),self.assertRaises(ValueError):validate_summary(changed)

    def test_duplicate_outcomes(self):
        with self.assertRaises(ValueError):validate_summary(SUMMARY+SUMMARY.splitlines()[-1])

    def test_wrong_cues_and_noop_sound(self):
        for changed in (SUMMARY.replace('raw=1 cue=3','raw=1 cue=2'),
                        SUMMARY.replace('raw=256 cue=10','raw=256 cue=8'),
                        SUMMARY+'TRADE-CUE event=0 raw=2 cue=4\n'):
            with self.subTest(changed=changed),self.assertRaises(ValueError):validate_summary(changed)

if __name__=='__main__':unittest.main()
