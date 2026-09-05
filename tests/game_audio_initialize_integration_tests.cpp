#include "game_audio_initialize_adapter.h"
#include "recovered/game_match_initialize.h"
#include "game_match_initialize_adapter.h"

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
            "game audio-initialize integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ffc00u;
constexpr std::uint32_t EntrySp = 0x807fff80u;
constexpr std::uint32_t Header = 0x8001502cu;
constexpr std::uint32_t Setting = 0x80021d7cu;
constexpr std::uint32_t Result = 0x80021ee0u;

struct Fixture {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x110000, 0);
    std::vector<std::uint8_t> ramKnown =
        std::vector<std::uint8_t>(0x110000, 1);
    std::array<std::uint8_t, 0x400> stack{};
    std::array<std::uint8_t, 0x400> stackKnown{};
    Nba97GameTextRegion regions[2] = {
        {Ram, ram.data(), ramKnown.data(), ram.size()},
        {Stack, stack.data(), stackKnown.data(), stack.size()}
    };
    Nba97GameAudioInitializeContext audio{};
    Nba97GameAudioInitializeProgress audioProgress{};
    Nba97GameAudioInitializeAdapterProgress adapterProgress{};
    Nba97GameAudioInitializeDependencies dependencies{};
    std::vector<Nba97GameAudioInitializeEvent> unresolved;
    std::vector<Nba97GameResourceLoaderEvent> attempts;
    std::uint32_t headerHandle = 0x80110000u;
    std::uint32_t bodyHandle = 0x80120000u;
    std::uint32_t rawResult = 0xabcdef01u;
    bool corruptLoaderSavedS1 = false;

    Fixture() {
        stack.fill(0);
        stackKnown.fill(1);
        audio.memory = {regions, 2};
        audio.operation_budget = 64;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            audio.registers.gpr[i] = {0x30000000u + i, 0x0f};
        audio.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        audio.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        audio.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x8002dbd8u, 0x0f};
        audio.io = audioIo;
        audio.user = this;
        put(Header, 0);
        put(Setting, 9, 1);
        dependencies.resource_loader_operation_budget = 8;
        dependencies.resource_loader_io = loaderIo;
        dependencies.resource_loader_user = this;
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
    static int loaderIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameResourceLoaderEvent* event,
        Nba97GameResourceLoaderValue* value) {
        auto& f = *static_cast<Fixture*>(user);
        f.attempts.push_back(*event);
        value->known = 1;
        value->word = event->argument[0] == 0x800247bcu
            ? f.headerHandle : f.bodyHandle;
        if (f.corruptLoaderSavedS1) {
            check(f.known(event->stack_pointer + 0x14u) != nullptr);
            *f.known(event->stack_pointer + 0x14u) = 0;
        }
        return 1;
    }
    static int audioIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameAudioInitializeEvent* event,
        Nba97GameAudioInitializeRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        f.unresolved.push_back(*event);
        if (event->pc == 0x800291a0u) {
            check(registers->gpr[NBA97_MATCH_INITIALIZE_A0].word ==
                    0x80021d6cu);
            check(registers->gpr[NBA97_MATCH_INITIALIZE_A1].word ==
                    f.headerHandle);
            check(registers->gpr[NBA97_MATCH_INITIALIZE_A2].word ==
                    f.bodyHandle);
        }
        if (event->pc == 0x8002913cu)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                f.headerHandle, 0x0f};
        else if (event->pc == 0x80029154u)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                f.bodyHandle, 0x0f};
        else if (event->pc == 0x800291dcu)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {f.rawResult, 0x0f};
        return 1;
    }
    int runAdapted() {
        return nba97_game_audio_initialize_with_recovered_dependencies(
            &audio, &dependencies, &audioProgress, &adapterProgress);
    }
};

void recoveredChildComposition() {
    Fixture f;
    check(f.runAdapted() == NBA97_TEXT_COMPLETE && f.audioProgress.completed);
    check(f.adapterProgress.resource_loader_invocations == 2 &&
        f.adapterProgress.unresolved_callbacks_completed == 8 &&
        f.attempts.size() == 2 && f.unresolved.size() == 8);
    for (unsigned i = 0; i < 2; ++i)
        check(f.adapterProgress.resource_loader_result[i] ==
                NBA97_TEXT_COMPLETE &&
            f.adapterProgress.resource_loader[i].completed &&
            f.adapterProgress.resource_loader[i].load_attempts == 1 &&
            f.adapterProgress.resource_loader[i].operations == 7);
    check(f.attempts[0].argument[0] == 0x800247bcu &&
        f.attempts[1].argument[0] == 0x800247c8u &&
        f.attempts[0].argument[1] == 0 && f.attempts[1].argument[1] == 0);
    check(f.audioProgress.new_bank_header.word == f.headerHandle &&
        f.audioProgress.bank_body.word == f.bodyHandle &&
        f.audioProgress.scaled_volume.word == 127 &&
        f.get(Header) == f.headerHandle && f.get(Result) == f.rawResult);
    check(f.audioProgress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
            0x30000010u &&
        f.audioProgress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp &&
        f.audioProgress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8002dbd8u);
}

void composedFailurePrefixesAndFallback() {
    Fixture loaderBounded;
    loaderBounded.dependencies.resource_loader_operation_budget = 0;
    check(loaderBounded.runAdapted() == NBA97_TEXT_IO_REFUSED &&
        !loaderBounded.audioProgress.completed &&
        loaderBounded.adapterProgress.resource_loader_invocations == 1 &&
        loaderBounded.adapterProgress.resource_loader_result[0] ==
            NBA97_TEXT_LIMIT &&
        loaderBounded.adapterProgress.resource_loader[0].operations == 0 &&
        loaderBounded.audioProgress.callbacks_completed == 0 &&
        loaderBounded.audioProgress.registers
            .gpr[NBA97_MATCH_INITIALIZE_S0].word == 0x30000010u);

    Fixture afterFirstStore;
    afterFirstStore.dependencies.resource_loader_operation_budget = 1;
    check(afterFirstStore.runAdapted() == NBA97_TEXT_IO_REFUSED &&
        afterFirstStore.adapterProgress.resource_loader[0].stores == 1 &&
        afterFirstStore.audioProgress.registers
            .gpr[NBA97_MATCH_INITIALIZE_S0].word == 0x800247bcu &&
        afterFirstStore.audioProgress.registers
            .gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word == 0x30000011u);

    Fixture afterSecondStore;
    afterSecondStore.dependencies.resource_loader_operation_budget = 2;
    check(afterSecondStore.runAdapted() == NBA97_TEXT_IO_REFUSED &&
        afterSecondStore.adapterProgress.resource_loader[0].stores == 2 &&
        afterSecondStore.audioProgress.registers
            .gpr[NBA97_MATCH_INITIALIZE_S0].word == 0x800247bcu &&
        afterSecondStore.audioProgress.registers
            .gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word == 0);

    Fixture beforeAttempt;
    beforeAttempt.dependencies.resource_loader_operation_budget = 3;
    check(beforeAttempt.runAdapted() == NBA97_TEXT_IO_REFUSED &&
        beforeAttempt.adapterProgress.resource_loader[0].stores == 3 &&
        beforeAttempt.adapterProgress.resource_loader[0].stopped_entry ==
            0x800941c8u &&
        beforeAttempt.audioProgress.registers
            .gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x80029c20u &&
        beforeAttempt.audioProgress.registers
            .gpr[NBA97_MATCH_INITIALIZE_A0].known_mask == 0);

    Fixture epilogueUnknown;
    epilogueUnknown.corruptLoaderSavedS1 = true;
    check(epilogueUnknown.runAdapted() == NBA97_TEXT_IO_REFUSED &&
        epilogueUnknown.adapterProgress.resource_loader_result[0] ==
            NBA97_TEXT_UNKNOWN &&
        epilogueUnknown.adapterProgress.resource_loader[0].reads == 1 &&
        epilogueUnknown.audioProgress.registers
            .gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x80029144u &&
        epilogueUnknown.audioProgress.registers
            .gpr[NBA97_MATCH_INITIALIZE_S0 + 1].known_mask == 0);

    Fixture fallback;
    fallback.audio.registers.gpr[NBA97_MATCH_INITIALIZE_S0].known_mask = 3;
    check(fallback.runAdapted() == NBA97_TEXT_COMPLETE &&
        fallback.adapterProgress.resource_loader_invocations == 0 &&
        fallback.adapterProgress.unresolved_callbacks_completed == 10 &&
        fallback.unresolved.size() == 10);

    Nba97GameAudioInitializeProgress progress{};
    Nba97GameAudioInitializeAdapterProgress adapter{};
    Nba97GameAudioInitializeDependencies dependencies{};
    check(nba97_game_audio_initialize_with_recovered_dependencies(
        nullptr, &dependencies, &progress, &adapter) == NBA97_TEXT_ARGUMENT);
    Fixture invalid;
    check(nba97_game_audio_initialize_with_recovered_dependencies(
        &invalid.audio, nullptr, &invalid.audioProgress,
        &invalid.adapterProgress) == NBA97_TEXT_ARGUMENT);
}

struct NaturalComposition {
    Fixture fixture;
    Nba97GameMatchInitializeContext parent{};
    Nba97GameMatchInitializeProgress parentProgress{};
    std::array<Nba97GameAudioInitializeAccess, 16> audioJournal{};
    std::vector<Nba97GameMatchInitializeEvent> parentCalls;
    std::size_t audioInvocations = 0;

    NaturalComposition() {
        fixture.put(0x80021d74u, 5);
        fixture.put(0x80021d78u, 12);
        parent.memory = fixture.audio.memory;
        parent.operation_budget = 19;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            parent.registers.gpr[i] = {0x60000000u + i, 0x0f};
        parent.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        parent.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        parent.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x8002da84u, 0x0f};
        parent.io = parentIo;
        parent.user = this;
    }
    static int parentIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameMatchInitializeEvent* event,
        Nba97GameMatchInitializeRegisters* registers) {
        auto& c = *static_cast<NaturalComposition*>(user);
        c.parentCalls.push_back(*event);
        if (event->entry != 0x80029114u)
            return 1;
        ++c.audioInvocations;
        Nba97GameAudioInitializeRegisters audioRegisters{};
        if (nba97_game_audio_initialize_registers_from_match_initialize(
                event, registers, &audioRegisters) != NBA97_TEXT_COMPLETE)
            return 0;
        Nba97GameAudioInitializeContext audio{
            *memory, 64, audioRegisters, Fixture::audioIo, &c.fixture,
            c.audioJournal.data(), c.audioJournal.size()};
        if (nba97_game_audio_initialize_with_recovered_dependencies(&audio,
                &c.fixture.dependencies,&c.fixture.audioProgress,
                &c.fixture.adapterProgress) != NBA97_TEXT_COMPLETE)
            return 0;
        *registers = c.fixture.audioProgress.registers;
        return 1;
    }
};

void naturalMatchInitializerComposition() {
    NaturalComposition c;
    Nba97GameMatchInitializeAdapterProgress initializerAdapter{};
    check(nba97_game_match_initialize_with_zero(&c.parent,1100,&c.parentProgress,
            &initializerAdapter) ==
            NBA97_TEXT_COMPLETE &&
        c.parentProgress.completed && c.audioInvocations == 1 &&
        c.fixture.audioProgress.completed);
    check(c.parentCalls.size() == 11 &&
        c.parentCalls[1].pc == 0x8002dbd0u &&
        c.parentCalls[1].entry == 0x80029114u &&
        c.fixture.audioProgress.frame_stack_pointer == EntrySp - 0x30u &&
        c.fixture.audioProgress.restored_return_address.word == 0x8002dbd8u);
    check(c.parentProgress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp &&
        c.parentProgress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8002da84u &&
        c.parentProgress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            c.fixture.rawResult &&
        c.fixture.get(Result) == c.fixture.rawResult);

    Nba97GameMatchInitializeEvent event{};
    Nba97GameMatchInitializeRegisters registers{};
    Nba97GameAudioInitializeRegisters out{};
    check(nba97_game_audio_initialize_registers_from_match_initialize(
        &event, &registers, &out) == NBA97_TEXT_ARGUMENT);
    event = {0x8002dbd0u, 0x8002dbd4u, 0x80029114u, 1,
        NBA97_MATCH_INITIALIZE_CHILD_80029114, 0};
    registers.gpr[NBA97_MATCH_INITIALIZE_T9] = {0x12345678u, 3};
    check(nba97_game_audio_initialize_registers_from_match_initialize(
            &event, &registers, &out) == NBA97_TEXT_COMPLETE &&
        out.gpr[NBA97_MATCH_INITIALIZE_T9].word == 0x12345678u &&
        out.gpr[NBA97_MATCH_INITIALIZE_T9].known_mask == 3);
}
}

int main() {
    recoveredChildComposition();
    composedFailurePrefixesAndFallback();
    naturalMatchInitializerComposition();
    std::printf("%u game audio-initialize integration checks passed\n", checks);
    return 0;
}
