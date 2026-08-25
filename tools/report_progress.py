#!/usr/bin/env python3
"""Generate reproducible decompilation and native-port progress reports."""

from __future__ import annotations

import argparse
import csv
import hashlib
import html
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "config" / "decomp"
REPORT_JSON = ROOT / "reports" / "progress.json"
REPORT_MD = ROOT / "docs" / "progress.md"
REPORT_HTML = ROOT / "docs" / "progress.html"
REPORT_SVG = ROOT / "docs" / "progress.svg"
VIEW_ROSTERS_MANIFEST = CONFIG / "view_rosters_verification.json"
VIEW_ROSTERS_REPORT = ROOT / "reports" / "view_rosters_fidelity.json"
INSTRUCTION_SEMANTICS_ORIGINAL = CONFIG / "instruction_semantics" / "view_rosters_original.json"
INSTRUCTION_SEMANTICS_MAPPING = CONFIG / "instruction_semantics" / "view_rosters_mapping.json"
INSTRUCTION_SEMANTICS_REPORT = ROOT / "reports" / "instruction_semantics.json"


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def parse_int(value: str) -> int:
    return int(value, 0)


def load_inventory(relative_path: str):
    path = ROOT / relative_path
    rows = []
    with path.open(newline="", encoding="utf-8-sig") as source:
        for row in csv.DictReader(source):
            rows.append(
                {
                    "name": row["Name"],
                    "address": row["StartAddress"].upper(),
                    "end_address": row["EndAddress"].upper(),
                    "size": parse_int(row["Size"]),
                }
            )
    return rows


def validate_private_input(binary: dict):
    path = ROOT / binary["local_path"]
    if not path.exists():
        return "private input (not distributed)"
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if len(data) != binary["file_size"]:
        raise ValueError(f"{path}: expected {binary['file_size']} bytes, got {len(data)}")
    if digest.lower() != binary["sha256"].lower():
        raise ValueError(f"{path}: SHA-256 does not match the canonical local input")
    # Keep generated reports reproducible on machines that intentionally lack
    # copyrighted inputs. Presence still enables strict local hash validation,
    # but it must not change committed output.
    return "private input (not distributed)"


def build_report():
    project = read_json(CONFIG / "project.json")
    recovered = read_json(CONFIG / "recovered_functions.json")["functions"]
    features = read_json(CONFIG / "features.json")["features"]
    fidelity_manifest = read_json(VIEW_ROSTERS_MANIFEST)
    fidelity_report = read_json(VIEW_ROSTERS_REPORT)
    semantic_original = read_json(INSTRUCTION_SEMANTICS_ORIGINAL)
    semantic_mapping = read_json(INSTRUCTION_SEMANTICS_MAPPING)
    semantic_report = read_json(INSTRUCTION_SEMANTICS_REPORT)
    inventories = {}
    binary_rows = []
    address_index = {}

    for binary in project["binaries"]:
        rows = load_inventory(binary["inventory"])
        inventories[binary["id"]] = rows
        code_bytes = sum(row["size"] for row in rows)
        binary_rows.append(
            {
                "id": binary["id"],
                "name": binary["name"],
                "functions": len(rows),
                "analyzed_code_bytes": code_bytes,
                "input_status": validate_private_input(binary),
            }
        )
        for row in rows:
            key = (binary["id"], row["address"])
            if key in address_index:
                raise ValueError(f"duplicate function address in inventory: {key}")
            address_index[key] = row

    valid_statuses = set(project["progress_model"]["function_statuses"])
    function_statuses = Counter()
    function_scopes = Counter()
    subsystem_stats = defaultdict(lambda: {"functions": 0, "bytes": 0, "verified": 0})
    touched_bytes = 0
    for item in recovered:
        item["address"] = item["address"].upper()
        key = (item["binary"], item["address"])
        source = address_index.get(key)
        if source is None:
            raise ValueError(f"recovered function is absent from canonical inventory: {key}")
        if item["status"] not in valid_statuses:
            raise ValueError(f"invalid function status for {key}: {item['status']}")
        for evidence in item.get("evidence", []):
            if not (ROOT / evidence).exists():
                raise ValueError(f"missing evidence file for {key}: {evidence}")
        item["size"] = source["size"]
        touched_bytes += source["size"]
        function_statuses[item["status"]] += 1
        function_scopes[item["scope"]] += 1
        stats = subsystem_stats[item["subsystem"]]
        stats["functions"] += 1
        stats["bytes"] += source["size"]
        if item["status"] == "behavior_verified":
            stats["verified"] += 1

    feature_credit = project["progress_model"]["feature_credit"]
    feature_statuses = Counter()
    feature_groups = defaultdict(lambda: {"items": 0, "credit": 0.0, "statuses": Counter()})
    seen_features = set()
    for feature in features:
        if feature["id"] in seen_features:
            raise ValueError(f"duplicate feature id: {feature['id']}")
        seen_features.add(feature["id"])
        if feature["status"] not in feature_credit:
            raise ValueError(f"invalid feature status: {feature['status']}")
        feature_statuses[feature["status"]] += 1
        group = feature_groups[feature["group"]]
        group["items"] += 1
        group["credit"] += feature_credit[feature["status"]]
        group["statuses"][feature["status"]] += 1

    expected_checks = {item["id"]: item for item in fidelity_manifest["checks"]}
    actual_checks = {item["id"]: item for item in fidelity_report["checks"]}
    if set(expected_checks) != set(actual_checks):
        raise ValueError("View Rosters fidelity report does not match its check manifest")
    possible_fidelity = 0.0
    earned_fidelity = 0.0
    derived_fidelity_groups = {}
    for check_id, expected in expected_checks.items():
        actual = actual_checks[check_id]
        if float(actual["weight"]) != float(expected["weight"]):
            raise ValueError(f"View Rosters fidelity weight changed for {check_id}")
        credit = float(actual["credit"])
        weight = float(expected["weight"])
        if credit < 0.0 or credit > weight:
            raise ValueError(f"View Rosters fidelity credit is outside its weight for {check_id}")
        possible_fidelity += weight
        earned_fidelity += credit
        group = derived_fidelity_groups.setdefault(expected["group"], [0.0, 0.0])
        group[0] += credit
        group[1] += weight
    derived_fidelity = round(earned_fidelity * 100.0 / possible_fidelity, 2)
    if fidelity_report["feature_ids"] != fidelity_manifest["feature_ids"] or \
            abs(float(fidelity_report["fidelity_percent"]) - derived_fidelity) > 0.01:
        raise ValueError("View Rosters fidelity summary is stale or inconsistent")
    reported_groups = fidelity_report.get("group_scores", {})
    if set(reported_groups) != set(derived_fidelity_groups):
        raise ValueError("View Rosters fidelity group summary is stale or incomplete")
    for group_name, (group_earned, group_possible) in derived_fidelity_groups.items():
        reported = reported_groups[group_name]
        group_percent = round(group_earned * 100.0 / group_possible, 2)
        if (abs(float(reported["earned_points"]) - group_earned) > 0.001 or
                abs(float(reported["possible_points"]) - group_possible) > 0.001 or
                abs(float(reported["percent"]) - group_percent) > 0.01):
            raise ValueError(f"View Rosters fidelity group is stale: {group_name}")
    feature_index = {item["id"]: item for item in features}
    for feature_id in fidelity_manifest["feature_ids"]:
        feature = feature_index.get(feature_id)
        if feature is None or feature.get("verification_report") != "reports/view_rosters_fidelity.json":
            raise ValueError(f"View Rosters fidelity is not linked from feature {feature_id}")
        if derived_fidelity < 100.0 and feature["status"] == "verified":
            raise ValueError(f"feature {feature_id} cannot be verified below 100% fidelity")
        if feature.get("instruction_semantics_report") != "reports/instruction_semantics.json":
            raise ValueError(f"instruction semantics are not linked from feature {feature_id}")

    semantic_originals = {item["address"]: item for item in semantic_original["functions"]}
    semantic_mappings = {item["address"]: item for item in semantic_mapping["functions"]}
    semantic_results = {item["address"]: item for item in semantic_report["functions"]}
    if not semantic_originals or set(semantic_originals) != set(semantic_mappings) or \
            set(semantic_originals) != set(semantic_results):
        raise ValueError("instruction-semantic function scopes are inconsistent")
    semantic_totals = {
        "functions": len(semantic_originals),
        "bytes": sum(int(item["size_bytes"]) for item in semantic_originals.values()),
        "instructions": sum(int(item["instruction_count"])
                            for item in semantic_originals.values()),
        "basic_blocks": sum(int(item["basic_block_count"])
                            for item in semantic_originals.values()),
        "control_flow_edges": sum(int(item["control_flow_edge_count"])
                                  for item in semantic_originals.values()),
        "direct_call_sites": sum(int(item["direct_call_site_count"])
                                 for item in semantic_originals.values()),
    }
    if semantic_report["original_scope"] != semantic_totals:
        raise ValueError("instruction-semantic original totals are stale")
    accounted_instructions = sum(
        int(item["accounted_instruction_count"]) for item in semantic_results.values())
    accounted_blocks = sum(
        int(item["accounted_basic_block_count"]) for item in semantic_results.values())
    verified_edges = sum(
        int(item["structurally_verified_edge_count"]) for item in semantic_results.values())
    recomp_found = sum(bool(item["recomp_entrypoint_found"])
                       for item in semantic_results.values())
    if semantic_report["instruction_accounting"] != {
            "accounted_instructions": accounted_instructions,
            "total_instructions": semantic_totals["instructions"],
            "accounted_basic_blocks": accounted_blocks,
            "total_basic_blocks": semantic_totals["basic_blocks"]}:
        raise ValueError("instruction-accounting totals are stale")
    if semantic_report["structural_verification"] != {
            "verified_control_flow_edges": verified_edges,
            "total_control_flow_edges": semantic_totals["control_flow_edges"]}:
        raise ValueError("instruction-semantic CFG totals are stale")
    if semantic_report["recomp_crosscheck"]["entrypoints_found"] != recomp_found or \
            semantic_report["recomp_crosscheck"]["total_functions"] != len(semantic_results):
        raise ValueError("instruction-semantic recomp cross-check is stale")

    total_functions = sum(row["functions"] for row in binary_rows)
    total_bytes = sum(row["analyzed_code_bytes"] for row in binary_rows)
    feature_points = sum(feature_credit[item["status"]] for item in features)
    matching = function_statuses["matching"]
    complete_scope = function_scopes["complete"]

    return {
        "schema_version": 1,
        "project": project["project"],
        "version": project["version"],
        "reconstruction": {
            "scoped_binaries": binary_rows,
            "discovered_functions": total_functions,
            "analyzed_code_bytes": total_bytes,
            "evidence_tracked_functions": len(recovered),
            "evidence_tracked_bytes": touched_bytes,
            "evidence_coverage_percent": round(len(recovered) * 100.0 / total_functions, 2),
            "evidence_byte_coverage_percent": round(touched_bytes * 100.0 / total_bytes, 2),
            "complete_functions": complete_scope,
            "matching_functions": matching,
            "status_counts": dict(sorted(function_statuses.items())),
            "scope_counts": dict(sorted(function_scopes.items())),
            "subsystems": {key: subsystem_stats[key] for key in sorted(subsystem_stats)},
        },
        "native_port": {
            "catalogued_features": len(features),
            "status_counts": dict(sorted(feature_statuses.items())),
            "roadmap_points": feature_points,
            "roadmap_completion_percent": round(feature_points * 100.0 / len(features), 2),
            "groups": {
                key: {
                    "items": feature_groups[key]["items"],
                    "points": feature_groups[key]["credit"],
                    "completion_percent": round(
                        feature_groups[key]["credit"] * 100.0 / feature_groups[key]["items"], 2
                    ),
                    "status_counts": dict(sorted(feature_groups[key]["statuses"].items())),
                }
                for key in sorted(feature_groups)
            },
            "features": features,
            "fidelity": {"view_rosters": fidelity_report},
            "instruction_semantics": semantic_report,
        },
        "methodology_notes": project["progress_model"]["notes"],
    }


def render_markdown(report: dict) -> str:
    reconstruction = report["reconstruction"]
    native = report["native_port"]
    lines = [
        "# Decompilation progress",
        "",
        "> Generated by `python tools/report_progress.py`. Do not edit this file manually.",
        "",
        "## Current baseline",
        "",
        f"- **{reconstruction['discovered_functions']:,}** functions and "
        f"**{reconstruction['analyzed_code_bytes']:,}** code bytes discovered by headless Ghidra "
        "in the currently scoped original binaries.",
        f"- **{reconstruction['evidence_tracked_functions']:,}** functions "
        f"({reconstruction['evidence_coverage_percent']:.2f}%) have explicit recomp/Ghidra/source evidence records.",
        f"- Evidence records touch **{reconstruction['evidence_tracked_bytes']:,}** original code bytes "
        f"({reconstruction['evidence_byte_coverage_percent']:.2f}%). Partial records do not mean the whole function is complete.",
        f"- **{reconstruction['complete_functions']}** functions are currently claimed behavior-complete and "
        f"**{reconstruction['matching_functions']}** are instruction-matching. We deliberately begin conservatively.",
        f"- Native-port roadmap estimate: **{native['roadmap_completion_percent']:.2f}%** "
        f"across {native['catalogued_features']} catalogued features.",
        f"- View Rosters end-to-end fidelity: **{native['fidelity']['view_rosters']['fidelity_percent']:.2f}%** "
        "from weighted behavioral and local-only visual-reference checks.",
        f"- View Rosters original-MIPS accounting: "
        f"**{native['instruction_semantics']['instruction_accounting']['accounted_instructions']}/"
        f"{native['instruction_semantics']['original_scope']['instructions']} instructions** have "
        "explicit block-level accounting; this is not an instruction-match percentage.",
        "",
        "## Original binary inventory",
        "",
        "| Binary | Functions | Analyzed code bytes | Private input |",
        "|---|---:|---:|---|",
    ]
    for binary in reconstruction["scoped_binaries"]:
        lines.append(
            f"| {binary['name']} | {binary['functions']:,} | "
            f"{binary['analyzed_code_bytes']:,} | {binary['input_status']} |"
        )
    lines += [
        "",
        "## Evidence coverage by subsystem",
        "",
        "| Subsystem | Tracked functions | Original bytes touched | Behavior-verified records |",
        "|---|---:|---:|---:|",
    ]
    for name, stats in reconstruction["subsystems"].items():
        lines.append(f"| {name} | {stats['functions']} | {stats['bytes']:,} | {stats['verified']} |")
    lines += [
        "",
        "## Native-port roadmap",
        "",
        "| Group | Items | Estimated completion | Status counts |",
        "|---|---:|---:|---|",
    ]
    for name, group in native["groups"].items():
        counts = ", ".join(f"{key}: {value}" for key, value in group["status_counts"].items())
        lines.append(f"| {name} | {group['items']} | {group['completion_percent']:.2f}% | {counts} |")
    fidelity = native["fidelity"]["view_rosters"]
    lines += [
        "",
        "## View Rosters fidelity",
        "",
        f"**{fidelity['fidelity_percent']:.2f}%** · "
        f"{fidelity['earned_points']:.2f}/{fidelity['possible_points']:.2f} weighted points",
        " · ".join(
            f"{name}: **{score['percent']:.2f}%**"
            for name, score in fidelity.get("group_scores", {}).items()
        ),
        "",
        "| Check | Group | Result | Credit |",
        "|---|---|---:|---:|",
    ]
    for check in fidelity["checks"]:
        result = (f"{check['similarity_percent']:.2f}% similarity"
                  if "similarity_percent" in check else check["status"])
        lines.append(f"| {check['description']} | {check['group']} | {result} | "
                     f"{check['credit']:.2f}/{check['weight']:.2f} |")
    lines += [
        "",
        "Original screenshots and generated heatmaps remain under `.local/`; the public report contains only measurements and cryptographic reference hashes.",
    ]
    semantics = native["instruction_semantics"]
    scope = semantics["original_scope"]
    accounting = semantics["instruction_accounting"]
    structural = semantics["structural_verification"]
    checkpoints = semantics["native_checkpoint_observation"]
    recomp = semantics["recomp_crosscheck"]
    lines += [
        "",
        "## View Rosters instruction semantics",
        "",
        f"- Original scope: **{scope['functions']} functions**, **{scope['instructions']} MIPS instructions**, "
        f"**{scope['basic_blocks']} basic blocks**, and **{scope['control_flow_edges']} CFG edges**.",
        f"- Explicitly accounted: **{accounting['accounted_instructions']}/{accounting['total_instructions']} instructions** "
        f"across **{accounting['accounted_basic_blocks']}/{accounting['total_basic_blocks']} blocks**.",
        f"- Structurally verified: **{structural['verified_control_flow_edges']}/{structural['total_control_flow_edges']} CFG edges**.",
        f"- Native semantic checkpoints observed: **{checkpoints['observed_functions']}/{checkpoints['total_functions']} functions**.",
        f"- Static-recomp entry points found: **{recomp['entrypoints_found']}/{recomp['total_functions']}**; "
        f"missing: **{', '.join(recomp['missing_entrypoints']) or 'none'}**.",
        "- Original/native trace-equivalent scenarios: **0**; exact MIPS matching is not configured.",
        "",
        "Source ownership, block accounting, CFG verification, runtime trace equivalence, and exact binary matching are independent tiers and are never blended into one score.",
    ]
    lines += ["", "## Methodology", ""]
    lines.extend(f"- {note}" for note in report["methodology_notes"])
    lines += [
        "",
        "The function inventories contain addresses, sizes, and generated names only. Original binaries, disc files, decoded images, audio, and all other copyrighted assets remain under `.local/` and are not published.",
        "",
    ]
    return "\n".join(lines)


def render_html(report: dict) -> str:
    reconstruction = report["reconstruction"]
    native = report["native_port"]
    roster_fidelity = native["fidelity"]["view_rosters"]["fidelity_percent"]
    semantic_accounting = native["instruction_semantics"]["instruction_accounting"]
    group_cards = []
    for name, group in native["groups"].items():
        group_cards.append(
            f'<section class="card"><h3>{html.escape(name)}</h3>'
            f'<div class="bar"><span style="width:{group["completion_percent"]}%"></span></div>'
            f'<p>{group["completion_percent"]:.2f}% · {group["items"]} catalogued items</p></section>'
        )
    notes = "".join(f"<li>{html.escape(note)}</li>" for note in report["methodology_notes"])
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>NBA Live 97 decompilation progress</title>
<style>
body{{font:16px system-ui;background:#090b18;color:#edf0ff;margin:0}}main{{max-width:1000px;margin:auto;padding:32px}}
h1{{color:#ffd72d}}.stats,.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:14px}}
.stat,.card{{background:#151934;border:1px solid #2c3567;border-radius:10px;padding:18px}}.value{{font-size:2rem;font-weight:800}}
.label,p,li{{color:#bac2e8}}.bar{{height:10px;background:#292e51;border-radius:8px;overflow:hidden}}.bar span{{display:block;height:100%;background:#ffd72d}}
code{{color:#ffd72d}}
</style></head><body><main>
<h1>NBA Live 97 decompilation progress</h1><p>{html.escape(report['version'])} · generated from committed manifests</p>
<div class="stats">
<div class="stat"><div class="value">{reconstruction['discovered_functions']:,}</div><div class="label">original functions discovered</div></div>
<div class="stat"><div class="value">{reconstruction['evidence_coverage_percent']:.2f}%</div><div class="label">functions with explicit evidence records</div></div>
<div class="stat"><div class="value">{reconstruction['matching_functions']}</div><div class="label">instruction-matching functions</div></div>
<div class="stat"><div class="value">{native['roadmap_completion_percent']:.2f}%</div><div class="label">native-port roadmap estimate</div></div>
<div class="stat"><div class="value">{roster_fidelity:.2f}%</div><div class="label">View Rosters measured fidelity</div></div>
<div class="stat"><div class="value">{semantic_accounting['accounted_instructions']}/{semantic_accounting['total_instructions']}</div><div class="label">View Rosters MIPS instructions explicitly accounted</div></div>
</div><h2>Native-port groups</h2><div class="grid">{''.join(group_cards)}</div>
<h2>How to read this</h2><ul>{notes}</ul>
<p>Private game inputs and decoded assets remain under <code>.local/</code>.</p>
</main></body></html>
"""


def render_svg(report: dict) -> str:
    """Render a GitHub-safe progress card from the canonical report values."""
    reconstruction = report["reconstruction"]
    native = report["native_port"]
    evidence = reconstruction["evidence_coverage_percent"]
    roadmap = native["roadmap_completion_percent"]
    roster_fidelity = native["fidelity"]["view_rosters"]["fidelity_percent"]
    semantic_accounting = native["instruction_semantics"]["instruction_accounting"]
    evidence_width = round(330.0 * evidence / 100.0, 2)
    roadmap_width = round(330.0 * roadmap / 100.0, 2)
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="760" height="178" viewBox="0 0 760 178" role="img" aria-labelledby="title desc">
<title id="title">NBA Live 97 decompilation progress</title>
<desc id="desc">{evidence:.2f} percent of discovered original functions have evidence records; the native port roadmap is {roadmap:.2f} percent complete.</desc>
<defs>
  <linearGradient id="panel" x1="0" y1="0" x2="1" y2="1"><stop stop-color="#121735"/><stop offset="1" stop-color="#080b1d"/></linearGradient>
  <linearGradient id="gold" x1="0" y1="0" x2="1" y2="0"><stop stop-color="#ffe44a"/><stop offset="1" stop-color="#f6a800"/></linearGradient>
</defs>
<rect x="1" y="1" width="758" height="176" rx="12" fill="url(#panel)" stroke="#343e7d" stroke-width="2"/>
<rect x="18" y="18" width="6" height="142" rx="3" fill="#ffd72d"/>
<g font-family="Segoe UI, Arial, sans-serif">
  <text x="42" y="38" fill="#ffd72d" font-size="20" font-weight="700">NBA LIVE 97 · DECOMP STATUS</text>
  <text x="718" y="37" fill="#8994ca" font-size="12" text-anchor="end">SLUS-00267</text>

  <text x="42" y="70" fill="#eef1ff" font-size="14" font-weight="600">ORIGINAL FUNCTION EVIDENCE</text>
  <text x="372" y="70" fill="#ffd72d" font-size="15" font-weight="700" text-anchor="end">{evidence:.2f}%</text>
  <rect x="42" y="79" width="330" height="12" rx="6" fill="#282e54"/>
  <rect x="42" y="79" width="{evidence_width}" height="12" rx="6" fill="url(#gold)"/>
  <text x="42" y="108" fill="#aeb7e4" font-size="12">{reconstruction['evidence_tracked_functions']:,} / {reconstruction['discovered_functions']:,} discovered functions tracked with explicit evidence</text>

  <text x="408" y="70" fill="#eef1ff" font-size="14" font-weight="600">NATIVE PORT ROADMAP</text>
  <text x="738" y="70" fill="#60e6a8" font-size="15" font-weight="700" text-anchor="end">{roadmap:.2f}%</text>
  <rect x="408" y="79" width="330" height="12" rx="6" fill="#282e54"/>
  <rect x="408" y="79" width="{roadmap_width}" height="12" rx="6" fill="#35c98c"/>
  <text x="408" y="108" fill="#aeb7e4" font-size="12">{native['catalogued_features']} catalogued, evidence-backed feature milestones</text>

  <line x1="42" y1="126" x2="738" y2="126" stroke="#29315f"/>
  <text x="42" y="150" fill="#8994ca" font-size="12">MIPS ACCOUNTED</text>
  <text x="160" y="150" fill="#eef1ff" font-size="13" font-weight="700">{semantic_accounting['accounted_instructions']}/{semantic_accounting['total_instructions']}</text>
  <text x="248" y="150" fill="#8994ca" font-size="12">VIEW ROSTERS</text>
  <text x="358" y="150" fill="#eef1ff" font-size="13" font-weight="700">{roster_fidelity:.2f}% fidelity</text>
  <text x="520" y="150" fill="#8994ca" font-size="12">ASSETS</text>
  <text x="580" y="150" fill="#eef1ff" font-size="13" font-weight="700">LOCAL ONLY</text>
</g>
</svg>
"""


def write_or_check(path: Path, content: str, check: bool) -> bool:
    content = content.replace("\r\n", "\n")
    if check:
        current = path.read_text(encoding="utf-8").replace("\r\n", "\n") if path.exists() else ""
        if current != content:
            print(f"out-of-date generated report: {path.relative_to(ROOT)}", file=sys.stderr)
            return False
        return True
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="fail if committed reports are stale")
    args = parser.parse_args()
    report = build_report()
    outputs = [
        (REPORT_JSON, json.dumps(report, indent=2, sort_keys=False) + "\n"),
        (REPORT_MD, render_markdown(report)),
        (REPORT_HTML, render_html(report)),
        (REPORT_SVG, render_svg(report)),
    ]
    ok = all(write_or_check(path, content, args.check) for path, content in outputs)
    if ok:
        verb = "validated" if args.check else "generated"
        print(
            f"Progress {verb}: {report['reconstruction']['discovered_functions']} functions, "
            f"{report['reconstruction']['evidence_tracked_functions']} evidence-tracked, "
            f"native roadmap {report['native_port']['roadmap_completion_percent']:.2f}%"
        )
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
