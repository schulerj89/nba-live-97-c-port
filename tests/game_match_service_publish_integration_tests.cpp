#include "game_match_service_publish_adapter.h"

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
            "match service publish integration check %u failed at %u\n",
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
    Nba97GameMatchServicePublishBinding publish{};
    std::vector<Nba97MatchTickCall> tick_calls;
    Nba97MatchTickCall natural_call{};
    Nba97GameMatchServicePublishEvent child_event{};
    Nba97GameMatchServicePublishMachine child_entry{};
    unsigned child_calls{};
    unsigned player_calls{};
    unsigned ball_calls{};
    unsigned frame_calls{};

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
        publish.memory = {&region, 1};
        publish.operation_budget = 7;
        publish.io = child;
        publish.user = this;

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
        put(0x800fdc48u, 0x80022000u, 4);
        put(0x800f9ffeu, 0xbeefu, 2);
        put(0x800fdb90u, 0xff80u, 2);
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
            publish.entry_machine.registers.gpr[i] =
                {0x41000000u + i * 0x00010101u,
                    static_cast<std::uint8_t>((i % 15u) + 1u)};
        publish.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] =
            {0, 0x0f};
        publish.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {0x800ff000u, 0x0f};
        publish.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {call.pc + 8u, 0x0f};
        publish.entry_machine.hi = {0x12345678u, 3};
        publish.entry_machine.lo = {0x9abcdef0u, 0x0c};
        publish.entry_machine_ready = 1;
    }
    static int access(void* opaque, std::uint32_t, std::uint32_t address,
        unsigned width, unsigned kind, Nba97PlayerFrameValue* value) {
        auto& c = *static_cast<Composition*>(opaque);
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
    static int child(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameMatchServicePublishEvent* event,
        Nba97GameMatchServicePublishMachine* machine) {
        auto& c = *static_cast<Composition*>(opaque);
        ++c.child_calls;
        c.child_event = *event;
        c.child_entry = *machine;
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
            {0x13579bdfu, 7};
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_V1] =
            {0x2468ace0u, 0x0b};
        return 1;
    }
    static int service(void* opaque, const Nba97MatchTickCall* call,
        Nba97GamePeriodValue* result) {
        auto& c = *static_cast<Composition*>(opaque);
        c.tick_calls.push_back(*call);
        if (call->entry == 0x8002de34u) {
            c.natural_call = *call;
            c.prepareEntry(*call);
            return nba97_game_match_service_publish_from_match_tick(
                &c.publish, call, result);
        }
        if (result)
            *result = {0, 1};
        return NBA97_BODY_OK;
    }
    static int player(void* opaque, std::uint32_t) {
        ++static_cast<Composition*>(opaque)->player_calls;
        return NBA97_BODY_OK;
    }
    static int ball(void* opaque, std::uint32_t, std::uint32_t pointer) {
        auto& c = *static_cast<Composition*>(opaque);
        ++c.ball_calls;
        return pointer == 0x80022000u ? NBA97_BODY_OK : NBA97_BODY_ARGUMENT;
    }
    static int net(void*, std::uint32_t) { return NBA97_BODY_OK; }
    static int frame(void* opaque, std::uint32_t) {
        auto& c = *static_cast<Composition*>(opaque);
        ++c.frame_calls;
        c.put(0x800fdb78u, 1, 1);
        return NBA97_BODY_OK;
    }
    int run() {
        return nba97_game_match_tick(&tick, &tick_progress);
    }
};

void actual_natural_tick_call() {
    Composition c;
    check(c.run() == NBA97_BODY_OK && c.tick_progress.completed);
    check(c.natural_call.pc == 0x80068d7cu &&
        c.natural_call.entry == 0x8002de34u &&
        c.natural_call.count == 0 && c.natural_call.args[0] == 0 &&
        c.natural_call.args[1] == 0);
    check(c.publish.invocations == 1 &&
        c.publish.result == NBA97_TEXT_COMPLETE &&
        c.publish.progress.completed && c.child_calls == 1);
    check(c.child_event.pc == 0x8002de5cu &&
        c.child_event.entry == 0x8002a264u &&
        c.child_event.argument_count == 0 &&
        c.child_entry.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8002de64u &&
        c.child_entry.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x800fefe8u);
    check(c.get(0x80015028u, 2) == 0xbeefu &&
        c.get(0x800170bcu, 4) == 0xffffff80u &&
        c.publish.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].word == 0x13579bdfu &&
        c.publish.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V1].known_mask == 0x0b);
    check(c.publish.progress.machine.hi.word == 0x12345678u &&
        c.publish.progress.machine.hi.known_mask == 3 &&
        c.publish.progress.machine.lo.word == 0x9abcdef0u &&
        c.publish.progress.machine.lo.known_mask == 0x0c &&
        c.player_calls == 1 && c.ball_calls == 1 && c.frame_calls == 1);
}

void explicit_machine_and_nested_prefix() {
    Composition absent;
    absent.publish.io = nullptr;
    check(absent.run() == NBA97_BODY_ARGUMENT &&
        absent.tick_progress.stopped_pc == 0x80068d7cu &&
        absent.tick_progress.stopped_entry == 0x8002de34u &&
        absent.publish.result == NBA97_TEXT_IO_REFUSED &&
        absent.publish.invocations == 1 && absent.publish.progress.stores == 3);

    Composition guards;
    Nba97MatchTickCall call{0x80068d7cu, 0x8002de34u, {0, 0}, 0};
    guards.prepareEntry(call);
    auto before = guards.publish.entry_machine;
    call.pc ^= 4u;
    check(!nba97_game_match_service_publish_from_match_tick(
        &guards.publish, &call, nullptr) && guards.publish.invocations == 0);
    call.pc ^= 4u;
    call.entry ^= 4u;
    check(!nba97_game_match_service_publish_from_match_tick(
        &guards.publish, &call, nullptr) && guards.publish.invocations == 0);
    call.entry ^= 4u;
    call.count = 1;
    check(!nba97_game_match_service_publish_from_match_tick(
        &guards.publish, &call, nullptr));
    call.count = 0;
    call.args[0] = 1;
    check(!nba97_game_match_service_publish_from_match_tick(
        &guards.publish, &call, nullptr));
    call.args[0] = 0;
    Nba97GamePeriodValue result{};
    check(!nba97_game_match_service_publish_from_match_tick(
        &guards.publish, &call, &result));
    guards.publish.entry_machine = before;
    guards.publish.entry_machine_ready = 0;
    check(!nba97_game_match_service_publish_from_match_tick(
        &guards.publish, &call, nullptr));
    guards.publish.entry_machine_ready = 1;
    guards.publish.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = 7;
    check(!nba97_game_match_service_publish_from_match_tick(
        &guards.publish, &call, nullptr));
    guards.publish.entry_machine = before;
    guards.publish.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .word ^= 4u;
    check(!nba97_game_match_service_publish_from_match_tick(
        &guards.publish, &call, nullptr));
    check(!nba97_game_match_service_publish_from_match_tick(
        nullptr, &call, nullptr));
}
}

int main() {
    actual_natural_tick_call();
    explicit_machine_and_nested_prefix();
    std::printf("game match service publish integration: %u checks passed\n",
        checks);
    return 0;
}
