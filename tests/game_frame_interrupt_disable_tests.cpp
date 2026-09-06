#include "recovered/game_frame_interrupt_disable.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "game frame interrupt-disable check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

bool sameWord(const Nba97GameFrameInterruptDisableWord& a,
    const Nba97GameFrameInterruptDisableWord& b) {
    return a.word == b.word && a.known_mask == b.known_mask;
}

struct Fixture {
    std::array<Nba97GameFrameInterruptDisableJournal, 4> journal{};
    Nba97GameFrameInterruptDisableContext context{};
    Nba97GameFrameInterruptDisableProgress progress{};
    Nba97GameFrameInterruptDisableMachine initial{};

    Fixture(std::uint32_t status = 0xabcdef01u,
        std::uint8_t status_known = 15) {
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            initial.registers.gpr[i] = {0x12000000u + i * 0x01020304u,
                static_cast<std::uint8_t>((i % 15u) + 1u)};
        initial.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 15};
        initial.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80049074u, 15};
        initial.hi = {0x89abcdefu, 5};
        initial.lo = {0x76543210u, 10};
        initial.cp0_status = {status, status_known};
        context.operation_budget = 2;
        context.machine = initial;
        context.journal = journal.data();
        context.journal_capacity = journal.size();
    }

    int run() {
        return nba97_game_frame_interrupt_disable(&context, &progress);
    }
};

void checkComplete(Fixture& f, std::uint32_t status,
    std::uint8_t known_mask) {
    const auto expected_new = status & 0xfffffffeu;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations == 2 && f.progress.cp0_reads == 1 &&
        f.progress.cp0_writes == 1 && f.progress.journal_events == 2 &&
        !f.progress.stopped_pc);
    check(f.progress.old_status.word == status &&
        f.progress.old_status.known_mask == known_mask &&
        f.progress.new_status.word == expected_new &&
        f.progress.new_status.known_mask == known_mask &&
        f.progress.machine.cp0_status.word == expected_new &&
        f.progress.machine.cp0_status.known_mask == known_mask);
    check(sameWord(f.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0], {status, known_mask}) &&
        sameWord(f.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V1], {expected_new, known_mask}));
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (i != NBA97_MATCH_INITIALIZE_V0 &&
            i != NBA97_MATCH_INITIALIZE_V1)
            check(sameWord(f.progress.machine.registers.gpr[i],
                f.initial.registers.gpr[i]));
    check(sameWord(f.progress.machine.hi, f.initial.hi) &&
        sameWord(f.progress.machine.lo, f.initial.lo));
    check(f.journal[0].pc == 0x80048ff4u &&
        f.journal[0].operation == 1 &&
        f.journal[0].kind ==
            NBA97_GAME_FRAME_INTERRUPT_DISABLE_CP0_READ &&
        f.journal[0].value == status &&
        f.journal[0].known_mask == known_mask);
    check(f.journal[1].pc == 0x80049000u &&
        f.journal[1].operation == 2 &&
        f.journal[1].kind ==
            NBA97_GAME_FRAME_INTERRUPT_DISABLE_CP0_WRITE &&
        f.journal[1].value == expected_new &&
        f.journal[1].known_mask == known_mask);
}

void fixedRandomAndKnownness() {
    constexpr std::array<std::uint32_t, 6> fixed{{
        0u, 1u, 2u, 3u, 0xffffffffu, 0x80000001u}};
    for (auto value : fixed)
        for (unsigned mask = 0; mask < 16; ++mask) {
            Fixture f(value, static_cast<std::uint8_t>(mask));
            checkComplete(f, value, static_cast<std::uint8_t>(mask));
        }

    std::uint32_t state = 0x13579bdfu;
    for (unsigned i = 0; i < 4096; ++i) {
        state = state * 1664525u + 1013904223u;
        const auto mask = static_cast<std::uint8_t>((state >> 28u) & 15u);
        Fixture f(state, mask);
        checkComplete(f, state, mask);
    }
}

void exactBudgetsAndUnknownReturnAddress() {
    Fixture before_read;
    before_read.context.operation_budget = 0;
    check(before_read.run() == NBA97_TEXT_LIMIT &&
        before_read.progress.operations == 0 &&
        before_read.progress.stopped_pc == 0x80048ff4u &&
        before_read.progress.journal_events == 0 &&
        std::memcmp(&before_read.progress.machine, &before_read.initial,
            sizeof before_read.initial) == 0);

    Fixture before_write;
    before_write.context.operation_budget = 1;
    check(before_write.run() == NBA97_TEXT_LIMIT &&
        before_write.progress.operations == 1 &&
        before_write.progress.cp0_reads == 1 &&
        before_write.progress.cp0_writes == 0 &&
        before_write.progress.stopped_pc == 0x80049000u &&
        before_write.progress.old_status.word == 0xabcdef01u &&
        before_write.progress.new_status.word == 0xabcdef00u &&
        before_write.progress.machine.cp0_status.word == 0xabcdef01u &&
        before_write.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].word == 0xabcdef01u &&
        before_write.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V1].word == 0xabcdef00u &&
        before_write.progress.journal_events == 1);

    Fixture exact;
    exact.context.operation_budget = 2;
    check(exact.run() == NBA97_TEXT_COMPLETE && exact.progress.completed &&
        exact.progress.machine.cp0_status.word == 0xabcdef00u);

    Fixture unknown_ra(0x80000001u, 15);
    unknown_ra.context.machine.registers.gpr[
        NBA97_MATCH_INITIALIZE_RA].known_mask = 7;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.operations == 2 &&
        unknown_ra.progress.cp0_writes == 1 &&
        unknown_ra.progress.stopped_pc == 0x80049004u &&
        unknown_ra.progress.machine.cp0_status.word == 0x80000000u &&
        unknown_ra.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].word == 0x80000001u &&
        unknown_ra.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V1].word == 0x80000000u &&
        !unknown_ra.progress.completed);
}

void validationAndRepeatability() {
    Fixture first(0x10203041u, 11);
    Fixture second(0x10203041u, 11);
    check(first.run() == NBA97_TEXT_COMPLETE &&
        second.run() == NBA97_TEXT_COMPLETE &&
        std::memcmp(&first.progress, &second.progress,
            sizeof first.progress) == 0 &&
        std::memcmp(first.journal.data(), second.journal.data(),
            sizeof first.journal) == 0);

    Fixture bad_journal;
    bad_journal.context.journal = nullptr;
    check(bad_journal.run() == NBA97_TEXT_ARGUMENT &&
        !bad_journal.progress.operations);
    Fixture bad_zero;
    bad_zero.context.machine.registers.gpr[0] = {1, 15};
    check(bad_zero.run() == NBA97_TEXT_ARGUMENT);
    Fixture unknown_zero;
    unknown_zero.context.machine.registers.gpr[0] = {0, 14};
    check(unknown_zero.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_gpr;
    bad_gpr.context.machine.registers.gpr[17].known_mask = 16;
    check(bad_gpr.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_hi;
    bad_hi.context.machine.hi.known_mask = 16;
    check(bad_hi.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_lo;
    bad_lo.context.machine.lo.known_mask = 16;
    check(bad_lo.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_status;
    bad_status.context.machine.cp0_status.known_mask = 16;
    check(bad_status.run() == NBA97_TEXT_ARGUMENT);

    Nba97GameFrameInterruptDisableProgress progress{};
    check(nba97_game_frame_interrupt_disable(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
    Nba97GameFrameInterruptDisableContext empty{};
    check(nba97_game_frame_interrupt_disable(&empty, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    fixedRandomAndKnownness();
    exactBudgetsAndUnknownReturnAddress();
    validationAndRepeatability();
    std::printf("%u game frame interrupt-disable checks passed\n", checks);
    return 0;
}
