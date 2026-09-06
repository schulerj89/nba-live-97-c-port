#include "recovered/game_audio_stream_status.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "game audio stream status check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Globals = 0x800c4300u;
constexpr std::uint32_t Flags = 0x800c43b0u;
constexpr std::uint32_t Busy = 0x800c43b1u;
constexpr std::uint32_t Stack = 0x807ff000u;
constexpr std::uint32_t EntrySp = 0x807fff00u;
constexpr std::uint32_t Frame = EntrySp - 8u;
constexpr std::uint32_t CallerRa = 0x80083f08u;

bool sameWord(const Nba97GameAudioStreamStatusWord& a,
    const Nba97GameAudioStreamStatusWord& b) {
    return a.word == b.word && a.known_mask == b.known_mask;
}

struct Fixture {
    std::array<std::uint8_t, 0x200> globals{};
    std::array<std::uint8_t, 0x200> globals_known{};
    std::array<std::uint8_t, 0x1000> stack{};
    std::array<std::uint8_t, 0x1000> stack_known{};
    std::array<Nba97GameTextRegion, 2> regions{{
        {Globals, globals.data(), globals_known.data(), globals.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}}};
    std::array<Nba97GameAudioStreamStatusAccess, 16> journal{};
    Nba97GameAudioStreamStatusContext context{};
    Nba97GameAudioStreamStatusProgress progress{};
    Nba97GameAudioStreamStatusRegisters initial{};

    Fixture(std::uint8_t flags = 7, std::uint8_t busy = 0) {
        globals_known.fill(1);
        stack.fill(0xcd);
        stack_known.fill(1);
        globals[Flags - Globals] = flags;
        globals[Busy - Globals] = busy;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            initial.gpr[i] = {0x11000000u + i * 0x01020304u,
                static_cast<std::uint8_t>(i == 30 ? 5 : 15)};
        initial.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 15};
        initial.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 15};
        initial.gpr[NBA97_MATCH_INITIALIZE_FP] = {0xa1b2c3d4u, 5};
        initial.gpr[NBA97_MATCH_INITIALIZE_RA] = {CallerRa, 15};
        context.memory = {regions.data(), regions.size()};
        context.operation_budget = 16;
        context.registers = initial;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
    }

    int run() {
        return nba97_game_audio_stream_status(&context, &progress);
    }
};

void allFlagsBusyAndRegisterResults() {
    check(NBA97_GAME_AUDIO_STREAM_STATUS_BODY_BYTES == 196 &&
        NBA97_GAME_AUDIO_STREAM_STATUS_BODY_INSTRUCTIONS == 49 &&
        NBA97_GAME_AUDIO_STREAM_STATUS_SPAN_BYTES == 228 &&
        NBA97_GAME_AUDIO_STREAM_STATUS_SPAN_WORDS == 57 &&
        NBA97_GAME_AUDIO_STREAM_STATUS_EXCLUDED_PAIRS == 4);
    for (unsigned flags = 0; flags < 256; ++flags) {
        Fixture f(static_cast<std::uint8_t>(flags), 0);
        const std::uint32_t expected = !(flags & 2u) ? 0xfffffff2u :
            !(flags & 1u) ? 1u : (flags & 4u) ? 3u : 1u;
        check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
        check(f.progress.returned_value.word == expected &&
            f.progress.returned_value.known_mask == 15 &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == expected);
        const std::uint32_t expected_v1 = !(flags & 2u) ? 0u :
            !(flags & 1u) ? 0u : flags & 4u;
        check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V1].word ==
            expected_v1 &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V1].known_mask == 15);
        check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp && sameWord(f.progress.restored_s8,
                f.initial.gpr[NBA97_MATCH_INITIALIZE_FP]) &&
            sameWord(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
                f.initial.gpr[NBA97_MATCH_INITIALIZE_RA]));
        for (unsigned r = 0; r < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++r)
            if (r != NBA97_MATCH_INITIALIZE_V0 &&
                r != NBA97_MATCH_INITIALIZE_V1 &&
                r != NBA97_MATCH_INITIALIZE_SP &&
                r != NBA97_MATCH_INITIALIZE_FP)
                check(sameWord(f.progress.registers.gpr[r], f.initial.gpr[r]));
    }

    for (unsigned busy : {1u, 255u}) {
        for (unsigned flags = 0; flags < 256; ++flags) {
            Fixture f(static_cast<std::uint8_t>(flags),
                static_cast<std::uint8_t>(busy));
            const std::uint32_t expected = (flags & 2u) ? 4u : 0xfffffff2u;
            check(f.run() == NBA97_TEXT_COMPLETE &&
                f.progress.returned_value.word == expected);
            check((flags & 2u) ? f.progress.access_events == 4u :
                f.progress.access_events == 3u);
        }
    }
}

void exactAccessPathsAndAliases() {
    Fixture full(7, 0);
    check(full.run() == NBA97_TEXT_COMPLETE && full.progress.operations == 6 &&
        full.progress.accesses == 6 && full.progress.reads == 5 &&
        full.progress.stores == 1 && full.progress.access_events == 6);
    const std::array<std::uint32_t, 6> pcs{{0x80084730u, 0x8008473cu,
        0x8008476cu, 0x80084794u, 0x800847c4u, 0x80084800u}};
    const std::array<std::uint32_t, 6> addresses{{Frame, Flags, Busy, Flags,
        Flags, Frame}};
    for (unsigned i = 0; i < pcs.size(); ++i) {
        check(full.journal[i].pc == pcs[i] &&
            full.journal[i].address == addresses[i] &&
            full.journal[i].operation == i + 1u &&
            full.journal[i].width == (i == 0 || i == 5 ? 4 : 1) &&
            full.journal[i].kind == (i == 0 ?
                NBA97_GAME_AUDIO_STREAM_STATUS_STORE :
                NBA97_GAME_AUDIO_STREAM_STATUS_READ));
    }
    check(full.journal[1].value == 7 && full.journal[3].value == 7 &&
        full.journal[4].value == 7 && full.journal[1].known_mask == 15 &&
        full.journal[5].known_mask == 5);

    Fixture bit1Only(3, 0);
    check(bit1Only.run() == NBA97_TEXT_COMPLETE &&
        bit1Only.progress.returned_value.word == 1 &&
        bit1Only.progress.access_events == 6);
    Fixture bit2Clear(5, 255);
    check(bit2Clear.run() == NBA97_TEXT_COMPLETE &&
        bit2Clear.progress.returned_value.word == 0xfffffff2u &&
        bit2Clear.progress.access_events == 3);

    std::array<std::uint8_t, 0x200> alias_data{};
    std::array<std::uint8_t, 0x200> alias_known{};
    alias_known.fill(1);
    Nba97GameTextRegion alias_region{Globals, alias_data.data(),
        alias_known.data(), alias_data.size()};
    Nba97GameAudioStreamStatusContext alias{};
    alias.memory = {&alias_region, 1};
    alias.operation_budget = 8;
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        alias.registers.gpr[i] = {0x41000000u + i, 15};
    alias.registers.gpr[0] = {0, 15};
    alias.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Flags + 8u, 15};
    alias.registers.gpr[NBA97_MATCH_INITIALIZE_FP] = {7u, 15};
    alias.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {CallerRa, 15};
    Nba97GameAudioStreamStatusProgress alias_out{};
    check(nba97_game_audio_stream_status(&alias, &alias_out) ==
        NBA97_TEXT_COMPLETE && alias_out.returned_value.word == 3 &&
        alias_out.first_flags.word == 7 && alias_out.busy.word == 0 &&
        alias_out.second_flags.word == 7 && alias_out.third_flags.word == 7 &&
        alias_out.restored_s8.word == 7 &&
        alias_out.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == Flags + 8u);
}

Nba97GameAudioStreamStatusRegisters expectedBudgetRegisters(
    const Fixture& f, unsigned budget) {
    auto expected = f.initial;
    expected.gpr[NBA97_MATCH_INITIALIZE_SP] = {Frame, 15};
    if (budget == 0)
        return expected;
    expected.gpr[NBA97_MATCH_INITIALIZE_FP] = {Frame, 15};
    expected.gpr[NBA97_MATCH_INITIALIZE_V0] = {0x800c0000u, 15};
    if (budget >= 2)
        expected.gpr[NBA97_MATCH_INITIALIZE_V1] = {2, 15};
    if (budget >= 4)
        expected.gpr[NBA97_MATCH_INITIALIZE_V1] = {1, 15};
    if (budget >= 5) {
        expected.gpr[NBA97_MATCH_INITIALIZE_V0] = {3, 15};
        expected.gpr[NBA97_MATCH_INITIALIZE_V1] = {4, 15};
    }
    return expected;
}

void budgetsUnknownnessMappingAndRepeatability() {
    const std::array<std::uint32_t, 6> stopped{{0x80084730u, 0x8008473cu,
        0x8008476cu, 0x80084794u, 0x800847c4u, 0x80084800u}};
    for (unsigned budget = 0; budget < 6; ++budget) {
        Fixture first(7, 0);
        Fixture second(7, 0);
        first.context.operation_budget = budget;
        second.context.operation_budget = budget;
        check(first.run() == NBA97_TEXT_LIMIT &&
            second.run() == NBA97_TEXT_LIMIT &&
            first.progress.operations == budget &&
            first.progress.stopped_pc == stopped[budget] &&
            std::memcmp(&first.progress.registers, &second.progress.registers,
                sizeof first.progress.registers) == 0);
        const auto expected = expectedBudgetRegisters(first, budget);
        for (unsigned r = 0; r < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++r)
            check(sameWord(first.progress.registers.gpr[r], expected.gpr[r]));
    }
    Fixture complete(7, 0);
    complete.context.operation_budget = 6;
    check(complete.run() == NBA97_TEXT_COMPLETE);

    Fixture unknown_flags(7, 0);
    unknown_flags.globals_known[Flags - Globals] = 0;
    check(unknown_flags.run() == NBA97_TEXT_UNKNOWN &&
        unknown_flags.progress.stopped_pc == 0x8008474cu &&
        unknown_flags.progress.operations == 2 &&
        unknown_flags.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask ==
            0x0e &&
        unknown_flags.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V1].known_mask ==
            0x0e);
    Fixture unknown_busy(7, 0);
    unknown_busy.globals_known[Busy - Globals] = 0;
    check(unknown_busy.run() == NBA97_TEXT_UNKNOWN &&
        unknown_busy.progress.stopped_pc == 0x80084774u &&
        unknown_busy.progress.operations == 3 &&
        unknown_busy.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask ==
            0x0e);

    Fixture partial_s8(7, 0);
    partial_s8.context.registers.gpr[NBA97_MATCH_INITIALIZE_FP] =
        {0xa1b2c3d4u, 5};
    check(partial_s8.run() == NBA97_TEXT_COMPLETE &&
        partial_s8.progress.restored_s8.word == 0xa1b2c3d4u &&
        partial_s8.progress.restored_s8.known_mask == 5);
    Fixture no_known(7, 0);
    no_known.regions[1].known = nullptr;
    no_known.context.registers.gpr[NBA97_MATCH_INITIALIZE_FP].known_mask = 5;
    check(no_known.run() == NBA97_TEXT_ARGUMENT &&
        no_known.progress.stopped_pc == 0x80084730u);

    Fixture unknown_sp(7, 0);
    unknown_sp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 14;
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x80084730u &&
        unknown_sp.progress.operations == 0);
    Fixture unaligned(7, 0);
    unaligned.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word += 2;
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80084730u);
    Fixture missing(7, 0);
    missing.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {0x90000008u, 15};
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_address == 0x90000000u);
    Fixture missing_globals(7, 0);
    missing_globals.context.memory = {&missing_globals.regions[1], 1};
    check(missing_globals.run() == NBA97_TEXT_RESOURCE &&
        missing_globals.progress.operations == 2 &&
        missing_globals.progress.stopped_pc == 0x8008473cu &&
        missing_globals.progress.stopped_address == Flags);

    std::array<std::uint8_t, 4> wrap_data{};
    std::array<std::uint8_t, 4> wrap_known{{1, 1, 1, 1}};
    Fixture wrapped(7, 0);
    Nba97GameTextRegion wrap_regions[2] = {
        wrapped.regions[0],
        {0xfffffffcu, wrap_data.data(), wrap_known.data(), wrap_data.size()}};
    wrapped.context.memory = {wrap_regions, 2};
    wrapped.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {4u, 15};
    check(wrapped.run() == NBA97_TEXT_COMPLETE &&
        wrapped.progress.frame_stack_pointer == 0xfffffffcu &&
        wrapped.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 4u);

    Fixture unknown_ra(7, 0);
    unknown_ra.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x80084808u &&
        unknown_ra.progress.operations == 6 &&
        !unknown_ra.progress.completed);

    Fixture repeated_a(7, 0);
    Fixture repeated_b(7, 0);
    check(repeated_a.run() == NBA97_TEXT_COMPLETE &&
        repeated_b.run() == NBA97_TEXT_COMPLETE &&
        std::memcmp(&repeated_a.progress.registers,
            &repeated_b.progress.registers,
            sizeof repeated_a.progress.registers) == 0 &&
        repeated_a.progress.access_events == repeated_b.progress.access_events &&
        std::memcmp(repeated_a.journal.data(), repeated_b.journal.data(),
            repeated_a.progress.access_events * sizeof repeated_a.journal[0]) == 0);

    Fixture overlap(7, 0);
    Nba97GameTextRegion duplicates[2] = {overlap.regions[0], overlap.regions[0]};
    overlap.context.memory = {duplicates, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT && !overlap.progress.operations);
    Fixture bad_mask;
    bad_mask.context.registers.gpr[8].known_mask = 16;
    check(bad_mask.run() == NBA97_TEXT_ARGUMENT);
    Nba97GameAudioStreamStatusContext empty{};
    Nba97GameAudioStreamStatusProgress progress{};
    check(nba97_game_audio_stream_status(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_audio_stream_status(&empty, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    allFlagsBusyAndRegisterResults();
    exactAccessPathsAndAliases();
    budgetsUnknownnessMappingAndRepeatability();
    std::printf("%u game audio stream status checks passed\n", checks);
    return 0;
}
