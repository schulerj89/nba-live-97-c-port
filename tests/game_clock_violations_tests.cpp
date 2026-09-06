#include "recovered/game_clock_violations.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "clock violations check %u failed at %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Main = 0x800fdb58u;
constexpr std::uint32_t Phase = 0x800fdb90u;
constexpr std::uint32_t Team = 0x800fdb94u;
constexpr std::uint32_t Shot = 0x800fdba4u;
constexpr std::uint32_t Timer82 = 0x800fdba8u;
constexpr std::uint32_t TimerFinal = 0x800fdbaau;
constexpr std::uint32_t Owner = 0x800fdbccu;
constexpr std::uint32_t BallPointer = 0x800fdc48u;
constexpr std::uint32_t ActorPointer = 0x800fdc34u;
constexpr std::uint32_t State = 0x800fe882u;
constexpr std::uint32_t State82 = 0x800fe884u;
constexpr std::uint32_t Block82 = 0x800fe88eu;
constexpr std::uint32_t BlockFinal = 0x800fe8e0u;
constexpr std::uint32_t Enable82 = 0x80021d90u;
constexpr std::uint32_t EnableFinal = 0x80021d91u;
constexpr std::uint32_t EnableShot = 0x80021d92u;
constexpr std::uint32_t Ball = 0x800e0000u;
constexpr std::uint32_t Actor = 0x800e1000u;

struct Call {
    Nba97GameClockViolationsEvent event{};
    Nba97GameClockViolationsMachine machine{};
};

struct Fixture {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameClockViolationsContext context{};
    Nba97GameClockViolationsProgress progress{};
    std::vector<Nba97GameClockViolationsAccess> journal =
        std::vector<Nba97GameClockViolationsAccess>(128);
    std::vector<Call> calls;
    std::size_t refuse_call{};
    std::size_t malform_call{};
    bool mutate_first{};
    std::uint32_t alternate_sp = 0x800fe000u;

    Fixture(std::uint32_t delta = 1) {
        context.memory = {&region, 1};
        context.operation_budget = 1000;
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.machine.registers.gpr[i] =
                {0x11000000u + i * 0x01010101u, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {delta, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {0x800ff000u, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x81234567u, 0x0f};
        context.machine.hi = {0x12345678u, 0x0f};
        context.machine.lo = {0x9abcdef0u, 0x0f};

        put(Main, 1, 4);
        put(Shot, 1, 4);
        put(EnableShot, 1, 1);
        put(Enable82, 1, 1);
        put(EnableFinal, 1, 1);
        put(Owner, 0, 2);
        put(Team, 0, 2);
        put(BallPointer, Ball, 4);
        put(Ball + 0x10u, 48u << 8u, 4);
        put(Ball + 0x18u, 0, 2);
        put(ActorPointer, Actor, 4);
        put(Actor + 0xa0u, 0, 2);
        put(Phase, 0x80, 2);
        put(State, 0, 2);
        put(State82, 2, 2);
        put(Block82, 0, 2);
        put(BlockFinal, 1, 2);
        put(Timer82, 10, 2);
        put(TimerFinal, 10, 2);
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Ram);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width) {
        auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
            known[at + i] = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[at + i]) << (8u * i);
        return value;
    }
    void forget(std::uint32_t address, unsigned width) {
        auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            known[at + i] = 0;
            bytes[at + i] = 0;
        }
    }
    int run() {
        return nba97_game_clock_violations(&context, &progress);
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameClockViolationsEvent* event,
        Nba97GameClockViolationsMachine* machine) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back({*event, *machine});
        if (f.refuse_call && f.calls.size() == f.refuse_call)
            return 0;
        if (f.malform_call && f.calls.size() == f.malform_call) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
            return 1;
        }
        if (f.mutate_first && f.calls.size() == 1) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
                {f.alternate_sp, 0x0f};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {2, 0x0f};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0] =
                {0xfeedbeefu, 0x0f};
            machine->hi = {0x0badc0deu, 0x0f};
            machine->lo = {0xc001d00du, 0x0f};
        }
        return 1;
    }
};

void check_call(const Call& call, std::uint32_t pc, std::uint32_t entry,
    std::uint32_t a0, unsigned count) {
    check(call.event.pc == pc && call.event.delay_slot_pc == pc + 4u &&
        call.event.entry == entry && call.event.argument_count == count);
    check(call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        pc + 8u);
    check(call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == a0 &&
        call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask ==
            0x0f);
}

void early_exit_and_preframe_order() {
    Fixture f(22);
    f.put(Main, 0, 4);
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations == 5 && f.progress.reads == 3 &&
        f.progress.stores == 2 && f.calls.empty());
    check(f.progress.access_events == 5);
    check(f.journal[0].pc == 0x80067d3cu && f.journal[0].address == Main &&
        f.journal[0].kind == NBA97_GAME_CLOCK_VIOLATIONS_READ);
    check(f.journal[1].pc == 0x80067d44u &&
        f.journal[2].pc == 0x80067d50u &&
        f.journal[3].pc == 0x80068008u &&
        f.journal[4].pc == 0x8006800cu);
    check(f.progress.frame_stack_pointer == 0x800fefe8u &&
        f.progress.restored_return_address.word == 0x81234567u &&
        f.progress.restored_s0.word == 0x21101010u &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x800ff000u);

    Fixture alias(1);
    alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {Main - 0x10u + 0x18u, 0x0f};
    auto old_s0 = alias.context.machine.registers.gpr[
        NBA97_MATCH_INITIALIZE_S0].word;
    check(alias.run() == NBA97_TEXT_COMPLETE);
    check(alias.get(Main, 4) == old_s0);
    check(alias.progress.first_violation_triggered == 0);
}

void first_sequence_and_ball_gates() {
    for (unsigned team : {0u, 1u, 0xffffu}) {
        Fixture f;
        f.put(Shot, 0, 4);
        f.put(Team, team, 2);
        check(f.run() == NBA97_TEXT_COMPLETE &&
            f.progress.first_violation_triggered && f.calls.size() == 4);
        std::uint32_t event = team ? 12u : 11u;
        std::uint32_t duration = team ? 20000u : 5000u;
        check_call(f.calls[0], team ? 0x80067de8u : 0x80067dd8u,
            0x80029590u, event, 1);
        check_call(f.calls[1], 0x80067df4u, 0x800295c8u, duration, 1);
        check_call(f.calls[2], 0x80067dfcu, 0x80062300u, 10, 1);
        check_call(f.calls[3], 0x80067e04u, 0x80062660u, 10, 0);
        check(f.get(State, 2) == 3);
    }

    struct Gate { std::uint32_t height; std::uint16_t velocity; bool fires; };
    for (const auto& gate : std::vector<Gate>{{48u << 8u, 0, true},
            {49u << 8u, 0, false}, {0x80000000u, 0, true},
            {48u << 8u, 1, false}, {48u << 8u, 0xffffu, true}}) {
        Fixture f;
        f.put(Shot, 0, 4);
        f.put(Owner, 0xffff, 2);
        f.put(Ball + 0x10u, gate.height, 4);
        f.put(Ball + 0x18u, gate.velocity, 2);
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(bool(f.progress.first_violation_triggered) == gate.fires);
    }
    for (unsigned shot : {1u, 0xffffffffu}) {
        Fixture f;
        f.put(Shot, shot, 4);
        check(f.run() == NBA97_TEXT_COMPLETE && f.calls.empty());
    }
    Fixture disabled;
    disabled.put(Shot, 0, 4);
    disabled.put(EnableShot, 0, 1);
    check(disabled.run() == NBA97_TEXT_COMPLETE && disabled.calls.empty());
}

void phase_82_gates_and_wrapping() {
    struct Case {
        std::uint16_t owner, actor, state, block;
        bool decrements;
    };
    for (const auto& c : std::vector<Case>{{0, 0, 2, 0, true},
            {0xffff, 0, 2, 0, false}, {0, 1, 2, 0, false},
            {0, 0, 1, 0, false}, {0, 0, 2, 1, false}}) {
        Fixture f;
        f.put(Phase, 0x82, 2);
        f.put(Owner, c.owner, 2);
        f.put(Actor + 0xa0u, c.actor, 2);
        f.put(State82, c.state, 2);
        f.put(Block82, c.block, 2);
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(bool(f.progress.phase_82_timer_decremented) == c.decrements);
    }

    for (std::uint16_t start : {std::uint16_t(0), std::uint16_t(1),
            std::uint16_t(0x7fff), std::uint16_t(0x8000),
            std::uint16_t(0xffff)})
        for (std::uint32_t delta : {0u, 1u, 22u, 0xffffu,
                0xffffffffu}) {
            Fixture f(delta);
            f.put(Phase, 0x82, 2);
            f.put(Timer82, start, 2);
            std::uint16_t wrapped = static_cast<std::uint16_t>(start - delta);
            bool negative = (wrapped & 0x8000u) != 0;
            check(f.run() == NBA97_TEXT_COMPLETE);
            check(f.progress.phase_82_timer_decremented);
            check(f.get(Timer82, 2) == (negative ? 0u : wrapped));
            check(bool(f.progress.phase_82_violation_triggered) == negative);
            if (negative)
                check(f.get(Phase, 2) == 0);
        }

    for (unsigned enabled : {0u, 255u}) {
        Fixture f;
        f.put(Phase, 0x82, 2);
        f.put(Timer82, 0, 2);
        f.put(Enable82, enabled, 1);
        check(f.run() == NBA97_TEXT_COMPLETE && f.get(Timer82, 2) == 0);
        check(bool(f.progress.phase_82_violation_triggered) ==
            (enabled != 0));
        check(f.get(Phase, 2) == (enabled ? 0u : 0x82u));
    }

    for (unsigned old_state : {0u, 1u, 2u, 0xffffu}) {
        Fixture f;
        f.put(Phase, 0x82, 2);
        f.put(Timer82, 0, 2);
        f.put(State, old_state, 2);
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(f.get(State, 2) == ((static_cast<std::int16_t>(old_state) < 2)
            ? 1u : 3u));
        check(f.calls.size() == 4);
        check_call(f.calls[2], 0x80067ef8u, 0x80062300u, 11, 1);
    }
}

void final_sequence_and_phase_gates() {
    for (std::uint16_t phase : {std::uint16_t(0xffff), std::uint16_t(0),
            std::uint16_t(0x7f), std::uint16_t(0x80),
            std::uint16_t(0x81), std::uint16_t(0x82),
            std::uint16_t(0x83)}) {
        Fixture f;
        f.put(Phase, phase, 2);
        f.put(BlockFinal, 0, 2);
        f.put(TimerFinal, 5, 2);
        check(f.run() == NBA97_TEXT_COMPLETE);
        bool expected = static_cast<std::int16_t>(phase) < 0x80;
        check(bool(f.progress.final_timer_decremented) == expected);
        check(f.get(TimerFinal, 2) == (expected ? 4u : 5u));
    }

    for (unsigned team : {0u, 1u}) {
        Fixture f;
        f.put(Phase, 0, 2);
        f.put(BlockFinal, 0, 2);
        f.put(TimerFinal, 0, 2);
        f.put(Team, team, 2);
        check(f.run() == NBA97_TEXT_COMPLETE &&
            f.progress.final_violation_triggered && f.calls.size() == 4);
        check_call(f.calls[0], team ? 0x80067fd0u : 0x80067fc0u,
            0x80029590u, team ? 12u : 11u, 1);
        check_call(f.calls[1], 0x80067fdcu, 0x800295c8u,
            team ? 20000u : 5000u, 1);
        check_call(f.calls[2], 0x80067fe4u, 0x80062300u, 12, 1);
        check_call(f.calls[3], 0x80067fecu, 0x80062660u, 12, 0);
        check(f.get(State, 2) == 4 && f.get(TimerFinal, 2) == 0);
    }

    for (auto [owner, enabled, fires] :
            std::vector<std::tuple<std::uint16_t, unsigned, bool>>{
                {std::uint16_t(0), 255u, true},
                {std::uint16_t(0xffff), 255u, false},
                {std::uint16_t(0), 0u, false}}) {
        Fixture f;
        f.put(Phase, 0, 2);
        f.put(BlockFinal, 0, 2);
        f.put(TimerFinal, 0, 2);
        f.put(Owner, owner, 2);
        f.put(EnableFinal, enabled, 1);
        check(f.run() == NBA97_TEXT_COMPLETE && f.get(TimerFinal, 2) == 0);
        check(bool(f.progress.final_violation_triggered) == fires);
    }
}

void child_mutation_full_machine_and_independent_triggers() {
    Fixture f;
    f.put(Shot, 0, 4);
    f.put(Phase, 0, 2);
    f.put(BlockFinal, 0, 2);
    f.put(TimerFinal, 1, 2);
    f.mutate_first = true;
    f.put(f.alternate_sp + 0x14u, 0x87654321u, 4);
    f.put(f.alternate_sp + 0x10u, 0x13572468u, 4);
    check(f.run() == NBA97_TEXT_COMPLETE);
    check(f.progress.first_violation_triggered &&
        f.progress.final_violation_triggered && f.calls.size() == 8);
    check(f.progress.machine.hi.word == 0x0badc0deu &&
        f.progress.machine.lo.word == 0xc001d00du &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0].word ==
            0xfeedbeefu);
    check(f.progress.restored_return_address.word == 0x87654321u &&
        f.progress.restored_s0.word == 0x13572468u &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            f.alternate_sp + 0x18u);

    Fixture all;
    all.put(Shot, 0, 4);
    all.put(Phase, 0x82, 2);
    all.put(Timer82, 0, 2);
    all.put(BlockFinal, 0, 2);
    all.put(TimerFinal, 0, 2);
    all.mutate_first = false;
    /* First sequence's final child makes the later phase-82 gate and timer
     * live; the callback mutation proves the source does not cache globals. */
    all.context.io = [](void* user, const Nba97GameTextMemory*,
        const Nba97GameClockViolationsEvent* event,
        Nba97GameClockViolationsMachine* machine) -> int {
        auto& x = *static_cast<Fixture*>(user);
        x.calls.push_back({*event, *machine});
        if (x.calls.size() == 4) {
            x.put(Phase, 0x82, 2);
            x.put(Timer82, 0, 2);
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {1, 0x0f};
        }
        return 1;
    };
    check(all.run() == NBA97_TEXT_COMPLETE &&
        all.progress.first_violation_triggered &&
        all.progress.phase_82_violation_triggered &&
        all.progress.final_violation_triggered && all.calls.size() == 12);
}

void unknown_failures_and_budgets() {
    Fixture unknown;
    unknown.forget(Main, 4);
    check(unknown.run() == NBA97_TEXT_UNKNOWN);
    check(unknown.progress.operations == 3 &&
        unknown.progress.stopped_pc == 0x80067d4cu &&
        unknown.progress.stores == 2);

    Fixture unknown_pointer;
    unknown_pointer.put(Shot, 0, 4);
    unknown_pointer.put(Owner, 0xffff, 2);
    unknown_pointer.forget(BallPointer, 4);
    check(unknown_pointer.run() == NBA97_TEXT_UNKNOWN &&
        unknown_pointer.progress.stopped_pc == 0x80067d9cu);

    Fixture unknown_ra;
    unknown_ra.put(Main, 0, 4);
    unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = 0;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x80068014u);

    Fixture unknown_delay;
    unknown_delay.put(Phase, 0x82, 2);
    unknown_delay.forget(Actor + 0xa0u, 2);
    check(unknown_delay.run() == NBA97_TEXT_UNKNOWN &&
        unknown_delay.progress.stopped_pc == 0x80067e58u &&
        unknown_delay.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].word == 2 &&
        unknown_delay.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0f);

    Fixture unknown_team;
    unknown_team.put(Phase, 0x82, 2);
    unknown_team.put(Timer82, 0, 2);
    unknown_team.forget(Team, 2);
    check(unknown_team.run() == NBA97_TEXT_UNKNOWN &&
        unknown_team.progress.stopped_pc == 0x80067eccu &&
        unknown_team.get(Phase, 2) == 0);

    Fixture alignment;
    alignment.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {0x800ff001u, 0x0f};
    check(alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        alignment.progress.stopped_pc == 0x80067d44u);

    const std::uint32_t refuse_pc[] = {0, 0x80067dd8u, 0x80067df4u,
        0x80067dfcu, 0x80067e04u};
    const std::uint32_t refuse_entry[] = {0, 0x80029590u, 0x800295c8u,
        0x80062300u, 0x80062660u};
    for (std::size_t refusal = 1; refusal <= 4; ++refusal) {
        Fixture refused;
        refused.put(Shot, 0, 4);
        refused.refuse_call = refusal;
        check(refused.run() == NBA97_TEXT_IO_REFUSED &&
            refused.progress.callbacks_completed == refusal - 1 &&
            refused.progress.stopped_pc == refuse_pc[refusal] &&
            refused.progress.stopped_entry == refuse_entry[refusal]);
    }
    Fixture malformed_child;
    malformed_child.put(Shot, 0, 4);
    malformed_child.malform_call = 2;
    check(malformed_child.run() == NBA97_TEXT_ARGUMENT &&
        malformed_child.progress.callbacks_completed == 1 &&
        malformed_child.progress.stopped_pc == 0x80067df4u);

    Fixture missing;
    auto small_region = missing.region;
    small_region.size = 0x1000;
    missing.context.memory = {&small_region, 1};
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc == 0x80067d3cu &&
        missing.progress.stopped_address == Main);

    Fixture wrapped_sp;
    std::uint8_t low_bytes[32]{};
    std::uint8_t low_known[32];
    for (auto& byte : low_known) byte = 1;
    Nba97GameTextRegion wrapped_regions[2] = {
        wrapped_sp.region, {0, low_bytes, low_known, sizeof low_bytes}};
    wrapped_sp.context.memory = {wrapped_regions, 2};
    wrapped_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {0x10u, 0x0f};
    wrapped_sp.put(Main, 0, 4);
    check(wrapped_sp.run() == NBA97_TEXT_COMPLETE &&
        wrapped_sp.progress.frame_stack_pointer == 0xfffffff8u &&
        wrapped_sp.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == 0x10u);

    Fixture wrapped_pointer;
    std::uint8_t pointer_bytes[32]{};
    std::uint8_t pointer_known[32];
    for (auto& byte : pointer_known) byte = 1;
    pointer_bytes[8] = 0;
    pointer_bytes[9] = 48;
    Nba97GameTextRegion pointer_regions[2] = {
        wrapped_pointer.region,
        {0, pointer_bytes, pointer_known, sizeof pointer_bytes}};
    wrapped_pointer.context.memory = {pointer_regions, 2};
    wrapped_pointer.put(Shot, 0, 4);
    wrapped_pointer.put(Owner, 0xffff, 2);
    wrapped_pointer.put(BallPointer, 0xfffffff8u, 4);
    check(wrapped_pointer.run() == NBA97_TEXT_COMPLETE &&
        wrapped_pointer.progress.first_violation_triggered);

    Fixture baseline;
    baseline.put(Shot, 0, 4);
    check(baseline.run() == NBA97_TEXT_COMPLETE);
    auto operations = baseline.progress.operations;
    for (std::size_t budget = 0; budget < operations; ++budget) {
        Fixture limited;
        limited.put(Shot, 0, 4);
        limited.context.operation_budget = budget;
        check(limited.run() == NBA97_TEXT_LIMIT);
        check(limited.progress.operations == budget &&
            !limited.progress.completed);
    }

    Nba97GameClockViolationsProgress progress{};
    check(nba97_game_clock_violations(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_clock_violations(&baseline.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
    auto malformed = baseline.context;
    malformed.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
    check(nba97_game_clock_violations(&malformed, &progress) ==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    early_exit_and_preframe_order();
    first_sequence_and_ball_gates();
    phase_82_gates_and_wrapping();
    final_sequence_and_phase_gates();
    child_mutation_full_machine_and_independent_triggers();
    unknown_failures_and_budgets();
    std::printf("game clock violations: %u checks passed\n", checks);
    return 0;
}
