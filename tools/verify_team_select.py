"""Check the bounded Team Select contract; never promote missing retail evidence."""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# Frozen bounded audit denominators, not a transitive graph or completion score.
DENOMINATORS = {"team_select_owner":637, "setup_callback":34,
                "shared_dependency":2596, "ratings_dependency":524}
MATCH_FUNCTIONS = {("gameonly", "0x80063D58"):97, ("gameonly", "0x800655B0"):156,
                   ("feonly", "0x8003E7A8"):45, ("feonly", "0x8003E698"):31,
                   ("feonly", "0x8003E714"):37, ("feonly", "0x8003E620"):15,
                   ("feonly", "0x8003A0D8"):20}
INPUT_FUNCTIONS = {("feonly", "0x8003B194"):("0x8003B1E8",21),
                   ("feonly", "0x8003F7C8"):("0x80040A1C",1173)}
INPUT_REFERENCES = {("team_select.json", "0x8003AE4C"):210,
                    ("team_select.json", "0x8003D930"):828,
                    ("team_select.json", "0x8004F934"):41,
                    ("user_setup.json", "0x80037010"):1716,
                    ("user_setup.json", "0x80036CA0"):42}


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def require(ok, message):
    if not ok:
        raise ValueError(message)


def metadata():
    ledger = read(ROOT / "config/decomp/team_select.json")
    seen, totals = set(), {}
    for f in ledger["functions"]:
        require(f["address"] not in seen, "double-counted function")
        seen.add(f["address"])
        require(0 <= f["instructions_accounted"] <= f["instructions_total"], "invalid instruction credit")
        require(f["instructions_accounted"] == 0, "new credit requires per-block evidence and a reviewed verifier extension")
        require(f["remaining_uncertainty"], "missing uncertainty")
        require(f["native_owner"] and f["scope"], "missing semantic owner/scope")
        totals[f["scope"]] = totals.get(f["scope"], 0) + f["instructions_total"]
    require(totals == ledger["instruction_denominators"] == DENOMINATORS and len(seen)==33, "ledger totals drifted")
    require(ledger["evidence"]["original_team_select_runtime"] == "pending", "retail status needs explicit new evidence")
    user = read(ROOT / "config/decomp/user_setup.json")
    seen, totals = set(), {}
    for f in user["functions"]:
        require(f["address"] not in seen, "User Setup double-counted function")
        seen.add(f["address"])
        require(f["instructions_accounted"] == 0 and f["instructions_total"] > 0,
                "User Setup instruction credit needs reviewed per-block evidence")
        require(f["native_owner"] and f["remaining_uncertainty"] and f["tests_evidence"],
                "User Setup missing semantic owner/evidence/uncertainty")
        totals[f["scope"]] = totals.get(f["scope"], 0) + f["instructions_total"]
    require(totals == user["instruction_denominators"] ==
            {"user_setup_owner":2194, "shared_dependency":1270} and len(seen)==13, "User Setup denominator drift")
    require(user["evidence"]["original_runtime"]=="pending", "User Setup original evidence needs explicit review")
    match = read(ROOT / "config/decomp/match_setup.json")
    seen, totals = {}, {}
    for f in match["functions"]:
        key = (f["binary"], f["address"])
        require(key not in seen, "match setup double-counted function")
        seen[key] = f["instructions_total"]
        require(f["instructions_accounted"] == 0 and f["native_owner"] and
                f["remaining_uncertainty"] and f["tests_evidence"], "match setup ownership/credit drift")
        totals[f["scope"]] = totals.get(f["scope"], 0) + f["instructions_total"]
    require(seen == MATCH_FUNCTIONS and totals == match["instruction_denominators"] ==
            {"roster_dependency":253, "rules_dependency":148}, "match setup denominator drift")
    require(match["evidence"]["original_runtime"] == "pending", "match original evidence needs explicit review")
    inputs = read(ROOT / "config/decomp/frontend_input.json")
    seen, totals = {}, {}
    for f in inputs["functions"]:
        key = (f["binary"], f["address"])
        require(key not in seen, "frontend input double-counted function")
        seen[key] = (f["end_exclusive"], f["instructions_total"])
        require(f["instructions_accounted"] == 0 and f["native_owner"] and
                f["remaining_uncertainty"] and f["tests_evidence"], "frontend input ownership/credit drift")
        require(int(f["end_exclusive"],16)-int(f["address"],16) == 4*f["instructions_total"],
                "frontend input full source extent drift")
        totals[f["scope"]] = totals.get(f["scope"],0)+f["instructions_total"]
    require(seen == INPUT_FUNCTIONS and totals == inputs["instruction_denominators"] ==
            {"shared_caller":1194}, "frontend input full-function denominator drift")
    references = {}
    inventories = {"team_select.json":ledger, "user_setup.json":user}
    for ref in inputs["shared_inventory_references"]:
        key = (ref["ledger"], ref["address"])
        require(ref["binary"] == "feonly" and key in INPUT_REFERENCES and key not in references,
                "frontend input duplicate/unknown shared reference")
        owner = next((f for f in inventories[ref["ledger"]]["functions"] if f["address"] == ref["address"]),None)
        require(owner and owner["instructions_total"] == ref["instructions_total"], "shared owner denominator drift")
        references[key] = ref["instructions_total"]
    require(references == INPUT_REFERENCES, "frontend input shared references drifted")
    require(all(inputs["evidence"][key] == "pending" for key in
                ("original_runtime","original_visual_timing_audio","live_physical_walkthrough")),
            "frontend input original evidence needs explicit review")
    scenarios = read(ROOT / "config/decomp/team_select_scenarios.json")
    ids = [s["id"] for s in scenarios["native_frames"]]
    require(len(ids) == len(set(ids)), "duplicate scenario")
    print("TEAM METADATA PASS: bounded denominators, separate tiers, pending original evidence retained")
    return scenarios


def ppm(path):
    parts = path.read_bytes().split(b"\n", 3)
    require(len(parts) == 4 and parts[:3] == [b"P6", b"512 240", b"255"], f"bad image header {path}")
    require(len(parts[3]) == 512*240*3, f"bad image extent {path}")
    return parts[3]


def match_snapshots(first, second, by_id, stock_ranks, modified_ranks):
    # Independent field relations from the bounded source contract. Native
    # artifacts are regression inputs, never newly adopted retail fixtures.
    option_addresses = [0x80021D86,0x80021D7C,0x80021D7D,0x80021D7E,0x80021D7F,
                        0x80021D95,0x80021D81,0x80021D82,0x80021D83,0x80021D84,0x80021D99]
    options = [1,9,9,9,9,5,0,0,3,0,1]  # unchanged first-boot settings in this fixture
    rules = [0,0,0,1,0,0,0,0,0,0,0,1,0,0]  # arcade + the fixture's out-of-bounds edit
    receipts = []
    for name, frame_id, ranks, roster_generation, profile_generation in (
            ("match_snapshot.json", "match-handoff-pending", stock_ranks, 0, 4),
            ("match_modified_snapshot.json", "match-modified-roster", modified_ranks, 1, 23)):
        s = read(first / name)
        require(s == read(second / name), f"snapshot nondeterminism {name}")
        require(s["scope"] == "partial ordinary exhibition snapshot" and s["pending"] == 3,
                f"unowned launch fields silently accepted {name}")
        require(s["setup"] == [0,0,2,0] and s["venue"] == s["launch_control"] == 0,
                f"Setup/exhibition projection {name}")
        require(s["assignments"] == [2,0,0,0,0,0,0,0] and s["selectors"] == [-2]*8 and
                s["controls_source"] == [0]*8 and s["profile_ids"] == [0]*8, f"controller projection {name}")
        require(s["rules"] == s["custom_rules"] == rules, f"effective custom-rule backup {name}")
        require(s["options"] == [{"address":a,"value":v} for a,v in zip(option_addresses,options)],
                f"noncontiguous option mapping {name}")
        require(s["roster_generation"] == roster_generation and s["profile_generation"] == profile_generation and
                s["created_generation"] == s["created_count"] == 0, f"source generations {name}")
        require(len(s["base_identity"]) == 32 and len(s["teams"]) == 2, f"snapshot shape {name}")
        frame = by_id[frame_id]
        require([t["id"] for t in s["teams"]] == [frame["home"],frame["away"]], f"selected teams {name}")
        for team in s["teams"]:
            ids, count = team["ids"], team["count"]
            require(len(ids) == 15 and 8 <= count <= 15 and all(0 <= n < 493 for n in ids[:count]) and
                    ids[count:] == [65535]*(15-count), f"ordered occupied roster prefix {name}")
            require(len(set(ids[:count])) == count and team["active"] == min(count,12) and
                    team["aliases"] == [i if i < count else 0 for i in range(12)] and
                    team["lineup"] == list(range(12)), f"roster index projection {name}")
            require(len(team["metadata"]) == 20 and
                    team["metadata"][:5] == [category[team["id"]] for category in ranks],
                    f"current rank metadata {name}")
        receipts.append(s)
    before, after = receipts
    require(before["base_identity"] == after["base_identity"], "accepted reorder changed base identity")
    home = list(before["teams"][0]["ids"])
    home[0], home[8] = home[8], home[0]
    require(after["teams"][0]["ids"] == home and
            after["teams"][0]["metadata"][5:] == before["teams"][0]["metadata"][5:],
            "owned match snapshot lost accepted reorder or immutable metadata")
    require(stock_ranks[0][by_id["match-modified-roster"]["away"]] == 1,
            "special29 ->30 -> rank1 navigation changed")
    print("MATCH SNAPSHOT HOST PASS: owned ordinary inputs, fresh saved-roster ranks, controls lifetime and pending guards")


def capture(first, second, contract, fixture):
    require(first.resolve() != second.resolve(), "two distinct capture directories required")
    states = read(first / "states.json")
    require(states == read(second / "states.json"), "native state nondeterminism")
    require([s["id"] for s in states] == [s["id"] for s in contract["native_frames"]], "scenario list drift")
    by_id = {s["id"]: s for s in states}
    for actual, expected in zip(states, contract["native_frames"]):
        require(all(actual[k] == v for k, v in expected["expect"].items()), f"state mismatch {actual['id']}")
        a, b = first / (actual["id"]+".ppm"), second / (actual["id"]+".ppm")
        require(ppm(a) == ppm(b), f"native frame nondeterminism {a.name}")
    for name in ("rank_cache.json", "modified_rank_cache.json"):
        cache = read(first / name)
        require(cache == read(second / name), f"numeric nondeterminism {name}")
        require(len(cache["scores"]) == 5 and all(len(c) == 29 for c in cache["scores"]), "bad score dimensions")
        require(len(cache["ranks"]) == 5 and all(sorted(c) == list(range(1, 32)) and c[29:] == [30,31] for c in cache["ranks"]), "bad rank permutation")
    stock = read(first / "rank_cache.json")
    changed = read(first / "modified_rank_cache.json")
    require(stock != changed, "saved/reopened reordered roster did not affect derived cache")
    match_snapshots(first, second, by_id, stock["ranks"], changed["ranks"])
    rank = stock["ranks"][0]
    require(rank[by_id["away-scoring-next"]["away"]] == rank[25] % 31 + 1, "rank scan mismatch")
    for name in ("selector-gold","help","help-return","help-early-close","invalid-square","setup-return","reentry",
                 "user-setup-entry","user-setup-return","user-reentry"):
        require(by_id[name]["away"] == by_id["away-scoring-next"]["away"], f"unexpected team mutation {name}")
    require(by_id["random-complete"]["away"] < 29, "random selected special team")
    require(by_id["random-last-wait"]["away"] == by_id["random-complete"]["away"], "final wait changed candidate")
    # Localized native regressions, not original pixel equivalence.
    def crop(name, x0, y0, x1, y1):
        pixels = ppm(first / (name+".ppm"))
        return b"".join(pixels[(y*512+x0)*3:(y*512+x1)*3] for y in range(y0,y1))
    require(crop("entry",368,15,488,78) != crop("home-left",368,15,488,78), "home logo unchanged")
    require(crop("away-scoring-next",190,119,440,135) != crop("selector-gold",190,119,440,135), "selected tint unchanged")
    require(crop("help",130,82,380,192) != crop("help-return",130,82,380,192), "Help unchanged")
    require(crop("user-help",111,70,401,210) != crop("user-help-return",111,70,401,210), "User Help unchanged")
    require(crop("user-home",0,73,512,141) != crop("user-away",0,73,512,141), "User assignment marker unchanged")
    require(crop("user-editor-help",120,50,400,205) != crop("user-editor-help-return",120,50,400,205), "editing Help unchanged")
    require(crop("user-delete-delay",166,88,346,173) != crop("user-delete-barrier",166,88,346,173), "delete text remained after shrink")
    if fixture:
        observed = read(fixture)
        for key in ("scores","ranks"):
            expected = observed["observed_original_"+key]
            require(len(expected)==29 and all(len(row)==5 for row in expected), "bad original fixture")
            require(all(stock[key][c][t] == expected[t][c] for t in range(29) for c in range(5)), "first mismatch: original rank cache "+key)
        print("TEAM HISTORICAL RANK CACHE PASS: 145/145 scores + 145/145 ranks; this is not a state3 runtime comparison")
    else:
        print("TEAM HISTORICAL RANK CACHE PENDING: pass --original-ranks with independently captured private fixture")
    for root in (first, second):
        trace = (root.parent / "trace.log").read_text()
        require(trace.count("role=setup-selector") == 2 and trace.count("SETUP-EXIT-WAIT") == 2 and
                trace.count("USER-EXIT-WAIT") == 5, "Setup/Cancel sound and dispatcher barriers")
        require(trace.count("role=team-help-open") == 2 and trace.count("role=team-help-close") == 2, "Help tick/handler sound events")
        require(trace.count("role=user-help-open")==2 and trace.count("role=user-help-close")==2,"User Help sound events")
        require(trace.count("role=user-dialog-confirm")==2 and trace.count("role=user-dialog-close")==6,
                "delete confirmation/notice close sounds")
        require(trace.count("USER-SAVE-FAILED")==1 and trace.count("USER-DELETE ")==1,
                "failed-save retry and accepted deletion traces")
        require(trace.count("USER-INLINE-SAVE-PASS PASS:")==1 and trace.count("USER-PLACEMENT-PIXELS PASS:")==1,
                "same-update save continuation and independent retained-target rendering")
        require(trace.count("MATCH-CONTROLS-INIT ") == 1 and trace.count("MATCH-SNAPSHOT revision=") == 4 and
                trace.count("MATCH-SNAPSHOT-PENDING ") == 1, "cold controls/snapshot publication/refusal traces")
        require("TEAM-HANDOFF" in trace and "USER-ENTRY" in trace and
                "MATCH-HANDOFF-PENDING" in trace and "TEAM-CAPTURE PASS:" in trace, "missing boundary/pass trace")
    print(f"TEAM NATIVE PASS: {len(states)}/{len(states)} deterministic frames + host state + isolated saved-roster adapter")
    print("PENDING: original state3/5 runtime, visuals/timing/audio, physical controls/topology cadence, text/arrow lifecycle and gameplay")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--first", type=Path)
    parser.add_argument("--second", type=Path)
    parser.add_argument("--original-ranks", type=Path)
    args = parser.parse_args()
    contract = metadata()
    if args.first or args.second:
        require(args.first and args.second, "both capture paths required")
        capture(args.first, args.second, contract, args.original_ranks)


if __name__ == "__main__":
    main()
