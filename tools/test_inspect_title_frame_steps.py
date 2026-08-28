import json
from pathlib import Path
import tempfile
import unittest
from PIL import Image
from inspect_title_frame_steps import consecutive_groups, inspect, sha


class TitleStepInspectorTests(unittest.TestCase):
    def test_groups_keep_nonconsecutive_repeats_separate(self):
        self.assertEqual(consecutive_groups([1,2,2,2,2,3,3,3,3,1]),
                         [[1],[2,3,4,5],[6,7,8,9],[10]])
        self.assertEqual(consecutive_groups([]), [])

    def fixture(self, root, colors):
        steps = []
        for n, color in enumerate(colors, 1):
            path = root / f'{n}.png'
            Image.new('RGB', (12, 10), (color, 0, 0)).save(path)
            asset = {'file': path.name, 'sha256': sha(path), 'size': [12, 10]}
            steps.append({'step': n, 'screen': asset, 'debugger': asset.copy()})
        manifest = root / 'observation.json'
        manifest.write_text(json.dumps({'kind': 'original_desktop_frame_steps',
            'title_roi': [1,1,8,8], 'steps': steps}))
        return manifest

    def test_reports_four_step_holds_without_parity_credit(self):
        with tempfile.TemporaryDirectory() as folder:
            path = self.fixture(Path(folder), [1,2,2,2,2,3,3,3,3,4])
            report = inspect(path)
            self.assertEqual(report['observed_four_step_holds'], 2)
            self.assertTrue(report['all_complete_holds_four_steps'])
            self.assertEqual(report['paired_scenario_credit'], 0)
            self.assertEqual(report['native_pixel_parity'], 'not_compared')

    def test_other_pattern_and_static_are_not_four_step_proof(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for colors in ([1,2,2,3], [1]*10):
                report = inspect(self.fixture(root, colors))
                self.assertFalse(report['all_complete_holds_four_steps'])

    def test_tampering_missing_step_and_bad_roi_fail(self):
        with tempfile.TemporaryDirectory() as folder:
            path = self.fixture(Path(folder), [1,2,3])
            original = json.loads(path.read_text())
            for mutate in (lambda m: m['steps'][0]['screen'].update(sha256='0'*64),
                           lambda m: m['steps'][1].update(step=3),
                           lambda m: m.update(title_roi=[0,0,20,10]),
                           lambda m: m['steps'][0]['screen'].update(file='../outside.png')):
                value = json.loads(json.dumps(original)); mutate(value)
                path.write_text(json.dumps(value))
                with self.assertRaises(ValueError): inspect(path)


if __name__ == '__main__':
    unittest.main()
