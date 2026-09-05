#include "recovered/game_audio_initialize.h"

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
        std::fprintf(stderr, "game audio-initialize check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80010000u;
constexpr std::uint32_t Stack = 0x807ffe00u;
constexpr std::uint32_t EntrySp = 0x807fff80u;
constexpr std::uint32_t FrameSp = EntrySp - 0x18u;
constexpr std::uint32_t Header = 0x8001502cu;
constexpr std::uint32_t HeaderName = 0x800247bcu;
constexpr std::uint32_t BodyName = 0x800247c8u;
constexpr std::uint32_t UploadState = 0x80021d6cu;
constexpr std::uint32_t Setting = 0x80021d7cu;
constexpr std::uint32_t Result = 0x80021ee0u;

struct LoggedCall {
    Nba97GameAudioInitializeEvent event{};
    Nba97GameAudioInitializeRegisters registers{};
};

struct Fixture {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x18000, 0xcd);
    std::vector<std::uint8_t> ramKnown =
        std::vector<std::uint8_t>(0x18000, 1);
    std::array<std::uint8_t, 0x200> stack{};
    std::array<std::uint8_t, 0x200> stackKnown{};
    Nba97GameTextRegion regions[2] = {
        {Ram, ram.data(), ramKnown.data(), ram.size()},
        {Stack, stack.data(), stackKnown.data(), stack.size()}
    };
    std::array<Nba97GameAudioInitializeAccess, 16> journal{};
    Nba97GameAudioInitializeContext context{};
    Nba97GameAudioInitializeProgress progress{};
    std::vector<LoggedCall> calls;
    std::size_t refuseCall = static_cast<std::size_t>(-1);
    std::size_t malformedCall = static_cast<std::size_t>(-1);
    std::uint32_t headerResult = 0x80110000u;
    std::uint32_t bodyResult = 0x80120000u;
    std::uint32_t rawResult = 0xdeadbeefu;
    std::uint8_t rawKnown = 0x0f;
    std::uint8_t headerKnown = 0x0f;
    bool mutateHeader = false;
    bool mutateS0 = false;
    bool mutateSaved = false;
    bool mutateLiveSp = false;

    Fixture() {
        stack.fill(0xcd);
        stackKnown.fill(1);
        context.memory = {regions, 2};
        context.operation_budget = 64;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {0x40000000u + i, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x8002dbd8u, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {
            0x51525354u, 0x0f};
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        put(Header, 0x80100000u);
        put(Setting, 7, 1);
    }

    std::uint8_t* byte(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base &&
                std::uint64_t(address - region.base) < region.size)
                return region.data + (address - region.base);
        return nullptr;
    }
    std::uint8_t* known(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base &&
                std::uint64_t(address - region.base) < region.size)
                return region.known ? region.known + (address - region.base) :
                    nullptr;
        return nullptr;
    }
    void put(std::uint32_t address, std::uint32_t value,
        unsigned width = 4, std::uint8_t mask = 0x0f) {
        for (unsigned i = 0; i < width; ++i) {
            check(byte(address + i) != nullptr);
            *byte(address + i) =
                static_cast<std::uint8_t>(value >> (i * 8u));
            if (known(address + i))
                *known(address + i) = (mask >> i) & 1u;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width = 4) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i) {
            check(byte(address + i) != nullptr);
            value |= std::uint32_t(*byte(address + i)) << (i * 8u);
        }
        return value;
    }
    std::uint8_t knownMask(std::uint32_t address, unsigned width = 4) {
        std::uint8_t mask = 0;
        for (unsigned i = 0; i < width; ++i)
            if (!known(address + i) || *known(address + i))
                mask = static_cast<std::uint8_t>(mask | (1u << i));
        return mask;
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameAudioInitializeEvent* event,
        Nba97GameAudioInitializeRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        const std::size_t index = f.calls.size();
        f.calls.push_back({*event, *registers});
        if (index == f.refuseCall)
            return 0;
        if (index == f.malformedCall)
            registers->gpr[NBA97_MATCH_INITIALIZE_T9].known_mask = 0x10;
        if (event->pc == 0x8002913cu)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                f.headerResult, f.headerKnown};
        else if (event->pc == 0x80029154u)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                f.bodyResult, 0x0f};
        else if (event->pc == 0x80029164u && f.mutateS0)
            registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {
                0x81234560u, 0x0f};
        else if (event->pc == 0x80029188u && f.mutateHeader)
            f.put(Header, 0x801abc00u);
        else if (event->pc == 0x800291b0u && f.mutateSaved) {
            f.put(FrameSp + 0x14u, 0x81223344u);
            f.put(FrameSp + 0x10u, 0xa1b2c3d4u, 4, 0x05);
        } else if (event->pc == 0x800291dcu) {
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                f.rawResult, f.rawKnown};
            if (f.mutateLiveSp) {
                registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
                    Stack + 0x40u, 0x0f};
            }
        }
        return 1;
    }
    int run() { return nba97_game_audio_initialize(&context, &progress); }
};

void exactCallsRegistersAndAccesses() {
    Fixture f;
    f.mutateHeader = true;
    f.mutateS0 = true;
    f.mutateSaved = true;
    f.rawKnown = 0x05;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations == 20 && f.progress.accesses == 9 &&
        f.progress.reads == 5 && f.progress.stores == 4 &&
        f.progress.callbacks_completed == 11 &&
        f.progress.access_events == 9 && f.calls.size() == 11);

    static constexpr std::uint32_t pc[11] = {
        0x8002912cu, 0x8002913cu, 0x80029154u, 0x80029164u,
        0x8002916cu, 0x80029180u, 0x80029188u, 0x800291a0u,
        0x800291a8u, 0x800291b0u, 0x800291dcu
    };
    static constexpr std::uint32_t entry[11] = {
        0x80090698u, 0x80029bfcu, 0x80029bfcu, 0x8008f4f0u,
        0x800adb48u, 0x8008cdc0u, 0x8008cc28u, 0x800ad360u,
        0x80090698u, 0x800aca08u, 0x80088e84u
    };
    static constexpr unsigned arguments[11] = {1, 2, 2, 2, 0, 4, 0, 3, 1, 1, 2};
    for (unsigned i = 0; i < 11; ++i)
        check(f.calls[i].event.pc == pc[i] &&
            f.calls[i].event.delay_slot_pc == pc[i] + 4u &&
            f.calls[i].event.entry == entry[i] &&
            f.calls[i].event.argument_count == arguments[i] &&
            f.calls[i].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
                pc[i] + 8u);
    check(f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        0x80100000u);
    check(f.calls[1].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        HeaderName &&
        f.calls[1].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == 0);
    check(f.calls[2].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        BodyName && f.journal[3].value == f.headerResult);
    check(f.calls[3].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        UINT32_MAX &&
        f.calls[3].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == 0 &&
        f.calls[3].registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
            f.bodyResult);
    check(f.calls[5].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 4 &&
        f.calls[5].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == 11000 &&
        f.calls[5].registers.gpr[NBA97_MATCH_INITIALIZE_A2].word == 0 &&
        f.calls[5].registers.gpr[NBA97_MATCH_INITIALIZE_A3].word == 0);
    check(f.calls[7].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        UploadState &&
        f.calls[7].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            0x801abc00u &&
        f.calls[7].registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
            0x81234560u);
    check(f.calls[8].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        0x81234560u &&
        f.calls[9].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 127 &&
        f.calls[10].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 105 &&
        f.calls[10].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            UINT32_MAX);

    static constexpr std::uint32_t accessPc[9] = {
        0x80029118u, 0x80029120u, 0x80029128u, 0x80029150u,
        0x80029194u, 0x800291bcu, 0x800291e8u, 0x800291ecu,
        0x800291f0u
    };
    static constexpr std::uint32_t accessAddress[9] = {
        Header, FrameSp + 0x14u, FrameSp + 0x10u, Header, Header,
        Setting, Result, FrameSp + 0x14u, FrameSp + 0x10u
    };
    static constexpr std::size_t accessOperation[9] = {
        1, 2, 3, 6, 12, 16, 18, 19, 20
    };
    for (unsigned i = 0; i < 9; ++i)
        check(f.journal[i].pc == accessPc[i] &&
            f.journal[i].address == accessAddress[i] &&
            f.journal[i].operation == accessOperation[i] &&
            f.journal[i].width == (i == 5 ? 1 : 4) &&
            f.journal[i].kind == ((i == 0 || i == 4 || i >= 5 && i != 6)
                ? NBA97_GAME_AUDIO_INITIALIZE_READ
                : NBA97_GAME_AUDIO_INITIALIZE_STORE));
    check(f.progress.old_bank_header.word == 0x80100000u &&
        f.progress.new_bank_header.word == f.headerResult &&
        f.progress.bank_body.word == f.bodyResult &&
        f.progress.volume_setting.word == 7 &&
        f.progress.scaled_volume.word == 105 &&
        f.progress.raw_volume_return.word == f.rawResult &&
        f.progress.raw_volume_return.known_mask == 0x05 &&
        f.get(Result) == f.rawResult && f.knownMask(Result) == 0x05);
    check(f.progress.restored_return_address.word == 0x81223344u &&
        f.progress.restored_s0.word == 0xa1b2c3d4u &&
        f.progress.restored_s0.known_mask == 0x05 &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            f.rawResult &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 0x05);
}

void optionalBranchAndAllVolumes() {
    Fixture noOld;
    noOld.put(Header, 0);
    check(noOld.run() == NBA97_TEXT_COMPLETE && noOld.calls.size() == 10 &&
        noOld.calls.front().event.pc == 0x8002913cu &&
        noOld.progress.operations == 19);
    check(noOld.get(FrameSp + 0x10u) == 0x51525354u);

    for (unsigned setting = 0; setting < 256; ++setting) {
        Fixture f;
        f.put(Header, 0);
        f.put(Setting, setting, 1);
        f.rawResult = 0x70000000u + setting;
        check(f.run() == NBA97_TEXT_COMPLETE);
        const std::uint32_t expected = setting * 15u < 128u
            ? setting * 15u : 127u;
        check(f.progress.scaled_volume.word == expected &&
            f.calls.back().event.pc == 0x800291dcu &&
            f.calls.back().registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
                expected && f.get(Result) == f.rawResult);
    }
}

void CallbackFailuresAndEveryBudgetPrefix() {
    for (std::size_t fail = 0; fail < 11; ++fail) {
        Fixture f;
        f.refuseCall = fail;
        check(f.run() == NBA97_TEXT_IO_REFUSED && !f.progress.completed &&
            f.calls.size() == fail + 1u &&
            f.progress.callbacks_completed == fail &&
            f.progress.stopped_pc == f.calls.back().event.pc &&
            f.progress.stopped_entry == f.calls.back().event.entry);
    }
    Fixture malformed;
    malformed.malformedCall = 5;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.calls.size() == 6 &&
        malformed.progress.callbacks_completed == 5);

    for (std::size_t budget = 0; budget < 20; ++budget) {
        Fixture f;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations == budget);
        for (std::size_t i = 0; i < f.progress.access_events; ++i)
            check(f.journal[i].operation <= budget);
    }
    for (std::size_t budget = 0; budget < 19; ++budget) {
        Fixture f;
        f.put(Header, 0);
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations == budget);
    }
    Fixture exact;
    exact.context.operation_budget = 20;
    check(exact.run() == NBA97_TEXT_COMPLETE &&
        exact.progress.operations == 20);

    Fixture beforeOldRelease;
    beforeOldRelease.context.operation_budget = 3;
    check(beforeOldRelease.run() == NBA97_TEXT_LIMIT &&
        beforeOldRelease.calls.empty() &&
        beforeOldRelease.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x80029134u &&
        beforeOldRelease.get(FrameSp + 0x10u) == 0x51525354u);
    Fixture beforeS0Capture;
    beforeS0Capture.context.operation_budget = 7;
    check(beforeS0Capture.run() == NBA97_TEXT_LIMIT &&
        beforeS0Capture.calls.size() == 3 &&
        beforeS0Capture.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x8002916cu &&
        beforeS0Capture.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_S0].word == beforeS0Capture.bodyResult);
    Fixture beforeHeaderStore;
    beforeHeaderStore.context.operation_budget = 5;
    check(beforeHeaderStore.run() == NBA97_TEXT_LIMIT &&
        beforeHeaderStore.progress.stopped_pc == 0x80029150u &&
        beforeHeaderStore.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_A0].word == BodyName &&
        beforeHeaderStore.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_AT].word == 0x80010000u);
}

void KnownnessMappingAlignmentAndWrap() {
    Fixture partialHeader;
    partialHeader.headerKnown = 0x05;
    check(partialHeader.run() == NBA97_TEXT_COMPLETE &&
        partialHeader.journal[3].known_mask == 0x05 &&
        partialHeader.knownMask(Header) == 0x05);

    Fixture implicitKnown;
    implicitKnown.headerKnown = 0x05;
    implicitKnown.regions[0].known = nullptr;
    check(implicitKnown.run() == NBA97_TEXT_ARGUMENT &&
        implicitKnown.progress.stopped_pc == 0x80029150u);

    Fixture unknownOld;
    *unknownOld.known(Header + 2u) = 0;
    check(unknownOld.run() == NBA97_TEXT_UNKNOWN &&
        unknownOld.progress.operations == 3 &&
        unknownOld.progress.stopped_pc == 0x80029124u &&
        unknownOld.get(FrameSp + 0x10u) == 0x51525354u);

    Fixture unknownSp;
    unknownSp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 3;
    check(unknownSp.run() == NBA97_TEXT_UNKNOWN &&
        unknownSp.progress.operations == 1 &&
        unknownSp.progress.stopped_pc == 0x8002911cu);

    Fixture noStack;
    noStack.context.memory.count = 1;
    check(noStack.run() == NBA97_TEXT_RESOURCE &&
        noStack.progress.operations == 2 &&
        noStack.progress.stopped_pc == 0x80029120u);

    Fixture misaligned;
    misaligned.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
        EntrySp + 2u;
    check(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.stopped_pc == 0x80029120u);

    Fixture unknownSetting;
    *unknownSetting.known(Setting) = 0;
    check(unknownSetting.run() == NBA97_TEXT_UNKNOWN &&
        unknownSetting.progress.operations == 16 &&
        unknownSetting.progress.stopped_pc == 0x800291d0u &&
        unknownSetting.progress.volume_setting.known_mask == 0 &&
        unknownSetting.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_V1].word == 7 &&
        unknownSetting.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_V1].known_mask == 0x0e &&
        unknownSetting.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_A0].word == 105 &&
        unknownSetting.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_A0].known_mask == 0 &&
        unknownSetting.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_V0].word == 1 &&
        unknownSetting.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0e);

    Fixture unknownRa;
    unknownRa.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 5;
    check(unknownRa.run() == NBA97_TEXT_UNKNOWN &&
        unknownRa.progress.operations == 20 &&
        unknownRa.progress.stopped_pc == 0x800291f8u &&
        unknownRa.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp && unknownRa.progress.restored_return_address.known_mask == 5);

    Fixture malformedMemory;
    *malformedMemory.known(Header + 1u) = 2;
    check(malformedMemory.run() == NBA97_TEXT_ARGUMENT &&
        malformedMemory.progress.operations == 1);

    Fixture overlap;
    overlap.regions[1].base = Ram + 0x10u;
    check(overlap.run() == NBA97_TEXT_ARGUMENT &&
        overlap.progress.operations == 0);

    Fixture badZero;
    badZero.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
    check(badZero.run() == NBA97_TEXT_ARGUMENT);
    Fixture badJournal;
    badJournal.context.access_journal = nullptr;
    badJournal.context.access_journal_capacity = 1;
    check(badJournal.run() == NBA97_TEXT_ARGUMENT);

    Fixture wrap;
    wrap.regions[1] = {0, wrap.stack.data(), wrap.stackKnown.data(), 0x40};
    wrap.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x10u, 0x0f};
    check(wrap.run() == NBA97_TEXT_COMPLETE &&
        wrap.progress.frame_stack_pointer == 0xfffffff8u &&
        wrap.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 0x10u &&
        wrap.get(0x08u) == 0x51525354u &&
        wrap.get(0x0cu) == 0x8002dbd8u);

    Fixture movedSp;
    movedSp.mutateLiveSp = true;
    movedSp.put(Stack + 0x50u, 0x778899aau);
    movedSp.put(Stack + 0x54u, 0x81234560u);
    check(movedSp.run() == NBA97_TEXT_COMPLETE &&
        movedSp.progress.restored_s0.word == 0x778899aau &&
        movedSp.progress.restored_return_address.word == 0x81234560u &&
        movedSp.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Stack + 0x58u);

    Fixture nativeAlias;
    constexpr std::size_t stackRaOffset =
        static_cast<std::size_t>(FrameSp + 0x14u - Stack);
    nativeAlias.regions[1].data = nativeAlias.ram.data() +
        (Header - Ram) - stackRaOffset;
    nativeAlias.regions[1].known = nativeAlias.ramKnown.data() +
        (Header - Ram) - stackRaOffset;
    check(nativeAlias.run() == NBA97_TEXT_COMPLETE &&
        nativeAlias.progress.old_bank_header.word == 0x80100000u &&
        nativeAlias.progress.restored_return_address.word ==
            nativeAlias.headerResult &&
        nativeAlias.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            nativeAlias.headerResult);

    Nba97GameAudioInitializeContext context{};
    Nba97GameAudioInitializeProgress progress{};
    check(nba97_game_audio_initialize(nullptr, nullptr) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_audio_initialize(&context, nullptr) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_audio_initialize(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    exactCallsRegistersAndAccesses();
    optionalBranchAndAllVolumes();
    CallbackFailuresAndEveryBudgetPrefix();
    KnownnessMappingAlignmentAndWrap();
    std::printf("%u game audio-initialize checks passed\n", checks);
    return 0;
}
