"""Synthetic fixtures only: never promote these checks to original-game evidence."""
import copy
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import wave

from PIL import Image
import verify_reorder_reference as v


class ReferenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.contract = v.read_json(v.CONTRACT)
        # Small invented test clips; production minimums are unchanged.
        for c in self.contract["scenarios"]:
            c["min_frames"] = 2
        self.case = self.contract["scenarios"][0]

    def record(self, path):
        return {"path": str(path.relative_to(self.root)), "sha256": v.sha(path)}

    def fixture(self, kind, case=None):
        case = case or self.case
        directory = self.root / "captures" / case["id"]
        directory.mkdir(parents=True, exist_ok=True)
        n = max(2, len(case["actions"]))
        frames = []
        for i in range(n):
            path = directory / f"{kind}-{i}.png"
            image = Image.new("RGB", (512, 240), (3, 5, 7))
            image.putpixel((i, 0), (20, 30, 40))
            image.save(path)
            frames.append(self.record(path))
        audio = directory / f"{kind}.wav"
        with wave.open(str(audio), "wb") as wav:
            wav.setparams((1, 2, 16000, 0, "NONE", "not compressed"))
            wav.writeframes(struct.pack("<hh", 1000, -1000) * 4096)
        trace = directory / f"{kind}.txt"
        trace.write_text("Synthetic test trace, not an original capture.\n")
        capture = {
            "schema_version": 1, "kind": kind, "scenario": case["id"],
            "contract_sha256": v.digest(self.contract),
            "provenance": {"tool": "unittest", "version": "1", "session": "synthetic",
                           "captured_at": "2026-01-01T00:00:00Z", "notes": "Invented fixture"},
            "continuous_frames": True,
            "asset_sha256": {"overlay": "a"*64, "roster_catalogue": "b"*64},
            "initial_state": dict(case["initial"], slot_table_sha256="c"*64),
            "settings": {"music": 0, "speech": 9, "sfx": 9}, "frame_rate": [60, 1],
            "inputs": [{"frame": i, "action": a} for i, a in enumerate(case["actions"])],
            "events": [{"frame": i, "name": a, "state": dict(
                {k: case["initial"].get(k, bounds[0]) for k, bounds in case.get("event_state_int_ranges", {}).items()},
                step=i)} for i, a in enumerate(case["events"])],
            "frames": frames, "audio": self.record(audio), "audio_frame_zero_sample": 0,
            "raw_trace": self.record(trace)}
        path = directory / f"{kind}.json"
        self.save(path, capture)
        return path, capture

    @staticmethod
    def save(path, value):
        path.write_text(json.dumps(value), encoding="utf-8")

    def load(self, path, kind="original", case=None):
        return v.load_capture(self.root, path, kind, case or self.case, v.digest(self.contract))

    def pair(self):
        a, _ = self.fixture("original")
        b, _ = self.fixture("native")
        return self.load(a), self.load(b, "native")

    def test_production_inventory_covers_all_five_gates(self):
        cases = v.validate_contract(v.read_json(v.CONTRACT))
        self.assertEqual(len(cases), 14)
        expected = {"help_first", "help_replacement", "reorder_navigation", "compare_navigation",
                    "cool_fact_play_stop", "no_cool_fact", "reset_cancel", "reset_confirm"}
        expected |= {f"{child}_{stage}" for child in ("view", "compare")
                     for stage in ("first", "replacement", "swapped")}
        self.assertEqual({c["id"] for c in cases}, expected)
        self.contract["scenarios"] = [c for c in self.contract["scenarios"] if c["gate"] != "help"]
        with self.assertRaises(ValueError):
            v.validate_contract(self.contract)

    def test_missing_references_do_not_pass(self):
        report = v.audit(self.contract, self.root, self.root/"captures")
        self.assertEqual(report["counts"]["missing_capture"], 14)
        self.assertEqual(report["status"], "incomplete")
        self.assertEqual(report["counts"]["supplied_media_equal"], 0)
        self.assertEqual(report["validated_original_captures"], 0)
        self.assertEqual(report["overall_feature_fidelity"], "not_inferred")

    def test_compare_navigation_includes_both_top_guards_and_return_to_top(self):
        case = next(c for c in self.contract["scenarios"] if c["id"] == "compare_navigation")
        self.assertEqual(case["actions"], ["previous_stat", "switch_side", "previous_stat",
            "next_team", "next_stat_layer", "next_stat_layer", "next_stat", "previous_stat",
            "previous_stat", "previous_team"])
        self.assertEqual(case["events"], [a + "_complete" for a in case["actions"]])
        self.assertTrue({"0x8005A1EC", "0x8003D930", "0x8003AB64"} <= set(case["source_anchors"]))
        a, _ = self.fixture("original", case)
        b, native = self.fixture("native", case)
        original = self.load(a, case=case)
        self.assertEqual(v.compare_pair(original, self.load(b, "native", case))["status"], "supplied_media_equal")
        for value in (None, True, "0", 256):
            trial = copy.deepcopy(native)
            trial["events"][0]["state"]["stat_top_right"] = value
            self.save(b, trial)
            with self.assertRaisesRegex(ValueError, "stat_top_right"):
                self.load(b, "native", case)
        trial = copy.deepcopy(native)
        del trial["events"][0]["state"]["stat_top_left"]
        self.save(b, trial)
        with self.assertRaisesRegex(ValueError, "stat_top_left"):
            self.load(b, "native", case)
        # Retain a hypothetical underflow as a difference, never clamp it to
        # the native value or discard it as a malformed positive-only index.
        native["events"][0]["state"]["stat_top_right"] = -1
        self.save(b, native)
        result = v.compare_pair(original, self.load(b, "native", case))
        self.assertEqual(result["status"], "differences")
        self.assertFalse(result["state_and_event_timing_equal"])

    def test_contract_rejects_invalid_state_ranges_and_unpaired_events(self):
        for bounds in ([0], [1, 0], [False, 1], [0, "1"]):
            trial = copy.deepcopy(self.contract)
            trial["scenarios"][0]["event_state_int_ranges"] = {"stat_top_left": bounds}
            with self.assertRaises(ValueError):
                v.validate_contract(trial)
        trial = copy.deepcopy(self.contract)
        trial["scenarios"][0]["events"].pop()
        with self.assertRaises(ValueError):
            v.validate_contract(trial)

    def test_all_supplied_synthetic_pairs_equal_but_not_authenticated(self):
        for case in self.contract["scenarios"]:
            self.fixture("original", case); self.fixture("native", case)
        report = v.audit(self.contract, self.root, self.root/"captures")
        self.assertEqual(report["counts"]["supplied_media_equal"], 14)
        self.assertEqual(report["capture_authenticity"], "requires_independent_source_review")

    def test_exact_decoded_media_and_events(self):
        self.assertEqual(v.compare_pair(*self.pair())["status"], "supplied_media_equal")

    def test_one_pixel_difference_is_not_averaged_away(self):
        original, native = self.pair()
        path = native["_frame_paths"][0]
        with Image.open(path) as source:
            image = source.copy()
        image.putpixel((100, 100), (4, 5, 7)); image.save(path)
        result = v.compare_pair(original, native)
        self.assertEqual(result["status"], "differences")
        self.assertEqual(result["video"]["different_pixels"], 1)
        self.assertEqual(result["video"]["max_channel_error"], 1)

    def test_audio_gain_and_latency_are_not_normalized(self):
        original, native = self.pair()
        rate, channels, pcm = native["_audio"]
        native["_audio"] = rate, channels, struct.pack("<h", 500) + pcm[2:]
        result = v.compare_pair(original, native)
        self.assertEqual(result["status"], "differences")
        self.assertEqual(result["audio"]["max_absolute_error"], 500)
        native["_audio"] = rate, channels, b"\0\0" + pcm[:-2]
        self.assertFalse(v.compare_pair(original, native)["audio"]["equal"])

    def test_different_audio_format_is_not_resampled(self):
        original, native = self.pair()
        native["_audio"] = 22050, 1, native["_audio"][2]
        self.assertEqual(v.compare_pair(original, native)["audio"]["status"], "not_comparable")
        self.assertEqual(v.compare_pair(original, native)["status"], "not_comparable")

    def test_clock_and_event_timing_differences_remain_visible(self):
        original, native = self.pair()
        native["frame_rate"] = [1000, 17]
        result = v.compare_pair(original, native)
        self.assertFalse(result["video"]["clock_equal"])
        native["events"][0]["frame"] = 1
        self.assertFalse(v.compare_pair(original, native)["state_and_event_timing_equal"])

    def test_mismatched_scenario_inputs_settings_or_baseline_refused(self):
        original, native = self.pair()
        for field in ("inputs", "settings", "initial_state", "asset_sha256"):
            trial = copy.deepcopy(native)
            trial[field] = {}
            self.assertEqual(v.compare_pair(original, trial)["status"], "not_comparable")

    def test_wrong_contract_kind_or_event_order_refused(self):
        for change in ({"contract_sha256": "d"*64}, {"kind": "native"},
                       {"scenario": "not_this_case"}, {"events": []}):
            path, capture = self.fixture("original")
            capture.update(change); self.save(path, capture)
            with self.assertRaises(ValueError):
                self.load(path)

    def test_negative_marker_missing_trace_and_stills_refused(self):
        for field in ("negative", "trace", "continuous", "one_frame"):
            path, capture = self.fixture("original")
            if field == "negative": capture["inputs"][0]["frame"] = -1
            elif field == "trace": capture["raw_trace"]["path"] = "absent.txt"
            elif field == "continuous": capture["continuous_frames"] = False
            else: capture["frames"] = capture["frames"][:1]
            self.save(path, capture)
            with self.assertRaises(ValueError):
                self.load(path)

    def test_frame_hash_escape_size_and_static_sequences_refused(self):
        for fault in ("hash", "escape", "size", "static", "repeated_path"):
            path, capture = self.fixture("original")
            if fault == "hash": capture["frames"][0]["sha256"] = "d"*64
            elif fault == "escape": capture["frames"][0]["path"] = "../outside.png"
            elif fault == "size":
                p = self.root/capture["frames"][0]["path"]
                Image.new("RGB", (1, 1)).save(p); capture["frames"][0] = self.record(p)
            elif fault == "repeated_path": capture["frames"][1] = capture["frames"][0]
            else:
                p = self.root/capture["frames"][1]["path"]
                p.write_bytes((self.root/capture["frames"][0]["path"]).read_bytes())
                capture["frames"][1] = self.record(p)
            self.save(path, capture)
            with self.assertRaises(ValueError):
                self.load(path)

    def test_silent_truncated_and_short_audio_refused(self):
        for fault in ("silent", "truncated", "short", "oversized"):
            path, capture = self.fixture("original")
            audio = self.root/capture["audio"]["path"]
            if fault == "truncated":
                audio.write_bytes(audio.read_bytes()[:-2])
            elif fault == "oversized":
                data = bytearray(audio.read_bytes())
                struct.pack_into("<I", data, 40, 0xfffffff0)
                audio.write_bytes(data)
            else:
                with wave.open(str(audio), "wb") as wav:
                    wav.setparams((1, 2, 16000, 0, "NONE", "not compressed"))
                    wav.writeframes(b"\0\0"*4096 if fault == "silent" else b"\1\0")
            capture["audio"] = self.record(audio); self.save(path, capture)
            with self.assertRaises(ValueError):
                self.load(path)

    def test_duplicate_keys_nonfinite_json_and_output_escape_refused(self):
        path = self.root/"bad.json"
        for content in ('{"a":1,"a":2}', '{"a":NaN}'):
            path.write_text(content)
            with self.assertRaises(ValueError):
                v.read_json(path)
        with self.assertRaises(ValueError):
            v.private_path(self.root, "../outside.json")

    def test_cli_rejects_nonprivate_input_and_nonreport_output(self):
        for args in (["--captures", "../outside"], ["--output", "saves/overwritten.json"]):
            result = subprocess.run([sys.executable, str(Path(v.__file__)), *args],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn("REFERENCE INVALID INPUT", result.stderr)


if __name__ == "__main__":
    unittest.main()
