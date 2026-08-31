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


def cursor_rng_step(words):
    """Independent 7A538 word/carry contract; never calls the native helper."""
    require(len(words) == 6 and all(type(word) is int and 0 <= word <= 0xffffffff
            for word in words), "invalid six-word RNG step input")
    radix = 1 << 32
    result, carry = list(words), 0
    # Descend from word4 to word0. Each sum uses the already updated successor
    # and the preceding unsigned carry; overflow above word0 is discarded.
    for index in range(4, -1, -1):
        carry, result[index] = divmod(words[index] + result[index+1] + carry, radix)
    # The source then increments from word5 toward word0 with carry. Treating
    # the array as one big-endian 192-bit integer keeps this stage independent
    # of the native increment loop, including wrap of all six words.
    combined = 0
    for word in result:
        combined = combined * radix + word
    combined = (combined + 1) % (1 << 192)
    return [(combined >> (32 * (5-index))) & 0xffffffff for index in range(6)]


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
    # This is the full scenario tier; separately recorded arrow-construction
    # observations do not complete navigation, flash, timing or exit coverage.
    require(ledger["evidence"]["original_team_select_runtime"] == "pending", "full retail status needs explicit new evidence")
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


def arrow_flash_cases(first, second):
    cases = read(first / "arrow_flash_cases.json")
    require(cases == read(second / "arrow_flash_cases.json"), "arrow flash host nondeterminism")
    require([(c["seed_kind"], c["arrow"]) for c in cases] ==
            [(seed, arrow) for seed in range(3) for arrow in range(4)], "missing flash host cases")
    # Independent source schedule: four interpolation updates, transition,
    # ten hold updates, transition, four return updates, cleanup. Signed
    # divisions truncate toward zero, including unequal inherited channels.
    def blend(start, target, elapsed):
        return [a + (1 if b >= a else -1) * (abs(b-a)*elapsed//4)
                for a, b in zip(start, target)]
    gold, neutral = [120,102,0], [128,128,128]
    for case in cases:
        seed, arrow = case["seed_kind"], case["arrow"]
        require(case["audio_setting"] == (9 if arrow & 1 else 0), "flash mute case drift")
        frames = case["frames"]
        require(len(frames) == 22, "flash schedule must include scheduling and21 updates")
        require(type(case["pixel_checks"]) is int and case["pixel_checks"] > 0,
                "flash needs original-glyph per-channel pixel checks")
        initial = [17,203,91] if seed == 2 else [0,0,0]
        for tick, frame in enumerate(frames):
            require(len(frame["shown"]) == len(frame["logical"]) == 4, "four retained arrows required")
            for index, paint in enumerate(frame["logical"]):
                require(paint["active"] == 1, "live arrow disappeared during flash")
                if index != arrow:
                    require(paint["flags"] == 0 and paint["rgb"] == neutral and paint["known"] == 7,
                            "flash changed the wrong arrow")
            paint = frame["logical"][arrow]
            flags = 0x42 if tick <= 4 else 0xc2 if tick <= 15 else 0x82 if tick <= 20 else 0
            duration = 10 if 5 <= tick <= 15 else 4
            elapsed = tick if tick <= 4 else tick-5 if tick <= 15 else min(tick-16,4)
            rgb = neutral if not tick else blend(initial,gold,tick) if tick <= 4 else \
                gold if tick <= 16 else blend(gold,neutral,min(tick-16,4))
            known = 0 if not seed and 1 <= tick <= 3 else 7
            require((paint["flags"],paint["duration"],paint["elapsed"],paint["known"]) ==
                    (flags,duration,elapsed,known), f"flash phase/mask mismatch {seed}/{arrow}/{tick}")
            if known:
                require(paint["rgb"] == rgb, f"flash RGB mismatch {seed}/{arrow}/{tick}")
            if tick:
                require(frame["shown"] == frame["logical"], "completed flash frame differs from pre-poll state")
            else:
                require(all(p["flags"] == 0 and p["rgb"] == neutral and p["known"] == 7
                            for p in frame["shown"]), "flash leaked into the already submitted input frame")
        require(frames[-1]["logical"][arrow]["start"] == [120,128,128],
                "flash cleanup lost source retained-red quirk")
    print("TEAM FLASH HOST PASS:12 cases/264 frames; all4 arrows, unknown/zero/unequal seeds, mute and presentation order")


def capture(first, second, contract, fixture):
    require(first.resolve() != second.resolve(), "two distinct capture directories required")
    states = read(first / "states.json")
    require(states == read(second / "states.json"), "native state nondeterminism")
    require([s["id"] for s in states] == [s["id"] for s in contract["native_frames"]], "scenario list drift")
    by_id = {s["id"]: s for s in states}
    arrow_flash_cases(first, second)
    # These harness checkpoints precede the newly opened screen's first
    # requested presentation. Do not allow artifacts to redefine that boundary.
    preview_frames = {"entry", "left-before-poll", "reentry", "user-editor-abandon",
                      "user-setup-return", "match-cancel-preserved"}
    entry_group = None
    for actual, expected in zip(states, contract["native_frames"]):
        require(all(actual[k] == v for k, v in expected["expect"].items()), f"state mismatch {actual['id']}")
        if actual["page"] == "Team Select":
            require(0 <= actual["shown_criterion"] < 6 and actual["shown_presentation"] >= 0,
                    f"invalid completed presentation metadata {actual['id']}")
            # Independent4FA3C/4F7B8 anchors. A shown object follows the
            # completed presentation, even when Cross has changed logical side.
            side = actual["shown_side"]
            arrows = [[x + 500*side, 96] for x in (320,460,-458,-318)]
            labels = [[0,0] if i % 6 == 0 else
                      [248,106+16*(i % 6)+(200 if i//6 != side else 0)] for i in range(12)]
            values = [[388 if i < 6 else 112,86 if i % 6 == 0 else 106+16*(i % 6)] for i in range(12)]
            require(actual["shown_arrows"] == arrows and actual["shown_labels"] == labels and
                    actual["shown_values"] == values, f"retained text/arrow pose mismatch {actual['id']}")
            if actual["id"] in preview_frames - {"left-before-poll"}:
                entry_group = 120+actual["side"]
            require(actual["team_graphic_count"] == 2 and actual["arrow_group"] == entry_group,
                    f"source graphics/group routing mismatch {actual['id']}")
            preview = actual["shown_entry_preview"]
            require(type(preview) is int and preview == int(actual["id"] in preview_frames),
                    f"entry preview/presentation boundary mismatch {actual['id']}")
            if preview:
                # Native crossfade preview projects a copy. Live entry nodes
                # must still contain their unpresented source commands/anchors.
                labels = [[0,0] if i % 6 == 0 else
                          [248,106+16*(i % 6)+(96 if i >= 6 else 0)] for i in range(12)]
                values = [[388 if i < 6 else 112,86] if i % 6 == 0 else
                          [388,106+16*(i % 6)+(96 if i >= 6 else 0)] for i in range(12)]
                arrows = [[x,96] for x in (320,460,-458,-318)]
                require(actual["arrow_group"] == 120+actual["side"], "entry arrow group changed")
            require(actual["logical_arrows"] == arrows and actual["logical_labels"] == labels and
                    actual["logical_values"] == values, f"callback/preview advanced live placement {actual['id']}")
            head_moving = 8 if preview and actual["side"] == 1 and actual["criterion"] else 0
            require(actual["value_head_moving"] == head_moving,
                    f"selected value head query mismatch {actual['id']}")
        a, b = first / (actual["id"]+".ppm"), second / (actual["id"]+".ppm")
        require(ppm(a) == ppm(b), f"native frame nondeterminism {a.name}")
    # Explicit owner-frame counts from the source contract and harness steps;
    # they constrain both artifacts even if each has the same wrong metadata.
    for before, after, count in (
            ("entry", "left-before-poll", 0), ("left-before-poll", "home-left", 1),
            ("home-left", "left-first-post-frame", 1), ("left-first-post-frame", "left-post-wait", 6),
            ("left-post-wait", "left-held-repeat", 1), ("away-active", "away-first-post-frame", 1),
            ("reentry", "start-held-exit", 2),
            ("help-poll-frame", "help-first-growth", 1), ("help-first-growth", "help-full-box", 12),
            ("help-full-box", "help-first-text", 1), ("help-first-text", "help", 10),
            ("help", "help-ack-frame", 1), ("help-ack-frame", "help-first-shrink", 1),
            ("random-poll-frame", "random-first-wait", 1), ("random-first-wait", "random-last-wait", 65)):
        require(by_id[after]["shown_presentation"] - by_id[before]["shown_presentation"] == count,
                f"completed presentation count mismatch {before} -> {after}")
    require(by_id["entry"]["arrow_group"] == by_id["away-active"]["arrow_group"] ==
            by_id["away-first-post-frame"]["arrow_group"] == 120 and by_id["reentry"]["arrow_group"] == 121,
            "Cross regrouped persistent arrows or reentry lost entry-page group")
    require(by_id["reentry"]["value_head_moving"] == 8 and by_id["start-held-exit"]["poll_phase"] == 4,
            "type41 graphics must bypass pending away-value settlement before Start")
    print("TEAM PLACEMENT HOST PASS: separate label/value and4 retained arrow poses; preview isolation and source graphics bypass")
    require(all(p["flags"] == 0 for p in by_id["home-left"]["shown_arrow_tints"]) and
            by_id["home-left"]["logical_arrow_tints"][0]["flags"] == 0x42,
            "Left flash must be scheduled after the input presentation")
    first_flash = by_id["left-first-post-frame"]["shown_arrow_tints"][0]
    require(first_flash["elapsed"] == 1 and first_flash["known"] == 0,
            "unanchored entry silently invented first-flash RGB")
    held = by_id["left-held-repeat"]["logical_arrow_tints"][0]
    require(held["flags"] == 0xc2 and held["elapsed"] == 0,
            "held Left must retrigger the hold clock")
    require(by_id["help-full-box"]["text_help_active"] == 1 and
            by_id["help-ack-frame"]["text_help_active"] == 0,
            "Help allocation/retirement boundary mismatch")
    for name in ("select-cleanup", "start-cleanup"):
        require(all(p["active"] for p in by_id[name]["shown_arrow_tints"]) and
                not any(p["active"] for p in by_id[name]["logical_arrow_tints"]),
                "exit retirement changed an already completed frame")
    require(by_id["help-full-box"]["shown_help_width"] == by_id["help-first-text"]["shown_help_width"] ==
            by_id["help-ack-frame"]["shown_help_width"], "full Help box changed before shrinking")
    for actual in states:
        require(len(actual["shared_rng"]) == 6 and all(type(word) is int and 0 <= word <= 0xffffffff
                for word in actual["shared_rng"]), "invalid six-word cursor/team RNG receipt")
    for before, after, draws in (
            ("setup", "setup-quarter-wrap", 1), ("setup-held-start", "entry", 0),
            ("left-before-poll", "home-left", 1), ("home-left", "left-post-wait", 0),
            ("left-post-wait", "left-held-repeat", 1), ("criterion-up-wrap", "criterion-down-wrap", 1),
            ("away-active", "away-first-post-frame", 0), ("selector-gold", "help-poll-frame", 1),
            ("help-poll-frame", "help", 0), ("help", "help-ack-frame", 1),
            ("help-ack-frame", "help-first-shrink", 0), ("random-poll-frame", "random-first-wait", 0)):
        require(by_id[after]["cursor_rng_draws"] - by_id[before]["cursor_rng_draws"] == draws,
                f"cursor cue count mismatch {before} -> {after}")
        if not draws and before != "random-poll-frame":
            require(by_id[after]["shared_rng"] == by_id[before]["shared_rng"],
                    f"six-word RNG changed without a source consumer {before} -> {after}")
        elif draws == 1:
            # Every one-draw interval listed above has exactly one accepted
            # cursor cue and no Random candidate or other six-word consumer.
            require(by_id[after]["shared_rng"] == cursor_rng_step(by_id[before]["shared_rng"]),
                    f"six-word cursor RNG transition mismatch {before} -> {after}")
    # Hand-specified seeds executed independently through original4F934/2F124/
    # 9267C/7A538, stopping at the first accepted4EF40. No disc table is embedded.
    expected_rng_cases = [
        {"setting":0,"seed":[1,2,3,4,5,6],"after":[21,20,18,15,11,7],"cursor_draws":0,"candidate":21},
        {"setting":9,"seed":[1,2,3,4,5,6],"after":[92,71,51,33,18,8],"cursor_draws":1,"candidate":28},
        {"setting":0,"seed":[29,0,0,0,0,0],"after":[36,6,5,4,3,3],"cursor_draws":0,"candidate":4},
        {"setting":9,"seed":[29,0,0,0,0,0],"after":[36,6,5,4,3,3],"cursor_draws":1,"candidate":4}]
    require(read(first/"cursor_rng_cases.json") == read(second/"cursor_rng_cases.json") == expected_rng_cases,
            "first mismatch: actual cue/Circle dispatch versus original seeded RNG boundary")
    print("CURSOR RNG HOST PASS: bootstrap/continuation counts and4 original-source seeded cue/Circle cases; full history pending")
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
    require(by_id["random-first-wait"]["shown_away"] == by_id["random-poll-frame"]["away"],
            "first random owner frame must show candidate1 before candidate2 mutation")
    require(by_id["random-poll-frame"]["shown_away"] == by_id["user-reentry"]["away"],
            "random poll frame must retain the previously accepted team")
    # Localized native regressions, not original pixel equivalence.
    def crop(name, x0, y0, x1, y1):
        pixels = ppm(first / (name+".ppm"))
        return b"".join(pixels[(y*512+x0)*3:(y*512+x1)*3] for y in range(y0,y1))
    require(crop("entry",368,15,488,78) == crop("home-left",368,15,488,78), "input leaked into pre-sample logo frame")
    require(crop("home-left",368,15,488,78) != crop("left-first-post-frame",368,15,488,78), "first postwait did not show changed logo")
    require(crop("help-full-box",130,82,380,192) != crop("help-first-text",130,82,380,192), "terminal growth created text before its presentation returned")
    require(crop("help",130,82,380,192) == crop("help-ack-frame",130,82,380,192), "acknowledgment removed text from its already completed frame")
    require(crop("help-ack-frame",130,82,380,192) != crop("help-first-shrink",130,82,380,192), "next Help presentation did not begin shrinking")
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
    print("PENDING: full original state3/5 runtime scenarios, visuals/timing/audio, physical controls/topology cadence, general text/arrow lifecycle and gameplay")


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
