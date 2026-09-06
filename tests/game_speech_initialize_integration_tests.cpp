#include "game_match_initialize_adapter.h"
#include "game_speech_initialize_adapter.h"

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
            "game speech-initialize integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ff000u;
constexpr std::uint32_t EntrySp = 0x807fff80u;

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
    Nba97GameSpeechInitializeContext speech{};
    Nba97GameSpeechInitializeProgress speechProgress{};
    Nba97GameSpeechInitializeDependencies dependencies{};
    Nba97GameSpeechInitializeAdapterProgress adapterProgress{};
    std::vector<Nba97GameSpeechInitializeEvent> unresolved;
    std::vector<Nba97GameResourceLoaderEvent> attempts;

    Fixture() {
        stack.fill(0);
        stackKnown.fill(1);
        speech.memory = {regions, 2};
        speech.operation_budget = 3000;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            speech.registers.gpr[i] = {0x50000000u + i, 0x0f};
        speech.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        speech.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        speech.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x8002dbe0u, 0x0f};
        speech.io = unresolvedIo;
        speech.user = this;
        dependencies.resource_loader_operation_budget = 8;
        dependencies.resource_loader_io = loaderIo;
        dependencies.resource_loader_user = this;
        put(0x80015018u, 1);
        put(0x8001edf4u, 3, 2);
        put(0x8001eeb8u, 7, 2);
        put(0x80021d74u, 3);
        put(0x80021d78u, 7);
        for (unsigned i = 0; i < 12; ++i) {
            const std::uint32_t home = 0x80030000u + i * 0x20u;
            const std::uint32_t away = 0x80030400u + i * 0x20u;
            put(0x80020b8cu + i * 4u, home);
            put(0x80020bbcu + i * 4u, away);
            put(home, i, 2);
            put(away, 100u + i, 2);
            put(home + 7u, i, 1);
            put(away + 7u, 100u + i, 1);
        }
    }
    std::uint8_t* byte(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base &&
                std::uint64_t(address - region.base) < region.size)
                return region.data + (address - region.base);
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
    static int loaderIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameResourceLoaderEvent* event,
        Nba97GameResourceLoaderValue* value) {
        auto& f = *static_cast<Fixture*>(user);
        f.attempts.push_back(*event);
        value->word = 0x81000000u +
            static_cast<std::uint32_t>(f.attempts.size());
        value->known = 1;
        return 1;
    }
    static int unresolvedIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameSpeechInitializeEvent* event,
        Nba97GameSpeechInitializeRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        f.unresolved.push_back(*event);
        const auto a0 = registers->gpr[NBA97_MATCH_INITIALIZE_A0];
        const auto a3 = registers->gpr[NBA97_MATCH_INITIALIZE_A3];
        switch (event->kind) {
        case NBA97_GAME_SPEECH_INITIALIZE_LOOKUP_8007FC08:
            check(a3.known_mask == 0x0f);
            putThrough(*memory, a3.word,
                0x82000000u + static_cast<std::uint32_t>(event->invocation));
            putThrough(*memory, a3.word + 4u, 4);
            break;
        case NBA97_GAME_SPEECH_INITIALIZE_ALLOCATE_80090160:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x80300000u, 0x0f};
            break;
        case NBA97_GAME_SPEECH_INITIALIZE_CONVERT_800AE54C:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                a0.word + 0x1000u, a0.known_mask};
            break;
        case NBA97_GAME_SPEECH_INITIALIZE_RELEASE_80090698:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x1234abcdu, 0x0f};
            break;
        default:
            break;
        }
        return 1;
    }
    int run() {
        unresolved.clear();
        attempts.clear();
        return nba97_game_speech_initialize_with_recovered_dependencies(
            &speech, &dependencies, &speechProgress, &adapterProgress);
    }
};

void recoveredLoaderComposition() {
    Fixture f;
    check(f.run() == NBA97_TEXT_COMPLETE && f.speechProgress.completed);
    check(f.adapterProgress.resource_loader_invocations == 3 &&
        f.adapterProgress.unresolved_callbacks_completed == 306 &&
        f.attempts.size() == 3 && f.unresolved.size() == 306);
    for (unsigned i = 0; i < 3; ++i)
        check(f.adapterProgress.resource_loader_result[i] ==
                NBA97_TEXT_COMPLETE &&
            f.adapterProgress.resource_loader[i].completed &&
            f.adapterProgress.resource_loader[i].operations == 7);
    check(f.attempts[0].argument[0] == 0x80027b28u &&
        f.attempts[1].argument[0] == 0x80027b34u &&
        f.attempts[2].argument[0] == 0x80027b78u &&
        f.attempts[2].argument[1] == 0x20u);
    check(f.get(0x800fe9c8u) == 0x81000001u &&
        f.get(0x800febdcu) == 0x81000002u &&
        f.speechProgress.index_payload.word == 0x81000003u);

    Fixture bounded;
    bounded.dependencies.resource_loader_operation_budget = 0;
    check(bounded.run() == NBA97_TEXT_IO_REFUSED &&
        bounded.adapterProgress.resource_loader_invocations == 1 &&
        bounded.adapterProgress.resource_loader_result[0] == NBA97_TEXT_LIMIT &&
        bounded.adapterProgress.resource_loader[0].operations == 0);

    Fixture fallback;
    fallback.speech.registers.gpr[NBA97_MATCH_INITIALIZE_GP].known_mask = 0;
    check(fallback.run() == NBA97_TEXT_COMPLETE &&
        fallback.adapterProgress.resource_loader_invocations == 0 &&
        fallback.adapterProgress.unresolved_callbacks_completed == 309);
}

struct Natural {
    Fixture fixture;
    Nba97GameMatchInitializeContext parent{};
    Nba97GameMatchInitializeProgress parentProgress{};
    Nba97GameMatchInitializeAdapterProgress parentAdapter{};
    std::vector<Nba97GameMatchInitializeEvent> calls;
    std::size_t speechCalls = 0;

    Natural() {
        parent.memory = fixture.speech.memory;
        parent.operation_budget = 32;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            parent.registers.gpr[i] = {0x60000000u + i, 0x0f};
        parent.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        parent.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        parent.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x8002da84u, 0x0f};
        parent.io = io;
        parent.user = this;
    }
    static int io(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameMatchInitializeEvent* event,
        Nba97GameMatchInitializeRegisters* registers) {
        auto& natural = *static_cast<Natural*>(user);
        natural.calls.push_back(*event);
        if (event->entry != 0x8007fd40u)
            return 1;
        ++natural.speechCalls;
        Nba97GameSpeechInitializeRegisters speechRegisters{};
        if (nba97_game_speech_initialize_registers_from_match_initialize(
                event, registers, &speechRegisters) != NBA97_TEXT_COMPLETE)
            return 0;
        Nba97GameSpeechInitializeContext speech{
            *memory, 3000, speechRegisters, Fixture::unresolvedIo,
            &natural.fixture, nullptr, 0};
        if (nba97_game_speech_initialize_with_recovered_dependencies(&speech,
                &natural.fixture.dependencies,
                &natural.fixture.speechProgress,
                &natural.fixture.adapterProgress) != NBA97_TEXT_COMPLETE)
            return 0;
        *registers = natural.fixture.speechProgress.registers;
        return 1;
    }
};

void naturalMatchInitializerComposition() {
    Natural natural;
    check(nba97_game_match_initialize_with_zero(&natural.parent, 1100,
            &natural.parentProgress, &natural.parentAdapter) ==
            NBA97_TEXT_COMPLETE &&
        natural.parentProgress.completed && natural.speechCalls == 1 &&
        natural.fixture.speechProgress.completed);
    check(natural.calls.size() == 11 && natural.calls[2].pc == 0x8002dbd8u &&
        natural.calls[2].entry == 0x8007fd40u &&
        natural.fixture.speechProgress.frame_stack_pointer ==
            EntrySp - 0x18u - 0x38u);
    check(natural.parentProgress.registers
            .gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
        natural.parentProgress.registers
            .gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x8002da84u &&
        natural.parentProgress.registers
            .gpr[NBA97_MATCH_INITIALIZE_V0].word == 0x1234abcdu);

    Nba97GameMatchInitializeEvent event{};
    Nba97GameMatchInitializeRegisters registers{};
    Nba97GameSpeechInitializeRegisters out{};
    check(nba97_game_speech_initialize_registers_from_match_initialize(
        &event, &registers, &out) == NBA97_TEXT_ARGUMENT);
    event = {0x8002dbd8u, 0x8002dbdcu, 0x8007fd40u, 1,
        NBA97_MATCH_INITIALIZE_CHILD_8007FD40, 0};
    registers.gpr[NBA97_MATCH_INITIALIZE_T9] = {0xaabbccddu, 3};
    check(nba97_game_speech_initialize_registers_from_match_initialize(
            &event, &registers, &out) == NBA97_TEXT_COMPLETE &&
        out.gpr[NBA97_MATCH_INITIALIZE_T9].word == 0xaabbccddu &&
        out.gpr[NBA97_MATCH_INITIALIZE_T9].known_mask == 3);
}
}

int main() {
    recoveredLoaderComposition();
    naturalMatchInitializerComposition();
    std::printf("%u game speech-initialize integration checks passed\n",
        checks);
    return 0;
}
