#include "game_speech_startup_adapter.h"

#include <cstring>

namespace {
struct AdapterRun {
    Nba97GameSceneRandomWarmupIo unresolved_io;
    void* unresolved_user;
    const Nba97GameSpeechStartupContext* speech;
    const Nba97GameRandomSeedContext* seed;
    Nba97GameSpeechStartupAdapterProgress* out;
};

int dispatch(void* user, const Nba97GameTextMemory* memory,
    const Nba97GameSceneRandomWarmupEvent* event,
    Nba97GameSceneRandomWarmupRegisters* registers) {
    auto& run = *static_cast<AdapterRun*>(user);
    if (event->kind == NBA97_GAME_SCENE_RANDOM_WARMUP_STARTUP_800800F8 &&
        event->pc == UINT32_C(0x800802b4) &&
        event->delay_slot_pc == UINT32_C(0x800802b8) &&
        event->entry == UINT32_C(0x800800f8))
        return nba97_game_speech_startup_from_warmup(memory, event, registers,
            run.speech, run.out) == NBA97_TEXT_COMPLETE;
    if (event->kind == NBA97_GAME_SCENE_RANDOM_WARMUP_SEED_80093694 &&
        event->pc == UINT32_C(0x800802d0) &&
        event->delay_slot_pc == UINT32_C(0x800802d4) &&
        event->entry == UINT32_C(0x80093694)) {
        Nba97GameRandomSeedAdapterProgress seed_progress{};
        seed_progress.seed_result = NBA97_TEXT_COMPLETE;
        const int result = nba97_game_random_seed_from_warmup(memory, event,
            registers, run.seed, &seed_progress);
        run.out->seed = seed_progress.seed;
        run.out->seed_result = seed_progress.seed_result;
        run.out->seed_invocations += seed_progress.seed_invocations;
        run.out->seed_event = seed_progress.seed_event;
        return result == NBA97_TEXT_COMPLETE;
    }
    if (!run.unresolved_io)
        return 0;
    const int accepted = run.unresolved_io(run.unresolved_user, memory,
        event, registers);
    if (accepted == 1)
        ++run.out->unresolved_callbacks_completed;
    return accepted;
}
}

int nba97_game_speech_startup_from_warmup(const Nba97GameTextMemory* memory,
    const Nba97GameSceneRandomWarmupEvent* event,
    Nba97GameSceneRandomWarmupRegisters* registers,
    const Nba97GameSpeechStartupContext* speech,
    Nba97GameSpeechStartupAdapterProgress* out) {
    if (!memory || !event || !registers || !speech || !out ||
        event->kind != NBA97_GAME_SCENE_RANDOM_WARMUP_STARTUP_800800F8 ||
        event->pc != UINT32_C(0x800802b4) ||
        event->delay_slot_pc != UINT32_C(0x800802b8) ||
        event->entry != UINT32_C(0x800800f8) || event->argument_count != 0)
        return NBA97_TEXT_ARGUMENT;
    ++out->speech_invocations;
    out->speech_event = *event;
    Nba97GameSpeechStartupContext context = *speech;
    context.memory = *memory;
    context.registers = *registers;
    out->speech_result = nba97_game_speech_startup(&context, &out->speech);
    *registers = out->speech.registers;
    return out->speech_result;
}

int nba97_game_scene_random_warmup_with_speech_startup(
    const Nba97GameSceneRandomWarmupContext* warmup,
    const Nba97GameSpeechStartupContext* speech,
    const Nba97GameRandomSeedContext* seed,
    Nba97GameSceneRandomWarmupProgress* progress,
    Nba97GameSpeechStartupAdapterProgress* adapter_progress) {
    if (!warmup || !speech || !seed || !progress || !adapter_progress)
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapter_progress, 0, sizeof *adapter_progress);
    adapter_progress->speech_result = NBA97_TEXT_COMPLETE;
    adapter_progress->seed_result = NBA97_TEXT_COMPLETE;
    Nba97GameSceneRandomWarmupContext composed = *warmup;
    AdapterRun run{warmup->io, warmup->user, speech, seed, adapter_progress};
    composed.io = dispatch;
    composed.user = &run;
    return nba97_game_scene_random_warmup(&composed, progress);
}
