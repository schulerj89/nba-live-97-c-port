#!/usr/bin/env python3
"""Measure local-only View Rosters behavior and visual-reference fidelity."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "config" / "decomp" / "view_rosters_verification.json"
PUBLIC_REPORT = ROOT / "reports" / "view_rosters_fidelity.json"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def image_crop_sha256(path: Path, crop) -> str:
    from PIL import Image
    image = Image.open(path).convert("RGB").crop(tuple(crop))
    return hashlib.sha256(image.tobytes()).hexdigest()


def visual_similarity(native_path: Path, reference_path: Path, crop, proof_dir: Path,
                      proof_name: str, comparison_region=None, write_proof: bool = True):
    try:
        from PIL import Image, ImageChops, ImageFilter, ImageStat
    except ImportError as error:
        raise RuntimeError("Pillow is required for local visual verification") from error

    native = Image.open(native_path).convert("RGB")
    reference = Image.open(reference_path).convert("RGB")
    if crop:
        reference = reference.crop(tuple(crop))
    # PS1 512x240 pixels are displayed at a 4:3 aspect ratio. Comparing both
    # sources at a modest common presentation size tolerates emulator/window
    # scaling while retaining layout, font, sprite, and palette differences.
    size = (256, 192)
    native = native.resize(size, Image.Resampling.LANCZOS)
    reference = reference.resize(size, Image.Resampling.LANCZOS)
    native_edges = native.convert("L").filter(ImageFilter.FIND_EDGES)
    # Window screenshots can differ by a few scaled pixels even with the same
    # 512x240 framebuffer. Register a bounded +/-4 presentation-pixel offset;
    # report that offset so the tolerance remains visible and auditable.
    comparison_box = tuple(comparison_region) if comparison_region else \
        (6, 6, size[0] - 6, size[1] - 6)
    best = None
    for offset_y in range(-4, 5):
        for offset_x in range(-4, 5):
            aligned = Image.new("RGB", size)
            aligned.paste(reference, (offset_x, offset_y))
            color_diff = ImageChops.difference(native, aligned).crop(comparison_box)
            color_mean = sum(ImageStat.Stat(color_diff).mean) / 3.0
            aligned_edges = aligned.convert("L").filter(ImageFilter.FIND_EDGES)
            edge_diff = ImageChops.difference(native_edges, aligned_edges).crop(comparison_box)
            edge_mean = ImageStat.Stat(edge_diff).mean[0]
            color_score = max(0.0, 100.0 * (1.0 - color_mean / 255.0))
            edge_score = max(0.0, 100.0 * (1.0 - edge_mean / 255.0))
            score = 0.65 * color_score + 0.35 * edge_score
            candidate = (score, color_score, edge_score, offset_x, offset_y, aligned)
            if best is None or candidate[0] > best[0]:
                best = candidate
    score, color_score, edge_score, offset_x, offset_y, reference = best
    color_diff = ImageChops.difference(native, reference)
    native_region = native.crop(comparison_box)
    reference_region = reference.crop(comparison_box)
    pixel_count = max(1, native_region.width * native_region.height)
    native_dark = 100.0 * sum(max(pixel) < 32 for pixel in native_region.getdata()) / pixel_count
    reference_dark = 100.0 * sum(max(pixel) < 32 for pixel in reference_region.getdata()) / pixel_count

    # The heatmap is diagnostic evidence and remains under .local/.
    if write_proof:
        proof_dir.mkdir(parents=True, exist_ok=True)
        Image.open(native_path).convert("RGB").save(proof_dir / f"{proof_name}_native_raw.png")
        native.save(proof_dir / f"{proof_name}_native.png")
        reference.save(proof_dir / f"{proof_name}_reference.png")
        heatmap = color_diff.convert("L").point(lambda value: min(255, value * 3))
        heatmap.save(proof_dir / f"{proof_name}_diff.png")
    return (round(score, 2), round(color_score, 2), round(edge_score, 2),
            offset_x, offset_y, round(native_dark, 2), round(reference_dark, 2))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture-dir", default=".local/verification/view_rosters/native")
    parser.add_argument("--reference-dir", default=".local/verification/view_rosters/references")
    parser.add_argument("--behavior-pass", action="store_true",
                        help="credit checks exercised by the passing native self-test")
    parser.add_argument("--require-references", action="store_true")
    args = parser.parse_args()

    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    capture_dir = ROOT / args.capture_dir
    reference_dir = ROOT / args.reference_dir
    metadata_path = capture_dir / "capture.json"
    metadata = json.loads(metadata_path.read_text(encoding="utf-8")) if metadata_path.exists() else {}
    expected_captures = metadata.get("captures", [])
    capture_dimensions_pass = bool(expected_captures)
    if capture_dimensions_pass:
        try:
            from PIL import Image
            capture_dimensions_pass = all(
                Image.open(capture_dir / name).size == (512, 240)
                for name in expected_captures
            )
        except (ImportError, OSError):
            capture_dimensions_pass = False

    results = []
    earned = 0.0
    possible = 0.0
    group_totals = {}
    missing_references = []
    diff_dir = ROOT / ".local" / "verification" / "view_rosters" / "diffs"
    for check in manifest["checks"]:
        weight = float(check["weight"])
        possible += weight
        result = {
            "id": check["id"],
            "group": check["group"],
            "weight": weight,
            "description": check["description"],
        }
        if check["source"] == "native_self_test":
            score = 100.0 if args.behavior_pass else 0.0
            result["status"] = "pass" if args.behavior_pass else "not_run"
        elif check["source"] == "capture":
            score = 100.0 if capture_dimensions_pass else 0.0
            result["status"] = "pass" if capture_dimensions_pass else "fail"
        elif check["source"] == "capture_difference":
            baseline = capture_dir / check["baseline"]
            changed = capture_dir / check["changed"]
            passed = (baseline.is_file() and changed.is_file() and
                      sha256(baseline) != sha256(changed))
            score = 100.0 if passed else 0.0
            result["status"] = "pass" if passed else "fail"
            if baseline.is_file():
                result["baseline_sha256"] = sha256(baseline)
            if changed.is_file():
                result["changed_sha256"] = sha256(changed)
        elif check["source"] == "capture_cadence":
            paths = [capture_dir / name for name in check["frames"]]
            if all(path.is_file() for path in paths):
                hashes = [image_crop_sha256(path, check["crop"]) for path in paths]
                hold_pass = all(hashes[left] == hashes[right]
                                for left, right in check["equal_pairs"])
                jump_pass = all(hashes[left] != hashes[right]
                                for left, right in check["different_pairs"])
                passed = hold_pass and jump_pass
                result["frame_crop_sha256"] = hashes
            else:
                passed = False
            score = 100.0 if passed else 0.0
            result["status"] = "pass" if passed else "fail"
        elif check["source"] == "visual_reference":
            native_paths = (
                sorted(capture_dir.glob(check["native_pattern"]))
                if "native_pattern" in check
                else [capture_dir / check["native"]]
            )
            reference_path = reference_dir / check["reference"]
            native_paths = [path for path in native_paths if path.exists()]
            if not native_paths or not reference_path.exists():
                score = 0.0
                result["status"] = "reference_missing"
                missing_references.append(check["id"])
            else:
                measured = [
                    (visual_similarity(path, reference_path, check.get("reference_crop"),
                                       diff_dir, check["id"],
                                       check.get("comparison_region"), False), path)
                    for path in native_paths
                ]
                (_, best_path) = max(measured, key=lambda item: item[0][0])
                (score, color_score, edge_score, offset_x, offset_y,
                 native_dark, reference_dark) = visual_similarity(
                    best_path, reference_path, check.get("reference_crop"),
                    diff_dir, check["id"], check.get("comparison_region"), True)
                result.update(
                    status="measured",
                    similarity_percent=score,
                    color_similarity_percent=color_score,
                    edge_similarity_percent=edge_score,
                    reference_sha256=sha256(reference_path),
                    matched_native_capture=best_path.name,
                    registration_offset=[offset_x, offset_y],
                    native_dark_pixel_percent=native_dark,
                    reference_dark_pixel_percent=reference_dark,
                )
        else:
            raise ValueError(f"unknown verification source: {check['source']}")
        result["credit"] = round(weight * score / 100.0, 4)
        earned += result["credit"]
        group = group_totals.setdefault(check["group"], {"earned_points": 0.0,
                                                         "possible_points": 0.0})
        group["earned_points"] += result["credit"]
        group["possible_points"] += weight
        results.append(result)

    group_scores = {}
    for group_name, totals in sorted(group_totals.items()):
        group_earned = round(totals["earned_points"], 4)
        group_possible = round(totals["possible_points"], 4)
        group_scores[group_name] = {
            "earned_points": group_earned,
            "possible_points": group_possible,
            "percent": round(group_earned * 100.0 / group_possible, 2),
        }

    report = {
        "schema_version": 1,
        "feature_ids": manifest["feature_ids"],
        "method": "weighted behavioral checks and local-only normalized visual references",
        "earned_points": round(earned, 4),
        "possible_points": round(possible, 4),
        "fidelity_percent": round(earned * 100.0 / possible, 2) if possible else 0.0,
        "group_scores": group_scores,
        "behavior_self_test_passed": bool(args.behavior_pass),
        "missing_reference_checks": missing_references,
        "checks": results,
    }
    PUBLIC_REPORT.parent.mkdir(parents=True, exist_ok=True)
    PUBLIC_REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(
        f"View Rosters fidelity: {report['earned_points']:.2f}/{report['possible_points']:.2f} "
        f"weighted points = {report['fidelity_percent']:.2f}%"
    )
    for result in results:
        if "similarity_percent" in result:
            print(f"  {result['id']}: {result['similarity_percent']:.2f}% visual similarity")
    if missing_references:
        print("  missing local references: " + ", ".join(missing_references))
    if args.require_references and missing_references:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
