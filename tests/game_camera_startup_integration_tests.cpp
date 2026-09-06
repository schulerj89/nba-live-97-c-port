#include "game_camera_startup_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "game camera-startup integration check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ffe00u;
constexpr std::uint32_t EntrySp = 0x807fff00u;

struct Composition {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x110000, 0);
    std::vector<std::uint8_t> ram_known =
        std::vector<std::uint8_t>(0x110000, 1);
    std::array<std::uint8_t, 0x400> stack{}, stack_known{};
    Nba97GameTextRegion regions[2] = {
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}
    };
    Nba97GameCameraStartupTickBinding binding{};
    Nba97MatchTickContext tick{};
    Nba97MatchTickProgress tick_progress{};
    std::vector<Nba97MatchTickCall> fallback_calls;
    Nba97GameCameraStartupEvent camera_event{};
    Nba97GameCameraStartupRegisters callback_registers{};
    bool refuse_camera = false;
    bool saw_camera = false;

    Composition() {
        stack.fill(0);
        stack_known.fill(1);
        binding.memory = {regions, 2};
        binding.operation_budget = 64;
        binding.io = cameraIo;
        binding.user = this;
        binding.fallback_service = fallback;
        binding.fallback_user = this;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            binding.entry_registers.gpr[i] = {
                0x31000000u + i * 0x00010101u, 0x0f};
        binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {0, 0x0f};
        binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
            EntrySp, 0x0f};
        binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x80068c34u, 0x0f};
        put(0x80021ed7u, 0xe7u, 1);
        put(0x80021ed9u, 0x12u, 1);
        put(0x80021edau, 0x34u, 1);
        put(0x800bc3d4u, 0x11111111u, 4);
        put(0x800bc3d8u, 0x22222222u, 4);
        put(0x800bc3dcu, 0x33333333u, 4);
        tick.access = access;
        tick.service = nba97_game_camera_startup_from_match_tick;
        tick.user = &binding;
        tick.operation_budget = 8;
        tick.incoming_s6 = {0, 0};
    }

    void put(std::uint32_t address, std::uint32_t value, unsigned width) {
        std::uint8_t* p = address >= Stack ?
            stack.data() + (address - Stack) : ram.data() + (address - Ram);
        for (unsigned i = 0; i < width; ++i)
            p[i] = static_cast<std::uint8_t>(value >> (8u * i));
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        const std::uint8_t* p = address >= Stack ?
            stack.data() + (address - Stack) : ram.data() + (address - Ram);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(p[i]) << (8u * i);
        return value;
    }
    int run() { return nba97_game_match_tick(&tick, &tick_progress); }

    static int access(void*, std::uint32_t, std::uint32_t, unsigned,
        unsigned, Nba97PlayerFrameValue*) {
        return NBA97_BODY_ARGUMENT;
    }
    static int fallback(void* user, const Nba97MatchTickCall* call,
        Nba97GamePeriodValue*) {
        auto& c = *static_cast<Composition*>(user);
        c.fallback_calls.push_back(*call);
        if (call->pc == 0x80068c24u && call->entry == 0x80066f88u)
            return NBA97_BODY_OK; /* Explicit prior hot-start fixture. */
        return NBA97_MATCH_TICK_SERVICE_REQUIRED;
    }
    static int cameraIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameCameraStartupEvent* event,
        Nba97GameCameraStartupRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.saw_camera = true;
        c.camera_event = *event;
        c.callback_registers = *registers;
        return c.refuse_camera ? 0 : 1;
    }
};

void natural_tick_composition() {
    Composition c;
    check(c.run() == NBA97_MATCH_TICK_SERVICE_REQUIRED);
    check(c.tick_progress.operations == 3 &&
        c.tick_progress.services == 2 &&
        c.tick_progress.stopped_pc == 0x80068c4cu &&
        c.tick_progress.stopped_entry == 0x80067468u &&
        !c.tick_progress.completed);
    check(c.fallback_calls.size() == 2 &&
        c.fallback_calls[0].pc == 0x80068c24u &&
        c.fallback_calls[0].entry == 0x80066f88u &&
        c.fallback_calls[1].pc == 0x80068c4cu &&
        c.fallback_calls[1].entry == 0x80067468u);
    check(c.binding.invocations == 1 &&
        c.binding.result == NBA97_TEXT_COMPLETE &&
        c.binding.progress.completed && c.saw_camera);
    check(c.camera_event.pc == 0x800796b8u &&
        c.callback_registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 12u &&
        c.callback_registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == 0);
    /* The explicit full-register fixture agrees with the tick's source-proven
       a0=0 call argument; the owner's branch delay then supplies a0=12. */
    check(c.get(0x801029bcu, 1) == 1 &&
        c.get(0x800dce00u, 4) == 0 &&
        c.get(0x8010607cu, 4) == 0x11111111u &&
        c.get(0x80106080u, 4) == 0x22222222u &&
        c.get(0x80106084u, 4) == 0x33333333u);
}

void nested_refusal_prefix() {
    Composition c;
    c.refuse_camera = true;
    check(c.run() == NBA97_GAME_CAMERA_STARTUP_TICK_CHILD_INCOMPLETE);
    check(c.tick_progress.operations == 2 && c.tick_progress.services == 1 &&
        c.tick_progress.stopped_pc == 0x80068c2cu &&
        c.tick_progress.stopped_entry == 0x80079664u);
    check(c.binding.result == NBA97_TEXT_IO_REFUSED &&
        c.binding.progress.operations == 9 &&
        c.binding.progress.stopped_pc == 0x800796b8u &&
        c.binding.progress.stopped_entry == 0x800799ccu &&
        !c.binding.progress.completed);
}

void explicit_entry_and_adapter_validation() {
    Composition contradictory;
    contradictory.binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {
        1, 0x0f};
    check(contradictory.run() ==
            NBA97_GAME_CAMERA_STARTUP_TICK_CHILD_INCOMPLETE &&
        contradictory.binding.result == NBA97_TEXT_ARGUMENT &&
        contradictory.binding.invocations == 0 &&
        !contradictory.saw_camera);
    check(contradictory.tick_progress.operations == 2 &&
        contradictory.tick_progress.services == 1 &&
        contradictory.tick_progress.stopped_pc == 0x80068c2cu);

    Composition unknown;
    unknown.binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask = 0;
    check(unknown.run() == NBA97_GAME_CAMERA_STARTUP_TICK_CHILD_INCOMPLETE &&
        unknown.binding.result == NBA97_TEXT_ARGUMENT &&
        unknown.binding.invocations == 0 && !unknown.saw_camera);

    Composition direct;
    Nba97MatchTickCall malformed{0x80068c2cu, 0x80079664u, {0, 0}, 0};
    check(nba97_game_camera_startup_from_match_tick(&direct.binding,
        &malformed, nullptr) ==
        NBA97_GAME_CAMERA_STARTUP_TICK_CHILD_INCOMPLETE);
    check(direct.binding.result == NBA97_TEXT_ARGUMENT &&
        direct.binding.invocations == 0);
    Nba97GamePeriodValue unexpected{};
    malformed.count = 1;
    check(nba97_game_camera_startup_from_match_tick(&direct.binding,
        &malformed, &unexpected) ==
        NBA97_GAME_CAMERA_STARTUP_TICK_CHILD_INCOMPLETE);
    malformed.args[0] = 1;
    check(nba97_game_camera_startup_from_match_tick(&direct.binding,
        &malformed, nullptr) ==
        NBA97_GAME_CAMERA_STARTUP_TICK_CHILD_INCOMPLETE &&
        direct.binding.invocations == 0);
    check(nba97_game_camera_startup_from_match_tick(nullptr, &malformed,
        nullptr) == NBA97_BODY_ARGUMENT);

    Nba97MatchTickCall prior{0x80068c24u, 0x80066f88u, {0, 0}, 0};
    check(nba97_game_camera_startup_from_match_tick(&direct.binding, &prior,
        nullptr) == NBA97_BODY_OK && direct.fallback_calls.size() == 1);
}
}

int main() {
    natural_tick_composition();
    nested_refusal_prefix();
    explicit_entry_and_adapter_validation();
    std::printf("game camera-startup integration: %u checks passed\n", checks);
}
