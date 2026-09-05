#include "recovered/game_scene_random_warmup.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "game scene random warm-up check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Stack = 0x807ffc00u;
constexpr std::uint32_t EntrySp = 0x807fff00u;
constexpr std::uint32_t FrameSp = EntrySp - 0x18u;
constexpr std::uint32_t CallerRa = 0x8002db78u;
constexpr std::uint32_t EntryS0 = 0x11223344u;
constexpr std::uint32_t RelocatedSp = FrameSp - 0x80u;

struct Call {
    Nba97GameSceneRandomWarmupEvent event{};
    Nba97GameSceneRandomWarmupRegisters registers{};
};

struct Fixture {
    std::array<std::uint8_t, 0x400> stack{};
    std::array<std::uint8_t, 0x400> stack_known{};
    Nba97GameTextRegion region{Stack, stack.data(), stack_known.data(),
        stack.size()};
    std::array<Nba97GameSceneRandomWarmupAccess, 4> journal{};
    Nba97GameSceneRandomWarmupContext context{};
    Nba97GameSceneRandomWarmupProgress progress{};
    std::vector<Call> calls;
    std::uint32_t first_random = 0x80000100u;
    std::uint8_t first_random_known = 0x0f;
    std::uint32_t second_random = 0xdeadbeefu;
    std::uint8_t second_random_known = 0x0f;
    std::uint32_t seed_return = 0x55667788u;
    std::uint8_t seed_return_known = 0x0f;
    std::size_t refuse_call = static_cast<std::size_t>(-1);
    std::size_t malformed_call = static_cast<std::size_t>(-1);
    bool malformed_zero = false;
    bool seed_zero_skip = false;
    bool seed_two_steps = false;
    bool seed_unknown_s0 = false;
    bool seed_partial_nonzero_s0 = false;
    bool keep_loop_running = false;
    bool stop_after_first_step = false;
    bool relocate_stack = false;
    bool unknown_live_sp = false;
    bool unaligned_live_sp = false;
    bool missing_live_sp = false;
    bool rewrite_saved_words = false;
    bool mutate_all = false;
    bool step_unknown_s0 = false;

    Fixture() {
        stack.fill(0xcd);
        stack_known.fill(1);
        context.memory = {&region, 1};
        context.operation_budget = 300;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {0x41000000u + i, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {CallerRa, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {EntryS0, 0x0f};
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
        const Nba97GameSceneRandomWarmupEvent* event,
        Nba97GameSceneRandomWarmupRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        const std::size_t call = f.calls.size();
        f.calls.push_back({*event, *registers});
        if (call == f.refuse_call)
            return 0;
        if (call == f.malformed_call) {
            if (f.malformed_zero)
                registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
            else
                registers->gpr[NBA97_MATCH_INITIALIZE_T9].known_mask = 0x10;
            return 1;
        }

        if (f.mutate_all) {
            for (unsigned i = 1;
                i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
                registers->gpr[i] = {
                    0x70000000u + static_cast<std::uint32_t>(call * 0x100u) + i,
                    static_cast<std::uint8_t>((call + i) & 0x0fu)};
            }
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
                FrameSp, 0x0f};
        }

        if (event->kind == NBA97_GAME_SCENE_RANDOM_WARMUP_STARTUP_800800F8) {
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x10203040u, 0x05};
        } else if (event->kind ==
            NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70) {
            if (event->invocation == 1)
                registers->gpr[NBA97_MATCH_INITIALIZE_V0] =
                    {f.first_random, f.first_random_known};
            else
                registers->gpr[NBA97_MATCH_INITIALIZE_V0] =
                    {f.second_random, f.second_random_known};
        } else if (event->kind ==
            NBA97_GAME_SCENE_RANDOM_WARMUP_SEED_80093694) {
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] =
                {f.seed_return, f.seed_return_known};
            if (f.seed_zero_skip)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0, 0x0f};
            if (f.seed_two_steps)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {2, 0x0f};
            if (f.seed_unknown_s0)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0, 0x0e};
            if (f.seed_partial_nonzero_s0)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0x100u, 0x02};
            if (f.unknown_live_sp)
                registers->gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 0x07;
            if (f.unaligned_live_sp)
                registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
                    FrameSp + 2u, 0x0f};
            if (f.missing_live_sp)
                registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
                    0x90000000u, 0x0f};
            if (f.relocate_stack) {
                registers->gpr[NBA97_MATCH_INITIALIZE_SP] =
                    {RelocatedSp, 0x0f};
                f.put(RelocatedSp + 0x14u, 0x81234560u);
                f.put(RelocatedSp + 0x10u, 0xaabbccddu, 0x05);
            }
            if (f.rewrite_saved_words) {
                f.put(FrameSp + 0x14u, 0x82345670u, 0x03);
                f.put(FrameSp + 0x10u, 0x01020304u, 0x06);
            }
        } else {
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                0x90000000u + static_cast<std::uint32_t>(event->invocation),
                0x0f};
            if (f.keep_loop_running)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {1, 0x0f};
            if (f.stop_after_first_step && event->invocation == 1)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0, 0x0f};
            if (f.seed_partial_nonzero_s0)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0, 0x0f};
            if (f.step_unknown_s0)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0, 0x0e};
        }
        return 1;
    }

    int run() {
        return nba97_game_scene_random_warmup(&context, &progress);
    }
};

void exact_order_delays_and_registers() {
    Fixture f;
    f.first_random = 0xffffffffu;
    f.second_random = 0x8765abcdu;
    f.seed_two_steps = true;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.calls.size() == 6 && f.progress.callbacks_completed == 6 &&
        f.progress.startup_calls == 1 && f.progress.random_calls == 2 &&
        f.progress.seed_calls == 1 && f.progress.step_calls == 2);
    check(f.progress.operations == 10 && f.progress.accesses == 4 &&
        f.progress.stores == 2 && f.progress.reads == 2 &&
        f.progress.access_events == 4);

    static constexpr std::uint32_t pc[6] = {
        0x800802b4u, 0x800802bcu, 0x800802c8u,
        0x800802d0u, 0x800802e0u, 0x800802e0u};
    static constexpr std::uint32_t entry[6] = {
        0x800800f8u, 0x8002ab70u, 0x8002ab70u,
        0x80093694u, 0x800935c4u, 0x800935c4u};
    static constexpr unsigned kind[6] = {
        NBA97_GAME_SCENE_RANDOM_WARMUP_STARTUP_800800F8,
        NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70,
        NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70,
        NBA97_GAME_SCENE_RANDOM_WARMUP_SEED_80093694,
        NBA97_GAME_SCENE_RANDOM_WARMUP_STEP_800935C4,
        NBA97_GAME_SCENE_RANDOM_WARMUP_STEP_800935C4};
    static constexpr std::size_t invocation[6] = {1, 1, 2, 1, 1, 2};
    for (unsigned i = 0; i < 6; ++i)
        check(f.calls[i].event.pc == pc[i] &&
            f.calls[i].event.delay_slot_pc == pc[i] + 4u &&
            f.calls[i].event.entry == entry[i] &&
            f.calls[i].event.kind == kind[i] &&
            f.calls[i].event.invocation == invocation[i] &&
            f.calls[i].event.operation == i + 3u &&
            f.calls[i].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
                pc[i] + 8u);
    check(f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_S0].word == EntryS0 &&
        f.get(FrameSp + 0x14u) == CallerRa &&
        f.get(FrameSp + 0x10u) == EntryS0);
    check(f.calls[2].registers.gpr[NBA97_MATCH_INITIALIZE_S0].word == 191 &&
        f.calls[2].registers.gpr[NBA97_MATCH_INITIALIZE_S0].known_mask == 0x0f);
    check(f.calls[3].event.argument_count == 1 &&
        f.calls[3].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0xabcdu &&
        f.calls[3].registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask == 0x0f);
    check(!f.calls[0].event.argument_count && !f.calls[1].event.argument_count &&
        !f.calls[2].event.argument_count && !f.calls[4].event.argument_count);
    check(f.calls[4].registers.gpr[NBA97_MATCH_INITIALIZE_S0].word == 1 &&
        f.calls[5].registers.gpr[NBA97_MATCH_INITIALIZE_S0].word == 0);
    check(f.progress.warmup_count.word == 191 &&
        f.progress.seed_argument.word == 0xabcdu &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0x90000002u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == CallerRa &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word == EntryS0);
    check(!f.progress.stopped_pc && !f.progress.stopped_address &&
        !f.progress.stopped_entry);

    static constexpr std::uint32_t access_pc[4] = {
        0x800802b0u, 0x800802b8u, 0x800802f0u, 0x800802f4u};
    static constexpr std::uint32_t access_address[4] = {
        FrameSp + 0x14u, FrameSp + 0x10u,
        FrameSp + 0x14u, FrameSp + 0x10u};
    static constexpr std::size_t operation[4] = {1, 2, 9, 10};
    for (unsigned i = 0; i < 4; ++i)
        check(f.journal[i].pc == access_pc[i] &&
            f.journal[i].address == access_address[i] &&
            f.journal[i].operation == operation[i] &&
            f.journal[i].width == 4 &&
            f.journal[i].kind == (i < 2 ?
                NBA97_GAME_SCENE_RANDOM_WARMUP_STORE :
                NBA97_GAME_SCENE_RANDOM_WARMUP_READ));
}

void all_gpr_forwarding_and_overwrites() {
    Fixture f;
    f.mutate_all = true;
    f.seed_zero_skip = true;
    f.first_random = 0xfedcba55u;
    f.second_random = 0x87654321u;
    check(f.run() == NBA97_TEXT_COMPLETE && f.calls.size() == 4);

    for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        const auto& second = f.calls[1].registers.gpr[i];
        if (i == NBA97_MATCH_INITIALIZE_RA)
            check(second.word == 0x800802c4u && second.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_SP)
            check(second.word == FrameSp && second.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_V0)
            check(second.word == 0x10203040u && second.known_mask == 0x05);
        else
            check(second.word == 0x70000000u + i &&
                second.known_mask == static_cast<std::uint8_t>(i & 0x0f));
    }
    for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        const auto& third = f.calls[2].registers.gpr[i];
        if (i == NBA97_MATCH_INITIALIZE_RA)
            check(third.word == 0x800802d0u && third.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_SP)
            check(third.word == FrameSp && third.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_V0)
            check(third.word == 0x55u && third.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_S0)
            check(third.word == 0x95u && third.known_mask == 0x0f);
        else
            check(third.word == 0x70000100u + i &&
                third.known_mask ==
                    static_cast<std::uint8_t>((i + 1u) & 0x0f));
    }
    for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        const auto& fourth = f.calls[3].registers.gpr[i];
        if (i == NBA97_MATCH_INITIALIZE_RA)
            check(fourth.word == 0x800802d8u && fourth.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_SP)
            check(fourth.word == FrameSp && fourth.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_A0)
            check(fourth.word == 0x4321u && fourth.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_V0)
            check(fourth.word == f.second_random && fourth.known_mask == 0x0f);
        else
            check(fourth.word == 0x70000200u + i &&
                fourth.known_mask ==
                    static_cast<std::uint8_t>((i + 2u) & 0x0f));
    }
    for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        const auto& final = f.progress.registers.gpr[i];
        if (i == NBA97_MATCH_INITIALIZE_RA)
            check(final.word == CallerRa && final.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_SP)
            check(final.word == EntrySp && final.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_S0)
            check(final.word == EntryS0 && final.known_mask == 0x0f);
        else if (i == NBA97_MATCH_INITIALIZE_V0)
            check(final.word == f.seed_return &&
                final.known_mask == f.seed_return_known);
        else
            check(final.word == 0x70000300u + i &&
                final.known_mask ==
                    static_cast<std::uint8_t>((i + 3u) & 0x0f));
    }
}

void all_random_counts_and_seed_truncation() {
    for (unsigned low = 0; low < 128; ++low) {
        Fixture f;
        f.first_random = 0xffff8f80u | low;
        f.second_random = 0x89abcdefu;
        check(f.run() == NBA97_TEXT_COMPLETE);
        const std::size_t count = low + 64u;
        check(f.progress.warmup_count.word == count &&
            f.progress.step_calls == count &&
            f.progress.operations == count + 8u &&
            f.calls.size() == count + 4u);
        check(f.calls[2].registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
            count);
        check(f.calls[3].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            0xcdefu);
    }

    Fixture partial_seed;
    partial_seed.first_random = 0;
    partial_seed.second_random = 0xa1b2c3d4u;
    partial_seed.second_random_known = 0x05;
    partial_seed.seed_zero_skip = true;
    check(partial_seed.run() == NBA97_TEXT_COMPLETE &&
        partial_seed.progress.seed_argument.word == 0xc3d4u &&
        partial_seed.progress.seed_argument.known_mask == 0x0d &&
        partial_seed.calls[3].registers.gpr[NBA97_MATCH_INITIALIZE_A0]
            .known_mask == 0x0d);
}

void live_s0_zero_unknown_mutation_and_bounded_runaway() {
    Fixture zero_skip;
    zero_skip.seed_zero_skip = true;
    check(zero_skip.run() == NBA97_TEXT_COMPLETE &&
        zero_skip.progress.step_calls == 0 && zero_skip.calls.size() == 4 &&
        zero_skip.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            zero_skip.seed_return);

    Fixture early_stop;
    early_stop.stop_after_first_step = true;
    check(early_stop.run() == NBA97_TEXT_COMPLETE &&
        early_stop.progress.step_calls == 1);

    Fixture unknown;
    unknown.seed_unknown_s0 = true;
    check(unknown.run() == NBA97_TEXT_UNKNOWN &&
        unknown.progress.stopped_pc == 0x800802d8u &&
        unknown.progress.operations == 6 && unknown.calls.size() == 4 &&
        unknown.progress.step_calls == 0);

    Fixture unknown_count;
    unknown_count.first_random_known = 0x0e;
    unknown_count.seed_return_known = 0x07;
    check(unknown_count.run() == NBA97_TEXT_UNKNOWN &&
        unknown_count.calls[2].registers.gpr[NBA97_MATCH_INITIALIZE_S0]
            .known_mask == 0x0e &&
        unknown_count.progress.warmup_count.known_mask == 0x0e &&
        unknown_count.progress.stopped_pc == 0x800802d8u);

    Fixture partial_nonzero;
    partial_nonzero.seed_partial_nonzero_s0 = true;
    check(partial_nonzero.run() == NBA97_TEXT_COMPLETE &&
        partial_nonzero.progress.step_calls == 1 &&
        partial_nonzero.calls[4].registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
            0xffu &&
        partial_nonzero.calls[4].registers.gpr[NBA97_MATCH_INITIALIZE_S0]
            .known_mask == 0);

    Fixture unknown_after_step;
    unknown_after_step.seed_two_steps = true;
    unknown_after_step.step_unknown_s0 = true;
    check(unknown_after_step.run() == NBA97_TEXT_UNKNOWN &&
        unknown_after_step.progress.step_calls == 1 &&
        unknown_after_step.progress.stopped_pc == 0x800802e8u &&
        unknown_after_step.progress.callbacks_completed == 5);

    Fixture runaway;
    runaway.keep_loop_running = true;
    runaway.context.operation_budget = 11;
    check(runaway.run() == NBA97_TEXT_LIMIT &&
        runaway.progress.operations == 11 && runaway.progress.step_calls == 5 &&
        runaway.progress.callbacks_completed == 9 && runaway.calls.size() == 9 &&
        runaway.progress.stopped_pc == 0x800802e0u &&
        runaway.progress.stopped_entry == 0x800935c4u &&
        runaway.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word == 0);
}

void stack_relocation_knownness_alignment_and_wrap() {
    Fixture relocated;
    relocated.seed_zero_skip = true;
    relocated.relocate_stack = true;
    check(relocated.run() == NBA97_TEXT_COMPLETE &&
        relocated.progress.restored_return_address.word == 0x81234560u &&
        relocated.progress.restored_s0.word == 0xaabbccddu &&
        relocated.progress.restored_s0.known_mask == 0x05 &&
        relocated.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            RelocatedSp + 0x18u &&
        relocated.journal[2].address == RelocatedSp + 0x14u &&
        relocated.journal[3].address == RelocatedSp + 0x10u);

    Fixture unknown_entry_sp;
    unknown_entry_sp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
        .known_mask = 0x07;
    check(unknown_entry_sp.run() == NBA97_TEXT_UNKNOWN &&
        !unknown_entry_sp.progress.operations &&
        unknown_entry_sp.progress.stopped_pc == 0x800802b0u &&
        !unknown_entry_sp.progress.stopped_address &&
        unknown_entry_sp.progress.frame_stack_pointer == FrameSp &&
        unknown_entry_sp.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_SP].word == FrameSp &&
        unknown_entry_sp.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_SP].known_mask == 0x07);

    Fixture unknown_live_sp;
    unknown_live_sp.seed_zero_skip = true;
    unknown_live_sp.unknown_live_sp = true;
    check(unknown_live_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_live_sp.progress.stopped_pc == 0x800802f0u &&
        unknown_live_sp.progress.operations == 6 &&
        unknown_live_sp.progress.callbacks_completed == 4 &&
        !unknown_live_sp.progress.reads);

    Fixture unaligned_live_sp;
    unaligned_live_sp.seed_zero_skip = true;
    unaligned_live_sp.unaligned_live_sp = true;
    check(unaligned_live_sp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_live_sp.progress.stopped_pc == 0x800802f0u &&
        unaligned_live_sp.progress.callbacks_completed == 4);

    Fixture missing_live_sp;
    missing_live_sp.seed_zero_skip = true;
    missing_live_sp.missing_live_sp = true;
    check(missing_live_sp.run() == NBA97_TEXT_RESOURCE &&
        missing_live_sp.progress.stopped_pc == 0x800802f0u &&
        missing_live_sp.progress.callbacks_completed == 4);

    Fixture unknown_ra;
    unknown_ra.seed_zero_skip = true;
    unknown_ra.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {CallerRa, 0x03};
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.operations == 8 && unknown_ra.progress.reads == 2 &&
        unknown_ra.progress.stopped_pc == 0x800802fcu &&
        unknown_ra.progress.restored_return_address.known_mask == 0x03 &&
        unknown_ra.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp);

    Fixture rewritten;
    rewritten.seed_zero_skip = true;
    rewritten.rewrite_saved_words = true;
    check(rewritten.run() == NBA97_TEXT_UNKNOWN &&
        rewritten.progress.restored_return_address.word == 0x82345670u &&
        rewritten.progress.restored_return_address.known_mask == 0x03 &&
        rewritten.progress.restored_s0.word == 0x01020304u &&
        rewritten.progress.restored_s0.known_mask == 0x06);

    Fixture unaligned;
    unaligned.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word += 2u;
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.operations == 1 &&
        unaligned.progress.stopped_pc == 0x800802b0u);

    Fixture missing;
    missing.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
        0x90000018u;
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc == 0x800802b0u);

    Fixture malformed_memory;
    malformed_memory.stack_known[FrameSp + 0x14u - Stack] = 2;
    check(malformed_memory.run() == NBA97_TEXT_ARGUMENT &&
        malformed_memory.progress.stopped_pc == 0x800802b0u);

    Fixture no_masks;
    no_masks.seed_zero_skip = true;
    no_masks.region.known = nullptr;
    check(no_masks.run() == NBA97_TEXT_COMPLETE);
    Fixture no_masks_partial;
    no_masks_partial.region.known = nullptr;
    no_masks_partial.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = 0x07;
    check(no_masks_partial.run() == NBA97_TEXT_ARGUMENT &&
        no_masks_partial.progress.operations == 1 &&
        !no_masks_partial.progress.stores);

    std::array<std::uint8_t, 0x20> bytes{};
    std::array<std::uint8_t, 0x20> known{};
    known.fill(1);
    Nba97GameTextRegion region{0, bytes.data(), known.data(), bytes.size()};
    Nba97GameSceneRandomWarmupContext wrap{};
    wrap.memory = {&region, 1};
    wrap.operation_budget = 8;
    wrap.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
    wrap.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x10u, 0x0f};
    wrap.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x87654320u, 0x0f};
    wrap.io = [](void*, const Nba97GameTextMemory*,
        const Nba97GameSceneRandomWarmupEvent* event,
        Nba97GameSceneRandomWarmupRegisters* registers) {
        if (event->kind == NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0f};
        if (event->kind == NBA97_GAME_SCENE_RANDOM_WARMUP_SEED_80093694)
            registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0, 0x0f};
        return 1;
    };
    Nba97GameSceneRandomWarmupProgress wrapped{};
    check(nba97_game_scene_random_warmup(&wrap, &wrapped) ==
        NBA97_TEXT_COMPLETE && wrapped.frame_stack_pointer == 0xfffffff8u &&
        wrapped.restored_return_address.word == 0x87654320u &&
        wrapped.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 0x10u);
}

void refusals_malformed_arguments_and_every_budget_prefix() {
    for (std::size_t fail = 0; fail < 5; ++fail) {
        Fixture f;
        f.seed_two_steps = true;
        f.refuse_call = fail;
        check(f.run() == NBA97_TEXT_IO_REFUSED && !f.progress.completed &&
            f.calls.size() == fail + 1u &&
            f.progress.callbacks_completed == fail &&
            f.progress.stopped_pc == f.calls.back().event.pc &&
            f.progress.stopped_entry == f.calls.back().event.entry);
    }
    Fixture no_io;
    no_io.context.io = nullptr;
    check(no_io.run() == NBA97_TEXT_IO_REFUSED &&
        no_io.progress.operations == 3 && no_io.progress.stores == 2 &&
        no_io.progress.stopped_pc == 0x800802b4u &&
        no_io.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x800802bcu);
    for (std::size_t fail = 0; fail < 5; ++fail) {
        Fixture malformed;
        malformed.seed_two_steps = true;
        malformed.malformed_call = fail;
        check(malformed.run() == NBA97_TEXT_ARGUMENT &&
            malformed.calls.size() == fail + 1u &&
            malformed.progress.callbacks_completed == fail &&
            malformed.progress.stopped_pc == malformed.calls.back().event.pc);
    }
    Fixture malformed_zero;
    malformed_zero.malformed_call = 1;
    malformed_zero.malformed_zero = true;
    check(malformed_zero.run() == NBA97_TEXT_ARGUMENT &&
        malformed_zero.progress.callbacks_completed == 1);

    for (std::size_t budget = 0; budget < 72; ++budget) {
        Fixture f;
        f.first_random = 0;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations == budget);
        const std::uint32_t expected_pc = budget == 0 ? 0x800802b0u :
            budget == 1 ? 0x800802b8u : budget == 2 ? 0x800802b4u :
            budget == 3 ? 0x800802bcu : budget == 4 ? 0x800802c8u :
            budget == 5 ? 0x800802d0u : budget < 70 ? 0x800802e0u :
            budget == 70 ? 0x800802f0u : 0x800802f4u;
        const std::size_t expected_callbacks = budget <= 2 ? 0 :
            (budget - 2u < 68u ? budget - 2u : 68u);
        check(f.progress.stopped_pc == expected_pc &&
            f.progress.callbacks_completed == expected_callbacks &&
            f.calls.size() == expected_callbacks &&
            f.progress.stores == (budget < 2 ? budget : 2u) &&
            f.progress.reads == (budget == 71 ? 1u : 0u) &&
            f.progress.accesses == (budget < 2 ? budget :
                budget == 70 ? 2u : budget == 71 ? 3u : 2u));
        if (budget >= 6 && budget < 70)
            check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
                69u - budget);
        if (budget == 71)
            check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
                CallerRa);
    }
    Fixture exact;
    exact.first_random = 0;
    exact.context.operation_budget = 72;
    check(exact.run() == NBA97_TEXT_COMPLETE && exact.progress.operations == 72);

    Fixture delay_save;
    delay_save.context.operation_budget = 1;
    check(delay_save.run() == NBA97_TEXT_LIMIT &&
        delay_save.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x800802bcu && delay_save.get(FrameSp + 0x14u) == CallerRa);
    Fixture delay_count;
    delay_count.first_random = 0x7fu;
    delay_count.context.operation_budget = 4;
    check(delay_count.run() == NBA97_TEXT_LIMIT &&
        delay_count.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x800802d0u &&
        delay_count.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
            191u);
    Fixture delay_seed;
    delay_seed.second_random = 0x1234abcdu;
    delay_seed.context.operation_budget = 5;
    check(delay_seed.run() == NBA97_TEXT_LIMIT &&
        delay_seed.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x800802d8u &&
        delay_seed.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            0xabcdu);
    Fixture delay_step;
    delay_step.seed_two_steps = true;
    delay_step.context.operation_budget = 6;
    check(delay_step.run() == NBA97_TEXT_LIMIT &&
        delay_step.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x800802e8u &&
        delay_step.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word == 1);

    Fixture overlap;
    Nba97GameTextRegion duplicates[2] = {overlap.region, overlap.region};
    overlap.context.memory = {duplicates, 2};
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
    Fixture bad_mask;
    bad_mask.context.registers.gpr[NBA97_MATCH_INITIALIZE_T0].known_mask = 0x10;
    check(bad_mask.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_zero;
    bad_zero.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {1, 0x0f};
    check(bad_zero.run() == NBA97_TEXT_ARGUMENT);
    Nba97GameSceneRandomWarmupContext context{};
    Nba97GameSceneRandomWarmupProgress progress{};
    check(nba97_game_scene_random_warmup(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_scene_random_warmup(&context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    exact_order_delays_and_registers();
    all_gpr_forwarding_and_overwrites();
    all_random_counts_and_seed_truncation();
    live_s0_zero_unknown_mutation_and_bounded_runaway();
    stack_relocation_knownness_alignment_and_wrap();
    refusals_malformed_arguments_and_every_budget_prefix();
    std::printf("%u game scene random warm-up checks passed\n", checks);
    return 0;
}
