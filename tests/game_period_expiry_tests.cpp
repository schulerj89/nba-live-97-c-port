#include "recovered/game_period_expiry.h"

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
        std::fprintf(stderr, "period expiry check %u failed at %u\n", checks,
            line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Sp = 0x800ff000u;
constexpr std::uint32_t Ra = 0x81234567u;
constexpr std::uint32_t Actor = 0x80012000u;
constexpr std::uint32_t Ball = 0x80013000u;

struct CallRecord {
    Nba97GamePeriodExpiryEvent event{};
    Nba97GamePeriodExpiryMachine machine{};
};

struct Fixture {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000, 0);
    std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x110000, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GamePeriodExpiryAccess journal[64]{};
    Nba97GamePeriodExpiryContext context{};
    Nba97GamePeriodExpiryProgress progress{};
    std::vector<CallRecord> calls;
    bool refuse{};
    bool malformed{};
    bool mutate{};
    std::uint32_t moved_sp = 0x800fe000u;

    Fixture() {
        context.memory = {&region, 1};
        context.operation_budget = 100;
        context.io = io;
        context.user = this;
        context.access_journal = journal;
        context.access_journal_capacity = std::size(journal);
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.machine.registers.gpr[i] =
                {0x22000000u + i * 0x01010101u, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Sp, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {Ra, 0x0f};
        context.machine.hi = {0x11223344u, 0x0f};
        context.machine.lo = {0x55667788u, 0x0f};
        put(0x800fdb58u, 0, 4);
        put(0x800fdbccu, 0, 2);
        put(0x800fdc34u, Actor, 4);
        put(Actor + 0x1au, 14, 1);
        put(Actor + 0xb4u, 0x7777, 2);
        put(0x800fdc48u, Ball, 4);
        put(Ball + 0x10u, 48u << 8u, 4);
        put(Ball + 0x18u, 0, 2);
        put(0x800fa034u, 0, 4);
        put(0x800fdb90u, 0x82, 2);
        put(0x800fe882u, 0, 2);
        put(0x80021d95u, 1, 1);
        put(0x800fa038u, 0, 2);
        put(0x800fdb76u, 2, 2);
        put(0x800fdb6cu, 1, 2);
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
    void set_known(std::uint32_t address, unsigned width, std::uint8_t mask) {
        auto at = offset(address);
        for (unsigned i = 0; i < width; ++i)
            known[at + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
    }
    static void write_memory(const Nba97GameTextMemory* memory,
        std::uint32_t address, std::uint32_t value) {
        for (std::size_t r = 0; r < memory->count; ++r) {
            auto& region = memory->region[r];
            if (address >= region.base && address - region.base <= region.size - 4) {
                auto at = static_cast<std::size_t>(address - region.base);
                for (unsigned i = 0; i < 4; ++i) {
                    region.data[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
                    if (region.known) region.known[at + i] = 1;
                }
                return;
            }
        }
    }
    static int io(void* user, const Nba97GameTextMemory* memory,
        const Nba97GamePeriodExpiryEvent* event,
        Nba97GamePeriodExpiryMachine* machine) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back({*event, *machine});
        if (f.mutate) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] =
                {Actor + 0x100u, 0x0f};
            machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1] = {61, 0x0f};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_T9] =
                {0xabcdef01u, 0x05};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
                {f.moved_sp, 0x0f};
            machine->hi = {0xaabbccddu, 0x03};
            machine->lo = {0x13579bdfu, 0x0a};
            write_memory(memory, f.moved_sp + 0x18u, 0x87654321u);
            write_memory(memory, f.moved_sp + 0x14u, 0x10203040u);
            write_memory(memory, f.moved_sp + 0x10u, 0x50607080u);
        }
        if (f.malformed)
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask = 0;
        return f.refuse ? 0 : 1;
    }
    int run(std::size_t budget = 100) {
        context.operation_budget = budget;
        return nba97_game_period_expiry(&context, &progress);
    }
};

void entry_and_owner_paths() {
    Fixture active;
    active.put(0x800fdb58u, 1, 4);
    check(active.run() == NBA97_TEXT_COMPLETE && active.progress.completed);
    check(active.progress.operations == 7 && active.progress.reads == 4 &&
        active.progress.stores == 3 && active.calls.empty());
    check(active.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 0);
    check(active.progress.frame_stack_pointer == Sp - 0x20u &&
        active.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == Sp &&
        active.progress.restored_return_address.word == Ra);

    Fixture negative;
    negative.put(0x800fdbccu, 0xffffu, 2);
    check(negative.run() == NBA97_TEXT_COMPLETE && negative.calls.empty());
    check(negative.get(0x800fdbccu, 2) == 0xffffu &&
        negative.get(0x800fdc34u, 4) == Actor);

    for (unsigned owner : {0u, 1u, 0x7fffu}) {
        Fixture f;
        f.put(0x800fdbccu, owner, 2);
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(f.get(Actor + 0xb4u, 2) == 30 &&
            f.get(0x800fdbccu, 2) == 0xffffu &&
            f.get(0x800fdb90u, 2) == 0 &&
            f.get(0x800fdc34u, 4) == Ball);
    }
}

void every_actor_type_and_child_contract() {
    for (unsigned type = 0; type < 256; ++type) {
        Fixture f;
        f.put(Actor + 0x1au, type, 1);
        check(f.run() == NBA97_TEXT_COMPLETE);
        bool called = type != 14 && type != 15 && type != 19;
        check(f.calls.size() == (called ? 1u : 0u));
        if (called) {
            auto& c = f.calls[0];
            check(c.event.pc == 0x800676ccu &&
                c.event.delay_slot_pc == 0x800676d0u &&
                c.event.entry == 0x800582dcu && c.event.argument_count == 2);
            check(c.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == Actor &&
                c.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == 1 &&
                c.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x800676d4u);
        }
    }

    Fixture refused;
    refused.put(Actor + 0x1au, 0, 1);
    refused.refuse = true;
    check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.progress.stopped_pc == 0x800676ccu &&
        refused.progress.stopped_entry == 0x800582dcu &&
        refused.get(Actor + 0xb4u, 2) == 0x7777u);
    Fixture malformed;
    malformed.put(Actor + 0x1au, 0, 1);
    malformed.malformed = true;
    check(malformed.run() == NBA97_TEXT_ARGUMENT);
}

void child_and_live_stack_mutation() {
    Fixture f;
    f.put(Actor + 0x1au, 0, 1);
    f.mutate = true;
    f.put(Actor + 0x100u + 0xb4u, 0x9999, 2);
    check(f.run() == NBA97_TEXT_COMPLETE && f.calls.size() == 1);
    check(f.get(Actor + 0x100u + 0xb4u, 2) == 30 &&
        f.get(Actor + 0xb4u, 2) == 0x7777u);
    check(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 61 &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T9].known_mask == 0x05 &&
        f.progress.machine.hi.word == 0xaabbccddu && f.progress.machine.hi.known_mask == 3 &&
        f.progress.machine.lo.word == 0x13579bdfu && f.progress.machine.lo.known_mask == 0x0a);
    check(f.progress.restored_return_address.word == 0x87654321u &&
        f.progress.restored_s1.word == 0x10203040u &&
        f.progress.restored_s0.word == 0x50607080u &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            f.moved_sp + 0x20u);
}

void ball_and_period_gates() {
    struct BallCase { std::uint32_t height; std::uint16_t velocity; bool timer; };
    const std::array<BallCase, 7> cases{{
        {48u << 8u, 0xffffu, false}, {48u << 8u, 0, true},
        {48u << 8u, 1, true}, {49u << 8u, 0, false},
        {0xffffcf00u, 0, true}, {0x000030ffu, 0, true},
        {0x00003100u, 0, false}}};
    for (const auto& c : cases) {
        Fixture f;
        f.put(Ball + 0x10u, c.height, 4);
        f.put(Ball + 0x18u, c.velocity, 2);
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(f.get(0x800fdb76u, 2) == (c.timer ? 1u : 2u));
    }

    struct GateCase { std::uint32_t top; std::uint16_t phase;
        std::uint16_t violation; std::uint8_t enable; bool set; };
    const std::array<GateCase, 7> gates{{
        {0, 0x82, 0, 1, true}, {0xffffffffu, 0x82, 0, 1, false},
        {0x80000000u, 0x82, 0, 1, false}, {0, 0x81, 0, 1, false},
        {0, 0x82, 1, 1, false}, {0, 0x82, 0, 0, false},
        {0, 0x82, 0, 255, true}}};
    for (const auto& g : gates) {
        Fixture f;
        f.put(0x800fdbccu, 0xffffu, 2);
        f.put(0x800fa034u, g.top, 4);
        f.put(0x800fdb90u, g.phase, 2);
        f.put(0x800fe882u, g.violation, 2);
        f.put(0x80021d95u, g.enable, 1);
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(f.get(0x800fa038u, 2) == (g.set ? 1u : 0u));
    }
}

void timer_wrap_and_signed_low_half() {
    struct TimerCase { std::uint16_t timer; std::uint16_t delta;
        std::uint16_t stored; std::uint32_t result; };
    const std::array<TimerCase, 12> cases{{
        {0,0,0,1}, {1,0,1,0}, {0x7fff,0,0x7fff,0},
        {0x8000,0,0x8000,1}, {0xffff,0,0xffff,1},
        {0,1,0xffff,1}, {1,1,0,1}, {0x7fff,1,0x7ffe,0},
        {0x8000,1,0x7fff,0}, {0xffff,1,0xfffe,1},
        {0,0xffff,1,0}, {0xffff,0xffff,0,1}}};
    for (const auto& c : cases) {
        Fixture f;
        f.put(0x800fdb76u, c.timer, 2);
        f.put(0x800fdb6cu, c.delta, 2);
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(f.get(0x800fdb76u, 2) == c.stored &&
            f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == c.result);
    }
}

void unknown_mapping_alignment_and_aliases() {
    Fixture main_unknown;
    main_unknown.set_known(0x800fdb58u, 4, 0);
    check(main_unknown.run() == NBA97_TEXT_UNKNOWN &&
        main_unknown.progress.stopped_pc == 0x8006767cu &&
        main_unknown.progress.stores == 3);

    Fixture unknown_s0;
    unknown_s0.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0] =
        {0x12340000u, 0x0c};
    unknown_s0.put(0x800fdb58u, 1, 4);
    check(unknown_s0.run() == NBA97_TEXT_COMPLETE &&
        unknown_s0.progress.restored_s0.known_mask == 0x0c);

    Fixture unknown_sp;
    unknown_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {Sp, 0x0e};
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x80067670u &&
        unknown_sp.progress.operations == 1);

    Fixture unknown_ra;
    unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {0x81230000u, 0x0c};
    unknown_ra.put(0x800fdb58u, 1, 4);
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x800677d0u &&
        unknown_ra.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == Sp);

    Fixture type_unknown;
    type_unknown.set_known(Actor + 0x1au, 1, 0);
    check(type_unknown.run() == NBA97_TEXT_UNKNOWN &&
        type_unknown.progress.stopped_pc == 0x800676b4u &&
        type_unknown.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 30 &&
        type_unknown.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0f &&
        type_unknown.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V1].known_mask == 0x0e);

    Fixture height_unknown;
    height_unknown.set_known(Ball + 0x10u, 4, 0x07);
    check(height_unknown.run() == NBA97_TEXT_UNKNOWN &&
        height_unknown.progress.stopped_pc == 0x8006771cu &&
        height_unknown.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 0);

    Fixture wrapped;
    wrapped.put(0x800fdc34u, 0xfffffff0u, 4);
    check(wrapped.run() == NBA97_TEXT_RESOURCE &&
        wrapped.progress.stopped_address == 0x0000000au);

    Fixture unaligned;
    unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {Sp + 1, 0x0f};
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80067670u);

    Fixture unmapped;
    unmapped.put(0x800fdc48u, 0, 4);
    check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.stopped_address == 0x10u);

    Fixture alias;
    alias.put(0x800fdc34u, 0x800fdb18u, 4); /* actor+b4 aliases OWNER */
    alias.put(0x800fdb32u, 14, 1);
    check(alias.run() == NBA97_TEXT_COMPLETE &&
        alias.get(0x800fdbccu, 2) == 0xffffu);
}

void every_operation_budget_prefix() {
    Fixture complete;
    check(complete.run() == NBA97_TEXT_COMPLETE);
    auto operations = complete.progress.operations;
    check(operations > 20 && operations < std::size(complete.journal));
    for (std::size_t budget = 0; budget < operations; ++budget) {
        Fixture f;
        check(f.run(budget) == NBA97_TEXT_LIMIT);
        check(f.progress.operations == budget);
        check(f.progress.access_events <= complete.progress.access_events);
        for (std::size_t i = 0; i < f.progress.access_events; ++i) {
            check(f.journal[i].pc == complete.journal[i].pc &&
                f.journal[i].address == complete.journal[i].address &&
                f.journal[i].kind == complete.journal[i].kind);
        }
    }
}
}

int main() {
    entry_and_owner_paths();
    every_actor_type_and_child_contract();
    child_and_live_stack_mutation();
    ball_and_period_gates();
    timer_wrap_and_signed_low_half();
    unknown_mapping_alignment_and_aliases();
    every_operation_budget_prefix();
    std::printf("period expiry tests passed: %u checks\n", checks);
}
