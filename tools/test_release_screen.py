import unittest
from verify_release_screen import CHECKPOINTS, CUES, validate_summary

SUMMARY = '\n'.join('RELEASE-CHECKPOINT '+name for name in CHECKPOINTS)+'\n'
SUMMARY += 'RELEASE-HOST-VERIFY PASS 30 checkpoints; isolated save only\n'
SUMMARY += 'ba23=(140,10)\nstate=0x11 descriptor=0x800B152C\nrect=121,80,270,125\nsecond NULL\n'
SUMMARY += 'RELEASE-SAVE-FAILED\nRELEASE-COMMIT\nROSTER-RESET-COMMIT\nRELEASE-MUTATE\nPASS 435 real-data donor slots\nreason=retained-pre-child-checkpoint\n'
SUMMARY += ''.join(f'RELEASE-CUE event={event} raw={raw} cue={cue}\n' for (event,raw),cue in CUES.items())


class ReleaseHostEvidenceTests(unittest.TestCase):
    def test_valid_summary(self):
        validate_summary(SUMMARY)

    def test_every_marker_required(self):
        for line in SUMMARY.splitlines():
            with self.subTest(line=line), self.assertRaises(ValueError):
                validate_summary(SUMMARY.replace(line, '', 1))

    def test_duplicate_or_reordered_checkpoint(self):
        for changed in (SUMMARY+'RELEASE-CHECKPOINT entry\n',
                        SUMMARY.replace('RELEASE-CHECKPOINT entry','RELEASE-CHECKPOINT donor-row')):
            with self.assertRaises(ValueError): validate_summary(changed)

    def test_failed_or_pending_actions(self):
        for changed in (SUMMARY+'RELEASE-SAVE-FAILED injected',
                        SUMMARY.replace('raw=2048','raw=2049'),
                        SUMMARY+'RELEASE-PENDING raw=16 pending\n'):
            with self.assertRaises(ValueError): validate_summary(changed)


if __name__ == '__main__': unittest.main()
