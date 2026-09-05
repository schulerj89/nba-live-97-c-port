#include "game_match_initialize_adapter.h"

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
        std::fprintf(stderr, "match-initialize integration check %u failed\n",
            checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ffe00u;
constexpr std::uint32_t SessionEntrySp = 0x807fffd0u;
constexpr std::uint32_t SessionFrameSp = SessionEntrySp - 0x28u;
constexpr std::uint32_t MatchState = 0x800fdb4cu;

struct Composition {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x100000, 0xcd);
    std::vector<std::uint8_t> ram_known =
        std::vector<std::uint8_t>(0x100000, 1);
    std::array<std::uint8_t, 0x200> stack{};
    std::array<std::uint8_t, 0x200> stack_known{};
    Nba97GameTextRegion regions[2] = {
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}
    };
    Nba97GameMatchSessionContext session{};
    Nba97GameMatchSessionProgress session_progress{};
    Nba97GameMatchInitializeProgress initialize_progress{};
    Nba97GameMatchInitializeAdapterProgress adapter_progress{};
    std::size_t zero_budget = 1000;
    std::size_t initialize_calls = 0;
    std::size_t initialize_children = 0;
    std::vector<Nba97GameMatchSessionEvent> session_calls;

    Composition() {
        stack.fill(0xcd);
        stack_known.fill(1);
        session = {{regions, 2}, 200, SessionEntrySp, 0x80029ae4u,
            {0xa0a0a0a0u, 0xb1b1b1b1u, 0xc2c2c2c2u}, 0x800d79c8u,
            sessionIo, this};
        put(0x8001ec94u, 0);
        put(0x80021d74u, 7);
        put(0x80021d78u, 11);
        put(0x80020c18u, 0xffffffffu);
        std::fill(ram.begin() + (MatchState - Ram),
            ram.begin() + (MatchState - Ram) + 0xe7cu,
            static_cast<std::uint8_t>(0x6a));
    }
    std::uint8_t* byte(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base &&
                std::uint64_t(address - region.base) < region.size)
                return region.data + (address - region.base);
        return nullptr;
    }
    void put(std::uint32_t address, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) {
            *byte(address + i) = static_cast<std::uint8_t>(value >> (i * 8u));
            ram_known[address - Ram + i] = 1;
        }
    }
    std::uint32_t get(std::uint32_t address) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(*byte(address + i)) << (i * 8u);
        return value;
    }
    static int initializeIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameMatchInitializeEvent* event,
        Nba97GameMatchInitializeRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        ++c.initialize_children;
        registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
            0x90000000u + static_cast<std::uint32_t>(c.initialize_children),
            0x0f};
        if (event->entry == 0x800763f4u)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0xcafebabeu, 0x0f};
        return 1;
    }
    static int sessionIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameMatchSessionEvent* event,
        Nba97GameMatchSessionValue* value) {
        auto& c = *static_cast<Composition*>(user);
        c.session_calls.push_back(*event);
        *value = {0, 1};
        if (event->kind == NBA97_GAME_MATCH_SESSION_INITIALIZE) {
            ++c.initialize_calls;
            Nba97GameMatchInitializeRegisters registers{};
            if (nba97_game_match_initialize_registers_from_session(event,
                    &registers) != NBA97_TEXT_COMPLETE)
                return 0;
            Nba97GameMatchInitializeContext context{*memory, 19, registers,
                initializeIo, &c, nullptr, 0};
            if (nba97_game_match_initialize_with_zero(&context, c.zero_budget,
                    &c.initialize_progress, &c.adapter_progress) !=
                NBA97_TEXT_COMPLETE)
                return 0;
            *value = {c.initialize_progress.registers
                    .gpr[NBA97_MATCH_INITIALIZE_V0].word,
                static_cast<std::uint8_t>(c.initialize_progress.registers
                    .gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0f)};
        }
        return 1;
    }
    int run() { return nba97_game_match_session(&session, &session_progress); }
};

void natural_match_session_composition() {
    Composition c;
    check(c.run() == NBA97_TEXT_COMPLETE && c.session_progress.completed);
    check(c.initialize_calls == 1 && c.initialize_children == 11 &&
        c.initialize_progress.completed &&
        c.adapter_progress.memory_zero_invocations == 1 &&
        c.adapter_progress.unresolved_callbacks_completed == 11);
    check(c.session_calls.size() == 23 &&
        c.session_calls[6].pc == 0x8002da7cu &&
        c.session_calls[6].entry == 0x8002db90u &&
        c.session_calls[7].entry == 0x8002db68u &&
        c.session_calls[8].entry == 0x8002dc38u &&
        c.session_calls[9].entry == 0x8002dc58u);
    check(c.initialize_progress.frame_stack_pointer == SessionFrameSp - 0x18u &&
        c.initialize_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            SessionFrameSp &&
        c.initialize_progress.restored_return_address.word == 0x8002da84u &&
        c.initialize_progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0xcafebabeu);
    check(c.get(0x80022084u) == 7 && c.get(0x80022adcu) == 11 &&
        c.get(0x80020c18u) == 0);
    for (std::uint32_t i = 0; i < 0xe7cu; ++i)
        check(*c.byte(MatchState + i) == 0);
}

void natural_child_failure_prefix() {
    Composition c;
    c.zero_budget = 8;
    check(c.run() == NBA97_TEXT_IO_REFUSED && !c.session_progress.completed);
    check(c.initialize_calls == 1 && c.initialize_children == 0 &&
        c.adapter_progress.memory_zero_result == NBA97_TEXT_LIMIT &&
        c.adapter_progress.memory_zero.operations == 8 &&
        c.initialize_progress.operations == 6 &&
        c.initialize_progress.callbacks_completed == 0);
    check(c.session_calls.size() == 7 &&
        c.session_progress.stopped_pc == 0x8002da7cu &&
        c.session_progress.stopped_entry == 0x8002db90u);
    bool changed = false;
    for (std::uint32_t i = 0; i < 0xe7cu; ++i)
        changed = changed || *c.byte(MatchState + i) == 0;
    check(changed && c.get(0x80022084u) == 7 && c.get(0x80022adcu) == 11);
}
}

int main() {
    natural_match_session_composition();
    natural_child_failure_prefix();
    std::printf("%u natural match-initialize composition checks passed\n", checks);
    return 0;
}
