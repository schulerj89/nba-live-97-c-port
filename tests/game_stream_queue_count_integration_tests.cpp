#include "game_stream_queue_count_adapter.h"

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
            "stream queue count integration check %u failed at %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x800ff000u;
constexpr std::uint32_t Head = 0x800c43a0u;
constexpr std::uint32_t NodeA = 0x80090000u;
constexpr std::uint32_t NodeB = 0x80090020u;
constexpr std::uint32_t NodeC = 0x80090040u;

struct Composition {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameStreamReadinessContext readiness{};
    Nba97GameStreamQueueCountContext queue{};
    Nba97GameStreamReadinessProgress readiness_progress{};
    Nba97GameStreamQueueCountAdapterProgress adapter{};
    std::vector<Nba97GameStreamQueueCountEvent> services;

    Composition() {
        readiness.memory = {&region, 1};
        readiness.operation_budget = 6;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            readiness.machine.registers.gpr[i] = {
                0x41000000u + i * 0x00010101u,
                static_cast<std::uint8_t>((i % 15u) + 1u)};
        readiness.machine.registers.gpr[0] = {0, 15};
        readiness.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {EntrySp, 15};
        readiness.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x8002a2f4u, 15};
        readiness.machine.hi = {0x12345678u, 3};
        readiness.machine.lo = {0x9abcdef0u, 12};
        queue.operation_budget = 200;
        queue.io = io;
        queue.user = this;
        put(0x800f0fdcu, 1);
        put(0x800c4410u, 0);
        put(Head, 0);
        put(NodeA, 0);
        put(NodeB, 0);
        put(NodeC, 0);
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Ram);
    }
    void put(std::uint32_t address, std::uint32_t value) {
        const auto at = offset(address);
        for (unsigned i = 0; i < 4; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            known[at + i] = 1;
        }
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameStreamQueueCountEvent* event,
        Nba97GameStreamQueueCountMachine* machine) {
        auto& c = *static_cast<Composition*>(user);
        c.services.push_back(*event);
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0] =
            {0xfeedbeefu, 9};
        machine->hi = {0x0badc0deu, 5};
        machine->lo = {0xc001d00du, 10};
        return 1;
    }
    int run() {
        return nba97_game_stream_readiness_with_queue_count(&readiness,
            &queue, &readiness_progress, &adapter);
    }
};

void ActualAdResultDomainAndFlagGate() {
    struct Case {
        std::uint32_t head;
        std::uint32_t a;
        std::uint32_t b;
        std::uint32_t raw;
        std::uint32_t ready;
    };
    const Case cases[] = {
        {0, 0, 0, 0xffffffffu, 1},
        {0xfffffffeu, 0, 0, 0, 1},
        {NodeA, 0, 0, 0, 1},
        {NodeA, NodeB, 0, 1, 1},
        {NodeA, NodeB, NodeC, 2, 0}};
    for (const auto& expected : cases) {
        Composition c;
        c.put(Head, expected.head);
        c.put(NodeA, expected.a);
        c.put(NodeB, expected.b);
        c.put(NodeC, 0);
        check(c.run() == NBA97_TEXT_COMPLETE &&
            c.readiness_progress.completed &&
            c.adapter.queue_result == NBA97_TEXT_COMPLETE &&
            c.adapter.queue_invocations == 1 &&
            c.adapter.queue_completions == 1);
        check(c.adapter.queue_count.returned_count.word == expected.raw &&
            c.readiness_progress.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_V0].word == expected.ready);
        check(c.adapter.queue_event.pc == 0x80088d30u &&
            c.adapter.queue_event.delay_slot_pc == 0x80088d34u &&
            c.adapter.queue_event.entry == 0x80084448u &&
            c.adapter.queue_event.argument_count == 0 &&
            c.adapter.queue_event.operation == 4 &&
            c.adapter.queue_event.invocation == 1);
        check(c.adapter.queue_count.frame_stack_pointer == EntrySp - 0x38u &&
            c.adapter.queue_count.saved_return_address.word == 0x80088d38u &&
            c.adapter.queue_count.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_SP].word == EntrySp - 0x18u &&
            c.readiness_progress.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_SP].word == EntrySp);
        if (expected.head == 0)
            check(c.services.empty());
        else
            check(c.services.size() == 2 &&
                c.services[0].entry == 0x80093d94u &&
                c.services[1].entry == 0x80093dd4u &&
                c.readiness_progress.machine.hi.word == 0x0badc0deu &&
                c.readiness_progress.machine.lo.word == 0xc001d00du);
    }

    Composition disabled;
    disabled.put(0x800f0fdcu, 0);
    disabled.put(Head, NodeA);
    disabled.put(NodeA, NodeB);
    check(disabled.run() == NBA97_TEXT_COMPLETE &&
        disabled.readiness_progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].word == 0 &&
        disabled.adapter.queue_invocations == 0 && disabled.services.empty());
}

void DirectValidationAndNestedPrefix() {
    Composition c;
    Nba97GameStreamReadinessEvent event{};
    event.kind = NBA97_GAME_STREAM_READINESS_CHILD_80084448;
    event.pc = 0x80088d30u;
    event.delay_slot_pc = 0x80088d34u;
    event.entry = 0x80084448u;
    event.operation = 4;
    event.invocation = 1;
    auto machine = c.readiness.machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80088d38u, 15};
    Nba97GameStreamQueueCountProgress progress{};
    check(nba97_game_stream_queue_count_from_stream_readiness(
        &c.readiness.memory, &event, &machine, &c.queue, &progress) ==
        NBA97_TEXT_COMPLETE && progress.completed);

    auto wrong = event;
    wrong.pc += 4;
    check(nba97_game_stream_queue_count_from_stream_readiness(
        &c.readiness.memory, &wrong, &machine, &c.queue, &progress) ==
        NBA97_TEXT_ARGUMENT);
    wrong = event;
    wrong.invocation = 0;
    check(nba97_game_stream_queue_count_from_stream_readiness(
        &c.readiness.memory, &wrong, &machine, &c.queue, &progress) ==
        NBA97_TEXT_ARGUMENT);
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ^= 4u;
    check(nba97_game_stream_queue_count_from_stream_readiness(
        &c.readiness.memory, &event, &machine, &c.queue, &progress) ==
        NBA97_TEXT_ARGUMENT);

    Composition limited;
    limited.queue.operation_budget = 4;
    check(limited.run() == NBA97_TEXT_IO_REFUSED &&
        limited.adapter.queue_result == NBA97_TEXT_LIMIT &&
        limited.adapter.queue_invocations == 1 &&
        limited.adapter.queue_completions == 0 &&
        limited.adapter.queue_count.stopped_pc == 0x80084574u &&
        limited.readiness_progress.stopped_pc == 0x80088d30u &&
        limited.readiness_progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x80088d38u);
}
}

int main() {
    ActualAdResultDomainAndFlagGate();
    DirectValidationAndNestedPrefix();
    std::printf("%u game stream queue count integration checks passed\n",
        checks);
    return 0;
}
