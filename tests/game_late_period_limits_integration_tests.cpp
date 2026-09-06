#include "game_late_period_limits_adapter.h"

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
            "game late-period-limits integration check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr int NextBoundary = -77;

struct Fixture {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    std::array<Nba97GameLatePeriodLimitsAccess, 16> journal{};
    Nba97GameLatePeriodLimitsContext limits{};
    Nba97GameLatePeriodLimitsTickBinding binding{};
    Nba97MatchTickProgress tick_progress{};
    std::vector<Nba97MatchTickCall> fallback_calls;

    Fixture() {
        limits.memory = {&region, 1};
        limits.operation_budget = journal.size();
        limits.access_journal = journal.data();
        limits.access_journal_capacity = journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            limits.registers.gpr[i] = {0x62000000u + i, 0x0f};
        limits.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        /* This is independent source-proven child state. No field is derived
           from the legacy Nba97MatchTickCall service record. */
        limits.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
            0x800ff800u, 0x0f};
        limits.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x80068cf4u, 0x0f};
        binding.limits = &limits;
        binding.entry_context_source_proven = 1;
        binding.fallback_service = fallback;
        binding.fallback_user = this;
        put(0x800fdb58u, 0, 4);
        put(0x800fdb68u, 3, 2);
        put(0x8010606cu, 0x7777u, 2);
        put(0x8001ee24u, 0, 2);
        put(0x8001eee8u, 0, 2);
        put(0x8001edecu, 0, 2);
        put(0x800fdb92u, 2, 2);
        put(0x800fdb8au, 0, 2);
    }

    void put(std::uint32_t address, std::uint32_t value, unsigned width) {
        const auto at = static_cast<std::size_t>(address - Ram);
        for (unsigned i = 0; i < width; ++i)
            bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
    }

    std::uint32_t get(std::uint32_t address, unsigned width) const {
        const auto at = static_cast<std::size_t>(address - Ram);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[at + i]) << (i * 8u);
        return value;
    }

    static int fallback(void* user, const Nba97MatchTickCall* call,
        Nba97GamePeriodValue*) {
        auto& f = *static_cast<Fixture*>(user);
        f.fallback_calls.push_back(*call);
        if (call->entry == 0x80066f88u || call->entry == 0x80079664u ||
            call->entry == 0x80067468u)
            return NBA97_BODY_OK;
        if (call->pc == 0x80068cf4u && call->entry == 0x800675e4u)
            return NextBoundary;
        return NBA97_BODY_ARGUMENT;
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
            f.known[at + i] = static_cast<std::uint8_t>(
                (value->known_mask >> i) & 1u);
        }
        return NBA97_BODY_OK;
    }
};

void adapter_contract_and_failures() {
    Fixture f;
    Nba97MatchTickCall exact{0x80068cecu, 0x80067550u, {0, 0}, 0};
    check(nba97_game_late_period_limits_from_match_tick(
        &f.binding, &exact, nullptr) == NBA97_BODY_OK);
    check(f.binding.invocations == 1 &&
        f.binding.owner_result == NBA97_TEXT_COMPLETE &&
        f.binding.progress.completed &&
        f.binding.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x800ff800u &&
        f.binding.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80068cf4u);

    Fixture absent;
    absent.binding.entry_context_source_proven = 0;
    check(nba97_game_late_period_limits_from_match_tick(
        &absent.binding, &exact, nullptr) ==
        NBA97_GAME_LATE_PERIOD_LIMITS_TICK_CONTEXT_REQUIRED);
    check(absent.binding.invocations == 0);

    Fixture bounded;
    bounded.limits.operation_budget = 2;
    check(nba97_game_late_period_limits_from_match_tick(
        &bounded.binding, &exact, nullptr) == NBA97_BODY_JOURNAL_LIMIT &&
        bounded.binding.owner_result == NBA97_TEXT_LIMIT &&
        bounded.binding.progress.stopped_pc == 0x80067570u &&
        bounded.get(0x8010606cu, 2) == 0u);

    Fixture unknown;
    const auto clock_at = static_cast<std::size_t>(0x800fdb58u - Ram);
    unknown.known[clock_at + 3u] = 0;
    check(nba97_game_late_period_limits_from_match_tick(
        &unknown.binding, &exact, nullptr) == NBA97_BODY_UNKNOWN &&
        unknown.binding.owner_result == NBA97_TEXT_UNKNOWN &&
        unknown.binding.progress.stopped_pc == 0x80067564u &&
        unknown.get(0x8010606cu, 2) == 0u);

    Fixture malformed;
    Nba97MatchTickCall bad = exact;
    bad.pc = 0x80068ce8u;
    check(nba97_game_late_period_limits_from_match_tick(
        &malformed.binding, &bad, nullptr) == NBA97_BODY_ARGUMENT);
    bad = exact;
    bad.count = 1;
    check(nba97_game_late_period_limits_from_match_tick(
        &malformed.binding, &bad, nullptr) == NBA97_BODY_ARGUMENT);
    Nba97GamePeriodValue result{};
    check(nba97_game_late_period_limits_from_match_tick(
        &malformed.binding, &exact, &result) == NBA97_BODY_ARGUMENT);
    check(nba97_game_late_period_limits_from_match_tick(
        nullptr, &exact, nullptr) == NBA97_BODY_ARGUMENT);
    check(nba97_game_late_period_limits_from_match_tick(
        &malformed.binding, nullptr, nullptr) == NBA97_BODY_ARGUMENT);
}

void natural_match_tick_composition() {
    Fixture f;
    Nba97MatchTickContext tick{};
    tick.access = Fixture::access;
    tick.service = nba97_game_late_period_limits_from_match_tick;
    tick.user = &f.binding;
    tick.operation_budget = 100;
    tick.incoming_s6 = {0, 0};

    /* The tick's memory callback needs the fixture while its service callback
       needs the binding, so point fallback storage at the enclosing fixture
       and use a small access bridge through that same binding. */
    struct AccessBridge {
        static int access(void* user, std::uint32_t pc,
            std::uint32_t address, unsigned width, unsigned kind,
            Nba97PlayerFrameValue* value) {
            auto* binding =
                static_cast<Nba97GameLatePeriodLimitsTickBinding*>(user);
            return Fixture::access(binding->fallback_user, pc, address, width,
                kind, value);
        }
    };
    tick.access = AccessBridge::access;
    const int result = nba97_game_match_tick(&tick, &f.tick_progress);
    check(result == NextBoundary && !f.tick_progress.completed);
    check(f.binding.invocations == 1 &&
        f.binding.owner_result == NBA97_TEXT_COMPLETE &&
        f.binding.progress.completed && f.binding.progress.operations == 9);
    check(f.get(0x8010606cu, 2) == 5u &&
        f.get(0x8001ee24u, 2) == 3u &&
        f.get(0x8001eee8u, 2) == 3u);
    check(f.fallback_calls.size() == 4 &&
        f.fallback_calls[0].pc == 0x80068c24u &&
        f.fallback_calls[0].entry == 0x80066f88u &&
        f.fallback_calls[1].pc == 0x80068c2cu &&
        f.fallback_calls[1].entry == 0x80079664u &&
        f.fallback_calls[2].pc == 0x80068c4cu &&
        f.fallback_calls[2].entry == 0x80067468u &&
        f.fallback_calls[3].pc == 0x80068cf4u &&
        f.fallback_calls[3].entry == 0x800675e4u &&
        f.fallback_calls[3].count == 0u &&
        f.fallback_calls[3].args[0] == 0u &&
        f.fallback_calls[3].args[1] == 0u);
    check(f.tick_progress.operations == 14 &&
        f.tick_progress.services == 4 &&
        f.tick_progress.stopped_pc == 0x80068cf4u &&
        f.tick_progress.stopped_entry == 0x800675e4u);
}
}

int main() {
    adapter_contract_and_failures();
    natural_match_tick_composition();
    std::printf("game late-period-limits integration: %u checks passed\n",
        checks);
    return 0;
}
