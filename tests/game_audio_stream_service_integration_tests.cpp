#include "game_audio_stream_service_adapter.h"

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
            "game audio stream service integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ff000u;
constexpr std::uint32_t EntrySp = 0x807fff00u;
constexpr std::uint32_t PumpFrame = EntrySp - 0x20u;
constexpr std::uint32_t ServiceFrame = PumpFrame - 0x18u;
constexpr std::uint32_t Header = 0x80010000u;

struct Composition {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x110000);
    std::vector<std::uint8_t> ramKnown =
        std::vector<std::uint8_t>(0x110000, 1);
    std::array<std::uint8_t, 0x1000> stack{};
    std::array<std::uint8_t, 0x1000> stackKnown{};
    std::array<Nba97GameTextRegion, 2> regions{{
        {Ram, ram.data(), ramKnown.data(), ram.size()},
        {Stack, stack.data(), stackKnown.data(), stack.size()}}};
    Nba97GameAudioStreamPumpContext pump{};
    Nba97GameAudioStreamStatusContext status{};
    Nba97GameAudioStreamServiceContext service{};
    Nba97GameAudioStreamPumpProgress pumpProgress{};
    Nba97GameAudioStreamStatusAdapterProgress statusProgress{};
    Nba97GameAudioStreamServiceAdapterProgress serviceProgress{};
    std::vector<Nba97GameAudioStreamPumpEvent> pumpEvents;
    std::vector<Nba97GameAudioStreamServiceEvent> serviceEvents;
    bool refuseServiceChild = false;

    Composition(std::uint8_t flags, std::uint32_t state) {
        stack.fill(0xcd);
        stackKnown.fill(1);
        put8(0x800c43b0u, flags);
        put8(0x800c43b1u, 0);
        put32(0x800c438cu, 0x87654320u);
        put32(0x8010473cu, Header);
        put32(Header + 0x24u, state);
        pump.memory = {regions.data(), regions.size()};
        pump.operation_budget = 64;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            pump.registers.gpr[i] = {0x31000000u + i * 0x01010101u, 15};
        pump.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 15};
        pump.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 15};
        pump.registers.gpr[NBA97_MATCH_INITIALIZE_FP] = {0xa1b2c3d4u, 5};
        pump.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80067654u, 15};
        pump.io = pumpIo;
        pump.user = this;
        status.operation_budget = 8;
        service.operation_budget = 16;
        service.io = serviceIo;
        service.user = this;
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
            bytes(address)[i] = static_cast<std::uint8_t>(value >> (8u * i));
    }

    static int pumpIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameAudioStreamPumpEvent* event,
        Nba97GameAudioStreamPumpRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.pumpEvents.push_back(*event);
        if (event->kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 15};
        if (event->kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_800840F0 ||
            event->kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088288)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x12345678u, 15};
        return 1;
    }

    static int serviceIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameAudioStreamServiceEvent* event,
        Nba97GameAudioStreamServiceRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.serviceEvents.push_back(*event);
        registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x13572468u, 15};
        return c.refuseServiceChild ? 0 : 1;
    }

    int run() {
        return nba97_game_audio_stream_pump_with_stream_status_and_service(
            &pump, &status, &service, &pumpProgress, &statusProgress,
            &serviceProgress);
    }
};

void naturalBothVCallSites() {
    Composition mode5(7, 1);
    check(mode5.run() == NBA97_TEXT_COMPLETE && mode5.pumpProgress.completed);
    check(mode5.statusProgress.status_invocations == 1 &&
        mode5.statusProgress.status_completions == 1 &&
        mode5.statusProgress.status.returned_value.word == 3);
    check(mode5.serviceProgress.service_invocations == 1 &&
        mode5.serviceProgress.service_completions == 1 &&
        mode5.serviceProgress.service_event.pc == 0x80083f78u &&
        mode5.serviceProgress.service_event.delay_slot_pc == 0x80083f7cu &&
        mode5.serviceProgress.service_event.entry == 0x80086190u);
    check(mode5.serviceProgress.service.completed &&
        mode5.serviceProgress.service.frame_stack_pointer == ServiceFrame &&
        mode5.serviceProgress.service.header_state.word == 1 &&
        mode5.serviceProgress.service.call_count[
            NBA97_GAME_AUDIO_STREAM_SERVICE_CHILD_800861E4] == 0 &&
        mode5.serviceProgress.service.returned_value.word == 1);
    check(mode5.pumpEvents.size() == 1 &&
        mode5.pumpEvents[0].kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018 &&
        mode5.serviceProgress.unresolved_callbacks_completed == 1 &&
        mode5.pumpProgress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp);

    Composition mode4(6, 0);
    check(mode4.run() == NBA97_TEXT_COMPLETE && mode4.pumpProgress.completed);
    check(mode4.statusProgress.status.returned_value.word == 1 &&
        mode4.serviceProgress.service_invocations == 1 &&
        mode4.serviceProgress.service_completions == 1 &&
        mode4.serviceProgress.service_event.pc == 0x80084034u &&
        mode4.serviceProgress.service_event.delay_slot_pc == 0x80084038u);
    check(mode4.serviceProgress.service.frame_stack_pointer == ServiceFrame &&
        mode4.serviceProgress.service.header_state.word == 0 &&
        mode4.serviceProgress.service.returned_value.word == 0x13572468u &&
        mode4.serviceProgress.service.call_count[
            NBA97_GAME_AUDIO_STREAM_SERVICE_CHILD_800861E4] == 1);
    check(mode4.serviceEvents.size() == 1 &&
        mode4.serviceEvents[0].pc == 0x800861c4u &&
        mode4.serviceEvents[0].entry == 0x800861e4u &&
        mode4.serviceEvents[0].argument_count == 0 &&
        mode4.pumpEvents.size() == 1 &&
        mode4.pumpEvents[0].kind == NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80088018);
}

void nestedPrefixesAndValidation() {
    Composition limited(7, 0);
    limited.service.operation_budget = 4;
    check(limited.run() == NBA97_TEXT_IO_REFUSED &&
        limited.serviceProgress.service_result == NBA97_TEXT_LIMIT &&
        limited.serviceProgress.service_invocations == 1 &&
        limited.serviceProgress.service_completions == 0 &&
        limited.serviceProgress.service.operations == 4 &&
        limited.serviceProgress.service.stopped_pc == 0x800861c4u &&
        limited.serviceProgress.service.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x800861ccu &&
        limited.pumpProgress.stopped_pc == 0x80083f78u);

    Composition refused(6, 0);
    refused.refuseServiceChild = true;
    check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.serviceProgress.service_result == NBA97_TEXT_IO_REFUSED &&
        refused.serviceEvents.size() == 1 &&
        refused.pumpProgress.stopped_pc == 0x80084034u);

    Composition invalid(7, 1);
    invalid.service.access_journal_capacity = 1;
    check(invalid.run() == NBA97_TEXT_IO_REFUSED &&
        invalid.serviceProgress.service_result == NBA97_TEXT_ARGUMENT &&
        invalid.serviceProgress.service.operations == 0 &&
        invalid.pumpProgress.stopped_pc == 0x80083f78u &&
        invalid.pumpProgress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80083f80u);

    Composition direct(7, 1);
    Nba97GameAudioStreamPumpEvent event{};
    event.kind = NBA97_GAME_AUDIO_STREAM_PUMP_CHILD_80086190;
    event.pc = 0x80083f78u;
    event.delay_slot_pc = 0x80083f7cu;
    event.entry = 0x80086190u;
    auto registers = direct.pump.registers;
    Nba97GameAudioStreamServiceAdapterProgress out{};
    check(nba97_game_audio_stream_service_from_stream_pump(
        &direct.pump.memory, &event, &registers, &direct.service, &out) ==
        NBA97_TEXT_COMPLETE && out.service_completions == 1);
    event.pc = 0x80084034u;
    event.delay_slot_pc = 0x80084038u;
    registers = direct.pump.registers;
    check(nba97_game_audio_stream_service_from_stream_pump(
        &direct.pump.memory, &event, &registers, &direct.service, &out) ==
        NBA97_TEXT_COMPLETE && out.service_completions == 2);
    event.pc = 0x80083f7cu;
    check(nba97_game_audio_stream_service_from_stream_pump(
        &direct.pump.memory, &event, &registers, &direct.service, &out) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_audio_stream_pump_with_stream_status_and_service(
        nullptr, &direct.status, &direct.service, &direct.pumpProgress,
        &direct.statusProgress, &out) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_audio_stream_pump_with_stream_status_and_service(
        &direct.pump, nullptr, &direct.service, &direct.pumpProgress,
        &direct.statusProgress, &out) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_audio_stream_pump_with_stream_status_and_service(
        &direct.pump, &direct.status, nullptr, &direct.pumpProgress,
        &direct.statusProgress, &out) == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    naturalBothVCallSites();
    nestedPrefixesAndValidation();
    std::printf("%u game audio stream service integration checks passed\n",
        checks);
    return 0;
}
