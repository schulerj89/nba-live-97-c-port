#include "recovered/game_frame_interrupt_restore.h"

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
            "game frame interrupt-restore check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

bool sameWord(const Nba97GameFrameInterruptRestoreWord& a,
    const Nba97GameFrameInterruptRestoreWord& b) {
    return a.word == b.word && a.known_mask == b.known_mask;
}

struct Fixture {
    std::array<Nba97GameFrameInterruptRestoreJournal, 2> journal{};
    Nba97GameFrameInterruptRestoreContext context{};
    Nba97GameFrameInterruptRestoreProgress progress{};
    Nba97GameFrameInterruptRestoreMachine initial{};

    Fixture(std::uint32_t a0 = 0xabcdef01u, std::uint8_t a0_known = 15,
        std::uint32_t old_status = 0x13579bdfu,
        std::uint8_t old_status_known = 0) {
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            initial.registers.gpr[i] = {0x21000000u + i * 0x01020304u,
                static_cast<std::uint8_t>((i % 15u) + 1u)};
        initial.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 15};
        initial.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {a0, a0_known};
        initial.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x800490a4u, 15};
        initial.hi = {0x89abcdefu, 5};
        initial.lo = {0x76543210u, 10};
        initial.cp0_status = {old_status, old_status_known};
        context.operation_budget = 1;
        context.machine = initial;
        context.journal = journal.data();
        context.journal_capacity = journal.size();
    }

    int run() {
        return nba97_game_frame_interrupt_restore(&context, &progress);
    }
};

void checkComplete(Fixture& f, std::uint32_t value,
    std::uint8_t known_mask) {
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.operations == 1 && f.progress.cp0_writes == 1 &&
        f.progress.journal_events == 1 && !f.progress.stopped_pc);
    check(f.progress.published_status.word == value &&
        f.progress.published_status.known_mask == known_mask &&
        f.progress.machine.cp0_status.word == value &&
        f.progress.machine.cp0_status.known_mask == known_mask);
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        check(sameWord(f.progress.machine.registers.gpr[i],
            f.initial.registers.gpr[i]));
    check(sameWord(f.progress.machine.hi, f.initial.hi) &&
        sameWord(f.progress.machine.lo, f.initial.lo));
    check(f.journal[0].pc == 0x8004900cu &&
        f.journal[0].operation == 1 &&
        f.journal[0].kind ==
            NBA97_GAME_FRAME_INTERRUPT_RESTORE_CP0_WRITE &&
        f.journal[0].value == value &&
        f.journal[0].known_mask == known_mask);
}

void fixedRandomKnownnessAndIgnoredOldStatus() {
    constexpr std::array<std::uint32_t, 4> fixed{{
        0u, 1u, 0xffffffffu, 0x80000001u}};
    for (auto value : fixed)
        for (unsigned mask = 0; mask < 16; ++mask) {
            Fixture f(value, static_cast<std::uint8_t>(mask),
                0xfedcba98u, static_cast<std::uint8_t>(15u - mask));
            checkComplete(f, value, static_cast<std::uint8_t>(mask));
        }

    std::uint32_t state = 0x2468ace1u;
    for (unsigned i = 0; i < 4096; ++i) {
        state = state * 1664525u + 1013904223u;
        const auto mask = static_cast<std::uint8_t>((state >> 28u) & 15u);
        Fixture f(state, mask, ~state,
            static_cast<std::uint8_t>((state >> 20u) & 15u));
        checkComplete(f, state, mask);
    }

    Fixture unknown_old(0x10203040u, 15, 0xdeadbeefu, 0);
    checkComplete(unknown_old, 0x10203040u, 15);
    Fixture different_old(0x10203040u, 15, 0u, 15);
    checkComplete(different_old, 0x10203040u, 15);
    check(unknown_old.progress.machine.cp0_status.word ==
        different_old.progress.machine.cp0_status.word);
}

void exactBudgetAndUnknownReturnAddress() {
    Fixture before_write(0x80000001u, 11, 0x55555555u, 3);
    before_write.context.operation_budget = 0;
    check(before_write.run() == NBA97_TEXT_LIMIT &&
        before_write.progress.operations == 0 &&
        before_write.progress.cp0_writes == 0 &&
        before_write.progress.journal_events == 0 &&
        before_write.progress.stopped_pc == 0x8004900cu &&
        std::memcmp(&before_write.progress.machine, &before_write.initial,
            sizeof before_write.initial) == 0);

    Fixture exact(0x80000001u, 11, 0x55555555u, 3);
    exact.context.operation_budget = 1;
    check(exact.run() == NBA97_TEXT_COMPLETE && exact.progress.completed &&
        exact.progress.machine.cp0_status.word == 0x80000001u &&
        exact.progress.machine.cp0_status.known_mask == 11);

    Fixture unknown_ra(0xffffffffu, 7, 0, 0);
    unknown_ra.context.machine.registers.gpr[
        NBA97_MATCH_INITIALIZE_RA].known_mask = 7;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.operations == 1 &&
        unknown_ra.progress.cp0_writes == 1 &&
        unknown_ra.progress.stopped_pc == 0x80049010u &&
        unknown_ra.progress.machine.cp0_status.word == 0xffffffffu &&
        unknown_ra.progress.machine.cp0_status.known_mask == 7 &&
        unknown_ra.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].word ==
            unknown_ra.initial.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word &&
        !unknown_ra.progress.completed);
}

void validationAndRepeatability() {
    Fixture first(0x10203040u, 9, 0xa5a5a5a5u, 6);
    Fixture second(0x10203040u, 9, 0x5a5a5a5au, 1);
    check(first.run() == NBA97_TEXT_COMPLETE &&
        second.run() == NBA97_TEXT_COMPLETE &&
        sameWord(first.progress.published_status,
            second.progress.published_status) &&
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
    bad_gpr.context.machine.registers.gpr[19].known_mask = 16;
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

    Nba97GameFrameInterruptRestoreProgress progress{};
    check(nba97_game_frame_interrupt_restore(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
    Fixture valid;
    check(nba97_game_frame_interrupt_restore(&valid.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    fixedRandomKnownnessAndIgnoredOldStatus();
    exactBudgetAndUnknownReturnAddress();
    validationAndRepeatability();
    std::printf("%u game frame interrupt-restore checks passed\n", checks);
    return 0;
}
