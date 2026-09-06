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
            "match clocks integration check %u failed at %u\n", checks,
            line);
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
    std::vector<Nba97MatchTickCall> tick_calls;
    std::vector<Nba97GameMatchClocksEvent> clock_calls;
    Nba97MatchTickCall natural_call{};
    bool alternate_path{};

    Composition() {
        tick.access = access;
        tick.service = service;
        tick.player_update = player;
        tick.ball_simulation = ball;
        tick.net_transform = net;
        tick.match_frame = frame;
        tick.user = this;
        tick.operation_budget = 500;
        tick.incoming_s6 = {0xfffffffeu, 1};
        clocks.memory = {&region, 1};
        clocks.operation_budget = 100;
        clocks.io = clockIo;
        clocks.user = this;

        put(0x8001edecu, 1, 2);
        put(0x800fdb92u, 2, 2);
        put(0x800fdb8au, 1, 2);
        put(0x80021d82u, 1, 1);
        put(0x800fdb7cu, 0, 2);
        put(0x800fe8ccu, 0, 2);
        put(0x800fe8c4u, 0, 2);
        put(0x800fdb68u, 5, 2);
        put(0x800fdb78u, 0, 1);
        put(0x800fdb6cu, 7, 2);

        put(0x800fdb58u, 60, 4);
        put(0x800fdb90u, 0, 2);
        put(0x800fdba4u, 0, 4);
        put(0x80021d92u, 1, 1);
        put(0x8001eeb4u, 5, 2);
        put(0x8001eeb6u, 9, 2);
        put(0x8001ef78u, 5, 2);
        put(0x8001ef7au, 9, 2);
        put(0x800fdb86u, 1, 2);
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
    void prepareEntry(const Nba97MatchTickCall& call) {
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            clocks.entry_machine.registers.gpr[i] =
                {0x31000000u + i * 0x01010101u, 0x0f};
        clocks.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] =
            {0, 0x0f};
        clocks.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            {call.args[0], 0x0f};
        clocks.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {0x800ff000u, 0x0f};
        clocks.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {call.pc + 8u, 0x0f};
        clocks.entry_machine.hi = {0x12345678u, 0x0f};
        clocks.entry_machine.lo = {0x9abcdef0u, 0x0f};
        clocks.entry_machine_ready = 1;
    }
    static int access(void* user, std::uint32_t, std::uint32_t address,
        unsigned width, unsigned kind, Nba97PlayerFrameValue* value) {
        auto& c = *static_cast<Composition*>(user);
        if (address < Ram || std::uint64_t(address) + width >
                std::uint64_t(Ram) + c.bytes.size())
            return NBA97_BODY_BOUNDS;
        const auto at = c.offset(address);
        if (kind == NBA97_FRAME_READ) {
            *value = {};
            for (unsigned i = 0; i < width; ++i)
                if (c.known[at + i]) {
                    value->word |= std::uint32_t(c.bytes[at + i]) <<
                        (i * 8u);
                    value->known_mask = static_cast<std::uint8_t>(
                        value->known_mask | (1u << i));
                }
        } else {
            for (unsigned i = 0; i < width; ++i) {
                c.bytes[at + i] = static_cast<std::uint8_t>(
                    value->word >> (i * 8u));
                c.known[at + i] = static_cast<std::uint8_t>(
                    (value->known_mask >> i) & 1u);
            }
        }
        return NBA97_BODY_OK;
    }
    static int clockIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameMatchClocksEvent* event,
        Nba97GameMatchClocksMachine*) {
        auto& c = *static_cast<Composition*>(user);
        c.clock_calls.push_back(*event);
        return 1;
    }
    static int service(void* user, const Nba97MatchTickCall* call,
        Nba97GamePeriodValue* result) {
        auto& c = *static_cast<Composition*>(user);
        c.tick_calls.push_back(*call);
        if (call->entry == 0x80067a60u) {
            c.natural_call = *call;
            c.prepareEntry(*call);
            return nba97_game_match_clocks_from_match_tick(
                &c.clocks, call, result);
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
    int run() {
        put(0x800fdb8au, alternate_path ? 0 : 1, 2);
        return nba97_game_match_tick(&tick, &tick_progress);
    }
};

void actual_tick_call_sites() {
    Composition live_delta;
    check(live_delta.run() == NBA97_BODY_OK &&
        live_delta.tick_progress.completed &&
        live_delta.clocks.result == NBA97_TEXT_COMPLETE &&
        live_delta.clocks.progress.completed &&
        live_delta.clocks.invocations == 1);
    check(live_delta.natural_call.pc == 0x80068d40u &&
        live_delta.natural_call.entry == 0x80067a60u &&
        live_delta.natural_call.count == 1 &&
        live_delta.natural_call.args[0] == 1);
    check(live_delta.clocks.entry_machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x80068d48u &&
        live_delta.clocks.entry_machine.hi.word == 0x12345678u &&
        live_delta.clocks.progress.frame_stack_pointer == 0x800fefd0u &&
        live_delta.clocks.progress.restored_return_address.word ==
            0x80068d48u);
    check(live_delta.get(0x800fdb58u, 4) == 59 &&
        live_delta.get(0x8001eeb4u, 2) == 4 &&
        live_delta.get(0x8001ef78u, 2) == 4 &&
        live_delta.get(0x800fdb86u, 2) == 0);

    Composition carried_s6;
    carried_s6.alternate_path = true;
    check(carried_s6.run() == NBA97_BODY_OK &&
        carried_s6.natural_call.pc == 0x80068d58u &&
        carried_s6.natural_call.args[0] == 0xfffffffeu &&
        carried_s6.clocks.progress.restored_return_address.word ==
            0x80068d60u);
    check(carried_s6.get(0x800fdb58u, 4) == 62 &&
        carried_s6.get(0x8001eeb4u, 2) == 7 &&
        carried_s6.get(0x8001ef78u, 2) == 7);
}

void nested_prefix_and_explicit_machine_guards() {
    Composition limited;
    limited.clocks.operation_budget = 5;
    check(limited.run() == NBA97_BODY_ARGUMENT &&
        !limited.tick_progress.completed &&
        limited.tick_progress.stopped_pc == 0x80068d40u &&
        limited.tick_progress.stopped_entry == 0x80067a60u &&
        limited.clocks.result == NBA97_TEXT_LIMIT &&
        limited.clocks.progress.operations == 5 &&
        limited.clocks.progress.stopped_pc == 0x80067a88u);

    Composition guards;
    Nba97MatchTickCall call{0x80068d40u, 0x80067a60u, {1, 0}, 1};
    guards.prepareEntry(call);
    auto before = guards.clocks.entry_machine;
    call.pc ^= 4u;
    check(!nba97_game_match_clocks_from_match_tick(
        &guards.clocks, &call, nullptr) &&
        guards.clocks.result == NBA97_TEXT_ARGUMENT &&
        guards.clocks.invocations == 0);
    call.pc ^= 4u;
    guards.clocks.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word =
        2;
    check(!nba97_game_match_clocks_from_match_tick(
        &guards.clocks, &call, nullptr) && guards.clocks.invocations == 0);
    guards.clocks.entry_machine = before;
    Nba97GamePeriodValue result{};
    check(!nba97_game_match_clocks_from_match_tick(
        &guards.clocks, &call, &result) && guards.clocks.invocations == 0);
    guards.clocks.entry_machine_ready = 0;
    check(!nba97_game_match_clocks_from_match_tick(
        &guards.clocks, &call, nullptr) && guards.clocks.invocations == 0);
    guards.clocks.entry_machine_ready = 2;
    check(!nba97_game_match_clocks_from_match_tick(
        &guards.clocks, &call, nullptr) && guards.clocks.invocations == 0);
    guards.clocks.entry_machine_ready = 1;
    call.args[1] = 1;
    check(!nba97_game_match_clocks_from_match_tick(
        &guards.clocks, &call, nullptr) && guards.clocks.invocations == 0);
    check(!nba97_game_match_clocks_from_match_tick(
        nullptr, &call, nullptr));
}
}

int main() {
    actual_tick_call_sites();
    nested_prefix_and_explicit_machine_guards();
    std::printf("game match clocks integration: %u checks passed\n", checks);
    return 0;
}
