#include "game_scene_random_warmup_adapter.h"

#include <cstring>

namespace {
struct AdapterRun {
    Nba97GameSceneLoadIo unresolved_io;
    void* unresolved_user;
    const Nba97GameSceneRandomWarmupContext* warmup;
    Nba97GameSceneRandomWarmupAdapterProgress* out;
};

int dispatch(void* user, const Nba97GameTextMemory* memory,
    const Nba97GameSceneLoadEvent* event,
    Nba97GameSceneLoadRegisters* registers) {
    auto& run = *static_cast<AdapterRun*>(user);
    if (event->kind == NBA97_GAME_SCENE_LOAD_CHILD_800802AC &&
        event->pc == UINT32_C(0x8002db70) &&
        event->entry == UINT32_C(0x800802ac)) {
        ++run.out->warmup_invocations;
        run.out->warmup_event = *event;
        Nba97GameSceneRandomWarmupContext context = *run.warmup;
        context.memory = *memory;
        context.registers = *registers;
        run.out->warmup_result = nba97_game_scene_random_warmup(
            &context, &run.out->warmup);
        *registers = run.out->warmup.registers;
        return run.out->warmup_result == NBA97_TEXT_COMPLETE;
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

int nba97_game_scene_load_with_random_warmup(
    const Nba97GameSceneLoadContext* scene,
    const Nba97GameSceneRandomWarmupContext* warmup,
    Nba97GameSceneLoadProgress* progress,
    Nba97GameSceneRandomWarmupAdapterProgress* adapter_progress) {
    if (!scene || !warmup || !adapter_progress)
        return NBA97_TEXT_ARGUMENT;
    std::memset(adapter_progress, 0, sizeof *adapter_progress);
    adapter_progress->warmup_result = NBA97_TEXT_COMPLETE;
    Nba97GameSceneLoadContext composed = *scene;
    AdapterRun run{scene->io, scene->user, warmup, adapter_progress};
    composed.io = dispatch;
    composed.user = &run;
    return nba97_game_scene_load(&composed, progress);
}
