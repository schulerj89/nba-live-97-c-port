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
                    "video_halfword_0x80021498": {"before": 0, "after": 0},
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
                    "initialize_0x8002DB90": "acknowledged-boundary",
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
                "entry_word_after": "0x801E0100",
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
                       "indirect_entry": "0x801E0100"},
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
            "does not draw, so none of the 98 natively captured frontend frames changed" in trace and
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
            "initialize 0x8002DB90, scene load 0x8002DB68, game loop 0x8002DC38 and teardown 0x8002DC58" in trace and
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
            "main read copied entry 0x801E0100" in trace and
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
