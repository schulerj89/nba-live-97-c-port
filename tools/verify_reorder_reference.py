#!/usr/bin/env python3
"""Fail-closed comparison of supplied Re-order capture pairs, not a fidelity score."""
from __future__ import annotations

import argparse
from array import array
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import sys
import wave

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / "config/decomp/reorder_reference_scenarios.json"
HEX = re.compile(r"^[0-9a-f]{64}$")
ID = re.compile(r"^[a-z][a-z0-9_]*$")
FRAME_BYTES_LIMIT = 4 * 1024 * 1024
AUDIO_BYTES_LIMIT = 32 * 1024 * 1024


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def sha(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def read_json(path):
    require(path.stat().st_size <= FRAME_BYTES_LIMIT, "JSON exceeds capture bound")
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, "duplicate JSON key")
            result[key] = value
        return result
    def constant(_):
        raise ValueError("non-finite JSON number")
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=pairs, parse_constant=constant)


def private_path(root, name):
    require(isinstance(name, str) and name and not Path(name).is_absolute(), "expected relative private path")
    result = (root / name).resolve()
    require(result.is_relative_to(root.resolve()) and result != root.resolve(), "path escapes private capture root")
    return result


def artifact(root, record, limit):
    require(isinstance(record, dict) and set(record) == {"path", "sha256"}, "invalid artifact record")
    require(HEX.fullmatch(record["sha256"]) is not None, "invalid artifact hash")
    path = private_path(root, record["path"])
    require(path.is_file() and path.stat().st_size <= limit, "missing or oversized artifact")
    require(sha(path) == record["sha256"], "artifact hash mismatch")
    return path


def validate_contract(contract):
    require(contract.get("schema_version") == 1 and contract.get("scope") == "reorder_reference",
            "unsupported reference contract")
    policy = contract["comparison_policy"]
    require(policy == {
        "video": "unfiltered_512x240_rgb_by_frame_no_warp",
        "audio": "pcm16_no_gain_normalization_no_latency_search",
        "result": "supplied_media_only_not_overall_fidelity",
        "private_root": ".local", "max_frames": 6000}, "comparison policy changed")
    cases = contract["scenarios"]
    require(cases and len({c["id"] for c in cases}) == len(cases), "empty/duplicate scenarios")
    require(set(contract["gates"]) == {"help", "view", "compare", "persistence_reset", "animation_audio"},
            "goal gates changed")
    require({c["gate"] for c in cases} == set(contract["gates"]), "uncovered goal gate")
    for case in cases:
        require(ID.fullmatch(case["id"]) is not None, "unsafe scenario ID")
        require(case["initial"] and case["actions"] and case["events"], "missing scenario requirements")
        require(all(isinstance(x, str) and ID.fullmatch(x) for x in case["actions"] + case["events"]),
                "invalid action/event names")
        require(len(case["actions"]) == len(case["events"]), "each input needs a completion marker")
        ranges = case.get("event_state_int_ranges", {})
        require(isinstance(ranges, dict), "invalid event state ranges")
        for field, bounds in ranges.items():
            require(isinstance(field, str) and ID.fullmatch(field) and isinstance(bounds, list) and
                    len(bounds) == 2 and all(type(x) is int for x in bounds) and bounds[0] <= bounds[1],
                    "invalid event state integer range")
        require(2 <= case["min_frames"] <= 6000, "invalid frame requirement")
        require(case["require_motion"] is True and case["require_nonzero_audio"] is True,
                "motion/audio requirements cannot silently disappear")
        require(case["source_anchors"] and all(re.fullmatch(r"0x800[0-9A-F]{5}", x)
                for x in case["source_anchors"]), "missing source anchors")
    return cases


def load_capture(private_root, path, kind, case, contract_hash):
    capture = read_json(path)
    require(capture.get("schema_version") == 1 and capture.get("kind") == kind and
            capture.get("scenario") == case["id"] and capture.get("contract_sha256") == contract_hash,
            "capture kind/scenario/contract mismatch")
    provenance = capture["provenance"]
    require(all(isinstance(provenance.get(k), str) and provenance[k].strip()
                for k in ("tool", "version", "session", "captured_at", "notes")), "missing capture provenance")
    require(capture.get("continuous_frames") is True, "checkpoint stills are not continuous capture")
    assets = capture["asset_sha256"]
    require(set(assets) == {"overlay", "roster_catalogue"} and
            all(isinstance(v, str) and HEX.fullmatch(v) for v in assets.values()), "invalid source identity")
    initial = capture["initial_state"]
    require(isinstance(initial, dict) and all(initial.get(k) == v for k, v in case["initial"].items()),
            "wrong initial scenario state")
    require(HEX.fullmatch(initial.get("slot_table_sha256", "")) is not None, "missing initial slot-table identity")
    settings = capture["settings"]
    require(set(settings) == {"music", "speech", "sfx"} and
            all(type(v) is int and 0 <= v <= 9 for v in settings.values()), "invalid audio settings")
    frames = capture["frames"]
    require(isinstance(frames, list) and case["min_frames"] <= len(frames) <= 6000,
            "too few/many continuous frames")
    rate = capture["frame_rate"]
    require(isinstance(rate, list) and len(rate) == 2 and
            all(type(v) is int and 0 < v <= 1000000 for v in rate) and 1 <= rate[0] / rate[1] <= 240,
            "invalid frame clock")
    require([i["action"] for i in capture["inputs"]] == case["actions"], "wrong/missing input sequence")
    require([e["name"] for e in capture["events"]] == case["events"], "wrong/missing event sequence")
    for sequence in (capture["inputs"], capture["events"]):
        previous = -1
        for event in sequence:
            frame = event["frame"]
            require(type(frame) is int and 0 <= frame < len(frames) and previous <= frame,
                    "unordered/out-of-range frame marker")
            previous = frame
    require(all(a["frame"] < b["frame"] for a, b in zip(capture["inputs"], capture["inputs"][1:])),
            "sequential actions require distinct input frames")
    require(all(e["frame"] >= i["frame"] for i, e in zip(capture["inputs"], capture["events"])),
            "completion event precedes its input")
    require(all(isinstance(e.get("state"), dict) and e["state"] for e in capture["events"]), "missing event state")
    for event in capture["events"]:
        for field, (low, high) in case.get("event_state_int_ranges", {}).items():
            value = event["state"].get(field)
            require(type(value) is int and low <= value <= high, "missing/invalid event state field: " + field)
    # The trace is retained for human provenance review. Its mere presence does
    # not authenticate the sidecar's state annotations or prove original execution.
    trace_path = artifact(private_root, capture["raw_trace"], FRAME_BYTES_LIMIT)
    require(trace_path.stat().st_size > 0, "empty source trace")
    from PIL import Image
    paths, pixel_hashes = [], set()
    for record in frames:
        frame_path = artifact(private_root, record, FRAME_BYTES_LIMIT)
        try:
            image = Image.open(frame_path)
        except Image.DecompressionBombError as error:
            raise ValueError("oversized decoded frame") from error
        with image:
            require(image.format in ("PNG", "PPM") and image.size == (512, 240) and image.mode == "RGB",
                    "frames must be lossless unfiltered 512x240 RGB")
            require(getattr(image, "n_frames", 1) == 1, "one image required per frame record")
            pixel_hashes.add(hashlib.sha256(image.tobytes()).hexdigest())
        paths.append(frame_path)
    require(len(set(paths)) == len(paths), "repeated frame paths are not a continuous capture")
    require(len(pixel_hashes) > 1, "static checkpoints cannot demonstrate animation")
    audio_path = artifact(private_root, capture["audio"], AUDIO_BYTES_LIMIT)
    with wave.open(str(audio_path), "rb") as wav:
        require(wav.getcomptype() == "NONE" and wav.getsampwidth() == 2 and
                wav.getnchannels() in (1, 2) and 8000 <= wav.getframerate() <= 192000,
                "audio must be mono/stereo PCM16")
        count, channels, sample_rate = wav.getnframes(), wav.getnchannels(), wav.getframerate()
        require(0 < count * channels * 2 <= AUDIO_BYTES_LIMIT, "declared PCM payload exceeds bound")
        pcm = wav.readframes(count)
        require(count > 0 and len(pcm) == count * channels * 2, "empty/truncated WAV")
    offset = capture["audio_frame_zero_sample"]
    require(type(offset) is int and 0 <= offset < count, "invalid declared audio origin")
    pcm = pcm[offset * channels * 2:]
    require(any(pcm), "required cue capture contains only silence")
    expected_duration = len(frames) * rate[1] / rate[0]
    require(len(pcm) / (channels * 2) >= math.floor(expected_duration * sample_rate),
            "audio does not span the captured frame sequence")
    capture["_frame_paths"] = paths
    capture["_audio"] = (sample_rate, channels, pcm)
    capture["_manifest_sha256"] = sha(path)
    return capture


def compare_pair(original, native):
    from PIL import Image, ImageChops
    reasons = []
    for field in ("asset_sha256", "initial_state", "settings", "inputs"):
        if original[field] != native[field]:
            reasons.append(field + "_differs")
    if reasons:
        return {"status": "not_comparable", "reasons": reasons}
    video = {"frames_original": len(original["frames"]), "frames_native": len(native["frames"]),
             "different_frames": [], "different_pixels": 0, "max_channel_error": 0}
    total_error = total_channels = 0
    for number, (a, b) in enumerate(zip(original["_frame_paths"], native["_frame_paths"])):
        with Image.open(a) as left, Image.open(b) as right:
            difference = ImageChops.difference(left, right)
            hist = difference.histogram()
            total_error += sum((i % 256) * count for i, count in enumerate(hist))
            total_channels += 512 * 240 * 3
            channels = difference.split()
            mask = ImageChops.lighter(ImageChops.lighter(channels[0], channels[1]), channels[2])
            changed = 512 * 240 - mask.histogram()[0]
            if changed:
                video["different_frames"].append(number)
                video["different_pixels"] += changed
                video["max_channel_error"] = max(video["max_channel_error"],
                    max(i % 256 for i, count in enumerate(hist) if count))
    video["mean_absolute_channel_error"] = total_error / total_channels
    video["clock_equal"] = (original["frame_rate"][0] * native["frame_rate"][1] ==
                            native["frame_rate"][0] * original["frame_rate"][1])
    video["equal"] = (not video["different_frames"] and video["clock_equal"] and
                      len(original["frames"]) == len(native["frames"]))
    audio = {"original_format": list(original["_audio"][:2]), "native_format": list(native["_audio"][:2])}
    if original["_audio"][:2] != native["_audio"][:2]:
        audio.update(status="not_comparable", equal=False)
    else:
        a, b = original["_audio"][2], native["_audio"][2]
        left, right = array("h"), array("h")
        left.frombytes(a); right.frombytes(b)
        if sys.byteorder != "little":
            left.byteswap(); right.byteswap()
        max_error = squared_error = compared = 0
        for x, y in zip(left, right):
            error = int(x) - int(y)
            max_error = max(max_error, abs(error))
            squared_error += error * error
            compared += 1
        audio.update(status="compared", equal=a == b,
                     samples_original=len(left), samples_native=len(right),
                     max_absolute_error=max_error,
                     rms_error=math.sqrt(squared_error / max(1, compared)),
                     original_rms=math.sqrt(sum(int(x)*x for x in left) / len(left)),
                     native_rms=math.sqrt(sum(int(x)*x for x in right) / len(right)),
                     alignment="declared_frame_zero_only_no_search")
    states_equal = original["events"] == native["events"]
    exact = video["equal"] and audio["equal"] and states_equal
    status = ("not_comparable" if audio["status"] == "not_comparable" else
              "supplied_media_equal" if exact else "differences")
    return {"status": status,
            "video": video, "audio": audio, "state_and_event_timing_equal": states_equal,
            "event_comparison": [{"name": a["name"], "original_frame": a["frame"], "native_frame": b["frame"],
                                  "state_equal": a["state"] == b["state"]}
                                 for a, b in zip(original["events"], native["events"])]}


def audit(contract, private_root, capture_dir):
    cases = validate_contract(contract)
    results = []
    for case in cases:
        result = {"id": case["id"], "gate": case["gate"], "status": "missing_capture"}
        captures = {}
        for kind in ("original", "native"):
            path = capture_dir / case["id"] / (kind + ".json")
            if not path.is_file():
                result[kind] = {"status": "missing"}
                continue
            try:
                require(path.resolve().is_relative_to(private_root.resolve()), "manifest escapes private root")
                captures[kind] = load_capture(private_root, path, kind, case, digest(contract))
                result[kind] = {"status": "validated_supplied_capture", "manifest_sha256": sha(path)}
            except (ValueError, KeyError, TypeError, OSError, wave.Error) as error:
                result[kind] = {"status": "invalid", "reason": str(error)}
                result["status"] = "invalid_capture"
        if len(captures) == 2:
            result.update(compare_pair(captures["original"], captures["native"]))
        results.append(result)
    counts = {name: sum(r["status"] == name for r in results) for name in (
        "missing_capture", "invalid_capture", "not_comparable", "differences", "supplied_media_equal")}
    status = ("incomplete" if any(counts[x] for x in ("missing_capture", "invalid_capture", "not_comparable"))
              else "differences" if counts["differences"] else "supplied_media_equal")
    return {"schema_version": 1, "scope": "reorder_supplied_reference_media",
            "status": status,
            "executed_at_utc": datetime.now(timezone.utc).isoformat(),
            "contract_sha256": digest(contract), "required_scenarios": len(cases),
            "counts": counts, "scenarios": results,
            "validated_original_captures": sum(r["original"]["status"] == "validated_supplied_capture" for r in results),
            "validated_native_captures": sum(r["native"]["status"] == "validated_supplied_capture" for r in results),
            "by_gate": {gate: {"required": sum(r["gate"] == gate for r in results),
                              "supplied_media_equal": sum(r["gate"] == gate and
                                  r["status"] == "supplied_media_equal" for r in results)}
                        for gate in contract["gates"]},
            "capture_authenticity": "requires_independent_source_review",
            "overall_feature_fidelity": "not_inferred",
            "native_checkpoint_images": "not_substituted_for_continuous_reference_pairs"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-config", action="store_true")
    parser.add_argument("--captures", default="verification/reorder/reference")
    parser.add_argument("--output", default="reports/reorder_reference_run.json")
    args = parser.parse_args()
    contract = read_json(CONTRACT)
    cases = validate_contract(contract)
    if args.check_config:
        print(f"Re-order reference contract valid: {len(cases)} scenarios; no reference comparisons run")
        return 0
    private_root = ROOT / ".local"
    capture_dir = private_path(private_root, args.captures)
    output = private_path(private_root, args.output)
    require(output.is_relative_to((private_root / "reports").resolve()) and output.suffix == ".json",
            "reference reports must stay under .local/reports as JSON")
    report = audit(contract, private_root, capture_dir)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("REORDER REFERENCE", json.dumps(report["counts"]), f"required={len(cases)}")
    print("Supplied media only; source accounting/native regressions/overall fidelity remain separate.")
    counts = report["counts"]
    if counts["missing_capture"] or counts["invalid_capture"] or counts["not_comparable"]:
        return 2
    return 1 if counts["differences"] else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, KeyError, TypeError, OSError, wave.Error) as error:
        print("REFERENCE INVALID INPUT:", error, file=sys.stderr)
        raise SystemExit(2)
