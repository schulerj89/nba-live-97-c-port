#!/usr/bin/env python3
"""Small, conservative Re-order ledger plus fresh asset-free/local test runner."""
from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "config/decomp/reorder_rosters.json"
REPORT = ROOT / "reports/reorder_rosters.json"
DOC = ROOT / "docs/reorder_rosters_progress.md"


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def encoded(value):
    return json.dumps(value, indent=2) + "\n"


def calculate(config=None, inventory=None):
    config = config if config is not None else read(CONFIG)
    inventory = inventory if inventory is not None else read(ROOT / config["inventory"])
    with (ROOT / "config/decomp/functions/feonly.csv").open(encoding="utf-8-sig", newline="") as source:
        sizes = {row["StartAddress"]: int(row["Size"], 0) for row in csv.DictReader(source)}
    owners = {item["address"]: item for item in config["functions"]}
    originals = {item["address"]: item for item in inventory["functions"]}
    if len(owners) != len(config["functions"]) or len(originals) != len(inventory["functions"]):
        raise ValueError("duplicate function address")
    if set(owners) != set(originals):
        raise ValueError("function scope differs from measured inventory")
    tests = config["core_tests"] + config["local_tests"]
    if len(tests) != len(set(tests)):
        raise ValueError("duplicate scenario ID")
    all_contracts = (config.get('selection_contracts', []) + config.get('screen_contracts', []) +
                     config.get('shared_contracts', []))
    contracts = {item['address']: item for item in all_contracts}
    if len(contracts) != len(all_contracts):
        raise ValueError('duplicate selection contract')
    for contract in contracts.values():
        if contract.get('status') != 'native_contract_tested' or not contract.get('scope') or not contract.get('integration_pending'):
            raise ValueError('selection contract needs tested scope and integration boundary')
        if not contract.get('tests') or not set(contract['tests']) <= set(tests):
            raise ValueError('selection contract needs declared tests')
        for source in contract['sources']:
            if source['symbol'] not in (ROOT / source['path']).read_text(encoding='utf-8'):
                raise ValueError('missing selection contract source')
    slices = {item["id"]: item for item in config["slices"]}
    if len(slices) != len(config["slices"]):
        raise ValueError("duplicate slice ID")
    for item in slices.values():
        if item["status"] not in {"pending", "native_tested", "reference_verified"} or not item["gate"]:
            raise ValueError("invalid slice gate/status")
    rows = []
    dependencies = set()
    for address, owner in owners.items():
        original = originals[address]
        if owner["slice"] not in slices:
            raise ValueError("unknown function slice")
        if original["size_bytes"] != sizes.get(address) or original["instruction_count"] * 4 != original["size_bytes"]:
            raise ValueError("original instruction denominator disagrees with Ghidra function inventory")
        blocks = {block["start"]: block for block in original["blocks"]}
        if len(blocks) != len(original["blocks"]):
            raise ValueError("duplicate original block")
        occupied = set()
        for block in blocks.values():
            start, end = int(block["start"], 0), int(block["end"], 0)
            words = set(range(start, end + 1, 4))
            if start % 4 or end % 4 != 3 or len(words) != block["instruction_count"] or words & occupied:
                raise ValueError("overlapping or inconsistent original blocks")
            if start < int(address, 0) or end >= int(address, 0) + original["size_bytes"]:
                raise ValueError("block outside original function")
            occupied.update(words)
        if len(occupied) != original["instruction_count"]:
            raise ValueError("blocks do not cover original instruction denominator")
        accounted = owner["accounted_blocks"]
        if len(accounted) != len(set(accounted)) or not set(accounted) <= blocks.keys():
            raise ValueError("invalid or duplicate accounted block")
        if accounted:
            if not owner.get("basis") or not owner.get("tests") or not set(owner["tests"]) <= set(tests):
                raise ValueError("instruction credit needs basis and declared tests")
            if owner["symbol"] not in (ROOT / owner["source"]).read_text(encoding="utf-8"):
                raise ValueError("missing native source owner")
        pending = [b for b in original["blocks"] if b["start"] not in accounted]
        if pending and not owner.get("pending"):
            raise ValueError("unaccounted instructions need a pending-work explanation")
        count = sum(blocks[b]["instruction_count"] for b in accounted)
        if address in {'0x800556B0', '0x800558E0', '0x80055AF8'} and accounted:
            evidence = owner.get('block_evidence', {})
            if set(evidence) != set(accounted):
                raise ValueError('shared owner needs exact per-block evidence coverage')
            for item in evidence.values():
                if not item.get('behavior') or not item.get('tests') or not set(item['tests']) <= set(owner['tests']):
                    raise ValueError('block evidence needs behavior and owner-declared tests')
        if not pending:
            for callee in original['direct_calls']:
                target = owners.get(callee)
                fully_owned = target and len(target['accounted_blocks']) == len(originals[callee]['blocks'])
                if not fully_owned and callee not in contracts:
                    raise ValueError(f'complete owner has unresolved call contract: {callee}')
        rows.append({"address": address, "name": owner["name"], "slice": owner["slice"],
                     "accounted": count, "total": original["instruction_count"],
                     "pending": original["instruction_count"] - count,
                     "pending_blocks": pending, "note": owner.get("pending", owner.get("basis")),
                     "block_evidence": owner.get("block_evidence", {})})
        dependencies.update(set(original["direct_calls"]) - owners.keys())
    total = sum(row["total"] for row in rows)
    accounted = sum(row["accounted"] for row in rows)
    next_slice = next((item["id"] for item in config["slices"] if item["status"] == "pending"), None)
    slice_counts = {}
    for row in rows:
        count = slice_counts.setdefault(row["slice"], {"accounted": 0, "total": 0, "pending": 0})
        for key in count:
            count[key] += row[key]
    return {"schema_version": 1, "scope": config["scope"],
            "measurement": "reviewed source accounting, not runtime execution or binary equivalence",
            "instructions": {"accounted": accounted, "total": total, "pending": total - accounted,
                             "percent": round(100 * accounted / total, 2) if total else 0},
            "fully_accounted_functions": sum(row["pending"] == 0 for row in rows),
            "function_count": len(rows), "functions": rows,
            "instruction_slices": slice_counts,
            "slices": config["slices"], "core_scenarios_defined": len(config["core_tests"]),
            "local_scenarios_defined": len(config["local_tests"]),
            "tests_executed_by_static_report": False,
            "next_slice": next_slice,
            "direct_dependencies_outside_inventory": sorted(dependencies),
            "feature_acceptance": "pending" if next_slice else "all declared gates reviewed; rerun acceptance evidence",
            "dependency_policy": config["dependency_policy"],
            "selection_contracts": config.get('selection_contracts', []),
            "screen_contracts": config.get('screen_contracts', []),
            "shared_contracts": config.get('shared_contracts', [])}


def markdown(report):
    counts = report["instructions"]
    lines = ["# Re-order Rosters: small-slice progress", "",
             "Generated by `python tools/verify_reorder_rosters.py`. Do not hand-edit.", "",
             f"Reviewed instruction accounting: **{counts['accounted']} / {counts['total']} ({counts['percent']:.2f}%)**; **{counts['pending']} pending**.", "",
             f"Next slice: **{report['next_slice'] or 'fresh acceptance review'}**. Feature acceptance: **{report['feature_acceptance']}**.", "",
             "This percentage covers only the initial ten-function inventory, not the whole feature or game. "
             "It is reviewed source accounting, not instruction execution, visual fidelity, or a binary match. "
             "Working fragments receive no whole-function credit. Native-tested means tests passed during the recorded implementation review; rerun them for current evidence.", "",
             "| Slice | Status | Exit gate |", "|---|---|---|"]
    lines += [f"| {i+1}. {s['name']} | {s['status']} | {s['gate']} |" for i, s in enumerate(report["slices"])]
    lines += ["", "## Instruction counts by area", "", "| Area | Accounted | Pending |", "|---|---:|---:|"]
    lines += [f"| {name} | {c['accounted']} / {c['total']} | {c['pending']} |" for name, c in report['instruction_slices'].items()]
    lines += ["", "## Original instructions", "", "| Function | Accounted | Pending |", "|---|---:|---:|"]
    lines += [f"| `{r['address']}` {r['name']} | {r['accounted']} / {r['total']} | {r['pending']} |" for r in report["functions"]]
    lines += ["", "## Selection call contracts (not full callee coverage)", "",
              "Completing selection-owner blocks means their native call contracts are represented and tested. "
              "It does not transfer credit to shared callees or prove the game screen works. "
              "The 166-instruction selection denominator excludes these callees' own instructions.", "",
              "| Original helper | Tested native contract | Integration still pending |", "|---|---|---|"]
    lines += [f"| `{c['address']}` | {c['scope']} | {c['integration_pending']} |" for c in report['selection_contracts']]
    lines += ["", "## Construction call contracts (not full callee coverage)", "",
              "The 298 entry/construction instructions cover the Re-order specialization. "
              "Graphics state0x0C and input layout0x0D are deliberately separate. "
              "The original shared-engine bodies are not credited by this owner.", "",
              "| Original helper | Tested native contract | Still outside the claim |", "|---|---|---|"]
    lines += [f"| `{c['address']}` | {c['scope']} | {c['integration_pending']} |" for c in report['screen_contracts']]
    lines += ["", "## Shared-helper call contracts (not full callee coverage)", "",
              "Each newly accounted shared block has a behavior note and named tests in the JSON report. "
              "These are reviewed mappings, not automatically proven MIPS equivalence. "
              "Cross-mode helper tests do not implement other transaction screens.", "",
              "| Original helper | Tested native contract | Still outside the claim |", "|---|---|---|"]
    lines += [f"| `{c['address']}` | {c['scope']} | {c['integration_pending']} |" for c in report['shared_contracts']]
    lines += ["", "## Next work / pending blocks", ""]
    for row in report["functions"]:
        if row["pending"]:
            blocks = ", ".join(f"`{b['start']}` ({b['instruction_count']})" for b in row["pending_blocks"])
            lines += [f"### {row['name']}", "", row["note"], "", f"Pending block starts (instruction counts): {blocks}.", ""]
    lines += ["## Run fresh verification", "", "```powershell",
              "pwsh -File scripts/verify_reorder_rosters.ps1", "python tools/verify_reorder_rosters.py --check", "```", "",
              f"{report['core_scenarios_defined']} asset-free scenarios and {report['local_scenarios_defined']} optional local-database scenario are defined. "
              "The local scenario checks every ordered slot pair on all 29 teams (6,525 cases). "
              "The static ledger does not assert these tests just ran. The wrapper builds, runs fresh tests, "
              "and writes ignored CLI/evidence logs under `.local/`.", "",
              "## Scope boundary", "", report["dependency_policy"], "",
              "Direct callees outside the fixed instruction inventory (scoped contracts above are not full body coverage; data callbacks also need audit):", "",
              ", ".join(f"`{address}`" for address in report["direct_dependencies_outside_inventory"]), "",
              "The Re-order menu card now opens the original-asset two-list screen. Selection/swaps, team scans, "
              "accept and full-snapshot discard are connected. Help, View/Compare child routes, exact audio/transition "
              "timing, disk saving/Reset and original-reference acceptance remain pending. "
              "See [the workflow](reorder_rosters_workflow.md) before promoting a slice.", ""]
    return "\n".join(lines)


def run_tests(executable, database, config):
    args = [str(executable.resolve())]
    if database:
        args += ["--database", str(database.resolve())]
    output = ROOT / ".local/reports/reorder_rosters_run.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    evidence = {"schema_version": 1, "status": "running",
                "executed_at_utc": datetime.now(timezone.utc).isoformat(),
                "local_database_tested": bool(database), "passed": []}
    # Invalidate a previous green run before starting; an interrupted run must
    # never leave the old success looking like the current result.
    output.write_text(encoded(evidence), encoding="utf-8")
    try:
        result = subprocess.run(args, cwd=ROOT, capture_output=True, text=True)
    except OSError:
        evidence["status"] = "launch_failed"
        output.write_text(encoded(evidence), encoding="utf-8")
        raise
    print(result.stdout, end="")
    print(result.stderr, end="")
    seen = re.findall(r"^REORDER PASS ([a-z_]+) \|", result.stdout, re.MULTILINE)
    expected = config["core_tests"] + (config["local_tests"] if database else [])
    if result.returncode or len(seen) != len(set(seen)) or set(seen) != set(expected):
        evidence.update(status="failed", observed_scenario_ids=seen, exit_code=result.returncode)
        output.write_text(encoded(evidence), encoding="utf-8")
        raise ValueError("fresh native test failed or scenario evidence differs from contract")
    evidence.update(status="passed", passed=seen, source_sha256={})
    for relative in ["src/recovered/roster_reorder.c", "src/recovered/roster_reorder.h",
                     "src/recovered/roster_lists.c", "src/recovered/roster_lists.h", "tests/roster_lists_test.cpp",
                     "src/recovered/reorder_screen.c", "src/recovered/reorder_screen.h",
                     "src/roster_database.cpp", "src/roster_database.hpp", "tests/reorder_rosters_test.cpp",
                     "src/reorder_preview.cpp", "src/reorder_preview.hpp", "src/psh_font.cpp", "src/psh_font.hpp",
                     "tools/extract_reorder_dialogs.py", "config/decomp/reorder_rosters.json"]:
        evidence["source_sha256"][relative] = hashlib.sha256((ROOT / relative).read_bytes()).hexdigest()
    evidence["executable_sha256"] = hashlib.sha256(executable.read_bytes()).hexdigest()
    if database:
        font = database.resolve().parent.parent / "fonts/ZFONT0.PSH"
        evidence["asset_sha256"] = {
            "extracted/FEONLY.BIN": hashlib.sha256((database.resolve().parent.parent.parent / 'extracted/FEONLY.BIN').read_bytes()).hexdigest(),
            "database/roster.n97db": hashlib.sha256(database.read_bytes()).hexdigest(),
            "fonts/ZFONT0.PSH": hashlib.sha256(font.read_bytes()).hexdigest(),
            "fonts/ZFONT1.PSH": hashlib.sha256((font.parent / 'ZFONT1.PSH').read_bytes()).hexdigest(),
            "reorder/dialogs.n97ui": hashlib.sha256((font.parent.parent / 'reorder/dialogs.n97ui').read_bytes()).hexdigest(),
            "reorder/discard.n97ui": hashlib.sha256((font.parent.parent / 'reorder/discard.n97ui').read_bytes()).hexdigest(),
        }
    output.write_text(encoded(evidence), encoding="utf-8")
    print("REORDER EVIDENCE .local/reports/reorder_rosters_run.json (no original assets included)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--import-inventory", type=Path)
    parser.add_argument("--native-test", type=Path)
    parser.add_argument("--database", type=Path)
    args = parser.parse_args()
    config = read(CONFIG)
    if args.database and not args.native_test:
        parser.error("--database requires --native-test")
    if args.check and args.import_inventory:
        parser.error("--check cannot import inventory")
    if args.import_inventory:
        exported = read(args.import_inventory)
        metadata = {"schema_version": 1, "binary": "feonly", "functions": [
            {key: f[key] for key in ("address", "size_bytes", "instruction_count", "blocks", "direct_calls")}
            for f in exported["functions"]]}
        calculate(config, metadata)  # Validate before replacing generated metadata.
        (ROOT / config["inventory"]).write_text(encoded(metadata), encoding="utf-8")
    report = calculate(config)
    for path, content in [(REPORT, encoded(report)), (DOC, markdown(report))]:
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != content:
                raise ValueError(f"stale report: {path.relative_to(ROOT)}")
        else:
            path.write_text(content, encoding="utf-8")
    if args.native_test:
        run_tests(args.native_test, args.database, config)
    counts = report["instructions"]
    print(f"REORDER ACCOUNTING {counts['accounted']}/{counts['total']} ({counts['percent']:.2f}%); {counts['pending']} instructions pending")
    print("REORDER FEATURE pending; core accounting is not screen completion")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError) as error:
        raise SystemExit(f"REORDER VERIFY FAIL: {error}")
