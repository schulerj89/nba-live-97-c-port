"""Regression checks for count-only native-port reporting (no game assets needed)."""

import copy
import unittest
import xml.etree.ElementTree as ET
from collections import Counter

import report_progress as progress


class MilestoneReportingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report = progress.build_report()

    def test_counts_match_manifest_and_subsystems(self):
        native = self.report["native_port"]
        features = progress.read_json(progress.CONFIG / "features.json")["features"]
        self.assertEqual(self.report["schema_version"], 2)
        self.assertEqual(native["catalogued_features"], len(features))
        self.assertEqual(native["status_counts"], Counter(f["status"] for f in features))
        self.assertEqual(sum(g["items"] for g in native["groups"].values()), len(features))
        for name, group in native["groups"].items():
            self.assertEqual(group["status_counts"],
                             Counter(f["status"] for f in features if f["group"] == name))
            self.assertEqual(group["items"], sum(group["status_counts"].values()))

    def test_no_weighted_roadmap_fields(self):
        native = self.report["native_port"]
        for key in ("roadmap_completion_percent", "roadmap_points"):
            self.assertNotIn(key, native)
        for group in native["groups"].values():
            self.assertNotIn("completion_percent", group)
            self.assertNotIn("points", group)
        model = progress.read_json(progress.CONFIG / "project.json")["progress_model"]
        self.assertNotIn("feature_credit", model)

    def test_every_renderer_includes_counts_and_gameplay_note(self):
        native = self.report["native_port"]
        for render in (progress.render_markdown, progress.render_html, progress.render_svg):
            output = render(self.report)
            self.assertIn(progress.milestone_counts(native["status_counts"]), output)
            self.assertIn(progress.gameplay_note(native), output)
            self.assertNotIn("Estimated completion", output)
            self.assertNotIn("native-port roadmap estimate", output)
        ET.fromstring(progress.render_svg(self.report))

    def test_implemented_status_is_not_hidden(self):
        counts = {"verified": 2, "implemented": 3, "partial": 4, "not_started": 5}
        self.assertEqual(progress.milestone_counts(counts),
                         "2 marked verified, 3 implemented, 4 partial, 5 not started")
        self.assertEqual(progress.milestone_counts({"partial": 1}), "1 partial")

    def test_gameplay_note_tracks_manifest_status(self):
        native = copy.deepcopy(self.report["native_port"])
        native["playable_basketball_status"] = "not_started"
        self.assertEqual(progress.gameplay_note(native),
                         "Playable basketball remains unimplemented.")
        native["playable_basketball_status"] = "partial"
        self.assertIn("marked partial", progress.gameplay_note(native))
        self.assertNotIn("remains unimplemented", progress.gameplay_note(native))


if __name__ == "__main__":
    unittest.main()
