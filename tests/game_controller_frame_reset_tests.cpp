#include "recovered/game_controller_frame_reset.h"

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
            "game controller-frame reset check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Timer = 0x800fe90eu;
constexpr std::uint32_t Delta = 0x800fdb6cu;
constexpr std::uint32_t Table = 0x800fdc50u;
constexpr std::uint32_t EntrySp = 0x800ff800u;
constexpr std::uint32_t IncomingRa = 0x80068cfcu;

constexpr std::array<std::uint32_t, 20> ZeroPrefixPcs{{
    0x800675f0u, 0x800675f4u,
    0x80067634u, 0x8006763cu, 0x80067634u, 0x8006763cu,
    0x80067634u, 0x8006763cu, 0x80067634u, 0x8006763cu,
    0x80067634u, 0x8006763cu, 0x80067634u, 0x8006763cu,
    0x80067634u, 0x8006763cu, 0x80067634u, 0x8006763cu,
    0x8006764cu, 0x80067654u
}};

struct Fixture {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u, 0xcd);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    std::array<std::uint8_t, 0x100> low{}, low_known{};
    Nba97GameTextRegion regions[2] = {
        {Ram, bytes.data(), known.data(), bytes.size()},
        {0, low.data(), low_known.data(), low.size()}
    };
    std::array<Nba97GameControllerFrameResetAccess, 32> journal{};
    Nba97GameControllerFrameResetContext context{};
    Nba97GameControllerFrameResetProgress progress{};
    Nba97GameControllerFrameResetEvent event{};
    Nba97GameControllerFrameResetRegisters callback_registers{};
    std::array<std::uint32_t, 8> pointers{};
    unsigned calls{};
    unsigned refuse_call{};
    bool malformed_callback{};
    bool mutate_all{};
    std::uint32_t incoming_ra = IncomingRa;

    Fixture(std::uint16_t timer = 0, std::uint16_t delta = 1) {
        low.fill(0xcd);
        low_known.fill(1);
        context.memory = {regions, 1};
        context.operation_budget = 64;
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {
                0x41000000u + i * 0x00010101u, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            incoming_ra, 0x0f};
        put(Timer, timer, 2);
        put(Delta, delta, 2);
        for (unsigned i = 0; i < pointers.size(); ++i) {
            pointers[i] = 0x80001000u + i * 0x100u;
            put(Table + i * 4u, pointers[i], 4);
            put(pointers[i] + 0x28u, 0xa100u + i, 2);
        }
    }

    std::uint8_t* data(std::uint32_t address) {
        return address < Ram ? low.data() + address :
            bytes.data() + (address - Ram);
    }
    std::uint8_t* knowledge(std::uint32_t address) {
        return address < Ram ? low_known.data() + address :
            known.data() + (address - Ram);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width,
        std::uint8_t mask = 0x0f) {
        auto* p = data(address);
        auto* k = knowledge(address);
        for (unsigned i = 0; i < width; ++i) {
            p[i] = static_cast<std::uint8_t>(value >> (8u * i));
            k[i] = static_cast<std::uint8_t>((mask >> i) & 1u);
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        const std::uint8_t* p = address < Ram ? low.data() + address :
            bytes.data() + (address - Ram);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(p[i]) << (8u * i);
        return value;
    }
    std::uint8_t getKnown(std::uint32_t address, unsigned width) const {
        const std::uint8_t* p = address < Ram ?
            low_known.data() + address : known.data() + (address - Ram);
        std::uint8_t mask = 0;
        for (unsigned i = 0; i < width; ++i)
            mask = static_cast<std::uint8_t>(mask | (p[i] << i));
        return mask;
    }
    int run() {
        return nba97_game_controller_frame_reset(&context, &progress);
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameControllerFrameResetEvent* event,
        Nba97GameControllerFrameResetRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        ++f.calls;
        f.event = *event;
        f.callback_registers = *registers;
        if (f.refuse_call)
            return 0;
        if (f.malformed_callback) {
            registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
            return 1;
        }
        if (f.mutate_all) {
            for (unsigned i = 0;
                i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
                registers->gpr[i] = {
                    0x71000000u + i * 0x00010101u,
                    static_cast<std::uint8_t>((i % 15u) + 1u)};
            registers->gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
                EntrySp + 0x100u, 0x0f};
            f.put(EntrySp + 0x118u, 0x81234560u, 4, 5);
        }
        return 1;
    }
};

void zero_timer_and_exact_order() {
    Fixture f(0, 0xffff);
    const auto incoming = f.context.registers;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations == 20 && f.progress.accesses == 19 &&
        f.progress.reads == 10 && f.progress.stores == 9 &&
        f.progress.callbacks_completed == 1 && f.calls == 1);
    check(!f.progress.timer_updated && !f.progress.timer_clamped &&
        f.progress.controller_slots_cleared == 8 && f.get(Timer, 2) == 0);
    check(f.event.pc == 0x8006764cu &&
        f.event.delay_slot_pc == 0x80067650u &&
        f.event.entry == 0x80083eecu && f.event.operation == 19 &&
        f.event.kind == NBA97_GAME_CONTROLLER_FRAME_RESET_83EEC &&
        f.event.argument_count == 0);
    check(f.progress.access_events == 19 &&
        f.journal[0].pc == 0x800675f0u &&
        f.journal[0].address == EntrySp - 8u &&
        f.journal[0].kind == NBA97_GAME_CONTROLLER_FRAME_RESET_STORE &&
        f.journal[1].pc == 0x800675f4u &&
        f.journal[1].address == Timer);
    for (unsigned i = 0; i < 8; ++i) {
        const auto& load = f.journal[2 + i * 2u];
        const auto& store = f.journal[3 + i * 2u];
        check(load.pc == 0x80067634u && load.address == Table + i * 4u &&
            load.value == f.pointers[i] && load.width == 4 &&
            load.kind == NBA97_GAME_CONTROLLER_FRAME_RESET_READ);
        check(store.pc == 0x8006763cu &&
            store.address == f.pointers[i] + 0x28u && store.value == 0 &&
            store.width == 2 &&
            store.kind == NBA97_GAME_CONTROLLER_FRAME_RESET_STORE &&
            f.get(f.pointers[i] + 0x28u, 2) == 0);
    }
    check(f.journal[18].pc == 0x80067654u &&
        f.journal[18].address == EntrySp - 8u &&
        f.progress.restored_return_address.word == IncomingRa);
    check(f.callback_registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 8 &&
        f.callback_registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 0 &&
        f.callback_registers.gpr[NBA97_MATCH_INITIALIZE_V1].word ==
            Table + 0x20u &&
        f.callback_registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp - 0x20u &&
        f.callback_registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80067654u);
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        EntrySp &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            IncomingRa);
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        if (i == NBA97_MATCH_INITIALIZE_V0 ||
            i == NBA97_MATCH_INITIALIZE_V1 ||
            i == NBA97_MATCH_INITIALIZE_A0 ||
            i == NBA97_MATCH_INITIALIZE_SP ||
            i == NBA97_MATCH_INITIALIZE_RA)
            continue;
        check(f.progress.registers.gpr[i].word == incoming.gpr[i].word &&
            f.progress.registers.gpr[i].known_mask ==
                incoming.gpr[i].known_mask);
    }
}

void timer_boundaries_and_wrapping_clamp() {
    constexpr std::array<std::uint16_t, 5> timers{{
        1, 0x7fff, 0x8000, 0xffff, 0
    }};
    constexpr std::array<std::uint16_t, 4> deltas{{0, 1, 2, 0xffff}};
    for (auto timer : timers) {
        for (auto delta : deltas) {
            Fixture f(timer, delta);
            check(f.run() == NBA97_TEXT_COMPLETE);
            if (timer == 0) {
                check(!f.progress.timer_updated &&
                    f.progress.operations == 20 &&
                    f.get(Timer, 2) == 0);
                bool read_delta = false;
                for (std::size_t i = 0; i < f.progress.access_events; ++i)
                    read_delta |= f.journal[i].address == Delta;
                check(!read_delta);
                continue;
            }
            const std::uint16_t adjusted =
                static_cast<std::uint16_t>(timer - delta);
            const bool clamp = (adjusted & 0x8000u) != 0;
            check(f.progress.timer_updated &&
                f.progress.timer_clamped == clamp &&
                f.get(Timer, 2) == (clamp ? 0 : adjusted) &&
                f.progress.initial_timer.word ==
                    ((timer & 0x8000u) ? 0xffff0000u | timer : timer) &&
                f.progress.delta.word == delta);
            check(f.progress.operations == (clamp ? 23u : 22u));
        }
    }

    Fixture wrappedPositive(0x8000, 1);
    check(wrappedPositive.run() == NBA97_TEXT_COMPLETE &&
        wrappedPositive.get(Timer, 2) == 0x7fffu &&
        !wrappedPositive.progress.timer_clamped &&
        wrappedPositive.progress.adjusted_timer.word == 0xffff7fffu);
    Fixture wrappedNegative(0x7fff, 0xffff);
    check(wrappedNegative.run() == NBA97_TEXT_COMPLETE &&
        wrappedNegative.get(Timer, 2) == 0 &&
        wrappedNegative.progress.timer_clamped &&
        wrappedNegative.progress.adjusted_timer.word == 0xffff8000u);
}

void live_pointers_alias_zero_wrap_and_overlap() {
    Fixture aliases;
    for (unsigned i = 0; i < 8; ++i)
        aliases.put(Table + i * 4u, 0x80001800u, 4);
    aliases.put(0x80001828u, 0xbeef, 2);
    check(aliases.run() == NBA97_TEXT_COMPLETE &&
        aliases.get(0x80001828u, 2) == 0);
    for (unsigned i = 0; i < 8; ++i)
        check(aliases.journal[3 + i * 2u].address == 0x80001828u);

    Fixture zero;
    zero.context.memory.count = 2;
    zero.put(Table, 0, 4);
    zero.put(0x28u, 0xbeef, 2);
    check(zero.run() == NBA97_TEXT_COMPLETE && zero.get(0x28u, 2) == 0 &&
        zero.journal[3].address == 0x28u);

    Fixture wrap;
    wrap.context.memory.count = 2;
    wrap.put(Table, 0xfffffff0u, 4);
    wrap.put(0x18u, 0xbeef, 2);
    check(wrap.run() == NBA97_TEXT_COMPLETE && wrap.get(0x18u, 2) == 0 &&
        wrap.journal[3].address == 0x18u);

    Fixture overlap;
    overlap.put(Table, Table + 4u - 0x28u, 4);
    overlap.put(Table + 4u, 0x80001234u, 4);
    overlap.put(0x80000028u, 0xbeef, 2);
    check(overlap.run() == NBA97_TEXT_COMPLETE &&
        overlap.journal[3].address == Table + 4u &&
        overlap.journal[5].address == 0x80000028u &&
        overlap.get(0x80000028u, 2) == 0);
}

void unknowns_and_failure_prefixes() {
    Fixture unknownBranch;
    unknownBranch.put(Timer, 0, 2, 2);
    check(unknownBranch.run() == NBA97_TEXT_UNKNOWN &&
        unknownBranch.progress.operations == 2 &&
        unknownBranch.progress.stopped_pc == 0x800675fcu &&
        unknownBranch.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .known_mask == 0x0e &&
        unknownBranch.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V1]
            .known_mask == 0x0e);

    Fixture partialTimer(1, 0);
    partialTimer.put(Timer, 1, 2, 1);
    check(partialTimer.run() == NBA97_TEXT_UNKNOWN &&
        partialTimer.progress.operations == 4 &&
        partialTimer.progress.stores == 2 &&
        partialTimer.progress.stopped_pc == 0x8006761cu &&
        partialTimer.get(Timer, 2) == 1 &&
        partialTimer.getKnown(Timer, 2) == 1 &&
        partialTimer.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .known_mask == 7);

    Fixture unknownPointer;
    unknownPointer.put(Table, 0x80001000u, 4, 0x0e);
    check(unknownPointer.run() == NBA97_TEXT_UNKNOWN &&
        unknownPointer.progress.operations == 3 &&
        unknownPointer.progress.stopped_pc == 0x8006763cu &&
        unknownPointer.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
            .word == 1 && unknownPointer.progress.controller_slots_cleared == 0);

    Fixture unknownRa;
    unknownRa.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 5;
    check(unknownRa.run() == NBA97_TEXT_UNKNOWN &&
        unknownRa.progress.operations == 20 &&
        unknownRa.progress.stopped_pc == 0x8006765cu &&
        unknownRa.progress.restored_return_address.known_mask == 5 &&
        unknownRa.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp);

    Fixture unknownSp;
    unknownSp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
        EntrySp, 0};
    check(unknownSp.run() == NBA97_TEXT_UNKNOWN &&
        unknownSp.progress.operations == 0 &&
        unknownSp.progress.stopped_pc == 0x800675f0u);

    Fixture unaligned;
    unaligned.put(Table, 0x80001001u, 4);
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.operations == 4 &&
        unaligned.progress.stopped_pc == 0x8006763cu &&
        unaligned.progress.controller_slots_cleared == 0);

    Fixture unmapped;
    unmapped.put(Table + 3u * 4u, 0x70000000u, 4);
    check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.controller_slots_cleared == 3 &&
        unmapped.progress.operations == 10 &&
        unmapped.progress.stopped_address == 0x70000028u);

    Fixture malformedKnown;
    malformedKnown.knowledge(Timer)[0] = 2;
    check(malformedKnown.run() == NBA97_TEXT_ARGUMENT &&
        malformedKnown.progress.operations == 2);

    Fixture untrackedUnknown;
    untrackedUnknown.regions[0].known = nullptr;
    untrackedUnknown.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = 5;
    check(untrackedUnknown.run() == NBA97_TEXT_ARGUMENT &&
        untrackedUnknown.progress.operations == 1);
}

void callback_mutation_refusal_and_stack_wrap() {
    Fixture mutated;
    mutated.mutate_all = true;
    check(mutated.run() == NBA97_TEXT_UNKNOWN &&
        mutated.progress.callbacks_completed == 1 &&
        mutated.progress.stopped_pc == 0x8006765cu &&
        mutated.progress.restored_return_address.word == 0x81234560u &&
        mutated.progress.restored_return_address.known_mask == 5 &&
        mutated.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp + 0x120u);
    for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        if (i == NBA97_MATCH_INITIALIZE_SP ||
            i == NBA97_MATCH_INITIALIZE_RA)
            continue;
        check(mutated.progress.registers.gpr[i].word ==
            0x71000000u + i * 0x00010101u);
    }

    Fixture refused;
    refused.refuse_call = 1;
    check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.progress.operations == 19 && refused.calls == 1 &&
        refused.progress.callbacks_completed == 0 &&
        refused.progress.stopped_pc == 0x8006764cu &&
        refused.progress.stopped_entry == 0x80083eecu &&
        refused.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80067654u);

    Fixture noCallback;
    noCallback.context.io = nullptr;
    check(noCallback.run() == NBA97_TEXT_IO_REFUSED &&
        noCallback.progress.operations == 19);

    Fixture malformed;
    malformed.malformed_callback = true;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations == 19 && malformed.calls == 1);

    Fixture wrap;
    wrap.context.memory.count = 2;
    wrap.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {8, 0x0f};
    check(wrap.run() == NBA97_TEXT_COMPLETE &&
        wrap.progress.frame_stack_pointer == 0xffffffe8u &&
        wrap.journal[0].address == 0 &&
        wrap.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 8 &&
        wrap.progress.restored_return_address.word == IncomingRa);
}

void every_operation_budget_prefix() {
    for (std::size_t budget = 0; budget < ZeroPrefixPcs.size(); ++budget) {
        Fixture f;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT &&
            f.progress.operations == budget && !f.progress.completed &&
            f.progress.stopped_pc == ZeroPrefixPcs[budget]);
    }
    Fixture exactZero;
    exactZero.context.operation_budget = ZeroPrefixPcs.size();
    check(exactZero.run() == NBA97_TEXT_COMPLETE);

    std::array<std::uint32_t, 22> positive{};
    positive[0] = 0x800675f0u;
    positive[1] = 0x800675f4u;
    positive[2] = 0x80067608u;
    positive[3] = 0x80067614u;
    for (unsigned i = 0; i < 8; ++i) {
        positive[4 + i * 2u] = 0x80067634u;
        positive[5 + i * 2u] = 0x8006763cu;
    }
    positive[20] = 0x8006764cu;
    positive[21] = 0x80067654u;
    for (std::size_t budget = 0; budget < positive.size(); ++budget) {
        Fixture f(2, 1);
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT &&
            f.progress.operations == budget &&
            f.progress.stopped_pc == positive[budget]);
    }
    Fixture exactPositive(2, 1);
    exactPositive.context.operation_budget = positive.size();
    check(exactPositive.run() == NBA97_TEXT_COMPLETE);

    std::array<std::uint32_t, 23> negative{};
    negative[0] = 0x800675f0u;
    negative[1] = 0x800675f4u;
    negative[2] = 0x80067608u;
    negative[3] = 0x80067614u;
    negative[4] = 0x80067624u;
    for (unsigned i = 0; i < 8; ++i) {
        negative[5 + i * 2u] = 0x80067634u;
        negative[6 + i * 2u] = 0x8006763cu;
    }
    negative[21] = 0x8006764cu;
    negative[22] = 0x80067654u;
    for (std::size_t budget = 0; budget < negative.size(); ++budget) {
        Fixture f(1, 2);
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT &&
            f.progress.operations == budget &&
            f.progress.stopped_pc == negative[budget]);
    }
    Fixture exactNegative(1, 2);
    exactNegative.context.operation_budget = negative.size();
    check(exactNegative.run() == NBA97_TEXT_COMPLETE);
}

void argument_validation() {
    Nba97GameControllerFrameResetProgress progress{};
    check(nba97_game_controller_frame_reset(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_controller_frame_reset(&f.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
    f.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
    check(f.run() == NBA97_TEXT_ARGUMENT);
    Fixture badMask;
    badMask.context.registers.gpr[NBA97_MATCH_INITIALIZE_T0].known_mask = 0x10;
    check(badMask.run() == NBA97_TEXT_ARGUMENT);
    Fixture noJournal;
    noJournal.context.access_journal = nullptr;
    noJournal.context.access_journal_capacity = 1;
    check(noJournal.run() == NBA97_TEXT_ARGUMENT);
    Fixture overlap;
    Nba97GameTextRegion duplicate[2] = {overlap.regions[0],
        overlap.regions[0]};
    overlap.context.memory = {duplicate, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    zero_timer_and_exact_order();
    timer_boundaries_and_wrapping_clamp();
    live_pointers_alias_zero_wrap_and_overlap();
    unknowns_and_failure_prefixes();
    callback_mutation_refusal_and_stack_wrap();
    every_operation_budget_prefix();
    argument_validation();
    std::printf("game controller-frame reset: %u checks passed\n", checks);
}
