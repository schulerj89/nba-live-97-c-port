#!/usr/bin/env python3
"""Validate recovered C ownership and optionally compare PS1 function bytes."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "config" / "decomp"


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def normalize_address(value: str) -> str:
    return f"0x{int(value, 0):08X}"


def load_inventory(path: Path):
    with path.open(newline="", encoding="utf-8-sig") as source:
        return {
            normalize_address(row["StartAddress"]): {
                "name": row["Name"],
                "size": int(row["Size"], 0),
            }
            for row in csv.DictReader(source)
        }


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def compare_words(original: bytes, candidate: bytes):
    words = min(len(original), len(candidate)) // 4
    equal = sum(
        original[index * 4 : index * 4 + 4] == candidate[index * 4 : index * 4 + 4]
        for index in range(words)
    )
    return equal, words


def verify(require_matching: bool):
    project = read_json(CONFIG / "project.json")
    recovery = read_json(CONFIG / "c_recovery.json")
    evidence = read_json(CONFIG / "recovered_functions.json")["functions"]
    evidence_index = {
        (item["binary"], normalize_address(item["address"])): item for item in evidence
    }
    binaries = {item["id"]: item for item in project["binaries"]}
    inventories = {
        binary_id: load_inventory(ROOT / item["inventory"])
        for binary_id, item in binaries.items()
    }
    candidate_root = ROOT / recovery["candidate_directory"]
    seen = set()
    results = []

    for item in recovery["functions"]:
        binary_id = item["binary"]
        address = normalize_address(item["address"])
        key = (binary_id, address)
        if key in seen:
            raise ValueError(f"duplicate C recovery record: {binary_id} {address}")
        seen.add(key)
        if binary_id not in binaries:
            raise ValueError(f"unknown binary in C recovery record: {binary_id}")
        inventory = inventories[binary_id].get(address)
        if inventory is None:
            raise ValueError(f"C recovery address absent from inventory: {binary_id} {address}")
        evidence_item = evidence_index.get(key)
        if evidence_item is None:
            raise ValueError(f"C recovery address has no evidence record: {binary_id} {address}")
        source_path = ROOT / item["source"]
        if not source_path.is_file():
            raise ValueError(f"missing recovered C source: {item['source']}")
        if item["symbol"] not in source_path.read_text(encoding="utf-8"):
            raise ValueError(f"missing C symbol {item['symbol']} in {item['source']}")
        if item["source"] not in evidence_item.get("evidence", []):
            raise ValueError(f"C source is not cited by evidence record: {binary_id} {address}")

        result = {
            "binary": binary_id,
            "address": address,
            "function_size": inventory["size"],
            "symbol": item["symbol"],
            "source": item["source"],
            "scope": item["scope"],
            "match_eligible": bool(item["match_eligible"]),
            "native_test": item["native_test"],
        }
        if not item["match_eligible"]:
            result["match_status"] = "fragment_behavior_only"
            results.append(result)
            continue

        binary = binaries[binary_id]
        original_path = ROOT / binary["local_path"]
        candidate_path = candidate_root / f"{binary_id}_{address[2:]}.bin"
        result["candidate_path"] = str(candidate_path.relative_to(ROOT)).replace("\\", "/")
        if not original_path.is_file():
            result["match_status"] = "original_missing"
        elif not candidate_path.is_file():
            if original_path.stat().st_size != binary["file_size"] or \
                    sha256(original_path).lower() != binary["sha256"].lower():
                raise ValueError(f"private original failed size/hash validation: {binary_id}")
            result["match_status"] = "candidate_missing"
        else:
            if original_path.stat().st_size != binary["file_size"] or \
                    sha256(original_path).lower() != binary["sha256"].lower():
                raise ValueError(f"private original failed size/hash validation: {binary_id}")
            original_data = original_path.read_bytes()
            candidate = candidate_path.read_bytes()
            start = (
                int(binary.get("image_file_offset", 0))
                + int(address, 0)
                - int(binary["load_address"], 0)
            )
            end = start + inventory["size"]
            if start < 0 or end > len(original_data):
                raise ValueError(f"original function range is outside {binary_id}: {address}")
            original = original_data[start:end]
            equal_bytes = sum(a == b for a, b in zip(original, candidate))
            equal_words, compared_words = compare_words(original, candidate)
            exact = candidate == original
            result.update(
                match_status="matching" if exact else "different",
                candidate_size=len(candidate),
                equal_bytes=equal_bytes,
                byte_similarity_percent=round(equal_bytes * 100.0 / max(len(original), len(candidate)), 2),
                equal_instruction_words=equal_words,
                compared_instruction_words=compared_words,
            )
        results.append(result)

    eligible = [item for item in results if item["match_eligible"]]
    matching = [item for item in eligible if item["match_status"] == "matching"]
    if require_matching and (not eligible or len(matching) != len(eligible)):
        raise ValueError(
            f"exact matching required, but {len(matching)}/{len(eligible)} eligible C functions match"
        )
    return {
        "schema_version": 1,
        "c_recovery_records": len(results),
        "behavior_only_fragments": sum(not item["match_eligible"] for item in results),
        "match_eligible_functions": len(eligible),
        "matching_functions": len(matching),
        "functions": results,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--require-matching", action="store_true")
    parser.add_argument(
        "--output", default=".local/reports/recovery_verification.json",
        help="local-only JSON result path",
    )
    args = parser.parse_args()
    try:
        result = verify(args.require_matching)
        output = ROOT / args.output
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
        print(
            "C recovery validated: "
            f"{result['c_recovery_records']} records, "
            f"{result['behavior_only_fragments']} behavior-only fragments, "
            f"{result['matching_functions']}/{result['match_eligible_functions']} exact matches"
        )
        print(f"Local result: {output.relative_to(ROOT)}")
        return 0
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"recovery verification failed: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
