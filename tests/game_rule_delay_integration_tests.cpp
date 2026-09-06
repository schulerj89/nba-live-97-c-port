#include "game_rule_delay_adapter.h"

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
            "game rule delay integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Main = 0x800fdb58u;
constexpr std::uint32_t Shot = 0x800fdba4u;
constexpr std::uint32_t Timer82 = 0x800fdba8u;
constexpr std::uint32_t TimerFinal = 0x800fdbaau;
constexpr std::uint32_t Phase = 0x800fdb90u;
constexpr std::uint32_t Owner = 0x800fdbccu;
constexpr std::uint32_t Team = 0x800fdb94u;
constexpr std::uint32_t State82 = 0x800fe884u;
constexpr std::uint32_t Block82 = 0x800fe88eu;
constexpr std::uint32_t BlockFinal = 0x800fe8e0u;
constexpr std::uint32_t Enable82 = 0x80021d90u;
constexpr std::uint32_t EnableFinal = 0x80021d91u;
constexpr std::uint32_t EnableShot = 0x80021d92u;
constexpr std::uint32_t Ball = 0x800e0000u;
constexpr std::uint32_t Actor = 0x800e1000u;

struct Composition {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameClockViolationsContext violations{};
    Nba97GameClockViolationsProgress violations_progress{};
    Nba97GameRuleDelayAdapterProgress adapter_progress{};
    std::vector<Nba97GameClockViolationsEvent> unresolved;
    bool chain_all_three = false;
    bool refuse_first = false;

    Composition(std::uint16_t team) {
        violations.memory = {&region, 1};
        violations.operation_budget = 1000;
        violations.io = io;
        violations.user = this;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            violations.machine.registers.gpr[i] = {
                0x31000000u + i * 0x01010101u,
                static_cast<std::uint8_t>((i % 15u) + 1u)};
        violations.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] =
            {0, 0x0f};
        violations.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            {1, 0x0f};
        violations.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {0x800ff000u, 0x0f};
        violations.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x80068d6cu, 0x0f};
        violations.machine.hi = {0x12345678u, 0x0f};
        violations.machine.lo = {0x9abcdef0u, 0x0f};

        put(Main, 1, 4);
        put(Shot, 0, 4);
        put(EnableShot, 1, 1);
        put(Enable82, 1, 1);
        put(EnableFinal, 1, 1);
        put(Owner, 0, 2);
        put(Team, team, 2);
        put(0x800fdc48u, Ball, 4);
        put(Ball + 0x10u, 48u << 8u, 4);
        put(Ball + 0x18u, 0, 2);
        put(0x800fdc34u, Actor, 4);
        put(Actor + 0xa0u, 0, 2);
        put(Phase, 0x80, 2);
        put(0x800fe882u, 0, 2);
        put(State82, 2, 2);
        put(Block82, 0, 2);
        put(BlockFinal, 0, 2);
        put(Timer82, 0, 2);
        put(TimerFinal, 0, 2);
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Ram);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width) {
        const auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
            known[at + i] = 1;
        }
    }
    int run() {
        return nba97_game_clock_violations_with_rule_delay(&violations,
            &violations_progress, &adapter_progress);
    }
    static int io(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameClockViolationsEvent* event,
        Nba97GameClockViolationsMachine* machine) {
        auto& c = *static_cast<Composition*>(opaque);
        if (!event)
            return 0;
        c.unresolved.push_back(*event);
        if (c.refuse_first && c.unresolved.size() == 1)
            return 0;
        if (event->kind == NBA97_GAME_CLOCK_VIOLATIONS_CHILD_80029590) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0] =
                {event->pc ^ 0x13579bdfu, 0x05};
            machine->hi = {event->pc ^ 0x2468ace0u, 0x0a};
            machine->lo = {event->pc ^ 0xfedcba98u, 0x03};
        }
        if (c.chain_all_three && c.unresolved.size() == 3) {
            c.put(Phase, 0x82, 2);
            c.put(Timer82, 0, 2);
        }
        return 1;
    }
};

void check_leaf_site(const Composition& c, unsigned site,
    std::uint32_t pc, std::uint32_t event_pc, std::uint32_t duration) {
    check(c.adapter_progress.site_invocations[site] == 1);
    const auto& event = c.adapter_progress.event[site];
    const auto& leaf = c.adapter_progress.rule[site];
    check(event.pc == pc && event.delay_slot_pc == pc + 4u &&
        event.entry == 0x800295c8u && event.argument_count == 1 &&
        event.operation != 0 && event.invocation == site + 1u);
    check(leaf.completed && leaf.operations == 0 && leaf.accesses == 0 &&
        leaf.reads == 0 && leaf.stores == 0);
    check(leaf.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            duration &&
        leaf.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask ==
            0x0f);
    check(leaf.return_address.word == pc + 8u &&
        leaf.return_address.known_mask == 0x0f);
    check(leaf.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0].word ==
            (event_pc ^ 0x13579bdfu) &&
        leaf.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0].known_mask ==
            0x05);
    check(leaf.machine.hi.word == (event_pc ^ 0x2468ace0u) &&
        leaf.machine.hi.known_mask == 0x0a &&
        leaf.machine.lo.word == (event_pc ^ 0xfedcba98u) &&
        leaf.machine.lo.known_mask == 0x03);
}

void actual_w_all_three_sites_and_team_variants() {
    for (const auto team : {std::uint16_t(0), std::uint16_t(1),
            std::uint16_t(0xffff)}) {
        Composition c(team);
        c.chain_all_three = true;
        check(c.run() == NBA97_TEXT_COMPLETE &&
            c.violations_progress.completed &&
            c.violations_progress.first_violation_triggered &&
            c.violations_progress.phase_82_violation_triggered &&
            c.violations_progress.final_violation_triggered);
        check(c.adapter_progress.rule_result == NBA97_TEXT_COMPLETE &&
            c.adapter_progress.invocations == 3 &&
            c.adapter_progress.unresolved_callbacks_completed == 9 &&
            c.unresolved.size() == 9);
        const std::uint32_t duration = team == 0 ? 5000u : 20000u;
        const std::uint32_t first_event = team == 0 ?
            0x80067dd8u : 0x80067de8u;
        const std::uint32_t phase_event = team == 0 ?
            0x80067ed4u : 0x80067ee4u;
        const std::uint32_t final_event = team == 0 ?
            0x80067fc0u : 0x80067fd0u;
        check_leaf_site(c, NBA97_GAME_RULE_DELAY_FIRST_VIOLATION,
            0x80067df4u, first_event, duration);
        check_leaf_site(c, NBA97_GAME_RULE_DELAY_PHASE_82_VIOLATION,
            0x80067ef0u, phase_event, duration);
        check_leaf_site(c, NBA97_GAME_RULE_DELAY_FINAL_VIOLATION,
            0x80067fdcu, final_event, duration);
        check(c.adapter_progress.duration_5000_invocations ==
                (team == 0 ? 3u : 0u) &&
            c.adapter_progress.duration_20000_invocations ==
                (team == 0 ? 0u : 3u));
        for (const auto& event : c.unresolved)
            check(event.kind !=
                NBA97_GAME_CLOCK_VIOLATIONS_CHILD_800295C8);
    }
}

Nba97GameClockViolationsEvent valid_event() {
    Nba97GameClockViolationsEvent event{};
    event.pc = 0x80067df4u;
    event.delay_slot_pc = 0x80067df8u;
    event.entry = 0x800295c8u;
    event.operation = 9;
    event.invocation = 1;
    event.kind = NBA97_GAME_CLOCK_VIOLATIONS_CHILD_800295C8;
    event.argument_count = 1;
    return event;
}

void adapter_guards_and_callback_refusal() {
    Composition base(0);
    auto event = valid_event();
    auto machine = base.violations.machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {5000, 0x0f};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {event.pc + 8u, 0x0f};
    const auto before = machine;
    Nba97GameRuleDelayProgress progress{};
    check(nba97_game_rule_delay_from_clock_violations(&event, &machine,
        &progress) == NBA97_TEXT_COMPLETE && progress.completed);
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        check(machine.registers.gpr[i].word == before.registers.gpr[i].word &&
            machine.registers.gpr[i].known_mask ==
                before.registers.gpr[i].known_mask);
    check(machine.hi.word == before.hi.word &&
        machine.hi.known_mask == before.hi.known_mask &&
        machine.lo.word == before.lo.word &&
        machine.lo.known_mask == before.lo.known_mask);

    for (const auto duration : {5000u, 20000u}) {
        auto accepted = before;
        accepted.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            {duration, 0x0f};
        check(nba97_game_rule_delay_from_clock_violations(&event, &accepted,
            &progress) == NBA97_TEXT_COMPLETE);
    }
    for (const auto duration : {0u, 4999u, 5001u, 19999u, 20001u,
            0xffffffffu}) {
        auto rejected = before;
        rejected.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            {duration, 0x0f};
        check(nba97_game_rule_delay_from_clock_violations(&event, &rejected,
            &progress) == NBA97_TEXT_ARGUMENT);
    }

    auto bad_event = event;
    bad_event.pc += 4u;
    check(nba97_game_rule_delay_from_clock_violations(&bad_event, &machine,
        &progress) == NBA97_TEXT_ARGUMENT);
    bad_event = event;
    bad_event.delay_slot_pc += 4u;
    check(nba97_game_rule_delay_from_clock_violations(&bad_event, &machine,
        &progress) == NBA97_TEXT_ARGUMENT);
    bad_event = event;
    bad_event.entry ^= 4u;
    check(nba97_game_rule_delay_from_clock_violations(&bad_event, &machine,
        &progress) == NBA97_TEXT_ARGUMENT);
    bad_event = event;
    bad_event.argument_count = 0;
    check(nba97_game_rule_delay_from_clock_violations(&bad_event, &machine,
        &progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_rule_delay_from_clock_violations(nullptr, &machine,
        &progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_rule_delay_from_clock_violations(&event, nullptr,
        &progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_rule_delay_from_clock_violations(&event, &machine,
        nullptr) == NBA97_TEXT_ARGUMENT);

    Composition refused(0);
    refused.refuse_first = true;
    check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        !refused.violations_progress.completed &&
        refused.violations_progress.stopped_pc == 0x80067dd8u &&
        refused.adapter_progress.invocations == 0 &&
        refused.adapter_progress.unresolved_callbacks_completed == 0);

    Nba97GameClockViolationsProgress parent_progress{};
    Nba97GameRuleDelayAdapterProgress adapter_progress{};
    check(nba97_game_clock_violations_with_rule_delay(nullptr,
        &parent_progress, &adapter_progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_clock_violations_with_rule_delay(&base.violations,
        nullptr, &adapter_progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_clock_violations_with_rule_delay(&base.violations,
        &parent_progress, nullptr) == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    actual_w_all_three_sites_and_team_variants();
    adapter_guards_and_callback_refusal();
    std::printf("%u game rule delay integration checks passed\n", checks);
    return 0;
}
