#include "game_period_expiry_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "period expiry integration check %u failed at %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Ball = 0x80013000u;

struct Composition {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000, 0);
    std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x110000, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97MatchTickContext tick{};
    Nba97MatchTickProgress tick_progress{};
    Nba97GamePeriodExpiryBinding expiry{};
    std::vector<Nba97MatchTickCall> calls;
    Nba97MatchTickCall natural{};
    bool prepare_machine{true};
    bool child_seen{};

    Composition() {
        tick.access = access;
        tick.service = service;
        tick.player_update = player;
        tick.ball_simulation = ball;
        tick.net_transform = net;
        tick.match_frame = frame;
        tick.user = this;
        tick.operation_budget = 500;
        tick.incoming_s6 = {2, 1};
        expiry.memory = {&region, 1};
        expiry.operation_budget = 100;
        expiry.io = child;
        expiry.user = this;

        put(0x8001edecu, 1, 2);
        put(0x800fdb92u, 2, 2);
        put(0x800fdb8au, 1, 2);
        put(0x80021d82u, 1, 1);
        put(0x800fdb7cu, 0, 2);
        put(0x800fe8ccu, 0, 2);
        put(0x800fe8c4u, 0, 2);
        put(0x800fdb68u, 5, 2);
        put(0x800fdb78u, 1, 1);
        put(0x800fdb6cu, 1, 2);

        put(0x800fdb58u, 0, 4);
        put(0x800fdbccu, 0xffffu, 2);
        put(0x800fdc34u, 0x80012000u, 4);
        put(0x800fdc48u, Ball, 4);
        put(Ball + 0x10u, 48u << 8u, 4);
        put(Ball + 0x18u, 0, 2);
        put(0x800fa034u, 0xffffffffu, 4);
        put(0x800fdb90u, 0x82, 2);
        put(0x800fe882u, 0, 2);
        put(0x80021d95u, 1, 1);
        put(0x800fdb76u, 0, 2);
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
    void prepare(const Nba97MatchTickCall& call) {
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            expiry.entry_machine.registers.gpr[i] =
                {0x41000000u + i * 0x01010101u, 0x0f};
        expiry.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] =
            {0, 0x0f};
        expiry.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {0x800ff000u, 0x0f};
        expiry.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {call.pc + 8u, 0x0f};
        expiry.entry_machine.hi = {0x12345678u, 0x0f};
        expiry.entry_machine.lo = {0x9abcdef0u, 0x0f};
        expiry.entry_machine_ready = 1;
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
                    value->word |= std::uint32_t(c.bytes[at + i]) << (8u * i);
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
    static int child(void* user, const Nba97GameTextMemory*,
        const Nba97GamePeriodExpiryEvent*, Nba97GamePeriodExpiryMachine*) {
        static_cast<Composition*>(user)->child_seen = true;
        return 1;
    }
    static int service(void* user, const Nba97MatchTickCall* call,
        Nba97GamePeriodValue* result) {
        auto& c = *static_cast<Composition*>(user);
        c.calls.push_back(*call);
        if (call->entry == 0x80067664u) {
            c.natural = *call;
            if (c.prepare_machine) c.prepare(*call);
            return nba97_game_period_expiry_from_match_tick(
                &c.expiry, call, result);
        }
        if (result) *result = {0, 1};
        return NBA97_BODY_OK;
    }
    static int player(void*, std::uint32_t) { return NBA97_BODY_OK; }
    static int ball(void*, std::uint32_t, std::uint32_t) {
        return NBA97_BODY_OK;
    }
    static int net(void*, std::uint32_t) { return NBA97_BODY_OK; }
    static int frame(void*, std::uint32_t) { return NBA97_BODY_OK; }
    int run() { return nba97_game_match_tick(&tick, &tick_progress); }
};

void natural_tick_call() {
    Composition c;
    check(c.run() == NBA97_BODY_OK && c.tick_progress.completed);
    check(c.expiry.result == NBA97_TEXT_COMPLETE && c.expiry.progress.completed &&
        c.expiry.invocations == 1);
    check(c.natural.pc == 0x80068d6cu && c.natural.entry == 0x80067664u &&
        c.natural.count == 0 && c.natural.args[0] == 0 && c.natural.args[1] == 0);
    check(c.expiry.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        0x80068d74u);
    check(c.expiry.progress.frame_stack_pointer == 0x800fefe0u &&
        c.expiry.progress.restored_return_address.word == 0x80068d74u &&
        c.expiry.progress.machine.hi.word == 0x12345678u &&
        c.expiry.progress.machine.lo.word == 0x9abcdef0u);
    check(c.get(0x800fdb76u, 2) == 0xffffu &&
        c.expiry.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 1);
    check(!c.child_seen); /* owner<0 keeps 0x800582DC explicit and unreached. */
    bool saw_previous = false;
    for (const auto& call : c.calls)
        if (call.pc == 0x80068d64u && call.entry == 0x80067d38u)
            saw_previous = true;
    check(saw_previous);
}

void adapter_validation_and_nested_failure() {
    Nba97GamePeriodValue result{};
    Nba97MatchTickCall call{0x80068d6cu, 0x80067664u, {0, 0}, 0};
    Nba97GamePeriodExpiryBinding binding{};
    check(!nba97_game_period_expiry_from_match_tick(&binding, &call, &result) &&
        binding.result == NBA97_TEXT_ARGUMENT);

    Composition missing;
    missing.prepare_machine = false;
    check(missing.run() != NBA97_BODY_OK &&
        missing.expiry.result == NBA97_TEXT_ARGUMENT &&
        missing.tick_progress.stopped_pc == 0x80068d6cu);

    Composition limited;
    limited.expiry.operation_budget = 0;
    check(limited.run() != NBA97_BODY_OK &&
        limited.expiry.result == NBA97_TEXT_LIMIT &&
        limited.expiry.progress.stopped_pc == 0x80067668u &&
        limited.tick_progress.stopped_pc == 0x80068d6cu);

    Composition base;
    base.prepare(call);
    auto wrong = call;
    wrong.pc = 0x80068d64u;
    check(!nba97_game_period_expiry_from_match_tick(
        &base.expiry, &wrong, &result));
    wrong = call;
    wrong.entry = 0x800582dcu;
    check(!nba97_game_period_expiry_from_match_tick(
        &base.expiry, &wrong, &result));
}
}

int main() {
    natural_tick_call();
    adapter_validation_and_nested_failure();
    std::printf("period expiry integration tests passed: %u checks\n", checks);
}
