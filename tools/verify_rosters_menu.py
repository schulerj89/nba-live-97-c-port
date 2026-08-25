#!/usr/bin/env python3
"""Quantify recovered Rosters card-menu behavior without publishing game assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import wave
from pathlib import Path

from verify_view_rosters import visual_similarity


ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / "reports" / "rosters_menu_fidelity.json"
EXPECTED_Y = [81, 75, 75, 81, 116, 110, 110, 116]
EXPECTED_X = [65, 165, 270, 365, 50, 150, 255, 345]
EXPECTED_ART_Y = [95, 88, 89, 98, 130, 123, 124, 133]
EXPECTED_ART_X = [77, 171, 282, 379, 62, 156, 267, 359]


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
        100 if (metadata.get("stack_y") == EXPECTED_Y and
                metadata.get("stack_x") == EXPECTED_X and
                metadata.get("art_y") == EXPECTED_ART_Y and
                metadata.get("art_x") == EXPECTED_ART_X and
                metadata.get("stack_pair_delta_y") == [35, 35, 35, 35] and
                metadata.get("layout_table") == "0x80094ED4 records 16..39") else 0,
        expected_x=EXPECTED_X, actual_x=metadata.get("stack_x"),
        expected_y=EXPECTED_Y, actual_y=metadata.get("stack_y"),
        expected_art_x=EXPECTED_ART_X, actual_art_x=metadata.get("art_x"),
        expected_art_y=EXPECTED_ART_Y, actual_art_y=metadata.get("art_y"),
        pair_delta_y=metadata.get("stack_pair_delta_y"),
        evidence="FEONLY screen-9 layout table 0x80094ED4 records 16..39")

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
        # Count only the plate rails, not the random ZCARD aperture. This
        # prevents a red player photo from falsely proving the red1 state.
        for name, boxes in {
                "reset": [(365, 81, 465, 106), (365, 106, 385, 175),
                          (445, 106, 465, 175), (365, 155, 465, 175)],
                "injuries": [(345, 116, 445, 141), (345, 141, 365, 210),
                             (425, 141, 445, 210), (345, 190, 445, 210)],
        }.items():
            red_counts[name] = sum(
                1 for box in boxes
                for red, green, blue in initial.crop(box).getdata()
                if red > 60 and red > green * 1.25 and red > blue * 1.25
            )
        red_plates_ok = all(count >= 350 for count in red_counts.values())
    except (NameError, OSError):
        red_plates_ok = False
    add(checks, "authored_red_locked_plates", "layout", 1,
        100 if red_plates_ok else 0, red_dominant_pixels=red_counts,
        minimum_per_card=350,
        evidence="FUN_800399C4 disabled type-2 state 0x80 rendered through ZSET4 red1 CLUT")

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
    plate_present_both_phases = False
    try:
        phase_images = [Image.open(phases[index]).convert("RGB") for index in (0, 1)]
        # View Rosters (index 4) is selected at x=50,y=116. Require opaque,
        # non-background rail pixels in every edge band in both flash states.
        rail_boxes = [(50, 116, 150, 138), (50, 138, 69, 206),
                      (131, 138, 150, 206), (50, 188, 150, 210)]
        rail_counts = [sum(
            1 for box in rail_boxes for red, green, blue in image.crop(box).getdata()
            if max(red, green, blue) - min(red, green, blue) < 85 and
               max(red, green, blue) > 45)
            for image in phase_images]
        plate_present_both_phases = all(count >= 400 for count in rail_counts)
    except (NameError, OSError):
        rail_counts = []
    add(checks, "selection_flash_12_vblanks", "animation", 1.5,
        100 if cadence_ok and plate_present_both_phases else 0,
        unique_frame_states=len(set(hashes)), frame_count=len(hashes),
        plate_present_both_phases=plate_present_both_phases,
        selected_plate_rail_pixels=rail_counts,
        evidence="FUN_8003F240 alternates normal/selected objects without removing the plate")

    title_phases = [capture / f"rosters_title_phase_{index:02d}.ppm"
                    for index in range(4)]
    title_hashes = []
    try:
        for path in title_phases:
            title = Image.open(path).convert("RGB").crop((140, 4, 390, 66))
            title_hashes.append(hashlib.sha256(title.tobytes()).hexdigest())
        title_jumble_ok = len(set(title_hashes)) == 4
    except (NameError, OSError):
        title_jumble_ok = False
    add(checks, "rosters_title_discrete_jumble", "animation", 1,
        100 if title_jumble_ok else 0, phase_count=len(title_hashes),
        unique_title_states=len(set(title_hashes)))

    sound_rows = metadata.get("sounds", [])
    sound_evidence = []
    bank_audio_ok = len(sound_rows) == 6
    pitch_semantics_ok = len(sound_rows) == 6
    audio_hashes = []
    for row in sound_rows:
        path = capture / row["file"]
        raw_path = capture / row["raw_file"]
        try:
            with wave.open(str(path), "rb") as wav:
                valid = (wav.getnchannels() == 1 and wav.getsampwidth() == 2 and
                         wav.getframerate() == row["rate"] == 22050 and
                         wav.getnframes() == row["samples"])
            with wave.open(str(raw_path), "rb") as raw_wav:
                raw_valid = (raw_wav.getnchannels() == 1 and
                             raw_wav.getsampwidth() == 2 and
                             raw_wav.getframerate() == 22050 and
                             raw_wav.getnframes() == row["source_samples"])
            audio_hashes.append(digest(path))
        except (OSError, wave.Error):
            valid = False
            raw_valid = False
        expected_samples = math.ceil(
            row["source_samples"] / (2.0 ** (row["pitch_cents"] / 1200.0)))
        pitch_valid = (row["requested_note"] == 60 and
                       row["pitch_cents"] == -100 * (row["root_note"] - 60) and
                       row["samples"] == expected_samples)
        bank_audio_ok = bank_audio_ok and valid and raw_valid
        pitch_semantics_ok = pitch_semantics_ok and pitch_valid
        sound_evidence.append({"id": row.get("id"), "role": row.get("role"),
                               "samples": row.get("samples"),
                               "source_samples": row.get("source_samples"),
                               "root_note": row.get("root_note"),
                               "pitch_cents": row.get("pitch_cents"),
                               "pitch_valid": pitch_valid, "valid": valid})
    repeated_right = capture / "zcursor_04_right_repeat.wav"
    right_sound = capture / "zcursor_04_right.wav"
    repeated_right_ok = (repeated_right.is_file() and right_sound.is_file() and
                         digest(repeated_right) == digest(right_sound))
    recovered_roles_ok = [row.get("role") for row in sound_rows[:4]] == [
        "down", "up", "left", "right"]
    bank_audio_ok = (bank_audio_ok and len(set(audio_hashes)) == 6 and
                     repeated_right_ok and recovered_roles_ok)
    add(checks, "zcursor_1_through_6", "audio", 0.75,
        100 if bank_audio_ok else 0,
        sounds=sound_evidence, all_waveforms_distinct=len(set(audio_hashes)) == 6,
        direction_mapping={"right": 4, "left": 3, "up": 2, "down": 1},
        repeated_right_byte_identical=repeated_right_ok)
    direction_rows = sound_rows[:4]
    direction_bank_indexing_ok = (
        pitch_semantics_ok and
        [row.get("id") for row in direction_rows] == [1, 2, 3, 4] and
        [row.get("root_note") for row in direction_rows] == [59, 60, 60, 60] and
        [row.get("pitch_cents") for row in direction_rows] == [100, 0, 0, 0])
    add(checks, "zcursor_bnkl_program_semantics", "audio", 0.75,
        100 if direction_bank_indexing_ok else 0,
        evidence="FUN_80091814 bank+8+sound_id*4 PATl lookup plus FUN_8009267C pitch",
        bank_table_offset=8,
        authored_pitch={"requested_note": 60,
                        "direction_root_notes": [59, 60, 60, 60],
                        "direction_pitch_cents": [100, 0, 0, 0]},
        sounds=[{"id": row.get("id"), "root_note": row.get("root_note"),
                 "pitch_cents": row.get("pitch_cents"),
                 "pitch_valid": row.get("pitch_valid")} for row in sound_evidence])

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
