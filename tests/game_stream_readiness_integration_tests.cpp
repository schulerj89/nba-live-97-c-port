#include "game_stream_readiness_adapter.h"

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
            "stream readiness integration check %u failed at %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x800ff000u;

struct Composition {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameMatchAudioServiceContext service{};
    Nba97GameClockReadContext clock{};
    Nba97GameAudioStreamStatusContext status{};
    Nba97GameStreamReadinessContext readiness{};
    Nba97GameMatchAudioServiceProgress service_progress{};
    Nba97GameStreamReadinessAdapterProgress adapter{};
    std::vector<Nba97GameMatchAudioServiceEvent> other_events;
    Nba97GameStreamReadinessEvent child_event{};
    Nba97GameStreamReadinessMachine child_entry{};
    std::uint32_t child_result = 1;
    unsigned child_calls = 0;

    Composition() {
        service.memory = {&region, 1};
        service.operation_budget = 64;
        service.io = audioIo;
        service.user = this;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            service.machine.registers.gpr[i] = {
                0x41000000u + i * 0x00010101u,
                static_cast<std::uint8_t>((i % 15u) + 1u)};
        service.machine.registers.gpr[0] = {0, 15};
        service.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {EntrySp, 15};
        service.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x8002de64u, 15};
        service.machine.hi = {0x12345678u, 3};
        service.machine.lo = {0x9abcdef0u, 12};
        clock.operation_budget = 1;
        status.operation_budget = 8;
        readiness.operation_budget = 6;
        readiness.io = readinessIo;
        readiness.user = this;

        put(0x800d7a70u, 1100, 4); /* AC counter */
        put(0x800e430cu, 1000, 4); /* old AB counter */
        put(0x800fda0cu, 3, 2);    /* mode 3 */
        put(0x8002149cu, 1, 4);    /* stream handle */
        put(0x800c43b0u, 7, 1);    /* X status -> 3 */
        put(0x800c43b1u, 0, 1);
        put(0x800f0fdcu, 1, 2);    /* AD enabled */
        put(0x800fda0eu, 77, 2);
        put(0x800fda10u, 88, 2);
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Ram);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width) {
        auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            known[at + i] = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[at + i]) << (i * 8u);
        return value;
    }
    static int audioIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameMatchAudioServiceEvent* event,
        Nba97GameMatchAudioServiceMachine* machine) {
        auto& c = *static_cast<Composition*>(user);
        c.other_events.push_back(*event);
        if (event->kind == NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80083EEC) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {9, 15};
            machine->hi = {0x0badc0deu, 5};
            machine->lo = {0xc001d00du, 10};
        } else {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 15};
        }
        return 1;
    }
    static int readinessIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameStreamReadinessEvent* event,
        Nba97GameStreamReadinessMachine* machine) {
        auto& c = *static_cast<Composition*>(user);
        ++c.child_calls;
        c.child_event = *event;
        c.child_entry = *machine;
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
            {c.child_result, 15};
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0] =
            {0xfeedbeefu, 9};
        return 1;
    }
    int run() {
        return nba97_game_match_audio_service_with_stream_readiness(&service,
            &clock, &status, &readiness, &service_progress, &adapter);
    }
};

void actualAbNaturalEvent() {
    Composition c;
    check(c.run() == NBA97_TEXT_COMPLETE && c.service_progress.completed);
    check(c.adapter.audio_service_result == NBA97_TEXT_COMPLETE &&
        c.adapter.readiness_result == NBA97_TEXT_COMPLETE &&
        c.adapter.readiness_invocations == 1 &&
        c.adapter.readiness_completions == 1 && c.child_calls == 1);
    check(c.adapter.readiness_event.kind ==
            NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80088D0C &&
        c.adapter.readiness_event.pc == 0x8002a2ecu &&
        c.adapter.readiness_event.delay_slot_pc == 0x8002a2f0u &&
        c.adapter.readiness_event.entry == 0x80088d0cu &&
        c.adapter.readiness_event.argument_count == 0 &&
        c.adapter.readiness_event.operation != 0 &&
        c.adapter.readiness_event.invocation == 1);
    check(c.adapter.readiness.frame_stack_pointer == EntrySp - 0x38u &&
        c.adapter.readiness.saved_return_address.word == 0x8002a2f4u &&
        c.adapter.readiness.restored_return_address.word == 0x8002a2f4u &&
        c.adapter.readiness.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == EntrySp - 0x20u &&
        c.child_entry.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80088d38u);
    check(c.adapter.audio_service.clock_read_invocations == 1);
    check(c.adapter.audio_service.stream_status_invocations == 1);
    check(c.adapter.audio_service.unresolved_callbacks_completed == 3);
    check(c.adapter.unresolved_callbacks_completed == 2);
    check(c.other_events.size() == 2);
    check(c.other_events[0].kind ==
        NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8008847C);
    check(c.other_events[1].kind ==
        NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80083EEC);
    check(c.get(0x800e430cu, 4) == 1100 &&
        c.get(0x800fda0eu, 2) == 88 &&
        c.service_progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_T0].word == 0xfeedbeefu &&
        c.service_progress.machine.hi.word == 0x0badc0deu &&
        c.service_progress.machine.lo.word == 0xc001d00du);
}

void directAdapterValidationAndNestedPrefix() {
    Composition c;
    Nba97GameMatchAudioServiceEvent event{};
    event.kind = NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80088D0C;
    event.pc = 0x8002a2ecu;
    event.delay_slot_pc = 0x8002a2f0u;
    event.entry = 0x80088d0cu;
    event.operation = 12;
    event.invocation = 1;
    auto machine = c.service.machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x8002a2f4u, 15};
    Nba97GameStreamReadinessProgress progress{};
    check(nba97_game_stream_readiness_from_match_audio_service(
        &c.service.memory, &event, &machine, &c.readiness, &progress) ==
        NBA97_TEXT_COMPLETE && progress.completed && c.child_calls == 1);

    auto wrong = event;
    wrong.pc += 4;
    check(nba97_game_stream_readiness_from_match_audio_service(
        &c.service.memory, &wrong, &machine, &c.readiness, &progress) ==
        NBA97_TEXT_ARGUMENT);
    wrong = event;
    wrong.operation = 0;
    check(nba97_game_stream_readiness_from_match_audio_service(
        &c.service.memory, &wrong, &machine, &c.readiness, &progress) ==
        NBA97_TEXT_ARGUMENT);
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ^= 4u;
    check(nba97_game_stream_readiness_from_match_audio_service(
        &c.service.memory, &event, &machine, &c.readiness, &progress) ==
        NBA97_TEXT_ARGUMENT);

    Composition limited;
    limited.readiness.operation_budget = 3;
    check(limited.run() == NBA97_TEXT_IO_REFUSED &&
        limited.adapter.readiness_result == NBA97_TEXT_LIMIT &&
        limited.adapter.readiness_invocations == 1 &&
        limited.adapter.readiness_completions == 0 &&
        limited.adapter.readiness.stopped_pc == 0x80088d30u &&
        limited.service_progress.stopped_pc == 0x8002a2ecu &&
        limited.service_progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x80088d38u);
}
}

int main() {
    actualAbNaturalEvent();
    directAdapterValidationAndNestedPrefix();
    std::printf("%u game stream readiness integration checks passed\n",
        checks);
    return 0;
}
