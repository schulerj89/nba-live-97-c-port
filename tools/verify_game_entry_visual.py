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
    require(len(states) == 98, "native click-through frame count drifted")
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
                "vblank_signals": 41, "final_frame_counter": 41,
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
                "buffer_selector": "0x8001EDE8", "buffer_selector_value": 0,
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
            "98 captured frontend frames were unchanged" in trace and
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
            "does not submit a GPU packet or change any of the 98 captured frontend frames" in trace and
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
            "frame counter 0x800D7A88, ending at 41" in trace and
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
            "does not draw, so none of the 98 natively captured frontend frames changed" in trace and
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
          "77-call GAMEONLY 0x80029994 diagnostic reached 0x8002D8D4 and FELOAD transfer")


if __name__ == "__main__":
    main()
