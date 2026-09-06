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
    announcements = [case["first_period_startup"]["announcement"] for case in first_cases]
    for announcement, mode in zip(announcements, (2,1)):
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
