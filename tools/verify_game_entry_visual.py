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


def ppm_hash(path):
    data = path.read_bytes()
    parts = data.split(b"\n", 3)
    require(parts[:3] == [b"P6", b"512 240", b"255"], f"bad PPM header: {path}")
    require(len(parts) == 4 and len(parts[3]) == 512 * 240 * 3, f"bad PPM extent: {path}")
    return hashlib.sha256(parts[3]).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--frames", type=Path, required=True)
    parser.add_argument("--trace", type=Path, required=True)
    args = parser.parse_args()

    states = read_json(args.frames / "states.json")
    by_id = {state["id"]: state for state in states}
    require(len(by_id) == len(states), "duplicate captured frame id")
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

    receipt = read_json(args.frames / "game_entry_trace.json")
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
    result = receipt["result"]
    require(result == {"status": "transferred", "callbacks": 77, "stores": 15,
                       "reads": 1, "match_orchestration": "0x8002D8D4",
                       "loaded_image": "0x80123400", "loaded_size": 5136,
                       "indirect_entry": "0x801E0100"},
            "translated game-entry result drifted")
    calls = receipt["calls"]
    require(len(calls) == 77 and [call["index"] for call in calls] == list(range(77)),
            "runtime call extent/order drifted")
    require(calls[0]["pc"] == "0x800299A4" and calls[0]["entry"] == "0x800948D0",
            "first initialization boundary drifted")
    require(calls[1]["pc"] == "0x800299AC" and calls[1]["entry"] == "0x800A4830",
            "global-pointer save boundary drifted")
    require(calls[24]["pc"] == "0x80029ADC" and calls[24]["entry"] == "0x8002D8D4",
            "match orchestration boundary drifted")
    require(calls[26]["entry"] == "0x80029BFC" and calls[27]["entry"] == "0x80090D60",
            "FELOAD load/size boundaries drifted")
    require([call["s0"] for call in calls[28:48]] ==
            [f"0x{value:08X}" for value in range(1, 21)] and
            [call["s0"] for call in calls[51:71]] ==
            [f"0x{value:08X}" for value in range(1, 21)],
            "delay-slot loop register order drifted")
    require(calls[75]["entry"] == "0x800AA468" and
            calls[76] == {"index": 76, "kind": "indirect", "pc": "0x80029BA8",
                          "entry": "0x801E0100", "s0": "0x00000014"},
            "loaded image copy/transfer boundary drifted")

    trace = args.trace.read_text(encoding="utf-8-sig")
    require("MATCH-HANDOFF-PENDING" in trace and "GAME-ENTRY-DIAG" in trace and
            "native recovered-input click-through" in trace and
            "0x800948D0 executed recovered owner" in trace and
            "guard 0x800C4B14 changed 0->1" in trace and
            "0x800A4830 executed recovered owner" in trace and
            "saved gp 0x800D79C8 to 0x800D6E2C" in trace and
            "no court/gameplay frame synthesized" in trace and "TEAM-CAPTURE PASS:" in trace,
            "required visual/diagnostic trace stages are missing")
    print("GAME ENTRY VISUAL PASS: Setup -> Team Select -> User Setup frames; "
          "native 0x800948D0 changed guard 0x800C4B14 from 0 to 1; "
          "native 0x800A4830 saved gp 0x800D79C8 at 0x800D6E2C; "
          "77-call GAMEONLY 0x80029994 diagnostic reached 0x8002D8D4 and FELOAD transfer")


if __name__ == "__main__":
    main()
