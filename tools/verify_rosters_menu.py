#!/usr/bin/env python3
"""Quantify recovered Rosters card-menu behavior without publishing game assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import wave
from pathlib import Path

from verify_view_rosters import visual_similarity


ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / "reports" / "rosters_menu_fidelity.json"
EXPECTED_Y = [76, 66, 66, 76, 118, 110, 110, 118]


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def add(checks, name: str, group: str, weight: float, score: float, **evidence):
    checks.append({
        "id": name,
        "group": group,
        "weight": weight,
        "score_percent": round(score, 2),
        "credit": round(weight * score / 100.0, 4),
        **evidence,
    })


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture-dir", default=".local/verification/rosters_menu/native")
    parser.add_argument("--reference", default=(
        ".local/verification/rosters_menu/references/rosters_original.png"))
    parser.add_argument("--behavior-pass", action="store_true")
    parser.add_argument("--require-reference", action="store_true")
    args = parser.parse_args()

    capture = ROOT / args.capture_dir
    reference = ROOT / args.reference
    metadata = json.loads((capture / "capture.json").read_text(encoding="utf-8"))
    checks = []

    add(checks, "native_behavior", "behavior", 1, 100 if args.behavior_pass else 0,
        evidence="native self-test covers navigation, lock/unlock and overlay visibility")
    add(checks, "recovered_stack", "layout", 1,
        100 if metadata.get("stack_y") == EXPECTED_Y else 0,
        expected=EXPECTED_Y, actual=metadata.get("stack_y"))

    required_frames = [capture / "rosters_initial.ppm",
                       capture / "rosters_reset_locked_attempt.ppm",
                       capture / "rosters_reset_enabled.ppm",
                       capture / "rosters_injuries_enabled.ppm"]
    try:
        from PIL import Image
        dimensions_ok = all(Image.open(path).size == (512, 240) for path in required_frames)
    except (ImportError, OSError):
        dimensions_ok = False
    add(checks, "capture_dimensions", "layout", 1, 100 if dimensions_ok else 0)

    red_counts = {}
    try:
        initial = Image.open(required_frames[0]).convert("RGB")
        for name, box in {"reset": (364, 76, 464, 170),
                          "injuries": (364, 118, 464, 212)}.items():
            red_counts[name] = sum(
                1 for red, green, blue in initial.crop(box).getdata()
                if red > 60 and red > green * 1.25 and red > blue * 1.25
            )
        red_plates_ok = all(count >= 500 for count in red_counts.values())
    except (NameError, OSError):
        red_plates_ok = False
    add(checks, "authored_red_locked_plates", "layout", 1,
        100 if red_plates_ok else 0, red_dominant_pixels=red_counts,
        minimum_per_card=500)

    locked_same = (all(path.is_file() for path in required_frames[:2]) and
                   digest(required_frames[0]) == digest(required_frames[1]))
    enabled_distinct = (all(path.is_file() for path in required_frames) and
                        digest(required_frames[0]) != digest(required_frames[2]) and
                        digest(required_frames[2]) != digest(required_frames[3]))
    add(checks, "reset_locked_skip", "availability", 1, 100 if locked_same else 0,
        predicate=metadata.get("availability", {}).get("reset"))
    add(checks, "dynamic_unlock_variants", "availability", 1,
        100 if enabled_distinct else 0,
        reset=metadata.get("availability", {}).get("reset"),
        injuries=metadata.get("availability", {}).get("injuries"))

    phases = [capture / f"rosters_select_phase_{index:02d}.ppm" for index in range(12)]
    hashes = [digest(path) for path in phases] if all(path.is_file() for path in phases) else []
    cadence_ok = (len(hashes) == 12 and len(set(hashes[0::2])) == 1 and
                  len(set(hashes[1::2])) == 1 and hashes[0] != hashes[1] and
                  metadata.get("flash_vblanks") == 12)
    add(checks, "selection_flash_12_vblanks", "animation", 1.5,
        100 if cadence_ok else 0, unique_frame_states=len(set(hashes)),
        frame_count=len(hashes))

    sound_rows = metadata.get("sounds", [])
    sound_evidence = []
    audio_ok = len(sound_rows) == 6
    audio_hashes = []
    for row in sound_rows:
        path = capture / row["file"]
        try:
            with wave.open(str(path), "rb") as wav:
                valid = (wav.getnchannels() == 1 and wav.getsampwidth() == 2 and
                         wav.getframerate() == row["rate"] == 22050 and
                         wav.getnframes() == row["samples"])
            audio_hashes.append(digest(path))
        except (OSError, wave.Error):
            valid = False
        audio_ok = audio_ok and valid
        sound_evidence.append({"id": row.get("id"), "role": row.get("role"),
                               "samples": row.get("samples"), "valid": valid})
    repeated_right = capture / "zcursor_01_right_repeat.wav"
    right_sound = capture / "zcursor_01_right.wav"
    repeated_right_ok = (repeated_right.is_file() and right_sound.is_file() and
                         digest(repeated_right) == digest(right_sound))
    recovered_roles_ok = [row.get("role") for row in sound_rows[:4]] == [
        "right", "left", "up", "down"]
    audio_ok = (audio_ok and len(set(audio_hashes)) == 6 and repeated_right_ok and
                recovered_roles_ok)
    add(checks, "zcursor_1_through_6", "audio", 1.5, 100 if audio_ok else 0,
        sounds=sound_evidence, all_waveforms_distinct=len(set(audio_hashes)) == 6,
        direction_mapping={"right": 1, "left": 2, "up": 3, "down": 4},
        repeated_right_byte_identical=repeated_right_ok)

    visual_missing = not reference.is_file()
    if visual_missing:
        visual_score = 0.0
        visual_evidence = {"status": "local_reference_missing"}
    else:
        measured = visual_similarity(
            capture / "rosters_initial.ppm", reference, [0, 0, 1122, 851],
            ROOT / ".local/verification/rosters_menu/diffs", "rosters_menu",
            [4, 4, 252, 188], True)
        visual_score = measured[0]
        visual_evidence = {
            "status": "measured",
            "similarity_percent": measured[0],
            "color_similarity_percent": measured[1],
            "edge_similarity_percent": measured[2],
            "registration_offset": [measured[3], measured[4]],
            "reference_sha256": digest(reference),
        }
    add(checks, "original_screenshot_similarity", "visual", 2, visual_score,
        **visual_evidence)

    possible = sum(check["weight"] for check in checks)
    earned = sum(check["credit"] for check in checks)
    groups = {}
    for check in checks:
        group = groups.setdefault(check["group"], {"earned": 0.0, "possible": 0.0})
        group["earned"] += check["credit"]
        group["possible"] += check["weight"]
    for group in groups.values():
        group["earned"] = round(group["earned"], 4)
        group["possible"] = round(group["possible"], 4)
        group["percent"] = round(group["earned"] * 100 / group["possible"], 2)

    report = {
        "schema_version": 1,
        "scope": "Rosters eight-card selection screen only",
        "method": "recovered predicates + deterministic framebuffer/audio captures + local original screenshot",
        "fidelity_percent": round(earned * 100 / possible, 2),
        "earned_points": round(earned, 4),
        "possible_points": round(possible, 4),
        "groups": groups,
        "checks": checks,
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Rosters menu fidelity: {earned:.2f}/{possible:.2f} = {report['fidelity_percent']:.2f}%")
    for check in checks:
        print(f"  {check['group']:<12} {check['id']:<30} {check['score_percent']:6.2f}%")
    if args.require_reference and visual_missing:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
