#include "game_audio_stream_status_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "game audio stream status integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ff000u;
constexpr std::uint32_t EntrySp = 0x807fff00u;
constexpr std::uint32_t PumpFrame = EntrySp - 0x20u;
constexpr std::uint32_t StatusFrame = PumpFrame - 8u;

struct Composition {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x110000);
    std::vector<std::uint8_t> ram_known =
        std::vector<std::uint8_t>(0x110000, 1);
    std::array<std::uint8_t, 0x1000> stack{};
    std::array<std::uint8_t, 0x1000> stack_known{};
    std::array<Nba97GameTextRegion, 2> regions{{
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}}};
    Nba97GameAudioStreamPumpContext pump{};
    Nba97GameAudioStreamStatusContext status{};
    Nba97GameAudioStreamPumpProgress pump_progress{};
    Nba97GameAudioStreamStatusAdapterProgress adapter_progress{};
    std::vector<Nba97GameAudioStreamPumpEvent> other_events;
    std::vector<std::uint32_t> statuses{0};
    unsigned status_index = 0;

    Composition(std::uint8_t flags, std::uint8_t busy) {
        stack.fill(0xcd);
        stack_known.fill(1);
        put8(0x800c43b0u, flags);
        put8(0x800c43b1u, busy);
        put32(0x800c438cu, 0x87654320u);
        pump.memory = {regions.data(), regions.size()};
        pump.operation_budget = 64;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            pump.registers.gpr[i] = {0x31000000u + i * 0x01010101u, 15};
        pump.registers.gpr[0] = {0, 15};
        pump.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 15};
        pump.registers.gpr[NBA97_MATCH_INITIALIZE_FP] = {0xa1b2c3d4u, 5};
        pump.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80067654u, 15};
        pump.io = pumpIo;
        pump.user = this;
        status.operation_budget = 8;
    }

    std::uint8_t* bytes(std::uint32_t address) {
        return address >= Stack ? stack.data() + (address - Stack) :
            ram.data() + (address - Ram);
    }
    void put8(std::uint32_t address, std::uint8_t value) {
        *bytes(address) = value;
    }
    void put32(std::uint32_t address, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i)
            bytes(address)[i] = static_cast<std::uint8_t>(value >> (i * 8u));
    }

    static int pumpIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameAudioStreamPumpEvent* event,
        Nba97GameAudioStreamPumpRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.other_events.push_back(*event);
        if (event->kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018) {
            const unsigned i = c.status_index < c.statuses.size() ?
                c.status_index : static_cast<unsigned>(c.statuses.size() - 1u);
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {c.statuses[i], 15};
            ++c.status_index;
        }
        if (event->kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_800840F0 ||
            event->kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088288)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x12345678u, 15};
        return 1;
    }

    int run() {
        return nba97_game_audio_stream_pump_with_stream_status(&pump, &status,
            &pump_progress, &adapter_progress);
    }
};

void naturalVGateAndModes() {
    Composition flags5(5, 0);
    check(flags5.run() == NBA97_TEXT_COMPLETE && flags5.pump_progress.completed &&
        flags5.adapter_progress.status_invocations == 1 &&
        flags5.adapter_progress.status_completions == 1 &&
        flags5.adapter_progress.status.returned_value.word == 0xfffffff2u &&
        flags5.pump_progress.initial_status.word == 0xfffffff2u &&
        flags5.pump_progress.returned_value.word == 0 &&
        flags5.other_events.empty());

    Composition flags7(7, 0);
    check(flags7.run() == NBA97_TEXT_COMPLETE && flags7.pump_progress.completed &&
        flags7.adapter_progress.status.returned_value.word == 3 &&
        flags7.pump_progress.first_flags.word == 7 &&
        flags7.other_events.size() == 2 &&
        flags7.other_events[0].kind ==
            NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80086190 &&
        flags7.other_events[1].kind ==
            NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018);

    Composition flags6(6, 0);
    check(flags6.run() == NBA97_TEXT_COMPLETE && flags6.pump_progress.completed &&
        flags6.adapter_progress.status.returned_value.word == 1 &&
        flags6.pump_progress.first_flags.word == 6 &&
        flags6.other_events.size() == 2 &&
        flags6.other_events[0].kind ==
            NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80086190 &&
        flags6.other_events[1].kind ==
            NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018);

    Composition busy4(7, 255);
    check(busy4.run() == NBA97_TEXT_COMPLETE && busy4.pump_progress.completed &&
        busy4.adapter_progress.status.returned_value.word == 4 &&
        busy4.pump_progress.initial_status.word == 4 &&
        busy4.other_events.size() == 2);

    for (Composition* c : {&flags5, &flags7, &flags6, &busy4}) {
        check(c->adapter_progress.status_event.pc == 0x80083f00u &&
            c->adapter_progress.status_event.delay_slot_pc == 0x80083f04u &&
            c->adapter_progress.status_event.entry == 0x8008472cu &&
            c->adapter_progress.status.frame_stack_pointer == StatusFrame &&
            c->adapter_progress.status.restored_s8.word == PumpFrame &&
            c->pump_progress.frame_stack_pointer == PumpFrame &&
            c->pump_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
                EntrySp &&
            c->pump_progress.restored_s8.word == 0xa1b2c3d4u &&
            c->pump_progress.restored_s8.known_mask == 5);
    }
}

void nestedPrefixAndAdapterValidation() {
    Composition limited(7, 0);
    limited.status.operation_budget = 2;
    check(limited.run() == NBA97_TEXT_IO_REFUSED &&
        limited.adapter_progress.status_result == NBA97_TEXT_LIMIT &&
        limited.adapter_progress.status_invocations == 1 &&
        limited.adapter_progress.status_completions == 0 &&
        limited.adapter_progress.status.operations == 2 &&
        limited.adapter_progress.status.stopped_pc == 0x8008476cu &&
        limited.pump_progress.stopped_pc == 0x80083f00u &&
        limited.pump_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            StatusFrame);

    Composition direct(7, 0);
    Nba97GameAudioStreamPumpEvent good{};
    good.kind = NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_8008472C;
    good.pc = 0x80083f00u;
    good.delay_slot_pc = 0x80083f04u;
    good.entry = 0x8008472cu;
    Nba97GameAudioStreamStatusAdapterProgress out{};
    auto registers = direct.pump.registers;
    check(nba97_game_audio_stream_status_from_stream_pump(
        &direct.pump.memory, &good, &registers, &direct.status, &out) ==
        NBA97_TEXT_COMPLETE && out.status_completions == 1);
    Nba97GameAudioStreamPumpEvent wrong = good;
    wrong.pc = 0x80083f04u;
    check(nba97_game_audio_stream_status_from_stream_pump(
        &direct.pump.memory, &wrong, &registers, &direct.status, &out) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_audio_stream_pump_with_stream_status(nullptr,
        &direct.status, &direct.pump_progress, &out) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_audio_stream_pump_with_stream_status(&direct.pump,
        nullptr, &direct.pump_progress, &out) == NBA97_TEXT_ARGUMENT);

    Composition invalid_status(7, 0);
    invalid_status.status.access_journal_capacity = 1;
    check(invalid_status.run() == NBA97_TEXT_IO_REFUSED &&
        invalid_status.adapter_progress.status_result == NBA97_TEXT_ARGUMENT &&
        invalid_status.pump_progress.stopped_pc == 0x80083f00u &&
        invalid_status.pump_progress.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == PumpFrame &&
        invalid_status.pump_progress.registers.gpr[
            NBA97_MATCH_INITIALIZE_FP].word == PumpFrame &&
        invalid_status.pump_progress.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x80083f08u);
}
}

int main() {
    naturalVGateAndModes();
    nestedPrefixAndAdapterValidation();
    std::printf("%u game audio stream status integration checks passed\n",
        checks);
    return 0;
}
