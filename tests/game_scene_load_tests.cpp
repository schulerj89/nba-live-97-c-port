#include "recovered/game_scene_load.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game scene-load check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Stack = 0x807ffe00u;
constexpr std::uint32_t EntrySp = 0x807fff80u;
constexpr std::uint32_t FrameSp = EntrySp - 0x18u;
constexpr std::uint32_t CallerRa = 0x8002da8cu;
constexpr std::uint32_t RelocatedSp = FrameSp - 0x40u;

struct Call {
    Nba97GameSceneLoadEvent event{};
    Nba97GameSceneLoadRegisters registers{};
};

struct Fixture {
    enum Mode { Plain, MutateAll, RefuseFirst, RefuseSecond,
        MalformedMask, MalformedZero, UnknownSpFirst, UnknownSaved,
        MissingRelocatedStack };
    std::array<std::uint8_t, 0x200> stack{};
    std::array<std::uint8_t, 0x200> stack_known{};
    Nba97GameTextRegion region{Stack, stack.data(), stack_known.data(),
        stack.size()};
    std::array<Nba97GameSceneLoadAccess, 2> journal{};
    Nba97GameSceneLoadContext context{};
    Nba97GameSceneLoadProgress progress{};
    std::vector<Call> calls;
    Mode mode = Plain;

    Fixture() {
        stack.fill(0xcd);
        stack_known.fill(1);
        context.memory = {&region, 1};
        context.operation_budget = 10;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {0x11000000u + i, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {CallerRa, 0x0f};
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
    }
    void put(std::uint32_t address, std::uint32_t value,
        std::uint8_t known_mask = 0x0f) {
        const auto offset = address - region.base;
        for (unsigned i = 0; i < 4; ++i) {
            region.data[offset + i] = std::uint8_t(value >> (i * 8u));
            if (region.known)
                region.known[offset + i] =
                    std::uint8_t((known_mask >> i) & 1u);
        }
    }
    std::uint32_t get(std::uint32_t address) const {
        const auto offset = address - region.base;
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(region.data[offset + i]) << (i * 8u);
        return value;
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameSceneLoadEvent* event,
        Nba97GameSceneLoadRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back({*event, *registers});
        const std::size_t call = f.calls.size();
        if ((f.mode == RefuseFirst && call == 1) ||
            (f.mode == RefuseSecond && call == 2))
            return 0;
        if (f.mode == MalformedMask) {
            registers->gpr[NBA97_MATCH_INITIALIZE_T0].known_mask = 0x10;
            return 1;
        }
        if (f.mode == MalformedZero) {
            registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
            return 1;
        }
        if (f.mode == UnknownSpFirst && call == 1) {
            registers->gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 0x07;
            return 1;
        }
        if (f.mode == UnknownSaved && call == 2) {
            f.put(FrameSp + 0x10u, 0x44332211u, 0x05);
            return 1;
        }
        if (f.mode == MissingRelocatedStack && call == 2) {
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
                0x90000000u, 0x0f};
            return 1;
        }
        if (f.mode == MutateAll) {
            if (call == 1) {
                for (unsigned i = 1;
                    i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
                    registers->gpr[i].word = 0x21000000u + i;
                    registers->gpr[i].known_mask =
                        static_cast<std::uint8_t>(i & 0x0f);
                }
                registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
                    RelocatedSp, 0x0f};
            } else {
                for (unsigned i = 1;
                    i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
                    registers->gpr[i].word = 0x31000000u + i;
                    registers->gpr[i].known_mask =
                        static_cast<std::uint8_t>((i + 3u) & 0x0f);
                }
                registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
                    RelocatedSp, 0x0f};
                f.put(RelocatedSp + 0x10u, 0x81234560u);
            }
        } else {
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                call == 1 ? 0x11112222u : 0xcafebabeu,
                static_cast<std::uint8_t>(call == 1 ? 0x0f : 0x07)};
        }
        return 1;
    }
    int run() { return nba97_game_scene_load(&context, &progress); }
};

void ordinary_order_and_nops() {
    Fixture f;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations == 4 && f.progress.accesses == 2 &&
        f.progress.reads == 1 && f.progress.stores == 1 &&
        f.progress.callbacks_completed == 2 &&
        f.progress.access_events == 2);
    check(f.progress.frame_stack_pointer == FrameSp &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
        f.progress.restored_return_address.word == CallerRa &&
        f.progress.restored_return_address.known_mask == 0x0f &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0xcafebabeu &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 0x07);
    check(f.get(FrameSp + 0x10u) == CallerRa && f.calls.size() == 2);
    check(f.calls[0].event.pc == 0x8002db70u &&
        f.calls[0].event.delay_slot_pc == 0x8002db74u &&
        f.calls[0].event.entry == 0x800802acu &&
        f.calls[0].event.kind == NBA97_GAME_SCENE_LOAD_CHILD_800802AC &&
        f.calls[0].event.operation == 2 &&
        !f.calls[0].event.argument_count &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8002db78u);
    check(f.calls[1].event.pc == 0x8002db78u &&
        f.calls[1].event.delay_slot_pc == 0x8002db7cu &&
        f.calls[1].event.entry == 0x80048d5cu &&
        f.calls[1].event.kind == NBA97_GAME_SCENE_LOAD_CHILD_80048D5C &&
        f.calls[1].event.operation == 3 &&
        !f.calls[1].event.argument_count &&
        f.calls[1].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8002db80u);
    check(f.journal[0].kind == NBA97_GAME_SCENE_LOAD_STORE &&
        f.journal[0].pc == 0x8002db6cu &&
        f.journal[0].address == FrameSp + 0x10u &&
        f.journal[0].value == CallerRa && f.journal[0].known_mask == 0x0f &&
        f.journal[0].width == 4 && f.journal[0].operation == 1);
    check(f.journal[1].kind == NBA97_GAME_SCENE_LOAD_READ &&
        f.journal[1].pc == 0x8002db80u &&
        f.journal[1].address == FrameSp + 0x10u &&
        f.journal[1].value == CallerRa && f.journal[1].known_mask == 0x0f &&
        f.journal[1].width == 4 && f.journal[1].operation == 4);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    Fixture no_masks;
    no_masks.region.known = nullptr;
    check(no_masks.run() == NBA97_TEXT_COMPLETE &&
        no_masks.progress.restored_return_address.word == CallerRa);
}

void full_register_forwarding_and_live_stack() {
    Fixture f;
    f.mode = Fixture::MutateAll;
    check(f.run() == NBA97_TEXT_COMPLETE && f.calls.size() == 2);
    check(f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == FrameSp &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8002db78u);
    for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        const auto& observed = f.calls[1].registers.gpr[i];
        if (i == NBA97_MATCH_INITIALIZE_RA) {
            check(observed.word == 0x8002db80u &&
                observed.known_mask == 0x0f);
        } else if (i == NBA97_MATCH_INITIALIZE_SP) {
            check(observed.word == RelocatedSp &&
                observed.known_mask == 0x0f);
        } else {
            check(observed.word == 0x21000000u + i &&
                observed.known_mask == static_cast<std::uint8_t>(i & 0x0f));
        }
    }
    for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        const auto& final = f.progress.registers.gpr[i];
        if (i == NBA97_MATCH_INITIALIZE_SP) {
            check(final.word == RelocatedSp + 0x18u &&
                final.known_mask == 0x0f);
        } else if (i == NBA97_MATCH_INITIALIZE_RA) {
            check(final.word == 0x81234560u && final.known_mask == 0x0f);
        } else {
            check(final.word == 0x31000000u + i &&
                final.known_mask ==
                    static_cast<std::uint8_t>((i + 3u) & 0x0f));
        }
    }
    check(f.progress.restored_return_address.word == 0x81234560u &&
        f.journal[1].address == RelocatedSp + 0x10u &&
        f.get(FrameSp + 0x10u) == CallerRa);
}

void unknownness_and_failures() {
    Fixture unknown_entry_sp;
    unknown_entry_sp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
        .known_mask = 0x07;
    check(unknown_entry_sp.run() == NBA97_TEXT_UNKNOWN &&
        !unknown_entry_sp.progress.operations &&
        unknown_entry_sp.progress.stopped_pc == 0x8002db68u &&
        unknown_entry_sp.calls.empty());

    Fixture unknown_ra;
    unknown_ra.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        CallerRa, 0x05};
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.operations == 4 && unknown_ra.progress.reads == 1 &&
        unknown_ra.progress.callbacks_completed == 2 &&
        unknown_ra.progress.stopped_pc == 0x8002db88u &&
        unknown_ra.progress.restored_return_address.known_mask == 0x05 &&
        unknown_ra.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp);

    Fixture unknown_saved;
    unknown_saved.mode = Fixture::UnknownSaved;
    check(unknown_saved.run() == NBA97_TEXT_UNKNOWN &&
        unknown_saved.progress.stopped_pc == 0x8002db88u &&
        unknown_saved.progress.restored_return_address.word == 0x44332211u &&
        unknown_saved.progress.restored_return_address.known_mask == 0x05);

    Fixture unknown_live_sp;
    unknown_live_sp.mode = Fixture::UnknownSpFirst;
    check(unknown_live_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_live_sp.calls.size() == 2 &&
        unknown_live_sp.progress.operations == 3 &&
        unknown_live_sp.progress.callbacks_completed == 2 &&
        unknown_live_sp.progress.stopped_pc == 0x8002db80u &&
        !unknown_live_sp.progress.reads);

    Fixture no_io;
    no_io.context.io = nullptr;
    check(no_io.run() == NBA97_TEXT_IO_REFUSED &&
        no_io.progress.operations == 2 && no_io.progress.stores == 1 &&
        !no_io.progress.callbacks_completed &&
        no_io.progress.stopped_pc == 0x8002db70u &&
        no_io.progress.stopped_entry == 0x800802acu);
    Fixture refused_first;
    refused_first.mode = Fixture::RefuseFirst;
    check(refused_first.run() == NBA97_TEXT_IO_REFUSED &&
        refused_first.calls.size() == 1 &&
        !refused_first.progress.callbacks_completed);
    Fixture refused_second;
    refused_second.mode = Fixture::RefuseSecond;
    check(refused_second.run() == NBA97_TEXT_IO_REFUSED &&
        refused_second.calls.size() == 2 &&
        refused_second.progress.callbacks_completed == 1 &&
        refused_second.progress.stopped_entry == 0x80048d5cu);
    Fixture malformed_mask;
    malformed_mask.mode = Fixture::MalformedMask;
    check(malformed_mask.run() == NBA97_TEXT_ARGUMENT &&
        malformed_mask.calls.size() == 1 &&
        !malformed_mask.progress.callbacks_completed);
    Fixture malformed_zero;
    malformed_zero.mode = Fixture::MalformedZero;
    check(malformed_zero.run() == NBA97_TEXT_ARGUMENT &&
        malformed_zero.calls.size() == 1 &&
        !malformed_zero.progress.callbacks_completed);
}

void budgets_memory_and_wrap() {
    constexpr std::array<std::uint32_t, 4> pcs = {
        0x8002db6cu, 0x8002db70u, 0x8002db78u, 0x8002db80u};
    for (std::size_t budget = 0; budget < pcs.size(); ++budget) {
        Fixture f;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT &&
            f.progress.operations == budget && !f.progress.completed &&
            f.progress.stopped_pc == pcs[budget]);
        check(f.calls.size() == (budget < 2 ? 0u : budget - 1u));
        check(f.get(FrameSp + 0x10u) ==
            (budget ? CallerRa : 0xcdcdcdcdu));
        if (budget == 1)
            check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
                0x8002db78u);
        if (budget >= 2)
            check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
                0x8002db80u);
    }
    Fixture exact;
    exact.context.operation_budget = 4;
    check(exact.run() == NBA97_TEXT_COMPLETE && exact.progress.operations == 4);

    Fixture unaligned;
    unaligned.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word++;
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x8002db6cu);
    Fixture missing;
    missing.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word = 0x90000020u;
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc == 0x8002db6cu);
    Fixture missing_relocated;
    missing_relocated.mode = Fixture::MissingRelocatedStack;
    check(missing_relocated.run() == NBA97_TEXT_RESOURCE &&
        missing_relocated.progress.stopped_pc == 0x8002db80u &&
        missing_relocated.progress.callbacks_completed == 2);
    Fixture malformed_memory;
    malformed_memory.stack_known[FrameSp + 0x10u - Stack] = 2;
    check(malformed_memory.run() == NBA97_TEXT_ARGUMENT &&
        malformed_memory.progress.stopped_pc == 0x8002db6cu);
    Fixture no_masks_unknown_ra;
    no_masks_unknown_ra.region.known = nullptr;
    no_masks_unknown_ra.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = 0x07;
    check(no_masks_unknown_ra.run() == NBA97_TEXT_ARGUMENT &&
        no_masks_unknown_ra.progress.operations == 1 &&
        !no_masks_unknown_ra.progress.stores);

    Fixture overlap;
    Nba97GameTextRegion duplicate[2] = {overlap.region, overlap.region};
    overlap.context.memory = {duplicate, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture empty;
    empty.region.size = 0;
    check(empty.run() == NBA97_TEXT_ARGUMENT);
    Fixture null_data;
    null_data.region.data = nullptr;
    check(null_data.run() == NBA97_TEXT_ARGUMENT);
    Fixture null_regions;
    null_regions.context.memory = {nullptr, 1};
    check(null_regions.run() == NBA97_TEXT_ARGUMENT);
    Fixture wrapped_region;
    wrapped_region.region.base = 0xfffffffcu;
    wrapped_region.region.size = 8;
    check(wrapped_region.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_journal;
    bad_journal.context.access_journal = nullptr;
    bad_journal.context.access_journal_capacity = 1;
    check(bad_journal.run() == NBA97_TEXT_ARGUMENT);
    Fixture malformed_input;
    malformed_input.context.registers.gpr[NBA97_MATCH_INITIALIZE_T0]
        .known_mask = 0x10;
    check(malformed_input.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_zero;
    bad_zero.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask = 0;
    check(bad_zero.run() == NBA97_TEXT_ARGUMENT);
    Nba97GameSceneLoadProgress progress{};
    check(nba97_game_scene_load(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_scene_load(&f.context, nullptr) == NBA97_TEXT_ARGUMENT);

    std::array<std::uint8_t, 0x20> bytes{};
    std::array<std::uint8_t, 0x20> known{};
    known.fill(1);
    Nba97GameTextRegion region{0, bytes.data(), known.data(), bytes.size()};
    Nba97GameSceneLoadContext wrap{};
    wrap.memory = {&region, 1};
    wrap.operation_budget = 4;
    wrap.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
    wrap.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x10u, 0x0f};
    wrap.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x87654320u, 0x0f};
    wrap.io = [](void*, const Nba97GameTextMemory*,
        const Nba97GameSceneLoadEvent*, Nba97GameSceneLoadRegisters*) {
        return 1;
    };
    Nba97GameSceneLoadProgress wrapped{};
    check(nba97_game_scene_load(&wrap, &wrapped) == NBA97_TEXT_COMPLETE &&
        wrapped.frame_stack_pointer == 0xfffffff8u &&
        wrapped.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 0x10u &&
        wrapped.restored_return_address.word == 0x87654320u);
}
}

int main() {
    ordinary_order_and_nops();
    full_register_forwarding_and_live_stack();
    unknownness_and_failures();
    budgets_memory_and_wrap();
    std::printf("game_scene_load: %u checks passed\n", checks);
}
