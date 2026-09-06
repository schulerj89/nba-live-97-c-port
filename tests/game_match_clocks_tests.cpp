#include "recovered/game_match_clocks.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "match clocks check %u failed at %u\n", checks,
            line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x800ff000u;
constexpr std::uint32_t EntryRa = 0x81234567u;

struct CallRecord {
    Nba97GameMatchClocksEvent event{};
    Nba97GameMatchClocksMachine machine{};
};

struct Fixture {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameMatchClocksAccess journal[96]{};
    Nba97GameMatchClocksContext context{};
    Nba97GameMatchClocksProgress progress{};
    std::vector<CallRecord> calls;
    std::uint32_t refuse_pc{};
    std::uint32_t mutate_pc{};
    std::uint32_t relocated_sp = 0x800fe000u;
    bool malformed{};

    Fixture() {
        context.memory = {&region, 1};
        context.operation_budget = 100;
        context.io = io;
        context.user = this;
        context.access_journal = journal;
        context.access_journal_capacity = std::size(journal);
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.machine.registers.gpr[i] =
                {0x20000000u + i * 0x01010101u, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {EntrySp, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {EntryRa, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {1, 0x0f};
        context.machine.hi = {0x11223344u, 0x0f};
        context.machine.lo = {0x55667788u, 0x0f};
        put(0x800fdb58u, 600, 4);
        put(0x800fdb90u, 0, 2);
        put(0x800fe882u, 0, 2);
        put(0x80021d90u, 1, 1);
        put(0x800fdb5cu, 0, 4);
        put(0x800fdb60u, 0xffffffffu, 4);
        put(0x800fdba4u, 120, 4);
        put(0x80021d92u, 1, 1);
        put(0x800fdb86u, 0x7777, 2);
        put(0x8001eeb4u, 5, 2);
        put(0x8001eeb6u, 9, 2);
        put(0x8001ef78u, 5, 2);
        put(0x8001ef7au, 9, 2);
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Ram);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width) {
        const auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            known[at + i] = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        const auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[at + i]) << (i * 8u);
        return value;
    }
    void setKnown(std::uint32_t address, unsigned width, std::uint8_t mask) {
        const auto at = offset(address);
        for (unsigned i = 0; i < width; ++i)
            known[at + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
    }
    void setDelta(std::uint32_t value, std::uint8_t mask = 0x0f) {
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {value, mask};
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameMatchClocksEvent* event,
        Nba97GameMatchClocksMachine* machine) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back({*event, *machine});
        if (event->pc == f.mutate_pc) {
            machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S2] = {2, 0x0f};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {200, 0x0f};
            machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1] = {300, 0x0f};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_T9] =
                {0xabcdef01u, 0x05};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
                {f.relocated_sp, 0x0f};
            machine->hi = {0xaabbccddu, 0x03};
            machine->lo = {0x13579bdfu, 0x0a};
        }
        if (f.malformed)
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask = 0;
        return event->pc != f.refuse_pc;
    }
    int run(std::size_t budget = 100) {
        context.operation_budget = budget;
        return nba97_game_match_clocks(&context, &progress);
    }
};

std::int64_t signed32(std::uint32_t value) {
    return value < 0x80000000u ? std::int64_t(value) :
        std::int64_t(value) - 0x100000000ll;
}

void frame_gate_and_phase_paths() {
    const std::array<std::uint16_t, 6> phases{
        0xffffu, 0x007fu, 0x0080u, 0x0081u, 0x0082u, 0x0083u};
    for (const auto phase : phases) {
        Fixture f;
        f.put(0x800fdb90u, phase, 2);
        const bool eligible = phase == 0xffffu || phase == 0x007fu ||
            phase == 0x0082u;
        check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
        check(bool(f.progress.main_clock_eligible) == eligible);
        check(f.progress.frame_stack_pointer == EntrySp - 0x30u &&
            f.progress.restored_return_address.word == EntryRa &&
            f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
                EntrySp);
        check(f.get(0x800fdb58u, 4) == (eligible ? 599u : 600u));
    }

    Fixture clock_zero;
    clock_zero.put(0x800fdb58u, 0, 4);
    check(clock_zero.run() == NBA97_TEXT_COMPLETE &&
        !clock_zero.progress.main_clock_eligible &&
        clock_zero.get(0x800fdb86u, 2) == 0x7777u &&
        clock_zero.progress.operations == 10);

    struct GateCase { std::uint32_t block; std::uint8_t enable;
        std::uint32_t limit; std::uint32_t stop; bool eligible; };
    const std::array<GateCase, 5> cases{{
        {1, 1, 0, 0xffffffffu, false},
        {0, 0, 0, 0xffffffffu, false},
        {0, 1, 600, 0xffffffffu, false},
        {0, 1, 599, 600, false},
        {0, 255, 0xffffffffu, 0, true}}};
    for (const auto& c : cases) {
        Fixture f;
        f.put(0x800fdb90u, 0x82, 2);
        f.put(0x800fe882u, c.block, 2);
        f.put(0x80021d90u, c.enable, 1);
        f.put(0x800fdb5cu, c.limit, 4);
        f.put(0x800fdb60u, c.stop, 4);
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(bool(f.progress.main_clock_eligible) == c.eligible);
    }
}

void exact_division_and_main_sounds() {
    const std::array<std::uint32_t, 10> values{{0, 1, 59, 60, 61, 119,
        120, 0x7fffffffu, 0x80000000u, 0xffffffffu}};
    for (const auto value : values) {
        Fixture f;
        f.put(0x800fdb58u, value, 4);
        f.put(0x800fdba4u, 0, 4);
        f.setDelta(0);
        check(f.run() == NBA97_TEXT_COMPLETE);
        if (value == 0) {
            check(f.progress.multiply_count == 0);
            continue;
        }
        const auto& t = f.progress.multiply[0];
        const auto product = signed32(value) * signed32(0x88888889u);
        check(t.pc == 0x80067b18u && t.multiplicand.word == value &&
            t.multiplier.word == 0x88888889u &&
            t.lo.word == std::uint32_t(product) &&
            t.hi.word == std::uint32_t(std::uint64_t(product) >> 32u));
        check(signed32(t.seconds.word) == signed32(value) / 60 &&
            t.hi.known_mask == 0x0f && t.lo.known_mask == 0x0f);
    }

    struct SoundCase { std::uint32_t clock; std::uint32_t delta;
        std::uint32_t pc; std::uint32_t argument; };
    const std::array<SoundCase, 4> sounds{{
        {1, 1, 0x80067bc8u, 10}, {240, 1, 0x80067b78u, 11},
        {7200, 1, 0x80067b94u, 2}, {3600, 1, 0x80067bb8u, 1}}};
    for (const auto& s : sounds) {
        Fixture f;
        f.put(0x800fdb58u, s.clock, 4);
        f.put(0x800fdba4u, 0, 4);
        f.setDelta(s.delta);
        const int status = f.run();
        if (status != NBA97_TEXT_COMPLETE || f.calls.empty())
            std::fprintf(stderr, "sound case clock=%u status=%d stop=%08x calls=%zu\n",
                s.clock, status, f.progress.stopped_pc, f.calls.size());
        check(status == NBA97_TEXT_COMPLETE && !f.calls.empty());
        check(f.calls.front().event.pc == s.pc &&
            f.calls.front().event.delay_slot_pc == s.pc + 4u &&
            f.calls.front().machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_A0].word == s.argument &&
            f.calls.front().machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_RA].word == s.pc + 8u);
    }

    Fixture negative;
    negative.put(0x800fdb58u, 0x80000000u, 4);
    negative.put(0x800fdba4u, 0, 4);
    negative.setDelta(0x7fffffffu);
    check(negative.run() == NBA97_TEXT_COMPLETE &&
        negative.get(0x800fdb58u, 4) == 0 &&
        negative.calls.front().event.pc == 0x80067bc8u);

    struct DeltaCase { std::uint32_t clock; std::uint32_t delta;
        std::uint32_t expected; };
    const std::array<DeltaCase, 5> deltas{{
        {100, 0x80000000u, 0x80000064u},
        {100, 0x7fffffffu, 0},
        {0x7fffffffu, 0x80000000u, 0xffffffffu},
        {0x80000000u, 0x7fffffffu, 0},
        {0xffffffffu, 0xfffffffeu, 1}}};
    for (const auto& d : deltas) {
        Fixture f;
        f.put(0x800fdb58u, d.clock, 4);
        f.put(0x800fdba4u, 0, 4);
        f.setDelta(d.delta);
        check(f.run() == NBA97_TEXT_COMPLETE &&
            f.get(0x800fdb58u, 4) == d.expected);
    }
}

void shot_clock_and_team_quirks() {
    Fixture shot_zero;
    shot_zero.put(0x800fdba4u, 1, 4);
    shot_zero.setDelta(1);
    check(shot_zero.run() == NBA97_TEXT_COMPLETE &&
        shot_zero.get(0x800fdba4u, 4) == 0);
    check(shot_zero.calls.back().event.pc == 0x80067ca8u &&
        shot_zero.calls.back().machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_A0].word == 10);

    Fixture disabled;
    disabled.put(0x800fdba4u, 1, 4);
    disabled.put(0x80021d92u, 0, 1);
    check(disabled.run() == NBA97_TEXT_COMPLETE && disabled.calls.empty());
    Fixture byte255;
    byte255.put(0x800fdba4u, 1, 4);
    byte255.put(0x80021d92u, 255, 1);
    check(byte255.run() == NBA97_TEXT_COMPLETE &&
        byte255.calls.back().event.pc == 0x80067ca8u);

    Fixture negative_shot;
    negative_shot.put(0x800fdba4u, 0xffffffffu, 4);
    negative_shot.setDelta(1);
    check(negative_shot.run() == NBA97_TEXT_COMPLETE &&
        negative_shot.get(0x800fdba4u, 4) == 0 &&
        negative_shot.calls.back().machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_A0].word == 10);

    Fixture threshold300;
    threshold300.put(0x800fdb58u, 300, 4);
    threshold300.put(0x800fdba4u, 240, 4);
    threshold300.setDelta(1);
    check(threshold300.run() == NBA97_TEXT_COMPLETE &&
        threshold300.calls.empty());
    Fixture threshold301;
    threshold301.put(0x800fdb58u, 301, 4);
    threshold301.put(0x800fdba4u, 240, 4);
    threshold301.setDelta(0);
    check(threshold301.run() == NBA97_TEXT_COMPLETE &&
        threshold301.calls.empty());

    Fixture low_half;
    low_half.put(0x800fdb58u, 10000, 4);
    low_half.put(0x800fdba4u, 32769u * 60u, 4);
    low_half.setDelta(60);
    check(low_half.run() == NBA97_TEXT_COMPLETE &&
        low_half.calls.back().event.pc == 0x80067ca8u &&
        low_half.calls.back().machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_A0].word == 11);

    Fixture timers;
    timers.put(0x800fdba4u, 0, 4);
    timers.put(0x8001eeb4u, 1, 2);
    timers.put(0x8001ef78u, 0xffffu, 2);
    timers.setDelta(2);
    check(timers.run() == NBA97_TEXT_COMPLETE &&
        timers.get(0x8001eeb4u, 2) == 0xffffu &&
        timers.get(0x8001eeb6u, 2) == 9 &&
        timers.get(0x8001ef78u, 2) == 0 &&
        timers.get(0x8001ef7au, 2) == 2 &&
        timers.get(0x800fdb86u, 2) == 0);
    timers.setDelta(2);
    check(timers.run() == NBA97_TEXT_COMPLETE &&
        timers.get(0x8001eeb4u, 2) == 0 &&
        timers.get(0x8001eeb6u, 2) == 2);
}

void callback_mutation_alias_and_full_state() {
    Fixture mutation;
    mutation.put(0x800fdb58u, 240, 4);
    mutation.put(0x800fdba4u, 0, 4);
    mutation.setDelta(1);
    mutation.mutate_pc = 0x80067b78u;
    mutation.put(mutation.relocated_sp + 0x2cu, 0x90000001u, 4);
    mutation.put(mutation.relocated_sp + 0x28u, 0x90000002u, 4);
    mutation.put(mutation.relocated_sp + 0x24u, 0x90000003u, 4);
    mutation.put(mutation.relocated_sp + 0x20u, 0x90000004u, 4);
    check(mutation.run() == NBA97_TEXT_COMPLETE &&
        mutation.progress.restored_return_address.word == 0x90000001u &&
        mutation.progress.restored_s2.word == 0x90000002u &&
        mutation.progress.restored_s1.word == 0x90000003u &&
        mutation.progress.restored_s0.word == 0x90000004u &&
        mutation.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == mutation.relocated_sp + 0x30u);
    check(mutation.get(0x8001eeb4u, 2) == 3 &&
        mutation.get(0x8001ef78u, 2) == 3 &&
        mutation.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_T9].known_mask == 0x05 &&
        mutation.progress.machine.hi.known_mask == 0x03 &&
        mutation.progress.machine.lo.word == 0x13579bdfu);

    Fixture alias;
    alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {0x800fdb60u, 0x0f};
    alias.context.machine.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S2] =
        {120, 0x0f};
    alias.put(0x800fdba4u, 0, 4);
    alias.setDelta(1);
    check(alias.run() == NBA97_TEXT_COMPLETE &&
        alias.progress.multiply[0].multiplicand.word == 120 &&
        alias.get(0x800fdb58u, 4) == 119 &&
        alias.progress.restored_s2.word == 119);

    Fixture gated;
    gated.put(0x800fdb90u, 0x81, 2);
    const auto before = gated.context.machine;
    check(gated.run() == NBA97_TEXT_COMPLETE);
    for (unsigned i = 9; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        if (i == NBA97_MATCH_INITIALIZE_S0 ||
            i == NBA97_GAME_MATCH_CLOCKS_S1 ||
            i == NBA97_GAME_MATCH_CLOCKS_S2 ||
            i == NBA97_MATCH_INITIALIZE_SP || i == NBA97_MATCH_INITIALIZE_RA)
            continue;
        check(gated.progress.machine.registers.gpr[i].word ==
                before.registers.gpr[i].word &&
            gated.progress.machine.registers.gpr[i].known_mask ==
                before.registers.gpr[i].known_mask);
    }
    check(gated.progress.machine.hi.word == before.hi.word &&
        gated.progress.machine.lo.word == before.lo.word);
}

void branch_delay_prefixes() {
    /* 0x80067B70 compares two resolved source quotients on entry-driven
     * executions. Stopping at the next mapped operation proves its SLTI delay
     * result remains in v0 on the equal branch. */
    Fixture main_equal;
    main_equal.put(0x800fdb58u, 180, 4);
    main_equal.put(0x800fdba4u, 0, 4);
    main_equal.setDelta(0);
    check(main_equal.run(8) == NBA97_TEXT_LIMIT &&
        main_equal.progress.stopped_pc == 0x80067bd8u &&
        main_equal.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].word == 1 &&
        main_equal.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0f);

    /* 0x80067C84's ORI delay is visible at the refused JAL boundary. */
    Fixture shot_compare;
    shot_compare.put(0x800fdb58u, 10000, 4);
    shot_compare.put(0x800fdba4u, 32769u * 60u, 4);
    shot_compare.setDelta(60);
    shot_compare.refuse_pc = 0x80067ca8u;
    check(shot_compare.run() == NBA97_TEXT_IO_REFUSED &&
        shot_compare.progress.stopped_pc == 0x80067ca8u &&
        shot_compare.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_A0].word == 11 &&
        shot_compare.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_A0].known_mask == 0x0f);

    /* The unknown 0x80067CA0 flag predicate still executes ORI a0,10 and the
     * published refusal state must include that delay-slot mutation. */
    Fixture unknown_shot_flag;
    unknown_shot_flag.put(0x800fdba4u, 1, 4);
    unknown_shot_flag.setKnown(0x80021d92u, 1, 0);
    check(unknown_shot_flag.run() == NBA97_TEXT_UNKNOWN &&
        unknown_shot_flag.progress.stopped_pc == 0x80067ca0u &&
        unknown_shot_flag.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_A0].word == 10 &&
        unknown_shot_flag.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_A0].known_mask == 0x0f);
}

void unknown_failures_and_every_budget_prefix() {
    Fixture unknown_phase;
    unknown_phase.setKnown(0x800fdb90u, 2, 0);
    check(unknown_phase.run() == NBA97_TEXT_UNKNOWN &&
        unknown_phase.progress.stopped_pc == 0x80067a8cu &&
        unknown_phase.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_A1].known_mask == 0x0f);

    Fixture unknown_clock;
    unknown_clock.put(0x800fdb58u, 600, 4);
    unknown_clock.setKnown(0x800fdb58u, 4, 0x0b);
    unknown_clock.setDelta(0);
    check(unknown_clock.run() == NBA97_TEXT_UNKNOWN &&
        unknown_clock.progress.stopped_pc == 0x80067b68u &&
        unknown_clock.progress.multiply_count == 2 &&
        unknown_clock.progress.multiply[0].hi.known_mask == 0 &&
        unknown_clock.progress.multiply[1].lo.known_mask == 0 &&
        unknown_clock.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0e);

    Fixture unknown_timer;
    unknown_timer.put(0x800fdba4u, 0, 4);
    unknown_timer.setKnown(0x8001eeb4u, 2, 1);
    check(unknown_timer.run() == NBA97_TEXT_UNKNOWN &&
        unknown_timer.progress.stopped_pc == 0x80067cc4u &&
        unknown_timer.get(0x800fdb86u, 2) == 0);

    Fixture unknown_ra;
    unknown_ra.put(0x800fdba4u, 0, 4);
    unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = 0x07;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x80067d30u &&
        unknown_ra.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == EntrySp);

    Fixture baseline;
    baseline.put(0x800fdba4u, 240, 4);
    baseline.put(0x800fdb58u, 7200, 4);
    baseline.setDelta(1);
    check(baseline.run() == NBA97_TEXT_COMPLETE);
    const auto total = baseline.progress.operations;
    for (std::size_t budget = 0; budget < total; ++budget) {
        Fixture f;
        f.put(0x800fdba4u, 240, 4);
        f.put(0x800fdb58u, 7200, 4);
        f.setDelta(1);
        check(f.run(budget) == NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations == budget);
    }

    Fixture refusal;
    refusal.put(0x800fdb58u, 1, 4);
    refusal.put(0x800fdba4u, 0, 4);
    refusal.refuse_pc = 0x80067bc8u;
    check(refusal.run() == NBA97_TEXT_IO_REFUSED &&
        refusal.progress.stopped_pc == 0x80067bc8u &&
        refusal.progress.stopped_entry == 0x80029258u &&
        refusal.progress.callbacks_completed == 0);
    struct RefusalCase { std::uint32_t main_clock; std::uint32_t shot_clock;
        std::uint32_t pc; };
    const std::array<RefusalCase, 5> refusals{{
        {240, 0, 0x80067b78u}, {1, 0, 0x80067bc8u},
        {7200, 0, 0x80067b94u}, {3600, 0, 0x80067bb8u},
        {600, 1, 0x80067ca8u}}};
    for (const auto& r : refusals) {
        Fixture f;
        f.put(0x800fdb58u, r.main_clock, 4);
        f.put(0x800fdba4u, r.shot_clock, 4);
        f.refuse_pc = r.pc;
        check(f.run() == NBA97_TEXT_IO_REFUSED &&
            f.progress.stopped_pc == r.pc &&
            f.progress.callbacks_completed == 0);
    }
    Fixture missing;
    missing.put(0x800fdb58u, 1, 4);
    missing.put(0x800fdba4u, 0, 4);
    missing.context.io = nullptr;
    check(missing.run() == NBA97_TEXT_IO_REFUSED);
    Fixture malformed;
    malformed.put(0x800fdb58u, 1, 4);
    malformed.put(0x800fdba4u, 0, 4);
    malformed.malformed = true;
    check(malformed.run() == NBA97_TEXT_ARGUMENT);

    Fixture unaligned;
    unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
        EntrySp + 1u;
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80067a64u);
    Fixture unmapped;
    unmapped.region.size = 0x100u;
    check(unmapped.run() == NBA97_TEXT_RESOURCE);
    Fixture overlap;
    Nba97GameTextRegion regions[2]{overlap.region, overlap.region};
    overlap.context.memory = {regions, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT);
    Fixture wrapping;
    std::array<std::uint8_t, 0x100> low_bytes{};
    std::array<std::uint8_t, 0x100> low_known{};
    low_known.fill(1);
    Nba97GameTextRegion wrap_regions[2]{
        {0, low_bytes.data(), low_known.data(), low_bytes.size()},
        wrapping.region};
    wrapping.context.memory = {wrap_regions, 2};
    wrapping.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {0x20u, 0x0f};
    wrapping.put(0x800fdb90u, 0x81, 2);
    check(wrapping.run() == NBA97_TEXT_COMPLETE &&
        wrapping.progress.frame_stack_pointer == 0xfffffff0u &&
        wrapping.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == 0x20u);
    Nba97GameMatchClocksProgress output{};
    check(nba97_game_match_clocks(nullptr, &output) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_match_clocks(&baseline.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    frame_gate_and_phase_paths();
    exact_division_and_main_sounds();
    shot_clock_and_team_quirks();
    callback_mutation_alias_and_full_state();
    branch_delay_prefixes();
    unknown_failures_and_every_budget_prefix();
    std::printf("game match clocks: %u checks passed\n", checks);
    return 0;
}
