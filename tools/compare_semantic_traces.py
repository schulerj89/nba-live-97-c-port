#!/usr/bin/env python3
"""Compare an original PS1 PC trace with a native semantic checkpoint trace."""

from __future__ import annotations

import argparse
import difflib
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ORIGINAL_MANIFEST = (
    ROOT / "config" / "decomp" / "instruction_semantics" /
    "view_rosters_original.json"
)
ADDRESS = re.compile(r"(?:0x)?([0-9A-Fa-f]{8})")


def collapse(values):
    result = []
    for value in values:
        if not result or result[-1] != value:
            result.append(value)
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--original", required=True,
                        help="local no$psx/PS1 PC trace text file")
    parser.add_argument("--native", default=
                        ".local/reports/view_rosters_semantic_trace.json")
    parser.add_argument("--output", default=
                        ".local/reports/view_rosters_trace_comparison.json")
    args = parser.parse_args()

    manifest = json.loads(ORIGINAL_MANIFEST.read_text(encoding="utf-8"))
    ranges = sorted(
        (int(item["address"], 0),
         int(item["address"], 0) + int(item["size_bytes"]),
         item["address"])
        for item in manifest["functions"]
    )
    scope = {item[2] for item in ranges}
    original_path = ROOT / args.original
    native_path = ROOT / args.native
    original_pcs = [int(match.group(1), 16)
                    for match in ADDRESS.finditer(original_path.read_text(
                        encoding="utf-8", errors="ignore"))]
    original_functions = []
    for pc in original_pcs:
        for start, end, address in ranges:
            if start <= pc < end:
                original_functions.append(address)
                break
    native = json.loads(native_path.read_text(encoding="utf-8"))
    native_functions = [value for value in native.get("sequence", []) if value in scope]
    original_sequence = collapse(original_functions)
    native_sequence = collapse(native_functions)
    matcher = difflib.SequenceMatcher(a=original_sequence, b=native_sequence,
                                      autojunk=False)
    matching = sum(block.size for block in matcher.get_matching_blocks())
    exact = original_sequence == native_sequence and bool(original_sequence)
    first_mismatch = None
    for index, (left, right) in enumerate(zip(original_sequence, native_sequence)):
        if left != right:
            first_mismatch = {"index": index, "original": left, "native": right}
            break
    if first_mismatch is None and len(original_sequence) != len(native_sequence):
        first_mismatch = {"index": min(len(original_sequence), len(native_sequence)),
                          "original": None, "native": None}
    result = {
        "schema_version": 1,
        "scope": "view_rosters",
        "method": "function-entry sequence derived from original PCs versus native semantic checkpoints",
        "original_pc_events_in_scope": len(original_functions),
        "original_function_transitions": len(original_sequence),
        "native_checkpoint_events_in_scope": len(native_functions),
        "native_function_transitions": len(native_sequence),
        "matching_transitions": matching,
        "sequence_similarity_percent": round(matcher.ratio() * 100.0, 2),
        "exact_sequence_match": exact,
        "first_mismatch": first_mismatch,
    }
    output = ROOT / args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(
        f"Semantic trace comparison: {matching} aligned transitions, "
        f"{result['sequence_similarity_percent']:.2f}% sequence similarity, "
        f"exact={'yes' if exact else 'no'}"
    )
    return 0 if original_sequence and native_sequence else 1


if __name__ == "__main__":
    raise SystemExit(main())
