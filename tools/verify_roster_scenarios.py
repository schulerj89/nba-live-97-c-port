#!/usr/bin/env python3
"""Verify native roster state traces and optional original no$psx PC traces."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / "config" / "decomp" / "view_rosters_scenarios.json"
ORIGINAL_MANIFEST = (
    ROOT / "config" / "decomp" / "instruction_semantics" /
    "view_rosters_original.json"
)
SEMANTIC_MAPPING = (
    ROOT / "config" / "decomp" / "instruction_semantics" /
    "view_rosters_mapping.json"
)
DEFAULT_NATIVE = ROOT / ".local" / "reports" / "view_rosters_scenario_trace.json"
DEFAULT_OUTPUT = ROOT / ".local" / "reports" / "view_rosters_scenario_comparison.json"
ADDRESS = re.compile(r"(?<![0-9A-Fa-f])(?:0x)?([0-9A-Fa-f]{8})(?![0-9A-Fa-f])")
BREAKPOINT_ENTRY = re.compile(
    r"^\s*PC\s*=\s*(?:0x)?([0-9A-Fa-f]{8})(?:\s|$)", re.IGNORECASE)


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def normalize(value: str) -> str:
    return f"0x{int(value, 0):08X}"


def collapse(values):
    result = []
    for value in values:
        if not result or result[-1] != value:
            result.append(value)
    return result


def is_subsequence(required, observed) -> bool:
    position = 0
    for value in observed:
        if position < len(required) and value == required[position]:
            position += 1
    return position == len(required)


def validate_contract(contract, original, mapping):
    if contract.get("schema_version") != 1 or contract.get("scope") != "view_rosters":
        raise ValueError("unsupported roster scenario contract")
    known = {normalize(item["address"]) for item in original["functions"]}
    scenarios = contract.get("scenarios", [])
    ids = [item.get("id") for item in scenarios]
    if not scenarios or len(ids) != len(set(ids)) or any(not value for value in ids):
        raise ValueError("scenario IDs must be non-empty and unique")
    mapped_ids = {item["id"] for item in mapping.get("trace_scenarios", [])}
    if set(ids) != mapped_ids:
        raise ValueError("scenario contract and semantic mapping IDs differ")
    for scenario in scenarios:
        event_names = [item.get("name") for item in scenario.get("events", [])]
        if not event_names or len(event_names) != len(set(event_names)):
            raise ValueError(f"{scenario['id']}: event names must be non-empty and unique")
        for event in scenario["events"]:
            required = {
                "name", "changed", "mode", "team", "player",
                "first_visible", "first_stat", "category", "display",
            }
            if set(event) != required:
                raise ValueError(f"{scenario['id']}/{event.get('name')}: state fields differ")
            if event["mode"] not in {"team_roster", "player_card"}:
                raise ValueError(f"{scenario['id']}/{event['name']}: invalid mode")
        for field in ("native_collapsed_sequence", "original_required_subsequence"):
            sequence = [normalize(value) for value in scenario.get(field, [])]
            if not sequence or not set(sequence) <= known:
                raise ValueError(f"{scenario['id']}: invalid {field}")


def parse_original_trace(path: Path, ranges):
    text = path.read_text(encoding="utf-8", errors="ignore")
    starts = {start: address for start, _end, address in ranges}
    breakpoint_entries = []
    has_breakpoint_records = False
    for line in text.splitlines():
        match = BREAKPOINT_ENTRY.match(line)
        if not match:
            continue
        has_breakpoint_records = True
        pc = int(match.group(1), 16)
        if pc in starts:
            breakpoint_entries.append(starts[pc])
    # Each PC= line is one discrete debugger stop. Preserve repeated calls to
    # the same entrypoint (for example previous then next player both entering
    # FUN_80059928). Raw per-instruction logs still collapse contiguous PCs
    # belonging to the same recovered function below.
    if has_breakpoint_records:
        return breakpoint_entries

    functions = []
    for match in ADDRESS.finditer(text):
        pc = int(match.group(1), 16)
        for start, end, address in ranges:
            if start <= pc < end:
                functions.append(address)
                break
    return collapse(functions)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--native", default=str(DEFAULT_NATIVE.relative_to(ROOT)))
    parser.add_argument("--original", action="append", default=[], metavar="ID=PATH",
                        help="local no$psx PC trace for one declared scenario")
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT.relative_to(ROOT)))
    parser.add_argument("--require-native", action="store_true")
    parser.add_argument("--require-original", action="store_true")
    parser.add_argument("--check-config", action="store_true")
    args = parser.parse_args()
    try:
        contract = read_json(CONTRACT)
        original = read_json(ORIGINAL_MANIFEST)
        mapping = read_json(SEMANTIC_MAPPING)
        validate_contract(contract, original, mapping)
        if args.check_config:
            print(f"Roster scenario contract valid: {len(contract['scenarios'])} scenarios")
            return 0

        declared = {item["id"]: item for item in contract["scenarios"]}
        supplied = {}
        for value in args.original:
            if "=" not in value:
                raise ValueError("--original must use ID=PATH")
            scenario_id, path = value.split("=", 1)
            if scenario_id not in declared or scenario_id in supplied:
                raise ValueError(f"unknown or duplicate original scenario: {scenario_id}")
            supplied[scenario_id] = ROOT / path
        if args.require_original and set(supplied) != set(declared):
            missing = sorted(set(declared) - set(supplied))
            raise ValueError(f"missing original scenario traces: {', '.join(missing)}")

        ranges = sorted(
            (int(item["address"], 0),
             int(item["address"], 0) + int(item["size_bytes"]),
             normalize(item["address"]))
            for item in original["functions"]
        )
        native_path = ROOT / args.native
        if args.require_native and not native_path.is_file():
            raise ValueError(f"missing native scenario trace: {native_path.relative_to(ROOT)}")
        native_scenarios = {}
        if native_path.is_file():
            native_report = read_json(native_path)
            if (native_report.get("schema_version") != 1 or
                    native_report.get("scope") != "view_rosters"):
                raise ValueError("unsupported native roster scenario trace")
            native_scenarios = {item["id"]: item for item in native_report["scenarios"]}
            if set(native_scenarios) != set(declared):
                raise ValueError("native scenario set differs from contract")

        results = []
        all_native = True
        all_original = bool(supplied)
        for scenario_id, expected in declared.items():
            native_status = "not_run"
            if scenario_id in native_scenarios:
                observed = native_scenarios[scenario_id]
                expected_events = expected["events"]
                if observed.get("events") != expected_events:
                    raise ValueError(f"{scenario_id}: native state-event trace differs")
                expected_sequence = [normalize(value)
                                     for value in expected["native_collapsed_sequence"]]
                observed_sequence = collapse(
                    [normalize(value) for value in observed.get("function_sequence", [])])
                if observed_sequence != expected_sequence:
                    raise ValueError(
                        f"{scenario_id}: native collapsed function sequence differs\n"
                        f"expected={expected_sequence}\nobserved={observed_sequence}")
                native_status = "verified"
            else:
                all_native = False

            original_status = "not_supplied"
            original_sequence = []
            original_sha256 = None
            if scenario_id in supplied:
                if not supplied[scenario_id].is_file():
                    raise ValueError(f"missing original trace: {supplied[scenario_id]}")
                original_sequence = parse_original_trace(supplied[scenario_id], ranges)
                required = [normalize(value)
                            for value in expected["original_required_subsequence"]]
                if not is_subsequence(required, original_sequence):
                    raise ValueError(
                        f"{scenario_id}: original trace lacks required subsequence\n"
                        f"required={required}\nobserved={original_sequence}")
                original_status = "verified_subsequence"
                original_sha256 = hashlib.sha256(
                    supplied[scenario_id].read_bytes()).hexdigest()
            elif args.require_original:
                all_original = False
            results.append({
                "id": scenario_id,
                "native_status": native_status,
                "original_status": original_status,
                "original_sha256": original_sha256,
                "original_collapsed_sequence": original_sequence,
            })

        result = {
            "schema_version": 1,
            "scope": "view_rosters",
            "native_contract_verified": all_native,
            "original_scenarios_verified": all_original and len(supplied) == len(declared),
            "scenarios": results,
        }
        output = ROOT / args.output
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, indent=2) + "\n",
                          encoding="utf-8", newline="\n")
        print(
            f"Roster scenarios: native={'verified' if all_native else 'not run'}; "
            f"original={len(supplied)}/{len(declared)} traces verified")
        return 0
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"roster scenario verification failed: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
