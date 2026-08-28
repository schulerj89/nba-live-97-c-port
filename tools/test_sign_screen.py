"""Synthetic adversarial tests for Sign host evidence; no game assets."""
import unittest
from verify_sign_screen import CHECKPOINTS, CUES, validate_summary

SUMMARY = '\n'.join('SIGN-CHECKPOINT ' + n for n in CHECKPOINTS) + '\n'
SUMMARY += 'SIGN-HOST-VERIFY PASS 26 checkpoints:\nSIGN-SAVE-FAILED injected\nSIGN-COMMIT generation=1\nROSTER-RESET-COMMIT restored\n'
SUMMARY += ''.join(f'SIGN-CUE event={e} raw={r} cue={c}\n' for (e, r), c in CUES.items())
SUMMARY += 'SIGN-STRIP-VERIFY xfrZ 39x156 at296,35; original opaque pixels=5669\n' * 3


class SignHostEvidenceTests(unittest.TestCase):
    def test_valid(self):
        validate_summary(SUMMARY)

    def test_each_marker_required(self):
        for line in SUMMARY.splitlines():
            with self.subTest(line=line), self.assertRaises(ValueError):
                validate_summary(SUMMARY.replace(line, '', 1))

    def test_checkpoint_order_and_duplicates(self):
        for changed in (SUMMARY + 'SIGN-CHECKPOINT entry\n',
                        SUMMARY.replace('SIGN-CHECKPOINT entry', 'SIGN-CHECKPOINT help-first')):
            with self.assertRaises(ValueError):
                validate_summary(changed)

    def test_wrong_cue_and_noop_sound(self):
        for changed in (SUMMARY.replace('raw=1 cue=3', 'raw=1 cue=2'),
                        SUMMARY + 'SIGN-CUE event=0 raw=2 cue=4\n'):
            with self.assertRaises(ValueError):
                validate_summary(changed)


if __name__ == '__main__':
    unittest.main()
