#include "game_loop_entry_adapter.h"

#include <algorithm>
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
        std::fprintf(stderr, "game loop-entry integration check %u failed\n",
            checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ffe00u;
constexpr std::uint32_t EntrySp = 0x807fff00u;

struct AdapterFixture {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x100000, 0);
    std::vector<std::uint8_t> ram_known =
        std::vector<std::uint8_t>(0x100000, 1);
    std::array<std::uint8_t, 0x200> stack{}, stack_known{};
    Nba97GameTextRegion regions[2] = {
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}
    };
    Nba97GameLoopEntryContext context{};
    Nba97GameLoopEntryMatchTickServices services{};
    Nba97GameLoopEntryProgress progress{};
    Nba97GameLoopEntryAdapterProgress adapter{};
    std::vector<Nba97MatchTickCall> calls;

    AdapterFixture() {
        stack.fill(0);
        stack_known.fill(1);
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {0x20000000u + i, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x10203040u, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 6] =
            {0xaaaafffdu, 0x03};
        context.memory = {regions, 2};
        context.operation_budget = 3;
        services.service = service;
        services.user = this;
        services.operation_budget = 100;
        put(0x8001edecu, 99, 2);
        put(0x800fdb78u, 0, 1);
        put(0x800fdb68u, 5, 2);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width) {
        const auto at = address - Ram;
        for (unsigned i = 0; i < width; ++i) {
            ram[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            ram_known[at + i] = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        const auto at = address - Ram;
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(ram[at + i]) << (i * 8u);
        return value;
    }
    static int service(void* user, const Nba97MatchTickCall* call,
        Nba97GamePeriodValue*) {
        auto& f = *static_cast<AdapterFixture*>(user);
        f.calls.push_back(*call);
        return NBA97_BODY_OK;
    }
    int run() {
        return nba97_game_loop_entry_with_match_tick(&context, &services,
            &progress, &adapter);
    }
};

void exact_non_resumable_tick_prefix() {
    AdapterFixture bounded;
    bounded.services.operation_budget = 0;
    check(bounded.run() == NBA97_TEXT_IO_REFUSED && !bounded.progress.completed);
    check(bounded.adapter.match_tick_invocations == 1 &&
        bounded.adapter.match_tick_result == NBA97_BODY_JOURNAL_LIMIT &&
        !bounded.adapter.match_tick.completed &&
        bounded.adapter.match_tick.operations == 0 &&
        bounded.adapter.match_tick.stopped_pc == 0x80068c24u &&
        bounded.adapter.match_tick.stopped_entry == 0x80066f88u);
    check(bounded.progress.operations == 2 && bounded.progress.stores == 1 &&
        bounded.progress.stopped_pc == 0x8002dc40u &&
        bounded.progress.stopped_entry == 0x80068bf8u &&
        !bounded.progress.callbacks_completed);
    check(bounded.progress.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask ==
        0x0f &&
        bounded.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask == 0 &&
        bounded.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 0 &&
        bounded.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].known_mask == 0);

    AdapterFixture missing;
    missing.services.service = nullptr;
    check(missing.run() == NBA97_TEXT_IO_REFUSED &&
        missing.adapter.match_tick_result ==
            NBA97_MATCH_TICK_SERVICE_REQUIRED &&
        missing.adapter.match_tick.operations == 1 &&
        missing.adapter.match_tick.stopped_pc == 0x80068c24u &&
        missing.adapter.match_tick.stopped_entry == 0x80066f88u);
}

struct SessionComposition {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x100000, 0);
    std::vector<std::uint8_t> ram_known =
        std::vector<std::uint8_t>(0x100000, 1);
    std::array<std::uint8_t, 0x200> stack{}, stack_known{};
    Nba97GameTextRegion regions[2] = {
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}
    };
    Nba97GameMatchSessionContext session{};
    Nba97GameMatchSessionProgress session_progress{};
    Nba97GameLoopEntryProgress loop_progress{};
    Nba97GameLoopEntryAdapterProgress adapter_progress{};
    std::vector<Nba97GameMatchSessionEvent> calls;
    std::size_t loop_calls = 0;

    SessionComposition() {
        stack.fill(0);
        stack_known.fill(1);
        session = {{regions, 2}, 200, EntrySp, 0x80029ae4u,
            {0xa0a0a0a0u, 0xb1b1b1b1u, 0xc2c2c2c2u}, 0x800d79c8u,
            sessionIo, this};
        put(0x8001ec94u, 0);
        put(0x80021d74u, 7);
    }
    void put(std::uint32_t address, std::uint32_t value) {
        const auto at = address - Ram;
        for (unsigned i = 0; i < 4; ++i) {
            ram[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            ram_known[at + i] = 1;
        }
    }
    static int sessionIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameMatchSessionEvent* event,
        Nba97GameMatchSessionValue* value) {
        auto& c = *static_cast<SessionComposition*>(user);
        c.calls.push_back(*event);
        *value = {0, 1};
        if (event->kind != NBA97_GAME_MATCH_SESSION_RUN_LOOP)
            return 1;
        ++c.loop_calls;
        Nba97GameMatchInitializeRegisters registers{};
        if (nba97_game_loop_entry_registers_from_session(event, &registers) !=
            NBA97_TEXT_COMPLETE)
            return 0;
        Nba97GameLoopEntryContext loop{*memory, 3, registers,
            nullptr, nullptr, nullptr, 0};
        Nba97GameLoopEntryMatchTickServices services{};
        services.operation_budget = 100;
        return nba97_game_loop_entry_with_match_tick(&loop, &services,
            &c.loop_progress, &c.adapter_progress) == NBA97_TEXT_COMPLETE;
    }
    int run() { return nba97_game_match_session(&session, &session_progress); }
};

void natural_match_session_boundary() {
    SessionComposition c;
    check(c.run() == NBA97_TEXT_IO_REFUSED && !c.session_progress.completed);
    check(c.loop_calls == 1 && c.calls.size() == 9 &&
        c.calls.back().kind == NBA97_GAME_MATCH_SESSION_RUN_LOOP &&
        c.calls.back().pc == 0x8002da8cu &&
        c.calls.back().entry == 0x8002dc38u);
    check(c.session_progress.stopped_pc == 0x8002da8cu &&
        c.session_progress.stopped_entry == 0x8002dc38u &&
        c.session_progress.callbacks_completed == 8);
    check(c.loop_progress.frame_stack_pointer == EntrySp - 0x28u - 0x18u &&
        c.loop_progress.stopped_pc == 0x8002dc40u &&
        c.loop_progress.stopped_entry == 0x80068bf8u &&
        c.adapter_progress.match_tick_result ==
            NBA97_MATCH_TICK_SERVICE_REQUIRED &&
        c.adapter_progress.match_tick.stopped_pc == 0x80068c24u &&
        c.adapter_progress.match_tick.stopped_entry == 0x80066f88u);
}

void session_register_validation() {
    Nba97GameMatchInitializeRegisters registers{};
    Nba97GameMatchSessionEvent event{};
    check(nba97_game_loop_entry_registers_from_session(nullptr, &registers) ==
        NBA97_TEXT_ARGUMENT);
    event.kind = NBA97_GAME_MATCH_SESSION_RUN_LOOP;
    event.pc = 0x8002da8cu;
    event.entry = 0x8002dc38u;
    event.stack_pointer = 0x807fff00u;
    event.global_pointer = 0x800d79c8u;
    event.return_address = 0x8002da94u;
    event.saved_register[0] = 1;
    event.saved_register[1] = 2;
    event.saved_register[2] = 3;
    check(nba97_game_loop_entry_registers_from_session(&event, &registers) ==
        NBA97_TEXT_COMPLETE);
    check(registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == event.stack_pointer &&
        registers.gpr[NBA97_MATCH_INITIALIZE_GP].word == event.global_pointer &&
        registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == event.return_address &&
        registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2].word == 3 &&
        registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 0);
}
}

int main() {
    exact_non_resumable_tick_prefix();
    natural_match_session_boundary();
    session_register_validation();
    std::printf("game_loop_entry integration: %u checks passed\n", checks);
}
