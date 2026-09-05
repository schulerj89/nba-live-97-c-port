#include "game_scene_random_warmup_adapter.h"

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
            "scene random warm-up integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Stack = 0x807ffc00u;
constexpr std::uint32_t SceneEntrySp = 0x807fff00u;
constexpr std::uint32_t SceneFrameSp = SceneEntrySp - 0x18u;
constexpr std::uint32_t WarmupFrameSp = SceneFrameSp - 0x18u;
constexpr std::uint32_t CallerRa = 0x8002da8cu;

struct Composition {
    std::array<std::uint8_t, 0x400> stack{};
    std::array<std::uint8_t, 0x400> stack_known{};
    Nba97GameTextRegion region{Stack, stack.data(), stack_known.data(),
        stack.size()};
    Nba97GameSceneLoadContext scene{};
    Nba97GameSceneRandomWarmupContext warmup{};
    Nba97GameSceneLoadProgress scene_progress{};
    Nba97GameSceneRandomWarmupAdapterProgress adapter_progress{};
    std::vector<Nba97GameSceneRandomWarmupEvent> warmup_calls;
    std::vector<Nba97GameSceneLoadEvent> unresolved_calls;
    Nba97GameSceneLoadRegisters second_child_registers{};
    bool refuse_second_child = false;

    Composition() {
        stack.fill(0xcd);
        stack_known.fill(1);
        scene.memory = {&region, 1};
        scene.operation_budget = 4;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            scene.registers.gpr[i] = {0x51000000u + i, 0x0f};
        scene.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        scene.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {SceneEntrySp, 0x0f};
        scene.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {CallerRa, 0x0f};
        scene.registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {0xaabbccddu, 0x0f};
        scene.io = sceneIo;
        scene.user = this;
        warmup.operation_budget = 72;
        warmup.io = warmupIo;
        warmup.user = this;
    }

    std::uint32_t get(std::uint32_t address) const {
        const auto offset = address - region.base;
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(region.data[offset + i]) << (i * 8u);
        return value;
    }

    static int warmupIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameSceneRandomWarmupEvent* event,
        Nba97GameSceneRandomWarmupRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.warmup_calls.push_back(*event);
        if (event->kind == NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                event->invocation == 1 ? 0u : 0xfacecafeu, 0x0f};
        if (event->kind == NBA97_GAME_SCENE_RANDOM_WARMUP_STEP_800935C4) {
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                0x60000000u + static_cast<std::uint32_t>(event->invocation),
                0x0f};
            registers->gpr[NBA97_MATCH_INITIALIZE_T0] = {
                static_cast<std::uint32_t>(event->invocation), 0x0f};
        }
        return 1;
    }

    static int sceneIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameSceneLoadEvent* event,
        Nba97GameSceneLoadRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.unresolved_calls.push_back(*event);
        c.second_child_registers = *registers;
        if (c.refuse_second_child)
            return 0;
        registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0xcafebabeu, 0x07};
        return 1;
    }

    int run() {
        return nba97_game_scene_load_with_random_warmup(&scene, &warmup,
            &scene_progress, &adapter_progress);
    }
};

void natural_scene_wrapper_composition() {
    Composition c;
    check(c.run() == NBA97_TEXT_COMPLETE && c.scene_progress.completed);
    check(c.adapter_progress.warmup_invocations == 1 &&
        c.adapter_progress.warmup_result == NBA97_TEXT_COMPLETE &&
        c.adapter_progress.unresolved_callbacks_completed == 1 &&
        c.adapter_progress.warmup.completed &&
        c.adapter_progress.warmup.step_calls == 64 &&
        c.adapter_progress.warmup.callbacks_completed == 68);
    check(c.scene_progress.operations == 4 &&
        c.scene_progress.callbacks_completed == 2 &&
        c.scene_progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0xcafebabeu &&
        c.scene_progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask ==
            0x07 &&
        c.scene_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            SceneEntrySp &&
        c.scene_progress.restored_return_address.word == CallerRa);
    check(c.warmup_calls.size() == 68 && c.unresolved_calls.size() == 1 &&
        c.unresolved_calls[0].kind == NBA97_GAME_SCENE_LOAD_CHILD_80048D5C &&
        c.second_child_registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            SceneFrameSp &&
        c.second_child_registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8002db80u &&
        c.second_child_registers.gpr[NBA97_MATCH_INITIALIZE_T0].word == 64 &&
        c.second_child_registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
            0xaabbccddu);
    check(c.adapter_progress.warmup.frame_stack_pointer == WarmupFrameSp &&
        c.adapter_progress.warmup.restored_return_address.word ==
            0x8002db78u &&
        c.get(SceneFrameSp + 0x10u) == CallerRa &&
        c.get(WarmupFrameSp + 0x14u) == 0x8002db78u &&
        c.get(WarmupFrameSp + 0x10u) == 0xaabbccddu);
}

void exact_nested_failure_prefixes() {
    Composition bounded;
    bounded.warmup.operation_budget = 6;
    check(bounded.run() == NBA97_TEXT_IO_REFUSED &&
        bounded.adapter_progress.warmup_result == NBA97_TEXT_LIMIT &&
        bounded.adapter_progress.warmup_invocations == 1 &&
        bounded.adapter_progress.warmup.operations == 6 &&
        bounded.adapter_progress.warmup.stopped_pc == 0x800802e0u &&
        bounded.adapter_progress.warmup.callbacks_completed == 4 &&
        bounded.adapter_progress.warmup.registers
            .gpr[NBA97_MATCH_INITIALIZE_S0].word == 63 &&
        bounded.scene_progress.operations == 2 &&
        !bounded.scene_progress.callbacks_completed &&
        bounded.scene_progress.stopped_pc == 0x8002db70u &&
        bounded.scene_progress.stopped_entry == 0x800802acu &&
        bounded.unresolved_calls.empty());

    Composition missing_provider;
    missing_provider.warmup.io = nullptr;
    check(missing_provider.run() == NBA97_TEXT_IO_REFUSED &&
        missing_provider.adapter_progress.warmup_result ==
            NBA97_TEXT_IO_REFUSED &&
        missing_provider.adapter_progress.warmup.operations == 3 &&
        missing_provider.adapter_progress.warmup.stores == 2 &&
        missing_provider.adapter_progress.warmup.stopped_entry == 0x800800f8u &&
        missing_provider.unresolved_calls.empty());

    Composition second_refusal;
    second_refusal.refuse_second_child = true;
    check(second_refusal.run() == NBA97_TEXT_IO_REFUSED &&
        second_refusal.adapter_progress.warmup_result == NBA97_TEXT_COMPLETE &&
        second_refusal.adapter_progress.warmup_invocations == 1 &&
        !second_refusal.adapter_progress.unresolved_callbacks_completed &&
        second_refusal.scene_progress.callbacks_completed == 1 &&
        second_refusal.scene_progress.stopped_entry == 0x80048d5cu &&
        second_refusal.unresolved_calls.size() == 1);
}

void adapter_arguments() {
    Composition c;
    Nba97GameSceneRandomWarmupAdapterProgress progress{};
    check(nba97_game_scene_load_with_random_warmup(nullptr, &c.warmup,
        &c.scene_progress, &progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_scene_load_with_random_warmup(&c.scene, nullptr,
        &c.scene_progress, &progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_scene_load_with_random_warmup(&c.scene, &c.warmup,
        &c.scene_progress, nullptr) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_scene_load_with_random_warmup(&c.scene, &c.warmup,
        nullptr, &progress) == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    natural_scene_wrapper_composition();
    exact_nested_failure_prefixes();
    adapter_arguments();
    std::printf("%u scene random warm-up integration checks passed\n", checks);
    return 0;
}
