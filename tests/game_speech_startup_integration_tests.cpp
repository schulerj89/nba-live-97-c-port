#include "game_speech_startup_adapter.h"

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
            "game speech startup integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ff800u;
constexpr std::uint32_t EntrySp = 0x807fff00u;
constexpr std::uint32_t WarmupSp = EntrySp - 0x18u;
constexpr std::uint32_t SpeechSp = WarmupSp - 0x20u;
constexpr std::uint32_t CallerRa = 0x8002da8cu;

struct Composition {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x110000);
    std::vector<std::uint8_t> ram_known =
        std::vector<std::uint8_t>(0x110000, 1);
    std::array<std::uint8_t, 0x800> stack{};
    std::array<std::uint8_t, 0x800> stack_known{};
    std::array<Nba97GameTextRegion, 2> regions{{
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}}};
    std::array<Nba97GameRandomSeedAccess, 6> seed_journal{};
    Nba97GameSceneRandomWarmupContext warmup{};
    Nba97GameSpeechStartupContext speech{};
    Nba97GameRandomSeedContext seed{};
    Nba97GameSceneRandomWarmupProgress warmup_progress{};
    Nba97GameSpeechStartupAdapterProgress adapter_progress{};
    std::vector<Nba97GameSceneRandomWarmupEvent> unresolved;
    std::vector<Nba97GameSpeechStartupEvent> speech_calls;
    Nba97GameSpeechStartupRegisters first_speech_child{};
    bool saw_first_speech_child = false;
    bool refuse_random = false;
    bool refuse_speech_cleanup = false;

    Composition() {
        stack.fill(0xcd);
        stack_known.fill(1);
        put(0x80015018u, 2);
        warmup.memory = {regions.data(), regions.size()};
        warmup.operation_budget = 72;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            warmup.registers.gpr[i] = {
                0x51000000u + i * 0x01010101u, 0x0f};
        warmup.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        warmup.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        warmup.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {CallerRa, 0x0f};
        warmup.registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {
            0x11223344u, 0x0f};
        warmup.io = warmupIo;
        warmup.user = this;
        speech.operation_budget = 22;
        speech.io = speechIo;
        speech.user = this;
        seed.operation_budget = 6;
        seed.access_journal = seed_journal.data();
        seed.access_journal_capacity = seed_journal.size();
    }

    void put(std::uint32_t address, std::uint32_t value) {
        auto* bytes = ram.data() + (address - Ram);
        for (unsigned i = 0; i < 4; ++i)
            bytes[i] = static_cast<std::uint8_t>(value >> (8u * i));
    }
    std::uint32_t get(std::uint32_t address) const {
        const std::uint8_t* bytes = address >= Stack ?
            stack.data() + (address - Stack) : ram.data() + (address - Ram);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(bytes[i]) << (8u * i);
        return value;
    }

    static int speechIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameSpeechStartupEvent* event,
        Nba97GameSpeechStartupRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.speech_calls.push_back(*event);
        if (!c.saw_first_speech_child) {
            c.saw_first_speech_child = true;
            c.first_speech_child = *registers;
        }
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_800853F4)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x81234560u, 0x0f};
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_80083D38)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x8abcdef0u, 0x0f};
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_800A5810)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {100, 0x0f};
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_8008847C)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {1, 0x0f};
        if (event->kind == NBA97_GAME_SPEECH_STARTUP_CHILD_8002ABB4) {
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0xdecafbadu, 0x0f};
            registers->gpr[NBA97_MATCH_INITIALIZE_T9] = {0x13579bdfu, 0x06};
            if (c.refuse_speech_cleanup)
                return 0;
        }
        return 1;
    }

    static int warmupIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameSceneRandomWarmupEvent* event,
        Nba97GameSceneRandomWarmupRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.unresolved.push_back(*event);
        if (event->kind == NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70) {
            if (c.refuse_random)
                return 0;
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                event->invocation == 1 ? 0u : 0xfacecafeu, 0x0f};
        }
        if (event->kind == NBA97_GAME_SCENE_RANDOM_WARMUP_STEP_800935C4)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                0x70000000u + static_cast<std::uint32_t>(event->invocation),
                0x0f};
        return 1;
    }

    int run() {
        return nba97_game_scene_random_warmup_with_speech_startup(&warmup,
            &speech, &seed, &warmup_progress, &adapter_progress);
    }
};

void natural_full_gpr_warmup_first_event_and_existing_seed() {
    Composition c;
    const auto entry = c.warmup.registers;
    check(c.run() == NBA97_TEXT_COMPLETE && c.warmup_progress.completed &&
        c.adapter_progress.speech_result == NBA97_TEXT_COMPLETE &&
        c.adapter_progress.seed_result == NBA97_TEXT_COMPLETE &&
        c.adapter_progress.speech_invocations == 1 &&
        c.adapter_progress.seed_invocations == 1);
    check(c.adapter_progress.speech_event.pc == 0x800802b4u &&
        c.adapter_progress.speech_event.delay_slot_pc == 0x800802b8u &&
        c.adapter_progress.speech_event.entry == 0x800800f8u &&
        c.adapter_progress.speech_event.operation == 3u);
    check(c.adapter_progress.seed_event.pc == 0x800802d0u &&
        c.adapter_progress.seed_event.entry == 0x80093694u &&
        c.adapter_progress.seed.completed &&
        c.adapter_progress.seed.operations == 6u);
    check(c.speech_calls.size() == 11 && c.unresolved.size() == 66 &&
        c.unresolved.front().kind ==
            NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70 &&
        c.unresolved.back().kind ==
            NBA97_GAME_SCENE_RANDOM_WARMUP_STEP_800935C4);
    check(c.adapter_progress.unresolved_callbacks_completed == 66 &&
        c.warmup_progress.startup_calls == 1 &&
        c.warmup_progress.seed_calls == 1 &&
        c.warmup_progress.random_calls == 2 &&
        c.warmup_progress.step_calls == 64);
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        auto expected = entry.gpr[i];
        if (i == NBA97_MATCH_INITIALIZE_SP)
            expected = {SpeechSp, 0x0f};
        if (i == NBA97_MATCH_INITIALIZE_RA)
            expected = {0x8008011cu, 0x0f};
        if (i == NBA97_MATCH_INITIALIZE_AT)
            expected = {0x800c0000u, 0x0f};
        check(c.first_speech_child.gpr[i].word == expected.word &&
            c.first_speech_child.gpr[i].known_mask == expected.known_mask);
    }
    check(c.adapter_progress.speech.frame_stack_pointer == SpeechSp &&
        c.adapter_progress.speech.restored_return_address.word == 0x800802bcu &&
        c.adapter_progress.speech.restored_s0.word == 0x11223344u);
    check(c.warmup_progress.restored_return_address.word == CallerRa &&
        c.warmup_progress.restored_s0.word == 0x11223344u &&
        c.warmup_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp &&
        c.warmup_progress.registers.gpr[NBA97_MATCH_INITIALIZE_T9].word ==
            0x13579bdfu &&
        c.warmup_progress.registers.gpr[NBA97_MATCH_INITIALIZE_T9].known_mask ==
            0x06);
    check(c.get(SpeechSp + 0x1cu) == 0x800802bcu &&
        c.get(SpeechSp + 0x18u) == 0x11223344u &&
        c.get(WarmupSp + 0x14u) == CallerRa &&
        c.get(0x8002149cu) == 0x81234560u &&
        c.get(0x800dc7e8u) == 0x8abcdef0u);
    for (unsigned i = 0; i < 6; ++i)
        check(c.seed_journal[i].pc >= 0x800936b0u &&
            c.seed_journal[i].address == 0x800c4ae8u + i * 4u);
}

void nested_failure_prefixes_and_adapter_validation() {
    Composition speech_limit;
    speech_limit.speech.operation_budget = 4;
    check(speech_limit.run() == NBA97_TEXT_IO_REFUSED &&
        speech_limit.adapter_progress.speech_result == NBA97_TEXT_LIMIT &&
        speech_limit.adapter_progress.speech_invocations == 1 &&
        speech_limit.adapter_progress.speech.operations == 4 &&
        speech_limit.adapter_progress.speech.stopped_pc == 0x80080114u &&
        speech_limit.warmup_progress.operations == 3 &&
        !speech_limit.warmup_progress.callbacks_completed &&
        speech_limit.unresolved.empty());

    Composition speech_refusal;
    speech_refusal.refuse_speech_cleanup = true;
    check(speech_refusal.run() == NBA97_TEXT_IO_REFUSED &&
        speech_refusal.adapter_progress.speech_result == NBA97_TEXT_IO_REFUSED &&
        speech_refusal.adapter_progress.speech.callbacks_completed == 10 &&
        speech_refusal.adapter_progress.speech.stopped_pc == 0x8008022cu &&
        speech_refusal.unresolved.empty());

    Composition seed_limit;
    seed_limit.seed.operation_budget = 3;
    check(seed_limit.run() == NBA97_TEXT_IO_REFUSED &&
        seed_limit.adapter_progress.speech_result == NBA97_TEXT_COMPLETE &&
        seed_limit.adapter_progress.seed_result == NBA97_TEXT_LIMIT &&
        seed_limit.adapter_progress.seed.operations == 3 &&
        seed_limit.adapter_progress.seed.stopped_pc == 0x800936f8u &&
        seed_limit.warmup_progress.callbacks_completed == 3 &&
        seed_limit.unresolved.size() == 2);

    Composition random_refusal;
    random_refusal.refuse_random = true;
    check(random_refusal.run() == NBA97_TEXT_IO_REFUSED &&
        random_refusal.adapter_progress.speech_result == NBA97_TEXT_COMPLETE &&
        random_refusal.adapter_progress.seed_invocations == 0 &&
        random_refusal.unresolved.size() == 1 &&
        random_refusal.warmup_progress.callbacks_completed == 1);

    Composition args;
    Nba97GameSpeechStartupAdapterProgress out{};
    check(nba97_game_scene_random_warmup_with_speech_startup(nullptr,
        &args.speech, &args.seed, &args.warmup_progress, &out) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_scene_random_warmup_with_speech_startup(&args.warmup,
        nullptr, &args.seed, &args.warmup_progress, &out) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_scene_random_warmup_with_speech_startup(&args.warmup,
        &args.speech, nullptr, &args.warmup_progress, &out) ==
        NBA97_TEXT_ARGUMENT);
    Nba97GameSceneRandomWarmupEvent wrong{};
    Nba97GameSceneRandomWarmupRegisters registers{};
    check(nba97_game_speech_startup_from_warmup(&args.warmup.memory, &wrong,
        &registers, &args.speech, &out) == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    natural_full_gpr_warmup_first_event_and_existing_seed();
    nested_failure_prefixes_and_adapter_validation();
    std::printf("%u game speech startup integration checks passed\n", checks);
    return 0;
}
