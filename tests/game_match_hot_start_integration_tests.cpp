#include "game_match_hot_start_adapter.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
constexpr std::uint32_t Base = 0x80000000u;
constexpr std::size_t Size = 0x120000u;
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct Fixture {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
    std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
    Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
    Nba97GameMatchHotStartContext hot{};
    Nba97GameMatchHotStartProgress hot_progress{};
    Nba97GameMatchHotStartTickAdapter adapter{};
    Nba97MatchTickContext tick{};
    Nba97MatchTickProgress tick_progress{};
    std::size_t hot_children = 0;

    Fixture() {
        hot.memory = {&region, 1};
        hot.operation_budget = 1000;
        hot.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            hot.registers.gpr[i] = {0x22000000u + i, 0x0f};
        hot.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x80010000u, 0x0f};
        hot.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x83456780u, 0x0f};
        hot.io = &Fixture::hotIo;
        hot.user = this;
        put32(0x80020becu, 0x80050000u);
        put32(0x80050000u, 0x80071000u);
        put32(0x80050020u, 0x80060000u);
        put8(0x80060009u, 0x4du);
        for (unsigned i = 0; i < 84; ++i) {
            put32(0x8001ec98u + i * 4u, 0);
            put32(0x800170c8u + i * 4u, 0);
        }
        adapter.hot_start_context = &hot;
        adapter.hot_start_progress = &hot_progress;
        tick.access = &Fixture::tickAccess;
        tick.service = &Fixture::tickService;
        tick.user = this;
        tick.operation_budget = 1000;
        tick.incoming_s6 = {0, 0};
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Base);
    }
    void put8(std::uint32_t address, std::uint8_t value) {
        bytes[offset(address)] = value;
    }
    void put32(std::uint32_t address, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i)
            bytes[offset(address) + i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
    static int hotIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameMatchHotStartEvent* event,
        Nba97GameMatchInitializeRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        ++f.hot_children;
        if (event->entry == 0x800a72bcu)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x80073000u, 0x0f};
        else if (event->pc == 0x80067088u)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x55aa55aau, 0x0f};
        return 1;
    }
    static int tickAccess(void*, std::uint32_t, std::uint32_t, unsigned,
        unsigned, Nba97PlayerFrameValue*) {
        check(false, "tick memory access must not precede its next service");
        return NBA97_BODY_ARGUMENT;
    }
    static int tickService(void* user, const Nba97MatchTickCall* call,
        Nba97GamePeriodValue* result) {
        auto& f = *static_cast<Fixture*>(user);
        return nba97_game_match_hot_start_dispatch_tick(&f.adapter, call, result);
    }
};

void naturalCallerReachesNextBoundary() {
    Fixture f;
    const int result = nba97_game_match_tick(&f.tick, &f.tick_progress);
    check(result == NBA97_MATCH_TICK_SERVICE_REQUIRED,
        "natural tick stops at the next unresolved service");
    check(f.tick_progress.stopped_pc == 0x80068c2cu &&
        f.tick_progress.stopped_entry == 0x80079664u,
        "completed hot-start boundary advances tick to exact next call");
    check(f.tick_progress.services == 1 &&
        f.tick_progress.outer_restarts == 1,
        "tick records one completed natural service");
    check(f.adapter.hot_start_invocations == 1 &&
        f.adapter.fallback_invocations == 0 &&
        f.adapter.hot_start_result == NBA97_TEXT_COMPLETE,
        "adapter owns only the exact natural hot-start event");
    check(f.hot_progress.completed && f.hot_progress.prefixes_written == 84 &&
        f.hot_progress.retry_attempts == 1 && f.hot_children == 3,
        "natural caller completes the full 72-instruction child");
    check(f.hot_progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
        0x55aa55aau,
        "natural composition retains final child v0");
}

void explicitEntryContextIsMandatory() {
    Nba97GameMatchHotStartTickAdapter adapter{};
    Nba97MatchTickCall call{0x80068c24u, 0x80066f88u, {0, 0}, 0};
    check(nba97_game_match_hot_start_dispatch_tick(&adapter, &call, nullptr) ==
        NBA97_BODY_ARGUMENT,
        "adapter refuses absent full register/stack entry context");
    check(adapter.hot_start_invocations == 0,
        "missing entry context never fabricates an invocation");
}

void incompleteChildStopsAtNaturalBoundary() {
    Fixture f;
    f.hot.operation_budget = 0;
    const int result = nba97_game_match_tick(&f.tick, &f.tick_progress);
    check(result == NBA97_MATCH_HOT_START_TICK_INCOMPLETE,
        "incomplete child is not reported as a successful tick service");
    check(f.adapter.hot_start_result == NBA97_TEXT_LIMIT &&
        f.adapter.hot_start_invocations == 1,
        "adapter retains exact owner failure and invocation count");
    check(f.tick_progress.stopped_pc == 0x80068c24u &&
        f.tick_progress.stopped_entry == 0x80066f88u &&
        f.tick_progress.services == 0,
        "natural caller remains stopped at the uncompleted boundary");
}

void nonTargetForwarding() {
    Fixture f;
    bool called = false;
    f.adapter.fallback_user = &called;
    f.adapter.fallback_service = [](void* user, const Nba97MatchTickCall* call,
        Nba97GamePeriodValue*) -> int {
        *static_cast<bool*>(user) = true;
        return call->entry == 0x80079664u ? NBA97_BODY_OK : NBA97_BODY_ARGUMENT;
    };
    Nba97MatchTickCall call{0x80068c2cu, 0x80079664u, {0, 0}, 1};
    check(nba97_game_match_hot_start_dispatch_tick(&f.adapter, &call, nullptr) ==
        NBA97_BODY_OK && called && f.adapter.fallback_invocations == 1,
        "non-target services forward without changing hot-start entry state");
}
}

int main() {
    naturalCallerReachesNextBoundary();
    explicitEntryContextIsMandatory();
    incompleteChildStopsAtNaturalBoundary();
    nonTargetForwarding();
    if (failures) {
        std::cerr << failures << " match hot-start integration checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "match tick to hot-start composition verified\n";
    return EXIT_SUCCESS;
}
