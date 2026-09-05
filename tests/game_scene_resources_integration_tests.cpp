#include "game_scene_resources_adapter.h"

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
    if (!condition)
        throw std::runtime_error("scene-resources integration check failed at line " +
            std::to_string(line));
}
#define check(condition) check_at((condition), __LINE__)

struct Composition {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xcd);
    std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
    Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
    Nba97GameSceneStartupContext startup{};
    Nba97GameSceneStartupProgress startup_progress{};
    Nba97GameSceneResourcesBinding resources{};
    std::size_t startup_calls{};
    std::size_t resource_calls{};
    std::vector<Nba97GameResourceLoaderEvent> loader_attempts;
    std::size_t refuse_resource = static_cast<std::size_t>(-1);
    bool partial_loader_inputs{};
    bool null_first_attempt{};
    bool refused_loader_attempt{};
    bool maximum_loader_path{};
    Nba97GameSceneStartupEvent resource_parent_event{};
    Nba97GameSceneStartupRegisters resource_parent_registers{};

    Composition() {
        startup.memory = {&region, 1};
        startup.operation_budget = 10000;
        startup.io = startup_io;
        startup.user = this;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            startup.registers.gpr[i] = {0x22000000u + i, 0x0f};
        startup.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        startup.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 0x0f};
        startup.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x82345678u, 0x0f};
        resources.operation_budget = 10000;
        resources.io = resource_io;
        resources.user = this;
        resources.resource_loader_operation_budget = 7;
        resources.resource_loader_io = loader_io;
        resources.resource_loader_user = this;
        seed();
    }
    std::size_t offset(std::uint32_t address) const {
        check(address >= Base && address - Base < bytes.size());
        return address - Base;
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width = 4) {
        const auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
            known[at + i] = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width = 4) const {
        const auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= static_cast<std::uint32_t>(bytes[at + i]) << (8u * i);
        return value;
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
        put(0x8001ede8u, 1u);
        put(0x80021d74u, 2u);
        put(0x80021d78u, 3u);
        put(0x800b7394u + 2u * 4u, 0x80027000u);
        put(0x800b741cu + 3u * 4u, 0x80027100u);
        put(0x800faba0u, 0x9100aba0u);
        put(0x80102918u, 0x91102918u);
    }
    static int resource_io(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameSceneResourcesEvent* event,
        Nba97GameSceneResourcesRegisters* registers) {
        auto& composition = *static_cast<Composition*>(opaque);
        const auto index = composition.resource_calls++;
        if (index == composition.refuse_resource)
            return 0;
        if (composition.partial_loader_inputs && event->pc == 0x80052c68u)
            registers->gpr[NBA97_MATCH_INITIALIZE_GP].known_mask = 0x07;
        if (composition.maximum_loader_path && event->pc == 0x80052fe8u)
            composition.put(0x800eb678u, 1u);
        if (composition.maximum_loader_path && event->pc == 0x800530a4u)
            composition.put(0x800eb678u, 0u);
        const auto a0 = registers->gpr[NBA97_MATCH_INITIALIZE_A0].word;
        const auto a1 = registers->gpr[NBA97_MATCH_INITIALIZE_A1].word;
        std::uint32_t result = 0xb0000000u ^ event->pc;
        if (event->entry == 0x80029bfcu)
            result = 0x90000000u ^ a0;
        else if (event->entry == 0x800a3fecu)
            result = a0 + a1 * 0x100u + (event->pc & 0xffu);
        else if (event->entry == 0x80090160u)
            result = 0xa0000000u ^ a0 ^ a1;
        registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {result, 0x0f};
        return 1;
    }
    static int loader_io(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameResourceLoaderEvent* event,
        Nba97GameResourceLoaderValue* value) {
        auto& composition = *static_cast<Composition*>(opaque);
        composition.loader_attempts.push_back(*event);
        if (composition.refused_loader_attempt)
            return 0;
        value->word = 0x90000000u ^ event->argument[0];
        value->known = 1;
        if (composition.null_first_attempt &&
            composition.loader_attempts.size() == 1u)
            value->word = 0;
        return 1;
    }
    static int startup_io(void* opaque, const Nba97GameTextMemory* memory,
        const Nba97GameSceneStartupEvent* event,
        Nba97GameSceneStartupRegisters* registers) {
        auto& composition = *static_cast<Composition*>(opaque);
        ++composition.startup_calls;
        if (event->kind == NBA97_GAME_SCENE_STARTUP_CONTROLLER_8008F224) {
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0f};
            return 1;
        }
        if (event->kind == NBA97_GAME_SCENE_STARTUP_CHILD_80052C20) {
            composition.resource_parent_event = *event;
            composition.resource_parent_registers = *registers;
            return nba97_game_scene_resources_from_scene_startup(
                &composition.resources, memory, event, registers);
        }
        if (event->kind == NBA97_GAME_SCENE_STARTUP_CHILD_80056944)
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x13579bdfu, 0x0f};
        return 1;
    }
};

void natural_scene_startup_composition() {
    Composition composition;
    check(nba97_game_scene_startup(&composition.startup,
        &composition.startup_progress) == NBA97_TEXT_COMPLETE);
    check(composition.startup_progress.completed &&
        composition.startup_progress.callbacks_completed == 19 &&
        composition.startup_calls == 19);
    check(composition.resources.invocations == 1 &&
        composition.resources.result == NBA97_TEXT_COMPLETE &&
        composition.resources.progress.completed &&
        composition.resources.progress.operations == 182 &&
        composition.resources.progress.callbacks_completed == 72 &&
        composition.resource_calls == 66 &&
        composition.resources.unresolved_callbacks_completed == 66 &&
        composition.resources.resource_loader_invocations == 6 &&
        composition.loader_attempts.size() == 6);
    for (unsigned i = 0; i < 6; ++i)
        check(composition.resources.resource_loader_result[i] == NBA97_TEXT_COMPLETE &&
            composition.resources.resource_loader[i].completed &&
            composition.resources.resource_loader[i].operations == 7 &&
            composition.resources.resource_loader[i].load_attempts == 1);
    check(composition.loader_attempts[0].argument[0] == 0x8002639cu &&
        composition.loader_attempts[0].argument[1] == 0x20u &&
        composition.loader_attempts[1].argument[0] == 0x80027000u &&
        composition.loader_attempts[2].argument[0] == 0x80027100u &&
        composition.loader_attempts[3].argument[0] == 0x800263acu &&
        composition.loader_attempts[4].argument[0] == 0x800263bcu &&
        composition.loader_attempts[5].argument[0] == 0x80026404u);
    check(composition.resource_parent_event.pc == 0x80048e94u &&
        composition.resource_parent_event.delay_slot_pc == 0x80048e98u &&
        composition.resource_parent_event.entry == 0x80052c20u &&
        composition.resource_parent_event.kind ==
            NBA97_GAME_SCENE_STARTUP_CHILD_80052C20);
    check(composition.resource_parent_registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        0x80048e9cu);
    check(composition.get(0x800b72dcu) == 1u &&
        composition.get(0x800fac20u) == 0xfffffffdu &&
        composition.get(0x800f0edcu) != 0xcdcdcdcdu &&
        composition.get(0x80103f44u) != 0xcdcdcdcdu);
    check(composition.get(0x8010424cu) == static_cast<std::uint32_t>(-20) &&
        composition.get(0x8010427cu) == 20u &&
        composition.get(0x800fee90u) == 300u &&
        composition.get(0x80021498u, 2) == 1u);
    check(composition.startup_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == Stack &&
        composition.startup_progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x82345678u &&
        composition.startup_progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 0x13579bdfu);
}

void nested_refusal_and_budget_prefix() {
    Composition refused;
    refused.refuse_resource = 5;
    check(nba97_game_scene_startup(&refused.startup,
        &refused.startup_progress) == NBA97_TEXT_IO_REFUSED);
    check(!refused.startup_progress.completed &&
        refused.startup_progress.callbacks_completed == 9 &&
        refused.startup_progress.stopped_pc == 0x80048e94u &&
        refused.startup_progress.stopped_entry == 0x80052c20u);
    check(refused.resources.result == NBA97_TEXT_IO_REFUSED &&
        refused.resources.progress.callbacks_completed == 8 &&
        refused.resources.progress.stopped_pc == 0x80052d90u &&
        refused.resources.progress.stopped_entry == 0x800a3fecu);

    Composition limited;
    limited.resources.operation_budget = 11;
    check(nba97_game_scene_startup(&limited.startup,
        &limited.startup_progress) == NBA97_TEXT_IO_REFUSED);
    check(limited.resources.result == NBA97_TEXT_LIMIT &&
        limited.resources.progress.operations == 11 &&
        limited.resources.progress.stopped_pc == 0x80052c8cu &&
        limited.resources.progress.stopped_entry == 0x80029bfcu);
    check(limited.get(0x800b72dcu) == 1u &&
        limited.get(0x800fb820u) == 0u &&
        limited.get(0x800fac20u) == 0xfffffffdu);

    Composition loader_limited;
    loader_limited.resources.resource_loader_operation_budget = 0;
    check(nba97_game_scene_startup(&loader_limited.startup,
        &loader_limited.startup_progress) == NBA97_TEXT_IO_REFUSED);
    check(loader_limited.resources.result == NBA97_TEXT_IO_REFUSED &&
        loader_limited.resources.progress.stopped_pc == 0x80052c8cu &&
        loader_limited.resources.progress.stopped_entry == 0x80029bfcu &&
        loader_limited.resources.resource_loader_invocations == 1 &&
        loader_limited.resources.resource_loader_result[0] == NBA97_TEXT_LIMIT &&
        loader_limited.resources.resource_loader[0].operations == 0 &&
        loader_limited.resources.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Stack - 0x28u - 0x20u - 0x20u);

    Composition loader_refused;
    loader_refused.refused_loader_attempt = true;
    check(nba97_game_scene_startup(&loader_refused.startup,
        &loader_refused.startup_progress) == NBA97_TEXT_IO_REFUSED);
    check(loader_refused.resources.resource_loader_result[0] == NBA97_TEXT_IO_REFUSED &&
        loader_refused.resources.resource_loader[0].stopped_pc == 0x80029c18u &&
        loader_refused.resources.resource_loader[0].stopped_entry == 0x800941c8u &&
        loader_refused.resources.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80029c20u &&
        loader_refused.resources.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask == 0);

    Composition retry;
    retry.null_first_attempt = true;
    retry.resources.resource_loader_operation_budget = 10;
    check(nba97_game_scene_startup(&retry.startup,
        &retry.startup_progress) == NBA97_TEXT_COMPLETE &&
        retry.resources.resource_loader[0].load_attempts == 2 &&
        retry.resources.resource_loader[0].null_results == 1 &&
        retry.loader_attempts.size() == 7);

    Composition fallback;
    fallback.partial_loader_inputs = true;
    check(nba97_game_scene_startup(&fallback.startup,
        &fallback.startup_progress) == NBA97_TEXT_COMPLETE &&
        fallback.resources.resource_loader_invocations == 0 &&
        fallback.resources.unresolved_callbacks_completed == 72 &&
        fallback.loader_attempts.empty() &&
        fallback.resources.progress.registers.gpr[NBA97_MATCH_INITIALIZE_GP].known_mask == 0x07);

    Composition maximum;
    maximum.maximum_loader_path = true;
    check(nba97_game_scene_startup(&maximum.startup,
        &maximum.startup_progress) == NBA97_TEXT_COMPLETE &&
        maximum.resources.resource_loader_invocations ==
            NBA97_GAME_SCENE_RESOURCES_LOADER_CALLS_MAX &&
        maximum.loader_attempts.size() ==
            NBA97_GAME_SCENE_RESOURCES_LOADER_CALLS_MAX &&
        maximum.get(0x800f0ed4u) != 0xcdcdcdcdu &&
        maximum.get(0x80103f44u) != 0xcdcdcdcdu);
}

void adapter_guards() {
    Composition composition;
    Nba97GameSceneStartupEvent event{};
    event.pc = 0x80048e94u;
    event.delay_slot_pc = 0x80048e98u;
    event.entry = 0x80052c20u;
    event.kind = NBA97_GAME_SCENE_STARTUP_CHILD_80052C20;
    auto registers = composition.startup.registers;
    const auto before = registers;
    event.pc ^= 4u;
    check(!nba97_game_scene_resources_from_scene_startup(
        &composition.resources, &composition.startup.memory, &event, &registers));
    check(composition.resources.result == NBA97_TEXT_ARGUMENT &&
        composition.resources.invocations == 0);
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
        check(registers.gpr[i].word == before.gpr[i].word &&
            registers.gpr[i].known_mask == before.gpr[i].known_mask);

    event.pc ^= 4u;
    composition.resources.access_journal = nullptr;
    composition.resources.access_journal_capacity = 1;
    check(!nba97_game_scene_resources_from_scene_startup(
        &composition.resources, &composition.startup.memory, &event, &registers));
    check(composition.resources.result == NBA97_TEXT_ARGUMENT &&
        composition.resources.invocations == 0);
}
}

int main() {
    try {
        natural_scene_startup_composition();
        nested_refusal_and_budget_prefix();
        adapter_guards();
        std::cout << checks << " scene-resources natural-startup checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << " after " << checks << " checks\n";
        return 1;
    }
}
