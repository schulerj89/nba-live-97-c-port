#include "game_scene_startup_adapter.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::uint32_t Base = 0x80000000u;
constexpr std::size_t Size = 0x120000u;
constexpr std::uint32_t Stack = 0x8010ff80u;
std::size_t checks;
void check_at(bool condition, int line) {
    ++checks;
    if (!condition) throw std::runtime_error(
        "scene-startup integration check failed at line " + std::to_string(line));
}
#define check(condition) check_at((condition), __LINE__)

struct Composition {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xcd);
    std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
    Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
    Nba97GameSceneStartupBinding binding{};
    Nba97GameSceneLoadContext wrapper{};
    Nba97GameSceneLoadProgress wrapper_progress{};
    std::size_t nested_calls{};
    std::size_t refuse_nested = static_cast<std::size_t>(-1);

    Composition() {
        wrapper.memory = {&region, 1};
        wrapper.operation_budget = 4;
        wrapper.io = wrapper_io;
        wrapper.user = this;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            wrapper.registers.gpr[i] = {0x22000000u + i, 0x0f};
        wrapper.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        wrapper.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 0x0f};
        wrapper.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x82345678u, 0x0f};
        binding.operation_budget = 10000;
        binding.io = nested_io;
        binding.user = this;
        seed();
    }
    std::size_t offset(std::uint32_t address) const {
        check(address >= Base && address - Base < bytes.size());
        return address - Base;
    }
    void put(std::uint32_t address, std::uint32_t word, unsigned width = 4) {
        const auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(word >> (8u * i));
            known[at + i] = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width = 4) const {
        const auto at = offset(address);
        std::uint32_t result = 0;
        for (unsigned i = 0; i < width; ++i)
            result |= static_cast<std::uint32_t>(bytes[at + i]) << (8u * i);
        return result;
    }
    void seed() {
        for (unsigned i = 0; i < 12; ++i) {
            const auto home = 0x80030000u + i * 4u;
            const auto away = 0x80030100u + i * 4u;
            put(0x80020b8cu + i * 4u, home);
            put(0x80020bbcu + i * 4u, away);
            put(home, static_cast<std::uint16_t>(-20 - static_cast<int>(i)), 2);
            put(away, static_cast<std::uint16_t>(20 + i), 2);
        }
        put(0x800fc650u, 0x80040000u);
        for (unsigned i = 0; i < 10; ++i) {
            const auto entity = 0x80041000u + i * 0x40u;
            const auto roster = 0x80042000u + i * 4u;
            put(0x80040000u + i * 4u, entity);
            put(entity + 0x20u, roster);
            put(roster, static_cast<std::uint16_t>(300 + i), 2);
        }
        put(0x800b729cu, 0x800aa000u);
        put(0x8001ede8u, 1);
    }
    static int nested_io(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameSceneStartupEvent* event,
        Nba97GameSceneStartupRegisters* registers) {
        auto& composition = *static_cast<Composition*>(opaque);
        const auto index = composition.nested_calls++;
        if (index == composition.refuse_nested) return 0;
        if (event->kind == NBA97_GAME_SCENE_STARTUP_CONTROLLER_8008F224)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0f};
        if (event->kind == NBA97_GAME_SCENE_STARTUP_CHILD_80056944)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x13579bdfu, 0x0f};
        return 1;
    }
    static int wrapper_io(void* opaque, const Nba97GameTextMemory* memory,
        const Nba97GameSceneLoadEvent* event,
        Nba97GameSceneLoadRegisters* registers) {
        auto& composition = *static_cast<Composition*>(opaque);
        if (event->kind == NBA97_GAME_SCENE_LOAD_CHILD_800802AC) {
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x2468ace0u, 0x0f};
            registers->gpr[NBA97_MATCH_INITIALIZE_T0] = {0x88776655u, 0x0f};
            return 1;
        }
        return nba97_game_scene_startup_from_scene_load(
            &composition.binding, memory, event, registers);
    }
};

void natural_wrapper_composition() {
    Composition composition;
    check(nba97_game_scene_load(&composition.wrapper,
        &composition.wrapper_progress) == NBA97_TEXT_COMPLETE);
    check(composition.wrapper_progress.completed &&
        composition.wrapper_progress.callbacks_completed == 2);
    check(composition.binding.invocations == 1 &&
        composition.binding.result == NBA97_TEXT_COMPLETE);
    check(composition.binding.progress.completed && composition.nested_calls == 19);
    check(composition.wrapper_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == Stack);
    check(composition.wrapper_progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x82345678u);
    check(composition.wrapper_progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 0x13579bdfu);
    check(composition.wrapper_progress.registers.gpr[NBA97_MATCH_INITIALIZE_T0].word == 0x88776655u);
    check(composition.get(0x8010424cu) == static_cast<std::uint32_t>(-20));
    check(composition.get(0x8010427cu) == 20u);
    check(composition.get(0x800fee90u) == 300u);
    check(composition.get(0x80021498u, 2) == 1u);
    check(composition.get(0x8001ede8u) == 1u);
}

void nested_refusal_retains_prefix() {
    Composition composition;
    composition.refuse_nested = 3;
    check(nba97_game_scene_load(&composition.wrapper,
        &composition.wrapper_progress) == NBA97_TEXT_IO_REFUSED);
    check(!composition.wrapper_progress.completed &&
        composition.wrapper_progress.callbacks_completed == 1);
    check(composition.wrapper_progress.stopped_pc == 0x8002db78u &&
        composition.wrapper_progress.stopped_entry == 0x80048d5cu);
    check(composition.binding.result == NBA97_TEXT_IO_REFUSED &&
        composition.binding.progress.callbacks_completed == 3);
    check(composition.binding.progress.stopped_pc == 0x80048dacu);
    check(composition.get(0x800eb684u) == 0);
    check(composition.get(0x800faba4u) == 0);
    check(composition.wrapper_progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        composition.binding.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word);
}

void adapter_event_guards() {
    Composition composition;
    auto event = Nba97GameSceneLoadEvent{};
    event.pc = 0x8002db78u;
    event.delay_slot_pc = 0x8002db7cu;
    event.entry = 0x80048d5cu;
    event.kind = NBA97_GAME_SCENE_LOAD_CHILD_80048D5C;
    auto registers = composition.wrapper.registers;
    auto before = registers;
    event.pc ^= 4u;
    check(!nba97_game_scene_startup_from_scene_load(&composition.binding,
        &composition.wrapper.memory, &event, &registers));
    check(composition.binding.result == NBA97_TEXT_ARGUMENT &&
        composition.binding.invocations == 0);
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        check(registers.gpr[i].word == before.gpr[i].word &&
            registers.gpr[i].known_mask == before.gpr[i].known_mask);
}
}

int main() {
    try {
        natural_wrapper_composition();
        nested_refusal_retains_prefix();
        adapter_event_guards();
        std::cout << checks << " scene-startup natural-wrapper checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << " after " << checks << " checks\n";
        return 1;
    }
}
