#include "game_clock_read_adapter.h"

#include <array>
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
            "game clock read integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ff800u;
constexpr std::uint32_t EntrySp = 0x807fff00u;
constexpr std::uint32_t Clock = 0x800d7a70u;

struct Composition {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x110000);
    std::vector<std::uint8_t> ram_known =
        std::vector<std::uint8_t>(0x110000, 1);
    std::array<std::uint8_t, 0x800> stack{};
    std::array<std::uint8_t, 0x800> stack_known{};
    std::array<Nba97GameTextRegion, 2> regions{{
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}}};
    std::array<Nba97GameClockReadAccess, 4> journal{};
    Nba97GameSpeechStartupContext speech{};
    Nba97GameClockReadAdapterContext clock{};
    Nba97GameSpeechStartupProgress speech_progress{};
    Nba97GameClockReadAdapterProgress adapter_progress{};
    std::vector<Nba97GameSpeechStartupEvent> unresolved;
    unsigned ready_calls = 0;
    unsigned pump_calls = 0;

    Composition() {
        stack.fill(0xcd);
        stack_known.fill(1);
        put(0x80015018u, 1);
        put(Clock, 0);
        speech.memory = {regions.data(), regions.size()};
        speech.operation_budget = 40;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            speech.registers.gpr[i] = {
                0x61000000u + i * 0x01010101u, 0x0f};
        speech.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        speech.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        speech.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x800802bcu, 0x0f};
        speech.io = io;
        speech.user = this;
        clock.operation_budget = 1;
        clock.access_journal = journal.data();
        clock.access_journal_capacity = journal.size();
    }

    void put(std::uint32_t address, std::uint32_t value) {
        auto* out = ram.data() + (address - Ram);
        for (unsigned i = 0; i < 4; ++i)
            out[i] = static_cast<std::uint8_t>(value >> (8u * i));
    }

    static int io(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameSpeechStartupEvent* event,
        Nba97GameSpeechStartupRegisters* registers) {
        auto& c = *static_cast<Composition*>(opaque);
        c.unresolved.push_back(*event);
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_800853F4)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x81234560u, 0x0f};
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_80083D38)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x8abcdef0u, 0x0f};
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_80083EEC) {
            ++c.pump_calls;
            c.put(Clock, c.pump_calls == 1 ? 100u : 341u);
        }
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_8008847C) {
            ++c.ready_calls;
            if (c.ready_calls == 1)
                c.put(Clock, 340u);
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0f};
        }
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_8002ABB4)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0xdecafbadu, 0x0f};
        return 1;
    }

    int run() {
        return nba97_game_speech_startup_with_clock_read(&speech, &clock,
            &speech_progress, &adapter_progress);
    }
};

void actual_q_both_clock_sites_and_external_counter_writes() {
    Composition c;
    check(c.run() == NBA97_TEXT_COMPLETE && c.speech_progress.completed);
    check(c.adapter_progress.clock_result == NBA97_TEXT_COMPLETE &&
        c.adapter_progress.invocations == 3 &&
        c.adapter_progress.initial_invocations == 1 &&
        c.adapter_progress.poll_invocations == 2 &&
        c.adapter_progress.clock_access_events == 3);
    check(c.adapter_progress.initial_event.pc == 0x800801ecu &&
        c.adapter_progress.initial_event.delay_slot_pc == 0x800801f0u &&
        c.adapter_progress.initial_event.entry == 0x800a5810u &&
        c.adapter_progress.poll_event.pc == 0x80080208u &&
        c.adapter_progress.poll_event.delay_slot_pc == 0x8008020cu &&
        c.adapter_progress.poll_event.entry == 0x800a5810u);
    check(c.adapter_progress.initial_clock.return_v0.word == 100u &&
        c.adapter_progress.poll_clock.return_v0.word == 341u &&
        c.speech_progress.deadline.word == 340u && c.ready_calls == 2 &&
        c.pump_calls == 2);
    check(c.journal[0].value == 100u && c.journal[1].value == 340u &&
        c.journal[2].value == 341u);
    for (unsigned i = 0; i < 3; ++i)
        check(c.journal[i].pc == 0x800a5814u &&
            c.journal[i].address == Clock && c.journal[i].width == 4 &&
            c.journal[i].operation == 1);
    check(c.adapter_progress.unresolved_callbacks_completed ==
        c.unresolved.size() && c.unresolved.size() == 12);
    for (const auto& event : c.unresolved)
        check(event.kind != NBA97_GAME_SPEECH_STARTUP_CHILD_800A5810);
    check(c.speech_progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
        0xdecafbadu);
}

void leaf_failure_prefix_and_adapter_validation() {
    Composition limited;
    limited.clock.operation_budget = 0;
    check(limited.run() == NBA97_TEXT_IO_REFUSED &&
        limited.adapter_progress.clock_result == NBA97_TEXT_LIMIT &&
        limited.adapter_progress.invocations == 1 &&
        limited.adapter_progress.initial_invocations == 1 &&
        limited.adapter_progress.initial_clock.return_v0.word == 0x800d0000u &&
        limited.adapter_progress.initial_clock.stopped_pc == 0x800a5814u &&
        limited.speech_progress.stopped_pc == 0x800801ecu);

    Composition args;
    Nba97GameClockReadProgress progress{};
    Nba97GameSpeechStartupEvent event{};
    event.kind = NBA97_GAME_SPEECH_STARTUP_CHILD_800A5810;
    event.pc = 0x800801ecu;
    event.delay_slot_pc = 0x800801f0u;
    event.entry = 0x800a5810u;
    Nba97GameSpeechStartupRegisters registers = args.speech.registers;
    check(nba97_game_clock_read_from_speech_startup(nullptr, &event,
        &registers, &args.clock, &progress) == NBA97_TEXT_ARGUMENT);
    event.pc = 0x800801f0u;
    check(nba97_game_clock_read_from_speech_startup(&args.speech.memory, &event,
        &registers, &args.clock, &progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_speech_startup_with_clock_read(nullptr, &args.clock,
        &args.speech_progress, &args.adapter_progress) == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    actual_q_both_clock_sites_and_external_counter_writes();
    leaf_failure_prefix_and_adapter_validation();
    std::printf("%u game clock read integration checks passed\n", checks);
    return 0;
}
