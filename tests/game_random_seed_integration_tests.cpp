#include "game_random_seed_adapter.h"

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
            "game random seed integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t SeedBase = 0x800c4ae8u;
constexpr std::uint32_t StackBase = 0x807ffc00u;
constexpr std::uint32_t EntrySp = 0x807fff00u;
constexpr std::uint32_t CallerRa = 0x8002db78u;
constexpr std::array<std::uint32_t, 6> Increment{{
    0xe45a0e56u, 0x2c081893u, 0x7be6b646u,
    0x81bae76du, 0x2e647ae1u, 0xa352fbe7u}};

struct Composition {
    std::array<std::uint8_t, 0x400> stack{};
    std::array<std::uint8_t, 0x400> stack_known{};
    std::array<std::uint8_t, 24> seed_bytes{};
    std::array<std::uint8_t, 24> seed_known{};
    std::array<Nba97GameTextRegion, 2> regions{{
        {StackBase, stack.data(), stack_known.data(), stack.size()},
        {SeedBase, seed_bytes.data(), seed_known.data(), seed_bytes.size()}}};
    std::array<Nba97GameRandomSeedAccess, 6> seed_journal{};
    Nba97GameSceneRandomWarmupContext warmup{};
    Nba97GameRandomSeedContext seed{};
    Nba97GameSceneRandomWarmupProgress warmup_progress{};
    Nba97GameRandomSeedAdapterProgress adapter_progress{};
    std::vector<Nba97GameSceneRandomWarmupEvent> unresolved;

    Composition() {
        stack.fill(0xcd);
        stack_known.fill(1);
        seed_bytes.fill(0xcd);
        seed_known.fill(1);
        warmup.memory = {regions.data(), regions.size()};
        warmup.operation_budget = 72;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            warmup.registers.gpr[i] = {0x61000000u + i, 0x0f};
        warmup.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        warmup.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        warmup.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {CallerRa, 0x0f};
        warmup.registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {
            0x11223344u, 0x0f};
        warmup.io = io;
        warmup.user = this;
        seed.operation_budget = 6;
        seed.access_journal = seed_journal.data();
        seed.access_journal_capacity = seed_journal.size();
    }

    std::uint32_t get_seed(unsigned index) const {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(seed_bytes[index * 4u + i]) << (8u * i);
        return value;
    }

    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameSceneRandomWarmupEvent* event,
        Nba97GameSceneRandomWarmupRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.unresolved.push_back(*event);
        if (event->kind == NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                event->invocation == 1 ? 0u : 0xfacecafeu, 0x0f};
        if (event->kind == NBA97_GAME_SCENE_RANDOM_WARMUP_STEP_800935C4)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
                0x70000000u + static_cast<std::uint32_t>(event->invocation),
                0x0f};
        return 1;
    }

    int run() {
        return nba97_game_scene_random_warmup_with_random_seed(&warmup, &seed,
            &warmup_progress, &adapter_progress);
    }
};

void natural_warmup_composition() {
    Composition c;
    check(c.run() == NBA97_TEXT_COMPLETE && c.warmup_progress.completed &&
        c.adapter_progress.seed_result == NBA97_TEXT_COMPLETE &&
        c.adapter_progress.seed.completed &&
        c.adapter_progress.seed_invocations == 1);
    check(c.adapter_progress.seed_event.pc == 0x800802d0u &&
        c.adapter_progress.seed_event.delay_slot_pc == 0x800802d4u &&
        c.adapter_progress.seed_event.entry == 0x80093694u &&
        c.adapter_progress.seed_event.kind ==
            NBA97_GAME_SCENE_RANDOM_WARMUP_SEED_80093694 &&
        c.adapter_progress.seed_event.argument_count == 1 &&
        c.adapter_progress.seed_event.invocation == 1);
    check(c.adapter_progress.seed.registers.gpr
            [NBA97_MATCH_INITIALIZE_A0].word ==
            c.warmup_progress.seed_argument.word + 0xdfbb3b64u &&
        c.warmup_progress.seed_argument.word == 0xcafeu &&
        c.adapter_progress.seed.registers.gpr
            [NBA97_MATCH_INITIALIZE_RA].word == 0x800802d8u &&
        c.adapter_progress.seed.registers.gpr
            [NBA97_MATCH_INITIALIZE_A1].word == SeedBase);
    std::uint32_t value = 0xcafeu;
    for (unsigned i = 0; i < 6; ++i) {
        value += Increment[i];
        check(c.get_seed(i) == value && c.seed_journal[i].value == value &&
            c.seed_journal[i].address == SeedBase + i * 4u);
    }
    check(c.unresolved.size() == 67 &&
        c.adapter_progress.unresolved_callbacks_completed == 67 &&
        c.warmup_progress.callbacks_completed == 68 &&
        c.warmup_progress.startup_calls == 1 &&
        c.warmup_progress.random_calls == 2 &&
        c.warmup_progress.seed_calls == 1 &&
        c.warmup_progress.step_calls == 64 &&
        c.warmup_progress.operations == 72);
    check(c.unresolved[0].kind ==
            NBA97_GAME_SCENE_RANDOM_WARMUP_STARTUP_800800F8 &&
        c.unresolved[1].kind ==
            NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70 &&
        c.unresolved[2].kind ==
            NBA97_GAME_SCENE_RANDOM_WARMUP_RANDOM_8002AB70);
    for (std::size_t i = 3; i < c.unresolved.size(); ++i)
        check(c.unresolved[i].kind ==
            NBA97_GAME_SCENE_RANDOM_WARMUP_STEP_800935C4 &&
            c.unresolved[i].invocation == i - 2u);
}

void seed_failure_prefix_and_arguments() {
    {
        Composition rejected;
        Nba97GameSceneRandomWarmupEvent event{};
        event.pc=0x800802d0u;event.delay_slot_pc=0x800802d4u;event.entry=0x80093694u;
        event.kind=NBA97_GAME_SCENE_RANDOM_WARMUP_SEED_80093694;
        Nba97GameSceneRandomWarmupRegisters registers{};
        check(nba97_game_random_seed_from_warmup(&rejected.warmup.memory,&event,&registers,
            &rejected.seed,&rejected.adapter_progress)==NBA97_TEXT_ARGUMENT);
        check(rejected.adapter_progress.seed_invocations==0);
        event.argument_count=1;
        check(nba97_game_random_seed_from_warmup(nullptr,&event,&registers,
            &rejected.seed,&rejected.adapter_progress)==NBA97_TEXT_ARGUMENT);
    }
    Composition bounded;
    bounded.seed.operation_budget = 3;
    check(bounded.run() == NBA97_TEXT_IO_REFUSED &&
        bounded.adapter_progress.seed_result == NBA97_TEXT_LIMIT &&
        bounded.adapter_progress.seed_invocations == 1 &&
        bounded.adapter_progress.seed.stores == 3 &&
        bounded.adapter_progress.seed.stopped_pc == 0x800936f8u &&
        bounded.adapter_progress.unresolved_callbacks_completed == 3 &&
        bounded.warmup_progress.operations == 6 &&
        bounded.warmup_progress.callbacks_completed == 3 &&
        bounded.warmup_progress.stopped_pc == 0x800802d0u &&
        bounded.warmup_progress.stopped_entry == 0x80093694u);

    Composition missing;
    missing.warmup.memory.count = 1;
    check(missing.run() == NBA97_TEXT_IO_REFUSED &&
        missing.adapter_progress.seed_result == NBA97_TEXT_RESOURCE &&
        !missing.adapter_progress.seed.stores &&
        missing.adapter_progress.seed.stopped_address == SeedBase);

    Composition c;
    check(nba97_game_scene_random_warmup_with_random_seed(nullptr, &c.seed,
        &c.warmup_progress, &c.adapter_progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_scene_random_warmup_with_random_seed(&c.warmup, nullptr,
        &c.warmup_progress, &c.adapter_progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_scene_random_warmup_with_random_seed(&c.warmup, &c.seed,
        nullptr, &c.adapter_progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_scene_random_warmup_with_random_seed(&c.warmup, &c.seed,
        &c.warmup_progress, nullptr) == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    natural_warmup_composition();
    seed_failure_prefix_and_arguments();
    std::printf("%u game random seed integration checks passed\n", checks);
    return 0;
}
