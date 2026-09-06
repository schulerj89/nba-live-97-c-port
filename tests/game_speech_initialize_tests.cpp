#include "recovered/game_speech_initialize.h"

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
            "game speech-initialize check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

bool sameRegisters(const Nba97GameSpeechInitializeRegisters& left,
    const Nba97GameSpeechInitializeRegisters& right) {
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (left.gpr[i].word != right.gpr[i].word ||
            left.gpr[i].known_mask != right.gpr[i].known_mask)
            return false;
    return true;
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ff000u;
constexpr std::uint32_t EntrySp = 0x807fff80u;
constexpr std::uint32_t Records = 0x80102fe0u;

struct Call {
    Nba97GameSpeechInitializeEvent event{};
    std::array<Nba97GameSpeechInitializeWord, 4> argument{};
    Nba97GameSpeechInitializeRegisters registers{};
};

struct Fixture {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x110000, 0);
    std::vector<std::uint8_t> ramKnown =
        std::vector<std::uint8_t>(0x110000, 1);
    std::array<std::uint8_t, 0x1000> stack{};
    std::array<std::uint8_t, 0x1000> stackKnown{};
    Nba97GameTextRegion regions[2] = {
        {Ram, ram.data(), ramKnown.data(), ram.size()},
        {Stack, stack.data(), stackKnown.data(), stack.size()}
    };
    Nba97GameSpeechInitializeContext context{};
    Nba97GameSpeechInitializeProgress progress{};
    std::vector<Nba97GameSpeechInitializeAccess> journal =
        std::vector<Nba97GameSpeechInitializeAccess>(900);
    std::vector<Call> calls;
    std::uint32_t secondLanguage = 0;
    bool sparse = false;
    bool mutateFirstOpenS0 = false;
    bool mutateCategoryLoop = false;
    bool moveStack = false;
    bool unknownFirstRecord = false;
    bool relocateS1OnFirstConvert = false;
    std::uint32_t recordLength = 4;
    std::size_t refuseOperation = 0;
    std::size_t malformedOperation = 0;

    explicit Fixture(std::uint32_t language = 1) {
        stack.fill(0);
        stackKnown.fill(1);
        context.memory = {regions, 2};
        context.operation_budget = 3000;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {0x40000000u + i, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x8002dbe0u, 0x0f};
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        put(0x80015018u, language);
        put(0x8001edf4u, 0x89abu, 2);
        put(0x8001eeb8u, 0xcdefu, 2);
        for (unsigned i = 0; i < 12; ++i) {
            const std::uint32_t home = 0x80030000u + i * 0x20u;
            const std::uint32_t away = 0x80030400u + i * 0x20u;
            put(0x80020b8cu + i * 4u, home);
            put(0x80020bbcu + i * 4u, away);
            put(home, 0x8000u + i, 2);
            put(away, 0x7fffu - i, 2);
            put(home + 7u, 0x80u + i, 1);
            put(away + 7u, 0x7fu - i, 1);
            const std::uint32_t alternate = 0x80030800u + i * 0x20u;
            put(0x80020c8cu + i * 4u, alternate);
            put(alternate, 0x1234u + i, 2);
            put(alternate + 7u, 0x40u + i, 1);
        }
        put(0x8001eef4u, 0x4567u, 2);
        for (unsigned i = 0; i < 100; ++i)
            put(0x80050000u + i * 12u, 0x40u + i);
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
        unsigned width = 4) {
        for (unsigned i = 0; i < width; ++i) {
            check(byte(address + i) != nullptr);
            *byte(address + i) =
                static_cast<std::uint8_t>(value >> (8u * i));
        }
    }
    std::uint32_t get(std::uint32_t address) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i) {
            check(byte(address + i) != nullptr);
            value |= std::uint32_t(*byte(address + i)) << (8u * i);
        }
        return value;
    }
    static void putThrough(const Nba97GameTextMemory& memory,
        std::uint32_t address, std::uint32_t value) {
        for (unsigned byteIndex = 0; byteIndex < 4; ++byteIndex) {
            bool found = false;
            for (std::size_t i = 0; i < memory.count; ++i) {
                auto& region = memory.region[i];
                const std::uint32_t current = address + byteIndex;
                if (current < region.base ||
                    std::uint64_t(current - region.base) >= region.size)
                    continue;
                region.data[current - region.base] =
                    static_cast<std::uint8_t>(value >> (8u * byteIndex));
                if (region.known)
                    region.known[current - region.base] = 1;
                found = true;
                break;
            }
            check(found);
        }
    }
    static int io(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameSpeechInitializeEvent* event,
        Nba97GameSpeechInitializeRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        Call call;
        call.event = *event;
        for (unsigned i = 0; i < 4; ++i)
            call.argument[i] = registers->gpr[NBA97_MATCH_INITIALIZE_A0 + i];
        call.registers = *registers;
        f.calls.push_back(call);
        if (f.refuseOperation == event->operation)
            return 0;
        if (f.malformedOperation == event->operation) {
            registers->gpr[NBA97_MATCH_INITIALIZE_T9].known_mask = 0x10;
            return 1;
        }
        switch (event->kind) {
        case NBA97_GAME_SPEECH_INITIALIZE_RESOURCE_LOAD_80029BFC:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                0x81000000u + static_cast<std::uint32_t>(event->invocation),
                0x0f};
            if (event->invocation == 2 && f.secondLanguage)
                putThrough(*memory, 0x80015018u, f.secondLanguage);
            break;
        case NBA97_GAME_SPEECH_INITIALIZE_INSTALL_800ADBF8:
            break;
        case NBA97_GAME_SPEECH_INITIALIZE_OPEN_800AEC00:
            if (event->invocation == 1 && f.mutateFirstOpenS0)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {
                    0x8001eef4u, 0x0f};
            break;
        case NBA97_GAME_SPEECH_INITIALIZE_LOOKUP_8007FC08: {
            check(call.argument[3].known_mask == 0x0f);
            const bool present = !f.sparse || (event->invocation & 1u) != 0;
            putThrough(*memory, call.argument[3].word,
                present ? 0x82000000u +
                    static_cast<std::uint32_t>(event->invocation) : 0);
            putThrough(*memory, call.argument[3].word + 4u, f.recordLength);
            if (f.unknownFirstRecord && call.argument[3].word == Records) {
                putThrough(*memory, Records, 0);
                check(f.known(Records) != nullptr);
                *f.known(Records) = 0;
            }
            if (f.mutateCategoryLoop && call.argument[1].word == 2)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 2] = {
                    UINT32_MAX, 0x0f};
            break;
        }
        case NBA97_GAME_SPEECH_INITIALIZE_ALLOCATE_80090160:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x80300000u, 0x0f};
            break;
        case NBA97_GAME_SPEECH_INITIALIZE_CONVERT_800AE54C:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                call.argument[0].word + 0x1000u, call.argument[0].known_mask};
            if (event->invocation == 1 && f.relocateS1OnFirstConvert)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = {
                    0x80050000u, 0x0f};
            break;
        case NBA97_GAME_SPEECH_INITIALIZE_RELEASE_80090698:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0xfeedfaceu, 0x0f};
            if (f.moveStack) {
                const std::uint32_t moved = Stack + 0x400u;
                registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {moved, 0x0f};
                for (unsigned i = 0; i < 10; ++i)
                    putThrough(*memory, moved + 0x10u + i * 4u,
                        0x70000000u + i);
            }
            break;
        default:
            break;
        }
        return 1;
    }
    int run() {
        calls.clear();
        return nba97_game_speech_initialize(&context, &progress);
    }
};

void fullLanguagesAndRecords() {
    constexpr std::uint32_t firstNames[3][2] = {
        {0x80027b28u, 0x80027b34u},
        {0x80027b44u, 0x80027b50u},
        {0x80027b60u, 0x80027b6cu}
    };
    constexpr std::uint32_t indexNames[3] = {
        0x80027b78u, 0x80027b88u, 0x80027b98u
    };
    for (unsigned languageCase = 0; languageCase < 3; ++languageCase) {
        const std::uint32_t language = languageCase + 1u;
        Fixture f(languageCase == 2 ? 9u : language);
        check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
        check(f.progress.call_count[
                NBA97_GAME_SPEECH_INITIALIZE_RESOURCE_LOAD_80029BFC] == 3 &&
            f.progress.call_count[
                NBA97_GAME_SPEECH_INITIALIZE_LOOKUP_8007FC08] == 100 &&
            f.progress.call_count[
                NBA97_GAME_SPEECH_INITIALIZE_COPY_8009CB0C] == 100 &&
            f.progress.call_count[
                NBA97_GAME_SPEECH_INITIALIZE_CONVERT_800AE54C] == 100);
        check(f.calls[0].argument[0].word == firstNames[languageCase][0] &&
            f.calls[1].argument[0].word == firstNames[languageCase][1] &&
            f.calls[2].argument[0].word == indexNames[languageCase]);
        check(f.get(0x800fe9c8u) == 0x81000001u &&
            f.get(0x800febdcu) == 0x81000002u &&
            f.get(0x800feabcu) == 0x80300000u);
        check(f.progress.allocation_size.word == 400u &&
            f.progress.allocation_size.known_mask == 0x0f);
        for (unsigned i = 0; i < 100; ++i) {
            check(f.get(Records + i * 12u) == 0x80300000u + i * 4u);
            check(f.get(Records + i * 12u + 4u) ==
                0x80301000u + i * 4u);
        }
        for (unsigned i = 100; i < 110; ++i)
            check(f.get(Records + i * 12u + 4u) == UINT32_MAX);
        check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
                EntrySp &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
                0x8002dbe0u &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
                0xfeedfaceu);
    }
}

void liveSecondLanguageSignedInputsAndSparseRecords() {
    Fixture f(1);
    f.secondLanguage = 2;
    f.sparse = true;
    check(f.run() == NBA97_TEXT_COMPLETE);
    check(f.calls[0].argument[0].word == 0x80027b28u &&
        f.calls[1].argument[0].word == 0x80027b34u &&
        f.calls[2].argument[0].word == 0x80027b88u);
    check(f.progress.first_language.word == 1 &&
        f.progress.second_language.word == 2 &&
        f.progress.allocation_size.word == 200u);
    std::vector<Call> lookups;
    for (const auto& call : f.calls)
        if (call.event.kind == NBA97_GAME_SPEECH_INITIALIZE_LOOKUP_8007FC08)
            lookups.push_back(call);
    check(lookups.size() == 100);
    check(lookups[4].argument[2].word == 0xffff8000u &&
        lookups[5].argument[2].word == 0x00007fffu &&
        lookups[6].argument[2].word == 0xffffff80u &&
        lookups[7].argument[2].word == 0x0000007fu);
    check(f.progress.call_count[
            NBA97_GAME_SPEECH_INITIALIZE_COPY_8009CB0C] == 50 &&
        f.progress.call_count[
            NBA97_GAME_SPEECH_INITIALIZE_CONVERT_800AE54C] == 50);
}

void failuresBudgetsKnownnessAndMapping() {
    Fixture complete;
    check(complete.run() == NBA97_TEXT_COMPLETE);
    const std::size_t operationCount = complete.progress.operations;
    check(operationCount > 1000);
    std::size_t nextCall = 0;
    for (std::size_t budget = 0; budget < operationCount; ++budget) {
        Fixture bounded;
        bounded.context.operation_budget = budget;
        check(bounded.run() == NBA97_TEXT_LIMIT &&
            bounded.progress.operations == budget &&
            !bounded.progress.completed);
        if (nextCall < complete.calls.size() &&
            complete.calls[nextCall].event.operation == budget + 1u) {
            check(sameRegisters(bounded.progress.registers,
                complete.calls[nextCall].registers));
            ++nextCall;
        }
    }
    check(nextCall == complete.calls.size());
    for (const auto& call : complete.calls) {
        Fixture refusedCall;
        refusedCall.refuseOperation = call.event.operation;
        check(refusedCall.run() == NBA97_TEXT_IO_REFUSED &&
            refusedCall.progress.operations == call.event.operation &&
            !refusedCall.progress.completed);
    }

    Fixture refused;
    refused.refuseOperation = 14;
    check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.progress.operations == 14 &&
        refused.progress.callbacks_completed == 1);

    Fixture malformedCallback;
    malformedCallback.malformedOperation = 12;
    check(malformedCallback.run() == NBA97_TEXT_ARGUMENT &&
        malformedCallback.progress.operations == 12 &&
        malformedCallback.progress.callbacks_completed == 0);

    Fixture unknownLanguage;
    *unknownLanguage.known(0x80015018u) = 0;
    check(unknownLanguage.run() == NBA97_TEXT_UNKNOWN &&
        unknownLanguage.progress.stopped_pc == 0x8007fd74u &&
        unknownLanguage.progress.stores == 10 &&
        unknownLanguage.get(EntrySp - 0x28u) == 0x40000010u);

    Fixture unknownSp;
    unknownSp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 3;
    check(unknownSp.run() == NBA97_TEXT_UNKNOWN &&
        unknownSp.progress.stopped_pc == 0x8007fd50u &&
        unknownSp.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp - 0x38u);

    Fixture misaligned;
    misaligned.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
        EntrySp + 2u;
    check(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.stopped_pc == 0x8007fd50u);

    Fixture malformed;
    *malformed.known(0x80015018u) = 2;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations == 1);

    Fixture overlap;
    overlap.regions[1].base = Ram + 0x100u;
    check(overlap.run() == NBA97_TEXT_ARGUMENT &&
        overlap.progress.operations == 0);

    Nba97GameSpeechInitializeContext context{};
    Nba97GameSpeechInitializeProgress progress{};
    check(nba97_game_speech_initialize(nullptr, nullptr) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_speech_initialize(&context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_speech_initialize(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
}

void liveCursorsMovedStackAndBoundedRunaway() {
    Fixture live;
    live.mutateFirstOpenS0 = true;
    check(live.run() == NBA97_TEXT_COMPLETE);
    std::vector<Call> lookups;
    for (const auto& call : live.calls)
        if (call.event.kind == NBA97_GAME_SPEECH_INITIALIZE_LOOKUP_8007FC08)
            lookups.push_back(call);
    check(lookups[0].argument[2].word == 0x4567u &&
        lookups[4].argument[2].word == 0x1234u);

    Fixture moved;
    moved.moveStack = true;
    check(moved.run() == NBA97_TEXT_COMPLETE &&
        moved.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Stack + 0x438u &&
        moved.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x70000009u &&
        moved.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
            0x70000000u);
    Call releaseCall{};
    bool foundRelease = false;
    for (const auto& call : moved.calls)
        if (call.event.kind == NBA97_GAME_SPEECH_INITIALIZE_RELEASE_80090698) {
            releaseCall = call;
            foundRelease = true;
        }
    check(foundRelease);
    Nba97GameSpeechInitializeRegisters expected = releaseCall.registers;
    expected.gpr[NBA97_MATCH_INITIALIZE_V0] = {0xfeedfaceu, 0x0f};
    expected.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack + 0x400u, 0x0f};
    const unsigned restored[10] = {
        NBA97_MATCH_INITIALIZE_RA, NBA97_MATCH_INITIALIZE_FP,
        NBA97_MATCH_INITIALIZE_S0 + 7, NBA97_MATCH_INITIALIZE_S0 + 6,
        NBA97_MATCH_INITIALIZE_S0 + 5, NBA97_MATCH_INITIALIZE_S0 + 4,
        NBA97_MATCH_INITIALIZE_S0 + 3, NBA97_MATCH_INITIALIZE_S0 + 2,
        NBA97_MATCH_INITIALIZE_S0 + 1, NBA97_MATCH_INITIALIZE_S0
    };
    for (unsigned i = 0; i < 10; ++i) {
        std::size_t operation = 0;
        for (std::size_t eventIndex = 0;
            eventIndex < moved.progress.access_events; ++eventIndex)
            if (moved.journal[eventIndex].pc == 0x800800c4u + i * 4u)
                operation = moved.journal[eventIndex].operation;
        check(operation != 0);
        Fixture prefix;
        prefix.moveStack = true;
        prefix.context.operation_budget = operation - 1u;
        check(prefix.run() == NBA97_TEXT_LIMIT &&
            prefix.progress.stopped_pc == 0x800800c4u + i * 4u &&
            sameRegisters(prefix.progress.registers, expected));
        expected.gpr[restored[i]] = {0x70000009u - i, 0x0f};
    }

    Fixture runaway;
    runaway.mutateCategoryLoop = true;
    runaway.context.operation_budget = 400;
    check(runaway.run() == NBA97_TEXT_LIMIT &&
        runaway.progress.operations == 400 && !runaway.progress.completed &&
        runaway.progress.call_count[
            NBA97_GAME_SPEECH_INITIALIZE_LOOKUP_8007FC08] > 100);

    Fixture unknownRecord;
    unknownRecord.unknownFirstRecord = true;
    check(unknownRecord.run() == NBA97_TEXT_UNKNOWN &&
        unknownRecord.progress.stopped_pc == 0x80080018u);

    Fixture wrappedSize;
    wrappedSize.recordLength = UINT32_MAX;
    check(wrappedSize.run() == NBA97_TEXT_COMPLETE &&
        wrappedSize.progress.allocation_size.word == 0xffffff9cu);
}

void relocatedLengthCursorPersistsAcrossPackingLoop() {
    Fixture relocated;
    relocated.relocateS1OnFirstConvert = true;
    check(relocated.run() == NBA97_TEXT_COMPLETE);
    std::vector<Call> copies;
    for (const auto& call : relocated.calls)
        if (call.event.kind == NBA97_GAME_SPEECH_INITIALIZE_COPY_8009CB0C)
            copies.push_back(call);
    check(copies.size() == 100 && copies[0].argument[2].word == 4 &&
        copies[1].argument[2].word == 0x41u);
    check(copies[1].registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
            Records + 12u &&
        copies[1].registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word ==
            0x8005000cu);

    Nba97GameSpeechInitializeAccess nextLength{};
    bool found = false;
    for (std::size_t i = 0; i < relocated.progress.access_events; ++i)
        if (relocated.journal[i].pc == 0x80080088u &&
            relocated.journal[i].address == 0x8005000cu) {
            nextLength = relocated.journal[i];
            found = true;
            break;
        }
    check(found && nextLength.kind == NBA97_GAME_SPEECH_INITIALIZE_READ &&
        nextLength.value == 0x41u);

    Fixture prefix;
    prefix.relocateS1OnFirstConvert = true;
    prefix.context.operation_budget = nextLength.operation - 1u;
    check(prefix.run() == NBA97_TEXT_LIMIT &&
        prefix.progress.stopped_pc == 0x80080088u &&
        prefix.progress.stopped_address == 0x8005000cu &&
        prefix.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
            Records + 12u &&
        prefix.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word ==
            0x8005000cu);
}
}

int main() {
    fullLanguagesAndRecords();
    liveSecondLanguageSignedInputsAndSparseRecords();
    failuresBudgetsKnownnessAndMapping();
    liveCursorsMovedStackAndBoundedRunaway();
    relocatedLengthCursorPersistsAcrossPackingLoop();
    std::printf("%u game speech-initialize checks passed\n", checks);
    return 0;
}
