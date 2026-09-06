#include "game_clock_violations_adapter.h"
#include "game_match_clocks_adapter.h"

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
            "clock violations integration check %u failed at %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;

struct Composition {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97MatchTickContext tick{};
    Nba97MatchTickProgress tick_progress{};
    Nba97GameMatchClocksBinding clocks{};
    Nba97GameClockViolationsBinding violations{};
    std::vector<Nba97MatchTickCall> tick_calls;
    std::vector<Nba97GameClockViolationsEvent> violation_calls;
    Nba97MatchTickCall natural_violation_call{};

    Composition() {
        tick.access = access;
        tick.service = service;
        tick.player_update = player;
        tick.ball_simulation = ball;
        tick.net_transform = net;
        tick.match_frame = frame;
        tick.user = this;
        tick.operation_budget = 500;
        tick.incoming_s6 = {22, 1};

        clocks.memory = {&region, 1};
        clocks.operation_budget = 200;
        clocks.io = clocksIo;
        clocks.user = this;
        violations.memory = {&region, 1};
        violations.operation_budget = 200;
        violations.io = violationsIo;
        violations.user = this;

        put(0x8001edecu, 1, 2);
        put(0x800fdb92u, 2, 2);
        put(0x800fdb8au, 0, 2);
        put(0x80021d82u, 1, 1);
        put(0x800fdb7cu, 0, 2);
        put(0x800fe8ccu, 0, 2);
        put(0x800fe8c4u, 0, 2);
        put(0x800fdb68u, 5, 2);
        put(0x800fdb78u, 0, 1);
        put(0x800fdb6cu, 22, 2);

        put(0x800fdb58u, 7200, 4);
        put(0x800fdb90u, 0, 2);
        put(0x800fdba4u, 180, 4);
        put(0x80021d92u, 1, 1);
        put(0x8001eeb4u, 100, 2);
        put(0x8001eeb6u, 9, 2);
        put(0x8001ef78u, 100, 2);
        put(0x8001ef7au, 9, 2);
        put(0x800fdb86u, 1, 2);

        put(0x800fdba8u, 100, 2);
        put(0x800fdbaau, 100, 2);
        put(0x800fdbccu, 0, 2);
        put(0x800fe8e0u, 0, 2);
        put(0x80021d91u, 1, 1);
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
    void prepareClocks(const Nba97MatchTickCall& call) {
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            clocks.entry_machine.registers.gpr[i] =
                {0x21000000u + i * 0x01010101u, 0x0f};
        clocks.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] =
            {0, 0x0f};
        clocks.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            {call.args[0], 0x0f};
        /* This explicit root fixture supplies the live tick s0 needed by the
         * later 0x80068D60 move; it is never inferred from the legacy API. */
        clocks.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0] =
            {22, 0x0f};
        clocks.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {0x800ff000u, 0x0f};
        clocks.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {call.pc + 8u, 0x0f};
        clocks.entry_machine.hi = {0x12345678u, 0x0f};
        clocks.entry_machine.lo = {0x9abcdef0u, 0x0f};
        clocks.entry_machine_ready = 1;
    }
    void prepareViolations(const Nba97MatchTickCall& call) {
        /* Compose the complete U owner output, then model only the evidenced
         * intervening 68D60 MOVE and 68D64 JAL link state. */
        violations.entry_machine = clocks.progress.machine;
        violations.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            clocks.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0];
        violations.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {call.pc + 8u, 0x0f};
        violations.entry_machine_ready = 1;
    }
    static int access(void* user, std::uint32_t, std::uint32_t address,
        unsigned width, unsigned kind, Nba97PlayerFrameValue* value) {
        auto& c = *static_cast<Composition*>(user);
        if (address < Ram || std::uint64_t(address) + width >
                std::uint64_t(Ram) + c.bytes.size())
            return NBA97_BODY_BOUNDS;
        auto at = c.offset(address);
        if (kind == NBA97_FRAME_READ) {
            *value = {};
            for (unsigned i = 0; i < width; ++i)
                if (c.known[at + i]) {
                    value->word |= std::uint32_t(c.bytes[at + i]) <<
                        (8u * i);
                    value->known_mask = static_cast<std::uint8_t>(
                        value->known_mask | (1u << i));
                }
        } else {
            for (unsigned i = 0; i < width; ++i) {
                c.bytes[at + i] = static_cast<std::uint8_t>(
                    value->word >> (8u * i));
                c.known[at + i] = static_cast<std::uint8_t>(
                    (value->known_mask >> i) & 1u);
            }
        }
        return NBA97_BODY_OK;
    }
    static int clocksIo(void*, const Nba97GameTextMemory*,
        const Nba97GameMatchClocksEvent*, Nba97GameMatchClocksMachine*) {
        return 1;
    }
    static int violationsIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameClockViolationsEvent* event,
        Nba97GameClockViolationsMachine*) {
        static_cast<Composition*>(user)->violation_calls.push_back(*event);
        return 1;
    }
    static int service(void* user, const Nba97MatchTickCall* call,
        Nba97GamePeriodValue* result) {
        auto& c = *static_cast<Composition*>(user);
        c.tick_calls.push_back(*call);
        if (call->entry == 0x80067a60u) {
            c.prepareClocks(*call);
            return nba97_game_match_clocks_from_match_tick(
                &c.clocks, call, result);
        }
        if (call->entry == 0x80067d38u) {
            c.natural_violation_call = *call;
            c.prepareViolations(*call);
            return nba97_game_clock_violations_from_match_tick(
                &c.violations, call, result);
        }
        if (result)
            *result = {call->entry == 0x80067664u ? 1u : 0u, 1};
        return NBA97_BODY_OK;
    }
    static int player(void*, std::uint32_t) { return NBA97_BODY_OK; }
    static int ball(void*, std::uint32_t, std::uint32_t) {
        return NBA97_BODY_OK;
    }
    static int net(void*, std::uint32_t) { return NBA97_BODY_OK; }
    static int frame(void*, std::uint32_t) { return NBA97_BODY_OK; }
};

void actual_tick_68d64_chain() {
    Composition c;
    check(nba97_game_match_tick(&c.tick, &c.tick_progress) == NBA97_BODY_OK &&
        c.tick_progress.completed);
    check(c.clocks.invocations == 1 &&
        c.clocks.result == NBA97_TEXT_COMPLETE && c.clocks.progress.completed);
    check(c.violations.invocations == 1 &&
        c.violations.result == NBA97_TEXT_COMPLETE &&
        c.violations.progress.completed);
    check(c.natural_violation_call.pc == 0x80068d64u &&
        c.natural_violation_call.entry == 0x80067d38u &&
        c.natural_violation_call.count == 1 &&
        c.natural_violation_call.args[0] == 22 &&
        c.natural_violation_call.args[1] == 0);
    check(c.violations.entry_machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x80068d6cu &&
        c.violations.entry_machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_S0].word == 22 &&
        c.violations.entry_machine.hi.word ==
            c.clocks.progress.machine.hi.word);
    check(c.get(0x800fdb58u, 4) == 7178 &&
        c.get(0x800fdba4u, 4) == 158 &&
        c.get(0x800fdbaau, 2) == 78 &&
        c.get(0x8001eeb4u, 2) == 78 &&
        c.get(0x8001ef78u, 2) == 78);
    check(c.violation_calls.empty());
}

void adapter_guards_and_nested_prefix() {
    Composition c;
    Nba97MatchTickCall call{0x80068d64u, 0x80067d38u, {22, 0}, 1};
    c.prepareClocks({0x80068d58u, 0x80067a60u, {22, 0}, 1});
    c.clocks.progress.machine = c.clocks.entry_machine;
    c.prepareViolations(call);
    auto before = c.violations.entry_machine;
    call.pc ^= 4u;
    check(!nba97_game_clock_violations_from_match_tick(
        &c.violations, &call, nullptr) && c.violations.invocations == 0);
    call.pc ^= 4u;
    c.violations.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word =
        21;
    check(!nba97_game_clock_violations_from_match_tick(
        &c.violations, &call, nullptr) && c.violations.invocations == 0);
    c.violations.entry_machine = before;
    Nba97GamePeriodValue result{};
    check(!nba97_game_clock_violations_from_match_tick(
        &c.violations, &call, &result) && c.violations.invocations == 0);
    c.violations.entry_machine_ready = 0;
    check(!nba97_game_clock_violations_from_match_tick(
        &c.violations, &call, nullptr) && c.violations.invocations == 0);
    c.violations.entry_machine_ready = 1;
    call.args[1] = 1;
    check(!nba97_game_clock_violations_from_match_tick(
        &c.violations, &call, nullptr) && c.violations.invocations == 0);
    check(!nba97_game_clock_violations_from_match_tick(nullptr, &call,
        nullptr));

    Composition limited;
    limited.violations.operation_budget = 3;
    check(nba97_game_match_tick(&limited.tick, &limited.tick_progress) ==
        NBA97_BODY_ARGUMENT);
    check(!limited.tick_progress.completed &&
        limited.tick_progress.stopped_pc == 0x80068d64u &&
        limited.tick_progress.stopped_entry == 0x80067d38u &&
        limited.violations.result == NBA97_TEXT_LIMIT &&
        limited.violations.progress.operations == 3 &&
        limited.violations.progress.stopped_pc == 0x80067d58u);
}
}

int main() {
    actual_tick_68d64_chain();
    adapter_guards_and_nested_prefix();
    std::printf("game clock violations integration: %u checks passed\n",
        checks);
    return 0;
}
