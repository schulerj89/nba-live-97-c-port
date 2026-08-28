import unittest
from verify_reset_release_host import outcome, release_state, check_release_state


class ResetReleaseEvidence(unittest.TestCase):
    def test_result_requires_exactly_one_success_and_no_audio_errors(self):
        log = 'REORDER-SAVE-VERIFY reset-confirm PASS; generation=2; default-different=0; team=3; first=45; second=46; selected=4;'
        self.assertEqual(outcome(log, 'reset-confirm')['generation'], 2)
        for invalid in ('', log + log, log + ' AUDIO-ERROR failure'):
            with self.assertRaises(ValueError): outcome(invalid, 'reset-confirm')
        with self.assertRaises(ValueError): outcome(log, 'reset-cancel')

    def test_release_state_requires_one_marker(self):
        log = 'RESET-RELEASE-CHECK player=123; owner=29; pool-count=68; reset-enabled=1'
        self.assertEqual(release_state(log), dict(player=123, owner=29, pool=68, enabled=1))
        for invalid in ('', log + log):
            with self.assertRaises(ValueError): release_state(invalid)

    def test_identity_owner_pool_and_eligibility_checked(self):
        baseline = dict(player=123, owner=3, pool=67, enabled=0)
        released = dict(player=123, owner=29, pool=68, enabled=1)
        check_release_state(released, baseline, True)
        check_release_state(baseline, baseline, False)
        for field in released:
            invalid = dict(released)
            invalid[field] += 1
            with self.assertRaises(ValueError): check_release_state(invalid, baseline, True)
        with self.assertRaises(ValueError): check_release_state(released, baseline, False)


if __name__ == '__main__':
    unittest.main()
