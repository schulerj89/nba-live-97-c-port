#include "game_scene_load_adapter.h"

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
        std::fprintf(stderr, "scene-load integration check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ffe00u;
constexpr std::uint32_t SessionEntrySp = 0x807fffd0u;
constexpr std::uint32_t SessionFrameSp = SessionEntrySp - 0x28u;
constexpr std::uint32_t SceneFrameSp = SessionFrameSp - 0x18u;

struct Composition {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x100000, 0xcd);
    std::vector<std::uint8_t> ram_known =
        std::vector<std::uint8_t>(0x100000, 1);
    std::array<std::uint8_t, 0x200> stack{};
    std::array<std::uint8_t, 0x200> stack_known{};
    Nba97GameTextRegion regions[2] = {
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}
    };
    Nba97GameMatchSessionContext session{};
    Nba97GameMatchSessionProgress session_progress{};
    Nba97GameSceneLoadProgress scene_progress{};
    std::vector<Nba97GameMatchSessionEvent> session_calls;
    std::vector<Nba97GameSceneLoadEvent> scene_calls;
    bool refuse_second_scene_child = false;
    std::size_t scene_invocations = 0;

    Composition() {
        stack.fill(0xcd);
        stack_known.fill(1);
        session = {{regions, 2}, 200, SessionEntrySp, 0x80029ae4u,
            {0xa0a0a0a0u, 0xb1b1b1b1u, 0xc2c2c2c2u}, 0x800d79c8u,
            sessionIo, this};
        put(0x8001ec94u, 0);
        put(0x80021d74u, 7);
    }
    std::uint8_t* byte(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base &&
                std::uint64_t(address - region.base) < region.size)
                return region.data + (address - region.base);
        return nullptr;
    }
    void put(std::uint32_t address, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i)
            *byte(address + i) = std::uint8_t(value >> (i * 8u));
    }
    std::uint32_t get(std::uint32_t address) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(*byte(address + i)) << (i * 8u);
        return value;
    }
    static int sceneIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameSceneLoadEvent* event,
        Nba97GameSceneLoadRegisters* registers) {
        auto& c = *static_cast<Composition*>(user);
        c.scene_calls.push_back(*event);
        if (c.refuse_second_scene_child && c.scene_calls.size() == 2)
            return 0;
        registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
            c.scene_calls.size() == 1 ? 0x10203040u : 0xcafebabeu, 0x0f};
        registers->gpr[NBA97_MATCH_INITIALIZE_T0] = {
            0x70000000u + static_cast<std::uint32_t>(c.scene_calls.size()),
            0x0f};
        return 1;
    }
    static int sessionIo(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameMatchSessionEvent* event,
        Nba97GameMatchSessionValue* value) {
        auto& c = *static_cast<Composition*>(user);
        c.session_calls.push_back(*event);
        *value = {0, 1};
        if (event->kind == NBA97_GAME_MATCH_SESSION_LOAD_SCENE) {
            ++c.scene_invocations;
            Nba97GameSceneLoadRegisters registers{};
            if (nba97_game_scene_load_registers_from_session(event,
                    &registers) != NBA97_TEXT_COMPLETE)
                return 0;
            Nba97GameSceneLoadContext context{*memory, 4, registers,
                sceneIo, &c, nullptr, 0};
            if (nba97_game_scene_load(&context, &c.scene_progress) !=
                NBA97_TEXT_COMPLETE)
                return 0;
            *value = {
                c.scene_progress.registers
                    .gpr[NBA97_MATCH_INITIALIZE_V0].word,
                static_cast<std::uint8_t>(c.scene_progress.registers
                    .gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0f)};
        }
        return 1;
    }
    int run() { return nba97_game_match_session(&session, &session_progress); }
};

void adapter_validation() {
    Nba97GameMatchSessionEvent event{};
    event.pc = 0x8002da84u;
    event.entry = 0x8002db68u;
    event.kind = NBA97_GAME_MATCH_SESSION_LOAD_SCENE;
    event.stack_pointer = SessionFrameSp;
    event.global_pointer = 0x800d79c8u;
    event.return_address = 0x8002da8cu;
    event.saved_register[0] = 1;
    event.saved_register[1] = 2;
    event.saved_register[2] = 3;
    Nba97GameSceneLoadRegisters registers{};
    check(nba97_game_scene_load_registers_from_session(&event, &registers) ==
        NBA97_TEXT_COMPLETE);
    check(registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word == 0 &&
        registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask == 0x0f &&
        registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == SessionFrameSp &&
        registers.gpr[NBA97_MATCH_INITIALIZE_GP].word == 0x800d79c8u &&
        registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x8002da8cu &&
        registers.gpr[NBA97_MATCH_INITIALIZE_S0].word == 1 &&
        registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word == 2 &&
        registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2].word == 3);
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        if (i != NBA97_MATCH_INITIALIZE_ZERO &&
            i != NBA97_MATCH_INITIALIZE_SP &&
            i != NBA97_MATCH_INITIALIZE_GP &&
            i != NBA97_MATCH_INITIALIZE_RA &&
            (i < NBA97_MATCH_INITIALIZE_S0 ||
                i > NBA97_MATCH_INITIALIZE_S0 + 2))
            check(registers.gpr[i].known_mask == 0);
    check(nba97_game_scene_load_registers_from_session(nullptr, &registers) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_scene_load_registers_from_session(&event, nullptr) ==
        NBA97_TEXT_ARGUMENT);
    event.pc++;
    check(nba97_game_scene_load_registers_from_session(&event, &registers) ==
        NBA97_TEXT_ARGUMENT);
    event.pc = 0x8002da84u;
    event.entry++;
    check(nba97_game_scene_load_registers_from_session(&event, &registers) ==
        NBA97_TEXT_ARGUMENT);
    event.entry = 0x8002db68u;
    event.kind = NBA97_GAME_MATCH_SESSION_INITIALIZE;
    check(nba97_game_scene_load_registers_from_session(&event, &registers) ==
        NBA97_TEXT_ARGUMENT);
    event.kind = NBA97_GAME_MATCH_SESSION_LOAD_SCENE;
    event.argument_count = 1;
    check(nba97_game_scene_load_registers_from_session(&event, &registers) ==
        NBA97_TEXT_ARGUMENT);
}

void natural_match_session_composition() {
    Composition c;
    check(c.run() == NBA97_TEXT_COMPLETE && c.session_progress.completed);
    check(c.scene_invocations == 1 && c.scene_progress.completed &&
        c.scene_progress.operations == 4 &&
        c.scene_progress.callbacks_completed == 2 && c.scene_calls.size() == 2);
    check(c.session_calls.size() == 23 &&
        c.session_calls[7].pc == 0x8002da84u &&
        c.session_calls[7].entry == 0x8002db68u &&
        c.session_calls[7].return_address == 0x8002da8cu);
    check(c.scene_progress.frame_stack_pointer == SceneFrameSp &&
        c.scene_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            SessionFrameSp &&
        c.scene_progress.restored_return_address.word == 0x8002da8cu &&
        c.scene_progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0xcafebabeu &&
        c.scene_progress.registers.gpr[NBA97_MATCH_INITIALIZE_T0].word ==
            0x70000002u &&
        c.get(SceneFrameSp + 0x10u) == 0x8002da8cu);
    check(c.scene_calls[0].entry == 0x800802acu &&
        c.scene_calls[1].entry == 0x80048d5cu);
}

void natural_child_failure_prefix() {
    Composition c;
    c.refuse_second_scene_child = true;
    check(c.run() == NBA97_TEXT_IO_REFUSED &&
        !c.session_progress.completed && !c.scene_progress.completed);
    check(c.scene_invocations == 1 && c.scene_calls.size() == 2 &&
        c.scene_progress.operations == 3 && c.scene_progress.stores == 1 &&
        c.scene_progress.callbacks_completed == 1 &&
        c.scene_progress.stopped_pc == 0x8002db78u &&
        c.scene_progress.stopped_entry == 0x80048d5cu);
    check(c.session_calls.size() == 8 &&
        c.session_progress.stopped_pc == 0x8002da84u &&
        c.session_progress.stopped_entry == 0x8002db68u &&
        c.get(SceneFrameSp + 0x10u) == 0x8002da8cu);
}
}

int main() {
    adapter_validation();
    natural_match_session_composition();
    natural_child_failure_prefix();
    std::printf("%u natural scene-load composition checks passed\n", checks);
}
