#include "game_match_audio_service_adapter.h"

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
            "match audio service integration check %u failed at %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x8010ff00u;
constexpr std::uint32_t Counter = 0x800d7a70u;

struct Composition {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    std::array<Nba97GameMatchAudioServiceAccess, 64> service_journal{};
    std::array<Nba97GameClockReadAccess, 2> clock_journal{};
    std::array<Nba97GameAudioStreamStatusAccess, 16> status_journal{};
    Nba97GameMatchAudioServiceContext service{};
    Nba97GameClockReadContext clock{};
    Nba97GameAudioStreamStatusContext status{};
    Nba97GameMatchAudioServiceProgress progress{};
    Nba97GameMatchAudioServiceAdapterProgress adapter{};
    Nba97GameMatchServicePublishContext publish{};
    Nba97GameMatchServicePublishProgress publish_progress{};
    std::vector<Nba97GameMatchAudioServiceEvent> unresolved;

    Composition() {
        service.memory = {&region, 1};
        service.operation_budget = 100;
        service.io = io;
        service.user = this;
        service.access_journal = service_journal.data();
        service.access_journal_capacity = service_journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            service.machine.registers.gpr[i] =
                {0x41000000u + i * 0x01010101u, 15};
        service.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 15};
        service.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {EntrySp, 15};
        service.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x8002de64u, 15};
        service.machine.registers.gpr[NBA97_MATCH_INITIALIZE_FP] =
            {0xa1b2c3d4u, 5};
        service.machine.hi = {0x12345678u, 15};
        service.machine.lo = {0x9abcdef0u, 15};
        clock.operation_budget = 1;
        clock.access_journal = clock_journal.data();
        clock.access_journal_capacity = clock_journal.size();
        status = {};
        status.operation_budget = 8;
        status.access_journal = status_journal.data();
        status.access_journal_capacity = status_journal.size();
        publish.memory = {&region, 1};
        publish.operation_budget = 8;
        publish.machine = service.machine;
        publish.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x80068d84u, 15};
        publish.io = publishIo;
        publish.user = this;

        put(0x800e430cu, 100, 4);
        put(Counter, 101, 4);
        put(0x800fda0cu, 3, 2);
        put(0x800fda0eu, 20, 2);
        put(0x800fda10u, 40, 2);
        put(0x800170bcu, 0, 4);
        put(0x8002149cu, 1, 4);
        put(0x800c43b0u, 7, 1);
        put(0x800c43b1u, 0, 1);
        put(0x800f9ffeu, 0xbeef, 2);
        put(0x800fdb90u, 0, 2);
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
        std::uint32_t result = 0;
        for (unsigned i = 0; i < width; ++i)
            result |= std::uint32_t(bytes[at + i]) << (i * 8u);
        return result;
    }
    void setKnown(std::uint32_t address, std::uint8_t mask,
        unsigned width) {
        auto at = offset(address);
        for (unsigned i = 0; i < width; ++i)
            known[at + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
    }
    std::uint8_t getKnown(std::uint32_t address, unsigned width) const {
        auto at = offset(address);
        std::uint8_t mask = 0;
        for (unsigned i = 0; i < width; ++i)
            mask = static_cast<std::uint8_t>(mask | (known[at + i] << i));
        return mask;
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameMatchAudioServiceEvent* event,
        Nba97GameMatchAudioServiceMachine* machine) {
        auto& c = *static_cast<Composition*>(user);
        c.unresolved.push_back(*event);
        if (event->kind == NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80088D0C)
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 15};
        else
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
                {0x2468ace0u, 15};
        if (event->kind == NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80083EEC) {
            /* This typed boundary carries the state V's narrower production
             * interface cannot expose, including child-mutated HI/LO. */
            machine->hi = {0x0badc0deu, 15};
            machine->lo = {0x13572468u, 15};
        }
        return 1;
    }
    static int publishIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameMatchServicePublishEvent* event,
        Nba97GameMatchServicePublishMachine* machine) {
        auto& c = *static_cast<Composition*>(user);
        return nba97_game_match_audio_service_from_match_service_publish(
            memory, event, machine, &c.service, &c.clock, &c.status,
            &c.progress, &c.adapter) == NBA97_TEXT_COMPLETE;
    }
};

void actualXAndTypedVComposition() {
    Composition c;
    check(nba97_game_match_audio_service_with_stream_status(&c.service,
        &c.clock, &c.status, &c.progress, &c.adapter) ==
        NBA97_TEXT_COMPLETE && c.progress.completed);
    check(c.adapter.clock_read_invocations == 1 &&
        c.adapter.clock_read_completions == 1 &&
        c.adapter.clock_read_result == NBA97_TEXT_COMPLETE &&
        c.adapter.clock_read.return_v0.word == 101 &&
        c.adapter.clock_read.return_v0.known_mask == 15);
    check(c.adapter.clock_read_event.pc == 0x8002a270u &&
        c.adapter.clock_read_event.delay_slot_pc == 0x8002a274u &&
        c.adapter.clock_read_event.entry == 0x800a5810u &&
        c.adapter.clock_read_event.argument_count == 0 &&
        c.clock_journal[0].pc == 0x800a5814u &&
        c.clock_journal[0].address == Counter);
    check(c.adapter.stream_status_invocations == 1 &&
        c.adapter.stream_status_completions == 1 &&
        c.adapter.stream_status_result == NBA97_TEXT_COMPLETE &&
        c.adapter.stream_status.returned_value.word == 3);
    check(c.adapter.stream_status_event.pc == 0x8002a2dcu &&
        c.adapter.stream_status_event.delay_slot_pc == 0x8002a2e0u &&
        c.adapter.stream_status_event.entry == 0x8008472cu &&
        c.adapter.stream_status_event.argument_count == 0);
    check(c.adapter.stream_status.frame_stack_pointer == EntrySp - 0x28u &&
        c.adapter.stream_status.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == EntrySp - 0x20u);
    check(c.unresolved.size() == 2 &&
        c.unresolved[0].kind ==
            NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80088D0C &&
        c.unresolved[1].kind ==
            NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80083EEC);
    check(c.adapter.unresolved_callbacks_completed == 2 &&
        c.progress.call_count[
            NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_800A5810] == 1 &&
        c.progress.call_count[
            NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_8008472C] == 1 &&
        c.progress.call_count[
            NBA97_GAME_MATCH_AUDIO_SERVICE_CHILD_80083EEC] == 1);
    check(c.get(0x800fda0eu, 2) == 40 &&
        c.progress.machine.hi.word == 0x0badc0deu &&
        c.progress.machine.lo.word == 0x13572468u);
}

void directFutureAACallBoundary() {
    Composition c;
    Nba97GameMatchAudioServiceCallerEvent event{
        0x8002de5cu, 0x8002de60u, 0x8002a264u, 0};
    auto machine = c.service.machine;
    check(nba97_game_match_audio_service_from_8002de5c(&c.service.memory,
        &event, &machine, &c.service, &c.clock, &c.status, &c.progress,
        &c.adapter) == NBA97_TEXT_COMPLETE);
    check(c.adapter.caller_invocations == 1 &&
        c.adapter.caller_completions == 1 &&
        c.adapter.caller_event.pc == event.pc &&
        machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
        machine.hi.word == 0x0badc0deu);

    Composition wrong;
    auto original = wrong.service.machine;
    auto wrong_event = event;
    wrong_event.pc += 4;
    check(nba97_game_match_audio_service_from_8002de5c(
        &wrong.service.memory, &wrong_event, &original, &wrong.service,
        &wrong.clock, &wrong.status, &wrong.progress, &wrong.adapter) ==
        NBA97_TEXT_ARGUMENT && wrong.adapter.caller_invocations == 0);
    wrong_event = event;
    original.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ^= 4u;
    check(nba97_game_match_audio_service_from_8002de5c(
        &wrong.service.memory, &wrong_event, &original, &wrong.service,
        &wrong.clock, &wrong.status, &wrong.progress, &wrong.adapter) ==
        NBA97_TEXT_ARGUMENT);
}

void actualAAIntoABIntoX() {
    Composition c;
    check(nba97_game_match_service_publish(&c.publish,
        &c.publish_progress) == NBA97_TEXT_COMPLETE &&
        c.publish_progress.completed && c.progress.completed);
    check(c.publish_progress.call_count[
            NBA97_GAME_MATCH_SERVICE_PUBLISH_CHILD_8002A264] == 1 &&
        c.adapter.caller_invocations == 1 &&
        c.adapter.caller_completions == 1 &&
        c.adapter.caller_event.pc == 0x8002de5cu &&
        c.adapter.caller_event.delay_slot_pc == 0x8002de60u &&
        c.adapter.caller_event.entry == 0x8002a264u);
    check(c.adapter.stream_status_invocations == 1 &&
        c.adapter.stream_status_completions == 1 &&
        c.adapter.stream_status.returned_value.word == 3);
    check(c.adapter.clock_read_invocations == 1 &&
        c.adapter.clock_read_completions == 1 &&
        c.adapter.clock_read.return_v0.word == 101 &&
        c.progress.clock_delta.word == 1);
    check(c.get(0x80015028u, 2) == 0xbeef &&
        c.get(0x800170bcu, 4) == 0 && c.get(0x800fda0eu, 2) == 40);
    check(c.publish_progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
        c.publish_progress.restored_return_address.word == 0x80068d84u &&
        c.publish_progress.machine.hi.word == 0x0badc0deu &&
        c.publish_progress.machine.lo.word == 0x13572468u);
}

void clockCounterValuesKnownnessAndReturnedMachine() {
    constexpr std::array<std::uint32_t, 5> values{{
        0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu}};
    for (const auto value : values) {
        Composition c;
        c.put(0x800fda0cu, 0, 2);
        c.put(0x800e430cu, 0x01020304u, 4);
        c.put(Counter, value, 4);
        check(nba97_game_match_audio_service_with_stream_status(&c.service,
            &c.clock, &c.status, &c.progress, &c.adapter) ==
            NBA97_TEXT_COMPLETE);
        check(c.adapter.clock_read_invocations == 1 &&
            c.adapter.clock_read_completions == 1 &&
            c.adapter.clock_read.return_v0.word == value &&
            c.adapter.clock_read.return_v0.known_mask == 15 &&
            c.clock_journal[0].value == value);
        check(c.progress.clock_delta.word == value - 0x01020304u &&
            c.get(0x800e430cu, 4) == value &&
            c.progress.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_V0].word == 0x82u);
        check(c.adapter.clock_read.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_RA].word == 0x8002a278u &&
            c.adapter.clock_read.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_SP].word == EntrySp - 0x20u &&
            c.adapter.clock_read.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_T0].word ==
                c.service.machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_T0].word &&
            c.adapter.clock_read.machine.hi.word == 0x12345678u &&
            c.adapter.clock_read.machine.lo.word == 0x9abcdef0u);
        check(c.adapter.stream_status_invocations == 0 &&
            c.unresolved.empty());
    }

    for (unsigned mask = 0; mask < 16; ++mask) {
        Composition c;
        c.put(0x800fda0cu, 0, 2);
        c.put(0x800e430cu, 0, 4);
        c.put(Counter, 0x78563412u, 4);
        c.setKnown(Counter, static_cast<std::uint8_t>(mask), 4);
        check(nba97_game_match_audio_service_with_stream_status(&c.service,
            &c.clock, &c.status, &c.progress, &c.adapter) ==
            NBA97_TEXT_COMPLETE && c.progress.completed);
        check(c.adapter.clock_read.return_v0.word == 0x78563412u &&
            c.adapter.clock_read.return_v0.known_mask == mask &&
            c.clock_journal[0].known_mask == mask &&
            c.getKnown(0x800e430cu, 4) == mask);
        check(c.progress.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_V0].word == 0x82u &&
            c.progress.machine.hi.word == 0x12345678u &&
            c.progress.machine.lo.word == 0x9abcdef0u);
    }
}

void nestedClockFailurePrefix() {
    Composition limited;
    limited.clock.operation_budget = 0;
    check(nba97_game_match_audio_service_with_stream_status(&limited.service,
        &limited.clock, &limited.status, &limited.progress,
        &limited.adapter) == NBA97_TEXT_IO_REFUSED);
    check(limited.adapter.clock_read_result == NBA97_TEXT_LIMIT &&
        limited.adapter.clock_read_invocations == 1 &&
        limited.adapter.clock_read_completions == 0 &&
        limited.adapter.clock_read.operations == 0 &&
        limited.adapter.clock_read.stopped_pc == 0x800a5814u &&
        limited.adapter.clock_read.return_v0.word == 0x800d0000u);
    check(limited.progress.stopped_pc == 0x8002a270u &&
        limited.progress.stopped_entry == 0x800a5810u &&
        limited.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].word == 0x800d0000u &&
        limited.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x8002a278u &&
        limited.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == EntrySp - 0x20u &&
        limited.progress.machine.hi.word == 0x12345678u &&
        limited.progress.machine.lo.word == 0x9abcdef0u &&
        limited.adapter.stream_status_invocations == 0 &&
        limited.unresolved.empty());
}

void nestedXFailurePrefixAndGuards() {
    Composition limited;
    limited.status.operation_budget = 2;
    check(nba97_game_match_audio_service_with_stream_status(&limited.service,
        &limited.clock, &limited.status, &limited.progress,
        &limited.adapter) == NBA97_TEXT_IO_REFUSED);
    check(limited.adapter.stream_status_result == NBA97_TEXT_LIMIT &&
        limited.adapter.stream_status_invocations == 1 &&
        limited.adapter.stream_status_completions == 0 &&
        limited.adapter.stream_status.operations == 2 &&
        limited.adapter.stream_status.stopped_pc == 0x8008476cu);
    check(limited.progress.stopped_pc == 0x8002a2dcu &&
        limited.progress.stopped_entry == 0x8008472cu &&
        limited.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == EntrySp - 0x28u &&
        limited.progress.machine.hi.word == 0x12345678u &&
        limited.progress.machine.lo.word == 0x9abcdef0u);

    Composition c;
    check(nba97_game_match_audio_service_with_stream_status(nullptr,
        &c.clock, &c.status, &c.progress, &c.adapter) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_match_audio_service_with_stream_status(&c.service,
        nullptr, &c.status, &c.progress, &c.adapter) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_match_audio_service_with_stream_status(&c.service,
        &c.clock, nullptr, &c.progress, &c.adapter) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_match_audio_service_with_stream_status(&c.service,
        &c.clock, &c.status, nullptr, &c.adapter) == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    actualXAndTypedVComposition();
    directFutureAACallBoundary();
    actualAAIntoABIntoX();
    clockCounterValuesKnownnessAndReturnedMachine();
    nestedClockFailurePrefix();
    nestedXFailurePrefixAndGuards();
    std::printf("%u game match audio service integration checks passed\n",
        checks);
    return 0;
}
