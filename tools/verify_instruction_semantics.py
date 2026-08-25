#!/usr/bin/env python3
"""Validate tiered instruction-semantic accounting without claiming false matches."""

from __future__ import annotations

import argparse
import csv
import json
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "config" / "decomp"
ORIGINAL = CONFIG / "instruction_semantics" / "view_rosters_original.json"
MAPPING = CONFIG / "instruction_semantics" / "view_rosters_mapping.json"
NATIVE = ROOT / ".local" / "reports" / "view_rosters_semantic_trace.json"
REPORT = ROOT / "reports" / "instruction_semantics.json"
RECOMP = ROOT / ".local" / "recomp" / "recompiled_full.cpp"


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def normalize_address(value: str) -> str:
    return f"0x{int(value, 0):08X}"


def load_inventory():
    path = CONFIG / "functions" / "feonly.csv"
    with path.open(newline="", encoding="utf-8-sig") as source:
        return {
            normalize_address(row["StartAddress"]): int(row["Size"], 0)
            for row in csv.DictReader(source)
        }


def calculate(require_native: bool):
    original = read_json(ORIGINAL)
    mapping = read_json(MAPPING)
    inventory = load_inventory()
    originals = {normalize_address(item["address"]): item for item in original["functions"]}
    mappings = {normalize_address(item["address"]): item for item in mapping["functions"]}
    if len(originals) != len(original["functions"]) or len(mappings) != len(mapping["functions"]):
        raise ValueError("duplicate instruction-semantic function address")
    if set(originals) != set(mappings):
        raise ValueError("original and source-mapping function scopes differ")

    native_functions = {}
    native_available = NATIVE.is_file()
    if require_native and not native_available:
        raise ValueError(f"missing native semantic report: {NATIVE.relative_to(ROOT)}")
    if native_available:
        native = read_json(NATIVE)
        native_functions = {
            normalize_address(item["address"]): item for item in native["functions"]
        }
        if int(native.get("dropped_events", 0)) != 0:
            raise ValueError("native semantic trace dropped events")
        native_sequence = [normalize_address(value) for value in native.get("sequence", [])]
        if len(native_sequence) != int(native.get("captured_sequence_events", -1)):
            raise ValueError("native semantic sequence length is inconsistent")
        native_counts = Counter(native_sequence)
        for address, item in native_functions.items():
            if native_counts[address] != int(item["native_event_count"]):
                raise ValueError(f"native semantic count disagrees with sequence: {address}")
    recomp_available = RECOMP.is_file()
    recomp_text = RECOMP.read_text(encoding="utf-8", errors="ignore") if recomp_available else ""

    total_instructions = total_blocks = total_edges = total_bytes = 0
    accounted_instructions = accounted_blocks = verified_edges = 0
    mapped_instructions = 0
    checkpoint_observed = 0
    recomp_entrypoints_found = 0
    missing_recomp_entrypoints = []
    function_results = []
    for address in sorted(originals, key=lambda value: int(value, 0)):
        source = originals[address]
        owner = mappings[address]
        if address not in inventory or inventory[address] != int(source["size_bytes"]):
            raise ValueError(f"Ghidra semantic size disagrees with inventory: {address}")
        if int(source["instruction_count"]) * 4 != int(source["size_bytes"]):
            raise ValueError(f"non-word instruction accounting in PS1 function: {address}")
        source_path = ROOT / owner["source"]
        if not source_path.is_file():
            raise ValueError(f"missing source owner: {owner['source']}")
        source_text = source_path.read_text(encoding="utf-8")
        for symbol in owner["symbols"]:
            if symbol not in source_text:
                raise ValueError(f"missing mapped symbol {symbol} in {owner['source']}")

        blocks = {normalize_address(item["start"]): item for item in source["blocks"]}
        listed_blocks = [normalize_address(item) for item in owner["accounted_blocks"]]
        if len(listed_blocks) != len(set(listed_blocks)) or not set(listed_blocks) <= set(blocks):
            raise ValueError(f"invalid accounted block list: {address}")
        if listed_blocks and not owner.get("accounting_basis"):
            raise ValueError(f"accounted blocks lack a written basis: {address}")
        edge_keys = {
            (normalize_address(item["from"]), normalize_address(item["to"]), item["type"])
            for item in source["edges"]
        }
        listed_edges = {
            (normalize_address(item["from"]), normalize_address(item["to"]), item["type"])
            for item in owner["structurally_verified_edges"]
        }
        if not listed_edges <= edge_keys:
            raise ValueError(f"invalid structurally verified edge: {address}")

        function_instructions = int(source["instruction_count"])
        function_blocks = int(source["basic_block_count"])
        function_edges = int(source["control_flow_edge_count"])
        block_instruction_credit = sum(int(blocks[item]["instruction_count"])
                                       for item in listed_blocks)
        native_item = native_functions.get(normalize_address(owner["native_checkpoint"]))
        observed = bool(native_item and int(native_item["native_event_count"]) > 0)
        checkpoint_observed += int(observed)
        recomp_symbol = f"overlay_FEONLY__{address[2:]}("
        recomp_found = recomp_available and recomp_symbol in recomp_text
        recomp_entrypoints_found += int(recomp_found)
        if recomp_available and not recomp_found:
            missing_recomp_entrypoints.append(address)
        total_instructions += function_instructions
        total_blocks += function_blocks
        total_edges += function_edges
        total_bytes += int(source["size_bytes"])
        mapped_instructions += function_instructions
        accounted_instructions += block_instruction_credit
        accounted_blocks += len(listed_blocks)
        verified_edges += len(listed_edges)
        function_results.append({
            "address": address,
            "name": owner["name"],
            "source": owner["source"],
            "source_mapped": True,
            "original_instruction_count": function_instructions,
            "original_basic_block_count": function_blocks,
            "accounted_instruction_count": block_instruction_credit,
            "accounted_basic_block_count": len(listed_blocks),
            "structurally_verified_edge_count": len(listed_edges),
            "native_checkpoint_observed": observed,
            "recomp_entrypoint_found": recomp_found,
            "original_trace_equivalent": False,
            "binary_matching": False,
        })

    report = {
        "schema_version": 1,
        "scope": mapping["scope"],
        "method": "tiered original-MIPS structural accounting; tiers are never combined",
        "original_scope": {
            "functions": len(originals),
            "bytes": total_bytes,
            "instructions": total_instructions,
            "basic_blocks": total_blocks,
            "control_flow_edges": total_edges,
            "direct_call_sites": sum(int(item["direct_call_site_count"])
                                     for item in originals.values()),
        },
        "source_ownership": {
            "mapped_functions": len(mappings),
            "total_functions": len(originals),
            "instructions_with_source_owner": mapped_instructions,
            "total_instructions": total_instructions,
        },
        "instruction_accounting": {
            "accounted_instructions": accounted_instructions,
            "total_instructions": total_instructions,
            "accounted_basic_blocks": accounted_blocks,
            "total_basic_blocks": total_blocks,
        },
        "structural_verification": {
            "verified_control_flow_edges": verified_edges,
            "total_control_flow_edges": total_edges,
        },
        "native_checkpoint_observation": {
            "observed_functions": checkpoint_observed,
            "total_functions": len(originals),
            "native_report_available": native_available,
        },
        "recomp_crosscheck": {
            "entrypoints_found": recomp_entrypoints_found,
            "total_functions": len(originals),
            "missing_entrypoints": missing_recomp_entrypoints,
            "local_recomp_available": recomp_available,
        },
        "original_trace_comparison": {
            "equivalent_scenarios": 0,
            "declared_scenarios": len(mapping.get("trace_scenarios", [])),
            "status": "not_established",
        },
        "binary_matching": {
            "matching_functions": 0,
            "eligible_functions": 0,
            "status": "not_configured",
        },
        "functions": function_results,
    }
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--require-native", action="store_true")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        native_was_local = NATIVE.is_file()
        recomp_was_local = RECOMP.is_file()
        result = calculate(args.require_native)
        if args.check:
            if not REPORT.is_file():
                raise ValueError(f"missing generated report: {REPORT.relative_to(ROOT)}")
            committed = read_json(REPORT)
            # CI intentionally has no private runtime report. Preserve only the
            # committed native-observation fields while recalculating every
            # static instruction/block/CFG count from public manifests.
            if not NATIVE.is_file():
                result["native_checkpoint_observation"] = \
                    committed["native_checkpoint_observation"]
                committed_functions = {
                    item["address"]: item for item in committed["functions"]
                }
                for item in result["functions"]:
                    item["native_checkpoint_observed"] = \
                        committed_functions[item["address"]]["native_checkpoint_observed"]
            if not RECOMP.is_file():
                result["recomp_crosscheck"] = committed["recomp_crosscheck"]
                committed_functions = {
                    item["address"]: item for item in committed["functions"]
                }
                for item in result["functions"]:
                    item["recomp_entrypoint_found"] = \
                        committed_functions[item["address"]]["recomp_entrypoint_found"]
            if committed != result:
                raise ValueError(f"out-of-date generated report: {REPORT.relative_to(ROOT)}")
            action = "validated"
        else:
            REPORT.parent.mkdir(parents=True, exist_ok=True)
            REPORT.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
            action = "generated"
        scope = result["original_scope"]
        accounting = result["instruction_accounting"]
        checkpoints = result["native_checkpoint_observation"]
        print(
            f"Instruction semantics {action}: {scope['instructions']} instructions, "
            f"{accounting['accounted_instructions']} explicitly accounted, "
            f"{checkpoints['observed_functions']}/{checkpoints['total_functions']} "
            "native checkpoints observed; original trace equivalence not claimed"
        )
        if args.check and (not native_was_local or not recomp_was_local):
            print("  asset-free check: static tiers recalculated; committed local runtime/recomp observations retained")
        return 0
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"instruction-semantic verification failed: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
