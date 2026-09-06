#include "game_tipoff_announcement_adapter.h"

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
            "tip-off announcement integration check %u failed at %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x800ff800u;
constexpr std::uint32_t CallerRa = 0x8006749cu;

struct Composition {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameFirstPeriodStartupContext first{};
    Nba97GameFirstPeriodStartupProgress first_progress{};
    Nba97GameTipoffAnnouncementBinding tipoff{};
    std::vector<Nba97GameFirstPeriodStartupEvent> first_calls;
    std::vector<Nba97GameTipoffAnnouncementEvent> tipoff_calls;
    Nba97GameFirstPeriodStartupEvent natural_event{};
    Nba97GameFirstPeriodStartupRegisters natural_registers{};
    std::uint32_t gate = 8;
    std::uint8_t mode = 2;

    Composition() {
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            first.registers.gpr[i] = {
                0x41000000u + i * 0x01010101u, 0x0f};
        first.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        first.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        first.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {CallerRa, 0x0f};
        first.memory = {&region, 1};
        first.operation_budget = 100;
        first.io = firstIo;
        first.user = this;
        tipoff.operation_budget = 100;
        tipoff.io = tipoffIo;
        tipoff.user = this;
        put(0x800eb680u, 0, 1);
        put(0x800fdb94u, 0x1234u, 2);
        put(0x80021d70u, mode, 1);
        put(0x80021d74u, 0x11112222u, 4);
        put(0x80021d78u, 0x33334444u, 4);
        put(0x8001ec94u, 1, 4);
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
    static int tipoffIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameTipoffAnnouncementEvent* event,
        Nba97GameTipoffAnnouncementRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.tipoff_calls.push_back(*event);
        switch (event->pc) {
        case 0x8007ef5cu:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {c.gate, 0x0f};
            break;
        case 0x8007ef8cu:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {20u, 0x0f};
            break;
        case 0x8007ef98u:
        case 0x8007efbcu:
        case 0x8007efd0u:
        case 0x8007efdcu:
        case 0x8007efe8u:
        case 0x8007f02cu:
        case 0x8007f038u:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] =
                {event->pc & 0xffu, 0x0f};
            break;
        case 0x8007f050u:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] =
                {0xabcdef01u, 0x03};
            registers->gpr[NBA97_MATCH_INITIALIZE_T8] =
                {0x2468ace0u, 0x09};
            break;
        }
        return 1;
    }
    static int firstIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameFirstPeriodStartupEvent* event,
        Nba97GameFirstPeriodStartupRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.first_calls.push_back(*event);
        if (event->kind == NBA97_GAME_FIRST_PERIOD_STARTUP_7EF4C) {
            c.natural_event = *event;
            c.natural_registers = *registers;
            c.put(0x80021d70u, c.mode, 1);
            return nba97_game_tipoff_announcement_from_first_period_startup(
                &c.tipoff, memory, event, registers);
        }
        return 1;
    }
    int run() {
        return nba97_game_first_period_startup(&first, &first_progress);
    }
};

void natural_actual_67450_gate_and_branches() {
    Composition mode_two;
    check(mode_two.run() == NBA97_TEXT_COMPLETE &&
        mode_two.first_progress.completed && mode_two.tipoff.progress.completed);
    check(mode_two.natural_event.pc == 0x80067450u &&
        mode_two.natural_event.delay_slot_pc == 0x80067454u &&
        mode_two.natural_event.entry == 0x8007ef4cu &&
        mode_two.natural_event.kind ==
            NBA97_GAME_FIRST_PERIOD_STARTUP_7EF4C &&
        mode_two.natural_event.argument_count == 0);
    check(mode_two.natural_registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        0x80067458u &&
        mode_two.natural_registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp - 0x18u);
    check(mode_two.tipoff.invocations == 1 &&
        mode_two.tipoff.result == NBA97_TEXT_COMPLETE &&
        mode_two.tipoff.progress.mode_path == 2 &&
        mode_two.tipoff.progress.operations == 23 &&
        mode_two.tipoff.progress.frame_stack_pointer == EntrySp - 0x38u &&
        mode_two.tipoff.progress.restored_return_address.word == 0x80067458u);
    check(mode_two.tipoff_calls.size() == 12 &&
        mode_two.first_progress.restored_return_address.word == CallerRa &&
        mode_two.first_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp &&
        mode_two.first_progress.registers.gpr[NBA97_MATCH_INITIALIZE_T8].word ==
            0x2468ace0u &&
        mode_two.first_progress.registers.gpr[NBA97_MATCH_INITIALIZE_T8]
            .known_mask == 0x09);

    Composition early;
    early.gate = 7;
    early.mode = 255;
    check(early.run() == NBA97_TEXT_COMPLETE &&
        early.tipoff.progress.completed && early.tipoff_calls.size() == 1 &&
        early.tipoff.progress.operations == 9 &&
        early.tipoff.progress.gate.word == 1 &&
        early.tipoff.progress.mode.known_mask == 0);

    Composition mode_one;
    mode_one.mode = 1;
    check(mode_one.run() == NBA97_TEXT_COMPLETE &&
        mode_one.tipoff.progress.mode_path == 1 &&
        mode_one.tipoff_calls.size() == 6 &&
        mode_one.tipoff.progress.operations == 16);
}

void nested_failure_prefix_and_adapter_guards() {
    Composition limited;
    limited.tipoff.operation_budget = 4;
    check(limited.run() == NBA97_TEXT_IO_REFUSED &&
        !limited.first_progress.completed &&
        limited.first_progress.stopped_pc == 0x80067450u &&
        limited.first_progress.stopped_entry == 0x8007ef4cu &&
        limited.tipoff.result == NBA97_TEXT_LIMIT &&
        limited.tipoff.progress.operations == 4 &&
        limited.tipoff.progress.stopped_pc == 0x8007ef5cu &&
        limited.tipoff.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8007ef64u);

    Composition args;
    Nba97GameFirstPeriodStartupEvent event{};
    event.pc = 0x80067450u;
    event.delay_slot_pc = 0x80067454u;
    event.entry = 0x8007ef4cu;
    event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_7EF4C;
    auto registers = args.first.registers;
    const auto before = registers;
    event.pc ^= 4u;
    check(!nba97_game_tipoff_announcement_from_first_period_startup(
        &args.tipoff, &args.first.memory, &event, &registers));
    check(args.tipoff.result == NBA97_TEXT_ARGUMENT &&
        args.tipoff.invocations == 0);
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        check(registers.gpr[i].word == before.gpr[i].word &&
            registers.gpr[i].known_mask == before.gpr[i].known_mask);
    event.pc ^= 4u;
    args.tipoff.access_journal = nullptr;
    args.tipoff.access_journal_capacity = 1;
    check(!nba97_game_tipoff_announcement_from_first_period_startup(
        &args.tipoff, &args.first.memory, &event, &registers) &&
        args.tipoff.result == NBA97_TEXT_ARGUMENT &&
        args.tipoff.invocations == 0);
    check(!nba97_game_tipoff_announcement_from_first_period_startup(
        nullptr, &args.first.memory, &event, &registers));
}
}

int main() {
    natural_actual_67450_gate_and_branches();
    nested_failure_prefix_and_adapter_guards();
    std::printf("game tip-off announcement integration: %u checks passed\n",
        checks);
    return 0;
}
