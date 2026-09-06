#include "recovered/game_first_period_startup.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game first-period startup check %u failed\n",
            checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x800ff800u;
constexpr std::uint32_t Flag = 0x800eb680u;
constexpr std::uint32_t OptionalClear = 0x800fdb4eu;
constexpr std::uint32_t Marker = 0x800fdb94u;

struct CallRecord {
    Nba97GameFirstPeriodStartupEvent event{};
    Nba97GameFirstPeriodStartupRegisters registers{};
};

struct Fixture {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x100000u, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x100000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameFirstPeriodStartupContext context{};
    Nba97GameFirstPeriodStartupProgress progress{};
    std::array<Nba97GameFirstPeriodStartupAccess, 8> journal{};
    std::vector<CallRecord> calls;
    unsigned refuse_call{};
    unsigned malformed_call{};
    unsigned set_flag_call{};
    std::uint8_t replacement_flag{};
    bool mutate_frame_pump{};
    bool mutate_all_last{};
    std::uint32_t incoming_ra = 0x10203040u;

    explicit Fixture(std::uint8_t flag = 0) {
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {0x11000000u + i, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {incoming_ra, 0x0f};
        context.memory = {&region, 1};
        context.operation_budget = 100;
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        put(Flag, flag, 1);
        put(OptionalClear, 0xbeefu, 2);
        put(Marker, 0x1234u, 2);
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Ram);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width,
        std::uint8_t mask = 0x0f) {
        const auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
            known[at + i] =
                static_cast<std::uint8_t>((mask >> i) & 1u);
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        const auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[at + i]) << (8u * i);
        return value;
    }
    std::uint8_t getKnown(std::uint32_t address, unsigned width) const {
        const auto at = offset(address);
        std::uint8_t mask = 0;
        for (unsigned i = 0; i < width; ++i)
            mask = static_cast<std::uint8_t>(mask |
                (known[at + i] ? (1u << i) : 0u));
        return mask;
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameFirstPeriodStartupEvent* event,
        Nba97GameFirstPeriodStartupRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back({*event, *registers});
        const unsigned call = static_cast<unsigned>(f.calls.size());
        if (f.refuse_call == call)
            return 0;
        if (f.set_flag_call == call)
            f.put(Flag, f.replacement_flag, 1);
        if (f.mutate_frame_pump && event->entry == 0x8002dd84u) {
            const auto live_sp =
                registers->gpr[NBA97_MATCH_INITIALIZE_SP];
            for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
                ++i)
                registers->gpr[i] = {0x61000000u + i,
                    static_cast<std::uint8_t>((i % 15u) + 1u)};
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = live_sp;
        }
        if (f.mutate_all_last && event->entry == 0x8007ef4cu) {
            const std::uint32_t relocated = EntrySp - 0x18u + 0x100u;
            for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
                ++i)
                registers->gpr[i] = {0x71000000u + i,
                    static_cast<std::uint8_t>((i % 15u) + 1u)};
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {relocated, 0x0f};
            f.put(relocated + 0x10u, 0x2468ace0u, 4, 0x0f);
        }
        if (f.malformed_call == call)
            registers->gpr[NBA97_MATCH_INITIALIZE_T9].known_mask = 0x10;
        return 1;
    }
    int run() {
        return nba97_game_first_period_startup(&context, &progress);
    }
};

constexpr std::array<std::uint32_t, 5> ZeroEntries = {
    0x800295d0u, 0x8002a244u, 0x8002a254u, 0x80065db0u,
    0x8007ef4cu
};
constexpr std::array<std::uint32_t, 7> NonzeroEntries = {
    0x800295d0u, 0x8002a244u, 0x8002dd84u, 0x8002ddccu,
    0x8002a254u, 0x80065db0u, 0x8007ef4cu
};
constexpr std::array<std::uint32_t, 12> NonzeroPrefixPcs = {
    0x800673f4u, 0x800673f8u, 0x80067400u, 0x8006740cu,
    0x8006741cu, 0x80067424u, 0x80067430u, 0x80067434u,
    0x80067444u, 0x80067448u, 0x80067450u, 0x80067458u
};
constexpr std::array<std::uint32_t, 9> ZeroPrefixPcs = {
    0x800673f4u, 0x800673f8u, 0x80067400u, 0x8006740cu,
    0x80067434u, 0x80067444u, 0x80067448u, 0x80067450u,
    0x80067458u
};

void exact_zero_path() {
    Fixture f;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations == 9 && f.progress.accesses == 4 &&
        f.progress.reads == 2 && f.progress.stores == 2 &&
        f.progress.callbacks_completed == ZeroEntries.size());
    check(f.calls.size() == ZeroEntries.size());
    for (std::size_t i = 0; i < ZeroEntries.size(); ++i) {
        check(f.calls[i].event.entry == ZeroEntries[i]);
        check(f.calls[i].event.delay_slot_pc == f.calls[i].event.pc + 4u);
    }
    check(f.calls[0].event.pc == 0x800673f8u &&
        f.calls[0].event.operation == 2 &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80067400u);
    check(f.calls[2].event.pc == 0x80067434u &&
        f.calls[2].event.argument_count == 1 &&
        f.calls[2].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 1u &&
        f.calls[2].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8006743cu);
    check(f.get(OptionalClear, 2) == 0xbeefu &&
        f.get(Marker, 2) == 0xffffu &&
        f.getKnown(Marker, 2) == 3u);
    check(f.get(EntrySp - 8u, 4) == f.incoming_ra &&
        f.progress.frame_stack_pointer == EntrySp - 0x18u &&
        f.progress.restored_return_address.word == f.incoming_ra &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            f.incoming_ra);
    check(f.progress.presentation_flag.word == 0 &&
        f.progress.presentation_flag.known_mask == 0x0f &&
        !f.progress.optional_presentation_executed);
    const std::array<std::uint32_t, 4> accessPcs = {
        0x800673f4u, 0x8006740cu, 0x80067444u, 0x80067458u
    };
    for (std::size_t i = 0; i < accessPcs.size(); ++i)
        check(f.journal[i].pc == accessPcs[i]);
    check(f.journal[0].operation == 1 &&
        f.journal[1].kind == NBA97_GAME_FIRST_PERIOD_STARTUP_READ &&
        f.journal[1].width == 1 && f.journal[2].operation == 6 &&
        f.journal[3].kind == NBA97_GAME_FIRST_PERIOD_STARTUP_READ);
}

void optional_path_and_live_flag() {
    for (const auto flag : {std::uint8_t{1}, std::uint8_t{255}}) {
        Fixture f(flag);
        check(f.run() == NBA97_TEXT_COMPLETE &&
            f.progress.optional_presentation_executed &&
            f.progress.operations == 12 && f.progress.accesses == 5 &&
            f.progress.stores == 3 && f.calls.size() == NonzeroEntries.size());
        for (std::size_t i = 0; i < NonzeroEntries.size(); ++i)
            check(f.calls[i].event.entry == NonzeroEntries[i]);
        check(f.progress.presentation_flag.word == flag &&
            f.progress.presentation_flag.known_mask == 0x0f &&
            f.get(OptionalClear, 2) == 0 && f.get(Marker, 2) == 0xffffu);
        check(f.journal[2].pc == 0x80067430u &&
            f.journal[2].operation == 7 &&
            f.journal[2].kind == NBA97_GAME_FIRST_PERIOD_STARTUP_STORE);
        check(f.calls[4].registers.gpr[NBA97_MATCH_INITIALIZE_AT].word ==
            0x80100000u);
    }

    Fixture mutated;
    mutated.set_flag_call = 2;
    mutated.replacement_flag = 255;
    check(mutated.run() == NBA97_TEXT_COMPLETE &&
        mutated.progress.optional_presentation_executed &&
        mutated.calls.size() == NonzeroEntries.size());

    Fixture live(1);
    live.mutate_frame_pump = true;
    check(live.run() == NBA97_TEXT_COMPLETE && live.calls.size() == 7);
    for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        if (i == NBA97_MATCH_INITIALIZE_RA ||
            i == NBA97_MATCH_INITIALIZE_SP)
            continue;
        check(live.calls[3].registers.gpr[i].word == 0x61000000u + i &&
            live.calls[3].registers.gpr[i].known_mask ==
                static_cast<std::uint8_t>((i % 15u) + 1u));
    }
    check(live.calls[3].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        0x8006742cu &&
        live.calls[3].registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp - 0x18u);
}

void full_gpr_and_stack_mutation() {
    Fixture f(1);
    f.mutate_all_last = true;
    check(f.run() == NBA97_TEXT_COMPLETE);
    for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        if (i == NBA97_MATCH_INITIALIZE_RA ||
            i == NBA97_MATCH_INITIALIZE_SP)
            continue;
        check(f.progress.registers.gpr[i].word == 0x71000000u + i &&
            f.progress.registers.gpr[i].known_mask ==
                static_cast<std::uint8_t>((i % 15u) + 1u));
    }
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word == 0 &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask ==
            0x0f);
    check(f.progress.restored_return_address.word == 0x2468ace0u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x2468ace0u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp + 0x100u);
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
        0x71000002u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 3u);
}

void unknown_data_and_return() {
    Fixture branch;
    branch.put(Flag, 0xa5u, 1, 0);
    check(branch.run() == NBA97_TEXT_UNKNOWN &&
        branch.progress.operations == 4 &&
        branch.progress.stopped_pc == 0x80067414u &&
        branch.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0xa5u &&
        branch.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask ==
            0x0e && branch.get(OptionalClear, 2) == 0xbeefu &&
        branch.get(Marker, 2) == 0x1234u);

    Fixture unknownRa;
    unknownRa.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 5;
    check(unknownRa.run() == NBA97_TEXT_UNKNOWN &&
        unknownRa.progress.operations == 9 &&
        unknownRa.progress.stopped_pc == 0x80067460u &&
        unknownRa.progress.restored_return_address.known_mask == 5 &&
        unknownRa.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp);

    Fixture unknownSp;
    unknownSp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0};
    check(unknownSp.run() == NBA97_TEXT_UNKNOWN &&
        unknownSp.progress.operations == 0 &&
        unknownSp.progress.stopped_pc == 0x800673f4u &&
        unknownSp.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
            .known_mask == 0);

    Fixture partialSp;
    partialSp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {EntrySp, 0x0e};
    check(partialSp.run() == NBA97_TEXT_UNKNOWN &&
        partialSp.progress.operations == 0 &&
        partialSp.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
            .known_mask == 0x0c);
}

void failures_wrap_and_alias() {
    Fixture alignment;
    alignment.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
        EntrySp + 1u;
    check(alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        alignment.progress.operations == 1 &&
        alignment.progress.stopped_pc == 0x800673f4u);

    Fixture missing;
    missing.context.memory = {nullptr, 0};
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.operations == 1 &&
        missing.progress.stopped_address == EntrySp - 8u);

    Fixture malformedMemory;
    malformedMemory.known[malformedMemory.offset(EntrySp - 8u)] = 2;
    check(malformedMemory.run() == NBA97_TEXT_ARGUMENT &&
        malformedMemory.progress.operations == 1);

    Fixture untrackedUnknown;
    untrackedUnknown.region.known = nullptr;
    untrackedUnknown.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = 5;
    check(untrackedUnknown.run() == NBA97_TEXT_ARGUMENT &&
        untrackedUnknown.progress.operations == 1);

    Fixture malformedCallback;
    malformedCallback.malformed_call = 1;
    check(malformedCallback.run() == NBA97_TEXT_ARGUMENT &&
        malformedCallback.progress.operations == 2 &&
        malformedCallback.progress.callbacks_completed == 0 &&
        malformedCallback.progress.stopped_entry == 0x800295d0u);

    Fixture noCallback;
    noCallback.context.io = nullptr;
    check(noCallback.run() == NBA97_TEXT_IO_REFUSED &&
        noCallback.progress.operations == 2 &&
        noCallback.progress.stopped_pc == 0x800673f8u);

    Fixture alias;
    alias.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {Marker + 8u, 0x0f};
    check(alias.run() == NBA97_TEXT_COMPLETE &&
        alias.progress.frame_stack_pointer == Marker - 0x10u &&
        alias.progress.restored_return_address.word ==
            ((alias.incoming_ra & 0xffff0000u) | 0xffffu) &&
        alias.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Marker + 8u);

    Fixture wrap;
    std::array<std::uint8_t, 32> lowBytes{};
    std::array<std::uint8_t, 32> lowKnown{};
    lowKnown.fill(1);
    Nba97GameTextRegion regions[2] = {
        {0, lowBytes.data(), lowKnown.data(), lowBytes.size()}, wrap.region
    };
    wrap.context.memory = {regions, 2};
    wrap.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {8u, 0x0f};
    check(wrap.run() == NBA97_TEXT_COMPLETE &&
        wrap.progress.frame_stack_pointer == 0xfffffff0u &&
        wrap.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 8u &&
        lowBytes[0] == static_cast<std::uint8_t>(wrap.incoming_ra));
}

void every_child_refusal_and_budget_prefix() {
    for (unsigned fail = 1; fail <= NonzeroEntries.size(); ++fail) {
        Fixture f(1);
        f.refuse_call = fail;
        check(f.run() == NBA97_TEXT_IO_REFUSED && !f.progress.completed &&
            f.progress.callbacks_completed == fail - 1u &&
            f.calls.size() == fail &&
            f.progress.stopped_entry == NonzeroEntries[fail - 1u]);
    }
    for (std::size_t budget = 0; budget < NonzeroPrefixPcs.size(); ++budget) {
        Fixture f(1);
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations == budget &&
            f.progress.stopped_pc == NonzeroPrefixPcs[budget]);
    }
    Fixture exactNonzero(1);
    exactNonzero.context.operation_budget = NonzeroPrefixPcs.size();
    check(exactNonzero.run() == NBA97_TEXT_COMPLETE &&
        exactNonzero.progress.operations == NonzeroPrefixPcs.size());

    for (std::size_t budget = 0; budget < ZeroPrefixPcs.size(); ++budget) {
        Fixture f;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations == budget &&
            f.progress.stopped_pc == ZeroPrefixPcs[budget]);
    }
    Fixture exactZero;
    exactZero.context.operation_budget = ZeroPrefixPcs.size();
    check(exactZero.run() == NBA97_TEXT_COMPLETE &&
        exactZero.progress.operations == ZeroPrefixPcs.size());
}

void argument_validation() {
    Nba97GameFirstPeriodStartupProgress progress{};
    check(nba97_game_first_period_startup(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_first_period_startup(&f.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
    f.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
    check(f.run() == NBA97_TEXT_ARGUMENT);
    Fixture malformedKnown;
    malformedKnown.context.registers.gpr[NBA97_MATCH_INITIALIZE_T0]
        .known_mask = 0x10;
    check(malformedKnown.run() == NBA97_TEXT_ARGUMENT);
    Fixture overlap;
    Nba97GameTextRegion regions[2] = {overlap.region, overlap.region};
    overlap.context.memory = {regions, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    exact_zero_path();
    optional_path_and_live_flag();
    full_gpr_and_stack_mutation();
    unknown_data_and_return();
    failures_wrap_and_alias();
    every_child_refusal_and_budget_prefix();
    argument_validation();
    std::printf("game first-period startup: %u checks passed\n", checks);
}
