#include "recovered/game_period_startup.h"

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
        std::fprintf(stderr, "game period-startup check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x800ff800u;
constexpr std::uint32_t Selector = 0x800fdb68u;
constexpr std::uint32_t Pointer = 0x80020c14u;
constexpr std::uint32_t Counter = 0x800fdb92u;
constexpr std::uint32_t ActiveBall = 0x800fdc48u;
constexpr std::uint32_t Delta = 0x800fdb6cu;
constexpr std::uint32_t Optional = 0x8001edecu;

struct CallRecord {
    Nba97GamePeriodStartupEvent event{};
    Nba97GamePeriodStartupRegisters registers{};
};

struct Fixture {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x100000u, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x100000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GamePeriodStartupContext context{};
    Nba97GamePeriodStartupProgress progress{};
    std::array<Nba97GamePeriodStartupAccess, 16> journal{};
    std::vector<CallRecord> calls;
    unsigned refuse_call = 0;
    unsigned malformed_call = 0;
    bool mutate_live = false;
    bool mutate_all_first = false;
    bool unknown_sp_last = false;
    std::uint32_t incoming_ra = 0x10203040u;
    std::uint32_t incoming_s0 = 0x50607080u;

    Fixture(std::uint16_t selector = 0, std::uint16_t optional = 0) {
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {0x11000000u + i, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {incoming_ra, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {incoming_s0, 0x0f};
        context.memory = {&region, 1};
        context.operation_budget = 100;
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        put(Selector, selector, 2);
        put(Pointer, 0x81234560u, 4);
        put(Optional, optional, 2);
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Ram);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width,
        std::uint8_t mask = 0x0f) {
        const auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            known[at + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        const auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[at + i]) << (i * 8u);
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
        const Nba97GamePeriodStartupEvent* event,
        Nba97GamePeriodStartupRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back({*event, *registers});
        const unsigned call = static_cast<unsigned>(f.calls.size());
        if (f.refuse_call == call)
            return 0;
        if (f.mutate_all_first && call == 1) {
            for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
                registers->gpr[i] = {0x71000000u + i,
                    static_cast<std::uint8_t>((i % 15u) + 1u)};
        }
        if (f.mutate_live && event->entry == 0x80029590u) {
            registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0x12345678u, 0x0f};
            f.put(Pointer, 0xdeadbeefu, 4);
        }
        if (f.mutate_live && event->entry == 0x8002dd84u) {
            const std::uint32_t relocated = EntrySp - 0x18u + 0x100u;
            registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0xabcd4321u, 0x0f};
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {relocated, 0x0f};
            f.put(relocated + 0x14u, 0x2468ace0u, 4);
            f.put(relocated + 0x10u, 0x13579bdfu, 4);
        }
        if (f.unknown_sp_last && event->pc == 0x80067518u)
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {0x55555555u, 0};
        if (f.malformed_call == call)
            registers->gpr[NBA97_MATCH_INITIALIZE_T9].known_mask = 0x10;
        return 1;
    }
    int run() { return nba97_game_period_startup(&context, &progress); }
};

constexpr std::array<std::uint32_t, 13> ZeroEntries = {
    0x80065db0u, 0x80063edcu, 0x800673f0u, 0x8002a25cu,
    0x80035318u, 0x80029590u, 0x8002dd84u, 0x80076b28u,
    0x80076b3cu, 0x80076b28u, 0x80076b3cu, 0x800a584cu,
    0x800a584cu
};
constexpr std::array<std::uint32_t, 14> NonzeroEntries = {
    0x80065db0u, 0x80063edcu, 0x80067194u, 0x8002a25cu,
    0x80035318u, 0x80029590u, 0x8002dd84u, 0x80076b28u,
    0x80076b3cu, 0x80076b28u, 0x80076b3cu, 0x800a584cu,
    0x800a584cu, 0x80035678u
};
constexpr std::array<std::uint32_t, 24> PrefixPcs = {
    0x8006746cu, 0x80067474u, 0x80067470u, 0x80067478u,
    0x80067484u, 0x800674a4u, 0x800674acu, 0x800674b8u,
    0x800674c0u, 0x800674ccu, 0x800674d4u, 0x800674dcu,
    0x800674e0u, 0x800674ecu, 0x800674f0u, 0x800674f8u,
    0x80067500u, 0x80067508u, 0x80067510u, 0x80067518u,
    0x80067524u, 0x80067534u, 0x8006753cu, 0x80067540u
};

void exact_zero_path() {
    Fixture f;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations == 23 && f.progress.accesses == 10 &&
        f.progress.reads == 5 && f.progress.stores == 5 &&
        f.progress.callbacks_completed == ZeroEntries.size());
    check(f.calls.size() == ZeroEntries.size());
    for (std::size_t i = 0; i < ZeroEntries.size(); ++i) {
        check(f.calls[i].event.entry == ZeroEntries[i]);
        check(f.calls[i].event.delay_slot_pc == f.calls[i].event.pc + 4u);
    }
    check(f.calls[0].event.pc == 0x80067470u &&
        f.calls[0].event.operation == 3 &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80067478u);
    check(f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
        f.incoming_s0 && f.get(EntrySp - 8u, 4) == f.incoming_s0);
    check(f.calls[3].registers.gpr[NBA97_MATCH_INITIALIZE_S0].word == 1u);
    check(f.calls[4].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 1u &&
        f.calls[4].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            UINT32_MAX && f.calls[4].event.argument_count == 2);
    check(f.calls[5].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 15u);
    check(f.get(Counter, 2) == 1u && f.get(ActiveBall, 4) == 0x81234560u &&
        f.get(Delta, 2) == 1u);
    check(f.get(EntrySp - 4u, 4) == f.incoming_ra &&
        f.progress.restored_return_address.word == f.incoming_ra &&
        f.progress.restored_s0.word == f.incoming_s0);
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            f.incoming_ra &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_AT].word ==
            0x80100000u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 0u);
    check(!f.progress.used_nonzero_period_path &&
        !f.progress.optional_service_called && f.progress.stopped_pc == 0);

    const std::array<std::uint32_t, 10> accessPcs = {
        0x8006746cu, 0x80067474u, 0x80067484u, 0x800674ccu,
        0x800674d4u, 0x800674dcu, 0x800674ecu, 0x80067524u,
        0x8006753cu, 0x80067540u
    };
    for (std::size_t i = 0; i < accessPcs.size(); ++i)
        check(f.journal[i].pc == accessPcs[i]);
    check(f.journal[0].operation == 1 && f.journal[1].operation == 2 &&
        f.journal[4].width == 2 &&
        f.journal[4].kind == NBA97_GAME_PERIOD_STARTUP_STORE &&
        f.journal[8].kind == NBA97_GAME_PERIOD_STARTUP_READ);
}

void signed_nonzero_and_optional_path() {
    Fixture f(0x8000u, 0xff00u);
    check(f.run() == NBA97_TEXT_COMPLETE && f.calls.size() == 14);
    for (std::size_t i = 0; i < NonzeroEntries.size(); ++i)
        check(f.calls[i].event.entry == NonzeroEntries[i]);
    check(f.progress.used_nonzero_period_path &&
        f.progress.optional_service_called && f.progress.operations == 24);
    check(f.progress.period_selector.word == 0xffff8000u &&
        f.progress.period_selector.known_mask == 0x0f &&
        f.progress.optional_flag.word == 0xff00u &&
        f.progress.optional_flag.known_mask == 0x0f);
    check(f.calls[2].event.pc == 0x800674a4u &&
        f.calls[2].event.argument_count == 1 &&
        f.calls[2].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 1u);
}

void live_register_memory_and_stack_mutation() {
    Fixture f;
    f.mutate_live = true;
    f.mutate_all_first = true;
    check(f.run() == NBA97_TEXT_COMPLETE);
    for (unsigned i = 1; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        if (i == NBA97_MATCH_INITIALIZE_RA)
            continue;
        check(f.calls[1].registers.gpr[i].word == 0x71000000u + i &&
            f.calls[1].registers.gpr[i].known_mask ==
                static_cast<std::uint8_t>((i % 15u) + 1u));
    }
    check(f.calls[1].registers.gpr[NBA97_MATCH_INITIALIZE_AT].word ==
        0x71000001u &&
        f.calls[1].registers.gpr[NBA97_MATCH_INITIALIZE_T8].word ==
            0x71000018u &&
        f.calls[1].registers.gpr[NBA97_MATCH_INITIALIZE_K1].word ==
            0x7100001bu);
    check(f.calls[1].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        0x80067480u &&
        f.calls[1].registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word == 0u);
    check(f.get(Counter, 2) == 0x5678u &&
        f.getKnown(Counter, 2) == 3u &&
        f.get(ActiveBall, 4) == 0xdeadbeefu &&
        f.get(Delta, 2) == 0x4321u);
    check(f.progress.published_pointer.word == 0xdeadbeefu &&
        f.progress.restored_return_address.word == 0x2468ace0u &&
        f.progress.restored_s0.word == 0x13579bdfu);
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        EntrySp + 0x100u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x2468ace0u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
            0x13579bdfu);
}

void unknown_data_propagation() {
    Fixture branch;
    branch.put(Selector, 0xa5a5u, 2, 0);
    check(branch.run() == NBA97_TEXT_UNKNOWN &&
        branch.progress.operations == 5 &&
        branch.progress.stopped_pc == 0x8006748cu &&
        branch.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 0);

    Fixture knownNonzero;
    knownNonzero.put(Selector, 1u, 2, 1);
    check(knownNonzero.run() == NBA97_TEXT_COMPLETE &&
        knownNonzero.progress.used_nonzero_period_path &&
        knownNonzero.progress.period_selector.known_mask == 1);

    Fixture optional;
    optional.put(Optional, 0u, 2, 0);
    check(optional.run() == NBA97_TEXT_UNKNOWN &&
        optional.progress.operations == 21 &&
        optional.progress.stopped_pc == 0x8006752cu &&
        optional.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask ==
            0x0c);

    Fixture knownOptional;
    knownOptional.put(Optional, 2u, 2, 1);
    check(knownOptional.run() == NBA97_TEXT_COMPLETE &&
        knownOptional.progress.optional_service_called &&
        knownOptional.progress.optional_flag.known_mask == 0x0d);

    Fixture partialPointer;
    partialPointer.put(Pointer, 0xa1b2c3d4u, 4, 5);
    check(partialPointer.run() == NBA97_TEXT_COMPLETE &&
        partialPointer.progress.published_pointer.word == 0xa1b2c3d4u &&
        partialPointer.progress.published_pointer.known_mask == 5 &&
        partialPointer.get(ActiveBall, 4) == 0xa1b2c3d4u &&
        partialPointer.getKnown(ActiveBall, 4) == 5);

    Fixture unknownRa;
    unknownRa.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 5;
    check(unknownRa.run() == NBA97_TEXT_UNKNOWN &&
        unknownRa.progress.operations == 23 &&
        unknownRa.progress.stopped_pc == 0x80067548u &&
        unknownRa.progress.restored_return_address.known_mask == 5 &&
        unknownRa.progress.restored_s0.word == unknownRa.incoming_s0 &&
        unknownRa.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp);

    Fixture unknownInitialSp;
    unknownInitialSp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {EntrySp, 0};
    check(unknownInitialSp.run() == NBA97_TEXT_UNKNOWN &&
        unknownInitialSp.progress.operations == 0 &&
        unknownInitialSp.progress.stopped_pc == 0x8006746cu &&
        unknownInitialSp.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
            .known_mask == 0);

    Fixture partialSp;
    partialSp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {EntrySp, 0x0e};
    check(partialSp.run() == NBA97_TEXT_UNKNOWN &&
        partialSp.progress.operations == 0 &&
        partialSp.progress.stopped_pc == 0x8006746cu &&
        partialSp.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
            .known_mask == 0x0c);

    Fixture childUnknownSp;
    childUnknownSp.unknown_sp_last = true;
    check(childUnknownSp.run() == NBA97_TEXT_UNKNOWN &&
        childUnknownSp.progress.operations == 21 &&
        childUnknownSp.progress.stopped_pc == 0x8006753cu);
}

void failure_alignment_wrap_and_alias() {
    Fixture alignment;
    alignment.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
        EntrySp + 1u;
    check(alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        alignment.progress.operations == 1 && alignment.progress.accesses == 1 &&
        alignment.progress.stopped_pc == 0x8006746cu);

    Fixture missing;
    missing.context.memory = {nullptr, 0};
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.operations == 1 &&
        missing.progress.stopped_address == EntrySp - 4u);

    Fixture malformedMemory;
    malformedMemory.known[malformedMemory.offset(EntrySp - 4u)] = 2;
    check(malformedMemory.run() == NBA97_TEXT_ARGUMENT &&
        malformedMemory.progress.operations == 1);

    Fixture untrackedUnknown;
    untrackedUnknown.region.known = nullptr;
    untrackedUnknown.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = 5;
    check(untrackedUnknown.run() == NBA97_TEXT_ARGUMENT &&
        untrackedUnknown.progress.operations == 1 &&
        untrackedUnknown.progress.stopped_pc == 0x8006746cu);

    Fixture malformedCallback;
    malformedCallback.malformed_call = 1;
    check(malformedCallback.run() == NBA97_TEXT_ARGUMENT &&
        malformedCallback.progress.operations == 3 &&
        malformedCallback.progress.callbacks_completed == 0 &&
        malformedCallback.progress.stopped_entry == 0x80065db0u);

    Fixture noCallback;
    noCallback.context.io = nullptr;
    check(noCallback.run() == NBA97_TEXT_IO_REFUSED &&
        noCallback.progress.operations == 3 &&
        noCallback.progress.stopped_pc == 0x80067470u);

    Fixture alias;
    alias.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {ActiveBall + 4u, 0x0f};
    check(alias.run() == NBA97_TEXT_COMPLETE &&
        alias.progress.frame_stack_pointer == ActiveBall - 0x14u &&
        alias.progress.restored_return_address.word == 0x81234560u &&
        alias.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            ActiveBall + 4u);

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
        wrap.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 8u);
    check(lowBytes[4] == static_cast<std::uint8_t>(wrap.incoming_ra) &&
        lowBytes[0] == static_cast<std::uint8_t>(wrap.incoming_s0));
}

void every_child_refusal_and_budget_prefix() {
    for (unsigned fail = 1; fail <= NonzeroEntries.size(); ++fail) {
        Fixture f(1, 2);
        f.refuse_call = fail;
        check(f.run() == NBA97_TEXT_IO_REFUSED && !f.progress.completed &&
            f.progress.callbacks_completed == fail - 1u &&
            f.calls.size() == fail &&
            f.progress.stopped_entry == NonzeroEntries[fail - 1u]);
    }
    for (std::size_t budget = 0; budget < PrefixPcs.size(); ++budget) {
        Fixture f(1, 2);
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations == budget &&
            f.progress.stopped_pc == PrefixPcs[budget]);
    }
    Fixture exact(1, 2);
    exact.context.operation_budget = PrefixPcs.size();
    check(exact.run() == NBA97_TEXT_COMPLETE && exact.progress.completed &&
        exact.progress.operations == PrefixPcs.size());
}

void argument_validation() {
    Nba97GamePeriodStartupProgress progress{};
    check(nba97_game_period_startup(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_period_startup(&f.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
    f.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
    check(f.run() == NBA97_TEXT_ARGUMENT);
    Fixture overlap;
    Nba97GameTextRegion regions[2] = {overlap.region, overlap.region};
    overlap.context.memory = {regions, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    exact_zero_path();
    signed_nonzero_and_optional_path();
    live_register_memory_and_stack_mutation();
    unknown_data_propagation();
    failure_alignment_wrap_and_alias();
    every_child_refusal_and_budget_prefix();
    argument_validation();
    std::printf("game period-startup: %u checks passed\n", checks);
}
