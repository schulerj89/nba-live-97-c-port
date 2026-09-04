#include "recovered/game_move_image.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game MoveImage check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807fff00u;
constexpr std::uint32_t EntrySp = 0x807fffd0u;
constexpr std::uint32_t FrameSp = EntrySp - 0x20u;
constexpr std::uint32_t Rect = 0x80024000u;
constexpr std::uint32_t Packet = 0x800c5668u;
constexpr std::uint32_t DriverGlobal = 0x800c55b8u;
constexpr std::uint32_t DriverTable = 0x800c5578u;
constexpr std::uint32_t DispatchContext = 0x8009b1f8u;
constexpr std::uint32_t Dispatch = 0x8009b298u;

struct Fixture {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x200000, 0xcd);
    std::vector<std::uint8_t> ram_known =
        std::vector<std::uint8_t>(0x200000, 1);
    std::array<std::uint8_t, 0x100> stack{};
    std::array<std::uint8_t, 0x100> stack_known{};
    Nba97GameTextRegion regions[2] = {
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}
    };
    Nba97GameMoveImageContext context{{regions, 2}, 100, Rect, 0, 0,
        EntrySp, 0x80029a9cu,
        {0xa0a0a0a0u, 0xb1b1b1b1u, 0xc2c2c2c2u},
        0x800d79c8u, io, this};
    Nba97GameMoveImageProgress progress{};
    std::vector<Nba97GameMoveImageEvent> calls;
    std::array<std::uint32_t, 5> dispatched_packet{};
    std::size_t refuse_call = std::numeric_limits<std::size_t>::max();
    std::uint32_t dispatch_return = 0x12345678u;
    std::uint8_t dispatch_return_known = 1;
    bool rewrite_rect_on_diagnostic = false;
    bool zero_height_on_diagnostic = false;
    bool rewrite_stack_on_dispatch = false;
    bool poison_stack_on_dispatch = false;

    Fixture() {
        stack.fill(0xcd);
        stack_known.fill(1);
        put(Rect, 0x00000200u);
        put(Rect + 4u, 0x01000200u);
        put(DriverGlobal, DriverTable);
        put(DriverTable + 8u, Dispatch);
        put(DriverTable + 0x18u, DispatchContext);
        /* These are the retail packet-header words. MoveImage must retain
           them rather than rebuilding or sanitizing the packet. */
        put(Packet, 0x04ffffffu);
        put(Packet + 4u, 0x80000000u);
        put(Packet + 8u, 0x11111111u);
        put(Packet + 12u, 0x22222222u);
        put(Packet + 16u, 0x33333333u);
    }

    std::uint8_t* byte(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base &&
                std::uint64_t(address - region.base) < region.size)
                return region.data + (address - region.base);
        check(false);
        return nullptr;
    }
    std::uint8_t* known(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base &&
                std::uint64_t(address - region.base) < region.size)
                return region.known ? region.known + (address - region.base) : nullptr;
        check(false);
        return nullptr;
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width = 4,
        std::uint8_t value_known = 1) {
        for (unsigned i = 0; i < width; ++i) {
            *byte(address + i) = static_cast<std::uint8_t>(value >> (8u * i));
            if (auto* mask = known(address + i))
                *mask = value_known;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width = 4) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(*byte(address + i)) << (8u * i);
        return value;
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameMoveImageEvent* event, Nba97GameMoveImageValue* value) {
        auto& f = *static_cast<Fixture*>(user);
        const auto call = f.calls.size();
        f.calls.push_back(*event);
        if (call == f.refuse_call)
            return 0;
        if (event->kind == NBA97_GAME_MOVE_IMAGE_DIAGNOSTIC) {
            if (f.rewrite_rect_on_diagnostic) {
                f.put(f.context.rectangle_address, 0x00400020u);
                f.put(f.context.rectangle_address + 4u, 0x00080010u);
            }
            if (f.zero_height_on_diagnostic)
                f.put(f.context.rectangle_address + 6u, 0, 2);
            /* Deliberately malformed/unknown-looking v0 is ignored by source. */
            *value = {0xfeedfaceu, 7};
            return 1;
        }
        for (unsigned i = 0; i < f.dispatched_packet.size(); ++i)
            f.dispatched_packet[i] = f.get(Packet + 4u * i);
        if (f.rewrite_stack_on_dispatch) {
            f.put(FrameSp + 0x1cu, 0x2468ace0u);
            f.put(FrameSp + 0x18u, 0xcccc2222u);
            f.put(FrameSp + 0x14u, 0xbbbb1111u);
            f.put(FrameSp + 0x10u, 0xaaaa0000u);
        }
        if (f.poison_stack_on_dispatch)
            *f.known(FrameSp + 0x1cu) = 0;
        *value = {f.dispatch_return, f.dispatch_return_known};
        return 1;
    }
    int run() { return nba97_game_move_image(&context, &progress); }
};

void startup_moves() {
    Fixture top;
    check(top.run() == NBA97_TEXT_COMPLETE && top.progress.completed &&
        top.progress.diagnostic_called && top.progress.gpu_dispatched);
    check(top.progress.operations == 20 && top.progress.accesses == 18 &&
        top.progress.reads == 11 && top.progress.stores == 7 &&
        top.progress.callbacks_completed == 2);
    check(top.progress.frame_stack_pointer == FrameSp &&
        top.progress.stack_pointer == EntrySp &&
        top.progress.global_pointer == 0x800d79c8u &&
        top.progress.rectangle_address == Rect &&
        top.progress.signed_width == 512 && top.progress.signed_height == 256 &&
        top.progress.width_read && top.progress.height_read &&
        !top.progress.zero_extent_return);
    check(top.progress.source_coordinate_word == 0x00000200u &&
        top.progress.destination_coordinate_word == 0 &&
        top.progress.extent_word == 0x01000200u &&
        top.progress.driver_table == DriverTable &&
        top.progress.dispatch_context == DispatchContext &&
        top.progress.dispatch_entry == Dispatch &&
        top.progress.return_v0 == 0x12345678u &&
        top.progress.return_v0_known);
    check(top.progress.restored_return_address == 0x80029a9cu &&
        top.progress.restored_saved_register[0] == 0xa0a0a0a0u &&
        top.progress.restored_saved_register[1] == 0xb1b1b1b1u &&
        top.progress.restored_saved_register[2] == 0xc2c2c2c2u);
    check(top.calls.size() == 2 &&
        top.calls[0].kind == NBA97_GAME_MOVE_IMAGE_DIAGNOSTIC &&
        top.calls[0].pc == 0x8009980cu &&
        top.calls[0].entry == 0x80099560u &&
        top.calls[0].argument_count == 2 &&
        top.calls[0].argument[0] == 0x8002831cu &&
        top.calls[0].argument[1] == Rect &&
        top.calls[0].stack_pointer == FrameSp &&
        top.calls[0].return_address == 0x80099814u);
    check(top.calls[1].kind == NBA97_GAME_MOVE_IMAGE_GPU_DISPATCH &&
        top.calls[1].pc == 0x80099884u &&
        top.calls[1].entry == Dispatch && top.calls[1].argument_count == 4 &&
        top.calls[1].argument[0] == DispatchContext &&
        top.calls[1].argument[1] == Packet &&
        top.calls[1].argument[2] == 0x14u &&
        top.calls[1].argument[3] == 0 &&
        top.calls[1].return_address == 0x8009988cu);
    check(top.calls[0].saved_register[0] == Rect &&
        top.calls[0].saved_register[1] == 0 &&
        top.calls[0].saved_register[2] == 0 &&
        top.calls[1].saved_register[0] == Rect &&
        top.dispatched_packet == std::array<std::uint32_t, 5>{
            0x04ffffffu, 0x80000000u, 0x00000200u, 0, 0x01000200u});

    Fixture bottom;
    bottom.context.destination_y = 0x100u;
    bottom.context.return_address = 0x80029aacu;
    check(bottom.run() == NBA97_TEXT_COMPLETE &&
        bottom.progress.destination_coordinate_word == 0x01000000u &&
        bottom.get(Packet + 12u) == 0x01000000u &&
        bottom.progress.restored_return_address == 0x80029aacu);

    Fixture no_masks;
    no_masks.regions[0].known = nullptr;
    no_masks.regions[1].known = nullptr;
    check(no_masks.run() == NBA97_TEXT_COMPLETE && no_masks.progress.completed &&
        no_masks.progress.return_v0_known);
}

void source_quirks_and_live_state() {
    Fixture zero_width;
    zero_width.put(Rect + 4u, 0x01000000u);
    check(zero_width.run() == NBA97_TEXT_COMPLETE &&
        zero_width.progress.zero_extent_return &&
        zero_width.progress.width_read && !zero_width.progress.height_read &&
        zero_width.progress.return_v0 == UINT32_MAX &&
        zero_width.progress.return_v0_known &&
        zero_width.progress.operations == 10 &&
        zero_width.progress.accesses == 9 && zero_width.calls.size() == 1 &&
        zero_width.get(Packet + 8u) == 0x11111111u);

    Fixture zero_height;
    zero_height.put(Rect + 4u, 0x00000200u);
    check(zero_height.run() == NBA97_TEXT_COMPLETE &&
        zero_height.progress.zero_extent_return &&
        zero_height.progress.width_read && zero_height.progress.height_read &&
        zero_height.progress.operations == 11 &&
        zero_height.progress.accesses == 10 && zero_height.calls.size() == 1);

    /* The source tests only equality with zero. Negative signed dimensions
       still construct and dispatch a GPU packet. */
    Fixture negative;
    negative.put(Rect + 4u, 0xffff8000u);
    check(negative.run() == NBA97_TEXT_COMPLETE && negative.calls.size() == 2 &&
        negative.progress.signed_width == -32768 &&
        negative.progress.signed_height == -1 &&
        negative.progress.extent_word == 0xffff8000u);

    Fixture truncation;
    truncation.context.destination_x = 0x1234abcdu;
    truncation.context.destination_y = 0xfedc9876u;
    check(truncation.run() == NBA97_TEXT_COMPLETE &&
        truncation.progress.destination_coordinate_word == 0x9876abcdu &&
        truncation.get(Packet + 12u) == 0x9876abcdu &&
        truncation.get(Packet) == 0x04ffffffu &&
        truncation.get(Packet + 4u) == 0x80000000u);

    /* The unconditional diagnostic can change every later live input. */
    Fixture rewritten;
    rewritten.rewrite_rect_on_diagnostic = true;
    check(rewritten.run() == NBA97_TEXT_COMPLETE &&
        rewritten.progress.source_coordinate_word == 0x00400020u &&
        rewritten.progress.extent_word == 0x00080010u &&
        rewritten.progress.signed_width == 16 &&
        rewritten.progress.signed_height == 8);

    Fixture changed_to_zero;
    changed_to_zero.zero_height_on_diagnostic = true;
    check(changed_to_zero.run() == NBA97_TEXT_COMPLETE &&
        changed_to_zero.progress.zero_extent_return &&
        !changed_to_zero.progress.gpu_dispatched &&
        changed_to_zero.calls.size() == 1);

    Fixture live_stack;
    live_stack.rewrite_stack_on_dispatch = true;
    check(live_stack.run() == NBA97_TEXT_COMPLETE &&
        live_stack.progress.restored_return_address == 0x2468ace0u &&
        live_stack.progress.restored_saved_register[0] == 0xaaaa0000u &&
        live_stack.progress.restored_saved_register[1] == 0xbbbb1111u &&
        live_stack.progress.restored_saved_register[2] == 0xcccc2222u);

    /* jalr has no source null check. Zero remains an offered boundary. */
    Fixture null_target;
    null_target.put(DriverTable + 8u, 0);
    check(null_target.run() == NBA97_TEXT_COMPLETE &&
        null_target.calls.size() == 2 && null_target.calls[1].entry == 0 &&
        null_target.progress.dispatch_entry == 0);

    Fixture unknown_sdk_result;
    unknown_sdk_result.dispatch_return_known = 0;
    check(unknown_sdk_result.run() == NBA97_TEXT_COMPLETE &&
        !unknown_sdk_result.progress.return_v0_known &&
        unknown_sdk_result.progress.return_v0 == 0x12345678u);
}

void limits_and_failures() {
    constexpr std::uint32_t pcs[20] = {
        0x800997e8u, 0x800997f0u, 0x800997f8u, 0x80099808u,
        0x8009980cu, 0x80099814u, 0x80099824u, 0x8009984cu,
        0x80099854u, 0x80099860u, 0x80099864u, 0x80099868u,
        0x80099874u, 0x80099878u, 0x8009987cu, 0x80099884u,
        0x8009988cu, 0x80099890u, 0x80099894u, 0x80099898u};
    for (std::size_t budget = 0; budget < 20; ++budget) {
        Fixture f;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget &&
            f.progress.stopped_pc == pcs[budget] && !f.progress.completed);
    }

    Fixture no_io;
    no_io.context.io = nullptr;
    check(no_io.run() == NBA97_TEXT_IO_REFUSED &&
        no_io.progress.stopped_pc == 0x8009980cu &&
        no_io.progress.stopped_entry == 0x80099560u && no_io.calls.empty());

    Fixture refuse_debug;
    refuse_debug.refuse_call = 0;
    check(refuse_debug.run() == NBA97_TEXT_IO_REFUSED &&
        refuse_debug.calls.size() == 1 && !refuse_debug.progress.callbacks_completed);

    Fixture refuse_gpu;
    refuse_gpu.refuse_call = 1;
    check(refuse_gpu.run() == NBA97_TEXT_IO_REFUSED &&
        refuse_gpu.calls.size() == 2 && refuse_gpu.progress.callbacks_completed == 1 &&
        refuse_gpu.get(Packet + 8u) == 0x00000200u &&
        refuse_gpu.get(Packet + 16u) == 0x01000200u);

    Fixture malformed_return;
    malformed_return.dispatch_return_known = 2;
    check(malformed_return.run() == NBA97_TEXT_ARGUMENT &&
        malformed_return.calls.size() == 2 &&
        malformed_return.progress.callbacks_completed == 1 &&
        malformed_return.progress.stopped_pc == 0x80099884u);

    Fixture unaligned_target;
    unaligned_target.put(DriverTable + 8u, Dispatch + 2u);
    check(unaligned_target.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_target.calls.size() == 1 &&
        unaligned_target.progress.stopped_pc == 0x80099884u &&
        unaligned_target.progress.stopped_entry == Dispatch + 2u &&
        unaligned_target.get(Packet + 8u) == 0x00000200u);

    Fixture unknown_width;
    *unknown_width.known(Rect + 4u) = 0;
    check(unknown_width.run() == NBA97_TEXT_UNKNOWN &&
        unknown_width.progress.stopped_pc == 0x80099814u &&
        unknown_width.calls.size() == 1);

    Fixture unknown_driver;
    *unknown_driver.known(DriverGlobal) = 0;
    check(unknown_driver.run() == NBA97_TEXT_UNKNOWN &&
        unknown_driver.progress.stopped_pc == 0x80099854u &&
        unknown_driver.progress.stopped_address == DriverGlobal);

    Fixture odd_rect;
    odd_rect.context.rectangle_address = Rect + 1u;
    check(odd_rect.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        odd_rect.progress.stopped_pc == 0x80099814u && odd_rect.calls.size() == 1);

    Fixture half_aligned_rect;
    half_aligned_rect.context.rectangle_address = Rect + 2u;
    half_aligned_rect.put(Rect + 6u, 1, 2);
    half_aligned_rect.put(Rect + 8u, 1, 2);
    check(half_aligned_rect.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        half_aligned_rect.progress.stopped_pc == 0x8009984cu);

    Fixture bad_stack;
    bad_stack.context.stack_pointer = EntrySp + 1u;
    check(bad_stack.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        bad_stack.progress.stopped_pc == 0x800997e8u);

    Fixture malformed_packet;
    *malformed_packet.known(Packet + 12u) = 2;
    check(malformed_packet.run() == NBA97_TEXT_ARGUMENT &&
        malformed_packet.progress.stopped_pc == 0x80099860u);

    Fixture poisoned_stack;
    poisoned_stack.poison_stack_on_dispatch = true;
    check(poisoned_stack.run() == NBA97_TEXT_UNKNOWN &&
        poisoned_stack.progress.stopped_pc == 0x8009988cu &&
        poisoned_stack.progress.callbacks_completed == 2);

    Fixture missing_ram;
    missing_ram.context.memory = {&missing_ram.regions[1], 1};
    check(missing_ram.run() == NBA97_TEXT_RESOURCE &&
        missing_ram.progress.stopped_pc == 0x80099814u);

    Fixture overlap;
    Nba97GameTextRegion duplicate[2] = {overlap.regions[0], overlap.regions[0]};
    overlap.context.memory = {duplicate, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT && !overlap.progress.operations);

    Fixture empty;
    empty.regions[0].size = 0;
    check(empty.run() == NBA97_TEXT_ARGUMENT && !empty.progress.operations);
    Fixture null_data;
    null_data.regions[0].data = nullptr;
    check(null_data.run() == NBA97_TEXT_ARGUMENT && !null_data.progress.operations);
    Fixture null_regions;
    null_regions.context.memory = {nullptr, 1};
    check(null_regions.run() == NBA97_TEXT_ARGUMENT && !null_regions.progress.operations);
    Nba97GameMoveImageProgress progress{};
    check(nba97_game_move_image(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
    Fixture valid;
    check(nba97_game_move_image(&valid.context, nullptr) == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    startup_moves();
    source_quirks_and_live_state();
    limits_and_failures();
    std::printf("game_move_image: %u checks passed\n", checks);
}
