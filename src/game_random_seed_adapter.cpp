#include "game_random_seed_adapter.h"

#include <cstring>

namespace {
struct AdapterRun {
    Nba97GameSceneRandomWarmupIo unresolved_io;
    void* unresolved_user;
    const Nba97GameRandomSeedContext* seed;
    Nba97GameRandomSeedAdapterProgress* out;
};

int dispatch(void* user, const Nba97GameTextMemory* memory,
    const Nba97GameSceneRandomWarmupEvent* event,
    Nba97GameSceneRandomWarmupRegisters* registers) {
    auto& run = *static_cast<AdapterRun*>(user);
    if (event->kind == NBA97_GAME_SCENE_RANDOM_WARMUP_SEED_80093694 &&
        event->pc == UINT32_C(0x800802d0) &&
        event->delay_slot_pc == UINT32_C(0x800802d4) &&
        event->entry == UINT32_C(0x80093694)) {
        return nba97_game_random_seed_from_warmup(memory,event,registers,
            run.seed,run.out) == NBA97_TEXT_COMPLETE;
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

int nba97_game_random_seed_from_warmup(const Nba97GameTextMemory* memory,
    const Nba97GameSceneRandomWarmupEvent* event,
    Nba97GameSceneRandomWarmupRegisters* registers,
    const Nba97GameRandomSeedContext* seed,
    Nba97GameRandomSeedAdapterProgress* out) {
    if(!memory || !event || !registers || !seed || !out ||
       event->kind!=NBA97_GAME_SCENE_RANDOM_WARMUP_SEED_80093694 ||
       event->pc!=0x800802d0u || event->delay_slot_pc!=0x800802d4u ||
       event->entry!=0x80093694u || event->argument_count!=1)
        return NBA97_TEXT_ARGUMENT;
    ++out->seed_invocations;out->seed_event=*event;
    Nba97GameRandomSeedContext context=*seed;
    context.memory=*memory;context.registers=*registers;
    out->seed_result=nba97_game_random_seed(&context,&out->seed);
    *registers=out->seed.registers;return out->seed_result;
}

int nba97_game_scene_random_warmup_with_random_seed(
    const Nba97GameSceneRandomWarmupContext* warmup,
    const Nba97GameRandomSeedContext* seed,
    Nba97GameSceneRandomWarmupProgress* progress,
    Nba97GameRandomSeedAdapterProgress* adapter_progress) {
    if (!warmup || !seed || !progress || !adapter_progress)
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapter_progress, 0, sizeof *adapter_progress);
    adapter_progress->seed_result = NBA97_TEXT_COMPLETE;
    Nba97GameSceneRandomWarmupContext composed = *warmup;
    AdapterRun run{warmup->io, warmup->user, seed, adapter_progress};
    composed.io = dispatch;
    composed.user = &run;
    return nba97_game_scene_random_warmup(&composed, progress);
}
