#include "game_period_startup_adapter.h"

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
        std::fprintf(stderr,
            "game period-startup integration check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x800ff800u;
constexpr int NextServiceRequired = -44;

struct Fixture {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x100000u, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x100000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GamePeriodStartupContext period{};
    Nba97GamePeriodStartupMatchTickContext adapter_context{};
    Nba97GamePeriodStartupProgress period_progress{};
    Nba97GamePeriodStartupAdapterProgress adapter_progress{};
    Nba97MatchTickProgress tick_progress{};
    std::vector<Nba97MatchTickCall> tick_calls;
    std::vector<Nba97GamePeriodStartupEvent> period_calls;
    bool refuse_period_child = false;

    Fixture() {
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            period.registers.gpr[i] = {0x22000000u + i, 0x0f};
        period.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        /* This complete child context is supplied independently. It is not
           reconstructed from Nba97MatchTickCall, which exposes no SP/GPRs. */
        period.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        period.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80068c54u, 0x0f};
        period.memory = {&region, 1};
        period.operation_budget = 100;
        period.io = periodIo;
        period.user = this;
        adapter_context.period = &period;
        adapter_context.entry_context_source_proven = 1;
        put(0x800fdb68u, 0, 2);
        put(0x80020c14u, 0x800fed00u, 4);
        put(0x8001edecu, 99, 2);
        put(0x800fdb78u, 0, 1);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width,
        std::uint8_t mask = 0x0f) {
        const auto at = static_cast<std::size_t>(address - Ram);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            known[at + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        const auto at = static_cast<std::size_t>(address - Ram);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[at + i]) << (i * 8u);
        return value;
    }
    static int periodIo(void* user, const Nba97GameTextMemory*,
        const Nba97GamePeriodStartupEvent* event,
        Nba97GamePeriodStartupRegisters*) {
        auto& f = *static_cast<Fixture*>(user);
        f.period_calls.push_back(*event);
        return f.refuse_period_child ? 0 : 1;
    }
    static int access(void* user, std::uint32_t, std::uint32_t address,
        unsigned width, unsigned kind, Nba97PlayerFrameValue* value) {
        auto& f = *static_cast<Fixture*>(user);
        if (!value || !width || width > 4 || address < Ram ||
            static_cast<std::uint64_t>(address - Ram) + width > f.bytes.size() ||
            kind > NBA97_FRAME_WRITE_POINTER)
            return NBA97_BODY_ARGUMENT;
        const auto at = static_cast<std::size_t>(address - Ram);
        if (kind == NBA97_FRAME_READ) {
            *value = {};
            for (unsigned i = 0; i < width; ++i)
                if (f.known[at + i]) {
                    value->word |= std::uint32_t(f.bytes[at + i]) << (i * 8u);
                    value->known_mask = static_cast<std::uint8_t>(
                        value->known_mask | (1u << i));
                }
            return NBA97_BODY_OK;
        }
        if (value->is_reference)
            return NBA97_BODY_REFERENCE_REQUIRED;
        for (unsigned i = 0; i < width; ++i) {
            f.bytes[at + i] =
                static_cast<std::uint8_t>(value->word >> (i * 8u));
            f.known[at + i] =
                static_cast<std::uint8_t>((value->known_mask >> i) & 1u);
        }
        return NBA97_BODY_OK;
    }
    static int service(void* user, const Nba97MatchTickCall* call,
        Nba97GamePeriodValue*) {
        auto& f = *static_cast<Fixture*>(user);
        f.tick_calls.push_back(*call);
        if (call->entry == 0x80066f88u || call->entry == 0x80079664u)
            return NBA97_BODY_OK;
        if (call->entry == 0x80067468u)
            return nba97_game_period_startup_from_match_tick(call,
                &f.adapter_context, &f.period_progress, &f.adapter_progress);
        return NextServiceRequired;
    }
};

void direct_adapter_contract() {
    Fixture f;
    Nba97MatchTickCall call{0x80068c4cu, 0x80067468u, {0, 0}, 0};
    check(nba97_game_period_startup_from_match_tick(&call,
        &f.adapter_context, &f.period_progress, &f.adapter_progress) ==
        NBA97_BODY_OK);
    check(f.adapter_progress.invocations == 1 &&
        f.adapter_progress.source_context_used &&
        f.adapter_progress.owner_result == NBA97_TEXT_COMPLETE &&
        f.period_progress.completed && f.period_calls.size() == 14);

    Fixture absent;
    absent.adapter_context.entry_context_source_proven = 0;
    check(nba97_game_period_startup_from_match_tick(&call,
        &absent.adapter_context, &absent.period_progress,
        &absent.adapter_progress) ==
        NBA97_GAME_PERIOD_STARTUP_TICK_CONTEXT_REQUIRED);
    check(absent.adapter_progress.invocations == 0 &&
        !absent.adapter_progress.source_context_used &&
        absent.period_calls.empty());

    Fixture malformed;
    call.pc = 0x80068c48u;
    check(nba97_game_period_startup_from_match_tick(&call,
        &malformed.adapter_context, &malformed.period_progress,
        &malformed.adapter_progress) == NBA97_BODY_ARGUMENT);
    check(nba97_game_period_startup_from_match_tick(nullptr,
        &malformed.adapter_context, &malformed.period_progress,
        &malformed.adapter_progress) == NBA97_BODY_ARGUMENT);

    Fixture unknown;
    Nba97MatchTickCall valid{0x80068c4cu, 0x80067468u, {0, 0}, 0};
    unknown.put(0x800fdb68u, 0, 2, 0);
    check(nba97_game_period_startup_from_match_tick(&valid,
        &unknown.adapter_context, &unknown.period_progress,
        &unknown.adapter_progress) == NBA97_BODY_UNKNOWN &&
        unknown.adapter_progress.owner_result == NBA97_TEXT_UNKNOWN &&
        unknown.period_progress.stopped_pc == 0x8006748cu);

    Fixture refused;
    refused.refuse_period_child = true;
    check(nba97_game_period_startup_from_match_tick(&valid,
        &refused.adapter_context, &refused.period_progress,
        &refused.adapter_progress) ==
            NBA97_GAME_PERIOD_STARTUP_TICK_CHILD_REQUIRED &&
        refused.adapter_progress.owner_result == NBA97_TEXT_IO_REFUSED &&
        refused.period_progress.stopped_entry == 0x80065db0u);
}

void natural_match_tick_composition() {
    Fixture f;
    Nba97MatchTickContext tick{};
    tick.access = Fixture::access;
    tick.service = Fixture::service;
    tick.user = &f;
    tick.operation_budget = 100;
    tick.incoming_s6 = {0, 0};
    const int result = nba97_game_match_tick(&tick, &f.tick_progress);
    check(result == NextServiceRequired && !f.tick_progress.completed);
    check(f.tick_calls.size() == 4 &&
        f.tick_calls[0].pc == 0x80068c24u &&
        f.tick_calls[0].entry == 0x80066f88u &&
        f.tick_calls[1].pc == 0x80068c2cu &&
        f.tick_calls[1].entry == 0x80079664u &&
        f.tick_calls[2].pc == 0x80068c4cu &&
        f.tick_calls[2].entry == 0x80067468u &&
        f.tick_calls[3].pc == 0x800691bcu &&
        f.tick_calls[3].entry == 0x80067930u);
    check(f.adapter_progress.invocations == 1 &&
        f.adapter_progress.source_context_used &&
        f.adapter_progress.owner_result == NBA97_TEXT_COMPLETE &&
        f.period_progress.completed && f.period_progress.operations == 24);
    check(f.period_calls.size() == 14 &&
        f.period_calls.front().entry == 0x80065db0u &&
        f.period_calls.back().entry == 0x80035678u);
    check(f.get(0x800fdb92u, 2) == 1u &&
        f.get(0x800fdc48u, 4) == 0x800fed00u &&
        f.get(0x800fdb6cu, 2) == 1u &&
        f.get(0x800fdb78u, 1) == 1u);
    check(f.tick_progress.operations == 9 && f.tick_progress.services == 3 &&
        f.tick_progress.stopped_pc == 0x800691bcu &&
        f.tick_progress.stopped_entry == 0x80067930u);
}
}

int main() {
    direct_adapter_contract();
    natural_match_tick_composition();
    std::printf("game period-startup integration: %u checks passed\n", checks);
}
