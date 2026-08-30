"""Check the bounded Team Select contract; never promote missing retail evidence."""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# Frozen bounded audit denominators, not a transitive graph or completion score.
DENOMINATORS = {"team_select_owner":637, "setup_callback":34,
                "shared_dependency":2596, "ratings_dependency":524}


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
        require(trace.count("role=team-help-open") == 2 and trace.count("role=team-help-close") == 2, "Help tick/handler sound events")
        require(trace.count("role=user-help-open")==2 and trace.count("role=user-help-close")==2,"User Help sound events")
        require(trace.count("role=user-dialog-confirm")==2 and trace.count("role=user-dialog-close")==6,
                "delete confirmation/notice close sounds")
        require(trace.count("USER-SAVE-FAILED")==1 and trace.count("USER-DELETE ")==1,
                "failed-save retry and accepted deletion traces")
        require("TEAM-HANDOFF" in trace and "USER-ENTRY" in trace and
                "MATCH-HANDOFF-PENDING" in trace and "TEAM-CAPTURE PASS:" in trace, "missing boundary/pass trace")
    print(f"TEAM NATIVE PASS: {len(states)}/{len(states)} deterministic frames + host state + isolated saved-roster adapter")
    print("PENDING: original state3/5 runtime, visuals/timing/audio, physical controls, topology debounce and gameplay")


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
