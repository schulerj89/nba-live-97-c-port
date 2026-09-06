#include "game_first_period_startup_adapter.h"

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
            "game first-period startup integration check %u failed\n",
            checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x800ff800u;

struct Composition {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x100000u, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x100000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GamePeriodStartupContext period{};
    Nba97GamePeriodStartupProgress period_progress{};
    Nba97GameFirstPeriodStartupBinding first{};
    std::vector<Nba97GamePeriodStartupEvent> period_calls;
    std::vector<Nba97GameFirstPeriodStartupEvent> first_calls;
    unsigned refuse_first{};
    Nba97GamePeriodStartupEvent parent_event{};
    Nba97GamePeriodStartupRegisters parent_entry_registers{};

    explicit Composition(std::uint8_t presentation_flag = 1) {
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            period.registers.gpr[i] = {0x22000000u + i, 0x0f};
        period.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        period.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        period.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x80068c54u, 0x0f};
        period.registers.gpr[NBA97_MATCH_INITIALIZE_S0] =
            {0x55667788u, 0x0f};
        period.memory = {&region, 1};
        period.operation_budget = 100;
        period.io = periodIo;
        period.user = this;
        first.operation_budget = 100;
        first.io = firstIo;
        first.user = this;
        put(0x800fdb68u, 0, 2);
        put(0x80020c14u, 0x800fed00u, 4);
        put(0x8001edecu, 0, 2);
        put(0x800eb680u, presentation_flag, 1);
        put(0x800fdb4eu, 0xbeefu, 2);
        put(0x800fdb94u, 0x1234u, 2);
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
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        const auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[at + i]) << (8u * i);
        return value;
    }
    static int firstIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameFirstPeriodStartupEvent* event,
        Nba97GameFirstPeriodStartupRegisters* registers) {
        auto& f = *static_cast<Composition*>(user);
        f.first_calls.push_back(*event);
        if (f.refuse_first == f.first_calls.size())
            return 0;
        if (event->entry == 0x8007ef4cu)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] =
                {0xcafebabeu, 0x05};
        return 1;
    }
    static int periodIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GamePeriodStartupEvent* event,
        Nba97GamePeriodStartupRegisters* registers) {
        auto& f = *static_cast<Composition*>(user);
        f.period_calls.push_back(*event);
        if (event->kind == NBA97_GAME_PERIOD_STARTUP_ZERO_PERIOD_SERVICE) {
            f.parent_event = *event;
            f.parent_entry_registers = *registers;
            return nba97_game_first_period_startup_from_period_startup(
                &f.first, memory, event, registers);
        }
        return 1;
    }
    int run() {
        return nba97_game_period_startup(&period, &period_progress);
    }
};

void natural_period_startup_composition() {
    Composition f(255);
    check(f.run() == NBA97_TEXT_COMPLETE && f.period_progress.completed);
    check(f.period_calls.size() == 13 &&
        f.parent_event.pc == 0x80067494u &&
        f.parent_event.delay_slot_pc == 0x80067498u &&
        f.parent_event.entry == 0x800673f0u &&
        f.parent_event.kind == NBA97_GAME_PERIOD_STARTUP_ZERO_PERIOD_SERVICE &&
        f.parent_event.argument_count == 0);
    check(f.parent_entry_registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        0x8006749cu &&
        f.parent_entry_registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp - 0x18u);
    check(f.first.invocations == 1 &&
        f.first.result == NBA97_TEXT_COMPLETE &&
        f.first.progress.completed && f.first.progress.operations == 12 &&
        f.first.progress.callbacks_completed == 7 &&
        f.first.progress.frame_stack_pointer == EntrySp - 0x30u &&
        f.first.progress.restored_return_address.word == 0x8006749cu);
    const std::array<std::uint32_t, 7> entries = {
        0x800295d0u, 0x8002a244u, 0x8002dd84u, 0x8002ddccu,
        0x8002a254u, 0x80065db0u, 0x8007ef4cu
    };
    check(f.first_calls.size() == entries.size());
    for (std::size_t i = 0; i < entries.size(); ++i)
        check(f.first_calls[i].entry == entries[i]);
    check(f.first_calls[4].pc == 0x80067434u &&
        f.first_calls[4].argument_count == 1);
    check(f.get(0x800fdb4eu, 2) == 0 &&
        f.get(0x800fdb94u, 2) == 0xffffu &&
        f.get(0x800fdb92u, 2) == 1u &&
        f.get(0x800fdc48u, 4) == 0x800fed00u &&
        f.get(0x800fdb6cu, 2) == 1u);
    check(f.period_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        EntrySp &&
        f.period_progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80068c54u);
}

void zero_flag_composition_and_prefix_failure() {
    Composition skipped(0);
    check(skipped.run() == NBA97_TEXT_COMPLETE &&
        skipped.first.progress.completed &&
        skipped.first.progress.operations == 9 &&
        skipped.first_calls.size() == 5 &&
        skipped.get(0x800fdb4eu, 2) == 0xbeefu &&
        skipped.get(0x800fdb94u, 2) == 0xffffu);

    Composition limited(1);
    limited.first.operation_budget = 4;
    check(limited.run() == NBA97_TEXT_IO_REFUSED &&
        !limited.period_progress.completed &&
        limited.period_progress.stopped_pc == 0x80067494u &&
        limited.period_progress.stopped_entry == 0x800673f0u &&
        limited.first.result == NBA97_TEXT_LIMIT &&
        limited.first.progress.operations == 4 &&
        limited.first.progress.stopped_pc == 0x8006741cu &&
        limited.first.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            1u &&
        limited.period_progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80067424u);

    Composition refused(1);
    refused.refuse_first = 3;
    check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.first.result == NBA97_TEXT_IO_REFUSED &&
        refused.first.progress.callbacks_completed == 2 &&
        refused.first.progress.stopped_pc == 0x8006741cu &&
        refused.first.progress.stopped_entry == 0x8002dd84u &&
        refused.period_progress.stopped_pc == 0x80067494u);
}

void adapter_guards() {
    Composition f;
    Nba97GamePeriodStartupEvent event{};
    event.pc = 0x80067494u;
    event.delay_slot_pc = 0x80067498u;
    event.entry = 0x800673f0u;
    event.kind = NBA97_GAME_PERIOD_STARTUP_ZERO_PERIOD_SERVICE;
    auto registers = f.period.registers;
    const auto before = registers;
    event.pc ^= 4u;
    check(!nba97_game_first_period_startup_from_period_startup(
        &f.first, &f.period.memory, &event, &registers));
    check(f.first.result == NBA97_TEXT_ARGUMENT && f.first.invocations == 0);
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        check(registers.gpr[i].word == before.gpr[i].word &&
            registers.gpr[i].known_mask == before.gpr[i].known_mask);

    event.pc ^= 4u;
    f.first.access_journal = nullptr;
    f.first.access_journal_capacity = 1;
    check(!nba97_game_first_period_startup_from_period_startup(
        &f.first, &f.period.memory, &event, &registers) &&
        f.first.result == NBA97_TEXT_ARGUMENT && f.first.invocations == 0);
    check(!nba97_game_first_period_startup_from_period_startup(
        nullptr, &f.period.memory, &event, &registers));
}
}

int main() {
    natural_period_startup_composition();
    zero_flag_composition_and_prefix_failure();
    adapter_guards();
    std::printf("game first-period startup integration: %u checks passed\n",
        checks);
}
