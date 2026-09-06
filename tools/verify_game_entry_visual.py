"""Verify the deterministic frontend-to-GAMEONLY 0x80029994 visual receipt."""

import argparse
import hashlib
import json
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def ppm_pixels(path, width=512, height=240):
    data = path.read_bytes()
    parts = data.split(b"\n", 3)
    extent = f"{width} {height}".encode("ascii")
    require(parts[:3] == [b"P6", extent, b"255"], f"bad PPM header: {path}")
    require(len(parts) == 4 and len(parts[3]) == width * height * 3,
            f"bad PPM extent: {path}")
    return parts[3]


def ppm_hash(path, width=512, height=240):
    return hashlib.sha256(ppm_pixels(path, width, height)).hexdigest()


def crop_rgb(pixels, source_width, x, y, width, height):
    rows = []
    for row in range(y, y + height):
        begin = (row * source_width + x) * 3
        rows.append(pixels[begin:begin + width * 3])
    return b"".join(rows)


def equal_outside_rect(before, after, canvas_width, canvas_height,
                       x, y, width, height):
    for row in range(canvas_height):
        begin = row * canvas_width * 3
        end = begin + canvas_width * 3
        if y <= row < y + height:
            left = begin + x * 3
            right = left + width * 3
            if before[begin:left] != after[begin:left] or \
                    before[right:end] != after[right:end]:
                return False
        elif before[begin:end] != after[begin:end]:
            return False
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--frames", type=Path, required=True)
    parser.add_argument("--trace", type=Path, required=True)
    args = parser.parse_args()

    states = read_json(args.frames / "states.json")
    require(len(states) == 134, "native click-through frame count drifted")
    by_id = {state["id"]: state for state in states}
    require(len(by_id) == len(states), "duplicate captured frame id")
    frontend_dispatch = read_json(args.frames / "frontend_dispatch_trace.json")
    fd = frontend_dispatch
    require(fd["program"] == "FEONLY" and int(fd["address"], 16) == 0x8003F7C8
            and int(fd["end"], 16) == 0x80040A1B and fd["instructions"] == 1173
            and fd["source_sha256"] ==
            "a42d7d2d97ab00ad7ddb214677b743dfd5d98d05119f9e6894fd092a6ccf1b9f",
            "frontend dispatcher provenance drifted")
    require(fd["completed"] == fd["accepted"] == fd["result"] == 1
            and fd["contract_failure"] == 0
            and [fd[k] for k in ("operations", "reads", "stores", "callbacks",
                                 "instruction_count")] == [177, 82, 53, 42, 903],
            "frontend dispatcher did not complete the exact synthetic source path")
    require([int(fd["entry_call"][k], 16) for k in ("pc", "delay", "ra")] ==
            [0x800360F4, 0x800360F8, 0x800360FC],
            "frontend dispatcher source entry boundary drifted")
    require(fd["before"]["launch"] == fd["after"]["launch"] == 1
            and fd["before"]["mode"] == 0 and fd["after"]["mode"] == 1
            and int(fd["before"]["retained_buffer"], 16) == 0x80140000
            and int(fd["after"]["retained_buffer"], 16) == 0,
            "accepted frontend mode or cleanup pointer was not published")
    require(all(fd["before"][k] == fd["after"][k] != 0 for k in
                ("home_roster_checksum", "away_roster_checksum")),
            "selected roster records did not reach their original output arrays")
    require(fd["user_setup"]["accepted_result"] == 6 and
            fd["user_setup"]["assignments"] == [1, 2, 0, 0, 0, 0, 0, 0],
            "native input API did not produce User Setup's accepted result")
    expected_fd_calls = [
        (0x8003F8C8,0x8003F7B0,2), (0x8003F8DC,0x800770D4,3),
        (0x8003F8F4,0x80030CDC,0), (0x8003F8FC,0x80030308,0),
        (0x8003F92C,0x8003D2A4,0), (0x8003F97C,0x800459C8,0),
        (0x8003FA08,0x80031A88,1), (0x8003FCF4,0x80037010,0),
        (0x8003FD3C,0x80061674,1), (0x8003FD44,0x80046D24,0),
        (0x8003FD4C,0x8003E7A8,0), (0x800407E8,0x80028B8C,0),
        (0x800407F0,0x800804E8,1), (0x800407F8,0x80028B8C,0),
        (0x80040850,0x8005851C,1), (0x80040868,0x8005851C,1)]
    expected_fd_calls += [(0x80040900,0x800909A8,3),
                          (0x80040964,0x800909A8,3)] * 12
    expected_fd_calls += [(0x800409D8,0x80029DD0,0), (0x800409E0,0x8002FC30,0)]
    require(len(fd["call_sequence"]) == len(expected_fd_calls),
            "frontend exit callback count drifted")
    fd_invocations = {}
    for actual, (pc, target, argc) in zip(fd["call_sequence"], expected_fd_calls):
        fd_invocations[pc] = fd_invocations.get(pc, 0) + 1
        require([int(actual[k], 16) for k in ("pc", "target", "delay")] ==
                [pc, target, pc + 4] and actual["argc"] == argc and
                actual["invocation"] == fd_invocations[pc],
                "frontend cleanup/roster call order or invocation drifted")
    fd_frames = ["frontend-dispatch-before", "frontend-dispatch-after"]
    require(all(name in by_id for name in fd_frames) and
            all(by_id[name]["page"] == "User Setup" for name in fd_frames),
            "frontend source probe is missing the native User Setup boundary")
    fd_hashes = [ppm_hash(args.frames / f"{name}.ppm") for name in fd_frames]
    require(fd_hashes[0] == fd_hashes[1],
            "synthetic CPU dispatcher probe unexpectedly changed the native menu")
    require(fd["visual_class"] == "UI/menu" and not fd["gameplay_shown"] and
            "80028B68" in fd["next_unbound_boundary"] and
            "GAMELOAD" in fd["next_unbound_boundary"],
            "dispatcher receipt lost the exact unbound loader boundary")
    fd_verified = {
        "program":"FEONLY", "address":"0x8003F7C8", "end":"0x80040A1B",
        "driver_frame_count":len(states), "source_receipt":frontend_dispatch,
        "before_sha256":fd_hashes[0], "after_sha256":fd_hashes[1],
        "visual_status":"Gameplay shown: BLOCKED",
        "reason":"Native accepted User Setup remains pending; original frontend exit services and GAMELOAD transfer are unbound."}
    (args.frames / "frontend_dispatch_verified.json").write_text(
        json.dumps(fd_verified, indent=2) + "\n", encoding="utf-8")

    # CPU-only wrapper proof uses an explicitly synthetic frontend-main boundary.
    fe = read_json(args.frames / "frontend_dispatch_entry_trace.json")
    require(fe["program"] == "FEONLY" and int(fe["address"], 16) == 0x800360D4
            and int(fe["inclusive_end"], 16) == 0x8003610B
            and fe["bytes"] == 56 and fe["instructions"] == 14
            and fe["source_sha256"] ==
            "6af71d91fded3e2b5260c84bb86fd101539e86fca86ffef2b9e06b93e32dbce0",
            "initialized frontend wrapper provenance drifted")
    require(fe["completed"] == fe["accepted"] == fe["result"] == 1
            and fe["contract_failure"] == 0,
            "initialized frontend wrapper failed its composed source path")
    require([int(fe["parent_call"][key], 16) for key in
             ("pc", "delay", "entry", "ra", "s0")] ==
            [0x80028AA0, 0x80028AA4, 0x800360D4, 0x80028AA8, 0],
            "synthetic parent boundary lost the original S0 delay effect")
    require([int(fe["child_call"][key], 16) for key in
             ("pc", "delay", "entry", "ra")] ==
            [0x800360F4, 0x800360F8, 0x8003F7C8, 0x800360FC]
            and fe["child_call"]["operation"] == 4,
            "wrapper did not compose its natural dispatcher child")
    fw = fe["wrapper"]
    require([fw[key] for key in ("operations", "accesses", "reads", "stores",
                                 "callbacks", "instruction_count")] ==
            [5, 4, 1, 3, 1, 14] and int(fw["frame_sp"], 16) == 0x801EFFE8,
            "wrapper exact instruction or access prefix drifted")
    expected_accesses = [
        (0x800360E0, 0x80021EE4, 1, 1, 2),
        (0x800360E8, 0x801EFFF8, 0x80028AA8, 2, 2),
        (0x800360F0, 0x800C6E68, 32, 3, 2),
        (0x800360FC, 0x801EFFF8, 0x80028AA8, 5, 1)]
    require(len(fw["access_journal"]) == 4, "wrapper access journal missing")
    for actual, (pc, address, value, operation, kind) in zip(
            fw["access_journal"], expected_accesses):
        require([int(actual[key], 16) for key in ("pc", "address", "value")] ==
                [pc, address, value] and actual["operation"] == operation
                and actual["kind"] == kind and actual["width"] == 4
                and actual["known_mask"] == 15,
                "wrapper ordered global/stack accesses drifted")
    require(fe["globals"]["before"] ==
            {"initialized": 0, "scalar": 0, "saved_ra_slot": 0}
            and fe["globals"]["after"] ==
            {"initialized": 1, "scalar": 32, "saved_ra_slot": 0x80028AA8},
            "wrapper global and saved return stores missing")
    require(all(int(fw[key]["word"], 16) == 0x80028AA8 and
                fw[key]["known_mask"] == 15 for key in ("saved_ra", "restored_ra")),
            "wrapper did not restore its parent return word")
    nested = fe["dispatcher"]
    require(nested["result"] == nested["completed"] == 1 and
            [nested[key] for key in ("operations", "reads", "stores", "callbacks",
                                     "instruction_count")] == [177, 82, 53, 42, 903]
            and fe["user_setup"]["accepted"] == 1
            and fe["user_setup"]["result"] == 6,
            "nested recovered dispatcher/User Setup acceptance failed")
    normalized_calls = [{**call, "argc": call["argument_count"]}
                        for call in nested["call_sequence"]]
    for call in normalized_calls:
        del call["argument_count"]
    require(normalized_calls == fd["call_sequence"],
            "wrapper composition changed the verified 42-call prerequisite sequence")
    machine = fe["final_machine"]
    require(len(machine["gpr"]) == 32 and
            all(word["known_mask"] == 15 for word in machine["gpr"]),
            "composed wrapper full GPR knownness drifted")
    for index, value in [(0, 0), (16, 0), (29, 0x801F0000), (31, 0x80028AA8)]:
        require(int(machine["gpr"][index]["word"], 16) == value,
                "composed wrapper live return state drifted")
    require(machine["hi"] == {"word": "0x10203040", "known_mask": 5}
            and machine["lo"] == {"word": "0x50607080", "known_mask": 10},
            "wrapper composition changed preserved HI/LO")
    fe_frames = ["frontend-dispatch-entry-before", "frontend-dispatch-entry-after"]
    require(all(name in by_id and by_id[name]["page"] == "User Setup"
                for name in fe_frames), "wrapper probe missing accepted User Setup frames")
    fe_hashes = [ppm_hash(args.frames / f"{name}.ppm") for name in fe_frames]
    require(fe_hashes[0] == fe_hashes[1] == fd_hashes[1],
            "CPU-only wrapper probe changed native pixels")
    require(fe["classification"] == "UI/menu" and fe["gameplay_shown"] == "BLOCKED"
            and "80028AA8" in fe["next_unbound_boundary"]
            and "80028B68" in fe["next_unbound_boundary"]
            and "GAMELOAD" in fe["next_unbound_boundary"]
            and "synthetic" in fe["fixture_contract"].lower(),
            "wrapper receipt obscures synthetic prerequisites or the live loader boundary")
    (args.frames / "frontend_dispatch_entry_verified.json").write_text(
        json.dumps({"program": "FEONLY", "address": "0x800360D4",
                    "end": "0x8003610B", "driver_frame_count": len(states),
                    "source_receipt": fe, "before_sha256": fe_hashes[0],
                    "after_sha256": fe_hashes[1],
                    "visual_status": "Gameplay shown: BLOCKED",
                    "reason": fe["next_unbound_boundary"]}, indent=2) + "\n",
        encoding="utf-8")

    fm = read_json(args.frames / "frontend_main_trace.json")
    require([fm[key] for key in ("program", "address", "inclusive_end", "bytes",
                                 "instructions", "source_sha256")] ==
            ["FEONLY", "0x80028800", "0x80028b8b", 908, 227,
             "a9325ac1de6cf8da7bd5a43d95da2f2e61bfc586cd3f265824d3e519cb42b208"],
            "frontend main provenance drifted")
    require(fm["completed"] == fm["accepted"] == fm["result"] == 1
            and fm["contract_failure"] == 0,
            "frontend main did not complete its synthetic service path")
    require([int(fm["parent_call"][key], 16) for key in
             ("pc", "delay", "entry", "ra")] ==
            [0x8007B838, 0x8007B83C, 0x80028800, 0x8007B840],
            "frontend main unowned overlay-entry boundary drifted")
    def main_word(actual, value):
        return int(actual["word"], 16) == value and actual["known_mask"] == 15
    branch = fm["branch_state"]
    require(all(main_word(branch[key], value) for key, value in
                (("initial_frontend_flag", 1), ("menu_frontend_flag", 1),
                 ("intro_flag", 0), ("context_selector", 0)))
            and branch["intro_iterations"] == 0 and branch["wait_iterations"] == 20,
            "frontend main independently loaded branch state drifted")
    main_receipt = fm["main"]
    require([main_receipt[key] for key in ("operations", "accesses", "reads",
             "stores", "callbacks", "instruction_count", "intro_iterations",
             "wait_iterations")] == [98, 33, 9, 24, 65, 299, 0, 20],
            "frontend main exact instruction/access/callback counts drifted")
    # Source ranges include branch delays and all twenty callback-live waits.
    expected_main_pcs = list(range(0x80028800, 0x80028850, 4))
    expected_main_pcs += list(range(0x80028864, 0x800289B4, 4))
    expected_main_pcs += list(range(0x800289C8, 0x80028A40, 4))
    expected_main_pcs += list(range(0x80028A50, 0x80028A7C, 4))
    expected_main_pcs += list(range(0x80028AA0, 0x80028B08, 4))
    expected_main_pcs += list(range(0x80028B08, 0x80028B1C, 4)) * 20
    expected_main_pcs += list(range(0x80028B1C, 0x80028B8C, 4))
    require([int(pc, 16) for pc in main_receipt["instruction_trace"]] ==
            expected_main_pcs, "frontend main ordered source instruction path drifted")
    expected_main_calls = [
        (0x80028810,0x8007B844,0), (0x80028818,0x8008B368,0),
        (0x80028834,0x800769E0,3), (0x80028880,0x8008BFB0,2),
        (0x80028898,0x80078B7C,0), (0x800288A8,0x8008A4F8,1),
        (0x800288B8,0x80079BF0,2), (0x800288C0,0x8007F5A8,1),
        (0x800288C8,0x8007F5D0,0), (0x800288D0,0x80076148,1),
        (0x800288D8,0x8008004C,1), (0x800288EC,0x8007844C,1),
        (0x800288F4,0x8008B104,0), (0x800288FC,0x800802B8,1),
        (0x80028904,0x80028B8C,0), (0x8002890C,0x80028ED0,1),
        (0x80028934,0x800807D8,3), (0x8002893C,0x800804E8,1),
        (0x8002894C,0x800807D8,3), (0x80028954,0x800804E8,1),
        (0x8002895C,0x8008044C,1), (0x80028974,0x8008BFB0,2),
        (0x800289F4,0x80035D80,0), (0x800289FC,0x800517BC,0),
        (0x80028A04,0x800673A0,0), (0x80028A0C,0x8008DA98,0),
        (0x80028A14,0x8008ACB0,0), (0x80028A50,0x80035984,0),
        (0x80028A58,0x8008E5A0,0), (0x80028A60,0x80064C90,0),
        (0x80028AA0,0x800360D4,0), (0x80028AA8,0x8002F084,0),
        (0x80028AB0,0x80028E08,0), (0x80028ACC,0x8007B11C,2),
        (0x80028AD8,0x80077CD4,1), (0x80028AF0,0x80084C44,2),
        (0x80028AF8,0x80084C84,1), (0x80028B00,0x80084C9C,1)]
    expected_main_calls += [(0x80028B08,0x80028B8C,0)] * 20
    expected_main_calls += [
        (0x80028B1C,0x8008B1F0,0), (0x80028B24,0x800785F0,0),
        (0x80028B2C,0x80076110,0), (0x80028B34,0x80051B44,0),
        (0x80028B44,0x8008A944,2), (0x80028B54,0x800909A8,3),
        (0x80028B68,0x801E1410,0)]
    require(len(main_receipt["call_sequence"]) == len(expected_main_calls) == 65,
            "frontend main call journal lost the naturally composed wrapper")
    main_invocations = {}
    for actual, (pc, target, argc) in zip(main_receipt["call_sequence"],
                                         expected_main_calls):
        main_invocations[pc] = main_invocations.get(pc, 0) + 1
        require([int(actual[key], 16) for key in ("pc", "delay", "target")] ==
                [pc, pc + 4, target] and actual["argument_count"] == argc
                and actual["invocation"] == main_invocations[pc]
                and actual["program"] == (2 if pc == 0x80028B68 else 1),
                "frontend main call order or source/target overlay drifted")
        require(main_word(actual["ra"], pc + 8)
                and main_word(actual["sp"], 0x801EFFD8),
                "frontend main callback machine lost live source frame/return state")
    main_calls_by_pc = {int(call["pc"], 16): call
                        for call in main_receipt["call_sequence"]}
    for pc, values in [(0x80028834, (0xDC, 0x800FF5C8, 0xFE238)),
                       (0x80028ACC, (0x80024854, 0)),
                       (0x80028AD8, (0x80170000,)),
                       (0x80028AF0, (0, 0)),
                       (0x80028B54, (0x80170000, 0x801E0000, 0x1000))]:
        require(all(main_word(main_calls_by_pc[pc][f"a{i}"], value)
                    for i, value in enumerate(values)),
                "frontend main delayed argument or retained load-size forwarding drifted")
    expected_main_accesses = [
        (0x80028804,0x801EFFFC,0x8007B840,4,2),
        (0x80028808,0x801EFFF8,0x33001212,4,2),
        (0x8002880C,0x801EFFF4,0x33001111,4,2),
        (0x80028814,0x801EFFF0,0x33001010,4,2),
        (0x80028840,0x80021EE4,1,4,1),
        (0x8002887C,0x800170C4,0x80013800,4,2),
        (0x8002888C,0x80015094,0x80140000,4,2),
        (0x80028894,0x800D9B4C,0,4,2),
        (0x800288E8,0x800D9ADC,120,4,2),
        (0x80028924,0x801EFFE8,512,2,2),
        (0x80028928,0x801EFFEC,512,2,2),
        (0x80028930,0x801EFFEA,0,2,2),
        (0x80028938,0x801EFFEE,256,2,2),
        (0x80028970,0x800170C4,0x80013800,4,2),
        (0x80028980,0x80021568,0,2,1),
        (0x80028988,0x80015094,0x80140000,4,2),
        (0x800289A0,0x800170C0,0x800214F0,4,2),
        (0x800289A8,0x80021504,0x8001726C,4,2),
        (0x800289D0,0x8002199C,0x8002156C,4,2),
        (0x800289E0,0x80021520,0x80022AE0,4,2),
        (0x800289F0,0x80015030,0x800BC424,4,2),
        (0x800289F8,0x80021C20,0,1,2),
        (0x80028A20,0x8001EDEC,0,2,1),
        (0x80028A2C,0x800D9B3C,0,4,2),
        (0x80028A34,0x800D9B40,1,4,2),
        (0x80028A6C,0x80021EE4,1,4,1),
        (0x80028AC8,0x800D9B40,1,4,2),
        (0x80028AEC,0x800D9B40,0,4,2),
        (0x80028B60,0x801E0000,0x801E1410,4,1),
        (0x80028B70,0x801EFFFC,0x8007B840,4,1),
        (0x80028B74,0x801EFFF8,0x33001212,4,1),
        (0x80028B78,0x801EFFF4,0x33001111,4,1),
        (0x80028B7C,0x801EFFF0,0x33001010,4,1)]
    require(len(main_receipt["access_journal"]) == len(expected_main_accesses),
            "frontend main memory journal extent drifted")
    access_pcs = {item[0] for item in expected_main_accesses}
    call_pcs = {item[0] for item in expected_main_calls}
    expected_access_operations = {}
    operation = 0
    for pc in expected_main_pcs:
        if pc in access_pcs:
            operation += 1
            expected_access_operations[pc] = operation
        # A JAL's memory-writing delay spends its access before the call.
        if pc - 4 in call_pcs:
            operation += 1
    require(operation == 98, "frontend main source operation expectation drifted")
    for actual, (pc, address, value, width, kind) in zip(
            main_receipt["access_journal"], expected_main_accesses):
        require([int(actual[key], 16) for key in ("pc", "address", "value")] ==
                [pc, address, value] and actual["width"] == width
                and actual["kind"] == kind and actual["known_mask"] == (1 << width)-1,
                "frontend main ordered memory effect drifted")
        require(actual["operation"] == expected_access_operations[pc],
                "frontend main delay-slot access/call ordering drifted")
    loader = fm["loader_state"]
    require(main_word(loader["handle"], 0x80170000)
            and main_word(loader["size"], 0x1000)
            and main_word(loader["dynamic_entry"], 0x801E1410)
            and loader["copy_size"] == 4096
            and int(loader["copy_source"], 16) == 0x80170000
            and int(loader["copy_destination"], 16) == 0x801E0000
            and int(loader["source_checksum"], 16) != 0
            and loader["source_checksum"] == loader["destination_checksum"]
            and loader["dynamic_program"] == "GAMELOAD"
            and loader["dynamic_outcome"] == "RETURNED",
            "frontend main synthetic GAMELOAD boundary lost payload identity")
    require(fm["memory"]["before"]["gameload_entry"] == 0
            and fm["memory"]["after"] == {"frontend_flag": 1, "frontend_busy": 0,
                  "frontend_scalar": 32, "gameload_entry": 0x801E1410},
            "frontend main generated copy or wrapper state was not published")
    require(fm["wrapper"]["result"] == fm["wrapper"]["completed"] == 1
            and [fm["wrapper"][key] for key in ("operations", "accesses", "reads",
                 "stores", "callbacks", "instruction_count")] == [5,4,1,3,1,14]
            and fm["dispatcher"]["result"] == fm["dispatcher"]["completed"] == 1
            and [fm["dispatcher"][key] for key in ("operations", "reads", "stores",
                 "callbacks", "instruction_count")] == [177,82,53,42,903]
            and fm["dispatcher"]["call_sequence"] == fe["dispatcher"]["call_sequence"]
            and fm["user_setup"] == {"accepted": 1, "result": 6},
            "frontend main recovered wrapper/dispatcher/User Setup composition failed")
    for i, value in [(0,0), (16,0x33001010), (17,0x33001111), (18,0x33001212),
                     (29,0x801F0000), (31,0x8007B840)]:
        require(main_word(fm["final_machine"]["gpr"][i], value),
                "frontend main did not restore source callee-saved state")
    require(fm["final_machine"]["hi"] == fe["final_machine"]["hi"]
            and fm["final_machine"]["lo"] == fe["final_machine"]["lo"],
            "frontend main synthetic callbacks changed preserved HI/LO")
    fm_frames = ["frontend-main-before", "frontend-main-after"]
    require(all(name in by_id and by_id[name]["page"] == "User Setup"
                for name in fm_frames), "frontend main native boundary frames missing")
    fm_hashes = [ppm_hash(args.frames / f"{name}.ppm") for name in fm_frames]
    require(fm_hashes[0] == fm_hashes[1] == fe_hashes[1],
            "CPU-only frontend main probe changed native menu pixels")
    boundary = fm["next_unbound_boundary"]
    require(fm["classification"] == "UI/menu" and fm["gameplay_shown"] == "BLOCKED"
            and "synthetic" in fm["fixture_contract"].lower()
            and all(pc in boundary["earliest_production"] for pc in
                    ("8007B838", "80028810", "8007B844"))
            and all(pc in boundary["post_acceptance"] for pc in
                    ("80028AA8", "8002F084", "80028ACC", "80028B68", "GAMELOAD")),
            "frontend main receipt hides unbound production services")
    (args.frames / "frontend_main_verified.json").write_text(
        json.dumps({"program": "FEONLY", "address": "0x80028800",
                    "end": "0x80028B8B", "driver_frame_count": len(states),
                    "source_receipt": fm, "before_sha256": fm_hashes[0],
                    "after_sha256": fm_hashes[1],
                    "visual_status": "Gameplay shown: BLOCKED",
                    "reason": boundary}, indent=2) + "\n", encoding="utf-8")

    cleanup = read_json(args.frames / "frontend_exit_cleanup_trace.json")
    require([cleanup[key] for key in ("program", "address", "inclusive_end",
             "bytes", "instructions", "source_sha256")] ==
            ["FEONLY", "0x8002f084", "0x8002f0e7", 100, 25,
             "38b3b7e879958bf82f3c214dea99f4b4bdb0c69f77eae68ae32692c7c9da29ec"],
            "frontend exit cleanup provenance drifted")
    require(cleanup["completed"] == cleanup["accepted"] == cleanup["result"] == 1
            and cleanup["contract_failure"] == 0,
            "frontend exit cleanup failed its explicit source-boundary probe")
    parent = cleanup["parent_call"]
    require([int(parent[key],16) for key in ("pc", "delay", "entry", "ra")] ==
            [0x80028AA8,0x80028AAC,0x8002F084,0x80028AB0]
            and parent["argument_count"] == 0 and parent["program"] == 1,
            "frontend exit cleanup caller contract drifted")
    cp = cleanup["cleanup"]
    require([cp[key] for key in ("operations", "accesses", "reads", "stores",
                                 "callbacks", "instruction_count")] ==
            [10,5,3,2,5,25]
            and [int(pc,16) for pc in cp["instruction_trace"]] ==
            list(range(0x8002F084,0x8002F0E8,4)),
            "frontend exit cleanup did not execute its full nonzero-release path")
    require(main_word(cp["loaded_cleanup_selector"],0xFFFFFFFF)
            and main_word(cp["loaded_release_flag"],0x80145678)
            and {key:int(value,16) for key,value in cleanup["memory"].items()} ==
            {"cleanup_selector":0xFFFFFFFF,"release_before":0x80145678,
             "release_after":0},
            "frontend exit cleanup selector/release publication drifted")
    expected_cleanup_accesses = [
        (0x8002F088,0x801EFFF8,0x80028AB0,1,2),
        (0x8002F0A0,0x80021D6C,0xFFFFFFFF,4,1),
        (0x8002F0B0,0x8001502C,0x80145678,6,1),
        (0x8002F0CC,0x8001502C,0,8,2),
        (0x8002F0D8,0x801EFFF8,0x80028AB0,10,1)]
    require(len(cp["access_journal"]) == 5, "cleanup access journal extent drifted")
    for actual,(pc,address,value,operation,kind) in zip(
            cp["access_journal"],expected_cleanup_accesses):
        require([int(actual[key],16) for key in ("pc","address","value")] ==
                [pc,address,value] and actual["operation"] == operation
                and actual["width"] == 4 and actual["known_mask"] == 15
                and actual["kind"] == kind,
                "cleanup ordered stack/global accesses or delay prefix drifted")
    expected_cleanup_calls = [
        (0x8002F08C,0x8002EFBC,0,0x63000404),
        (0x8002F094,0x800394D4,0,0x63000404),
        (0x8002F0A4,0x80028C90,1,0xFFFFFFFF),
        (0x8002F0C0,0x8007760C,1,0x80145678),
        (0x8002F0D0,0x80076540,0,0x80145678)]
    require(len(cp["call_sequence"]) == 5, "cleanup callback journal extent drifted")
    initial_cleanup_gpr = [0] + [0x63000000+i*0x101 for i in range(1,32)]
    for index,(actual,(pc,target,argc,a0)) in enumerate(zip(
            cp["call_sequence"],expected_cleanup_calls)):
        require([int(actual[key],16) for key in ("pc","delay","target")] ==
                [pc,pc+4,target] and actual["argument_count"] == argc
                and actual["invocation"] == 1
                and main_word(actual["a0"],a0)
                and main_word(actual["sp"],0x801EFFE8)
                and main_word(actual["ra"],pc+8),
                "cleanup exact child contract or forwarded selector/pointer drifted")
        expected = initial_cleanup_gpr.copy()
        expected[4],expected[29],expected[31] = a0,0x801EFFE8,pc+8
        if index == 4:
            expected[1] = 0x80010000
        actual_machine = actual["machine"]
        require(len(actual_machine["gpr"]) == 32
                and all(main_word(word,value) for word,value in
                        zip(actual_machine["gpr"],expected))
                and actual_machine["hi"] == fe["final_machine"]["hi"]
                and actual_machine["lo"] == fe["final_machine"]["lo"],
                "cleanup full observed callback machine disagrees with fixture contract")
    expected_final = initial_cleanup_gpr.copy()
    expected_final[1],expected_final[4] = 0x80010000,0x80145678
    expected_final[29],expected_final[31] = 0x801F0000,0x80028AB0
    require(len(cleanup["final_machine"]["gpr"]) == 32
            and all(main_word(word,value) for word,value in
                    zip(cleanup["final_machine"]["gpr"],expected_final))
            and cleanup["final_machine"]["hi"] == fe["final_machine"]["hi"]
            and cleanup["final_machine"]["lo"] == fe["final_machine"]["lo"],
            "cleanup lost the live return machine or restored stack/RA")
    cleanup_frames = ["frontend-exit-cleanup-before","frontend-exit-cleanup-after"]
    require(all(name in by_id and by_id[name]["page"] == "User Setup"
                for name in cleanup_frames), "cleanup native boundary frames missing")
    cleanup_hashes = [ppm_hash(args.frames/f"{name}.ppm") for name in cleanup_frames]
    require(cleanup_hashes[0] == cleanup_hashes[1] == fm_hashes[1],
            "CPU-only cleanup probe changed native User Setup pixels")
    cb = cleanup["next_unbound_boundary"]
    require(cleanup["classification"] == "no direct visual effect"
            and cleanup["gameplay_shown"] == "BLOCKED"
            and "synthetic" in cleanup["fixture_contract"].lower()
            and "standalone" in cleanup["fixture_contract"].lower()
            and "8007B844" in cb["earliest_production"]
            and all(pc in cb["next_cleanup_child"] for pc in ("8002F08C","8002EFBC"))
            and "GAMELOAD" in cb["remaining_main_chain"],
            "cleanup receipt conceals standalone fixtures or next unbound child")
    (args.frames/"frontend_exit_cleanup_verified.json").write_text(
        json.dumps({"program":"FEONLY","address":"0x8002F084","end":"0x8002F0E7",
                    "driver_frame_count":len(states),"source_receipt":cleanup,
                    "before_sha256":cleanup_hashes[0],"after_sha256":cleanup_hashes[1],
                    "visual_status":"Gameplay shown: BLOCKED","reason":cb},indent=2)+"\n",
        encoding="utf-8")

    wait = read_json(args.frames / "frontend_exit_wait_trace.json")
    require([wait[key] for key in ("program", "address", "inclusive_end",
             "bytes", "instructions", "source_sha256")] ==
            ["FEONLY", "0x8002efbc", "0x8002f083", 200, 50,
             "45eed4157e3ece4487b1c0c8ea03ed780461937168a8d38e05853200ebf6ad53"],
            "frontend exit wait provenance drifted")
    require(wait["completed"] == wait["accepted"] == wait["result"] == 1
            and wait["contract_failure"] == 0,
            "frontend exit wait source-boundary probe failed")
    parent = wait["parent_call"]
    require([int(parent[key],16) for key in ("pc", "delay", "entry", "ra")] ==
            [0x8002F08C,0x8002F090,0x8002EFBC,0x8002F094]
            and parent["argument_count"] == 0 and parent["program"] == 1,
            "wait natural-caller boundary contract drifted")
    wp = wait["wait"]
    require([wp[key] for key in ("operations", "accesses", "reads", "stores",
                                "callbacks", "instruction_count")] ==
            [19,9,5,4,10,50]
            and [int(pc,16) for pc in wp["instruction_trace"]] ==
            list(range(0x8002EFBC,0x8002F084,4)),
            "wait did not execute its complete deadline path")
    branch = wait["branch_state"]
    require(branch["exit_path"] == 4 and branch["loop_iterations"] == 1
            and all(main_word(branch[key],value) for key,value in
                    (("initial_handle",0x80145678),("deadline",1360),
                     ("first_poll",0),("second_poll",0),("clock",1361)))
            and {key:int(value,16) for key,value in wait["memory"].items()} ==
            {"handle_before":0x80145678,"handle_after":0xFFFFFFFF,
             "secondary_before":0x80123458,"secondary_after":0},
            "wait deadline, latched polls, or ordered flag clears drifted")
    expected_wait_accesses = [
        (0x8002EFC0,0x80017268,0x80145678,1,1),
        (0x8002EFCC,0x801EFFFC,0x8002F094,2,2),
        (0x8002EFD4,0x801EFFF8,0x11223344,3,2),
        (0x8002F030,0x80017268,0x80145678,10,1),
        (0x8002F044,0x80017268,0xFFFFFFFF,12,2),
        (0x8002F05C,0x8002149C,0x80123458,15,1),
        (0x8002F06C,0x8002149C,0,17,2),
        (0x8002F070,0x801EFFFC,0x8002F094,18,1),
        (0x8002F074,0x801EFFF8,0x11223344,19,1)]
    require(len(wp["access_journal"]) == 9, "wait access extent drifted")
    for actual,(pc,address,value,operation,kind) in zip(
            wp["access_journal"],expected_wait_accesses):
        require([int(actual[key],16) for key in ("pc","address","value")] ==
                [pc,address,value] and actual["operation"] == operation
                and actual["width"] == 4 and actual["known_mask"] == 15
                and actual["kind"] == kind, "wait memory/access order drifted")
    expected_wait_calls = [
        (0x8002EFDC,0x8007B2BC,3),(0x8002EFE4,0x8008DA5C,0),
        (0x8002EFF0,0x8006B6A0,0),(0x8002F000,0x8006FCF0,0),
        (0x8002F010,0x80039260,0),(0x8002F018,0x8008DA5C,0),
        (0x8002F034,0x80092C34,1),(0x8002F048,0x80028C28,0),
        (0x8002F050,0x8006FAA0,0),(0x8002F060,0x80028CF4,1)]
    require(len(wp["call_sequence"]) == 10, "wait child extent drifted")
    expected = [0] + [0x52000000+i*0x101 for i in range(1,32)]
    expected[4],expected[5],expected[6] = 0x80145678,100,0xFFFFFFFF
    expected[16],expected[29] = 0x11223344,0x801EFFE8
    def wait_machine_matches(machine, gpr):
        return (len(machine["gpr"]) == 32 and
                all(main_word(word,value) for word,value in zip(machine["gpr"],gpr))
                and machine["hi"] == fe["final_machine"]["hi"]
                and machine["lo"] == fe["final_machine"]["lo"])
    for index,(actual,(pc,target,argc)) in enumerate(zip(
            wp["call_sequence"],expected_wait_calls)):
        expected[31] = pc+8
        expected[2] = 1000 if index == 2 else (1 if index == 6 else 0xFFFFFFFF)
        if index >= 2:
            expected[16] = 1360
        if index >= 7:
            expected[1] = 0x80010000
        if index == 9:
            expected[4] = 0x80123458
        require([int(actual[key],16) for key in ("pc","delay","target")] ==
                [pc,pc+4,target] and actual["argument_count"] == argc
                and actual["invocation"] == 1
                and wait_machine_matches(actual["machine"],expected),
                "wait full callback machine or delay-slot arguments drifted")
    expected[1],expected[16],expected[29],expected[31] = (
        0x80020000,0x11223344,0x801F0000,0x8002F094)
    require(wait_machine_matches(wait["final_machine"],expected),
            "wait did not preserve its full return machine")
    wait_frames = ["frontend-exit-wait-before","frontend-exit-wait-after"]
    require(all(name in by_id and by_id[name]["page"] == "User Setup"
                for name in wait_frames), "wait native boundary frames missing")
    wait_hashes = [ppm_hash(args.frames/f"{name}.ppm") for name in wait_frames]
    require(wait_hashes[0] == wait_hashes[1] == cleanup_hashes[1],
            "CPU-only wait probe changed native User Setup pixels")
    wb = wait["next_unbound_boundary"]
    require(wait["classification"] == "no direct visual effect"
            and wait["gameplay_shown"] == "BLOCKED"
            and "synthetic" in wait["fixture_contract"].lower()
            and "standalone" in wait["fixture_contract"].lower()
            and "8007B844" in wb["earliest_production"]
            and all(pc in wb["next_wait_child"] for pc in ("8002EFDC","8007B2BC")),
            "wait receipt conceals fixtures or its next unbound child")
    (args.frames/"frontend_exit_wait_verified.json").write_text(
        json.dumps({"program":"FEONLY","address":"0x8002EFBC","end":"0x8002F083",
                    "driver_frame_count":len(states),"source_receipt":wait,
                    "before_sha256":wait_hashes[0],"after_sha256":wait_hashes[1],
                    "visual_status":"Gameplay shown: BLOCKED","reason":wb},indent=2)+"\n",
        encoding="utf-8")

    drain = read_json(args.frames / "frontend_exit_drain_trace.json")
    require([drain[key] for key in ("program", "address", "inclusive_end",
             "bytes", "instructions", "source_sha256")] ==
            ["FEONLY", "0x800394d4", "0x80039573", 160, 40,
             "5b71620fae4987715d545936b770ac78df30d0b8501120fd3ae5e3abb1a61617"],
            "frontend exit drain provenance drifted")
    require(drain["completed"] == drain["accepted"] == drain["result"] == 1
            and drain["contract_failure"] == 0,
            "frontend exit drain source-boundary probe failed")
    parent = drain["parent_call"]
    require([int(parent[key],16) for key in ("pc", "delay", "entry", "ra")] ==
            [0x8002F094,0x8002F098,0x800394D4,0x8002F09C]
            and parent["argument_count"] == 0 and parent["program"] == 1,
            "drain natural-caller boundary contract drifted")
    dp = drain["drain"]
    expected_drain_trace = (list(range(0x800394D4,0x80039510,4)) +
                            list(range(0x800394F0,0x80039500,4)) +
                            list(range(0x80039510,0x80039574,4)))
    require([dp[key] for key in ("operations", "accesses", "reads", "stores",
                                "callbacks", "instruction_count",
                                "poll_attempts", "zero_poll_results")] ==
            [15,7,4,3,8,44,2,1]
            and [int(pc,16) for pc in dp["instruction_trace"]] == expected_drain_trace
            and all(main_word(dp[key],value) for key,value in
                    (("initial_active_flag",1),("first_mode_flag",0x80145678),
                     ("second_mode_flag",0x80145678))),
            "drain poll loop or independent mode snapshots drifted")
    require({key:int(value,16) for key,value in drain["memory"].items()} ==
            {"active_before":1,"active_after":0,"busy_before":0xABCDEF01,
             "busy_after":0,"mode":0x80145678},
            "drain did not clear the retained active and busy flags")
    expected_drain_accesses = [
        (0x800394D8,0x800F84C4,1,1,1),
        (0x800394E4,0x801EFFF8,0x8002F09C,2,2),
        (0x80039514,0x8002149C,0x80145678,7,1),
        (0x8003951C,0x800F43B0,0,8,2),
        (0x80039524,0x800F84C4,0,9,2),
        (0x80039544,0x8002149C,0x80145678,12,1),
        (0x80039564,0x801EFFF8,0x8002F09C,15,1)]
    require(len(dp["access_journal"]) == 7, "drain access extent drifted")
    for actual,(pc,address,value,operation,kind) in zip(
            dp["access_journal"],expected_drain_accesses):
        require([int(actual[key],16) for key in ("pc","address","value")] ==
                [pc,address,value] and actual["operation"] == operation
                and actual["width"] == 4 and actual["known_mask"] == 15
                and actual["kind"] == kind, "drain ordered memory accesses drifted")
    expected_drain_calls = [
        (0x800394E8,0x800393F0,0,1),(0x800394F0,0x800392A0,0,1),
        (0x80039500,0x80038E84,0,1),(0x800394F0,0x800392A0,0,2),
        (0x80039530,0x80029B64,2,1),(0x80039538,0x8008C274,0,1),
        (0x80039554,0x8006CDE4,1,1),(0x8003955C,0x8006AE60,0,1)]
    require(len(dp["call_sequence"]) == 8, "drain call extent drifted")
    expected = [0] + [0x63000000+i*0x101 for i in range(1,32)]
    expected[29] = 0x801EFFE8
    for index,(actual,(pc,target,argc,invocation)) in enumerate(zip(
            dp["call_sequence"],expected_drain_calls)):
        expected[31] = pc+8
        expected[2] = 1 if index < 2 else (0 if index < 4 else 0x80145678)
        if index >= 4:
            expected[1],expected[4],expected[5] = 0x80100000,0,0
        if index >= 6:
            expected[4] = 0x80145678
        require([int(actual[key],16) for key in ("pc","delay","target")] ==
                [pc,pc+4,target] and actual["argument_count"] == argc
                and actual["invocation"] == invocation
                and wait_machine_matches(actual["machine"],expected),
                "drain full callback machine or delay-slot argument contract drifted")
    expected[29],expected[31] = 0x801F0000,0x8002F09C
    require(wait_machine_matches(drain["final_machine"],expected),
            "drain did not preserve its full return machine")
    drain_frames = ["frontend-exit-drain-before","frontend-exit-drain-after"]
    require(all(name in by_id and by_id[name]["page"] == "User Setup"
                for name in drain_frames), "drain native boundary frames missing")
    drain_hashes = [ppm_hash(args.frames/f"{name}.ppm") for name in drain_frames]
    require(drain_hashes[0] == drain_hashes[1] == wait_hashes[1],
            "CPU-only drain probe changed native User Setup pixels")
    db = drain["next_unbound_boundary"]
    require(drain["classification"] == "no direct visual effect"
            and drain["gameplay_shown"] == "BLOCKED"
            and "synthetic" in drain["fixture_contract"].lower()
            and "8007B844" in db["earliest_production"]
            and all(pc in db["first_drain_child"] for pc in ("800394E8","800393F0")),
            "drain receipt conceals fixtures or its next unbound child")
    (args.frames/"frontend_exit_drain_verified.json").write_text(
        json.dumps({"program":"FEONLY","address":"0x800394D4","end":"0x80039573",
                    "driver_frame_count":len(states),"source_receipt":drain,
                    "before_sha256":drain_hashes[0],"after_sha256":drain_hashes[1],
                    "visual_status":"Gameplay shown: BLOCKED","reason":db},indent=2)+"\n",
        encoding="utf-8")

    clock_read = read_json(args.frames / "frontend_clock_read_trace.json")
    require([clock_read[key] for key in ("program", "address", "inclusive_end",
             "bytes", "instructions", "source_sha256")] ==
            ["FEONLY", "0x8008da5c", "0x8008da6b", 16, 4,
             "9bf283cf0c65c4bd13e3e94df28927dc756088764e78bf2e59298f9faeef85c0"],
            "frontend clock reader provenance drifted")
    require(clock_read["completed"] == clock_read["result"] == 1
            and clock_read["contract_failure"] == 0,
            "composed frontend wait/clock reader probe failed")
    require({key:int(value,16) for key,value in clock_read["clock_memory"].items()} ==
            {"address":0x800D9AB8,"before":1000,"after_fixture_update":1361},
            "composed clock reader did not observe the live fixture update")
    cr_wait, cr = clock_read["wait"], clock_read["clock"]
    require([cr_wait[key] for key in ("operations", "accesses", "callbacks",
                                     "clock_callbacks", "instruction_count")] ==
            [19,9,10,2,50]
            and [int(pc,16) for pc in cr_wait["instruction_trace"]] ==
            list(range(0x8002EFBC,0x8002F084,4))
            and [int(pc,16) for pc in cr["reader_instruction_trace"]] ==
            list(range(0x8008DA5C,0x8008DA6C,4)),
            "wait or clock source instruction trace drifted")
    require(len(cr_wait["access_journal"]) == 9,
            "composed wait access extent drifted")
    for actual,(pc,address,value,operation,kind) in zip(
            cr_wait["access_journal"],expected_wait_accesses):
        mask = 5 if pc in (0x8002EFD4,0x8002F074) else 15
        require([int(actual[key],16) for key in ("pc","address","value")] ==
                [pc,address,value] and actual["operation"] == operation
                and actual["width"] == 4 and actual["known_mask"] == mask
                and actual["kind"] == kind,
                "clock composition changed wait memory order or saved knownness")
    def clock_machine_matches(machine, gpr, s0_mask):
        return (len(machine["gpr"]) == 32 and
                all(word == {"word":f"0x{value:08x}",
                             "known_mask":s0_mask if index == 16 else 15}
                    for index,(word,value) in enumerate(zip(machine["gpr"],gpr)))
                and machine["hi"] == {"word":"0x10203040","known_mask":6}
                and machine["lo"] == {"word":"0x50607080","known_mask":9})
    require(len(cr_wait["call_sequence"]) == 10,
            "composed wait call extent drifted")
    expected = [0] + [0x63000000+i*0x101 for i in range(1,32)]
    expected[4],expected[5],expected[6] = 0x80145678,100,0xFFFFFFFF
    expected[16],expected[29] = 0x11223344,0x801EFFE8
    for index,(actual,(pc,target,argc)) in enumerate(zip(
            cr_wait["call_sequence"],expected_wait_calls)):
        expected[31] = pc+8
        expected[2] = 1000 if index == 2 else (1 if index == 6 else 0xFFFFFFFF)
        if index >= 2:
            expected[16] = 1360
        if index >= 7:
            expected[1] = 0x80010000
        if index == 9:
            expected[4] = 0x80123458
        mask = 5 if index < 2 else 15
        require([int(actual[key],16) for key in ("pc","delay","target")] ==
                [pc,pc+4,target] and actual["argument_count"] == argc
                and actual["invocation"] == 1
                and clock_machine_matches(actual["machine"],expected,mask),
                "composed wait callback machine or arguments drifted")
        if index in (1,5):
            reader = cr["initial" if index == 1 else "loop"]
            value = 1000 if index == 1 else 1361
            returned = expected.copy()
            returned[2] = value
            require(int(reader["parent_pc"],16) == pc
                    and reader["result"] == reader["operations"] == 1
                    and reader["instruction_count"] == 4
                    and main_word(reader["loaded_clock"],value)
                    and clock_machine_matches(reader["final_machine"],returned,mask)
                    and reader["access"] ==
                    {"pc":"0x8008da60","address":"0x800d9ab8",
                     "value":f"0x{value:08x}","operation":1,"width":4,
                     "known_mask":15,"kind":1},
                    "reader did not replace only V0 with its actual retained read")
    expected[1],expected[16],expected[29],expected[31] = (
        0x80020000,0x11223344,0x801F0000,0x8002F094)
    require(clock_machine_matches(clock_read["final_machine"],expected,5),
            "composed clock reader lost wait return state")
    clock_frames = ["frontend-clock-read-before","frontend-clock-read-after"]
    require(all(name in by_id and by_id[name]["page"] == "User Setup"
                for name in clock_frames), "clock reader boundary frames missing")
    clock_hashes = [ppm_hash(args.frames/f"{name}.ppm") for name in clock_frames]
    require(clock_hashes[0] == clock_hashes[1] == drain_hashes[1],
            "CPU-only clock reader composition changed native pixels")
    crb = clock_read["next_unbound_boundary"]
    require(clock_read["classification"] == "no direct visual effect"
            and clock_read["gameplay_shown"] == "BLOCKED"
            and "synthetic" in clock_read["fixture_contract"].lower()
            and "8007B844" in crb["earliest_production"]
            and "8007B2BC" in crb["next_wait_child"],
            "clock receipt conceals synthetic entry state or production dependencies")
    (args.frames/"frontend_clock_read_verified.json").write_text(
        json.dumps({"program":"FEONLY","address":"0x8008DA5C","end":"0x8008DA6B",
                    "driver_frame_count":len(states),"source_receipt":clock_read,
                    "before_sha256":clock_hashes[0],"after_sha256":clock_hashes[1],
                    "visual_status":"Gameplay shown: BLOCKED","reason":crb},indent=2)+"\n",
        encoding="utf-8")

    io_drain = read_json(args.frames / "frontend_io_drain_trace.json")
    require(io_drain["program"] == "FEONLY" and int(io_drain["address"],16) == 0x800393f0
            and int(io_drain["inclusive_end"],16) == 0x800394d3
            and io_drain["bytes"] == 228 and io_drain["instructions"] == 57
            and io_drain["source_sha256"] == "ddd6a228f2ddfecfebe23641b1c36c549e82172f38dfe659484b2d9e521ea50c"
            and io_drain["completed"] == io_drain["accepted"] == io_drain["result"] == 1
            and not io_drain["contract_failure"], "I/O drain source/completion receipt drifted")
    require(io_drain["parent_call"] == {"pc":"0x800394e8","delay":"0x800394ec",
            "entry":"0x800393f0","argument_count":0,"program":1,"ra":"0x800394f0"}
            and io_drain["status_fixture"] == [3,1,4,5,-1,0,2,6], "I/O drain fixture boundary drifted")
    iod = io_drain["drain"]
    require([iod[k] for k in ("operations","accesses","reads","stores","callbacks",
            "slot_iterations","poll_attempts","zero_poll_results","instruction_count")]
            == [24,20,12,8,4,8,2,1,164], "I/O drain execution counts drifted")
    iod_trace = list(range(0x800393f0,0x80039408,4))
    for status in (3,1,4,5,-1,0,2,6):
        iod_trace += list(range(0x80039408,0x80039420,4))
        if status == 3:
            iod_trace += list(range(0x8003944c,0x80039480,4))
        else:
            iod_trace += [0x80039420,0x80039424]
            if status < 4:
                iod_trace += [0x80039428,0x8003942c]
                iod_trace += (list(range(0x8003946c,0x80039480,4)) if status == 1
                              else [0x80039430,0x80039434])
            else:
                iod_trace += [0x80039438,0x8003943c,0x80039440]
                if status < 6:
                    iod_trace += [0x80039444,0x80039448,0x80039480,0x80039484,0x80039488]
                iod_trace += [0x8003948c]
        iod_trace += [0x80039490,0x80039494,0x80039498]
    iod_trace += list(range(0x8003949c,0x800394bc,4))
    iod_trace += list(range(0x8003949c,0x800394ac,4)) + list(range(0x800394bc,0x800394d4,4))
    require([int(pc,16) for pc in iod["instruction_trace"]] == iod_trace
            and len(set(iod_trace)) == 57, "I/O drain complete source PC path drifted")
    # Source-ordered frame, slot and restore accesses for the generated status fixture.
    iod_accesses = [
        (0x800393f4,0x801efff4,0x63001111,1,2),(0x800393fc,0x801efff0,0x63001010,2,2),
        (0x80039404,0x801efff8,0x800394f0,3,2),(0x80039410,0x800ef840,3,4,1),
        (0x80039454,0x800ef844,0x80145678,5,1),(0x80039468,0x800ef844,0,7,2),
        (0x80039474,0x800ef840,0,8,2),(0x80039410,0x800ef864,1,9,1),
        (0x80039474,0x800ef864,0,10,2),(0x80039410,0x800ef888,4,11,1),
        (0x80039488,0x800ef878,0,12,2),(0x80039410,0x800ef8ac,5,13,1),
        (0x80039488,0x800ef89c,0,14,2),(0x80039410,0x800ef8d0,0xffffffff,15,1),
        (0x80039410,0x800ef8f4,0,16,1),(0x80039410,0x800ef918,2,17,1),
        (0x80039410,0x800ef93c,6,18,1),(0x800394bc,0x801efff8,0x800394f0,22,1),
        (0x800394c0,0x801efff4,0x63001111,23,1),(0x800394c4,0x801efff0,0x63001010,24,1)]
    require(len(iod["access_journal"]) == len(iod_accesses), "I/O drain access length drifted")
    for actual, (pc,address,value,operation,kind) in zip(iod["access_journal"],iod_accesses):
        require([int(actual[k],16) for k in ("pc","address","value")] == [pc,address,value]
                and [actual[k] for k in ("operation","kind","width","known_mask")]
                == [operation,kind,4,15], "I/O drain ordered access drifted")
    def iod_machine(machine, expected):
        return (len(machine["gpr"]) == 32 and all(int(w["word"],16) == v and w["known_mask"] == 15
                for w,v in zip(machine["gpr"],expected))
                and machine["hi"] == {"word":"0x10203040","known_mask":5}
                and machine["lo"] == {"word":"0x50607080","known_mask":10})
    iod_gpr = [0x63000000+i*0x101 for i in range(32)]
    iod_gpr[0]=0; iod_gpr[1]=0x800f0000; iod_gpr[2]=1; iod_gpr[3]=3
    iod_gpr[4]=0x80145678; iod_gpr[16]=0; iod_gpr[17]=0; iod_gpr[29]=0x801effe0
    iod_calls = [(0x80039458,0x80077638,1,1),(0x8003949c,0x800392a0,0,1),
                 (0x800394ac,0x80038e84,0,1),(0x8003949c,0x800392a0,0,2)]
    require(len(iod["call_sequence"]) == 4, "I/O drain callback count drifted")
    for index,(actual,(pc,target,argc,invocation)) in enumerate(zip(iod["call_sequence"],iod_calls)):
        if index:
            iod_gpr[1]=0x800f00fc; iod_gpr[2]=0; iod_gpr[3]=6; iod_gpr[16]=288; iod_gpr[17]=8
        iod_gpr[31]=pc+8
        require([int(actual[k],16) for k in ("pc","delay","target")] == [pc,pc+4,target]
                and actual["argument_count"] == argc and actual["invocation"] == invocation
                and iod_machine(actual["machine"],iod_gpr), "I/O drain exact callback machine drifted")
    iod_gpr[2]=1; iod_gpr[16]=0x63001010; iod_gpr[17]=0x63001111
    iod_gpr[29]=0x801f0000; iod_gpr[31]=0x800394f0
    require(iod_machine(io_drain["final_machine"],iod_gpr), "I/O drain live epilogue drifted")
    iod_frames = ("frontend-io-drain-before","frontend-io-drain-after")
    require(all(n in by_id and by_id[n]["page"] == "User Setup" for n in iod_frames), "I/O drain native frames absent")
    iod_hashes = [ppm_hash(args.frames/f"{n}.ppm") for n in iod_frames]
    require(iod_hashes[0] == iod_hashes[1] == clock_hashes[1], "CPU I/O drain changed pixels")
    require(io_drain["gameplay_shown"] == "BLOCKED" and io_drain["classification"] == "no direct visual effect"
            and "Synthetic standalone" in io_drain["fixture_contract"]
            and "80077638" in io_drain["next_unbound_boundary"]["io_children"],
            "I/O drain conceals standalone fixtures or remaining dependencies")
    (args.frames/"frontend_io_drain_verified.json").write_text(json.dumps({
        "program":"FEONLY","address":"0x800393F0","end":"0x800394D3",
        "driver_frame_count":len(states),"source_receipt":io_drain,
        "before_sha256":iod_hashes[0],"after_sha256":iod_hashes[1],
        "visual_status":"Gameplay shown: BLOCKED","reason":io_drain["next_unbound_boundary"]},indent=2)+"\n",encoding="utf-8")

    io_complete = read_json(args.frames/"frontend_io_complete_trace.json")
    require(io_complete["program"] == "FEONLY" and int(io_complete["address"],16) == 0x800392a0
            and int(io_complete["inclusive_end"],16) == 0x800392f7
            and [io_complete[k] for k in ("bytes","instructions","completed","result","contract_failure")]
            == [88,22,1,1,0] and io_complete["source_sha256"] ==
            "dca1d4f4bf2b7847a1175abe703ab434c4ed51efccb2af341b536de466f98d7a", "I/O completion source receipt drifted")
    icd=io_complete["drain"]
    require([icd[k] for k in ("operations","accesses","callbacks","poll_invocations","instruction_count")]
            == [15,7,8,2,44] and [int(pc,16) for pc in icd["instruction_trace"]] == expected_drain_trace,
            "I/O completion natural parent path drifted")
    require(io_complete["status_memory"] == {"active_before":"0x00000001","active_after":"0x00000000",
            "busy_before":"0xabcdef01","busy_after":"0x00000000","slot0_before":"0x80000000",
            "slot0_after":"0x00000000","final_slots":["0x00000000"]*8}, "I/O completion retained state drifted")
    require(len(icd["access_journal"]) == 7, "I/O completion parent access extent drifted")
    for actual,(pc,address,value,operation,kind) in zip(icd["access_journal"],expected_drain_accesses):
        require([int(actual[k],16) for k in ("pc","address","value")] == [pc,address,value]
                and [actual[k] for k in ("operation","kind","width","known_mask")]
                == [operation,kind,4,15], "I/O completion parent access order drifted")
    ic_gpr=[0]+[0x73000000+i*0x101 for i in range(1,32)]
    ic_gpr[29]=0x801effe8
    require(len(icd["call_sequence"]) == 8, "I/O completion parent call extent drifted")
    for index,(actual,(pc,target,argc,invocation)) in enumerate(zip(icd["call_sequence"],expected_drain_calls)):
        ic_gpr[31]=pc+8
        if index < 2: ic_gpr[2]=1
        if index in (2,3): ic_gpr[1]=0x800f0000; ic_gpr[2]=0; ic_gpr[3]=0; ic_gpr[4]=1
        if index >= 4:
            ic_gpr[1]=0x80100000;ic_gpr[2]=0x80145678;ic_gpr[3]=288;ic_gpr[4]=0;ic_gpr[5]=0
        if index >= 6: ic_gpr[4]=0x80145678
        require([int(actual[k],16) for k in ("pc","delay","target")] == [pc,pc+4,target]
                and actual["argument_count"] == argc and actual["invocation"] == invocation
                and actual["operation"] == [3,4,5,6,10,11,13,14][index]
                and iod_machine(actual["machine"],ic_gpr), "I/O completion parent full machine drifted")
    records=io_complete["io_complete"]["records"]
    require(io_complete["io_complete"]["invocations"] == len(records) == 2, "Actual poll records missing")
    for index,record in enumerate(records):
        parent_machine=icd["call_sequence"][1 if index == 0 else 3]["machine"]
        trace=list(range(0x800392a0,0x800392b4,4))+[0x800392c4]
        if index == 0:
            trace+=list(range(0x800392c8,0x800392e0,4))+[0x800392bc,0x800392c0,0x800392f0,0x800392f4]
        else:
            trace+=list(range(0x800392c8,0x800392ec,4))*8+[0x800392ec,0x800392f0,0x800392f4]
        count=2 if index == 0 else 9
        require(record["parent_pc"] == "0x800394f0" and record["invocation"] == index+1
                and record["result"] == 1 and record["operations"] == record["accesses"] == count
                and record["status_reads"] == count-1 and record["instruction_count"] == len(trace)
                and record["parent_machine"] == parent_machine
                and [int(pc,16) for pc in record["instruction_trace"]] == trace
                and main_word(record["active_word"],1)
                and main_word(record["last_status"],0x80000000 if index == 0 else 0),
                "Actual poll control flow or input machine drifted")
        expected_accesses=[(0x800392a4,0x800f84c4,1)]+[(0x800392d0,0x800ef840+i*36,
                            0x80000000 if index == 0 else 0) for i in range(count-1)]
        require(len(record["access_journal"]) == count, "Poll access receipt truncated")
        for op,(actual,(pc,address,value)) in enumerate(zip(record["access_journal"],expected_accesses),1):
            require([int(actual[k],16) for k in ("pc","address","value")] == [pc,address,value]
                    and [actual[k] for k in ("operation","kind","width","known_mask")]
                    == [op,1,4,15], "Actual completion poll memory read drifted")
        returned=[int(w["word"],16) for w in parent_machine["gpr"]]
        returned[1]=0x800f0000 if index == 0 else 0x800f00fc
        returned[2]=index;returned[3]=0 if index == 0 else 288;returned[4]=1 if index == 0 else 8
        require(iod_machine(record["final_machine"],returned), "Actual completion poll live return drifted")
    ic_gpr[29]=0x801f0000;ic_gpr[31]=0x8002f09c
    require(iod_machine(io_complete["final_machine"],ic_gpr), "Completion composition epilogue drifted")
    ic_frames=("frontend-io-complete-before","frontend-io-complete-after")
    require(all(n in by_id and by_id[n]["page"] == "User Setup" for n in ic_frames), "Completion frames missing")
    ic_hashes=[ppm_hash(args.frames/f"{n}.ppm") for n in ic_frames]
    require(ic_hashes[0] == ic_hashes[1] == iod_hashes[1], "Completion CPU-only composition changed pixels")
    require(io_complete["gameplay_shown"] == "BLOCKED" and io_complete["classification"] == "no direct visual effect"
            and "Synthetic standalone" in io_complete["fixture_contract"]
            and "80038E84" in io_complete["next_unbound_boundary"]["after_poll"], "Completion receipt conceals fixture dependencies")
    (args.frames/"frontend_io_complete_verified.json").write_text(json.dumps({
        "program":"FEONLY","address":"0x800392A0","end":"0x800392F7","driver_frame_count":len(states),
        "source_receipt":io_complete,"before_sha256":ic_hashes[0],"after_sha256":ic_hashes[1],
        "visual_status":"Gameplay shown: BLOCKED","reason":io_complete["next_unbound_boundary"]},indent=2)+"\n",encoding="utf-8")

    overlay_load=read_json(args.frames/"frontend_overlay_load_trace.json")
    require(overlay_load["program"] == "FEONLY" and int(overlay_load["address"],16) == 0x8007b11c
            and int(overlay_load["inclusive_end"],16) == 0x8007b13b
            and [overlay_load[k] for k in ("bytes","instructions","accepted","result","completed","contract_failure")]
            == [32,8,1,1,1,0] and overlay_load["source_sha256"] ==
            "97d8f0e4eb51bd581d1431e5995abb4ea56b67408568f334d91a8b93e61029e2", "Overlay-load source receipt drifted")
    require(overlay_load["parent_call"] == {"pc":"0x80028acc","delay":"0x80028ad0","entry":"0x8007b11c",
            "argument_count":2,"program":1,"a0":{"word":"0x80024854","known_mask":15},
            "a1":{"word":"0x00000000","known_mask":15}}, "Overlay-load natural boundary drifted")
    olp=overlay_load["owner"]
    require([olp[k] for k in ("operations","accesses","reads","stores","callbacks","instruction_count")]
            == [3,2,1,1,1,8] and main_word(olp["delay_a2"],1)
            and main_word(olp["child_return"],0x80170000)
            and [int(pc,16) for pc in olp["instruction_trace"]] == list(range(0x8007b11c,0x8007b13c,4)),
            "Overlay-load full instruction/delay prefix drifted")
    require(len(olp["access_journal"]) == 2, "Overlay-load stack access receipt truncated")
    for actual,(pc,operation,kind) in zip(olp["access_journal"],[(0x8007b120,1,2),(0x8007b12c,3,1)]):
        require([int(actual[k],16) for k in ("pc","address","value")] == [pc,0x801efff8,0x80028ad4]
                and [actual[k] for k in ("operation","width","known_mask","kind")]
                == [operation,4,15,kind], "Overlay-load exact frame access drifted")
    olg=[0]+[0x61000000+i*0x101 for i in range(1,32)]
    olg[4]=0x80024854;olg[5]=0;olg[6]=1;olg[29]=0x801effe8;olg[31]=0x8007b12c
    require(len(olp["call_sequence"]) == 1, "Overlay-load child count drifted")
    olc=olp["call_sequence"][0]
    require([int(olc[k],16) for k in ("pc","delay","target")] == [0x8007b124,0x8007b128,0x8007b15c]
            and olc["argument_count"] == 3 and olc["invocation"] == 1 and iod_machine(olc["machine"],olg),
            "Overlay-load child full CPU and transitive argument contract drifted")
    olg[2]=0x80170000;olg[29]=0x801f0000;olg[31]=0x80028ad4
    require(iod_machine(overlay_load["final_machine"],olg), "Overlay-load callback-live epilogue drifted")
    olframes=("frontend-overlay-load-before","frontend-overlay-load-after")
    require(all(n in by_id and by_id[n]["page"] == "User Setup" for n in olframes), "Overlay-load native frames missing")
    olhashes=[ppm_hash(args.frames/f"{n}.ppm") for n in olframes]
    require(olhashes[0] == olhashes[1] == ic_hashes[1], "CPU-only overlay-load changed pixels")
    require(overlay_load["gameplay_shown"] == "BLOCKED" and overlay_load["classification"] == "no direct visual effect"
            and "synthetic standalone" in overlay_load["fixture_contract"]
            and "8007B15C" in overlay_load["next_unbound_boundary"]["loader_child"], "Overlay-load fixture/dependency contract absent")
    (args.frames/"frontend_overlay_load_verified.json").write_text(json.dumps({
        "program":"FEONLY","address":"0x8007B11C","end":"0x8007B13B","driver_frame_count":len(states),
        "source_receipt":overlay_load,"before_sha256":olhashes[0],"after_sha256":olhashes[1],
        "visual_status":"Gameplay shown: BLOCKED","reason":overlay_load["next_unbound_boundary"]},indent=2)+"\n",encoding="utf-8")

    load_payload=read_json(args.frames/"frontend_load_payload_trace.json")
    require(load_payload["program"] == "FEONLY" and int(load_payload["address"],16) == 0x8007b15c
            and int(load_payload["inclusive_end"],16) == 0x8007b18f
            and [load_payload[k] for k in ("bytes","instructions","completed","contract_failure")] == [52,13,1,0]
            and load_payload["source_sha256"] == "aaf6935467d7d6bad48e084fafaf71528d7b8e6ebb23deca4bef4e2f2f9b3ebf",
            "Loaded-payload source receipt drifted")
    def lp_word(value,mask=15):
        return {"word":f"0x{value:08x}","known_mask":mask}
    def lp_machine(sp,ra,v0=None):
        words=[0]+[0x75000000+i*0x101 for i in range(1,32)]
        words[4]=0x80024854;words[5]=0;words[6]=1;words[29]=sp;words[31]=ra
        result={"gpr":[lp_word(w) for w in words],"hi":lp_word(0x10203040,6),"lo":lp_word(0x50607080,9)}
        if v0 is not None:result["gpr"][2]=v0
        return result
    def lp_access(pc,address,value,operation,kind,mask=15):
        return {"pc":f"0x{pc:08x}","address":f"0x{address:08x}","value":f"0x{value:08x}",
                "operation":operation,"width":4,"known_mask":mask,"kind":kind}
    require(len(load_payload["paths"]) == 2,"Loaded-payload paths missing")
    for nonnull,path in enumerate(load_payload["paths"]):
        child=lp_word(0x80170000 if nonnull else 0)
        returned=lp_word(0x801e1410,7) if nonnull else lp_word(0)
        require(path["kind"] == ("nonnull" if nonnull else "null") and path["result"] == 1
                and path["descriptor"] == {"address":"0x80170000","word":"0x801e1410","known_mask":7}
                and path["parent_call"] == {"pc":"0x8007b124","delay":"0x8007b128","target":"0x8007b15c",
                    "argument_count":3,"machine":lp_machine(0x801effe8,0x8007b12c)}, "Payload natural boundary drifted")
        parent=path["overlay_load"];owner=path["load_payload"]
        require([parent[k] for k in ("operations","accesses","callbacks","instruction_count")] == [3,2,1,8]
                and parent["child_return"] == returned
                and parent["instruction_trace"] == [f"0x{pc:08x}" for pc in range(0x8007b11c,0x8007b13c,4)]
                and parent["access_journal"] == [lp_access(0x8007b120,0x801efff8,0x80028ad4,1,2),
                    lp_access(0x8007b12c,0x801efff8,0x80028ad4,3,1)], "Payload actual parent receipt drifted")
        expected_trace=list(range(0x8007b15c,0x8007b174,4))+([0x8007b17c] if nonnull else [0x8007b174,0x8007b178])+list(range(0x8007b180,0x8007b190,4))
        expected_access=[lp_access(0x8007b160,0x801effe0,0x8007b12c,1,2)]
        if nonnull:expected_access.append(lp_access(0x8007b17c,0x80170000,0x801e1410,3,1,7))
        expected_access.append(lp_access(0x8007b180,0x801effe0,0x8007b12c,4 if nonnull else 3,1))
        require([owner[k] for k in ("operations","accesses","reads","stores","callbacks","instruction_count")]
                == ([4,3,2,1,1,11] if nonnull else [3,2,1,1,1,12])
                and owner["forwarded_a0"] == lp_word(0x80024854) and owner["forwarded_a1"] == lp_word(0)
                and owner["forwarded_a2"] == lp_word(1) and owner["child_return"] == child
                and owner["payload_result"] == returned
                and [int(pc,16) for pc in owner["instruction_trace"]] == expected_trace
                and owner["access_journal"] == expected_access,"Payload full source path or memory order drifted")
        require(owner["call_sequence"] == [{"pc":"0x8007b164","delay":"0x8007b168","target":"0x8007b1d0",
                    "argument_count":3,"invocation":1,"machine":lp_machine(0x801effd0,0x8007b16c)}]
                and owner["final_machine"] == lp_machine(0x801effe8,0x8007b12c,returned)
                and path["final_machine"] == lp_machine(0x801f0000,0x80028ad4,returned),
                "Payload full callback-live machine drifted")
    lpframes=("frontend-load-payload-before","frontend-load-payload-after")
    require(all(n in by_id and by_id[n]["page"] == "User Setup" for n in lpframes),"Payload native frames missing")
    lphashes=[ppm_hash(args.frames/f"{n}.ppm") for n in lpframes]
    require(lphashes[0] == lphashes[1] == olhashes[1],"Payload CPU composition changed pixels")
    require(load_payload["gameplay_shown"] == "BLOCKED" and load_payload["classification"] == "no direct visual effect"
            and "synthetic standalone" in load_payload["fixture_contract"]
            and "8007B1D0" in load_payload["next_unbound_boundary"]["loader_child"],"Payload dependency disclosure absent")
    (args.frames/"frontend_load_payload_verified.json").write_text(json.dumps({
        "program":"FEONLY","address":"0x8007B15C","end":"0x8007B18F","driver_frame_count":len(states),
        "source_receipt":load_payload,"before_sha256":lphashes[0],"after_sha256":lphashes[1],
        "visual_status":"Gameplay shown: BLOCKED","reason":load_payload["next_unbound_boundary"]},indent=2)+"\n",encoding="utf-8")

    frontend_copy=read_json(args.frames/"frontend_memory_copy_trace.json")
    require(frontend_copy["program"] == "FEONLY" and frontend_copy["address"] == "0x800909A8"
            and frontend_copy["range"] == "0x800909A8..0x80090CC7"
            and frontend_copy["evidence_sha256"] == "589207dc7895ba0151f714f53c02c357959170daed411e652ca281ac7216ef4b"
            and [frontend_copy[k] for k in ("bytes","instructions","length","operations","access_events","reads","stores","instruction_events","contract_failure")]
            == [800,200,4096,2048,2048,1024,1024,2329,0],"Frontend copy source/count receipt drifted")
    require([frontend_copy[k] for k in ("main_result","main_stopped_pc","main_stopped_target","copy_result","copy_completed")]
            == [-5,0x80028b68,0x801e1410,1,1] and frontend_copy["source"] == "0x80140000"
            and frontend_copy["destination"] == "0x801E0000","Copy did not reach explicitly refused GAMELOAD boundary")
    def fc_hash(data):
        value=0xcbf29ce484222325
        for byte in data:value=((value^byte)*1099511628211)&0xffffffffffffffff
        return f"{value:016x}"
    def fc_le(value,width=4):return value.to_bytes(width,"little")
    require(frontend_copy["hash_algorithm"] == "FNV-1a-64" and frontend_copy["hash_seed"] == "0xcbf29ce484222325"
            and frontend_copy["access_hash_layout"] == "per event: le32 pc,address,logical_address,value; le64 operation; u8 width,known_mask,transfer_mask,kind"
            and frontend_copy["pc_hash_layout"] == "executed order: le32 pc per event"
            and frontend_copy["machine_hash_layout"] == "170 bytes: gpr0..gpr31,hi,lo; each le32 word then u8 known_mask",
            "Copy receipt canonical layouts drifted")
    fc_payload=bytearray((i*37+(i>>5)+11)&255 for i in range(4096));fc_payload[:4]=fc_le(0x801e1410)
    require(frontend_copy["source_hash"] == frontend_copy["destination_after_hash"] == fc_hash(fc_payload)
            and frontend_copy["destination_before_hash"] == fc_hash(bytes([0xa5])*4096),"Copy retained payload fingerprints drifted")
    fc_access=bytearray();fc_op=0
    for block in range(64):
        for half in range(2):
            for kind in (1,2):
                for index in range(8):
                    offset=block*64+half*32+index*4
                    pc=(0x800909cc if kind == 1 else 0x800909ec)+half*64+index*4
                    address=(0x80140000 if kind == 1 else 0x801e0000)+offset
                    value=int.from_bytes(fc_payload[offset:offset+4],"little");fc_op+=1
                    fc_access+=fc_le(pc)+fc_le(address)+fc_le(address)+fc_le(value)+fc_le(fc_op,8)+bytes([4,15,15,kind])
    fc_pcs=[0x800909a8,0x800909ac,0x800909b0,0x80090b9c,0x80090ba0,0x80090ba4,0x80090ba8]+list(range(0x800909b0,0x800909cc,4))
    fc_pcs+=list(range(0x800909cc,0x80090a5c,4))*64
    fc_pcs += [0x80090a5c,0x80090a60,0x80090a64,0x80090a98,0x80090a9c,0x80090aa0,0x80090abc,0x80090ac0,0x80090ac4,0x80090ae0,0x80090ae4]
    require(len(fc_pcs) == 2329 and frontend_copy["pc_hash"] == fc_hash(b"".join(fc_le(pc) for pc in fc_pcs))
            and frontend_copy["access_hash"] == fc_hash(fc_access),"Copy exact canonical PC/access journals drifted")
    fc_input=frontend_copy["copy_machine_input"];fc_output=frontend_copy["copy_machine_output"]
    for state,field,expected in [(fc_input,"input_cpu_hash","3549d5ad34747f25"),(fc_output,"output_cpu_hash","683dbb60493dd505")]:
        require(len(state["gpr"]) == 32 and all(w["known_mask"] == 15 for w in state["gpr"])
                and state["hi"] == {"word":0x12345678,"known_mask":5}
                and state["lo"] == {"word":0x9abcdef0,"known_mask":10},"Copy full machine or masks absent")
        encoded=b"".join(fc_le(w["word"])+bytes([w["known_mask"]]) for w in state["gpr"]+[state["hi"],state["lo"]])
        require(len(encoded) == 170 and fc_hash(encoded) == frontend_copy[field] == expected,"Copy canonical full CPU fingerprint drifted")
    require([fc_input["gpr"][i]["word"] for i in (0,4,5,6,31)] == [0,0x80140000,0x801e0000,4096,0x80028b5c],"Copy natural argument/RA boundary drifted")
    expected_words=[w["word"] for w in fc_input["gpr"]]
    for i,value in [(1,0),(2,0),(4,0x80141000),(5,0x801e1000),(6,0xffffffff),(7,0x80141000)]:expected_words[i]=value
    for i in range(8):expected_words[8+i]=int.from_bytes(fc_payload[4064+i*4:4068+i*4],"little")
    require([w["word"] for w in fc_output["gpr"]] == expected_words,"Copy path-dependent live registers drifted")
    fc_frames=("frontend-memory-copy-before","frontend-memory-copy-after")
    require(all(n in by_id and by_id[n]["page"] == "User Setup" for n in fc_frames),"Frontend copy native frames missing")
    fc_hashes=[ppm_hash(args.frames/f"{n}.ppm") for n in fc_frames]
    require(fc_hashes[0] == fc_hashes[1] == lphashes[1],"CPU-only frontend copy changed actual native pixels")
    require(frontend_copy["gameplay_shown"] == "BLOCKED" and "synthetic standalone" in frontend_copy["fixture_contract"]
            and "unbound GAMELOAD" in frontend_copy["fixture_contract"],"Frontend copy conceals fixture dependencies")
    (args.frames/"frontend_memory_copy_verified.json").write_text(json.dumps({
        "program":"FEONLY","address":"0x800909A8","end":"0x80090CC7","driver_frame_count":len(states),
        "source_receipt":frontend_copy,"before_sha256":fc_hashes[0],"after_sha256":fc_hashes[1],
        "visual_status":"Gameplay shown: BLOCKED","reason":"FEONLY 0x80028B68 dynamic GAMELOAD transfer and production filesystem/lifecycle remain unbound"},indent=2)+"\n",encoding="utf-8")

    resource_load=read_json(args.frames/"frontend_resource_load_trace.json")
    require(resource_load["program"] == "FEONLY" and int(resource_load["address"],16) == 0x8007b1d0
            and int(resource_load["inclusive_end"],16) == 0x8007b2bb
            and [resource_load[k] for k in ("bytes","instructions","result","completed","contract_failure")] == [236,59,1,1,0]
            and resource_load["source_sha256"] == "16756cd9554b869085b0f84eb6b2f1b9fe0931e7bb07f40c9f08ce90a3677c26",
            "Resource-load source receipt drifted")
    rlp=resource_load["owner"]
    require([rlp[k] for k in ("operations","accesses","reads","stores")] == [25,19,12,7]
            and [int(pc,16) for pc in rlp["instruction_trace"]] == list(range(0x8007b1d0,0x8007b2bc,4)),
            "Resource-load complete source path drifted")
    rl_access=[(0x8007b1d4,0x801effec,0x51001111,1,2,15),
        (0x8007b1dc,0x801efff4,0x51001313,2,2,15),(0x8007b1e4,0x801efff0,0x51001212,3,2,15),
        (0x8007b1ec,0x801efff8,0x8007b16c,4,2,15),(0x8007b1f4,0x801effe8,0x51001010,5,2,15),
        (0x8007b218,0x801effd0,1,7,2,15),(0x8007b21c,0x801effe0,4096,9,1,15),
        (0x8007b244,0x801effd8,0x44,11,1,15),(0x8007b248,0x80170000,0x55667788,12,1,9),
        (0x8007b24c,0x801effe0,4096,13,1,15),(0x8007b258,0x801effe0,4096,15,1,15),
        (0x8007b260,0x800d9ae8,4096,16,2,15),(0x8007b264,0x801effd8,0x44,17,1,15),
        (0x8007b274,0x800d9b50,0x80061000,19,1,15),(0x8007b29c,0x801efff8,0x8007b16c,21,1,15),
        (0x8007b2a0,0x801efff4,0x51001313,22,1,15),(0x8007b2a4,0x801efff0,0x51001212,23,1,15),
        (0x8007b2a8,0x801effec,0x51001111,24,1,15),(0x8007b2ac,0x801effe8,0x51001010,25,1,15)]
    require(rlp["access_journal"] == [lp_access(pc,address,value,op,kind,mask) for pc,address,value,op,kind,mask in rl_access],
            "Resource-load exact saved frame, fifth argument, descriptor and late global access drifted")
    rl_gpr=[lp_word(0)]+[lp_word(0x51000000+i*0x101) for i in range(1,32)]
    def rl_set(changes):
        for reg,value in changes.items():rl_gpr[reg]=lp_word(value) if isinstance(value,int) else value
    def rl_cpu():return {"gpr":[dict(w) for w in rl_gpr],"hi":lp_word(0x12345678,5),"lo":lp_word(0x9abcdef0,10)}
    rl_set({4:0x80024854,5:0,6:1,29:0x801effc0,31:0x8007b1f8,17:0x80024854,18:1,19:0})
    expected_rl_machines=[rl_cpu()]
    rl_set({2:0x80160000,16:0,5:0x801effd8,6:0x801effdc,7:0x801effe0,31:0x8007b21c})
    expected_rl_machines.append(rl_cpu())
    rl_set({5:4096,6:0,7:1,31:0x8007b238});expected_rl_machines.append(rl_cpu())
    rl_set({2:0x80170000,16:0x80170000,4:0x44,5:lp_word(0x55667788,9),6:4096,31:0x8007b258})
    expected_rl_machines.append(rl_cpu())
    rl_set({1:0x800e0000,2:4096,31:0x8007b270});expected_rl_machines.append(rl_cpu())
    rl_set({2:0x80061000,4:0x80170000,5:0x80024854,6:0,7:1,31:0x8007b294});expected_rl_machines.append(rl_cpu())
    rl_sites=[(0x8007b1f0,0x8008a2c8,1,6),(0x8007b214,0x8008a594,5,8),(0x8007b230,0x80077160,4,10),
        (0x8007b250,0x8008a810,3,14),(0x8007b268,0x8008a7b0,1,18),(0x8007b28c,0x80061000,4,20)]
    require(len(rlp["calls"]) == 6,"Resource-load callback journal truncated")
    for call,(pc,target,argc,operation),machine in zip(rlp["calls"],rl_sites,expected_rl_machines):
        require([int(call[k],16) for k in ("pc","delay","target")] == [pc,pc+4,target]
                and call["argument_count"] == argc and call["operation"] == operation and call["invocation"] == 1
                and call["program"] == 1 and call["machine"] == machine,"Resource-load exact callback full-machine boundary drifted")
    rl_set({2:lp_word(0x89abcdef,6),16:0x51001010,17:0x51001111,18:0x51001212,19:0x51001313,
            29:0x801f0000,31:0x8007b16c})
    require(resource_load["final_machine"] == rl_cpu(),"Resource-load callback-live result and restore drifted")
    rlframes=("frontend-resource-load-before","frontend-resource-load-after")
    require(all(n in by_id and by_id[n]["page"] == "User Setup" for n in rlframes),"Resource-load native frames missing")
    rlhashes=[ppm_hash(args.frames/f"{n}.ppm") for n in rlframes]
    require(rlhashes[0] == rlhashes[1] == fc_hashes[1],"Resource-load CPU probe changed native pixels")
    require(resource_load["gameplay_shown"] == "BLOCKED" and resource_load["classification"] == "no direct visual effect"
            and "standalone" in resource_load["fixture_contract"] and "8008A2C8" in resource_load["next_unbound_boundary"],
            "Resource-load standalone fixture and next service disclosure absent")
    (args.frames/"frontend_resource_load_verified.json").write_text(json.dumps({
        "program":"FEONLY","address":"0x8007B1D0","end":"0x8007B2BB","driver_frame_count":len(states),
        "source_receipt":resource_load,"before_sha256":rlhashes[0],"after_sha256":rlhashes[1],
        "visual_status":"Gameplay shown: BLOCKED","reason":resource_load["next_unbound_boundary"]},indent=2)+"\n",encoding="utf-8")

    ge = json.loads((args.frames/"gameload_entry_trace.json").read_text())
    require(ge["program"] == "GAMELOAD" and ge["range"] == "801E1410-801E14B7" and ge["bytes"] == 168 and ge["instructions"] == 42
            and ge["source_sha256"] == "86de52922bd45fe1e8c5dd5768bb04d31a1a1ba8d0c9bc429d8a53b1919ae560", "GAMELOAD entry source identity drifted")
    require(ge["contract_failure"] == 0 and ge["result"] == -6 and ge["completed"] == 0 and ge["transferred"] == 0 and ge["trapped"] == 1
            and ge["stopped_pc"] == 0x801e14b4 and ge["gameplay_shown"] == "BLOCKED", "GAMELOAD entry lost honest terminal BREAK")
    require(ge["counts"] == dict(operations=2081, accesses=2079, reads=3, stores=2076, words_cleared=2073, callbacks=2, pc_events=10402), "GAMELOAD entry counts drifted")
    ge_pcs=list(range(0x801e1410,0x801e1420,4))+list(range(0x801e1420,0x801e1434,4))*2073+list(range(0x801e1434,0x801e14b8,4))
    require(ge["pc_hash_fnv1a64"] == int(fc_hash(b"".join(fc_le(pc) for pc in ge_pcs)),16), "GAMELOAD entry full PC journal drifted")
    ge_access=[(0x801e1420,0x801e903c+4*i,0,i+1,2) for i in range(2073)]
    ge_access += [(0x801e1438,0x801e8b70,0x800000,2074,1),(0x801e1460,0x801e8b6c,0x8000,2075,1),
                  (0x801e1474,0x801e8b50,0x60cf58,2076,2),(0x801e1480,0x801e8b4c,0x801eb0a0,2077,2),
                  (0x801e1488,0x801e903c,0x80028b70,2078,2),(0x801e14a4,0x801e903c,0x80028b70,2080,1)]
    ge_bytes=b"".join(fc_le(pc)+fc_le(address)+fc_le(value)+fc_le(op,8)+bytes([4,15,kind]) for pc,address,value,op,kind in ge_access)
    require(ge["access_hash_fnv1a64"] == int(fc_hash(ge_bytes),16), "GAMELOAD entry full access journal drifted")
    ge_samples=[]
    for i in (0,2072,2073,2078):
        pc,address,value,op,kind=ge_access[i]
        ge_samples.append(dict(index=i,pc=pc,address=address,value=value,operation=op,kind=kind,known_mask=15))
    require(ge["access_samples"] == ge_samples, "GAMELOAD entry boundary samples drifted")
    ge_words=[0x46000000+i*0x010203 for i in range(32)];ge_masks=[i%15+1 for i in range(32)]
    for i,value in {0:0,1:0x801f0000,2:0x7ffff8,3:0x8000,4:0x801eb0a4,5:0x60cf58,8:0x80000000,28:0x801e903c,29:0x807ffff8,30:0x807ffff8,31:0x801e14a0}.items():
        ge_words[i]=value;ge_masks[i]=15
    def ge_machine():return dict(gpr_words=ge_words.copy(),gpr_known_masks=ge_masks.copy(),hi=dict(word=0x12345678,known_mask=5),lo=dict(word=0x90abcdef,known_mask=10))
    ge_calls=[dict(pc=0x801e1498,target=0x801e1590,delay=0x801e149c,argument_count=2,invocation=1,operation=2079,outcome="RETURNED",machine=ge_machine())]
    ge_words[31]=0x801e14b4
    ge_calls.append(dict(pc=0x801e14ac,target=0x801e136c,delay=0x801e14b0,argument_count=0,invocation=1,operation=2081,outcome="RETURNED",machine=ge_machine()))
    require(ge["call_sequence"] == ge_calls and ge["final_machine"] == ge_machine(), "GAMELOAD entry full CPU transport drifted")
    require(ge["memory"] == dict(bss_first=0x80028b70,bss_last=0,heap_size=0x60cf58,heap_base=0x801eb0a0)
            and "Synthetic standalone" in ge["fixture_contract"] and "preserving every register" in ge["fixture_contract"]
            and "801E1498->801E1590" in ge["next_unbound_boundary"], "GAMELOAD entry memory/fixture contract drifted")
    ge_frames=["gameload-entry-before","gameload-entry-after"]
    require(all(n in by_id for n in ge_frames), "GAMELOAD entry native frames missing")
    ge_hashes=[ppm_hash(args.frames/f"{n}.ppm") for n in ge_frames]
    require(ge_hashes[0] == ge_hashes[1] == rlhashes[1], "GAMELOAD entry CPU probe changed native pixels")
    (args.frames/"gameload_entry_verified.json").write_text(json.dumps(dict(program="GAMELOAD",address="0x801E1410",end="0x801E14B7",driver_frame_count=len(states),source_receipt=ge,before_sha256=ge_hashes[0],after_sha256=ge_hashes[1],gameplay_shown="BLOCKED"),indent=2))

    lookup=json.loads((args.frames/"frontend_resource_lookup_trace.json").read_text())
    require(lookup["program"] == "FEONLY" and lookup["address"] == "0x8008a2c8" and lookup["inclusive_end"] == "0x8008a407"
            and lookup["bytes"] == 320 and lookup["instructions"] == 80 and lookup["source_sha256"] == "2bc268004a25001f37dc4a8df569c9a94b5dea9e5253ab3533dbc18e08df00d1", "Resource lookup identity drifted")
    require(lookup["result"] == 1 and lookup["completed"] == 1 and lookup["contract_failure"] == 0 and lookup["gameplay_shown"] == "BLOCKED", "Resource lookup outcome drifted")
    lk=lookup["owner"]
    require([lk[n] for n in ("operations","accesses","reads","stores")] == [20,16,10,6], "Resource lookup counts drifted")
    require(lk["instruction_trace"] == [f"0x{pc:08x}" for pc in list(range(0x8008a2c8,0x8008a328,4))+list(range(0x8008a330,0x8008a354,4))+list(range(0x8008a3e8,0x8008a408,4))], "Resource lookup exact PC trace drifted")
    lkregs=[lp_word(0x71000000+i*0x101) for i in range(32)]
    for i,v in {0:0,4:0x80024854,29:0x801f0000,31:0x8007b1f8}.items():lkregs[i]=lp_word(v)
    lkentry=[w.copy() for w in lkregs]
    lkframe=0x801effd8
    lkaccess=[lp_access(pc,lkframe+offset,int(lkentry[reg]["word"],16),op,2) for pc,offset,reg,op in [(0x8008a2cc,24,18,1),(0x8008a2d4,32,31,2),(0x8008a2d8,28,19,3),(0x8008a2dc,20,17,4),(0x8008a2e4,16,16,5)]]
    lkaccess += [lp_access(0x8008a2f4,0x80110018,8,7,1),lp_access(0x8008a308,0x80110018,0,8,2),lp_access(0x8008a30c,0x80110014,8,9,1),lp_access(0x8008a330,0x80110000,0x80120000,11,1),lp_access(0x8008a334,0x80130000,0x80140000,12,1,9),lp_access(0x8008a338,0x80110014,8,13,1)]
    lkaccess += [lp_access(pc,lkframe+offset,int(lkentry[reg]["word"],16),op,1) for pc,offset,reg,op in [(0x8008a3e8,32,31,16),(0x8008a3ec,28,19,17),(0x8008a3f0,24,18,18),(0x8008a3f4,20,17,19),(0x8008a3f8,16,16,20)]]
    require(lk["access_journal"] == lkaccess, "Resource lookup exact access journal drifted")
    def lk_machine():return dict(gpr=[w.copy() for w in lkregs],hi=lp_word(0x12345678,5),lo=lp_word(0x9abcdef0,10))
    lkregs[18]=lkentry[4].copy();lkregs[29]=lp_word(lkframe);lkregs[31]=lp_word(0x8008a2e8)
    lk_calls=[]
    def lk_call(pc,target,op,site,argc):
        lk_calls.append(dict(pc=f"0x{pc:08x}",delay=f"0x{pc+4:08x}",target=f"0x{target:08x}",operation=op,invocation=1,site=site,program=1,argument_count=argc,machine=lk_machine()))
    lk_call(0x8008a2e0,0x8008a0a8,6,1,1)
    for i,v in {2:0,3:0xfffffff7,5:8,6:0,7:0,17:0x80110000,31:0x8008a31c}.items():lkregs[i]=lp_word(v)
    lk_call(0x8008a314,0x800771f0,10,2,4)
    for i,v in {2:0x80130000,4:0x80120000,6:8,16:0x80130000,31:0x8008a344}.items():lkregs[i]=lp_word(v)
    lkregs[5]=lp_word(0x80140000,9)
    lk_call(0x8008a33c,0x800909a8,14,3,3)
    lkregs[4]=lp_word(0x80110000);lkregs[31]=lp_word(0x8008a34c)
    lk_call(0x8008a344,0x80077638,15,4,1)
    require(lk["calls"] == lk_calls, "Resource lookup full child CPU contracts drifted")
    for i in (16,17,18,19,29,31):lkregs[i]=lkentry[i].copy()
    require(lookup["final_machine"] == lk_machine() and "Synthetic standalone" in lookup["fixture_contract"]
            and "0x8008A2E0" in lookup["next_unbound_boundary"], "Resource lookup full return/fixture contract drifted")
    lkframes=["frontend-resource-lookup-before","frontend-resource-lookup-after"]
    require(all(n in by_id for n in lkframes), "Resource lookup native frames missing")
    lkhashes=[ppm_hash(args.frames/f"{n}.ppm") for n in lkframes]
    require(lkhashes[0] == lkhashes[1] == ge_hashes[1], "Resource lookup CPU probe changed native pixels")
    (args.frames/"frontend_resource_lookup_verified.json").write_text(json.dumps(dict(program="FEONLY",address="0x8008A2C8",end="0x8008A407",driver_frame_count=len(states),source_receipt=lookup,before_sha256=lkhashes[0],after_sha256=lkhashes[1],gameplay_shown="BLOCKED"),indent=2))

    gm=json.loads((args.frames/"gameload_main_trace.json").read_text())
    require(gm["routine"] == "GAMELOAD:801E136C-801E140F" and gm["source_bytes"] == 164 and gm["source_instructions"] == 41
            and gm["source_sha256"] == "a2d2a4b742c47b1c72d89e7c8b2ddbada0fee604cef947e11914515653e82398", "GAMELOAD main source identity drifted")
    gm_expected=dict(result=-5,completed=0,transferred=0,stopped_pc=0x801e13f4,stopped_target=0x80020000,operations=13,accesses=4,callbacks_completed=8,call_attempts=9,call_completions=8,instruction_count=36,instruction_events=36,access_events=4,copies=2,call_overflow=0,contract_failure=0,gameplay_shown="BLOCKED")
    require(all(gm[k] == v for k,v in gm_expected.items()), "GAMELOAD main refused transfer/counts drifted")
    gm_regs=[[0x33000000+i*0x101,i&15] for i in range(32)]
    for i,v in {0:0,29:0x801ff000,31:0x801e14b4}.items():gm_regs[i]=[v,15]
    def gm_machine():return dict(gpr=[w.copy() for w in gm_regs],hi=[0x12345678,5],lo=[0x9abcdef0,10])
    def gm_machine_bytes(m):return b"".join(fc_le(w)+bytes([k]) for w,k in m["gpr"]+[m["hi"],m["lo"]])
    gm_input=gm_machine();gm_regs[29]=[0x801fefe8,15]
    gm_pcs=[0x801e1374,0x801e137c,0x801e1384,0x801e1394,0x801e13b0,0x801e13c4,0x801e13cc,0x801e13e0,0x801e13f4]
    gm_targets=[0x801e14b8,0x801e000c,0x801e059c,0x801e0938,0x801e1344,0x801e1300,0x801e1670,0x801e1344,0x80020000]
    gm_ops=[3,4,5,6,8,9,10,11,13];gm_argc=[0,0,0,2,3,2,0,3,0];gm_calls=[]
    for i,pc in enumerate(gm_pcs):
        changes={}
        if i==3:changes={4:0x8001000c,5:707}
        elif i==4:changes={16:4096,4:0x801b0000,5:0x80015008,6:4096}
        elif i==5:changes={4:0x801e0060,5:0x80015000}
        elif i==7:changes={4:0x80015008,5:0x801b0000,6:4096}
        elif i==8:changes={2:0x80020000}
        changes[31]=pc+8
        for reg,value in changes.items():gm_regs[reg]=[value,15]
        gm_calls.append(dict(pc=pc,delay=pc+4,target=gm_targets[i],operation=gm_ops[i],invocation=1,site=i+1,argc=gm_argc[i],program=2 if i==8 else 1,machine=gm_machine()))
    require(gm["input_machine"] == gm_input and gm["output_machine"] == gm_machine() and gm["call_sequence"] == gm_calls, "GAMELOAD main full CPU/9-child transport drifted")
    gm_access=[(0x801e1370,0x801feffc,0x801e14b4,1,15,2),(0x801e1378,0x801feff8,0x33001010,2,0,2),(0x801e13a0,0x80015004,4096,7,15,1),(0x801e13ec,0x80015000,0x80020000,12,15,1)]
    gm_access_bytes=b"".join(fc_le(pc)+fc_le(addr)+fc_le(value)+fc_le(op,8)+bytes([4,mask,kind]) for pc,addr,value,op,mask,kind in gm_access)
    gm_call_bytes=b"".join(fc_le(c["pc"])+fc_le(c["delay"])+fc_le(c["target"])+fc_le(c["operation"],8)+fc_le(c["invocation"],8)+bytes([c["site"],c["argc"],c["program"]])+gm_machine_bytes(c["machine"]) for c in gm_calls)
    gm_fingerprints=dict(input_machine=fc_hash(gm_machine_bytes(gm_input)),output_machine=fc_hash(gm_machine_bytes(gm_machine())),accesses=fc_hash(gm_access_bytes),pcs=fc_hash(b"".join(fc_le(pc) for pc in range(0x801e136c,0x801e13fc,4))),calls=fc_hash(gm_call_bytes))
    require(all(gm["canonical_fingerprint"][k] == v for k,v in gm_fingerprints.items()), "GAMELOAD main canonical full journals drifted")
    require(gm["canonical_fingerprint"]["algorithm"] == "FNV-1a-64" and gm["canonical_fingerprint"]["seed"] == "cbf29ce484222325" and gm["canonical_fingerprint"]["call_layout"] == "u32 pc,delay,target;u64 operation,invocation;u8 site,argc,program;170-byte machine", "GAMELOAD main hash contract drifted")
    def gm_checksum(data):
        value=2166136261
        for byte in data:value=((value^byte)*16777619)&0xffffffff
        return value
    gm_staged=bytearray((i*37+11)&255 for i in range(4096));gm_staged[0x90:0x94]=fc_le(1)
    gm_loaded=bytearray((i*19+3)&255 for i in range(4096));gm_loaded[:4]=fc_le(0x80020000);gm_loaded[4:8]=fc_le(4096)
    require(gm["staged_checksum"] == gm["restored_checksum"] == gm_checksum(gm_staged) and gm["loaded_checksum"] == gm_checksum(gm_loaded), "GAMELOAD main synthetic fixture payload checksums drifted")
    require(gm["fixture_contract"]["gameonly_service"] == "refused/unbound" and gm["next_unbound_boundary"]["first_production"] == "801E1374->801E14B8 startup", "GAMELOAD main missing service boundary drifted")
    gmframes=["gameload-main-before","gameload-main-after"]
    require(all(n in by_id for n in gmframes), "GAMELOAD main native frames missing")
    gmhashes=[ppm_hash(args.frames/f"{n}.ppm") for n in gmframes]
    require(gmhashes[0] == gmhashes[1] == lkhashes[1], "GAMELOAD main CPU probe changed native pixels")
    (args.frames/"gameload_main_verified.json").write_text(json.dumps(dict(program="GAMELOAD",address="0x801E136C",end="0x801E140F",driver_frame_count=len(states),source_receipt=gm,before_sha256=gmhashes[0],after_sha256=gmhashes[1],gameplay_shown="BLOCKED"),indent=2))

    bh=json.loads((args.frames/"gameload_bios_heap_trace.json").read_text())
    require(bh["program"] == "GAMELOAD" and bh["address"] == "0x801E1590" and bh["inclusive_end"] == "0x801E159B" and bh["bytes"] == 12 and bh["instructions"] == 3 and bh["source_sha256"] == "4487ee3019aae533a71d191483e6876aa40c2530923670ec0e012a78204fb863", "GAMELOAD BIOS heap source identity drifted")
    bh_expected=dict(operations=1,callbacks_completed=1,parent_result=-5,parent_completed=0,parent_transferred=0,parent_stopped_pc=0x801e14ac,parent_stopped_target=0x801e136c,pc_sequence=[0x801e1590,0x801e1594,0x801e1598],call_pc=0x801e1594,delay_pc=0x801e1598,bios_vector=0xa0,service=0x39,argument_count=2,callback_machine_words=34,final_machine_words=34,stack_pointer=0x807ffff8,synthetic_raster=False,gameplay_shown="BLOCKED")
    require(all(bh[k] == v for k,v in bh_expected.items()), "GAMELOAD BIOS heap exact tail transfer/refusal prefix drifted")
    bh_machine=[dict(word=0x47000000+i*0x10113,known_mask=i*5&15) for i in range(32)]
    for i,value in {0:0,1:0x801f0000,2:0x7ffff8,3:0x8000,4:0x801eb0a4,5:0x60cf58,8:0x80000000,9:0x39,10:0xa0,28:0x801e903c,29:0x807ffff8,30:0x807ffff8,31:0x801e14a0}.items():bh_machine[i]=dict(word=value,known_mask=15)
    bh_machine += [dict(word=0x12345678,known_mask=5),dict(word=0x9abcdef0,known_mask=10)]
    require(bh["callback_machine"] == bh_machine, "GAMELOAD BIOS full callback CPU drifted")
    bh_machine[31]=dict(word=0x801e14b4,known_mask=15)
    require(bh["final_machine"] == bh_machine, "GAMELOAD BIOS full stopped CPU drifted")
    bh_memory=bytearray(0x200000)
    for address,value in {0x801e8b70:0x800000,0x801e8b6c:0x8000,0x801e8b50:0x60cf58,0x801e8b4c:0x801eb0a0,0x801e903c:0x80028b70}.items():bh_memory[address-0x80000000:address-0x80000000+4]=fc_le(value)
    bh_hash=2166136261
    for byte in bh_memory:
        bh_hash=((bh_hash^byte)*16777619)&0xffffffff
        bh_hash=((bh_hash^1)*16777619)&0xffffffff
    require(bh["callback_memory_before"] == bh["callback_memory_after"] == bh_hash and "Synthetic BIOS" in bh["fixture_contract"] and "refuses" in bh["fixture_contract"] and "BIOS A0:39" in bh["next_unbound_boundary"], "GAMELOAD BIOS retained memory/fixture boundary drifted")
    bhframes=["gameload-bios-heap-before","gameload-bios-heap-after"]
    require(all(n in by_id for n in bhframes), "GAMELOAD BIOS heap native frames missing")
    bhhashes=[ppm_hash(args.frames/f"{n}.ppm") for n in bhframes]
    require(bhhashes[0] == bhhashes[1] == gmhashes[1], "GAMELOAD BIOS heap CPU probe changed native pixels")
    (args.frames/"gameload_bios_heap_verified.json").write_text(json.dumps(dict(program="GAMELOAD",address="0x801E1590",end="0x801E159B",driver_frame_count=len(states),source_receipt=bh,before_sha256=bhhashes[0],after_sha256=bhhashes[1],gameplay_shown="BLOCKED"),indent=2))

    info=json.loads((args.frames/"frontend_resource_info_trace.json").read_text())
    require(info["program"] == "FEONLY" and info["address"] == "0x8008a594" and info["inclusive_end"] == "0x8008a6eb"
            and info["bytes"] == 344 and info["instructions"] == 86 and info["source_sha256"] == "494529aeb56f769fbc5f40e3792f83492ad9368f40e6672ce2f4359a6d0a887a", "Resource info identity drifted")
    require(info["result"] == 1 and info["completed"] == 1 and info["contract_failure"] == 0 and info["gameplay_shown"] == "BLOCKED", "Resource info outcome drifted")
    ri=info["owner"]
    require([ri[n] for n in ("operations","accesses","reads","stores","callbacks","attempts")] == [30,25,11,14,5,1], "Resource info counts drifted")
    require(ri["instruction_trace"] == [f"0x{pc:08x}" for pc in list(range(0x8008a594,0x8008a5f8,4))+list(range(0x8008a620,0x8008a688,4))+list(range(0x8008a6ac,0x8008a6ec,4))], "Resource info exact PC trace drifted")
    riregs=[lp_word(0x57000000+i*0x101,(i%15)+1) for i in range(32)]
    for i,v in {0:0,5:0x801e0000,6:0x801e0004,7:0x801e0008,29:0x801f0000,31:0x8007b21c}.items():riregs[i]=lp_word(v)
    riregs[4]=lp_word(0x80024854,5)
    rientry=[w.copy() for w in riregs];riframe=0x801efea0
    risaves=[(0x8008a598,332,21,1),(0x8008a5a0,312,16,2),(0x8008a5a8,336,22,3),(0x8008a5b0,328,20,4),(0x8008a5b8,316,17,5),(0x8008a5c0,320,18,6),(0x8008a5c8,340,23,7)]
    riaccess=[lp_access(pc,riframe+offset,int(rientry[reg]["word"],16),op,2,rientry[reg]["known_mask"]) for pc,offset,reg,op in risaves]
    riaccess += [lp_access(0x8008a5cc,0x801f0010,0x2468ace0,8,1,9),lp_access(0x8008a5e4,riframe+344,0x8007b21c,9,2),lp_access(0x8008a5ec,riframe+324,int(rientry[19]["word"],16),10,2,rientry[19]["known_mask"])]
    riaccess += [lp_access(0x8008a634,0x801e0000,0,12,2),lp_access(0x8008a638,0x801e0004,0,13,2),lp_access(0x8008a640,0x801e0008,0,14,2),lp_access(0x8008a654,0x801e0000,0x44,17,2),lp_access(0x8008a660,0x801e0000,0x44,19,1),lp_access(0x8008a6b8,0x801e0008,0x1200,21,2)]
    rirestores=[(31,344),(23,340),(22,336),(21,332),(20,328),(19,324),(18,320),(17,316),(16,312)]
    riaccess += [lp_access(0x8008a6bc+i*4,riframe+off,int(rientry[reg]["word"],16),22+i,1,rientry[reg]["known_mask"]) for i,(reg,off) in enumerate(rirestores)]
    require(ri["access_journal"] == riaccess,"Resource info exact access/knownness journal drifted")
    def ri_machine():return dict(gpr=[w.copy() for w in riregs],hi=lp_word(0x12345678,5),lo=lp_word(0x9abcdef0,10))
    ricalls=[]
    def ri_call(pc,target,op,argc):
        ricalls.append(dict(pc=f"0x{pc:08x}",delay=f"0x{pc+4:08x}",target=f"0x{target:08x}",operation=op,invocation=1,program=1,argument_count=argc,machine=ri_machine()))
    for i,v in {4:0x800d96a8,5:0x800d9a58,6:6,16:0x801e0000,17:0,18:10,20:0x801e0008,22:0x801e0004,29:riframe,31:0x8008a5f0}.items():riregs[i]=lp_word(v)
    riregs[21]=rientry[4].copy();riregs[23]=lp_word(0x2468ace0,9)
    ri_call(0x8008a5e8,0x80084910,11,3)
    for i,v in {2:1,4:riframe+24,5:0x800d9a60,6:0x800d96a8,31:0x8008a644}.items():riregs[i]=lp_word(v)
    riregs[7]=rientry[4].copy();ri_call(0x8008a63c,0x80083b70,15,4)
    riregs[5]=lp_word(1);riregs[31]=lp_word(0x8008a650);ri_call(0x8008a648,0x8007f588,16,2)
    riregs[2]=lp_word(0x44);riregs[4]=lp_word(0x44);riregs[31]=lp_word(0x8008a660);ri_call(0x8008a658,0x8008a408,18,1)
    for i,v in {2:0x1200,5:0,6:0,17:0x1200,31:0x8008a674}.items():riregs[i]=lp_word(v)
    ri_call(0x8008a66c,0x8007f318,20,3)
    require(ri["calls"] == ricalls,"Resource info complete child CPU contracts drifted")
    riregs[2]=lp_word(0);riregs[4]=lp_word(riframe+24)
    for i in list(range(16,24))+[29,31]:riregs[i]=rientry[i].copy()
    require(info["final_machine"] == ri_machine() and "Synthetic standalone" in info["fixture_contract"] and "0x80084910" in info["next_unbound_boundaries"],"Resource info full return/fixture contract drifted")
    riframes=["frontend-resource-info-before","frontend-resource-info-after"]
    require(all(n in by_id for n in riframes),"Resource info native frames missing")
    rihashes=[ppm_hash(args.frames/f"{n}.ppm") for n in riframes]
    require(rihashes[0] == rihashes[1] == bhhashes[1],"Resource info CPU probe changed native pixels")
    (args.frames/"frontend_resource_info_verified.json").write_text(json.dumps(dict(program="FEONLY",address="0x8008A594",end="0x8008A6EB",driver_frame_count=len(states),source_receipt=info,before_sha256=rihashes[0],after_sha256=rihashes[1],gameplay_shown="BLOCKED"),indent=2))

    required = ["setup", "entry", "user-setup-entry", "match-handoff-pending"]
    require(all(frame in by_id for frame in required), "screen-driving path is incomplete")
    require([by_id[name]["page"] for name in required] ==
            ["Game Setup", "Team Select", "User Setup", "User Setup"],
            "screen-driving page order drifted")
    require([states.index(by_id[name]) for name in required] ==
            sorted(states.index(by_id[name]) for name in required),
            "screen-driving frame order drifted")
    require(by_id["match-handoff-pending"]["match_revision"] == 1 and
            by_id["match-handoff-pending"]["assignment"] == 2,
            "accepted match was not captured")

    hashes = {name: ppm_hash(args.frames / f"{name}.ppm") for name in required}
    require(len({hashes["setup"], hashes["entry"], hashes["user-setup-entry"]}) == 3,
            "Setup, Team Select and User Setup frames are not visually distinct")
    require(hashes["user-setup-entry"] != hashes["match-handoff-pending"],
            "accepted controller assignment did not change the User Setup frame")
    move_hashes = {
        name: ppm_hash(args.frames / f"{name}.ppm")
        for name in ["move-image-before-buffer0", "move-image-source",
                     "move-image-buffer0", "move-image-buffer1"]}
    require(move_hashes["move-image-before-buffer0"] !=
            move_hashes["move-image-source"],
            "MoveImage diagnostic source is indistinguishable from the old buffer")
    require(move_hashes["move-image-source"] ==
            move_hashes["move-image-buffer0"] ==
            move_hashes["move-image-buffer1"],
            "MoveImage did not reproduce its source in both retained VRAM buffers")
    sync_hashes = {
        name: ppm_hash(args.frames / f"{name}.ppm")
        for name in ["draw-sync-before-buffer0", "draw-sync-after-buffer0"]}
    require(sync_hashes["draw-sync-before-buffer0"] ==
            move_hashes["move-image-before-buffer0"],
            "MoveImage packets became visible before DrawSync")
    require(sync_hashes["draw-sync-before-buffer0"] !=
            sync_hashes["draw-sync-after-buffer0"],
            "DrawSync did not produce a visible retained-VRAM transition")
    require(sync_hashes["draw-sync-after-buffer0"] ==
            move_hashes["move-image-source"] ==
            move_hashes["move-image-buffer0"],
            "DrawSync completion does not match the submitted MoveImage source")
    display_paths = {
        name: args.frames / f"{name}.ppm"
        for name in ["set-disp-mask-before", "set-disp-mask-after"]}
    display_hashes = {name: ppm_hash(path) for name, path in display_paths.items()}
    require(set(ppm_pixels(display_paths["set-disp-mask-before"])) == {0},
            "SetDispMask pre-enable scanout is not completely masked")
    require(display_hashes["set-disp-mask-before"] !=
            display_hashes["set-disp-mask-after"],
            "SetDispMask did not produce a visible retained-scanout transition")
    require(display_hashes["set-disp-mask-after"] ==
            sync_hashes["draw-sync-after-buffer0"] ==
            move_hashes["move-image-source"],
            "SetDispMask enabled scanout does not match the completed active buffer")
    validator_hashes = {
        name: ppm_hash(args.frames / f"{name}.ppm")
        for name in ["crc-validator-install-before",
                     "crc-validator-install-after"]}
    require(validator_hashes["crc-validator-install-before"] ==
            validator_hashes["crc-validator-install-after"] ==
            display_hashes["set-disp-mask-after"],
            "CRCF validator registration unexpectedly changed retained scanout")
    frame_rate_hashes = {
        name: ppm_hash(args.frames / f"{name}.ppm")
        for name in ["frame-rate-reset-before", "frame-rate-reset-after"]}
    require(frame_rate_hashes["frame-rate-reset-before"] ==
            frame_rate_hashes["frame-rate-reset-after"] ==
            validator_hashes["crc-validator-install-after"],
            "frame-rate tracker reset unexpectedly changed retained scanout")
    match_session_hashes = {
        name: ppm_hash(args.frames / f"{name}.ppm")
        for name in ["match-session-before", "match-session-after"]}
    require(match_session_hashes["match-session-before"] ==
            match_session_hashes["match-session-after"] ==
            frame_rate_hashes["frame-rate-reset-after"],
            "match-session owner unexpectedly fabricated retained scanout")
    loading_display = {
        name: ppm_pixels(args.frames / f"{name}.ppm")
        for name in ["loading-screen-display-before",
                     "loading-screen-display-after"]}
    require(hashlib.sha256(loading_display["loading-screen-display-before"]).hexdigest() ==
            match_session_hashes["match-session-after"],
            "loading-screen capture did not begin at the retained visible page")
    require(loading_display["loading-screen-display-before"] !=
            loading_display["loading-screen-display-after"],
            "loading-screen compositor did not visibly replace the active page")
    loading_vram_names = ["loading-screen-vram-before",
                          "loading-screen-vram-after-top-left",
                          "loading-screen-vram-after-bottom-left",
                          "loading-screen-vram-complete"]
    loading_vram = {
        name: ppm_pixels(args.frames / f"{name}.ppm", 1024, 512)
        for name in loading_vram_names}
    require(len({hashlib.sha256(loading_vram[name]).hexdigest()
                 for name in loading_vram_names}) == 4,
            "the three loading-screen VRAM uploads are not visually distinct stages")
    before = loading_vram["loading-screen-vram-before"]
    first = loading_vram["loading-screen-vram-after-top-left"]
    second = loading_vram["loading-screen-vram-after-bottom-left"]
    complete = loading_vram["loading-screen-vram-complete"]
    loading_pixels = loading_display["loading-screen-display-after"]
    require(crop_rgb(first, 1024, 0, 0, 512, 240) == loading_pixels and
            equal_outside_rect(before, first, 1024, 512, 0, 0, 512, 240),
            "first loading-screen upload escaped (0,0,512,240)")
    require(crop_rgb(second, 1024, 0, 256, 512, 240) == loading_pixels and
            equal_outside_rect(first, second, 1024, 512, 0, 256, 512, 240),
            "second loading-screen upload escaped (0,256,512,240)")
    require(crop_rgb(complete, 1024, 512, 0, 512, 240) == loading_pixels and
            equal_outside_rect(second, complete, 1024, 512, 512, 0, 512, 240),
            "third loading-screen upload escaped (512,0,512,240)")
    resource_loader_frames = {
        name: ppm_pixels(args.frames / f"{name}.ppm")
        for name in ["resource-loader-zload-before",
                     "resource-loader-zload-after",
                     "resource-loader-feload-before",
                     "resource-loader-feload-after"]}
    require(resource_loader_frames["resource-loader-zload-before"] ==
            resource_loader_frames["resource-loader-zload-after"] ==
            loading_display["loading-screen-display-before"],
            "zloadscr.psh retry wrapper unexpectedly changed retained scanout")
    require(resource_loader_frames["resource-loader-feload-before"] ==
            resource_loader_frames["resource-loader-feload-after"] ==
            loading_display["loading-screen-display-after"],
            "feload.bin retry wrapper unexpectedly changed retained scanout")
    heap_payload_size_frames = {
        name: ppm_pixels(args.frames / f"{name}.ppm")
        for name in ["heap-payload-size-before",
                     "heap-payload-size-after"]}
    require(heap_payload_size_frames["heap-payload-size-before"] ==
            heap_payload_size_frames["heap-payload-size-after"] ==
            resource_loader_frames["resource-loader-feload-after"],
            "heap payload-size query unexpectedly changed retained scanout")
    cd_sync_frames = {
        name: ppm_pixels(args.frames / f"{name}.ppm")
        for name in ["cd-sync-before", "cd-sync-after"]}
    require(cd_sync_frames["cd-sync-before"] ==
            cd_sync_frames["cd-sync-after"] ==
            heap_payload_size_frames["heap-payload-size-after"],
            "CdSync wrapper unexpectedly changed retained scanout")
    cd_ready_callback_frames = {
        name: ppm_pixels(args.frames / f"{name}.ppm")
        for name in ["cd-ready-callback-before", "cd-ready-callback-after"]}
    require(cd_ready_callback_frames["cd-ready-callback-before"] ==
            cd_ready_callback_frames["cd-ready-callback-after"] ==
            cd_sync_frames["cd-sync-after"],
            "CdReadyCallback exchange unexpectedly changed retained scanout")
    cd_sync_callback_frames = {
        name: ppm_pixels(args.frames / f"{name}.ppm")
        for name in ["cd-sync-callback-before", "cd-sync-callback-after"]}
    require(cd_sync_callback_frames["cd-sync-callback-before"] ==
            cd_sync_callback_frames["cd-sync-callback-after"] ==
            cd_ready_callback_frames["cd-ready-callback-after"],
            "CdSyncCallback exchange unexpectedly changed retained scanout")
    vblank_shutdown_frames = {
        name: ppm_pixels(args.frames / f"{name}.ppm")
        for name in ["vblank-shutdown-before", "vblank-shutdown-after"]}
    require(vblank_shutdown_frames["vblank-shutdown-before"] ==
            vblank_shutdown_frames["vblank-shutdown-after"] ==
            cd_sync_callback_frames["cd-sync-callback-after"],
            "VBlank shutdown wrapper unexpectedly changed retained scanout")
    clock_shutdown_frames = {
        name: ppm_pixels(args.frames / f"{name}.ppm")
        for name in ["clock-shutdown-before", "clock-shutdown-after"]}
    require(clock_shutdown_frames["clock-shutdown-before"] ==
            clock_shutdown_frames["clock-shutdown-after"] ==
            vblank_shutdown_frames["vblank-shutdown-after"],
            "game-clock shutdown wrapper unexpectedly changed retained scanout")
    controller_suspend_frames = {
        name: ppm_pixels(args.frames / f"{name}.ppm")
        for name in ["controller-suspend-before", "controller-suspend-after"]}
    require(controller_suspend_frames["controller-suspend-before"] ==
            controller_suspend_frames["controller-suspend-after"] ==
            clock_shutdown_frames["clock-shutdown-after"],
            "controller-suspend wrapper unexpectedly changed retained scanout")
    memory_zero_frames = {
        name: ppm_pixels(args.frames / f"{name}.ppm")
        for name in ["shutdown-table-zero-before", "shutdown-table-zero-after"]}
    require(memory_zero_frames["shutdown-table-zero-before"] ==
            memory_zero_frames["shutdown-table-zero-after"] ==
            controller_suspend_frames["controller-suspend-after"],
            "shutdown-table zero-fill unexpectedly changed retained scanout")
    memory_copy_frames = {
        name: ppm_pixels(args.frames / f"{name}.ppm")
        for name in ["feload-memory-copy-before", "feload-memory-copy-after"]}
    require(memory_copy_frames["feload-memory-copy-before"] ==
            memory_copy_frames["feload-memory-copy-after"] ==
            memory_zero_frames["shutdown-table-zero-after"],
            "FELOAD CPU-memory copy unexpectedly changed retained scanout")

    receipt = read_json(args.frames / "game_entry_trace.json")
    feload = read_json(args.frames / "feload_entry_trace.json")
    require((feload["program"], feload["address"], feload["inclusive_end"],
             feload["bytes"], feload["instructions"], feload["call_pc"]) ==
            ("FELOAD", "0x801E1410", "0x801E14B7", 168, 42, "0x80029BA8"),
            "FELOAD startup provenance drifted")
    require(feload["classification"] == "no direct visual effect" and
            feload["routine_capture_frame_numbers"] == [0, 1] and
            "synthetic" in feload["scope"] and "no live" in feload["scope"],
            "FELOAD startup diagnostic scope drifted")
    require(feload["words_cleared"] == 2067 and
            feload["bss_before_byte"] == 165 and
            feload["bss_after_zero_except_saved_ra"] and
            feload["operations"] == 2075 and feload["reads"] == 3 and
            feload["stores"] == 2070 and
            feload["heap_base"] == 0x801EB088 and
            feload["heap_size"] == 0x10F70 and
            feload["saved_ra"] == feload["restored_ra"] == 0x80029BB0 and
            feload["sp"] == feload["s8"] == 0x801FFFF8 and
            feload["gp"] == 0x801E903C,
            "FELOAD startup CPU state/order receipt drifted")
    require(feload["calls"] == [
        {"pc": 0x801E1498, "entry": 0x801E1590, "a0": 0x801EB08C,
         "a1": 0x10F70, "ra": 0x801E14A0},
        {"pc": 0x801E14AC, "entry": 0x801E136C, "a0": 0x801EB08C,
         "a1": 0x10F70, "ra": 0x801E14B4}],
        "FELOAD startup call PCs/delay-slot registers drifted")
    feload_hashes = {name: ppm_hash(args.frames / name)
                     for name in feload["captures"]}
    require(len(feload_hashes) == 2 and len(set(feload_hashes.values())) == 1 and
            next(iter(feload_hashes.values())) ==
            ppm_hash(args.frames / "feload-memory-copy-after.ppm"),
            "FELOAD CPU startup changed retained scanout")
    (args.frames / "feload_entry_verified.json").write_text(json.dumps({
        "program": "FELOAD", "address": "0x801E1410",
        "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": feload_hashes,
        "cpu_receipt": "feload_entry_trace.json",
        "classification": "no direct visual effect"}, indent=2) + "\n",
        encoding="utf-8")
    initialize = read_json(args.frames / "match_initialize_trace.json")
    require((initialize["program"], initialize["address"], initialize["inclusive_end"],
             initialize["bytes"], initialize["instructions"], initialize["call_pc"]) ==
            ("GAMEONLY", "0x8002DB90", "0x8002DC37", 168, 42, "0x8002DA7C"),
            "match initializer provenance drifted")
    require(initialize["classification"] == "no direct visual effect" and
            initialize["routine_capture_frame_numbers"] == [0, 1] and
            "synthetic" in initialize["scope"] and "no advancing" in initialize["scope"],
            "match initializer scope drifted")
    require(initialize["operations"] == 19 and initialize["reads"] == 3 and
            initialize["stores"] == 4 and initialize["calls_completed"] == 12 and
            initialize["zero_bytes"] == 3708 and initialize["zero_stores"] == 928 and
            initialize["zero_before_byte"] == 90 and initialize["zero_after"] and
            initialize["final_flag_before"] == 0xA5A5A5A5 and
            initialize["final_flag_after"] == 0 and initialize["final_child_saw_clear"] and
            initialize["return_v0"] == 0x800763F4 and
            initialize["restored_ra"] == 0x8002DA84 and initialize["sp"] == 0x807FFFA8,
            "match initializer CPU state receipt drifted")
    children = initialize["typed_children"]
    require([call["pc"] for call in children] == [
        0x8002DBC8, 0x8002DBD0, 0x8002DBD8, 0x8002DBE0, 0x8002DBE8,
        0x8002DBF0, 0x8002DBF8, 0x8002DC00, 0x8002DC08, 0x8002DC10, 0x8002DC20] and
        [call["entry"] for call in children] == [
        0x80063D58, 0x80029114, 0x8007FD40, 0x800294F8, 0x8002AB30,
        0x800640D8, 0x800659F0, 0x80065DB0, 0x80031E00, 0x80038A18, 0x800763F4] and
        children[9]["a0"] == children[10]["a0"] == 0xFFFFFFFF,
        "match initializer child order/delay-slot argument drifted")
    accesses = initialize["parent_accesses"]
    require([access["pc"] for access in accesses] == [
        0x8002DB94, 0x8002DB9C, 0x8002DBAC, 0x8002DBB4, 0x8002DBBC, 0x8002DC1C, 0x8002DC28] and
        [access["address"] for access in accesses] == [
        0x80021D74, 0x80021D78, 0x807FFFA0, 0x80022084, 0x80022ADC, 0x80020C18, 0x807FFFA0] and
        [accesses[0]["value"], accesses[1]["value"]] == initialize["team_snapshots"] and
        [accesses[3]["value"], accesses[4]["value"]] == initialize["team_snapshots"],
        "match initializer memory order/snapshots drifted")
    initialize_hashes = {name: ppm_hash(args.frames / name)
                         for name in initialize["captures"]}
    require(len(initialize_hashes) == 2 and len(set(initialize_hashes.values())) == 1 and
            next(iter(initialize_hashes.values())) ==
            ppm_hash(args.frames / "match-session-before.ppm"),
            "match initializer changed retained scanout")
    (args.frames / "match_initialize_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x8002DB90",
        "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": initialize_hashes,
        "cpu_receipt": "match_initialize_trace.json",
        "classification": "no direct visual effect"}, indent=2) + "\n", encoding="utf-8")
    reset = initialize["match_state_reset"]
    require((reset["program"],reset["address"],reset["inclusive_end"],reset["bytes"],reset["instructions"],reset["call_pc"]) == ("GAMEONLY","0x800659F0","0x80065B17",296,74,"0x8002DBF8"), "Match-state reset provenance drifted")
    require(reset["completed"] and reset["same_parent_memory"] and reset["classification"] == "no direct visual effect"
            and "remaining typed" in reset["scope"] and "no advancing" in reset["scope"]
            and (reset["operations"],reset["reads"],reset["stores"],reset["calls_completed"],reset["spin_iterations"]) == (26,4,8,14,24)
            and (reset["zero_calls"],reset["roster_calls"],reset["restored_ra"],reset["sp"]) == (4,1,0x8002DC00,0x807FFF90)
            and reset["hilo_known_masks"] == [0,0] and reset["final_halfwords"] == [0,65535,5,0]
            and reset["mode_98"] == int(reset["mode"] == 98)
            and initialize["zero_after_checkpoint"] == "immediately after parent zero before first child", "Match-state reset state drifted")
    require([(z["address"],z["length"],z["stores"],z["completed"]) for z in reset["zero_ranges"]] == [(0x8001F33C,0x4B0,301,1),(0x8001F7EC,0x1320,1225,1),(0x8001EDF4,0xC4,50,1),(0x8001EEB8,0xC4,50,1)], "Match-state reset zero ranges drifted")
    require([c["pc"] for c in reset["typed_children"]] == [0x80065A9C,0x80065AA4,0x80065ACC], "Match-state reset child order drifted")
    (args.frames / "match_state_reset_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800659F0","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":initialize_hashes,"cpu_receipt":"match_initialize_trace.json","state":reset,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    profile = reset["controller_profile_reset"]
    require((profile["program"],profile["address"],profile["inclusive_end"],profile["bytes"],profile["instructions"],profile["call_pc"]) == ("GAMEONLY","0x80083490","0x800835C3",308,77,"0x80065A38"), "Profile reset provenance drifted")
    require(profile["completed"] and profile["same_parent_memory"] and profile["records_verified"]
            and profile["classification"] == "no direct visual effect" and "runtime-generated" in profile["scope"]
            and (profile["operations"],profile["reads"],profile["stores"],profile["zero_calls"],profile["records_started"],profile["records_copied"],profile["bytes_copied"]) == (742,374,360,8,8,6,354)
            and (profile["return_address"],profile["sp"]) == (0x80065A40,0x807FFF70)
            and profile["hilo_known_masks"] == [0,0], "Profile reset machine or records drifted")
    (args.frames / "controller_profile_reset_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80083490","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":initialize_hashes,"cpu_receipt":"match_initialize_trace.json","state":profile,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    headers = reset["team_header_initialize"]
    require((headers["program"],headers["address"],headers["inclusive_end"],headers["bytes"],headers["instructions"]) == ("GAMEONLY","0x800655B0","0x8006581F",624,156), "Team-header provenance drifted")
    require(headers["completed"] and headers["same_parent_memory"] and headers["classification"] == "no direct visual effect" and "runtime-generated" in headers["scope"] and len(headers["calls"]) == 2, "Team-header composition drifted")
    for item,pc,team,side in zip(headers["calls"],[0x80065A88,0x80065A94],[0x8001EDF4,0x8001EEB8],[0,5]):
        n=item["active_count"];d=int(item["difficulty"]>=2);r54,r57=item["ranks"]
        require(item["statuses_verified"] and item["actors_verified"] and item["lineups_verified"] and 0<=n<=12
                and (item["call_pc"],item["team"],item["side"],item["return_address"],item["sp"]) == (pc,team,side,pc+8,0x807FFF70)
                and (item["operations"],item["reads"],item["stores"],item["status_iterations"],item["unused_iterations"],item["actor_iterations"]) == (79+n+d,38+n,41+d,n,12-n,5)
                and item["direction"] == (0x14E00 if side else 0xFFFEB200)
                and item["thresholds"] == [(120-2*r57)&65535,(r57+28)//(2 if d else 1),(1260-32*r54)&65535]
                and item["hilo_known_masks"] == [0,0], "Team-header source state drifted")
    (args.frames / "team_header_initialize_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800655B0","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":initialize_hashes,"cpu_receipt":"match_initialize_trace.json","state":headers,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    strategy = reset["team_strategy_apply"]
    require((strategy["program"],strategy["address"],strategy["inclusive_end"],strategy["bytes"],strategy["instructions"]) == ("GAMEONLY","0x80065820","0x800659EF",464,116), "Team-strategy provenance drifted")
    require(strategy["completed"] and strategy["same_parent_memory"] and strategy["classification"] == "no direct visual effect" and "runtime-generated" in strategy["scope"] and len(strategy["calls"]) == 2, "Team-strategy composition drifted")
    for index,item in enumerate(strategy["calls"]):
        require(item["fields_verified"] and item["lineup_verified"] and item["count_after"] == (item["count_before"]-1)&65535
                and (item["call_pc"],item["team"],item["side"],item["injury"],item["child_pc"],item["child_entry"],item["return_address"],item["sp"]) ==
                ([0x80065ABC,0x80065AC4][index],[0x8001EDF4,0x8001EEB8][index],[0,5][index],[0,5][index],[0x800659C4,0x80065998][index],[0x80064DBC,0x800646A8][index],[0x80065AC4,0x80065ACC][index],0x807FFF70)
                and (item["operations"],item["reads"],item["stores"]) == [(26,15,10),(19,11,7)][index] and item["hilo_known_masks"] == [0,0], "Team-strategy state drifted")
    (args.frames / "team_strategy_apply_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80065820","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":initialize_hashes,"cpu_receipt":"match_initialize_trace.json","state":strategy,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    buffer = reset["match_buffer_initialize"]
    if reset["mode"] != 98:
        require((buffer["program"],buffer["address"],buffer["inclusive_end"],buffer["bytes"],buffer["instructions"]) == ("GAMEONLY","0x8006432C","0x80064387",92,23), "Match-buffer provenance drifted")
        require(buffer["completed"] and buffer["same_parent_memory"] and buffer["buffer_verified"] and buffer["classification"] == "no direct visual effect" and "runtime-generated" in buffer["scope"]
                and (buffer["operations"],buffer["reads"],buffer["stores"],buffer["callbacks"],buffer["zero_stores"],buffer["zero_completed"]) == (7,1,4,2,223,1)
                and (buffer["child_pc"],buffer["child_entry"],buffer["return_address"],buffer["sp"]) == (0x80064370,0x80076AD0,0x80065B00,0x807FFF70)
                and buffer["hilo_known_masks"] == [0,0], "Match-buffer source state drifted")
        (args.frames / "match_buffer_initialize_verified.json").write_text(json.dumps({
            "program":"GAMEONLY","address":"0x8006432C","driver_frame_count":len(states),
            "input_transition_frames":{name:states.index(by_id[name]) for name in required},
            "frame_sha256":initialize_hashes,"cpu_receipt":"match_initialize_trace.json","state":buffer,
            "classification":"no direct visual effect"
        },indent=2)+"\n",encoding="utf-8")
    else:
        require(buffer is None, "Mode-98 unexpectedly entered match-buffer initializer")
    rewind = reset["mode98_buffer_rewind"] if reset["mode"] == 98 else buffer["match_buffer_rewind"]
    require((rewind["program"],rewind["address"],rewind["inclusive_end"],rewind["bytes"],rewind["instructions"]) == ("GAMEONLY","0x80076AD0","0x80076B27",88,22), "Buffer-rewind provenance drifted")
    require(rewind["completed"] and rewind["same_parent_memory"] and rewind["pointers_verified"] and rewind["flags_verified"] and rewind["classification"] == "no direct visual effect"
            and (rewind["operations"],rewind["reads"],rewind["stores"],rewind["callbacks"],rewind["zero_stores"],rewind["zero_bytes_stored"]) == (9,2,6,1,2,8)
            and rewind["call_pc"] == (0x80065AE8 if reset["mode"] == 98 else 0x80064370)
            and rewind["return_address"] == rewind["call_pc"]+8 and rewind["sp"] == (0x807FFF70 if reset["mode"] == 98 else 0x807FFF50)
            and rewind["return_value"] == rewind["pointer"] and rewind["hilo_known_masks"] == [0,0], "Buffer-rewind source state drifted")
    (args.frames / "match_buffer_rewind_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80076AD0","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":initialize_hashes,"cpu_receipt":"match_initialize_trace.json","state":rewind,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    candidate = strategy["calls"][0]["candidate_select"]
    require((candidate["program"],candidate["address"],candidate["inclusive_end"],candidate["bytes"],candidate["instructions"]) == ("GAMEONLY","0x80064DBC","0x8006506F",692,173), "Substitution-candidate provenance drifted")
    require(candidate["completed"] and candidate["same_parent_memory"] and candidate["classification"] == "no direct visual effect"
            and (candidate["call_pc"],candidate["team"],candidate["count"],candidate["injury_status"],candidate["callbacks"],candidate["return_value"],candidate["return_address"],candidate["sp"]) == (0x800659C4,0x8001EDF4,3,65534,0,0,0x800659CC,0x807FFF58)
            and (candidate["operations"],candidate["reads"],candidate["stores"]) == (27,26,1) and candidate["hilo_known_masks"] == [0,0]
            and strategy["calls"][1]["candidate_select"] is None, "Substitution-candidate source state drifted")
    (args.frames / "substitution_candidate_select_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80064DBC","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":initialize_hashes,"cpu_receipt":"match_initialize_trace.json","state":candidate,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    audio = initialize["audio_initialize"]
    require((audio["program"], audio["address"], audio["inclusive_end"],
             audio["bytes"], audio["instructions"], audio["call_pc"]) ==
            ("GAMEONLY", "0x80029114", "0x800291FF", 236, 59, "0x8002DBD0"),
            "audio initializer provenance drifted")
    require(audio["classification"] == "no direct visual effect" and
            "synthetic" in audio["scope"] and "no audible" in audio["scope"] and
            (audio["operations"], audio["reads"], audio["stores"], audio["calls_completed"]) == (20, 5, 4, 11)
            and audio["old_header"] == 0x80117000 and audio["loaded_header"] == 0x80118000
            and audio["live_header"] == 0x80118100 and audio["body"] == 0x80119000
            and audio["setting"] == 9 and audio["scaled_volume"] == 127
            and audio["result_before"] == 0xA5A5A5A5
            and audio["raw_return"] == audio["result_after"] == 0xFEEDBEEF
            and audio["restored_ra"] == 0x8002DBD8 and audio["sp"] == 0x807FFF90
            and audio["upload_args"] == [0x80021D6C, 0x80118100, 0x80119000],
            "audio initializer live bank/volume/stack state drifted")
    require(audio["loaders"] == [{"operations": 8, "attempts": 2, "null_results": 1}]*2
            and audio["typed_children"] == [
                {"pc": pc, "entry": entry} for pc, entry in (
                    (0x8002912C, 0x80090698), (0x80029164, 0x8008F4F0),
                    (0x8002916C, 0x800ADB48), (0x80029180, 0x8008CDC0),
                    (0x80029188, 0x8008CC28), (0x800291A0, 0x800AD360),
                    (0x800291A8, 0x80090698), (0x800291B0, 0x800ACA08),
                    (0x800291DC, 0x80088E84))],
            "audio initializer source call order or recovered retry behavior drifted")
    (args.frames / "audio_initialize_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x80029114", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": initialize_hashes, "cpu_receipt": "match_initialize_trace.json",
        "state": audio, "classification": "no direct visual effect"
    }, indent=2) + "\n", encoding="utf-8")
    scene = read_json(args.frames / "scene_load_trace.json")
    require((scene["program"], scene["address"], scene["inclusive_end"],
             scene["bytes"], scene["instructions"], scene["call_pc"]) ==
            ("GAMEONLY", "0x8002DB68", "0x8002DB8F", 40, 10, "0x8002DA84"),
            "scene wrapper provenance drifted")
    require(scene["classification"] == "no direct visual effect" and
            "synthetic" in scene["scope"] and "no advancing" in scene["scope"] and
            (scene["operations"], scene["reads"], scene["stores"], scene["calls_completed"]) == (4, 1, 1, 2)
            and scene["saved_address"] == 0x807FFFA0 and scene["saved_before"] == 0x8002DA84
            and scene["saved_after"] == scene["restored_ra"] == 0x8002DA8C
            and scene["sp"] == 0x807FFFA8 and scene["return_v0"] == 0x80048D5C,
            "scene wrapper stack/call state drifted")
    require(scene["children"] == [
        {"pc": 0x8002DB70, "entry": 0x800802AC, "delay_slot_pc": 0x8002DB74},
        {"pc": 0x8002DB78, "entry": 0x80048D5C, "delay_slot_pc": 0x8002DB7C}]
        and scene["accesses"] == [
            {"pc": pc, "address": 0x807FFFA0, "value": 0x8002DA8C}
            for pc in (0x8002DB6C, 0x8002DB80)],
        "scene wrapper access or typed-child order drifted")
    scene_hashes = {name: ppm_hash(args.frames / name) for name in scene["captures"]}
    require(scene["routine_capture_frame_numbers"] == [0, 1] and len(scene_hashes) == 2
            and len(set(scene_hashes.values())) == 1
            and next(iter(scene_hashes.values())) == next(iter(initialize_hashes.values())),
            "scene wrapper changed retained scanout")
    (args.frames / "scene_load_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x8002DB68", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": scene_hashes, "cpu_receipt": "scene_load_trace.json",
        "classification": "no direct visual effect"
    }, indent=2) + "\n", encoding="utf-8")
    warmup = scene["random_warmup"]
    require((warmup["program"], warmup["address"], warmup["inclusive_end"],
             warmup["bytes"], warmup["instructions"], warmup["call_pc"]) ==
            ("GAMEONLY", "0x800802AC", "0x80080303", 88, 22, "0x8002DB70"),
            "random warm-up provenance drifted")
    require(warmup["classification"] == "no direct visual effect" and
            "synthetic" in warmup["scope"] and warmup["completed"] == 1 and
            (warmup["operations"], warmup["reads"], warmup["stores"], warmup["calls_completed"]) ==
            (73, 2, 2, 69) and warmup["count"] == 65 and warmup["seed"] == 0xCAFE and
            warmup["frame_sp"] == 0x807FFF78 and warmup["restored_ra"] == 0x8002DB78 and
            warmup["step_counts"] == list(range(64, -1, -1)),
            "random warm-up count, delay decrement, seed or stack state drifted")
    expected_warmup_calls = [(0x800802B4, 0x800800F8), (0x800802BC, 0x8002AB70),
                            (0x800802C8, 0x8002AB70), (0x800802D0, 0x80093694)] + \
                           [(0x800802E0, 0x800935C4)] * 65
    require(warmup["children"] == [{"pc": pc, "entry": entry, "delay_slot_pc": pc + 4}
                                   for pc, entry in expected_warmup_calls] and
            warmup["accesses"] == [
                {"pc": pc, "address": address, "value": value, "known_mask": 15}
                for pc, address, value in [
                    (0x800802B0, 0x807FFF8C, 0x8002DB78),
                    (0x800802B8, 0x807FFF88, warmup["restored_s0"]),
                    (0x800802F0, 0x807FFF8C, 0x8002DB78),
                    (0x800802F4, 0x807FFF88, warmup["restored_s0"]) ]],
            "random warm-up exact child or memory journal drifted")
    (args.frames / "scene_random_warmup_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x800802AC", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": scene_hashes, "cpu_receipt": "scene_load_trace.json",
        "state": warmup, "classification": "no direct visual effect"
    }, indent=2) + "\n", encoding="utf-8")
    startup = scene["scene_startup"]
    require((startup["program"], startup["address"], startup["inclusive_end"],
             startup["bytes"], startup["instructions"], startup["call_pc"]) ==
            ("GAMEONLY", "0x80048D5C", "0x80048FE3", 648, 162, 0x8002DB78),
            "scene startup provenance drifted")
    require(startup["classification"] == "no direct visual effect" and
            "synthetic" in startup["scope"] and startup["completed"] == 1 and
            startup["operations"] == startup["reads"] + startup["stores"] + 19 and
            startup["reads"] + startup["stores"] == startup["access_count"] == 165 and
            startup["calls_completed"] == 19 and startup["controller_iterations"] == 8 and
            startup["controller_matches"] == 4 and startup["roster_iterations"] == 12 and
            startup["entity_iterations"] == 10 and startup["frame_sp"] == 0x807FFF68 and
            startup["restored_ra"] == 0x8002DB80 and startup["selector_before"] == 7 and
            startup["selector_after"] == 1 and startup["render_enable"] == 1 and
            startup["camera"] == [0, 0, 0x2E00, 0x55AA, 0xF95C, 0, 0] and
            startup["controllers"] == [2, 0] * 4 and
            startup["home_ids"] == [(i-300) & 0xFFFFFFFF for i in range(12)] and
            startup["away_ids"] == list(range(200, 212)) and
            startup["entity_ids"] == [(1000+i if i%2 else -1000-i) & 0xFFFFFFFF for i in range(10)],
            "scene startup controller/ID/camera/buffer state drifted")
    startup_calls = [(0x80048DAC, 0x8008F224)] * 8 + [
        (0x80048DF0, 0x8004D38C), (0x80048E94, 0x80052C20), (0x80048E9C, 0x800A7738),
        (0x80048EAC, 0x80056074), (0x80048EB8, 0x8005605C), (0x80048F20, 0x80099CA4),
        (0x80048F4C, 0x80099ACC), (0x80048F78, 0x80099CA4), (0x80048FA0, 0x80099ACC),
        (0x80048FB4, 0x80063EDC), (0x80048FBC, 0x80056944)]
    require([(c["pc"], c["entry"]) for c in startup["children"]] == startup_calls and
            all(c["delay_slot_pc"] == c["pc"]+4 and c["ra"] == c["pc"]+8 for c in startup["children"]) and
            [c["a0"] for c in startup["children"][:8]] == list(range(8)) and
            [c["a0"] for c in startup["children"][13:17]] ==
            [0x8002205C, 0x80021EEC, 0x80022070, 0x80021F48],
            "scene startup child/delay/buffer argument order drifted")
    (args.frames / "scene_startup_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x80048D5C", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": scene_hashes, "cpu_receipt": "scene_load_trace.json",
        "state": startup, "classification": "no direct visual effect"
    }, indent=2) + "\n", encoding="utf-8")
    resources = startup["resources"]
    require((resources["program"], resources["address"], resources["inclusive_end"],
             resources["bytes"], resources["instructions"], resources["call_pc"]) ==
            ("GAMEONLY", "0x80052C20", "0x800530FB", 1244, 311, 0x80048E94),
            "scene resources provenance drifted")
    require(resources["classification"] == "no direct visual effect" and
            "synthetic" in resources["scope"] and resources["completed"] == 1 and
            (resources["operations"], resources["reads"], resources["stores"], resources["calls_completed"]) ==
            (182, 46, 64, 72) and resources["typed_calls"] == 66 and resources["loader_count"] == 6 and
            resources["frame_sp"] == 0x807FFF48 and resources["restored_ra"] == 0x80048E9C,
            "scene resources operation or stack prefix drifted")
    resource_names = [0x8002639C, 0x8011B000, 0x8011B100, 0x800263AC, 0x800263BC, 0x80026404]
    resource_roots = [0x80140000 + (name & 0xFFFF) for name in resource_names]
    require(resources["loaders"] == [
        {"operations": 8, "attempts": 2, "nulls": 1, "return_v0": root} for root in resource_roots] and
        resources["attempts"] == [
            {"pc": 0x80029C18, "entry": 0x800941C8, "filename": name,
             "flags": 0x20 if i == 0 else 0, "attempt": attempt}
            for i, name in enumerate(resource_names) for attempt in (1, 2)],
        "scene resource recovered retry-loader arguments/null prefix drifted")
    require(resources["publications"] == [list(pair) for pair in [
        (0x800B72DC, 1), (0x800FB820, 0), (0x800FAC20, 0xFFFFFFFD), (0x800F9FC0, resource_roots[0]),
        (0x800F0EDC, resource_roots[1]), (0x800F0FAC, resource_roots[2]),
        (0x800EBC38, resource_roots[1]), (0x800F0F64, resource_roots[2]),
        (0x800FABCC, resource_roots[3]), (0x800D9284, 0), (0x801041A0, resource_roots[4]),
        (0x800FDB34, 0x8011D000), (0x800DCBE8, 0x8011E000), (0x80103F44, resource_roots[5])]] and
        resources["lookup_tables"] == [[root + 4*i for i in range(count)]
            for root, count in ((resource_roots[1], 10), (resource_roots[2], 10), (resource_roots[3], 26))],
        "scene resource publications or lookup table values drifted")
    release_calls = [c for c in resources["children"] if c["entry"] == 0x80090698]
    require([(c["pc"], c["a0"]) for c in release_calls] == [
        (0x80052FA4, resource_roots[4]), (0x80052FC8, 0x8011C000),
        (0x80052FD8, 0x8011E000), (0x80052FE8, 0x8011D000),
        (0x8005301C, 0x8011C100), (0x8005302C, resource_roots[0])],
        "scene resource exact release order drifted")
    (args.frames / "scene_resources_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x80052C20", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": scene_hashes, "cpu_receipt": "scene_load_trace.json",
        "state": resources, "classification": "no direct visual effect"
    }, indent=2) + "\n", encoding="utf-8")
    seed = warmup["random_seed"]
    require((seed["program"], seed["address"], seed["inclusive_end"], seed["bytes"], seed["instructions"]) ==
            ("GAMEONLY", "0x80093694", "0x80093733", 160, 40), "random seed provenance drifted")
    seed_words = [(0xCAFE + n) & 0xFFFFFFFF for n in
                  (0xE45A0E56,0x106226E9,0x8C48DD2F,0x0E03C49C,0x3C683F7D,0xDFBB3B64)]
    seed_pcs = [0x800936B0,0x800936C8,0x800936E0,0x800936F8,0x80093710,0x80093728]
    require(seed["completed"] == 1 and seed["classification"] == "no direct visual effect"
            and (seed["invocations"], seed["operations"], seed["stores"]) == (1, 6, 6)
            and (seed["call_pc"], seed["delay_slot_pc"]) == (0x800802D0,0x800802D4)
            and seed["words"] == seed_words
            and seed["accesses"] == [{"pc":pc,"address":0x800C4AE8+i*4,"value":seed_words[i],"known_mask":15}
                for i,pc in enumerate(seed_pcs)]
            and (seed["final_a0"], seed["final_a1"], seed["final_at"], seed["final_v0"]) ==
                (seed_words[-1],0x800C4AE8,0xD1A9FBE7,0xA352FBE7), "native six-word seed publication drifted")
    (args.frames / "random_seed_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80093694","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":scene_hashes,"cpu_receipt":"scene_load_trace.json","state":seed,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    loop = read_json(args.frames / "loop_entry_trace.json")
    require((loop["program"], loop["address"], loop["inclusive_end"],
             loop["bytes"], loop["instructions"], loop["call_pc"]) ==
            ("GAMEONLY", "0x8002DC38", "0x8002DC57", 32, 8, "0x8002DA8C"),
            "loop wrapper provenance drifted")
    require(loop["classification"] == "BLOCKED" and not loop["completed"]
            and "isolated" in loop["scope"] and "terminated" in loop["scope"]
            and (loop["operations"], loop["reads"], loop["stores"], loop["calls_completed"]) == (2, 0, 1, 0)
            and loop["stopped_pc"] == 0x8002DC40 and loop["stopped_entry"] == 0x80068BF8
            and loop["saved_pc"] == 0x8002DC3C and loop["saved_address"] == 0x807FFFA0
            and loop["saved_value"] == 0x8002DA94 and loop["unknown_output_gprs"] == 31
            and loop["tick"] == {"entry": 0x80068BF8, "completed": False, "operations": 1,
                "stopped_pc": 0x80068C24, "stopped_entry": 0x80066F88,
                "simulation_steps": 0, "frame_pumps": 0},
            "loop-entry probe must retain its exact unresolved tick boundary")
    loop_hashes = {name: ppm_hash(args.frames / name) for name in loop["captures"]}
    require(loop["routine_capture_frame_numbers"] == [0, 1] and len(loop_hashes) == 2
            and len(set(loop_hashes.values())) == 1
            and next(iter(loop_hashes.values())) == next(iter(initialize_hashes.values())),
            "blocked loop-entry probe changed scanout")
    (args.frames / "loop_entry_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x8002DC38", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json",
        "missing_boundary": "GAMEONLY 0x80068C24 -> 0x80066F88",
        "classification": "BLOCKED"
    }, indent=2) + "\n", encoding="utf-8")
    hot = loop["hot_start"]
    require((hot["program"], hot["address"], hot["inclusive_end"], hot["bytes"], hot["instructions"]) ==
            ("GAMEONLY", "0x80066F88", "0x800670A7", 288, 72), "hot-start provenance drifted")
    prefixes, total = [], 0
    for i in range(84):
        prefixes.append(total & 65535)
        total += max((i * 13) & 255 if i % 3 else 0, (255 - i * 3) & 255 if i % 4 else 0)
    require(hot["completed"] and "explicit synthetic" in hot["scope"]
            and hot["classification"] == "no direct visual effect"
            and hot["prefixes"] == prefixes and hot["prefixes_written"] == 84
            and (hot["calls"], hot["retry_attempts"], hot["hot_pointer"], hot["load_flag"], hot["cleared_halfword"]) ==
                (4, 2, 0x80130000, 1, 0)
            and hot["frame_stack_pointer"] == 0x801FFEE0 and hot["restored_ra"] == 0x80068C2C
            and hot["final_v0"] == 0x12345678
            and (hot["next_pc"], hot["next_entry"], hot["simulation_steps"], hot["frame_pumps"]) ==
                (0x80068C4C, 0x80067468, 0, 0), "hot-start native CPU fixture drifted")
    (args.frames / "match_hot_start_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x80066F88", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": hot,
        "classification": "no direct visual effect"
    }, indent=2) + "\n", encoding="utf-8")
    camera = hot["camera_startup"]
    require((camera["program"], camera["address"], camera["inclusive_end"], camera["bytes"], camera["instructions"]) ==
            ("GAMEONLY", "0x80079664", "0x80079757", 244, 61), "camera startup provenance drifted")
    require(camera["completed"] and "recovered hot-start output" in camera["scope"]
            and camera["classification"] == "no direct visual effect"
            and (camera["operations"], camera["reads"], camera["stores"], camera["calls"]) == (23,6,16,1)
            and (camera["call_pc"], camera["child_pc"], camera["child_args"]) == (0x80068C2C,0x800796B8,[12,0])
            and camera["camera_bytes"] == [0xE7,0x91] and camera["vector"] == [0xFFFF1234,0x12345678,0x87654321]
            and (camera["frame_stack_pointer"], camera["restored_ra"], camera["final_v0"]) ==
                (0x801FFEE8,0x80068C34,0xFFFFFFFF), "camera startup native CPU fixture drifted")
    (args.frames / "camera_startup_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80079664","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":camera,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    selection = camera["camera_select"]
    require((selection["program"],selection["address"],selection["inclusive_end"],selection["bytes"],selection["instructions"]) ==
            ("GAMEONLY","0x800799CC","0x80079D37",876,219), "camera selector provenance drifted")
    require(selection["completed"] and selection["classification"] == "no direct visual effect"
            and "explicit synthetic" in selection["scope"] and selection["call_pc"] == 0x800796B8
            and (selection["operations"],selection["reads"],selection["stores"]) == (37,12,21)
            and selection["call_pcs"] == [0x80079AB4,0x80079B7C,0x80079C8C,0x80079D0C]
            and (selection["mode"],selection["selected_pointer"],selection["force_flag"],selection["busy"]) ==
                (12,0x80124000,1,0)
            and selection["copied_words"] == [0x70000000+i*16 for i in range(5)]+[256]
            and selection["cleared_words"] == [0]*6
            and selection["frame_stack_pointer"] == 0x801FFE90 and selection["restored_ra"] == 0x800796C0,
            "camera selector native CPU fixture drifted")
    (args.frames / "camera_select_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800799CC","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":selection,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    period = loop["period_startup"]
    contact = period["ball_actor_contact_probe"]
    require((contact["program"], contact["address"], contact["inclusive_end"],
             contact["bytes"], contact["instructions"]) ==
            ("GAMEONLY", "0x800602CC", "0x80060E8B", 3008, 752), "ball contact provenance drifted")
    require(contact["completed"] and contact["classification"] == "no direct visual effect"
            and "typed geometry, acquisition and release services" in contact["scope"]
            and (contact["phase_before"],contact["phase_after"],contact["phase_delay"]) == (129,130,3)
            and (contact["operations"],contact["reads"],contact["stores"],contact["callbacks"],contact["actor_resets"]) == (69,38,22,9,2)
            and (contact["frame_stack_pointer"],contact["returned_sp"],contact["restored_ra"]) == (0x801FEFC0,0x801FF000,0x80060EDC)
            and contact["typed_call_pcs"] == [0x8006036C,0x800605B0,0x80060710,0x80060894,0x8006089C,0x80060974,0x80060988],
            "ball contact CPU phase transition drifted")
    (args.frames / "ball_actor_contact_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800602CC","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":contact,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    gate = contact["coordinate_gate"]
    require((gate["program"],gate["address"],gate["inclusive_end"],gate["bytes"],gate["instructions"]) ==
            ("GAMEONLY","0x80060E8C","0x80060EF7",108,27), "coordinate gate provenance drifted")
    require(gate["completed"] and gate["classification"] == "no direct visual effect"
            and gate["scope"] == "actual complete contact child; independent CPU fixture"
            and (gate["operations"],gate["reads"],gate["stores"],gate["callbacks"]) == (6,4,1,1)
            and gate["returned_value"] == 1 and gate["call_pc"] == 0x80060ED4
            and gate["child_arguments"] == [0x80001000,0x80002000,0]
            and (gate["frame_stack_pointer"],gate["returned_sp"],gate["restored_ra"]) ==
                (0x801FF000,0x801FF018,0x80061078), "coordinate gate actual child composition drifted")
    (args.frames / "ball_contact_gate_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80060E8C","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":gate,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    dispatch = period["contact_dispatch_probe"]
    require((dispatch["program"],dispatch["address"],dispatch["inclusive_end"],dispatch["bytes"],dispatch["instructions"]) ==
            ("GAMEONLY","0x80060FBC","0x800610FB",320,80), "contact dispatch provenance drifted")
    require(dispatch["completed"] and dispatch["contact_completed"]
            and dispatch["classification"] == "no direct visual effect"
            and "actual coordinate gate and contact owners" in dispatch["scope"]
            and (dispatch["operations"],dispatch["reads"],dispatch["stores"],dispatch["callbacks"]) == (62,47,4,11)
            and (dispatch["coordinate_gates"],dispatch["actor_pairs"]) == (2,9)
            and (dispatch["phase_before"],dispatch["phase_after"],dispatch["phase_delay"]) == (129,130,3)
            and (dispatch["frame_stack_pointer"],dispatch["returned_sp"],dispatch["restored_ra"]) ==
                (0x801FF018,0x801FF038,0x80068E10), "contact dispatch actual composition drifted")
    (args.frames / "contact_dispatch_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80060FBC","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":dispatch,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    actor_gate = period["actor_contact_gate_probe"]
    require((actor_gate["program"],actor_gate["address"],actor_gate["inclusive_end"],actor_gate["bytes"],actor_gate["instructions"]) ==
            ("GAMEONLY","0x8005FAA8","0x8005FAE7",64,16), "actor gate provenance drifted")
    require(actor_gate["completed"] and actor_gate["parent_completed"]
            and actor_gate["classification"] == "no direct visual effect"
            and "typed eligibility child returns zero" in actor_gate["scope"]
            and (actor_gate["invocations"],actor_gate["call_pc"]) == (45,0x8006104C)
            and (actor_gate["operations"],actor_gate["reads"],actor_gate["stores"],actor_gate["callbacks"]) == (5,3,1,1)
            and (actor_gate["difference"],actor_gate["shifted_difference"],actor_gate["returned_value"]) == (256,1,1)
            and (actor_gate["frame_stack_pointer"],actor_gate["returned_sp"],actor_gate["restored_ra"]) ==
                (0x801FF000,0x801FF018,0x80061054), "actor gate natural caller CPU fixture drifted")
    (args.frames / "actor_contact_gate_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8005FAA8","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":actor_gate,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    acquisition = period["ball_acquire_probe"]
    require((acquisition["program"],acquisition["address"],acquisition["inclusive_end"],acquisition["bytes"],acquisition["instructions"]) ==
            ("GAMEONLY","0x8005D140","0x8005D9EF",2224,556), "acquisition provenance drifted")
    require(acquisition["completed"] and acquisition["parent_completed"]
            and acquisition["classification"] == "no direct visual effect"
            and "actual complete ball contact caller and acquisition owner" in acquisition["scope"]
            and (acquisition["invocations"],acquisition["call_pc"]) == (1,0x8006089C)
            and (acquisition["operations"],acquisition["reads"],acquisition["stores"],acquisition["callbacks"]) == (66,24,42,0)
            and (acquisition["owner_before"],acquisition["owner_after"],acquisition["published_actor"],acquisition["published_team"]) ==
                (65535,0,0x80002000,0x8001EDF4)
            and (acquisition["phase_before"],acquisition["phase_after"],acquisition["phase_delay"]) == (129,130,3)
            and (acquisition["frame_stack_pointer"],acquisition["returned_sp"],acquisition["restored_ra"]) ==
                (0x801FEF90,0x801FEFC0,0x800608A4), "acquisition actual caller CPU fixture drifted")
    (args.frames / "ball_acquire_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8005D140","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":acquisition,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    actor_input = period["actor_input_probe"]
    require((actor_input["program"],actor_input["address"],actor_input["inclusive_end"],actor_input["bytes"],actor_input["instructions"]) ==
            ("GAMEONLY","0x800686B8","0x80068BF7",1344,336), "actor input provenance drifted")
    require(actor_input["completed"] and actor_input["classification"] == "no direct visual effect"
            and "no live tick bridge" in actor_input["scope"]
            and (actor_input["operations"],actor_input["reads"],actor_input["stores"],actor_input["callbacks"]) == (195,146,34,15)
            and (actor_input["countdown_before"],actor_input["countdown_after"],actor_input["controller_flag"]) == (1,0,1)
            and (actor_input["last_actor"],actor_input["last_team"],actor_input["action_target"]) ==
                (0x80110900,0x8001EEB8,0x80068A7C)
            and (actor_input["frame_stack_pointer"],actor_input["returned_sp"],actor_input["restored_ra"]) ==
                (0x801FEFB8,0x801FF000,0x80068E94), "actor input native CPU fixture drifted")
    (args.frames / "actor_input_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800686B8","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":actor_input,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    eligibility = period["actor_contact_eligibility_probe"]
    require((eligibility["program"], eligibility["address"], eligibility["inclusive_end"],
             eligibility["bytes"], eligibility["instructions"]) ==
            ("GAMEONLY", "0x8005F948", "0x8005FAA7", 352, 88), "eligibility provenance drifted")
    require(eligibility["completed"] and eligibility["parent_completed"]
            and eligibility["classification"] == "no direct visual effect"
            and "independent CPU fixture" in eligibility["scope"]
            and "typed action" in eligibility["scope"]
            and (eligibility["geometry_calls"], eligibility["action_calls"]) == (1, 1)
            and (eligibility["operations"], eligibility["reads"], eligibility["stores"], eligibility["callbacks"]) == (15, 10, 3, 2)
            and (eligibility["normalized_x"], eligibility["normalized_y"], eligibility["action_raw_return"],
                 eligibility["returned_value"], eligibility["parent_returned_value"]) == (3, 4, 0x123456CD, 0xCD, 1)
            and (eligibility["frame_stack_pointer"], eligibility["returned_sp"], eligibility["restored_ra"]) ==
                (0x801FEFC8, 0x801FEFE8, 0x8005FAD4), "eligibility natural caller CPU fixture drifted")
    (args.frames / "actor_contact_eligibility_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x8005F948", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": eligibility,
        "classification": "no direct visual effect"
    }, indent=2)+"\n", encoding="utf-8")
    camera_transform = period["camera_frame_transform_probe"]
    require((camera_transform["program"], camera_transform["address"], camera_transform["inclusive_end"],
             camera_transform["bytes"], camera_transform["instructions"]) ==
            ("GAMEONLY", "0x80051098", "0x80051293", 508, 127), "camera transform provenance drifted")
    require(camera_transform["completed"] and camera_transform["classification"] == "no direct visual effect"
            and "independent full machine" in camera_transform["scope"]
            and "typed camera and GTE fixtures" in camera_transform["scope"]
            and camera_transform["translation_before"] == [0, 0, 0]
            and camera_transform["translation_after"] == [104, 205, 306]
            and (camera_transform["callbacks"], camera_transform["multiply_count"]) == (4, 3)
            and (camera_transform["operations"], camera_transform["reads"], camera_transform["stores"]) == (47, 22, 21)
            and (camera_transform["frame_stack_pointer"], camera_transform["returned_sp"], camera_transform["restored_ra"],
                 camera_transform["next_pc"]) == (0x801FEFD0, 0x801FF000, 0x800490BC, 0x800490C0),
            "camera transform native CPU state drifted")
    (args.frames / "camera_frame_transform_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x80051098", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": camera_transform,
        "classification": "no direct visual effect"
    }, indent=2)+"\n", encoding="utf-8")
    interrupt_disable = period["frame_interrupt_disable_probe"]
    require((interrupt_disable["program"], interrupt_disable["address"], interrupt_disable["inclusive_end"],
             interrupt_disable["bytes"], interrupt_disable["instructions"]) ==
            ("GAMEONLY", "0x80048FF4", "0x8004900B", 24, 6), "interrupt-disable provenance drifted")
    require(interrupt_disable["completed"] and interrupt_disable["frame_completed"]
            and interrupt_disable["classification"] == "no direct visual effect"
            and "explicit CP0 state" in interrupt_disable["scope"] and "typed restore" in interrupt_disable["scope"]
            and (interrupt_disable["status_before"], interrupt_disable["status_disabled"], interrupt_disable["status_after_typed_restore"]) ==
                (0xABCDEF01, 0xABCDEF00, 0xABCDEF01)
            and (interrupt_disable["invocations"], interrupt_disable["completions"], interrupt_disable["operations_per_call"]) == (13, 13, 2)
            and interrupt_disable["call_counts"] == [1, 10, 1, 1]
            and interrupt_disable["call_pcs"] == [0x80049070, 0x800491C8, 0x8004920C, 0x8004927C],
            "interrupt-disable native CP0 state drifted")
    (args.frames / "frame_interrupt_disable_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x80048FF4", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": interrupt_disable,
        "classification": "no direct visual effect"
    }, indent=2)+"\n", encoding="utf-8")
    draw_offset_command = period["draw_offset_command_probe"]
    require((draw_offset_command["program"],draw_offset_command["address"],draw_offset_command["inclusive_end"],draw_offset_command["bytes"],draw_offset_command["instructions"]) == ("GAMEONLY","0x8009A7DC","0x8009A823",72,18), "Draw-offset provenance drifted")
    require(draw_offset_command["completed"] and draw_offset_command["parent_completed"] and draw_offset_command["classification"] == "no direct visual effect"
            and "two synthetic packet helpers" in draw_offset_command["scope"]
            and (draw_offset_command["offset_calls"],draw_offset_command["packet_calls"],draw_offset_command["dma_calls"],draw_offset_command["submit_calls"],draw_offset_command["copy_calls"]) == (2,2,2,2,4)
            and draw_offset_command["cache_matches_last_environment"]
            and draw_offset_command["commands"] == [0xE5001802,0xE5001802], "Draw-offset native state drifted")
    for item,hi in zip(draw_offset_command["offsets"],[0x80048F4C,0x80048FA0]):
        require((item["operations"],item["reads"],item["return_v0"],item["return_address"],item["hi"]) == (1,1,0xE5001802,0x8009A3B8,hi), "Draw-offset machine drifted")
    (args.frames / "draw_offset_command_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8009A7DC","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":draw_offset_command,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    draw_mode_command = period["draw_mode_command_probe"]
    require((draw_mode_command["program"],draw_mode_command["address"],draw_mode_command["inclusive_end"],draw_mode_command["bytes"],draw_mode_command["instructions"]) == ("GAMEONLY","0x8009A5E8","0x8009A643",92,23), "Draw-mode provenance drifted")
    require(draw_mode_command["completed"] and draw_mode_command["parent_completed"] and draw_mode_command["classification"] == "no direct visual effect"
            and "one synthetic packet helper" in draw_mode_command["scope"]
            and (draw_mode_command["mode_calls"],draw_mode_command["packet_calls"],draw_mode_command["dma_calls"],draw_mode_command["submit_calls"],draw_mode_command["copy_calls"]) == (2,2,2,2,4)
            and draw_mode_command["cache_matches_last_environment"]
            and draw_mode_command["commands"] == [0xE1000634,0xE1000634], "Draw-mode native state drifted")
    for item,hi in zip(draw_mode_command["modes"],[0x80048F4C,0x80048FA0]):
        require((item["operations"],item["reads"],item["return_v0"],item["return_address"],item["hi"]) == (1,1,0xE1000634,0x8009A3D0,hi), "Draw-mode machine drifted")
    (args.frames / "draw_mode_command_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8009A5E8","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":draw_mode_command,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    texture_window_command = period["texture_window_command_probe"]
    require((texture_window_command["program"],texture_window_command["address"],texture_window_command["inclusive_end"],texture_window_command["bytes"],texture_window_command["instructions"]) == ("GAMEONLY","0x8009A824","0x8009A8A7",132,33), "Texture-window provenance drifted")
    require(texture_window_command["completed"] and texture_window_command["parent_completed"] and texture_window_command["classification"] == "no direct visual effect"
            and "all five packet helpers recovered" in texture_window_command["scope"]
            and (texture_window_command["window_calls"],texture_window_command["packet_calls"],texture_window_command["dma_calls"],texture_window_command["submit_calls"],texture_window_command["copy_calls"]) == (2,2,2,2,4)
            and texture_window_command["cache_matches_last_environment"]
            and texture_window_command["commands"] == [0xE2020E18,0xE2020E18], "Texture-window native state drifted")
    for item,hi in zip(texture_window_command["windows"],[0x80048F4C,0x80048FA0]):
        require((item["operations"],item["reads"],item["stores"],item["return_v0"],item["return_address"],item["hi"]) == (8,4,4,0xE2020E18,0x8009A3DC,hi), "Texture-window machine drifted")
    (args.frames / "texture_window_command_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8009A824","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":texture_window_command,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    gpu_packet_dma = period["gpu_packet_dma_probe"]
    require((gpu_packet_dma["program"],gpu_packet_dma["address"],gpu_packet_dma["inclusive_end"],gpu_packet_dma["bytes"],gpu_packet_dma["instructions"]) == ("GAMEONLY","0x8009B1F8","0x8009B243",76,19), "GPU packet DMA provenance drifted")
    require(gpu_packet_dma["completed"] and gpu_packet_dma["parent_completed"] and gpu_packet_dma["classification"] == "no direct visual effect"
            and "without GPU consumption" in gpu_packet_dma["scope"] and gpu_packet_dma["cache_matches_last_environment"]
            and (gpu_packet_dma["dma_calls"],gpu_packet_dma["submit_calls"],gpu_packet_dma["copy_calls"]) == (2,2,4)
            and gpu_packet_dma["port_addresses"] == [0x1F801814,0x1F8010A0,0x1F8010A4,0x1F8010A8], "GPU packet DMA native state drifted")
    for item,packet,hi in zip(gpu_packet_dma["leaves"],[0x80021F64,0x80021F08],[0x80048F4C,0x80048FA0]):
        require((item["operations"],item["reads"],item["stores"],item["return_v0"],item["return_address"],item["hi"]) == (8,4,4,0x1F8010A8,0x8009B3B0,hi)
                and item["port_words"] == [0x04000002,packet,0,0x01000401], "GPU packet DMA machine drifted")
    (args.frames / "gpu_packet_dma_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8009B1F8","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":gpu_packet_dma,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    graphics_submit = period["graphics_submit_probe"]
    require((graphics_submit["program"],graphics_submit["address"],graphics_submit["inclusive_end"],graphics_submit["bytes"],graphics_submit["instructions"]) == ("GAMEONLY","0x8009B298","0x8009B57B",740,185), "Graphics submission provenance drifted")
    require(graphics_submit["classification"] == "no direct visual effect" and "synthetic" in graphics_submit["scope"], "Graphics submission classification drifted")
    for scenario,queued in zip(graphics_submit["scenarios"],[False,True]):
        require(scenario["completed"] and scenario["parent_completed"] and scenario["queued"] == queued and scenario["cache_matches_last_environment"]
                and (scenario["submit_calls"],scenario["copy_calls"],scenario["service_calls"],scenario["head_after"],scenario["tail_after"]) == (2,4,10 if queued else 8,3 if queued else 1,0), "Graphics submission state drifted")
        for item,value,hi in zip(scenario["submissions"],[2,3] if queued else [0,0],[0x80048F4C,0x80048FA0]):
            require((item["callbacks"],item["copied_words"],item["return_v0"],item["return_address"],item["hi"]) == (5 if queued else 4,16 if queued else 0,value,0x80099B60,hi), "Graphics submission machine drifted")
    (args.frames / "graphics_submit_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8009B298","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":graphics_submit,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    draw_area_start = period["draw_area_start_probe"]
    require((draw_area_start["program"],draw_area_start["address"],draw_area_start["inclusive_end"],draw_area_start["bytes"],draw_area_start["instructions"]) == ("GAMEONLY","0x8009A644","0x8009A70F",204,51), "Draw-area start provenance drifted")
    require(draw_area_start["completed"] and draw_area_start["parent_completed"] and draw_area_start["classification"] == "no direct visual effect"
            and "four synthetic packet-word helpers" in draw_area_start["scope"]
            and (draw_area_start["area_calls"],draw_area_start["packet_calls"],draw_area_start["submit_calls"],draw_area_start["copy_calls"]) == (2,2,2,4)
            and draw_area_start["cache_matches_last_environment"]
            and draw_area_start["commands"] == [0xE3008040,0xE3008040], "Draw-area start native state drifted")
    for item,hi in zip(draw_area_start["areas"],[0x80048F4C,0x80048FA0]):
        require((item["operations"],item["reads"],item["return_v0"],item["return_address"],item["hi"]) == (3,3,0xE3008040,0x8009A36C,hi), "Draw-area start machine drifted")
    (args.frames / "draw_area_start_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8009A644","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":draw_area_start,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    draw_area_end = period["draw_area_end_probe"]
    require((draw_area_end["program"],draw_area_end["address"],draw_area_end["inclusive_end"],draw_area_end["bytes"],draw_area_end["instructions"]) == ("GAMEONLY","0x8009A710","0x8009A7DB",204,51), "Draw-area end provenance drifted")
    require(draw_area_end["completed"] and draw_area_end["parent_completed"] and draw_area_end["classification"] == "no direct visual effect"
            and "three synthetic packet-word helpers" in draw_area_end["scope"]
            and (draw_area_end["area_calls"],draw_area_end["packet_calls"],draw_area_end["submit_calls"],draw_area_end["copy_calls"]) == (2,2,2,4)
            and draw_area_end["cache_matches_last_environment"]
            and draw_area_end["commands"] == [0xE4017C7F,0xE4017C7F], "Draw-area end native state drifted")
    for item,hi in zip(draw_area_end["areas"],[0x80048F4C,0x80048FA0]):
        require((item["operations"],item["reads"],item["return_v0"],item["return_address"],item["hi"]) == (3,3,0xE4017C7F,0x8009A3A4,hi), "Draw-area end machine drifted")
    (args.frames / "draw_area_end_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8009A710","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":draw_area_end,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    draw_packet = period["draw_packet_probe"]
    require((draw_packet["program"],draw_packet["address"],draw_packet["inclusive_end"],draw_packet["bytes"],draw_packet["instructions"]) == ("GAMEONLY","0x8009A344","0x8009A5E7",676,169), "Draw packet provenance drifted")
    require(draw_packet["completed"] and draw_packet["parent_completed"] and draw_packet["classification"] == "no direct visual effect"
            and "five synthetic packet-word helpers" in draw_packet["scope"]
            and (draw_packet["packet_calls"],draw_packet["submit_calls"],draw_packet["copy_calls"]) == (2,2,4)
            and draw_packet["cache_matches_last_environment"]
            and draw_packet["tags_after"] == [0x09FFFFFF,0x06FFFFFF]
            and draw_packet["last_packet"] == [0x09FFFFFF,0x90000001,0x90000002,0x90000003,0x90000004,0x90000005,0xE6000000,0x02332211,0x00200040,0x00400040], "Draw packet native state drifted")
    for item,value,hi in zip(draw_packet["packets"],[6,9],[0x80048F4C,0x80048FA0]):
        require((item["callbacks"],item["return_v0"],item["return_address"],item["hi"]) == (5,value,0x80099B28,hi), "Draw packet machine drifted")
    (args.frames / "draw_packet_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8009A344","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":draw_packet,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    draw_environment = period["draw_environment_probe"]
    require((draw_environment["program"],draw_environment["address"],draw_environment["inclusive_end"],draw_environment["bytes"],draw_environment["instructions"]) == ("GAMEONLY","0x80099ACC","0x80099B8F",196,49), "Draw environment provenance drifted")
    require(draw_environment["completed"] and draw_environment["parent_completed"] and draw_environment["classification"] == "no direct visual effect"
            and "synthetic packet, submission, MMIO and BIOS services" in draw_environment["scope"]
            and (draw_environment["draw_calls"],draw_environment["packet_calls"],draw_environment["submit_calls"],draw_environment["draw_copy_calls"],draw_environment["all_copy_calls"]) == (2,2,2,2,4)
            and draw_environment["cache_matches_last_environment"]
            and draw_environment["tags_before"] == [0x12000000,0x34000000]
            and draw_environment["tags_after"] == [0x12FFFFFF,0x34FFFFFF], "Draw environment native state drifted")
    for item,pc,env in zip(draw_environment["draws"],[0x80048F4C,0x80048FA0],[0x80021F48,0x80021EEC]):
        require((item["call_pc"],item["operations"],item["reads"],item["stores"],item["callbacks"],item["return_v0"],item["return_address"],item["hi"],item["copy_t1"],item["copy_t2"]) == (pc,17,9,5,3,env,pc+8,pc,0x2A,0xA0), "Draw environment machine drifted")
    (args.frames / "draw_environment_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80099ACC","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":draw_environment,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    gpu_command = period["gpu_control_command_probe"]
    require((gpu_command["program"],gpu_command["address"],gpu_command["inclusive_end"],gpu_command["bytes"],gpu_command["instructions"]) == ("GAMEONLY","0x8009B16C","0x8009B193",40,10), "GPU command provenance drifted")
    require(gpu_command["completed"] and gpu_command["parent_completed"] and gpu_command["classification"] == "no direct visual effect"
            and "mapped synthetic MMIO and BIOS service" in gpu_command["scope"]
            and (gpu_command["gpu_calls"],gpu_command["video_calls"],gpu_command["copy_calls"]) == (5,2,2)
            and (gpu_command["port_address"],gpu_command["port_before"],gpu_command["port_after"]) == (0x1F801814,0,0x0800002E)
            and gpu_command["cache_bytes"] == [0x64,0x28,0x31,0x2E]
            and gpu_command["commands"] == [0x0500500A,0x0503C064,0x06CDA328,0x07048431,0x0800002E], "GPU command native state drifted")
    for item,command,pc in zip(gpu_command["leaves"],gpu_command["commands"],[0x80099D6C,0x80099D6C,0x80099F78,0x80099FA4,0x8009A114]):
        require((item["operations"],item["reads"],item["stores"],item["return_v0"],item["at"],item["return_address"]) == (3,1,2,command>>24,0x800E0000+(command>>24),pc+8), "GPU command machine drifted")
    (args.frames / "gpu_control_command_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8009B16C","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":gpu_command,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    video_mode = period["video_mode_probe"]
    require((video_mode["program"],video_mode["address"],video_mode["inclusive_end"],video_mode["bytes"],video_mode["instructions"]) == ("GAMEONLY","0x800985CC","0x800985DB",16,4), "Video query provenance drifted")
    require(video_mode["completed"] and video_mode["parent_completed"] and video_mode["classification"] == "no direct visual effect"
            and "synthetic GPU and BIOS services" in video_mode["scope"]
            and (video_mode["scene_display_calls"],video_mode["copy_calls"],video_mode["query_calls"]) == (2,2,2)
            and (video_mode["source_address"],video_mode["source_word_before"],video_mode["source_word_after"],video_mode["environment_video_byte"]) == (0x800C54AC,1,1,1)
            and video_mode["commands"] == [0x0500500A,0x0503C064,0x06CDA328,0x07048431,0x0800002E], "Video query native state drifted")
    for item,pc in zip(video_mode["queries"],[0x80099DE8,0x8009A034]):
        require((item["call_pc"],item["operations"],item["reads"],item["return_v0"],item["return_mask"],item["return_address"]) == (pc,1,1,1,15,pc+8), "Video query caller/return drifted")
    (args.frames / "video_mode_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800985CC","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":video_mode,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    display_environment = period["display_environment_probe"]
    require((display_environment["program"],display_environment["address"],display_environment["inclusive_end"],display_environment["bytes"],display_environment["instructions"]) == ("GAMEONLY","0x80099CA4","0x8009A153",1200,300), "Display environment provenance drifted")
    require(display_environment["completed"] and display_environment["parent_completed"] and display_environment["classification"] == "no direct visual effect"
            and "synthetic GPU/video/BIOS services" in display_environment["scope"]
            and (display_environment["display_calls"],display_environment["copy_calls"],display_environment["video_calls"]) == (2,2,2)
            and display_environment["commands"] == [0x0500500A,0x0503C064,0x06CDA328,0x07048431,0x0800002E]
            and display_environment["cache_matches_final_environment"] and (display_environment["copy_t1"],display_environment["copy_t2"]) == (0x2A,0xA0), "Display environment native state drifted")
    for item,pc,env,changed in zip(display_environment["invocations"],[0x80048F20,0x80048F78],[0x80022070,0x8002205C],[0,1]):
        require((item["call_pc"],item["return_v0"],item["return_address"],item["screen_changed"],item["mode_changed"]) == (pc,env,pc+8,changed,changed), "Display environment caller/cache branch drifted")
    (args.frames / "display_environment_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80099CA4","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":display_environment,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    bios_copy = period["bios_memory_copy_probe"]
    require((bios_copy["program"],bios_copy["address"],bios_copy["inclusive_end"],bios_copy["bytes"],bios_copy["instructions"]) == ("GAMEONLY","0x8009CB0C","0x8009CB17",12,3), "BIOS copy provenance drifted")
    require(bios_copy["completed"] and bios_copy["parent_completed"] and bios_copy["classification"] == "no direct visual effect"
            and "synthetic resource and BIOS services" in bios_copy["scope"] and (bios_copy["operations"],bios_copy["callbacks"]) == (1,1)
            and (bios_copy["call_pc"],bios_copy["delay_pc"],bios_copy["bios_vector"],bios_copy["service"]) == (0x8009CB10,0x8009CB14,0xA0,0x2A)
            and (bios_copy["destination_before"],bios_copy["destination_after"]) == (0,0xEFBEADDE)
            and (bios_copy["returned_t1"],bios_copy["returned_t2"]) == (0x2A,0xA0)
            and (bios_copy["returned_v0"],bios_copy["returned_v0_mask"]) == (0xCAFEBABE,7)
            and (bios_copy["returned_hi"],bios_copy["returned_lo"],bios_copy["hi_mask"],bios_copy["lo_mask"]) == (0xAABBCCDD,0x12345678,3,12)
            and (bios_copy["return_address"],bios_copy["parent_v0"]) == (0x80080094,0x1234ABCD), "BIOS copy native state drifted")
    (args.frames / "bios_memory_copy_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8009CB0C","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":bios_copy,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    gte_reference = period["gte_reference_transform_probe"]
    require((gte_reference["program"],gte_reference["address"],gte_reference["inclusive_end"],gte_reference["bytes"],gte_reference["instructions"]) == ("GAMEONLY","0x80056650","0x80056677",40,10), "GTE reference provenance drifted")
    require(gte_reference["completed"] and gte_reference["parent_completed"] and gte_reference["classification"] == "no direct visual effect"
            and "production GTE hardware; synthetic packed table" in gte_reference["scope"]
            and (gte_reference["operations"],gte_reference["reads"],gte_reference["stores"],gte_reference["hardware_calls"]) == (7,2,4,1)
            and gte_reference["output_before"] == [100,200,300] and gte_reference["output_after"] == [3,0xFFFFFFFF,7]
            and gte_reference["mac"] == [3,0xFFFFFFFF,7] and gte_reference["ir"] == [3,0xFFFFFFFF,7]
            and gte_reference["camera_tail"] == [7,4,13] and gte_reference["controls"] == [0xE6671999,0x20001999,0xF0000000,0x20000000,0x1000,0,0,0]
            and gte_reference["flag"] == 0 and gte_reference["unrelated_gte_preserved"]
            and (gte_reference["returned_sp"],gte_reference["return_address"]) == (0x801FEFD0,0x80051230), "GTE reference native state drifted")
    (args.frames / "gte_reference_transform_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80056650","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":gte_reference,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    gte_translation = period["gte_translation_install_probe"]
    require((gte_translation["program"], gte_translation["address"], gte_translation["inclusive_end"],
             gte_translation["bytes"], gte_translation["instructions"]) ==
            ("GAMEONLY", "0x80055F44", "0x80055F5F", 28, 7), "GTE translation provenance drifted")
    require(gte_translation["completed"] and gte_translation["parent_completed"] and gte_translation["matrix_completed"]
            and gte_translation["rotation_completed"] and gte_translation["classification"] == "no direct visual effect"
            and "synthetic packed table" in gte_translation["scope"] and "typed reference service" in gte_translation["scope"]
            and (gte_translation["operations"], gte_translation["reads"], gte_translation["control_writes"]) == (6, 3, 3)
            and gte_translation["controls_before"] == [0xA5000005,0xA5000006,0xA5000007]
            and gte_translation["controls_before_masks"] == [5,6,7]
            and gte_translation["controls_after"] == [0xE6671999,0x20001999,0xF0000000,0x20000000,0x1000,0,0,0]
            and gte_translation["controls_after_masks"] == [15]*8 and gte_translation["raw_loads"] == [0]*3
            and gte_translation["untouched_controls_preserved"]
            and (gte_translation["returned_sp"],gte_translation["return_address"]) == (0x801FEFD0,0x80051214),
            "GTE translation native state drifted")
    (args.frames / "gte_translation_install_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x80055F44", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": gte_translation,
        "classification": "no direct visual effect"
    }, indent=2)+"\n", encoding="utf-8")
    gte_rotation = period["gte_rotation_install_probe"]
    require((gte_rotation["program"], gte_rotation["address"], gte_rotation["inclusive_end"],
             gte_rotation["bytes"], gte_rotation["instructions"]) ==
            ("GAMEONLY", "0x80055F18", "0x80055F43", 44, 11), "GTE rotation provenance drifted")
    require(gte_rotation["completed"] and gte_rotation["parent_completed"] and gte_rotation["matrix_completed"]
            and gte_rotation["classification"] == "no direct visual effect"
            and "synthetic packed table" in gte_rotation["scope"]
            and "typed translation/reference services" in gte_rotation["scope"]
            and (gte_rotation["operations"], gte_rotation["reads"], gte_rotation["control_writes"]) == (10, 5, 5)
            and gte_rotation["controls_before"] == [0]*5 and gte_rotation["controls_before_masks"] == [0]*5
            and gte_rotation["controls_after"] == [0xE6671999, 0x20001999, 0xF0000000, 0x20000000, 0x1000]
            and gte_rotation["controls_after_masks"] == [15]*5
            and gte_rotation["raw_loads"] == [0xE6671999, 0x20001999, 0xF0000000, 0x20000000, 0xABCD1000]
            and gte_rotation["untouched_controls_unknown"]
            and (gte_rotation["returned_sp"], gte_rotation["return_address"]) == (0x801FEFD0, 0x8005120C),
            "GTE rotation native state drifted")
    (args.frames / "gte_rotation_install_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x80055F18", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": gte_rotation,
        "classification": "no direct visual effect"
    }, indent=2)+"\n", encoding="utf-8")
    dma = period["ordering_table_dma_probe"]
    require((dma["program"], dma["address"], dma["inclusive_end"], dma["bytes"], dma["instructions"]) ==
            ("GAMEONLY", "0x8009A97C", "0x8009AA63", 232, 58), "ordering DMA provenance drifted")
    require(dma["classification"] == "no direct visual effect" and "mapped MMIO fixture" in dma["scope"]
            and "typed DMA start/wait services" in dma["scope"] and len(dma["runs"]) == 2, "ordering DMA scope drifted")
    for error, run in enumerate(dma["runs"]):
        count = 32 if error else 4096
        require(run["completed"] and run["parent_completed"]
                and (run["operations"], run["reads"], run["stores"], run["callbacks"], run["waits"]) ==
                    ((21, 11, 8, 2, 1) if error else (23, 13, 8, 2, 1))
                and run["dma_address"] == 0x800F5C50 + count * 4 - 4 and run["dma_count"] == count
                and (run["master_before"], run["master_after"]) == (0x12345678, 0x1A345678)
                and (run["control_before"], run["control_started"], run["control_after"]) ==
                    (0x55667788, 0x11000002, 0x11000002 if error else 0x10000002)
                and (run["head_before"], run["head_after"]) == (0, 0xC567C)
                and run["backend_return"] == (0xFFFFFFFF if error else count)
                and run["parent_return"] == 0x800F5C50
                and (run["returned_sp"], run["restored_ra"]) == (0x801000E0, 0x800999C4),
                "ordering DMA native state drifted")
    (args.frames / "ordering_table_dma_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x8009A97C", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": dma,
        "classification": "no direct visual effect"
    }, indent=2)+"\n", encoding="utf-8")
    rotation = period["rotation_matrix_probe"]
    require((rotation["program"], rotation["address"], rotation["inclusive_end"], rotation["bytes"], rotation["instructions"]) ==
            ("GAMEONLY", "0x80056080", "0x800562CB", 588, 147), "rotation matrix provenance drifted")
    require(rotation["completed"] and rotation["parent_completed"]
            and rotation["classification"] == "no direct visual effect"
            and "synthetic packed table" in rotation["scope"] and "typed GTE services" in rotation["scope"]
            and (rotation["operations"], rotation["reads"], rotation["stores"], rotation["multiplies"]) == (15, 6, 9, 14)
            and rotation["angles"] == [1, 1, 1]
            and rotation["matrix_before"] == [1, 65535, 32767, 0, 0, 0, 0, 0, 0]
            and rotation["matrix_return"] == [4096, 61440, 4096, 8192, 0, 61440, 0, 8192, 4096]
            and rotation["matrix_after"] == [6553, 58983, 6553, 8192, 0, 61440, 0, 8192, 4096]
            and (rotation["entry_pc"], rotation["returned_value"], rotation["returned_sp"], rotation["return_address"]) ==
                (0x80051168, 0x800F9FD8, 0x801FEFD0, 0x80051170)
            and (rotation["hi"], rotation["lo"]) == (0, 16777216), "rotation matrix native state drifted")
    (args.frames / "rotation_matrix_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x80056080", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": rotation,
        "classification": "no direct visual effect"
    }, indent=2)+"\n", encoding="utf-8")
    overlay = period["camera_overlay_packets_probe"]
    require((overlay["program"], overlay["address"], overlay["inclusive_end"], overlay["bytes"], overlay["instructions"]) ==
            ("GAMEONLY", "0x80075D40", "0x80076273", 1332, 333), "camera overlay provenance drifted")
    require(overlay["completed"] and not overlay["frame_completed"] and overlay["frame_stopped_pc"] == 0x800490E8
            and overlay["classification"] == "no direct visual effect"
            and "independent full machine" in overlay["scope"] and "recovered packet linker" in overlay["scope"]
            and overlay["links"] == 2 and overlay["callbacks"] == 2
            and (overlay["operations"], overlay["reads"], overlay["stores"]) == (22, 15, 5)
            and overlay["table_address"] == 0x800F5C50 and overlay["table_before"] == 0 and overlay["packet_before"] == [0x654321, 0xABCDEF]
            and overlay["table_after"] == 0xFA284 and overlay["packet_after"] == [0, 0xFA25C]
            and overlay["returned_sp"] == 0x801FF000 and overlay["restored_ra"] == 0x800490D0,
            "camera overlay native packet state drifted")
    (args.frames / "camera_overlay_packets_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x80075D40", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": overlay,
        "classification": "no direct visual effect"
    }, indent=2)+"\n", encoding="utf-8")
    clear_table = period["clear_ordering_table_probe"]
    require((clear_table["program"], clear_table["address"], clear_table["inclusive_end"], clear_table["bytes"], clear_table["instructions"]) ==
            ("GAMEONLY", "0x80099960", "0x800999F7", 152, 38), "clear-table provenance drifted")
    require(clear_table["completed"] and clear_table["frame_completed"]
            and clear_table["classification"] == "no direct visual effect"
            and "independent full entry machines" in clear_table["scope"] and "typed clear backend" in clear_table["scope"]
            and clear_table["heads_before"] == [0, 0] and clear_table["heads_after"] == [0xC567C, 0xC567C],
            "clear-table head state drifted")
    for i, call in enumerate(clear_table["calls"]):
        pc = [0x80049084, 0x80049094][i]
        require(call == {"pc": pc, "count": [32, 4096][i], "target": 0x8009A97C, "operations": 11,
                         "returned_sp": 0x80180000 + (i + 1) * 0x100, "restored_ra": pc + 8},
                "clear-table native call state drifted")
    require(len(clear_table["calls"]) == 2, "clear-table call count drifted")
    (args.frames / "clear_ordering_table_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x80099960", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": clear_table,
        "classification": "no direct visual effect"
    }, indent=2)+"\n", encoding="utf-8")
    interrupt_restore = period["frame_interrupt_restore_probe"]
    require((interrupt_restore["program"], interrupt_restore["address"], interrupt_restore["inclusive_end"],
             interrupt_restore["bytes"], interrupt_restore["instructions"]) ==
            ("GAMEONLY", "0x8004900C", "0x80049017", 12, 3), "interrupt-restore provenance drifted")
    require(interrupt_restore["completed"] and interrupt_restore["frame_completed"]
            and interrupt_restore["classification"] == "no direct visual effect"
            and "recovered disable and restore" in interrupt_restore["scope"]
            and "typed rendering fixtures" in interrupt_restore["scope"]
            and (interrupt_restore["status_disabled"], interrupt_restore["status_restored"]) == (0xABCDEF00, 0xABCDEF01)
            and (interrupt_restore["disable_completions"], interrupt_restore["restore_completions"], interrupt_restore["operations_per_call"]) == (13, 13, 1)
            and interrupt_restore["call_counts"] == [1, 10, 1, 1]
            and interrupt_restore["call_pcs"] == [0x8004909C, 0x800491D8, 0x8004926C, 0x800492C0],
            "interrupt-restore native CP0 state drifted")
    (args.frames / "frame_interrupt_restore_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x8004900C", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": interrupt_restore,
        "classification": "no direct visual effect"
    }, indent=2)+"\n", encoding="utf-8")
    collision = period["actor_collision_response_probe"]
    require((collision["program"], collision["address"], collision["inclusive_end"], collision["bytes"], collision["instructions"]) ==
            ("GAMEONLY", "0x8005F3BC", "0x8005F887", 1228, 307), "collision response provenance drifted")
    require(collision["completed"] and collision["parent_completed"]
            and collision["classification"] == "no direct visual effect"
            and "independent CPU fixture" in collision["scope"] and "typed impulse service" in collision["scope"]
            and collision["contact_before"] == [0, 0, 0] and collision["contact_after"] == [9, 120, 1]
            and collision["normal"] == [256, 0] and collision["callbacks"] == 2
            and (collision["operations"], collision["reads"], collision["stores"]) == (51, 30, 19)
            and (collision["normal_velocity"], collision["tangent_velocity"], collision["parent_returned_value"]) == (64, 0, 1)
            and (collision["resolver_pc"], collision["resolver_argument_count"]) == (0x8005F598, 8)
            and (collision["frame_stack_pointer"], collision["returned_sp"], collision["restored_ra"], collision["parent_restored_ra"]) ==
                (0x800FEF90, 0x800FEFE8, 0x8005F934, 0x81234568), "collision response native state drifted")
    (args.frames / "actor_collision_response_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x8005F3BC", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": collision,
        "classification": "no direct visual effect"
    }, indent=2)+"\n", encoding="utf-8")
    tactics = period["team_tactics_probe"]
    require((tactics["program"], tactics["address"], tactics["end"], tactics["bytes"], tactics["instructions"])
            == ("GAMEONLY", "0x800747B0", "0x80075D3F", 5520, 1380), "Team tactics provenance drifted")
    require(tactics["classification"] == "no direct visual effect" and tactics["completed"]
            and tactics["same_parent_memory"] and tactics["actual_call_pc"] == "0x80068E28"
            and "independent full-machine snapshot" in tactics["entry_machine"]
            and (tactics["operations"], tactics["reads"], tactics["stores"], tactics["callbacks"],
                 tactics["actor_iterations"], tactics["opposing_actor_iterations"])
            == (342, 223, 96, 23, 5, 5), "Team tactics execution drifted")
    require(tactics["before"] == {"defense_timer": 10}
            and tactics["after"] == {"defense_timer": 9, "actor0_possession_distance": 100,
                                      "actor0_basket_distance": 100, "opposing_minimum": 100}
            and (tactics["output_sp"], tactics["output_ra"], tactics["parent_stop_pc"], tactics["parent_stop_entry"])
            == ("0x801FF000", "0x80068E30", "0x80068E30", "0x8006817C"), "Team tactics state drifted")
    require(tactics["child_call_sites"] == ["0x800749CC", "0x800749F0"] * 5 + ["0x80074AE8"]
            + ["0x80074C1C", "0x80074C44"] * 5 + ["0x80074D30", "0x80075458"],
            "Team tactics typed child order drifted")
    (args.frames / "team_tactics_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x800747B0", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": tactics,
        "classification": "no direct visual effect"
    }, indent=2) + "\n", encoding="utf-8")
    cross_half = period["cross_half_rule_probe"]
    require((cross_half["program"],cross_half["address"],cross_half["inclusive_end"],cross_half["bytes"],cross_half["instructions"]) == ("GAMEONLY","0x8006817C","0x8006830B",400,100), "Crossing rule provenance drifted")
    require(cross_half["classification"]=="no direct visual effect" and "independent synthetic actual match-tick caller" in cross_half["scope"] and "no advancing match" in cross_half["scope"], "Crossing rule scope drifted")
    require(cross_half["completed"] and not cross_half["parent_completed"] and cross_half["same_parent_memory"]
            and (cross_half["call_pc"],cross_half["operations"],cross_half["reads"],cross_half["stores"],cross_half["callbacks"],cross_half["prerequisite_events"],cross_half["duration_noop_calls"]) == (0x80068E30,23,14,4,5,16,1)
            and (cross_half["timer_before"],cross_half["timer_after"],cross_half["blocker_before"],cross_half["blocker_after"],cross_half["rule_before"],cross_half["rule_after"]) == (12,13,1,0,0,8)
            and (cross_half["sp"],cross_half["ra"],cross_half["parent_stop_pc"],cross_half["parent_stop_entry"]) == (0x800FF000,0x80068E38,0x80068E78,0x80076B28)
            and cross_half["hilo_known_masks"] == [3,12] and cross_half["typed_child_pcs"] == [0x80068290,0x800682B4,0x800682D8,0x800682E0], "Crossing rule natural state drifted")
    (args.frames / "cross_half_rule_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8006817C","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":cross_half,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    timers = cross_half["actor_timers"]
    stamina = cross_half["stamina_handicap"]
    require((stamina["program"], stamina["address"], stamina["inclusive_end"], stamina["bytes"], stamina["instructions"])
            == ("GAMEONLY", "0x80068504", "0x800686B7", 436, 109), "Stamina provenance drifted")
    require(stamina["classification"] == "no direct visual effect" and stamina["completed"]
            and stamina["same_parent_memory"] and stamina["machine_from_actor_timers"]
            and (stamina["call_pc"], stamina["score_updates"], stamina["stamina_updates"], stamina["handicap_after"], stamina["score_before"], stamina["score_after"], stamina["stamina_before"], stamina["stamina_after"], stamina["flag_before"], stamina["flag_after"], stamina["countdown_after_delay"], stamina["sp"], stamina["ra"])
            == (0x80068E60, 24, 10, 5, 10, 11, 20, 16, 1, 0, 59, 0x800FF000, 0x80068E68)
            and stamina["delta_address"] == "0x800FDB7E"
            and stamina["operations"] == stamina["reads"] + stamina["stores"], "Stamina natural state drifted")
    (args.frames / "stamina_handicap_verified.json").write_text(json.dumps({
        "program":"GAMEONLY", "address":"0x80068504", "driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes, "cpu_receipt":"loop_entry_trace.json", "state":stamina,
        "classification":"no direct visual effect"
    }, indent=2)+"\n", encoding="utf-8")
    require((timers["program"], timers["address"], timers["inclusive_end"], timers["bytes"], timers["instructions"])
            == ("GAMEONLY", "0x8006830C", "0x80068503", 504, 126), "Actor timers provenance drifted")
    require(timers["classification"] == "no direct visual effect" and timers["completed"] and timers["same_parent_memory"] and timers["machine_from_crossing_rule"]
            and (timers["call_pc"],timers["actor_count"],timers["team_updates"],timers["participation_updates"],timers["multiply_count"],timers["sp"],timers["ra"])
            == (0x80068E38,11,10,1,11,0x800FF000,0x80068E40)
            and (timers["operations"], timers["reads"], timers["stores"]) == (240,156,84)
            and timers["timers_before"] == [5,1,2] and timers["timers_after"] == [4,0,1]
            and (timers["cache_before"],timers["cache_after"],timers["participation_before"],timers["participation_after"]) == (59,60,5,6), "Actor timers natural state drifted")
    (args.frames / "actor_timers_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8006830C","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":timers,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    frame_ui = period["frame_ui_service_probe"]
    require((frame_ui["program"],frame_ui["address"],frame_ui["inclusive_end"],frame_ui["bytes"],frame_ui["instructions"]) == ("GAMEONLY","0x80032B10","0x80032BB7",168,42), "Frame UI provenance drifted")
    require(frame_ui["classification"] == "BLOCKED" and "independent synthetic actual match-tick caller" in frame_ui["scope"] and "no rendered match frame" in frame_ui["scope"], "Frame UI scope drifted")
    require(frame_ui["completed"] and frame_ui["parent_completed"] and frame_ui["same_parent_memory"]
            and (frame_ui["call_pc"],frame_ui["instruction_count"],frame_ui["reads"],frame_ui["stores"],frame_ui["callbacks"],frame_ui["prerequisite_calls"],frame_ui["synthetic_frame_completions"],frame_ui["v0"],frame_ui["sp"],frame_ui["ra"]) == (0x8002DDAC,18,3,1,1,31,1,1,0x801FF000,0x8002DDB4)
            and frame_ui["hilo_known_masks"] == [7,11]
            and frame_ui["blocked_children"] == ["0x80031C5C","0x8003066C","0x80032774"], "Frame UI natural state drifted")
    countdown = frame_ui["countdown_update"]
    upload = frame_ui["image_record_upload"]
    require((upload["program"], upload["address"], upload["inclusive_end"], upload["bytes"], upload["instructions"])
            == ("GAMEONLY", "0x80094540", "0x800946A3", 356, 89), "Image upload provenance drifted")
    require(upload["classification"] == "BLOCKED" and "independent synthetic active countdown caller" in upload["scope"]
            and upload["completed"] and upload["parent_completed"] and upload["same_parent_memory"]
            and (upload["call_pc"], upload["operations"], upload["reads"], upload["stores"], upload["callbacks"], upload["records"], upload["sp"], upload["ra"])
            == (0x80032AE4, 25, 11, 13, 1, 1, 0x801FEFC0, 0x80032AEC)
            and (upload["header_before"], upload["header_after"], upload["cache_before"], upload["cache_after"])
            == (0x23, 0x2B, 65535, 2) and upload["rectangle"] == [0x340, 0xF0, 0x10, 1]
            and upload["blocked_children"] == ["0x800A3BF8"], "Image upload natural state drifted")
    (args.frames / "image_record_upload_verified.json").write_text(json.dumps({
        "program":"GAMEONLY", "address":"0x80094540", "driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes, "cpu_receipt":"loop_entry_trace.json", "state":upload,
        "classification":"BLOCKED"
    }, indent=2)+"\n", encoding="utf-8")
    text_submission = upload["text_submission"]
    require((text_submission["program"],text_submission["address"],text_submission["inclusive_end"],text_submission["bytes"],text_submission["instructions"])
            == ("GAMEONLY","0x80030D18","0x80031523",2060,515), "Text submission provenance drifted")
    require(text_submission["classification"] == "BLOCKED" and text_submission["completed"] and text_submission["same_parent_memory"]
            and (text_submission["call_pc"],text_submission["callbacks"],text_submission["record"],text_submission["sp"],text_submission["ra"],text_submission["clear_owners"],text_submission["clear_backend_calls"],text_submission["head_before"],text_submission["head_after"],text_submission["record_slot"])
            == (0x800329E8,4,0x80120000,0x801FEFC0,0x800329F0,2,2,65535,0,201)
            and (text_submission["instruction_count"],text_submission["operations"],text_submission["reads"],text_submission["stores"]) == (162,72,35,33)
            and text_submission["record_heads"] == [0xC567C,0xC567C]
            and text_submission["blocked_children"] == ["0x8002EB50","0x8002EF88","0x8002ECD4","0x800AA468","0x80056914"]
            and text_submission["blocked_clear_backend"] == "0x8009A97C", "Text submission natural state drifted")
    (args.frames / "text_submission_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80030D18","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":text_submission,
        "classification":"BLOCKED"
    },indent=2)+"\n",encoding="utf-8")
    submit = upload["rectangle_upload_submit"]
    require((submit["program"],submit["address"],submit["inclusive_end"],submit["bytes"],submit["instructions"])
            == ("GAMEONLY","0x800944F4","0x8009453F",76,19), "Rectangle submit provenance drifted")
    require(submit["classification"] == "BLOCKED" and submit["completed"] and submit["same_parent_memory"]
            and (submit["call_pc"],submit["instruction_count"],submit["operations"],submit["reads"],submit["stores"],submit["callbacks"],submit["sp"],submit["ra"],submit["pending_before"],submit["pending_after"])
            == (0x8009464C,19,9,3,4,2,0x801FEF90,0x80094654,0,1)
            and submit["blocked_children"] == ["0x8009971C"], "Rectangle submit state drifted")
    (args.frames / "rectangle_upload_submit_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800944F4","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":submit,
        "classification":"BLOCKED"
    },indent=2)+"\n",encoding="utf-8")
    normalized = submit["rectangle_normalize"]
    require((normalized["program"],normalized["address"],normalized["inclusive_end"],normalized["bytes"],normalized["instructions"])
            == ("GAMEONLY","0x80094440","0x8009446B",44,11), "Rectangle normalization provenance drifted")
    require(normalized["classification"] == "no direct visual effect" and normalized["completed"] and normalized["same_parent_memory"]
            and (normalized["call_pc"],normalized["instruction_count"],normalized["operations"],normalized["reads"],normalized["stores"],normalized["sp"],normalized["ra"])
            == (0x80094508,7,1,1,0,0x801FEF70,0x80094510)
            and normalized["even_rectangle"] == [16,1] and normalized["odd_before"] == [17,2] and normalized["odd_after"] == [17,3]
            and (normalized["odd_instruction_count"],normalized["odd_operations"],normalized["odd_reads"],normalized["odd_stores"]) == (11,3,2,1)
            and "independent synthetic CQ invocation" in normalized["odd_scope"], "Rectangle normalization state drifted")
    (args.frames / "rectangle_normalize_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80094440","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":normalized,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    require((countdown["program"], countdown["address"], countdown["inclusive_end"], countdown["bytes"], countdown["instructions"])
            == ("GAMEONLY", "0x8003287C", "0x80032B0F", 660, 165), "Countdown provenance drifted")
    require(countdown["classification"] == "BLOCKED" and countdown["completed"] and countdown["same_parent_memory"]
            and (countdown["call_pc"], countdown["cache_before"], countdown["cache_after"], countdown["generated_table_bytes"], countdown["callbacks"], countdown["sp"], countdown["ra"])
            == (0x80032B18, 7, 65535, 22, 1, 0x801FEFE8, 0x80032B20)
            and (countdown["instruction_count"], countdown["operations"], countdown["reads"], countdown["stores"]) == (52, 34, 17, 16)
            and countdown["blocked_children"] == ["0x80030D18", "0x80094540"], "Countdown natural state drifted")
    text_clear = countdown["text_chain_clear"]
    require((text_clear["program"],text_clear["address"],text_clear["inclusive_end"],text_clear["bytes"],text_clear["instructions"])
            == ("GAMEONLY","0x8003066C","0x800306E7",124,31), "Text-chain provenance drifted")
    require(text_clear["classification"] == "BLOCKED" and text_clear["completed"] and text_clear["same_parent_memory"]
            and (text_clear["call_pc"],text_clear["instruction_count"],text_clear["operations"],text_clear["reads"],text_clear["stores"],text_clear["chain_iterations"],text_clear["slot"],text_clear["head_before"],text_clear["head_after"],text_clear["sp"],text_clear["ra"])
            == (0x8003295C,39,11,8,3,2,201,3,65535,0x801FEFA8,0x80032964)
            and text_clear["link_flags_before"] == [0x7777,0x8888] and text_clear["link_flags_after"] == [0,0], "Text-chain native state drifted")
    (args.frames / "text_chain_clear_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8003066C","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":text_clear,
        "classification":"BLOCKED"
    },indent=2)+"\n",encoding="utf-8")
    (args.frames / "countdown_ui_update_verified.json").write_text(json.dumps({
        "program":"GAMEONLY", "address":"0x8003287C", "driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes, "cpu_receipt":"loop_entry_trace.json", "state":countdown,
        "classification":"BLOCKED"
    }, indent=2)+"\n", encoding="utf-8")
    order=frame_ui["ordered_checkpoint_indices"]
    require(len(order)==6 and all(a<b for a,b in zip(order,order[1:])), "Frame UI source order drifted")
    (args.frames / "frame_ui_service_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80032B10","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":frame_ui,
        "classification":"BLOCKED"
    },indent=2)+"\n",encoding="utf-8")
    phase = period["camera_phase_select_probe"]
    require((phase["program"],phase["address"],phase["inclusive_end"],phase["bytes"],phase["instructions"]) == ("GAMEONLY","0x8007E26C","0x8007E463",504,126), "Camera phase provenance drifted")
    require(phase["classification"] == "no direct visual effect" and "independent synthetic" in phase["scope"], "Camera phase scope drifted")
    require(len(phase["cases"]) == 2, "Camera phase cases missing")
    for i, case in enumerate(phase["cases"]):
        require(case["completed"] and case["same_parent_memory"] and case["busy_before"] == [0,7][i]
                and (case["call_pc"],case["phase_after"],case["published_phase"],case["busy_after"],case["phase_changed"],case["nested_camera_calls"],case["return_address"],case["sp"]) == (0x80079A0C,1,1,0,i,i,0x80079A14,0x801FEFA8)
                and (case["operations"],case["reads"],case["stores"],case["callbacks"]) == [(10,7,3,0),(17,7,6,4)][i]
                and case["camera_child_pcs"] == ([] if i==0 else [0x80079B7C,0x80079C2C,0x80079D0C])
                and case["hilo_known_masks"] == [0,0]
                and case["adjustment_pcs"] == ([] if i==0 else [0x8007E3A8,0x8007E3B0,0x8007E3B8])
                and case["adjustment_args"] == ([] if i==0 else [15,8,8]), "Camera phase natural composition drifted")
    (args.frames / "camera_phase_select_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8007E26C","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":phase,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    elapsed_cases=[selection["elapsed_dispatch"],phase["cases"][1]["elapsed_dispatch"]]
    require(phase["cases"][0]["elapsed_dispatch"] is None,"Unchanged phase unexpectedly dispatched elapsed owner")
    for i,elapsed in enumerate(elapsed_cases):
        require((elapsed["program"],elapsed["address"],elapsed["inclusive_end"],elapsed["bytes"],elapsed["instructions"]) == ("GAMEONLY","0x800798B4","0x800799CB",280,70),"Camera elapsed provenance drifted")
        require(elapsed["classification"]=="no direct visual effect" and "actual camera-selector caller" in elapsed["scope"] and "no advancing match" in elapsed["scope"],"Camera elapsed scope drifted")
        pc=[0x80079C8C,0x80079C2C][i]
        require(elapsed["completed"] and (elapsed["call_pc"],elapsed["delay_pc"],elapsed["return_address"],elapsed["requested_delta"]) == (pc,pc+4,pc+8,0xFFFFFFFF)
                and (elapsed["operations"],elapsed["reads"],elapsed["stores"],elapsed["callbacks"],elapsed["instruction_count"]) == (14,8,5,1,48)
                and (elapsed["elapsed_after"],elapsed["cache_before"],elapsed["cache_after"],elapsed["publication_after"],elapsed["child_pc"]) == (0,0xFFFFFFFF,42,42,0x8007999C)
                and elapsed["sp"] == [0x801FFE90,0x801FEF38][i] and elapsed["hilo_known_masks"]==[0,0],"Camera elapsed native state drifted")
        lookup = elapsed["state_lookup"]
        gate = elapsed["remainder_gate"]
        require((gate["program"],gate["address"],gate["inclusive_end"],gate["bytes"],gate["instructions"])
                == ("GAMEONLY","0x8007A468","0x8007A497",48,12), "Camera remainder provenance drifted")
        require(gate["classification"] == "no direct visual effect" and gate["completed"]
                and gate["parent_completed"] and gate["same_parent_memory"]
                and "explicit second elapsed dispatch" in gate["scope"]
                and (gate["call_pc"],gate["operations"],gate["reads"],gate["ra"],gate["cache_after"],gate["publication_after"],gate["elapsed_after"])
                == (0x80079978,1,1,0x80079980,42,42,0)
                and (gate["source"],gate["remainder"],gate["returned_value"],gate["instruction_count"],gate["lookup_completions"])
                == [(0,0,1,11,0),(0xFFFFFF00,0xFFFFFF00,0,12,1)][i]
                and gate["sp"] == [0x801FFE78,0x801FEF20][i], "Camera remainder native state drifted")
        require((lookup["program"],lookup["address"],lookup["inclusive_end"],lookup["bytes"],lookup["instructions"])
                == ("GAMEONLY","0x8007A410","0x8007A467",88,22), "Camera lookup provenance drifted")
        require(lookup["classification"] == "no direct visual effect" and lookup["completed"] and lookup["same_parent_memory"]
                and (lookup["call_pc"],lookup["operations"],lookup["reads"],lookup["raw_return"],lookup["ra"])
                == (0x8007999C,2,2,42,0x800799A4)
                and (lookup["source"],lookup["signed_index"],lookup["negative_table"],lookup["lookup_address"],lookup["instruction_count"])
                == [(0,0,0,0x800BC204,16),(0xFFFFFF00,7,1,0x800BC240,15)][i]
                and lookup["sp"] == [0x801FFE78,0x801FEF20][i], "Camera lookup signed path drifted")
    (args.frames / "camera_remainder_gate_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8007A468","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json",
        "cases":[case["remainder_gate"] for case in elapsed_cases], "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    (args.frames / "camera_state_lookup_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8007A410","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json",
        "cases":[case["state_lookup"] for case in elapsed_cases], "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    (args.frames / "camera_elapsed_dispatch_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800798B4","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","cases":elapsed_cases,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    camera_end = period["camera_override_end_probe"]
    require((camera_end["program"], camera_end["address"], camera_end["inclusive_end"],
             camera_end["bytes"], camera_end["instructions"]) ==
            ("GAMEONLY", "0x8007A36C", "0x8007A39F", 52, 13), "camera teardown provenance drifted")
    require(camera_end["completed"] and camera_end["classification"] == "no direct visual effect"
            and "independent full machine" in camera_end["scope"] and "typed camera restore" in camera_end["scope"]
            and (camera_end["flag_before"], camera_end["flag_after"], camera_end["tail_before"], camera_end["tail_after"]) == (1, 0, 2, 1)
            and (camera_end["selection_writes"], camera_end["selected"], camera_end["claim"]) == (1, 4, 0)
            and (camera_end["operations"], camera_end["reads"], camera_end["stores"], camera_end["callbacks"]) == (5, 2, 2, 1)
            and camera_end["returned_value"] == 0xCAFEBABE
            and (camera_end["frame_stack_pointer"], camera_end["returned_sp"], camera_end["restored_ra"]) ==
                (0x801FEFE8, 0x801FF000, 0x80065578), "camera teardown ordered CPU state drifted")
    (args.frames / "camera_override_end_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x8007A36C", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": camera_end,
        "classification": "no direct visual effect"
    }, indent=2)+"\n", encoding="utf-8")
    opponent = period["opponent_contact_probe"]
    require((opponent["program"], opponent["address"], opponent["inclusive_end"],
             opponent["bytes"], opponent["instructions"]) ==
            ("GAMEONLY", "0x8005F888", "0x8005F947", 192, 48), "opponent contact provenance drifted")
    require(opponent["completed"] and opponent["parent_completed"]
            and opponent["classification"] == "no direct visual effect"
            and "independent CPU fixture" in opponent["scope"] and "typed collision response" in opponent["scope"]
            and (opponent["geometry_calls"], opponent["action_calls"]) == (1, 1)
            and (opponent["operations"], opponent["reads"], opponent["stores"], opponent["callbacks"]) == (9, 7, 1, 1)
            and opponent["input_pair"] == [0x80010000, 0x80010200]
            and opponent["dispatched_pair"] == [0x80010200, 0x80010000]
            and (opponent["owner"], opponent["first_id"], opponent["returned_value"], opponent["parent_returned_value"]) == (7, 100, 0xCD, 0xCD)
            and (opponent["frame_stack_pointer"], opponent["returned_sp"], opponent["restored_ra"]) ==
                (0x801FEFC8, 0x801FEFE0, 0x8005FA34), "opponent contact ordered CPU state drifted")
    (args.frames / "opponent_contact_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x8005F888", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": opponent,
        "classification": "no direct visual effect"
    }, indent=2)+"\n", encoding="utf-8")
    actor_resume = period["actor_resume_period_probe"]
    require((actor_resume["program"], actor_resume["address"], actor_resume["inclusive_end"],
             actor_resume["bytes"], actor_resume["instructions"]) ==
            ("GAMEONLY", "0x800582DC", "0x800583FB", 288, 72), "actor resume provenance drifted")
    require(actor_resume["completed"] and actor_resume["parent_completed"]
            and actor_resume["classification"] == "no direct visual effect"
            and "independent zero-clock actor fixture" in actor_resume["scope"]
            and actor_resume["call_pc"] == 0x800676CC
            and (actor_resume["operations"], actor_resume["reads"], actor_resume["stores"]) == (22,12,7)
            and actor_resume["actor"] == 0x80160000
            and (actor_resume["state_before"], actor_resume["state_after"], actor_resume["animation_before"]) == (27,1,[37,36])
            and (actor_resume["cleared_4e"], actor_resume["flags_9a"], actor_resume["field_b8"], actor_resume["copied_a6"]) == (0,3,47,0x1234)
            and actor_resume["call_pcs"] == [0x80058374,0x8005837C,0x800583E0]
            and (actor_resume["frame_stack_pointer"], actor_resume["returned_sp"], actor_resume["restored_ra"]) ==
                (0x801FFEC8,0x801FFEE0,0x800676D4)
            and (actor_resume["parent_returned_value"], actor_resume["parent_restored_ra"], actor_resume["parent_phase"],
                 actor_resume["parent_owner"], actor_resume["parent_actor"], actor_resume["parent_actor_timer"]) ==
                (0,0x80068D74,0,0xFFFF,0x80161000,30), "actor resume native CPU fixture drifted")
    (args.frames / "actor_resume_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800582DC","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":actor_resume,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    require((period["program"], period["address"], period["inclusive_end"], period["bytes"], period["instructions"]) ==
            ("GAMEONLY", "0x80067468", "0x8006754F", 232, 58), "period startup provenance drifted")
    require(period["completed"] and "explicit synthetic" in period["scope"]
            and period["classification"] == "no direct visual effect"
            and (period["operations"], period["reads"], period["stores"], period["calls"]) == (23, 5, 5, 13)
            and period["call_pcs"] == [0x80067470,0x80067478,0x800674A4,0x800674AC,0x800674B8,0x800674C0,
                0x800674E0,0x800674F0,0x800674F8,0x80067500,0x80067508,0x80067510,0x80067518]
            and (period["signed_selector"], period["published_pointer"], period["pre_pump_counter"], period["post_pump_delta"]) ==
                (0xFFFF8000, 0x80123400, 0x4321, 0x8765)
            and period["frame_stack_pointer"] == 0x801FFEE8 and period["restored_ra"] == 0x80068C54
            and (period["next_pc"], period["next_entry"], period["simulation_steps"], period["frame_pumps"]) ==
                (0x80068D84, 0x8006801C, 0, 0), "period startup native CPU fixture drifted")
    (args.frames / "period_startup_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x80067468", "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": loop_hashes, "cpu_receipt": "loop_entry_trace.json", "state": period,
        "classification": "no direct visual effect"
    }, indent=2) + "\n", encoding="utf-8")
    record_cases = [case["match_buffer_record"] for case in [period]+period["zero_period_cases"]]
    for record in record_cases:
        require((record["program"],record["address"],record["inclusive_end"],record["bytes"],record["instructions"]) == ("GAMEONLY","0x80076B3C","0x80076FC7",1164,291), "Match-buffer record provenance drifted")
        require(record["completed"] and record["same_parent_memory"] and record["classification"] == "no direct visual effect" and len(record["calls"]) == 2, "Match-buffer record composition drifted")
        for i, (item,pc) in enumerate(zip(record["calls"],(0x800674F8,0x80067508))):
            require(item["fields_verified"] and (item["call_pc"],item["entity_iterations"],item["snapshot"],item["pending_before"],item["pending_after"],item["cursor"],item["rewind_calls"],item["zero_stores"],item["return_address"],item["sp"]) == (pc,11,[0x800F1814,0x800F1918][i],1,0,item["compression"]["returned_pointer"],1-i,2*(1-i),pc+8,0x801FFEE8)
                    and item["compression_args"] == [[0x800F1814,0x800F1918][i],[0x800F1918,0x800F1814][i],item["compression"]["output"],130] and item["hilo_known_masks"] == [0,0]
                    and (item["operations"],item["reads"],item["stores"],item["callbacks"]) == (405-i,209,194,2-i), "Match-buffer record state drifted")
    (args.frames / "match_buffer_record_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80076B3C","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":record_cases,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    compression_cases = []
    for record in record_cases:
        cursor = 0x800CCC00
        for i,item in enumerate(record["calls"]):
            compression = item["compression"]; compression_cases.append(compression)
            require((compression["program"],compression["address"],compression["inclusive_end"],compression["bytes"],compression["instructions"]) == ("GAMEONLY","0x800767FC","0x800768EF",244,61), "Match-buffer compressor provenance drifted")
            n = compression["encoded_length"]
            require(compression["completed"] and compression["same_parent_memory"] and compression["packet_decoded"] and compression["classification"] == "no direct visual effect"
                    and (compression["call_pc"],compression["output"],compression["count"],compression["operations"],compression["reads"],compression["stores"],compression["elements"],compression["full_flag_groups"],compression["toggle_before"],compression["toggle_after"],compression["returned_pointer"],compression["return_address"],compression["sp"]) == (0x80076E58,cursor,130,262+n,261,n+1,130,32,i,1-i,cursor+n,0x80076E60,0x801FFED0)
                    and compression["hilo_known_masks"] == [0,0], "Match-buffer compressor packet or machine drifted")
            cursor += n
    (args.frames / "match_buffer_compress_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800767FC","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":compression_cases,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    pending_cases = [case["match_buffer_pending"] for case in [period]+period["zero_period_cases"]]
    for pending in pending_cases:
        require((pending["program"],pending["address"],pending["inclusive_end"],pending["bytes"],pending["instructions"]) == ("GAMEONLY","0x80076B28","0x80076B3B",20,5), "Match-buffer pending provenance drifted")
        require(pending["completed"] and pending["same_parent_memory"] and pending["classification"] == "no direct visual effect" and len(pending["calls"]) == 2, "Match-buffer pending composition drifted")
        for item, pc in zip(pending["calls"], (0x800674F0,0x80067500)):
            require((item["call_pc"],item["operations"],item["stores"],item["address"],item["value"],item["return_v0"],item["at"],item["return_address"],item["sp"]) == (pc,1,1,0x800FE864,1,1,0x80100000,pc+8,0x801FFEE8) and item["hilo_known_masks"] == [0,0], "Match-buffer pending state drifted")
    (args.frames / "match_buffer_pending_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80076B28","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":pending_cases,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    first_cases = period["zero_period_cases"]
    require(len(first_cases) == 2, "first-period capture cases missing")
    for case, flag in zip(first_cases, (0,255)):
        first = case["first_period_startup"]
        require((first["program"],first["address"],first["inclusive_end"],first["bytes"],first["instructions"]) ==
                ("GAMEONLY","0x800673F0","0x80067467",120,30), "first-period provenance drifted")
        expected_calls = [0x800673F8,0x80067400] + ([0x8006741C,0x80067424] if flag else []) + [0x80067434,0x80067448,0x80067450]
        require(first["completed"] and first["classification"] == "no direct visual effect"
                and "explicit synthetic" in first["scope"] and first["flag"] == flag
                and (first["operations"],first["reads"],first["stores"]) == ((12,2,3) if flag else (9,2,2))
                and first["call_pcs"] == expected_calls and first["marker"] == 0xFFFF
                and first["presentation_halfword"] == (0 if flag else 0xBEEF)
                and first["frame_stack_pointer"] == 0x801FFED0 and first["restored_ra"] == 0x8006749C
                and case["signed_selector"] == 0 and case["call_pcs"][2] == 0x80067494
                and (case["next_pc"],case["next_entry"],case["simulation_steps"],case["frame_pumps"]) ==
                    (0x80068D84,0x8006801C,0,0), "first-period native CPU fixture drifted")
    (args.frames / "first_period_startup_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800673F0","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":first_cases,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    audio_noops = [case["first_period_startup"]["audio_noop"] for case in first_cases]
    for i, noop in enumerate(audio_noops):
        require((noop["program"],noop["address"],noop["inclusive_end"],noop["bytes"],noop["instructions"]) == ("GAMEONLY","0x8002A254","0x8002A25B",8,2), "Audio no-op provenance drifted")
        require(noop["completed"] and noop["same_parent_memory"] and noop["memory_and_registers_unchanged"] and noop["classification"] == "no direct visual effect"
                and (noop["call_pc"],noop["operations"],noop["accesses"],noop["a0"],noop["v0"],noop["return_address"],noop["sp"]) == (0x80067434,0,0,1,[0,0x8008048C][i],0x8006743C,0x801FFED0)
                and noop["hilo_known_masks"] == [0,0], "Audio no-op state drifted")
    (args.frames / "period_audio_noop_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8002A254","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":audio_noops,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    audio_flags = [case["first_period_startup"]["audio_flag_clear"] for case in first_cases]
    for flag in audio_flags:
        require((flag["program"],flag["address"],flag["inclusive_end"],flag["bytes"],flag["instructions"]) == ("GAMEONLY","0x8002A244","0x8002A253",16,4), "Audio flag clear provenance drifted")
        require(flag["completed"] and flag["same_parent_memory"] and flag["v0_preserved"] and flag["classification"] == "no direct visual effect"
                and (flag["call_pc"],flag["flag_before"],flag["flag_after"],flag["operations"],flag["stores"],flag["store_address"],flag["store_pc"],flag["at"],flag["v0"],flag["return_address"],flag["sp"]) == (0x80067400,215,0,1,1,0x800B1FD5,0x8002A248,0x800B0000,1,0x80067408,0x801FFED0)
                and flag["hilo_known_masks"] == [0,0], "Audio flag clear state drifted")
    (args.frames / "period_audio_flag_clear_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8002A244","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":audio_flags,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    music_cases = [case["first_period_startup"]["music_start"] for case in first_cases]
    for i, music in enumerate(music_cases):
        require((music["program"],music["address"],music["inclusive_end"],music["bytes"],music["instructions"]) == ("GAMEONLY","0x800295D0","0x8002968B",188,47), "Period music provenance drifted")
        require(music["completed"] and music["same_parent_memory"] and music["classification"] == "no direct visual effect"
                and (music["call_pc"],music["volume_fixture"],music["loaded_before"],music["loaded_after"],music["playing_after"],music["scaled_volume"],music["operations"],music["reads"],music["stores"],music["callbacks"],music["load_executed"],music["return_address"],music["sp"]) == (0x800673F8,14+i,i,1,1,126+i,16-4*i,7-2*i,4-i,5-i,1-i,0x80067400,0x801FFED0)
                and music["hilo_known_masks"] == [0,0]
                and music["call_pcs"] == ([0x80029618] if i==0 else [])+[0x8002964C,0x80029654,0x8002965C,0x80029664]
                and music["arguments"] == ([0x80150000,0x80160000] if i==0 else [])+[0,0,126+i,120], "Period music state or calls drifted")
    (args.frames / "period_music_start_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800295D0","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":music_cases,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    finish_cases = [case["first_period_startup"]["presentation_finish"] for case in first_cases]
    require(finish_cases[0] is None, "Skipped presentation unexpectedly ran")
    finish = finish_cases[1]
    require((finish["program"],finish["address"],finish["inclusive_end"],finish["bytes"],finish["instructions"]) == ("GAMEONLY","0x8002DDCC","0x8002DE33",104,26), "Presentation finish provenance drifted")
    require(finish["completed"] and finish["same_parent_memory"] and finish["classification"] == "no direct visual effect"
            and (finish["call_pc"],finish["flag_before"],finish["flag_after"],finish["active_after"],finish["published_word"],finish["gate"],finish["operations"],finish["reads"],finish["stores"],finish["callbacks"],finish["return_address"],finish["sp"],finish["returned_value"]) == (0x80067424,255,0,0,0x80170000,0,10,3,5,2,0x8006742C,0x801FFED0,0x8008048C)
            and finish["hilo_known_masks"] == [0,0] and finish["call_pcs"] == [0x8002DDF8,0x8002DE14], "Presentation finish state drifted")
    (args.frames / "period_presentation_finish_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8002DDCC","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":finish,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    card = finish["pregame_match_card"]
    require((card["program"],card["address"],card["inclusive_end"],card["bytes"],card["instructions"]) == ("GAMEONLY","0x80044550","0x80044997",1096,274), "Pregame card provenance drifted")
    require(card["completed"] and card["same_parent_memory"] and card["classification"] == "BLOCKED"
            and (card["call_pc"],card["polls"],card["layouts"],card["texts"],card["clock_reads"],card["stream_pumps"],card["readiness_checks"],card["clock"],card["input_fixture"],card["font_mode_after"],card["skip_after"],card["return_address"],card["sp"]) == (0x8002DDF8,1,7,8,2,1,1,100,0x180,0,0,0x8002DE00,0x801FFEB8)
            and card["hilo_known_masks"] == [0,0]
            and (card["operations"],card["reads"],card["stores"],card["callbacks"]) == (108,23,52,33)
            and card["call_pcs"] == [0x80044568,0x80044570,0x800445A8,0x800445D4,0x80044600,0x80044624,0x80044648,0x8004466C,0x80044694,0x800446BC,0x800446DC,0x800446F8,0x80044714,0x8004472C,0x80044754,0x8004476C,0x80044788,0x800447A0,0x800447C8,0x800447E0,0x80044834,0x8004484C,0x80044874,0x80044884,0x80044898,0x800448A4,0x800448AC,0x800448B8,0x80044904,0x80044920,0x80044954,0x8004495C,0x8004496C], "Pregame card composed CPU prefix drifted")
    (args.frames / "pregame_match_card_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80044550","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":card,
        "classification":"BLOCKED","visible_screen":"User Setup; pregame renderer unresolved"
    },indent=2)+"\n",encoding="utf-8")
    selection = finish["pregame_selection_screen"]
    require((selection["program"],selection["address"],selection["inclusive_end"],selection["bytes"],selection["instructions"]) == ("GAMEONLY","0x80046C2C","0x80046F67",828,207), "Pregame selection provenance drifted")
    require(selection["completed"] and selection["same_parent_memory"] and selection["classification"] == "BLOCKED"
            and (selection["call_pc"],selection["polls"],selection["redraws"],selection["clock_reads"],selection["stream_pumps"],selection["menu_calls"],selection["clock"],selection["controller_after"],selection["skip_after"],selection["return_address"],selection["sp"],selection["returned_value"]) == (0x8002DE14,3,2,2,3,1,100,0x1234,0,0x8002DE1C,0x801FFEB8,0x8008048C)
            and selection["hilo_known_masks"] == [0,0] and selection["input_fixture"] == [4,32,128] and selection["selection_pairs"] == [0,12,1,13]
            and (selection["operations"],selection["reads"],selection["stores"],selection["callbacks"]) == (60,22,12,26)
            and selection["call_pcs"] == [0x80046C54,0x80046C70,0x80046C7C,0x80046CA8,0x80046CB0,0x80046CB8,0x80046CCC,0x80046CD8,0x80046E0C,0x80046C7C,0x80046CA8,0x80046CB0,0x80046CB8,0x80046CCC,0x80046CD8,0x80046D68,0x80046D84,0x80046ED8,0x80046CCC,0x80046CD8,0x80046ED8,0x80046F08,0x80046F14,0x80046F1C,0x80046F24,0x80046F2C], "Pregame selection composed CPU state drifted")
    (args.frames / "pregame_selection_screen_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80046C2C","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":selection,
        "classification":"BLOCKED","visible_screen":"User Setup; pregame renderer unresolved"
    },indent=2)+"\n",encoding="utf-8")
    announcements = [case["first_period_startup"]["announcement"] for case in first_cases]
    for announcement, mode in zip(announcements, (2,2)):
        require((announcement["program"],announcement["address"],announcement["inclusive_end"],announcement["bytes"],announcement["instructions"]) ==
                ("GAMEONLY","0x8007EF4C","0x8007F073",296,74), "announcement provenance drifted")
        expected_calls = ([0x8007EF5C,0x8007EF70,0x8007EF8C,0x8007EF98,0x8007EFA4,0x8007EFAC,
            0x8007EFBC,0x8007EFD0,0x8007EFDC,0x8007EFE8,0x8007EFFC,0x8007F050] if mode==2 else
            [0x8007EF5C,0x8007EF70,0x8007F02C,0x8007F038,0x8007F048,0x8007F050])
        require(announcement["completed"] and announcement["classification"] == "no direct visual effect"
                and "synthetic speech service" in announcement["scope"] and announcement["call_pc"] == 0x80067450
                and announcement["mode"] == mode and announcement["call_pcs"] == expected_calls
                and (announcement["operations"],announcement["reads"],announcement["stores"]) == ((23,7,4) if mode==2 else (16,6,4))
                and announcement["announcement_args"] == ([0x80180100,0x80180200,0x20,0x80190000] if mode==2 else [0x80180000,0x80180100,5])
                and announcement["frame_stack_pointer"] == 0x801FFEB0 and announcement["restored_ra"] == 0x80067458,
                "announcement native CPU fixture drifted")
    (args.frames / "tipoff_announcement_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8007EF4C","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":announcements,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    limit_cases = [period]+first_cases
    for case, signed_period in zip(limit_cases, (0xFFFF8000,0,0)):
        limits = case["late_period_limits"]
        require((limits["program"],limits["address"],limits["inclusive_end"],limits["bytes"],limits["instructions"]) ==
                ("GAMEONLY","0x80067550","0x800675E3",148,37), "late-period limits provenance drifted")
        require(limits["completed"] and limits["classification"] == "no direct visual effect"
                and "independent synthetic full-GPR" in limits["scope"] and limits["call_pc"] == 0x80068CEC
                and (limits["operations"],limits["reads"],limits["stores"]) == (3,2,1)
                and (limits["clock"],limits["period"],limits["limit_before"],limits["limit_after"],limits["returned_ra"]) ==
                    (0,signed_period,0xBEEF,0,0x80068CF4), "late-period limits native CPU fixture drifted")
    (args.frames / "late_period_limits_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80067550","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json",
        "state":[case["late_period_limits"] for case in limit_cases],
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    reset_cases = [case["controller_frame_reset"] for case in limit_cases]
    for reset in reset_cases:
        require((reset["program"],reset["address"],reset["inclusive_end"],reset["bytes"],reset["instructions"]) ==
                ("GAMEONLY","0x800675E4","0x80067663",128,32), "controller reset provenance drifted")
        require(reset["completed"] and reset["classification"] == "no direct visual effect"
                and "explicit root" in reset["scope"] and reset["call_pc"] == 0x80068CF4
                and (reset["operations"],reset["reads"],reset["stores"],reset["calls"]) == (23,11,11,1)
                and reset["child_pc"] == 0x8006764C
                and (reset["timer_before"],reset["delta"],reset["timer_after"],reset["cleared_slots"]) == (1,2,0,8)
                and reset["controller_fields"] == [0]*8
                and reset["frame_stack_pointer"] == 0x801FFEE0 and reset["restored_ra"] == 0x80068CFC,
                "controller reset native CPU fixture drifted")
    (args.frames / "controller_frame_reset_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800675E4","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":reset_cases,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    clock_cases = [case["match_clocks"] for case in limit_cases]
    for clocks, phase in zip(clock_cases,(0,0x81,0x82)):
        require((clocks["program"],clocks["address"],clocks["inclusive_end"],clocks["bytes"],clocks["instructions"]) ==
                ("GAMEONLY","0x80067A60","0x80067D37",728,182), "match clocks provenance drifted")
        paused=phase==0x81
        require(clocks["completed"] and clocks["classification"] == "no direct visual effect"
                and "independent synthetic machine" in clocks["scope"] and clocks["call_pc"] == 0x80068D58
                and clocks["phase"] == phase and clocks["delta"] == 22
                and (clocks["operations"],clocks["reads"],clocks["stores"]) == ((10,6,4) if paused else ((25,13,10) if phase==0 else (25,15,9)))
                and clocks["call_pcs"] == ([] if paused else ([0x80067B94,0x80067CA8] if phase==0 else [0x80067B94]))
                and clocks["call_args"] == ([] if paused else ([2,11] if phase==0 else [2]))
                and (clocks["main_before"],clocks["main_after"],clocks["shot_before"],clocks["shot_after"]) ==
                    (7200,7200 if paused else 7178,180,158 if phase==0 else 180)
                and clocks["team_timers"] == ([1,0] if paused else [0xFFEB,0])
                and clocks["team_states"] == ([0xAAAA,0xBBBB] if paused else [0xAAAA,2])
                and clocks["signal"] == (0xBEEF if paused else 0) and clocks["multiply_count"] == (0 if paused else (4 if phase==0 else 2))
                and clocks["frame_stack_pointer"] == 0x801FFED0 and clocks["restored_ra"] == 0x80068D60,
                "match clocks native CPU fixture drifted")
    (args.frames / "match_clocks_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80067A60","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":clock_cases,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    violation_cases = [case["clock_violations"] for case in limit_cases]
    for rule,phase in zip(violation_cases,(0,0x81,0x82)):
        expected = {0:((20,11,5),[0x80067FC0,0x80067FDC,0x80067FE4,0x80067FEC],[11,5000,12,0],[1,0],[0,0,1]),
                    0x81:((8,6,2),[],[],[1,1],[0,0,0]),
                    0x82:((37,20,9),[0x80067EE4,0x80067EF0,0x80067EF8,0x80067F00,0x80067FD0,0x80067FDC,0x80067FE4,0x80067FEC],[12,20000,11,0,12,20000,12,0],[0,0],[0,1,1])}[phase]
        require((rule["program"],rule["address"],rule["inclusive_end"],rule["bytes"],rule["instructions"]) ==
                ("GAMEONLY","0x80067D38","0x8006801B",740,185), "clock violations provenance drifted")
        require(rule["completed"] and rule["classification"] == "no direct visual effect"
                and "explicit initial machine" in rule["scope"] and rule["call_pc"] == 0x80068D64
                and (rule["phase_before"],rule["phase_after"],rule["delta"]) == (phase,0x81 if phase==0x81 else 0,22)
                and (rule["operations"],rule["reads"],rule["stores"]) == expected[0]
                and rule["call_pcs"] == expected[1] and rule["call_args"] == expected[2]
                and rule["timer_before"] == [1,1] and rule["timer_after"] == expected[3] and rule["triggers"] == expected[4]
                and rule["violation_state"] == (0 if phase==0x81 else 4)
                and rule["frame_stack_pointer"] == 0x801FFEE8 and rule["restored_ra"] == 0x80068D6C,
                "clock violations native CPU fixture drifted")
    (args.frames / "clock_violations_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80067D38","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":violation_cases,
        "classification":"no direct visual effect"
    },indent=2)+chr(10),encoding="utf-8")
    delay_cases=[case["rule_delays"] for case in violation_cases]
    for leaves,pcs,duration in zip(delay_cases,([0x80067FDC],[],[0x80067EF0,0x80067FDC]),(5000,0,20000)):
        require([leaf["call_pc"] for leaf in leaves]==pcs,"rule delay native call coverage drifted")
        for leaf in leaves:
            require((leaf["program"],leaf["address"],leaf["inclusive_end"],leaf["bytes"],leaf["instructions"]) ==
                    ("GAMEONLY","0x800295C8","0x800295CF",8,2),"rule delay provenance drifted")
            require(leaf["completed"] and leaf["classification"]=="no direct visual effect"
                    and "actual clock-violation event" in leaf["scope"] and leaf["machine_unchanged"]
                    and (leaf["operations"],leaf["reads"],leaf["stores"],leaf["ignored_duration"])==(0,0,0,duration)
                    and leaf["returned_sp"]==0x801FFEE8 and leaf["returned_ra"]==leaf["call_pc"]+8,
                    "rule delay native CPU fixture drifted")
    (args.frames / "rule_delay_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800295C8","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":delay_cases,
        "classification":"no direct visual effect"
    },indent=2)+chr(10),encoding="utf-8")
    expiry_cases = [case["period_expiry"] for case in limit_cases]
    for expiry in expiry_cases:
        require((expiry["program"],expiry["address"],expiry["inclusive_end"],expiry["bytes"],expiry["instructions"]) ==
                ("GAMEONLY","0x80067664","0x800677D7",372,93), "period expiry provenance drifted")
        require(expiry["completed"] and expiry["classification"] == "no direct visual effect"
                and "actual violation owner output" in expiry["scope"] and expiry["call_pc"] == 0x80068D6C
                and (expiry["operations"],expiry["reads"],expiry["stores"],expiry["child_calls"]) == (7,4,3,0)
                and expiry["returned_value"] == 0 and expiry["frame_stack_pointer"] == 0x801FFEE0
                and expiry["restored_ra"] == 0x80068D74, "period expiry native CPU fixture drifted")
    (args.frames / "period_expiry_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80067664","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":expiry_cases,
        "classification":"no direct visual effect"
    },indent=2)+chr(10),encoding="utf-8")
    publication_cases = [case["service_publication"] for case in limit_cases]
    for publication, phase in zip(publication_cases,(0,0x81,0)):
        require((publication["program"],publication["address"],publication["inclusive_end"],publication["bytes"],publication["instructions"]) ==
                ("GAMEONLY","0x8002DE34","0x8002DE73",64,16), "service publication provenance drifted")
        require(publication["completed"] and publication["classification"] == "no direct visual effect"
                and "actual period-expiry output" in publication["scope"] and publication["call_pc"] == 0x80068D7C
                and (publication["operations"],publication["reads"],publication["stores"],publication["child_calls"]) == (7,3,3,1)
                and publication["child_pc"] == 0x8002DE5C
                and (publication["status_before"],publication["status_after"],publication["phase_before"],publication["phase_after"]) ==
                    (0xBEEF,0xFFFF,0xDEADBEEF,phase)
                and (publication["child_v0"],publication["child_v1"]) ==
                    (publication["match_audio_service"]["returned_v0"],publication["match_audio_service"]["returned_v1"])
                and publication["frame_stack_pointer"] == 0x801FFEE8 and publication["restored_ra"] == 0x80068D84,
                "service publication native CPU fixture drifted")
    (args.frames / "match_service_publish_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8002DE34","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":publication_cases,
        "classification":"no direct visual effect"
    },indent=2)+chr(10),encoding="utf-8")
    audio_cases=[case["match_audio_service"] for case in publication_cases]
    expected_audio=(
        (1,1,98,15,8,6,1,98<<16,0x800FDA0E,[]),
        (2,0,1,15,6,5,4,0x8002A444^0x13572468,1000,[0x8002A424,0x8002A43C,0x8002A444]),
        (3,0,0xFFEB,20,9,6,5,0x82,0,[0x8002A2FC,0x8002A30C]),
    )
    for service,expected,phase in zip(audio_cases,expected_audio,(0,0x81,0)):
        mode,state,timer,ops,reads,stores,calls,v0,v1,pcs=expected
        require((service["program"],service["address"],service["inclusive_end"],service["bytes"],service["instructions"]) ==
                ("GAMEONLY","0x8002A264","0x8002A463",512,128),"match audio provenance drifted")
        require(service["completed"] and service["classification"]=="no direct visual effect"
                and "explicit mode" in service["scope"] and service["call_pc"]==0x8002DE5C
                and (service["mode_before"],service["mode_after"],service["timer_before"],service["timer_after"],service["phase"]) ==
                    (mode,state,480 if mode==1 else 1,timer,phase)
                and (service["operations"],service["reads"],service["stores"],service["child_calls"]) == (ops,reads,stores,calls)
                and (service["clock_before"],service["clock_after"],service["delta"]) == (1000,1022,22)
                and (service["returned_v0"],service["returned_v1"]) == (v0,v1)
                and service["unresolved_call_pcs"]==pcs and service["frame_stack_pointer"]==0x801FFEC8
                and service["restored_ra"]==0x8002DE64 and service["status_calls"]==(1 if mode==3 else 0)
                and service["status_value"]==(3 if mode==3 else 0),"match audio native CPU fixture drifted")
        leaf=service["clock_read"]
        require(leaf["completed"] and leaf["address"]=="0x800A5810" and leaf["call_pc"]==0x8002A270
                and (leaf["reads"],leaf["value"],leaf["returned_sp"],leaf["returned_ra"])==(1,1022,0x801FFEC8,0x8002A278),
                "match audio clock composition drifted")
    (args.frames / "match_audio_service_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8002A264","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":audio_cases,
        "classification":"no direct visual effect"
    },indent=2)+chr(10),encoding="utf-8")
    require(audio_cases[0]["stream_readiness"] is None and audio_cases[1]["stream_readiness"] is None,
            "stream readiness executed outside mode3")
    readiness=audio_cases[2]["stream_readiness"]
    queue=readiness["queue_count"]
    require((queue["program"],queue["address"],queue["inclusive_end"],queue["bytes"],queue["instructions"]) ==
            ("GAMEONLY","0x80084448","0x80084587",320,80),"stream queue count provenance drifted")
    require(queue["completed"] and queue["classification"]=="no direct visual effect"
            and "two synthetic nodes" in queue["scope"] and queue["call_pc"]==0x80088D30
            and (queue["operations"],queue["reads"],queue["stores"])==(30,20,8)
            and (queue["head"],queue["links"],queue["iterations"],queue["returned_value"])==(0x80173000,1,2,1)
            and (queue["counter_before"],queue["counter_incremented"],queue["counter_after"])==(0xFFFFFFFF,0,0xFFFFFFFF)
            and queue["call_pcs"]==[0x8008447C,0x8008455C]
            and (queue["frame_stack_pointer"],queue["returned_sp"],queue["restored_ra"])==(0x801FFE90,0x801FFEB0,0x80088D38),
            "stream queue count native CPU fixture drifted")
    (args.frames / "stream_queue_count_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80084448","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":queue,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    require((readiness["program"],readiness["address"],readiness["inclusive_end"],readiness["span_bytes"],readiness["span_words"],readiness["body_bytes"],readiness["instructions"]) ==
            ("GAMEONLY","0x80088D0C","0x80088D7B",112,28,104,26),"stream readiness provenance drifted")
    require(readiness["completed"] and readiness["classification"]=="no direct visual effect"
            and "explicit enabled flag" in readiness["scope"] and readiness["call_pc"]==0x8002A2EC
            and (readiness["operations"],readiness["reads"],readiness["stores"],readiness["flag"],readiness["child_calls"])==(6,3,2,1,1)
            and (readiness["child_pc"],readiness["child_value"],readiness["returned_value"])==(0x80088D30,1,1)
            and (readiness["frame_stack_pointer"],readiness["returned_sp"],readiness["restored_ra"])==(0x801FFEB0,0x801FFEC8,0x8002A2F4),
            "stream readiness native CPU fixture drifted")
    (args.frames / "stream_readiness_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80088D0C","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":readiness,
        "classification":"no direct visual effect"
    },indent=2)+chr(10),encoding="utf-8")
    playback = scene["random_warmup"]["speech_startup"]
    clock_reads=playback["clock_reads"]
    require(len(clock_reads)==3,"clock read native coverage missing")
    for leaf,pc,sample in zip(clock_reads,(0x800801EC,0x80080208,0x80080208),(1000,1240,1241)):
        require((leaf["program"],leaf["address"],leaf["inclusive_end"],leaf["bytes"],leaf["instructions"]) ==
                ("GAMEONLY","0x800A5810","0x800A581F",16,4),"clock read provenance drifted")
        require(leaf["completed"] and leaf["classification"]=="no direct visual effect"
                and "explicit retained counter fixture" in leaf["scope"] and leaf["call_pc"]==pc
                and (leaf["operations"],leaf["reads"],leaf["counter_address"],leaf["returned_value"])==(1,1,0x800D7A70,sample)
                and leaf["returned_sp"]==playback["frame_stack_pointer"] and leaf["returned_ra"]==pc+8,
                "clock read native CPU fixture drifted")
    (args.frames / "clock_read_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800A5810","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":clock_reads,
        "classification":"no direct visual effect"
    },indent=2)+chr(10),encoding="utf-8")
    pumps = playback["audio_stream_pumps"] + [case["audio_stream_pump"] for case in reset_cases]
    require(len(pumps)==5, "stream pump native parent coverage missing")
    stream_services = [service for pump in pumps for service in pump["stream_services"]]
    require(len(stream_services)==10,"stream service native coverage missing")
    for pump in pumps:
        for i,service in enumerate(pump["stream_services"]):
            pc=0x80083F78 if pump["mode"]==5 else 0x80084034
            require((service["program"],service["address"],service["inclusive_end"],service["bytes"],service["instructions"]) ==
                    ("GAMEONLY","0x80086190","0x800861E3",84,21),"stream service provenance drifted")
            require(service["completed"] and service["classification"]=="no direct visual effect"
                    and "explicit synthetic header" in service["scope"] and service["call_pc"]==pc
                    and (service["header"],service["header_state"])==(0x80171000,i)
                    and (service["operations"],service["reads"],service["stores"])==((6,4,2) if i else (7,4,2))
                    and service["child_calls"]==(0 if i else 1) and service["child_pc"]==(0 if i else 0x800861C4)
                    and service["returned_value"]==(1 if i else 0x13572468)
                    and service["frame_stack_pointer"]==pump["frame_stack_pointer"]-0x18 and service["restored_ra"]==pc+8,
                    "stream service native CPU fixture drifted")
    (args.frames / "audio_stream_service_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80086190","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":stream_services,
        "classification":"no direct visual effect"
    },indent=2)+chr(10),encoding="utf-8")
    for pump, caller, mode in zip(pumps,(0x800801E4,0x8008021C,0x8006764C,0x8006764C,0x8006764C),(5,6,5,5,5)):
        require((pump["program"],pump["address"],pump["inclusive_end"],pump["bytes"],pump["instructions"]) ==
                ("GAMEONLY","0x80083EEC","0x800840EF",516,129), "stream pump provenance drifted")
        expected_calls = ([0x80083F00,0x80083F78,0x80083F88,0x80083FC4,0x80083F78,0x80083F88] if mode==5 else
                          [0x80083F00,0x80084034,0x80084044,0x80084034,0x80084044])
        require(pump["completed"] and pump["classification"] == "no direct visual effect"
                and "explicit synthetic stream services" in pump["scope"] and pump["call_pc"] == caller and pump["mode"] == mode
                and (pump["operations"],pump["reads"],pump["stores"]) == ((26,13,7) if mode==5 else (23,13,5))
                and pump["call_pcs"] == expected_calls and pump["status_queries"] == 2
                and pump["handler_calls"] == (1 if mode==5 else 0) and pump["handler_value"] == (0x12345678 if mode==5 else 0)
                and pump["returned_value"] == 0 and pump["restored_ra"] == caller+8
                and pump["frame_stack_pointer"] == (0x801FFEC0 if caller==0x8006764C else 0x807FFF38),
                "stream pump native CPU fixture drifted")
    (args.frames / "audio_stream_pump_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x80083EEC","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":pumps,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    gates=[pump["stream_status"] for pump in pumps]
    for gate,flags,busy in zip(gates,(7,6,7,7,7),(255,0,0,0,0)):
        require((gate["program"],gate["address"],gate["inclusive_end"],gate["body_bytes"],gate["body_instructions"],gate["span_bytes"],gate["span_instructions"]) ==
                ("GAMEONLY","0x8008472C","0x8008480F",196,49,228,57), "stream status body/span provenance drifted")
        operations=4 if busy else (6 if flags==7 else 5)
        require(gate["completed"] and gate["classification"] == "no direct visual effect"
                and "actual stream-pump event" in gate["scope"] and gate["call_pc"] == 0x80083F00
                and gate["flags"] == flags and gate["busy"] == busy
                and (gate["operations"],gate["reads"],gate["stores"]) == (operations,operations-1,1)
                and gate["returned_value"] == (4 if busy else (3 if flags==7 else 1)) and gate["returned_ra"] == 0x80083F08,
                "stream status native CPU fixture drifted")
    require([gate["frame_stack_pointer"] for gate in gates] == [0x807FFF30]*2+[0x801FFEB8]*3, "stream status nested stack drifted")
    (args.frames / "audio_stream_status_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8008472C","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":loop_hashes,"cpu_receipt":"loop_entry_trace.json","state":gates,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    require((playback["program"],playback["address"],playback["inclusive_end"],playback["bytes"],playback["instructions"]) ==
            ("GAMEONLY","0x800800F8","0x80080247",336,84), "speech startup provenance drifted")
    require(playback["completed"] and playback["classification"] == "no direct visual effect"
            and "explicit synthetic audio/time services" in playback["scope"] and playback["call_pc"] == 0x800802B4
            and (playback["operations"],playback["reads"],playback["stores"],playback["calls"]) == (26,4,7,15)
            and playback["call_pcs"] == [0x80080114,0x80080124,0x8008018C,0x8008019C,0x800801BC,0x800801C8,
                0x800801DC,0x800801E4,0x800801EC,0x800801F8,0x80080208,0x8008021C,0x800801F8,0x80080208,0x8008022C]
            and (playback["language"],playback["filename"],playback["handle"],playback["voice"],playback["fifth_argument"]) ==
                (1,0x80027BB0,0x80170000,0x80170100,1)
            and playback["clock_samples"] == [1000,1240,1241] and playback["deadline"] == 1240
            and (playback["ready_polls"],playback["service_pumps"]) == (2,2) and playback["cleared_globals"] == [0,0]
            and playback["frame_stack_pointer"] == 0x807FFF58 and playback["restored_ra"] == 0x800802BC,
            "speech startup native CPU fixture drifted")
    (args.frames / "speech_startup_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x800800F8","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":scene_hashes,"cpu_receipt":"scene_load_trace.json","state":playback,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    speech = initialize["speech_initialize"]
    require((speech["program"], speech["address"], speech["inclusive_end"], speech["bytes"], speech["instructions"]) ==
            ("GAMEONLY", "0x8007FD40", "0x800800F7", 952, 238), "speech initializer provenance drifted")
    speech_destinations = [0x80103220+i*12 for i in range(4)]
    for i in range(12):
        speech_destinations.extend(base+i*12 for base in (0x80102FE0,0x80103070,0x80103100,0x80103190))
    speech_destinations.extend(0x80103250+i*12 for i in range(48))
    speech_packed, speech_size = [], 0
    for i in range(0,100,3):
        speech_packed.append(0x80160000+speech_size)
        speech_size += (i%4+1)*4
    require(speech["completed"] and "recovered retry loaders" in speech["scope"]
            and speech["classification"] == "no direct visual effect" and speech["call_pc"] == 0x8002DBD8
            and speech["languages"] == [1,1] and speech["aux_pointers"] == [0x80137B28,0x80137B34]
            and (speech["lookups"], speech["copies"], speech["conversions"], speech["sentinels"]) == (100,34,34,10)
            and speech["lookup_destinations"] == speech_destinations and speech["packed_pointers"] == speech_packed
            and (speech["allocation_size"],speech["allocation_pointer"],speech["released_pointer"]) == (speech_size,0x80160000,0x80137B78)
            and speech["restored_ra"] == 0x8002DBE0
            and speech["loaders"] == [{"operations":8,"attempts":2,"null_results":1}]*3,
            "speech native CPU fixture drifted")
    (args.frames / "speech_initialize_verified.json").write_text(json.dumps({
        "program":"GAMEONLY","address":"0x8007FD40","driver_frame_count":len(states),
        "input_transition_frames":{name:states.index(by_id[name]) for name in required},
        "frame_sha256":initialize_hashes,"cpu_receipt":"match_initialize_trace.json","state":speech,
        "classification":"no direct visual effect"
    },indent=2)+"\n",encoding="utf-8")
    roster = initialize["roster_bindings"]
    require((roster["program"], roster["address"], roster["inclusive_end"],
             roster["bytes"], roster["instructions"], roster["call_pc"]) ==
            ("GAMEONLY", "0x80063D58", "0x80063EDB", 388, 97, "0x8002DBC8"),
            "roster bindings source identity drifted")
    require(roster["completed"] and roster["classification"] == "no direct visual effect"
            and roster["counts"] == [3, 12] and roster["published_table"] == 0x80015034
            and (roster["operations"], roster["reads"], roster["stores"]) == (159, 50, 109)
            and roster["home"] == [0x8002208C + (i if i < 3 else 0)*0x6E for i in range(12)]
            and roster["away"] == [0x800225B4 + i*0x6E for i in range(12)]
            and roster["home"] == roster["mirror_home"]
            and roster["away"] == roster["mirror_away"],
            "recovered roster owner did not publish exact mapped roster bindings")
    (args.frames / "roster_bindings_verified.json").write_text(json.dumps({
        "program": "GAMEONLY", "address": "0x80063D58",
        "driver_frame_count": len(states),
        "input_transition_frames": {name: states.index(by_id[name]) for name in required},
        "frame_sha256": initialize_hashes, "cpu_receipt": "match_initialize_trace.json",
        "state": roster, "classification": "no direct visual effect"
    }, indent=2) + "\n", encoding="utf-8")
    require("not a live loader" in receipt["scope"] and "gameplay frame" in receipt["scope"],
            "diagnostic receipt lost its non-gameplay scope boundary")
    require(receipt["driver"] == {"kind": "native recovered-input handlers",
                                  "screens": ["Game Setup", "Team Select", "User Setup"],
                                  "frame_format": "P6 PPM"},
            "visual capture is not attributed to the native input test driver")
    require(receipt["source"] == {"binary": "GAMEONLY", "address": "0x80029994",
                                  "end_exclusive": "0x80029BCC", "instructions": 142},
            "translated source identity drifted")
    require(receipt["static_initializers"] == {
                "binary": "GAMEONLY", "address": "0x800948D0",
                "end_exclusive": "0x80094940", "instructions": 28,
                "call_pc": "0x800299A4", "guard_address": "0x800C4B14",
                "guard_before": 0, "guard_after": 1, "constructor_count": 0,
                "constructor_callbacks": 0, "operations": 8,
                "status": "initialized"},
            "recovered 0x800948D0 execution receipt drifted")
    require(receipt["global_pointer_save"] == {
                "binary": "GAMEONLY", "address": "0x800A4830",
                "end_exclusive": "0x800A4844", "instructions": 5,
                "call_pc": "0x800299AC", "destination": "0x800D6E2C",
                "value": "0x800D79C8", "operations": 1, "status": "saved"},
            "recovered 0x800A4830 execution receipt drifted")
    require(receipt["heap_initialize"] == {
                "binary": "GAMEONLY", "address": "0x8008FA6C",
                "end_exclusive": "0x8008FB4C", "instructions": 56,
                "call_pc": "0x800299C8", "closure_pcs": 169,
                "descriptor_count": 220, "arena": "0x8010B61C",
                "arena_size": 991716, "payload_begin": "0x8010D87C",
                "heap_bank": "0x80103D50", "accesses": 258,
                "events": 250, "stores": 248, "formatter_callbacks": 2,
                "low_name": "LOW MB_RAM  ", "high_name": "HIGH MB_RAM ",
                "status": "initialized"},
            "recovered 0x8008FA6C heap execution receipt drifted")
    require(receipt["cd_directory_initialize"] == {
                "binary": "GAMEONLY", "address": "0x80091C08",
                "end_exclusive": "0x80091DE0", "instructions": 118,
                "call_pc": "0x800299D8", "buffer": "0x80103550",
                "child_calls": 10, "accesses": 32, "reads": 17,
                "stores": 15, "polls": 0, "disc_base_sector": 256,
                "primary_volume_sector": 272, "descriptor_delta": 16,
                "root_directory_lba": 23, "root_directory_size": 2048,
                "cache_flag": "0x800C4ABC", "status": "initialized"},
            "recovered 0x80091C08 CD-directory execution receipt drifted")
    require(receipt["path_prefix_set"] == {
                "binary": "GAMEONLY", "address": "0x800A35D8",
                "end_exclusive": "0x800A364C", "instructions": 29,
                "call_pc": "0x800299E8", "source": "0x800247E4",
                "destination": "0x800D6DAC", "path": "cdrom:",
                "child_calls": 2, "accesses": 5, "reads": 3,
                "stores": 2, "copied_length": 6, "final_length": 6,
                "separator_appended": False, "status": "selected"},
            "recovered 0x800A35D8 path-prefix execution receipt drifted")
    require(receipt["directory_cache_configure"] == {
                "binary": "GAMEONLY", "address": "0x80092C7C",
                "end_exclusive": "0x80092CBC", "instructions": 16,
                "call_pc": "0x800299F8", "cache": "0x8001000C",
                "capacity": 707, "record_size": 20, "reserved_bytes": 14140,
                "capacity_global": "0x800C4AB8",
                "pointer_global": "0x801046A0", "accesses": 8,
                "reads": 3, "stores": 5, "child_calls": 0,
                "status": "configured"},
            "recovered 0x80092C7C directory-cache configuration receipt drifted")
    require(receipt["interrupt_mask_set"] == {
                "binary": "GAMEONLY", "address": "0x800985B4",
                "end_exclusive": "0x800985CC", "instructions": 6,
                "call_pc": "0x80029A08", "api": "SetIntrMask",
                "mask_global": "0x800C54AC", "requested_mask": 0,
                "previous_mask": 2047, "published_mask": 0,
                "accesses": 2, "reads": 1, "stores": 1,
                "child_calls": 0, "status": "cleared-before-callback-reset"},
            "recovered 0x800985B4 interrupt-mask receipt drifted")
    require(receipt["reset_callback"] == {
                "binary": "GAMEONLY", "address": "0x800985DC",
                "end_exclusive": "0x8009860C", "instructions": 12,
                "call_pc": "0x80029A10", "api": "ResetCallback",
                "dispatch_pointer_global": "0x800C54C8",
                "dispatch_table": "0x800C54B0", "dispatch_slot_offset": 12,
                "dispatch_target": "0x80098714",
                "frame_stack_pointer": "0x807FFFB8",
                "restored_return_address": "0x80029A18",
                "accesses": 4, "reads": 3, "stores": 1,
                "child_calls": 1, "child_return": 1,
                "child_status": "synthetic-required-boundary",
                "visual_effect": "none", "status": "dispatched"},
            "recovered 0x800985DC ResetCallback dispatch receipt drifted")
    require(receipt["controller_resume"] == {
                "binary": "GAMEONLY", "address": "0x8008F1D4",
                "end_exclusive": "0x8008F224", "instructions": 20,
                "call_pcs": ["0x80029A18", "0x80029A30"],
                "requested_mode": 8, "pad_mode_global": "0x800D7A48",
                "final_pad_mode": 8, "suspend_flag_global": "0x800C4A70",
                "initial_suspend_flag": 1, "final_suspend_flag": 0,
                "clock_snapshot_global": "0x800C4A74", "clock_snapshot": 37,
                "initializer_entry": "0x80091184", "clock_entry": "0x800A5810",
                "first_call_operations": 8, "first_call_accesses": 6,
                "first_call_child_calls": 2,
                "first_call_status": "input-reinitialized",
                "second_call_operations": 4, "second_call_accesses": 4,
                "second_call_child_calls": 0,
                "second_call_status": "mode-reasserted-input-already-active",
                "visual_effect": "none", "status": "resumed"},
            "recovered 0x8008F1D4 controller-resume receipt drifted")
    require(receipt["reset_graph"] == {
                "binary": "GAMEONLY", "address": "0x80099058",
                "end_exclusive": "0x800991B0", "instructions": 86,
                "call_pc": "0x80029A20", "api": "ResetGraph",
                "requested_mode": 3, "masked_mode": 3,
                "driver_table_global": "0x800C55B8",
                "driver_table": "0x800C5578",
                "state_global": "0x800C55C0", "reset_type": 0,
                "display_width": 1024, "display_height": 512,
                "memory_set_calls": 3, "reset_callback_calls": 1,
                "bios_a0_49_calls": 1, "device_reset_calls": 1,
                "child_calls": 7, "nested_reset_target": "0x80098714",
                "operations": 23, "accesses": 16, "reads": 9,
                "stores": 7, "source_quirks": {
                    "mode_mask": 7,
                    "reset_result_truncated_to_byte": True,
                    "unchecked_reset_type_index": True,
                    "unguarded_driver_dispatch": True},
                "visual_effect": "none",
                "status": "initialized-mapped-ps1-gpu-state"},
            "recovered 0x80099058 ResetGraph receipt drifted")
    require(receipt["graph_debug_set"] == {
                "binary": "GAMEONLY", "address": "0x800992C4",
                "end_exclusive": "0x80099330", "instructions": 27,
                "call_pc": "0x80029A28", "api": "SetGraphDebug",
                "level_global": "0x800C55C2",
                "callback_global": "0x800C55BC",
                "requested_level": 0, "previous_level": 0,
                "published_level": 0, "diagnostic_calls": 0,
                "return_value": 0, "operations": 6, "accesses": 6,
                "reads": 3, "stores": 3, "source_quirks": {
                    "argument_truncated_to_byte": True,
                    "zero_low_byte_skips_diagnostic": True,
                    "unguarded_diagnostic_dispatch": True,
                    "callback_return_ignored": True},
                "visual_effect": "none", "status": "debug-disabled"},
            "recovered 0x800992C4 SetGraphDebug receipt drifted")
    require(receipt["vblank_initialize"] == {
                "binary": "GAMEONLY", "address": "0x800A43E8",
                "end_exclusive": "0x800A44D4", "instructions": 59,
                "call_pc": "0x80029A38", "callback_table": "0x800D6E0C",
                "callback_slots": 8, "cleared_slots": 8,
                "interrupt_channel": 0, "interrupt_handler": "0x800A450C",
                "counter_spec": "0xF2000003", "counter_target": 1,
                "counter_mode": 4096, "set_rcnt_return": 0,
                "start_rcnt_return": 0,
                "frame_counter_globals": ["0x800D7A88", "0x800D7AFC",
                                          "0x800D7B00"],
                "child_calls": 8, "operations": 54, "accesses": 46,
                "reads": 27, "stores": 19, "source_quirks": {
                    "set_rcnt_rejects_index_3": True,
                    "start_rcnt_unmasks_before_false_return": True,
                    "raw_child_returns_ignored": True,
                    "prefix_writes_not_rolled_back": True},
                "visual_effect": "none",
                "status": "mapped-ps1-vblank-state-initialized"},
            "recovered 0x800A43E8 VBlank initialization receipt drifted")
    require(receipt["clock_initialize"] == {
                "binary": "GAMEONLY", "address": "0x800914D8",
                "end_exclusive": "0x8009167C", "instructions": 105,
                "call_pc": "0x80029A4C", "requested_rate": 120,
                "live_rate_divisor": 120, "clock_base": 4233600,
                "guard_address": "0x800C4AA4", "guard_before": 0,
                "guard_after": 1, "callback_table": "0x800D6DEC",
                "callback_slots": 8, "cleared_slots": 8,
                "interrupt_channel": 6, "interrupt_handler": "0x800916B4",
                "shutdown_handler": "0x8009167C",
                "counter_spec": "0xF2000002", "timer_target": 35280,
                "requested_counter_mode": 4096,
                "hardware_counter_mode": 600,
                "counter_interrupt_mask": 64, "effective_rate": 120,
                "set_rcnt_return": 1, "start_rcnt_return": 1,
                "reset_clock_globals": ["0x800D7A7C", "0x800D7A70",
                                          "0x800D7B2C", "0x800D7B28"],
                "child_calls": 7, "operations": 62, "accesses": 55,
                "reads": 31, "stores": 24, "source_quirks": {
                    "signed_double_division": True,
                    "quantized_effective_rate": True,
                    "divide_traps_prefix_commit": True,
                    "raw_child_returns_ignored": True,
                    "warm_path_skips_registration": True},
                "visual_effect": "none",
                "status": "mapped-ps1-clock-service-initialized"},
            "recovered 0x800914D8 clock initialization receipt drifted")
    require(receipt["gte_initialize"] == {
                "binary": "GAMEONLY", "address": "0x80056678",
                "end_exclusive": "0x800566E0", "instructions": 26,
                "call_pc": "0x80029A54",
                "cop0_status_before": "0x10900401",
                "cop0_status_after": "0x50900401",
                "cu2_mask": "0x40000000",
                "controls": {"OFX": 0, "OFY": 0, "H": 1000,
                             "DQA": -4194, "DQB": 20971520,
                             "ZSF3": 341, "ZSF4": 256},
                "controls_written": 7, "untouched_control_registers": 25,
                "operations": 9, "reads": 1, "stores": 8,
                "return_v0": "0x50900401", "source_quirks": {
                    "preserves_non_cu2_status_bits": True,
                    "leaves_other_gte_state_live": True,
                    "zsf3_zsf4_are_independent": True,
                    "return_is_updated_status": True},
                "visual_effect": "none",
                "status": "retained-gte-projection-controls-initialized"},
            "recovered 0x80056678 GTE initialization receipt drifted")
    require(receipt["clock_delta"] == {
                "binary": "GAMEONLY", "address": "0x800A584C",
                "end_exclusive": "0x800A5880", "instructions": 13,
                "call_pc": "0x80029A5C", "clock_leaf": "0x800A5810",
                "snapshot_address": "0x800D7B2C", "previous_snapshot": 0,
                "sampled_clock": 0, "delta": 0, "child_calls": 1,
                "operations": 7, "accesses": 6, "reads": 3, "stores": 3,
                "source_quirks": {"gp_relative_snapshot": True,
                                  "captures_old_before_child": True,
                                  "commits_sample_before_return": True,
                                  "raw_subu_wraparound": True},
                "visual_effect": "none", "status": "clock-baseline-refreshed"},
            "recovered 0x800A584C clock-delta receipt drifted")
    require(receipt["presentation_wait"] == {
                "binary": "GAMEONLY", "address": "0x80029BDC",
                "end_exclusive": "0x80029BFC", "instructions": 8,
                "call_pcs": ["0x80029A64", "0x80029B20", "0x80029B50"],
                "invocations": 41, "service_entry": "0x800A9CC0",
                "service_child_calls": 41, "fixture_path": "cold-one-vblank",
                "ready_global": "0x800D7A80",
                "frame_counter_global": "0x800D7A88",
                "vblank_signals": 41, "final_frame_counter": 52,
                "later_match_session_vblank_signals": 11,
                "operations_per_call": 3, "accesses_per_call": 2,
                "reads_per_call": 1, "stores_per_call": 1,
                "source_quirks": {"live_ra_reload": True,
                                  "child_v0_retained": True,
                                  "child_wait_has_no_timeout": True,
                                  "child_service_remains_explicit": True},
                "visual_effect": "none",
                "status": "41-source-vblank-boundaries-acknowledged"},
            "recovered 0x80029BDC presentation-wait receipt drifted")
    require(receipt["video_environment_initialize"] == {
                "binary": "GAMEONLY", "address": "0x80029F20",
                "end_exclusive": "0x8002A098", "instructions": 94,
                "call_pc": "0x80029A6C", "mode_argument": 0,
                "background_byte": 0,
                "display_environments": ["0x8002205C", "0x80022070"],
                "draw_environments": ["0x80021EEC", "0x80021F48"],
                "display_rects": [
                    {"x": 0, "y": 256, "w": 512, "h": 240},
                    {"x": 0, "y": 0, "w": 512, "h": 240}],
                "draw_rects": [
                    {"x": 0, "y": 0, "w": 512, "h": 240},
                    {"x": 0, "y": 256, "w": 512, "h": 240}],
                "set_def_calls": 4, "put_calls": 4, "draw_sync_calls": 1,
                "operations": 44, "accesses": 35, "reads": 7,
                "stores": 28, "direct_control_byte_stores": 16,
                "buffer_selector": "0x8001EDE8", "buffer_selector_value": 1,
                "last_active_pair": 1, "return_v0": 0,
                "source_quirks": {
                    "fifth_arguments_are_delay_slot_stack_stores": True,
                    "mode_is_low_byte_truncated": True,
                    "touches_two_setdef_untouched_drawenvs": True,
                    "rgb_cleared_only_in_initialized_drawenvs": True,
                    "pair1_active_while_selector_zero": True,
                    "live_register_epilogue": True},
                "visual_effect": "none",
                "status": "ps1-double-buffer-environments-initialized"},
            "recovered 0x80029F20 video-environment receipt drifted")
    require(receipt["move_image"] == {
                "binary": "GAMEONLY", "address": "0x800997E4",
                "end_exclusive": "0x800998A8", "instructions": 49,
                "api": "MoveImage",
                "call_pcs": ["0x80029A94", "0x80029AA4"],
                "invocations": 2,
                "rectangle": {"x": 512, "y": 0, "w": 512, "h": 256},
                "destinations": [{"x": 0, "y": 0}, {"x": 0, "y": 256}],
                "packet": "0x800C5668",
                "packet_words_after": ["0x04FFFFFF", "0x80000000",
                                       "0x00000200", "0x01000000",
                                       "0x01000200"],
                "driver_table_global": "0x800C55B8",
                "driver_table": "0x800C5578",
                "dispatch_context": "0x8009B1F8",
                "dispatch_entry": "0x8009B298",
                "diagnostic_calls": 2, "gpu_dispatches": 2,
                "operations_per_call": 20, "accesses_per_call": 18,
                "reads_per_call": 11, "stores_per_call": 7,
                "pixel_words_per_copy": 131072,
                "pixel_words_copied": 262144,
                "submitted_packets": 2,
                "completion_owner": "0x800994F4",
                "source_quirks": {
                    "diagnostic_precedes_extent_check": True,
                    "only_zero_extent_is_rejected": True,
                    "destination_coordinates_truncate_to_16_bits": True,
                    "packet_header_words_remain_live": True,
                    "unguarded_indirect_dispatch": True,
                    "live_register_epilogue": True},
                "visual_fixture": "generated diagnostic grid, not retail pixels",
                "captures": ["move-image-before-buffer0.ppm",
                             "move-image-source.ppm",
                             "move-image-buffer0.ppm",
                             "move-image-buffer1.ppm"],
                "visual_effect": "two diagnostic VRAM copies submitted; following DrawSync completed both; native frontend unchanged",
                "status": "both-vram-copy-packets-submitted"},
            "recovered 0x800997E4 MoveImage receipt drifted")
    require(receipt["gpu_sync"] == {
                "binary": "GAMEONLY", "address": "0x800994F4",
                "end_exclusive": "0x80099560", "instructions": 27,
                "api": "DrawSync", "call_pc": "0x80029AAC", "mode": 0,
                "driver_table_global": "0x800C55B8",
                "driver_table": "0x800C5578", "dispatch_offset": "0x3C",
                "dispatch_entry": "0x8009B9B4", "submitted_before": 2,
                "completed_before": 0, "completed_after": 2,
                "queued_through": 2, "dma_busy_samples": 1,
                "timer_reads": 4, "device_reads": 7,
                "backend_observations": 2, "source_steps": 4,
                "stack_reads": 2, "stack_writes": 2,
                "source_v0": 0, "synchronized": True,
                "source_quirks": {
                    "debug_callback_precedes_live_table_reload": True,
                    "indirect_dispatch_is_unguarded": True,
                    "signed_timeout_comparisons": True,
                    "timeout_poll_counter_postincrements": True,
                    "timeout_returns_minus_one_after_reset": True,
                    "live_o32_epilogue_reload": True},
                "visual_fixture": "generated diagnostic grid, not retail pixels",
                "captures": ["draw-sync-before-buffer0.ppm",
                             "draw-sync-after-buffer0.ppm"],
                "visual_effect": "pending MoveImage packets became visible in both retained VRAM buffers during DrawSync; native frontend unchanged",
                "status": "gpu-submissions-completed"},
            "recovered 0x800994F4 DrawSync receipt drifted")
    require(receipt["display_mask_set"] == {
                "binary": "GAMEONLY", "address": "0x80099458",
                "end_exclusive": "0x800994F4", "instructions": 39,
                "api": "SetDispMask", "call_pc": "0x80029AB4",
                "mask": 1, "debug_level": 0, "diagnostic_calls": 0,
                "environment_cache": "0x800C562C",
                "environment_cache_clear_calls": 0,
                "driver_table_global": "0x800C55B8",
                "driver_table": "0x800C5578", "dispatch_offset": "0x10",
                "dispatch_entry": "0x8009B16C",
                "gpu_control_word": "0x03000000",
                "display_enable_bit": 0, "display_enabled": True,
                "active_display_environment": "0x80022070",
                "return_v0": 3, "operations": 10, "accesses": 9,
                "reads": 6, "stores": 3, "child_calls": 1,
                "source_quirks": {
                    "full_word_zero_test": True,
                    "gp1_enable_bit_is_active_low": True,
                    "disable_clears_environment_cache_first": True,
                    "debug_callback_precedes_live_table_load": True,
                    "unguarded_indirect_dispatch": True,
                    "raw_child_v0_retained": True,
                    "live_o32_epilogue_reload": True},
                "visual_fixture": "generated retained scanout, not retail pixels",
                "captures": ["set-disp-mask-before.ppm",
                             "set-disp-mask-after.ppm"],
                "visual_effect": "black masked diagnostic scanout became the completed retained framebuffer; native frontend unchanged",
                "status": "display-enabled"},
            "recovered 0x80099458 SetDispMask receipt drifted")
    require(receipt["resource_validator_install"] == {
                "binary": "GAMEONLY", "address": "0x800A3E20",
                "end_exclusive": "0x800A3E38", "instructions": 6,
                "call_pc": "0x80029ABC",
                "callback_global": "0x800D7B1C",
                "previous_callback": "0x00000000",
                "installed_callback": "0x800A3D60",
                "callback_role": "whole-file CRCF validation",
                "callback_status": "separate untranslated function",
                "return_v0": "0x800A3D60", "operations": 1,
                "accesses": 1, "stores": 1, "child_calls": 0,
                "source_quirks": {
                    "unconditional_overwrite": True,
                    "previous_callback_not_read": True,
                    "callback_not_invoked": True,
                    "incidental_pointer_return": True},
                "visual_fixture": "generated retained scanout, not retail pixels",
                "captures": ["crc-validator-install-before.ppm",
                             "crc-validator-install-after.ppm"],
                "visual_effect": "callback pointer installed; retained scanout and native frontend unchanged",
                "status": "crcf-validator-registered"},
            "recovered 0x800A3E20 resource-validator installer receipt drifted")
    require(receipt["frame_rate_reset"] == {
                "binary": "GAMEONLY", "address": "0x800A7738",
                "end_exclusive": "0x800A7770", "instructions": 14,
                "call_pc": "0x80029AD4",
                "consumer": "0x800A7460 cmn_frate.c tracker",
                "words": {
                    "frame_counter": {"address": "0x800D7B44",
                                      "before": 9, "after": 0},
                    "auxiliary": {"address": "0x800D7B48",
                                  "before": 0x11111111, "after": 0},
                    "clock_baseline": {"address": "0x800D7B4C",
                                       "before": 0x22222222, "after": 0},
                    "instantaneous_rate_fixed": {"address": "0x800D7B50",
                                                 "before": 0x33333333,
                                                 "after": 0},
                    "average_rate_fixed": {"address": "0x800D7B54",
                                            "before": 0x44444444,
                                            "after": 0},
                    "last_report_clock": {"address": "0x800D7B58",
                                          "before": 0x55555555,
                                          "after": 0}},
                "clock_leaf": "0x800A5810", "clock_source": "0x800D7A70",
                "sampled_clock": 0, "sample_known": True, "return_v0": 0,
                "operations": 9, "accesses": 8, "reads": 1,
                "stores": 7, "child_calls": 1,
                "source_quirks": {
                    "clears_precede_clock_callback": True,
                    "unguarded_sample_store": True,
                    "incidental_sample_return": True,
                    "gp_relative_words": True,
                    "live_o32_ra_reload": True,
                    "auxiliary_role_unproven": True},
                "visual_fixture": "generated retained scanout, not retail pixels",
                "captures": ["frame-rate-reset-before.ppm",
                             "frame-rate-reset-after.ppm"],
                "visual_effect": "tracker state reset; retained scanout and native frontend unchanged",
                "status": "frame-rate-tracker-reset"},
            "recovered 0x800A7738 frame-rate reset receipt drifted")
    require(receipt["match_session"] == {
                "binary": "GAMEONLY", "address": "0x8002D8D4",
                "end_exclusive": "0x8002DB68", "instructions": 165,
                "call_pc": "0x80029ADC",
                "instruction_sha256":
                    "8b903bb9beff9912b32380c6def33d0d05dae91c37bef14f99228587c1a9851e",
                "path": "ordinary-no-custom-location", "operations": 54,
                "accesses": 31, "reads": 6, "stores": 25,
                "child_calls": 23,
                "child_entries": [
                    "0x800AA0BC", "0x800A7738", "0x8009CA00",
                    "0x8009CAD0", "0x8009CA00", "0x8009CAD0",
                    "0x8002DB90", "0x8002DB68", "0x8002DC38",
                    "0x8002DC58", "0x800AA0BC", "0x80029BDC",
                    "0x800994F4", "0x80029BDC", "0x80029BDC",
                    "0x80029BDC", "0x80029BDC", "0x80029BDC",
                    "0x80029BDC", "0x80029BDC", "0x80029BDC",
                    "0x80029BDC", "0x80029BDC"],
                "calls": {"clear_rectangle": 2, "frame_rate_reset": 1,
                          "set_default_environment": 4,
                          "location_lookup": 0, "session_stage": 4,
                          "presentation_wait": 11, "draw_sync": 1},
                "environments": {
                    "draw": ["0x80021EEC", "0x80021F48"],
                    "display": ["0x8002205C", "0x80022070"],
                    "extent": [512, 240]},
                "state": {
                    "video_halfword_0x80021498": {"before": 0, "after": 1},
                    "draw_control_0x80021F04": {"before": 0, "after": 1},
                    "draw_control_0x80021F60": {"before": 0, "after": 1},
                    "session_flag_0x800EB680": {"before": 0, "after": 1},
                    "exit_byte_0x80015021": {"before": 0, "after": 0},
                    "vblank_counter_0x800D7A88": {"before": 1, "after": 12},
                    "frame_counter_0x800D7B44": {"before": 0, "after": 0}},
                "presentation": {"waits": 11,
                                 "source_vblank_signals": 11,
                                 "host_sleep_used": False},
                "downstream_stages": {
                    "initialize_0x8002DB90": "recovered-owner-with-typed-children",
                    "load_scene_0x8002DB68": "acknowledged-boundary",
                    "run_loop_0x8002DC38": "acknowledged-boundary",
                    "teardown_0x8002DC58": "acknowledged-boundary"},
                "source_quirks": {
                    "independent_location_recheck": True,
                    "late_enable_can_restore_zero_fields": True,
                    "late_disable_can_skip_restore": True,
                    "team_index_reloaded_for_each_phase": True,
                    "changing_index_can_split_records": True,
                    "team_index_unchecked": True,
                    "signed_low16_location": True,
                    "live_o32_epilogue_reload": True},
                "visual_fixture": "generated retained scanout, not retail pixels",
                "captures": ["match-session-before.ppm",
                             "match-session-after.ppm"],
                "visual_effect": "session state and environment controls changed; retained scanout stayed pixel-identical because downstream gameplay stages remain explicit boundaries",
                "status": "match-session-orchestrated"},
            "recovered 0x8002D8D4 match-session receipt drifted")
    require(receipt["loading_screen"] == {
                "binary": "GAMEONLY", "address": "0x80029E58",
                "end_exclusive": "0x80029F20", "instructions": 50,
                "call_pc": "0x80029AE4",
                "instruction_sha256":
                    "a7cd09cf9222d55787b6188292a434ef2d3645f61fc8cbe214251ac39827bf7e",
                "resource_name": {"address": "0x800247F8",
                                  "text": "zloadscr.psh"},
                "image_key": {"address": "0x80024808", "text": "LdS1"},
                "resource_handle": "0x80130000",
                "image_address": "0x80140000",
                "path": "loaded-resource", "operations": 16,
                "accesses": 6, "reads": 3, "stores": 3,
                "child_calls": 10,
                "child_entries": ["0x80029BFC", "0x800A5478",
                                  "0x800994F4", "0x800946B8",
                                  "0x800994F4", "0x800946B8",
                                  "0x800994F4", "0x800946B8",
                                  "0x800994F4", "0x80090698"],
                "draw_sync_calls": 4,
                "uploads": {"owner": "0x800946B8",
                            "coordinates": [[0, 0], [0, 256], [512, 0]],
                            "source_format": "16-bit retained fixture",
                            "source_extent": [512, 240],
                            "transfer_callbacks": 3,
                            "pixel_words": 368640},
                "resource_released": True, "return_v0": 0,
                "source_quirks": {
                    "null_resource_silently_skips": True,
                    "null_image_is_not_guarded": True,
                    "sync_before_each_upload_and_after_last": True,
                    "fifth_upload_argument_is_delay_slot_zero": True,
                    "release_v0_remains_live": True,
                    "live_o32_epilogue_reload": True},
                "visual_fixture":
                    "generated retained 512x240 image, not retail art",
                "captures": ["loading-screen-display-before.ppm",
                             "loading-screen-display-after.ppm",
                             "loading-screen-vram-before.ppm",
                             "loading-screen-vram-after-top-left.ppm",
                             "loading-screen-vram-after-bottom-left.ppm",
                             "loading-screen-vram-complete.ppm"],
                "visual_effect": "the same generated image was uploaded to the exact three source coordinates; the full-VRAM captures expose each incremental placement",
                "status": "loading-screen-composited"},
            "recovered 0x80029E58 loading-screen receipt drifted")
    require(receipt["resource_loader"] == {
                "binary": "GAMEONLY", "address": "0x80029BFC",
                "end_exclusive": "0x80029C40", "instructions": 17,
                "source_bytes_sha256":
                    "9534c90429813e90d899fe455f4d83c249eb738b1bc06b93be4470dd0486f9dc",
                "load_attempt_entry": "0x800941C8", "invocations": 2,
                "attempt_calls": 5, "null_results": 3,
                "callers": [{
                    "call_pc": "0x80029E70",
                    "resource_name": {"address": "0x800247F8",
                                      "text": "zloadscr.psh"},
                    "attempts": 2, "null_results": 1,
                    "result": "0x80130000"}, {
                    "call_pc": "0x80029AFC",
                    "resource_name": {"address": "0x800247EC",
                                      "text": "feload.bin"},
                    "attempts": 3, "null_results": 2,
                    "result": "0x80123400"}],
                "operations": [8, 9], "accesses": [6, 6],
                "reads": [3, 3], "stores": [3, 3],
                "source_quirks": {
                    "retries_null_forever": True,
                    "no_timeout_or_backoff": True,
                    "arguments_cached_across_retries": True,
                    "successful_v0_remains_live": True,
                    "live_o32_epilogue_reload": True},
                "captures": ["resource-loader-zload-before.ppm",
                             "resource-loader-zload-after.ppm",
                             "resource-loader-feload-before.ppm",
                             "resource-loader-feload-after.ppm"],
                "visual_effect": "the retry wrapper changed no pixels; its successful results fed the recovered loading-screen compositor and the FELOAD transfer",
                "status": "retry-wrapper-completed"},
            "recovered 0x80029BFC resource-loader receipt drifted")
    require(receipt["heap_payload_size"] == {
                "binary": "GAMEONLY", "address": "0x80090D60",
                "end_exclusive": "0x80090D84", "instructions": 9,
                "source_bytes_sha256":
                    "665368c63a001c084cd5c009548768ad5db5a385cad175c378e9f10f7ccdaaa0",
                "call_pc": "0x80029B08", "payload": "0x80123400",
                "descriptor_lookup_entry": "0x80090618",
                "descriptor": "0x8010B66C", "requested_size": 5136,
                "operations": 4, "accesses": 3, "reads": 2,
                "stores": 1, "child_calls": 1,
                "lookup": {"actual_recovered_owner": True,
                           "accesses": 5, "stores": 0},
                "fixture": "successful FELOAD service publishes one retained allocation descriptor",
                "source_quirks": {
                    "null_descriptor_reads_low_ram_0x14": True,
                    "descriptor_plus_0x14_wraps_32_bit": True,
                    "requested_size_read_precedes_live_ra_reload": True,
                    "malformed_heap_sentinel_behavior_retained": True},
                "captures": ["heap-payload-size-before.ppm",
                             "heap-payload-size-after.ppm"],
                "visual_effect": "no pixels changed; the returned allocation size feeds the FELOAD overlay transfer",
                "status": "requested-size-returned"},
            "recovered 0x80090D60 heap payload-size receipt drifted")
    require(receipt["cd_sync"] == {
                "binary": "GAMEONLY", "address": "0x8009DBA0",
                "end_exclusive": "0x8009DBC0", "instructions": 8,
                "source_bytes_sha256":
                    "3950cb563b219b3b5b59d41cd74547b23be952e3f494769fc8d77fe186380db3",
                "psyq_name": "CdSync", "call_pc": "0x80029B34",
                "mode": 0, "result_buffer": "0x00000000",
                "service_entry": "0x8009E740", "service_result": 2,
                "other_callers": ["0x80092028", "0x80092164",
                                  "0x80092274"],
                "operations": 3, "accesses": 2, "reads": 1,
                "stores": 1, "child_calls": 1,
                "service_scope": "typed CdlComplete fixture; no CD device or internal state-machine effects claimed",
                "source_quirks": {
                    "arguments_forwarded_unchanged": True,
                    "result_pointer_not_dereferenced_by_wrapper": True,
                    "child_v0_remains_live": True,
                    "live_o32_epilogue_reload": True,
                    "wrapper_adds_no_timeout_or_return_normalization": True},
                "captures": ["cd-sync-before.ppm", "cd-sync-after.ppm"],
                "visual_effect": "no pixels changed; the wrapper synchronizes the CD command boundary before callback removal",
                "status": "cd-command-synchronized"},
            "recovered 0x8009DBA0 CdSync receipt drifted")
    require(receipt["cd_ready_callback"] == {
                "binary": "GAMEONLY", "address": "0x8009DBE0",
                "end_exclusive": "0x8009DBF8", "instructions": 6,
                "source_bytes_sha256":
                    "98c5f9f745cd61ca8a7268bf74d7dea2419d421b67d277c31d38f64b41113414",
                "psyq_name": "CdReadyCallback", "call_pc": "0x80029B3C",
                "callback_global": "0x800C57E4",
                "requested_callback": "0x00000000",
                "previous_callback": "0x8009D9DC",
                "fixture_origin": "source default callback installed by earlier untranslated CdInit boundary",
                "other_callers": ["0x8009D978", "0x8009FABC",
                                  "0x8009FC4C", "0x8009FC80",
                                  "0x8009FE64", "0x8009FEEC",
                                  "0x800A0144"],
                "operations": 2, "accesses": 2, "reads": 1, "stores": 1,
                "source_quirks": {
                    "previous_value_read_before_store": True,
                    "raw_replacement_not_validated": True,
                    "previous_value_can_remain_unknown": True,
                    "unknown_previous_does_not_suppress_store": True,
                    "no_callback_invoked": True},
                "captures": ["cd-ready-callback-before.ppm",
                             "cd-ready-callback-after.ppm"],
                "visual_effect": "no pixels changed; the ready callback slot changed from 0x8009D9DC to NULL",
                "status": "ready-callback-cleared"},
            "recovered 0x8009DBE0 CdReadyCallback receipt drifted")
    require(receipt["cd_sync_callback"] == {
                "binary": "GAMEONLY", "address": "0x8009DBF8",
                "end_exclusive": "0x8009DC10", "instructions": 6,
                "source_bytes_sha256":
                    "a5f87457838841a01d7e1d1695406ed58575fa304d34b46e5ef4eb106cadddae",
                "psyq_name": "CdSyncCallback", "call_pc": "0x80029B44",
                "callback_global": "0x800C57E8",
                "requested_callback": "0x00000000",
                "previous_callback": "0x8009DA04",
                "fixture_origin": "source default callback installed by earlier untranslated CdInit boundary",
                "other_callers": ["0x8002B70C", "0x8002BB14",
                                  "0x80091F44", "0x80091FC4",
                                  "0x8009D988", "0x8009F8F0",
                                  "0x8009F998", "0x8002D244",
                                  "0x80092360", "0x80092760",
                                  "0x8009FE74", "0x8009FEF4",
                                  "0x800A0044", "0x800A0158"],
                "operations": 2, "accesses": 2, "reads": 1, "stores": 1,
                "source_quirks": {
                    "previous_value_read_before_store": True,
                    "raw_replacement_not_validated": True,
                    "previous_value_can_remain_unknown": True,
                    "unknown_previous_does_not_suppress_store": True,
                    "no_callback_invoked": True},
                "captures": ["cd-sync-callback-before.ppm",
                             "cd-sync-callback-after.ppm"],
                "visual_effect": "no pixels changed; the sync callback slot changed from 0x8009DA04 to NULL",
                "status": "sync-callback-cleared"},
            "recovered 0x8009DBF8 CdSyncCallback receipt drifted")
    require(receipt["vblank_shutdown"] == {
                "binary": "GAMEONLY", "address": "0x800A44D4",
                "end_exclusive": "0x800A450C", "instructions": 14,
                "source_bytes_sha256":
                    "d30124f93b39486830bd850d0f764977363aebcc9919f7546bf0c1917be5a54c",
                "call_pc": "0x80029B64", "service": "InterruptCallback",
                "service_entry": "0x8009860C", "interrupt_number": 0,
                "callback_slot": "0x800C54D0",
                "replacement_callback": "0x00000000",
                "previous_handler": "0x800A450C",
                "fixture_origin": "handler installed by the earlier recovered VBlank initializer",
                "only_caller": "0x80029B64", "operations": 5,
                "accesses": 4, "reads": 2, "stores": 2,
                "child_calls": 1,
                "source_quirks": {
                    "no_critical_section": True,
                    "hardcoded_interrupt_and_null_callback": True,
                    "child_v0_remains_live": True,
                    "live_saved_ra_reload": True,
                    "live_saved_s8_reload": True,
                    "previous_handler_not_checked": True},
                "service_scope": "typed PS1 callback-table fixture; no host interrupt or timing effect claimed",
                "captures": ["vblank-shutdown-before.ppm",
                             "vblank-shutdown-after.ppm"],
                "visual_effect": "no pixels changed; retained VBlank handler state changed from installed to removed",
                "status": "vblank-handler-removed"},
            "recovered 0x800A44D4 VBlank shutdown receipt drifted")
    require(receipt["clock_shutdown"] == {
                "binary": "GAMEONLY", "address": "0x8009167C",
                "end_exclusive": "0x800916B4", "instructions": 14,
                "source_bytes_sha256":
                    "0724e7dd8a73dd92dde6a9128d2435f60888f950b29d1bf83f6d8e29f259c5dd",
                "call_pc": "0x80029B6C", "service": "InterruptCallback",
                "service_entry": "0x8009860C", "interrupt_number": 6,
                "callback_slot": "0x800C54E8",
                "replacement_callback": "0x00000000",
                "previous_handler": "0x800916B4",
                "fixture_origin": "handler installed by the earlier recovered game-clock initializer",
                "direct_caller": "0x80029B6C",
                "registered_shutdown_handler": True, "operations": 5,
                "accesses": 4, "reads": 2, "stores": 2,
                "child_calls": 1,
                "source_quirks": {
                    "no_critical_section": True,
                    "hardcoded_interrupt_and_null_callback": True,
                    "child_v0_remains_live": True,
                    "live_saved_ra_reload": True,
                    "live_saved_s8_reload": True,
                    "previous_handler_not_checked": True},
                "service_scope": "typed PS1 callback-table fixture; no host interrupt or timer effect claimed",
                "captures": ["clock-shutdown-before.ppm",
                             "clock-shutdown-after.ppm"],
                "visual_effect": "no pixels changed; retained game-clock IRQ6 handler state changed from installed to removed",
                "status": "clock-handler-removed"},
            "recovered 0x8009167C game-clock shutdown receipt drifted")
    require(receipt["controller_suspend"] == {
                "binary": "GAMEONLY", "address": "0x8008F19C",
                "end_exclusive": "0x8008F1D4", "instructions": 14,
                "source_bytes_sha256":
                    "40a13c532487813e5aee2bb9caf333e1c69ddbb581cef01b9ae24ea103e10570",
                "call_pc": "0x80029B74",
                "suspend_flag_global": "0x800C4A70",
                "initial_suspend_flag": 0, "final_suspend_flag": 1,
                "shutdown_service_entry": "0x80091224",
                "only_caller": "0x80029B74", "operations": 5,
                "accesses": 4, "reads": 2, "stores": 2,
                "child_calls": 1, "return_v0": 1,
                "return_v0_known": True,
                "child_return_fixture": "unknown-and-discarded",
                "source_quirks": {
                    "read_flag_before_frame_allocation": True,
                    "branch_delay_ra_store_always": True,
                    "conditional_shutdown_and_flag_store": True,
                    "child_v0_discarded": True,
                    "nonzero_fast_path_not_normalized": True,
                    "live_saved_ra_reload": True},
                "service_scope": "typed PS1 controller shutdown fixture; no host input device effect claimed",
                "captures": ["controller-suspend-before.ppm",
                             "controller-suspend-after.ppm"],
                "visual_effect": "no pixels changed; retained PS1 input state changed from active to suspended",
                "status": "input-suspended"},
            "recovered 0x8008F19C controller-suspend receipt drifted")
    require(receipt["memory_zero"] == {
                "binary": "GAMEONLY", "entry_address": "0x800A3A74",
                "shared_core_address": "0x800A3A78",
                "end_exclusive": "0x800A3BB8", "entry_instructions": 1,
                "shared_core_instructions": 80, "effective_instructions": 81,
                "entry_sha256":
                    "3eec77d0e95c14d4c06c9e1d4548029c2bcc34fa7770a485652dbb193a79036c",
                "shared_core_sha256":
                    "5cf83e6e51d1bf5e8b4accba1415bedee7aa4d9a5c63c188b29f34b1678825f8",
                "effective_path_sha256":
                    "968a1ee3cee7769e2adb6c49db48dfe8836a0c76d91f05581076bf809690f772",
                "call_pc": "0x80029B84", "destination": "0x800D6DEC",
                "length": 32, "unique_bytes_cleared": 32,
                "operations": 9, "accesses": 9, "stores": 9,
                "store_traffic_bytes": 36,
                "working_destination": "0x800D6E08",
                "working_count": "0xFFFFFFFC", "return_v0": 1,
                "return_v0_known": True,
                "state_before": "already-zero-from-clock-initialize",
                "state_after": "zero",
                "source_quirks": {
                    "swr_head_store": True, "swl_tail_store": True,
                    "overlapping_store_traffic": True,
                    "zero_length_writes_one_byte": True,
                    "int_min_wraps_to_huge_byte_loop": True,
                    "incoming_v0_remains_live": True},
                "captures": ["shutdown-table-zero-before.ppm",
                             "shutdown-table-zero-after.ppm"],
                "visual_effect": "no pixels changed; eight already-zero shutdown callback words were explicitly cleared again",
                "status": "shutdown-table-cleared"},
            "recovered 0x800A3A74 zero-fill receipt drifted")
    require(receipt["memory_copy"] == {
                "binary": "GAMEONLY", "address": "0x800AA468",
                "end_exclusive": "0x800AA788", "instructions": 200,
                "instruction_sha256":
                    "2d9ed18f5de6fe3edc1fab9996769b418452b1c32eb3fd2cce7ed1f2b0c2350d",
                "call_pc": "0x80029B94", "source": "0x80123400",
                "destination": "0x801E0000", "length": 5136,
                "direction": "forward", "alignment_result_v0": 0,
                "operations": 2568, "accesses": 2568,
                "reads": 1284, "stores": 1284,
                "read_traffic_bytes": 5136,
                "store_traffic_bytes": 5136,
                "destination_changed": True, "payload_matches": True,
                "entry_word_before": "0x00000000",
                "entry_word_after": "0x801E1410",
                "source_quirks": {
                    "signed_address_comparisons": True,
                    "trapping_signed_end_adds": True,
                    "grouped_loads_precede_grouped_stores": True,
                    "unaligned_lwl_lwr_swl_swr_pairs": True,
                    "aligned_backward_tail_repeats_partial_word_traffic": True,
                    "negative_length_can_wrap_to_huge_loop": True,
                    "return_is_alignment_bits_not_destination": True},
                "captures": ["feload-memory-copy-before.ppm",
                             "feload-memory-copy-after.ppm"],
                "visual_effect": "no pixels changed; 5136 retained CPU bytes moved and main then read the copied overlay entry",
                "status": "feload-image-copied"},
            "recovered 0x800AA468 memory-copy receipt drifted")
    result = receipt["result"]
    require(result == {"status": "transferred", "callbacks": 77, "stores": 15,
                       "reads": 1, "match_orchestration": "0x8002D8D4",
                       "loading_screen": "0x80029E58",
                       "resource_loader": "0x80029BFC",
                       "heap_payload_size": "0x80090D60",
                       "loaded_image": "0x80123400", "loaded_size": 5136,
                       "cd_sync": "0x8009DBA0",
                       "cd_ready_callback": "0x8009DBE0",
                       "cd_sync_callback": "0x8009DBF8",
                       "vblank_shutdown": "0x800A44D4",
                       "clock_shutdown": "0x8009167C",
                       "controller_suspend": "0x8008F19C",
                       "memory_zero": "0x800A3A74",
                       "memory_copy": "0x800AA468",
                       "indirect_entry": "0x801E1410"},
            "translated game-entry result drifted")
    calls = receipt["calls"]
    require(len(calls) == 77 and [call["index"] for call in calls] == list(range(77)),
            "runtime call extent/order drifted")
    require(calls[48]["pc"] == "0x80029B34" and
            calls[48]["entry"] == "0x8009DBA0" and
            calls[49]["pc"] == "0x80029B3C" and
            calls[49]["entry"] == "0x8009DBE0" and
            calls[50]["pc"] == "0x80029B44" and
            calls[50]["entry"] == "0x8009DBF8",
            "CdSync/callback-exchange main boundaries drifted")
    require(calls[71]["pc"] == "0x80029B64" and
            calls[71]["entry"] == "0x800A44D4",
            "VBlank shutdown main boundary drifted")
    require(calls[72]["pc"] == "0x80029B6C" and
            calls[72]["entry"] == "0x8009167C",
            "game-clock shutdown main boundary drifted")
    require(calls[73]["pc"] == "0x80029B74" and
            calls[73]["entry"] == "0x8008F19C" and
            calls[73]["kind"] == "direct",
            "controller-suspend main boundary drifted")
    require(calls[74]["pc"] == "0x80029B84" and
            calls[74]["entry"] == "0x800A3A74" and
            calls[74]["kind"] == "direct",
            "shutdown-table zero-fill main boundary drifted")
    require(calls[0]["pc"] == "0x800299A4" and calls[0]["entry"] == "0x800948D0",
            "first initialization boundary drifted")
    require(calls[1]["pc"] == "0x800299AC" and calls[1]["entry"] == "0x800A4830",
            "global-pointer save boundary drifted")
    require(calls[2]["pc"] == "0x800299C8" and calls[2]["entry"] == "0x8008FA6C",
            "heap-initialization boundary drifted")
    require(calls[3]["pc"] == "0x800299D8" and calls[3]["entry"] == "0x80091C08",
            "CD-directory initialization boundary drifted")
    require(calls[4]["pc"] == "0x800299E8" and calls[4]["entry"] == "0x800A35D8",
            "path-prefix selection boundary drifted")
    require(calls[5]["pc"] == "0x800299F8" and calls[5]["entry"] == "0x80092C7C",
            "directory-cache configuration boundary drifted")
    require(calls[6]["pc"] == "0x80029A08" and calls[6]["entry"] == "0x800985B4",
            "interrupt-mask clear boundary drifted")
    require(calls[7]["pc"] == "0x80029A10" and calls[7]["entry"] == "0x800985DC",
            "ResetCallback dispatch boundary drifted")
    require(calls[8]["pc"] == "0x80029A18" and calls[8]["entry"] == "0x8008F1D4" and
            calls[11]["pc"] == "0x80029A30" and calls[11]["entry"] == "0x8008F1D4",
            "controller-resume call boundaries drifted")
    require(calls[9]["pc"] == "0x80029A20" and calls[9]["entry"] == "0x80099058",
            "ResetGraph call boundary drifted")
    require(calls[10]["pc"] == "0x80029A28" and calls[10]["entry"] == "0x800992C4",
            "SetGraphDebug call boundary drifted")
    require(calls[12]["pc"] == "0x80029A38" and calls[12]["entry"] == "0x800A43E8",
            "VBlank initialization boundary drifted")
    require(calls[13]["pc"] == "0x80029A4C" and calls[13]["entry"] == "0x800914D8",
            "game-clock initialization boundary drifted")
    require(calls[14]["pc"] == "0x80029A54" and calls[14]["entry"] == "0x80056678",
            "GTE initialization boundary drifted")
    require(calls[15]["pc"] == "0x80029A5C" and calls[15]["entry"] == "0x800A584C",
            "clock-delta boundary drifted")
    require(calls[16]["pc"] == "0x80029A64" and calls[16]["entry"] == "0x80029BDC" and
            all(call["pc"] == "0x80029B20" and call["entry"] == "0x80029BDC"
                for call in calls[28:48]) and
            all(call["pc"] == "0x80029B50" and call["entry"] == "0x80029BDC"
                for call in calls[51:71]),
            "presentation-wait boundaries drifted")
    require(calls[17]["pc"] == "0x80029A6C" and
            calls[17]["entry"] == "0x80029F20",
            "video-environment initialization boundary drifted")
    require(calls[18]["pc"] == "0x80029A94" and
            calls[18]["entry"] == "0x800997E4" and
            calls[19]["pc"] == "0x80029AA4" and
            calls[19]["entry"] == "0x800997E4",
            "two MoveImage startup boundaries drifted")
    require(calls[20]["pc"] == "0x80029AAC" and
            calls[20]["entry"] == "0x800994F4",
            "DrawSync startup boundary drifted")
    require(calls[21]["pc"] == "0x80029AB4" and
            calls[21]["entry"] == "0x80099458",
            "SetDispMask startup boundary drifted")
    require(calls[22]["pc"] == "0x80029ABC" and
            calls[22]["entry"] == "0x800A3E20",
            "resource-validator install boundary drifted")
    require(calls[23]["pc"] == "0x80029AD4" and
            calls[23]["entry"] == "0x800A7738",
            "frame-rate reset boundary drifted")
    require(calls[24]["pc"] == "0x80029ADC" and calls[24]["entry"] == "0x8002D8D4",
            "match orchestration boundary drifted")
    require(calls[25]["pc"] == "0x80029AE4" and calls[25]["entry"] == "0x80029E58",
            "execution did not continue after the recovered match-session owner")
    require(calls[26]["entry"] == "0x80029BFC" and calls[27]["entry"] == "0x80090D60",
            "FELOAD load/size boundaries drifted")
    require([call["s0"] for call in calls[28:48]] ==
            [f"0x{value:08X}" for value in range(1, 21)] and
            [call["s0"] for call in calls[51:71]] ==
            [f"0x{value:08X}" for value in range(1, 21)],
            "delay-slot loop register order drifted")
    require(calls[75]["pc"] == "0x80029B94" and
            calls[75]["entry"] == "0x800AA468" and
            calls[75]["kind"] == "direct" and
            calls[76] == {"index": 76, "kind": "indirect", "pc": "0x80029BA8",
                          "entry": "0x801E1410", "s0": "0x00000014"},
            "loaded image copy/transfer boundary drifted")

    trace = args.trace.read_text(encoding="utf-8-sig")
    require("MATCH-HANDOFF-PENDING" in trace and "GAME-ENTRY-DIAG" in trace and
            "native recovered-input click-through" in trace and
            "0x800948D0 executed recovered owner" in trace and
            "guard 0x800C4B14 changed 0->1" in trace and
            "0x800A4830 executed recovered owner" in trace and
            "saved gp 0x800D79C8 to 0x800D6E2C" in trace and
            "0x8008FA6C executed recovered heap owner" in trace and
            "220 descriptors, 248 stores" in trace and
            "0x80091C08 executed recovered CD-directory owner" in trace and
            "10 child calls, root LBA 23, length 2048" in trace and
            "0x800A35D8 executed recovered path-prefix owner" in trace and
            "2 BIOS string calls, copied cdrom: to 0x800D6DAC" in trace and
            "skipped separator append because the source ended in colon" in trace and
            "0x80092C7C executed recovered directory-cache owner" in trace and
            "preallocated 707-entry, 14140-byte PS1 cache at 0x8001000C" in trace and
            "0x800985B4 executed recovered PsyQ SetIntrMask owner" in trace and
            "returned prior mask 0x000007FF" in trace and
            "cleared mapped PS1 interrupt/callback mask 0x800C54AC before ResetCallback" in trace and
            "without changing native OS interrupts or rendering" in trace and
            "0x800985DC executed recovered PsyQ ResetCallback dispatch wrapper" in trace and
            "loaded table 0x800C54B0 through 0x800C54C8" in trace and
            "slot +0x0C target 0x80098714" in trace and
            "saved and restored caller RA 0x80029A18" in trace and
            "invoked one explicit diagnostic child fixture" in trace and
            "wrapper changed no native OS callbacks or pixels" in trace and
            "controller-resume owner 0x8008F1D4 ran at call PCs 0x80029A18 and 0x80029A30" in trace and
            "first saw suspend flag 1, invoked initializer 0x80091184" in trace and
            "stored clock 37 from 0x800A5810 at 0x800C4A74" in trace and
            "second saw input already active and only reasserted mode 8 at 0x800D7A48" in trace and
            "native input devices and pixels did not" in trace and
            "0x80099058 executed PsyQ ResetGraph(3)" in trace and
            "cleared 128 bookkeeping bytes" in trace and
            "nested ResetCallback to 0x80098714" in trace and
            "called BIOS A0:49 with 0x000C5578" in trace and
            "published reset type 0 and 1024x512 limits at 0x800C55C0" in trace and
            "filled 112 cached environment bytes with 0xFF" in trace and
            "original mode-mask, low-byte truncation, unchecked type index and unguarded dispatch quirks remain" in trace and
            "native renderer and captured pixels were unchanged" in trace and
            "0x800992C4 executed PsyQ SetGraphDebug(0)" in trace and
            "stored debug level 0 at 0x800C55C2" in trace and
            "returned previous level 0" in trace and
            "skipped the 0x800C55BC diagnostic pointer" in trace and
            "original byte truncation, zero-low-byte alias, ignored callback return and unguarded nonzero dispatch quirks remain" in trace and
            "native logging, renderer and captured pixels were unchanged" in trace and
            "0x800A43E8 initialized the VBlank service" in trace and
            "cleared eight callback words at 0x800D6E0C" in trace and
            "installed handler 0x800A450C on interrupt channel 0" in trace and
            "issued SetRCnt/StartRCnt for 0xF2000003" in trace and
            "reset frame counters 0x800D7A88, 0x800D7AFC and 0x800D7B00" in trace and
            "SetRCnt rejected index 3 while StartRCnt still unmasked VBlank before returning false" in trace and
            "both raw returns were ignored" in trace and
            "did not install a native OS interrupt or synthesize VBlank cadence" in trace and
            "captured frontend frames were unchanged" in trace and
            "0x800914D8 initialized the source game clock" in trace and
            "cold guard 0x800C4AA4 changed 0->1" in trace and
            "eight callback words at 0x800D6DEC were cleared" in trace and
            "IRQ6 handler 0x800916B4 was installed" in trace and
            "shutdown handler 0x8009167C was registered" in trace and
            "signed 4233600/120 produced Timer 2 target 35280 and effective rate 120" in trace and
            "SetRCnt/StartRCnt for 0xF2000002 returned true" in trace and
            "diagnostic hardware mode 0x0258 and interrupt-mask bit 0x0040" in trace and
            "clock globals 0x800D7A7C, 0x800D7A70, 0x800D7B2C and 0x800D7B28 were reset" in trace and
            "original signed double-division quantization and prefix-committing divide BREAK paths remain" in trace and
            "did not install a native OS interrupt or synthesize Timer 2 cadence" in trace and
            "0x80056678 initialized retained GTE projection state" in trace and
            "CP0 Status 0x10900401 became 0x50900401 by setting only CU2" in trace and
            "ZSF3 0x0155, ZSF4 0x0100, H 1000, DQA -4194, DQB 0x01400000, OFX 0 and OFY 0" in trace and
            "matrices, FIFOs, FLAG and the other 25 control registers remain live exactly as in GAMEONLY" in trace and
            "establishes later court/player/net projection inputs" in trace and
            "does not submit a GPU packet or change any of the captured frontend frames" in trace and
            "0x800A584C refreshed the gameplay clock baseline" in trace and
            "captured gp+0x164 (0x800D7B2C) as 0" in trace and
            "0x800A5810 leaf to sample retained clock 0" in trace and
            "returned delta 0" in trace and
            "original pre-child capture, commit-before-return, gp-relative addressing and raw 32-bit SUBU wraparound remain" in trace and
            "no host cadence was invented" in trace and
            "0x80029BDC executed its presentation-wait wrapper" in trace and
            "both twenty-iteration loops at 0x80029B20 and 0x80029B50" in trace and
            "for 41 invocations total" in trace and
            "explicit synchronization service 0x800A9CC0" in trace and
            "ready flag 0x800D7A80" in trace and
            "source 0x800A450C VBlank ISR" in trace and
            "contributed 41 increments to frame counter 0x800D7A88" in trace and
            "embedded match-session owner contributed eleven more for a final 52" in trace and
            "incidental v0 remained live and no timeout was added" in trace and
            "did not sleep on a host clock, drive the native renderer" in trace and
            "0x80029F20 initialized GAMEONLY's PS1 double-buffer environments" in trace and
            "call PC 0x80029A6C with mode 0" in trace and
            "display rectangles at (0,256,512,240) and (0,0,512,240)" in trace and
            "opposite draw rectangles at y=0/y=256" in trace and
            "four SetDef calls, four Put calls and DrawSync(0) completed" in trace and
            "leaving pair 1 last installed while selector 0x8001EDE8 was reset to 0" in trace and
            "all four o32 fifth arguments executed as mapped JAL delay-slot stores" in trace and
            "dtd/isbg are changed in two adjacent DRAWENV records never passed to SetDefDrawEnv" in trace and
            "RGB is cleared only in the two initialized records" in trace and
            "does not draw, so none of the 100 natively captured frontend frames changed" in trace and
            "0x800997E4 executed PsyQ MoveImage twice" in trace and
            "call PCs 0x80029A94 and 0x80029AA4" in trace and
            "RECT(512,0,512,256) submitted copies" in trace and
            "first to (0,0), then to (0,256)" in trace and
            "unconditional 0x80099560 diagnostic boundary" in trace and
            "retained packet header words 0x04FFFFFF/0x80000000" in trace and
            "wrote source/destination/extent at 0x800C5670..0x800C5678" in trace and
            "live table 0x800C5578 target 0x8009B298" in trace and
            "only exact zero extents rejected while negative extents dispatch" in trace and
            "low-16-bit destination truncation" in trace and
            "move-image-before-buffer0.ppm" in trace and
            "generated retained-VRAM test grid, not retail art" in trace and
            "0x800994F4 ran PsyQ DrawSync(0)" in trace and
            "call PC 0x80029AAC" in trace and
            "recovered 27-instruction wrapper and default 0x8009B9B4 closure" in trace and
            "live table 0x800C5578 slot +0x3C resolved to 0x8009B9B4" in trace and
            "2 submitted MoveImage packets and 0 completed" in trace and
            "DMA2 reported busy once" in trace and
            "four timer-register reads preserved timeout accounting" in trace and
            "second observation required both packets complete" in trace and
            "262144 16-bit words became visible" in trace and
            "draw-sync-before-buffer0.ppm" in trace and
            "debug-before-table-reload" in trace and
            "signed timeout comparisons" in trace and
            "post-incremented poll counter" in trace and
            "timeout reset/-1 return" in trace and
            "live o32 epilogue quirks remain" in trace and
            "0x80099458 ran PsyQ SetDispMask(1)" in trace and
            "call PC 0x80029AB4" in trace and
            "recovered 39-instruction owner" in trace and
            "debug level 0 skipped 0x800C55BC" in trace and
            "disable-only 20-byte clear at 0x800C562C" in trace and
            "live table 0x800C5578 slot +0x10 resolved to retail target 0x8009B16C" in trace and
            "active-low GP1(03h) control word 0x03000000" in trace and
            "retained child v0=3" in trace and
            "display environment 0x80022070" in trace and
            "set-disp-mask-before.ppm is black while masked" in trace and
            "original full-word zero testing, active-low bit, disable pre-clear" in trace and
            "0x800A3E20 from call PC 0x80029ABC" in trace and
            "six-instruction owner" in trace and
            "replaced callback global 0x800D7B1C value 0x00000000" in trace and
            "whole-file CRCF validator 0x800A3D60" in trace and
            "made no child call" in trace and
            "incidentally retained 0x800A3D60 in v0" in trace and
            "separate validator body remains untranslated" in trace and
            "native host filesystem loader was not redirected" in trace and
            "original unconditional overwrite, no-read/no-guard registration" in trace and
            "crc-validator-install-before.ppm" in trace and
            "pixel-identical generated retained scanout" in trace and
            "0x800A7738 from call PC 0x80029AD4" in trace and
            "recovered 14-instruction frame-rate tracker reset" in trace and
            "0x800D7B44, auxiliary word 0x800D7B48" in trace and
            "cleared before the child call" in trace and
            "0x800A5810 then sampled retained source clock 0 into baseline 0x800D7B4C" in trace and
            "cmn_frate.c and TIMERHZ NOT SET diagnostics" in trace and
            "no host cadence was invented" in trace and
            "original pre-callback store order, unguarded sample store" in trace and
            "frame-rate-reset-before.ppm" in trace and
            "native frontend renderer" in trace and
            "0x8002D8D4 from call PC 0x80029ADC" in trace and
            "recovered 165-instruction match-session owner" in trace and
            "two clear boundaries bracketed four 512x240" in trace and
            "nested 0x800A7738 reset completed" in trace and
            "initialize 0x8002DB90 executed its recovered owner and zero-fill child" in trace and
            "ordinary no-custom-location path performed no team-table patch" in trace and
            "eleven recovered presentation wrappers" in trace and
            "without host sleeps" in trace and
            "independent location recheck, signed low-16 venue code" in trace and
            "repeated unchecked team-index loads" in trace and
            "late-enable zero restore, late-disable skipped restore" in trace and
            "split-record writes and live o32 reload bugs remain" in trace and
            "match-session-before.ppm and match-session-after.ppm are pixel-identical" in trace and
            "no downstream court or gameplay work was fabricated" in trace and
            "outer execution continued at 0x80029E58" in trace and
            "0x80029E58 from call PC 0x80029AE4" in trace and
            "recovered 50-instruction loading-screen compositor" in trace and
            "resource name zloadscr.psh at 0x800247F8" in trace and
            "key LdS1 at 0x80024808" in trace and
            "existing recovered 0x800946B8 owner performed three" in trace and
            "512x240 transfers at (0,0), (0,256) and (512,0)" in trace and
            "four explicit DrawSync(0) boundaries" in trace and
            "loading-screen-vram-complete.ppm" in trace and
            "original silent null-resource return, unchecked null-image dispatch" in trace and
            "self-driving test supplied inputs through recovered handlers" in trace and
            "not computer control" in trace and
            "continued to FELOAD" in trace and
            "next recovered boundary 0x80029BFC" in trace and
            "17-instruction resource-load retry wrapper" in trace and
            "attempt entry 0x800941C8" in trace and
            "zloadscr.psh from call PC 0x80029E70 returned null once" in trace and
            "feload.bin from call PC 0x80029AFC returned null twice" in trace and
            "five exact attempt calls and three known-null results" in trace and
            "filename and flags cached unchanged across retries" in trace and
            "resource-loader-zload-before.ppm and resource-loader-zload-after.ppm are pixel-identical" in trace and
            "resource-loader-feload-before.ppm" in trace and
            "all four frames and logs were captured natively without computer control" in trace and
            "persistent-failure infinite retry" in trace and
            "no timeout or backoff" in trace and
            "next recovered boundary 0x80090D60" in trace and
            "9-instruction heap payload-size query" in trace and
            "call PC 0x80029B08 after feload.bin loaded" in trace and
            "allocation descriptor 0x8010B66C" in trace and
            "actual recovered 0x80090618 heap owner" in trace and
            "five reads and no stores" in trace and
            "requested-size word +0x14 as 5136" in trace and
            "heap-payload-size-before.ppm and heap-payload-size-after.ppm are pixel-identical" in trace and
            "captured natively by the self-driving recovered-input test, not computer control" in trace and
            "unchecked null descriptor read from low RAM address 0x00000014" in trace and
            "32-bit pointer wrapping" in trace and
            "next recovered boundary 0x8009DBA0" in trace and
            "8-instruction PsyQ CdSync wrapper" in trace and
            "call PC 0x80029B34 after the first twenty post-FELOAD presentation waits" in trace and
            "forwarded mode 0 and null result pointer unchanged" in trace and
            "internal CD_sync service 0x8009E740" in trace and
            "returned CdlComplete code 2" in trace and
            "without claiming a CD device or the 160-instruction internal state machine" in trace and
            "retained that raw child v0" in trace and
            "cd-sync-before.ppm and cd-sync-after.ppm are pixel-identical" in trace and
            "exact child-call log were captured natively by the self-driving recovered-input test, not computer control" in trace and
            "no wrapper-side result-pointer validation" in trace and
            "no added timeout or return-code normalization" in trace and
            "next recovered boundary 0x8009DBE0" in trace and
            "6-instruction PsyQ CdReadyCallback exchange" in trace and
            "call PC 0x80029B3C immediately after CdSync" in trace and
            "source default ready callback 0x8009D9DC" in trace and
            "global 0x800C57E4" in trace and
            "stored main's null replacement" in trace and
            "returned the old pointer without invoking either callback" in trace and
            "internal CdReady 0x8009E9C0 reads this exact slot at 0x8009EB78" in trace and
            "distinguishing it from adjacent CdSyncCallback" in trace and
            "cd-ready-callback-before.ppm and cd-ready-callback-after.ppm are pixel-identical" in trace and
            "old/new pointer log were captured natively by the self-driving recovered-input test, not computer control" in trace and
            "possibly unknown old v0" in trace and
            "unconditional replacement" in trace and
            "next recovered boundary 0x8009DBF8" in trace and
            "6-instruction PsyQ CdSyncCallback exchange" in trace and
            "call PC 0x80029B44 immediately after CdReadyCallback" in trace and
            "source default sync callback 0x8009DA04" in trace and
            "global 0x800C57E8" in trace and
            "internal CD_sync 0x8009E740 reads this exact slot at 0x8009E8BC" in trace and
            "cd-sync-callback-before.ppm and cd-sync-callback-after.ppm are pixel-identical" in trace and
            "both frames and the old/new pointer log were captured natively by the self-driving recovered-input test, not computer control" in trace and
            "next recovered boundary 0x800A44D4" in trace and
            "14-instruction VBlank shutdown wrapper" in trace and
            "call PC 0x80029B64 after the second twenty-presentation wait" in trace and
            "PsyQ InterruptCallback(0,NULL) at 0x8009860C through callback slot 0x800C54D0" in trace and
            "removed source handler 0x800A450C" in trace and
            "left that old-handler value live in v0" in trace and
            "vblank-shutdown-before.ppm and vblank-shutdown-after.ppm are pixel-identical" in trace and
            "lack of a critical section" in trace and
            "mutable saved-ra/s8 epilogue remain" in trace and
            "no Windows interrupt or host timing behavior was invented" in trace and
            "next recovered boundary 0x8009167C" in trace and
            "14-instruction game-clock shutdown wrapper" in trace and
            "call PC 0x80029B6C immediately after VBlank shutdown" in trace and
            "PsyQ InterruptCallback(6,NULL) at 0x8009860C" in trace and
            "callback slot 0x800C54E8" in trace and
            "removed source Timer 2 handler 0x800916B4" in trace and
            "clock-shutdown-before.ppm and clock-shutdown-after.ppm are pixel-identical" in trace and
            "no Windows interrupt or host timer behavior was invented" in trace and
            "next recovered boundary 0x8008F19C" in trace and
            "14-instruction controller-suspend wrapper" in trace and
            "only call PC 0x80029B74 immediately after game-clock shutdown" in trace and
            "active flag zero from 0x800C4A70 before allocating its frame" in trace and
            "controller shutdown service 0x80091224 once" in trace and
            "discarded the fixture's unknown v0" in trace and
            "stored suspend flag one" in trace and
            "controller-suspend-before.ppm and controller-suspend-after.ppm are pixel-identical" in trace and
            "non-normalized nonzero fast path" in trace and
            "no Windows keyboard or gamepad behavior was invented" in trace and
            "next recovered boundary 0x800A3A74" in trace and
            "one-instruction zero-fill entry" in trace and
            "call PC 0x80029B84 immediately after controller suspend" in trace and
            "complete 80-instruction optimized fill core at 0x800A3A78" in trace and
            "9 stores and 36 bytes of overlapping SWR/SW/SWL traffic" in trace and
            "32-byte shutdown callback table at 0x800D6DEC" in trace and
            "already zero from the recovered clock initializer" in trace and
            "shutdown-table-zero-before.ppm and shutdown-table-zero-after.ppm are pixel-identical" in trace and
            "store metrics were captured natively by the self-driving recovered-input test, not computer control" in trace and
            "zero-length delay-slot byte write" in trace and
            "INT_MIN huge-loop wrap" in trace and
            "unchanged live v0 remain" in trace and
            "next recovered boundary 0x800AA468" in trace and
            "complete 200-instruction optimized memory-copy helper" in trace and
            "all 5136 retained FELOAD bytes" in trace and
            "1284 reads and 1284 stores" in trace and
            "main read copied entry 0x801E1410" in trace and
            "feload-memory-copy-before.ppm and feload-memory-copy-after.ppm are pixel-identical" in trace and
            "destination bytes changed and match the source" in trace and
            "alignment-bit v0" in trace and
            "negative-length runaway behavior remain" in trace and
            "no court/gameplay frame synthesized" in trace and "TEAM-CAPTURE PASS:" in trace,
            "required visual/diagnostic trace stages are missing")
    print("GAME ENTRY VISUAL PASS: Setup -> Team Select -> User Setup frames; "
          "native 0x800948D0 changed guard 0x800C4B14 from 0 to 1; "
          "native 0x800A4830 saved gp 0x800D79C8 at 0x800D6E2C; "
          "native 0x8008FA6C initialized the 220-descriptor gameplay heap; "
          "native 0x80091C08 published CD root LBA 23 and length 2048; "
          "native 0x800A35D8 selected the cdrom: file prefix without adding a separator; "
          "native 0x80092C7C registered a 707-entry PS1 directory cache at 0x8001000C; "
          "native PsyQ SetIntrMask 0x800985B4 cleared the mapped callback mask before reset; "
          "native PsyQ ResetCallback wrapper 0x800985DC dispatched table slot +0x0C to 0x80098714 "
          "with no direct pixel effect; "
          "native controller-resume 0x8008F1D4 initialized input once, then took its already-active fast path; "
          "native PsyQ ResetGraph 0x80099058 initialized mapped GPU state and retained source quirks "
          "without changing captured pixels; "
          "native PsyQ SetGraphDebug 0x800992C4 disabled mapped diagnostics, returned the prior level, "
          "and retained byte-alias/unguarded-dispatch quirks without changing captured pixels; "
          "native VBlank initializer 0x800A43E8 cleared eight callback slots, installed the source "
          "handler through explicit fixtures, retained its counter-3 failure quirk, and changed no pixels; "
          "native game-clock initializer 0x800914D8 installed IRQ6, configured Timer 2 for 120 Hz, "
          "retained signed division traps, and changed no pixels; "
          "native GTE initializer 0x80056678 enabled CU2 and installed seven retained projection controls "
          "without changing pixels; "
          "native clock-delta sampler 0x800A584C refreshed the zero startup baseline through 0x800A5810, "
          "retained raw 32-bit wraparound, and changed no pixels; "
          "native presentation-wait wrapper 0x80029BDC crossed explicit service 0x800A9CC0 41 times, "
          "acknowledged source VBlank state without host timing, retained its unbounded wait, and changed no pixels; "
          "native video-environment initializer 0x80029F20 configured both original 512x240 PS1 buffer pairs, "
          "retained its asymmetric DRAWENV writes and selector mismatch, and changed no pixels; "
          "native PsyQ MoveImage 0x800997E4 submitted two diagnostic VRAM copies; "
          "native PsyQ DrawSync 0x800994F4 waited for and completed both packets, emitted before/after "
          "PPM proof, and preserved its timeout/dispatch quirks while leaving frontend pixels unchanged; "
          "native PsyQ SetDispMask 0x80099458 emitted active-low GP1(03h) enable through retail "
          "table slot +0x10, captured masked/visible scanout frames, and retained its original quirks; "
          "native 0x800A3E20 installed whole-file CRCF validator 0x800A3D60 at 0x800D7B1C, "
          "captured identical before/after scanout frames, and retained its overwrite/return quirks; "
          "native 0x800A7738 cleared and re-seeded the source frame-rate tracker through 0x800A5810, "
          "captured identical before/after scanout frames, and retained its ordering/return quirks; "
          "native match-session owner 0x8002D8D4 configured both buffer pairs, crossed 23 exact child "
          "boundaries and eleven source VBlanks, retained the retail location/index restore bugs, and "
          "captured identical before/after scanout without fabricating downstream gameplay; "
          "native loading-screen compositor 0x80029E58 loaded zloadscr.psh/LdS1 and used the recovered "
          "image owner to place one generated 512x240 fixture at all three exact VRAM coordinates, with "
          "incremental PPM proof and original null-handling quirks retained; "
          "native resource-load retry wrapper 0x80029BFC retried zloadscr.psh once and feload.bin "
          "twice after known-null attempts, preserved its infinite-retry bug, and emitted "
          "pixel-identical native before/after frames; "
          "native heap payload-size query 0x80090D60 used recovered lookup 0x80090618, returned "
          "the retained FELOAD allocation's 5136-byte requested size, preserved its unchecked-null "
          "low-RAM read, and emitted pixel-identical native before/after frames; "
          "native PsyQ CdSync wrapper 0x8009DBA0 forwarded mode 0 and null result to typed service "
          "0x8009E740, retained its raw return/epilogue behavior, and emitted pixel-identical "
          "native before/after frames; "
          "native PsyQ CdReadyCallback 0x8009DBE0 returned default callback 0x8009D9DC, cleared "
          "slot 0x800C57E4, retained raw exchange semantics, and emitted pixel-identical native "
          "before/after frames; "
          "native PsyQ CdSyncCallback 0x8009DBF8 returned default callback 0x8009DA04, cleared "
          "slot 0x800C57E8, retained raw exchange semantics, and emitted pixel-identical native "
          "before/after frames; "
          "native VBlank shutdown 0x800A44D4 removed handler 0x800A450C through "
          "InterruptCallback(0,NULL), retained live v0/stack semantics, and emitted pixel-identical "
          "native before/after frames; "
          "native game-clock shutdown 0x8009167C removed IRQ6 handler 0x800916B4 through "
          "InterruptCallback(6,NULL), retained live v0/stack semantics, and emitted pixel-identical "
          "native before/after frames; "
          "native controller suspend 0x8008F19C called service 0x80091224 once, changed the retained "
          "PS1 input flag from active to suspended, preserved fast-path/stack quirks, and emitted "
          "pixel-identical native before/after frames without changing host input; "
          "native zero-fill entry 0x800A3A74 fell through its 80-instruction shared core, issued "
          "nine source stores over the 32-byte shutdown table, retained delay-slot/overlap quirks, "
          "and emitted pixel-identical native before/after frames; "
          "native memory-copy 0x800AA468 moved all 5136 FELOAD bytes with 2568 exact accesses, "
          "preserved overlap/alignment/runaway quirks, exposed matching CPU snapshots, and emitted "
          "pixel-identical native before/after frames; "
          "77-call GAMEONLY 0x80029994 diagnostic reached FELOAD transfer")


if __name__ == "__main__":
    main()
