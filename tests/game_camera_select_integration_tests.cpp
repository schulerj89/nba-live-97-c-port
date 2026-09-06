#include "game_camera_select_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "game camera-select integration check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x8010f000u;

struct Composition {
    std::vector<std::uint8_t> ram =
        std::vector<std::uint8_t>(0x110000, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000, 1);
    Nba97GameTextRegion region{Ram, ram.data(), known.data(), ram.size()};
    Nba97GameCameraStartupContext startup{};
    Nba97GameCameraStartupProgress startup_progress{};
    Nba97GameCameraSelectStartupBinding binding{};
    std::array<Nba97GameCameraStartupAccess, 128> startup_journal{};
    std::array<Nba97GameCameraSelectAccess, 256> select_journal{};
    std::vector<Nba97GameCameraSelectEvent> select_calls;
    Nba97GameCameraStartupRegisters initial{};
    std::uint32_t refuse_pc = 0;

    Composition() {
        startup.memory = {&region, 1};
        startup.operation_budget = 128;
        startup.io = nba97_game_camera_select_from_camera_startup;
        startup.user = &binding;
        startup.access_journal = startup_journal.data();
        startup.access_journal_capacity = startup_journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            startup.registers.gpr[i] = {
                0x33000000u + i * 0x00010101u, 0x0f};
        startup.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        startup.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        startup.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x80068c34u, 0x0f};
        initial = startup.registers;
        binding.operation_budget = 256;
        binding.io = selectIo;
        binding.user = this;
        binding.access_journal = select_journal.data();
        binding.access_journal_capacity = select_journal.size();
        put(0x80021ed7u, 8, 1);
        put(0x80021ed8u, 0x5a, 1);
        put(0x80021ed9u, 0x12, 1);
        put(0x80021edau, 0x34, 1);
        put(0x800bc3d4u, 0x11111111u, 4);
        put(0x800bc3d8u, 0x22222222u, 4);
        put(0x800bc3dcu, 0x33333333u, 4);
        for (unsigned i = 0; i < 6; ++i)
            put(0x80109aa8u + i * 4u, 0x71000000u + i, 4);
        put(0x800bc268u + 8u * 4u, 0x88888888u, 4);
        put(0x800bc268u + 12u * 4u, 0xccccccccu, 4);
    }

    void put(std::uint32_t address, std::uint32_t value, unsigned width) {
        auto offset = static_cast<std::size_t>(address - Ram);
        for (unsigned i = 0; i < width; ++i)
            ram[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
    }
    std::uint32_t get(std::uint32_t address, unsigned width = 4) const {
        auto offset = static_cast<std::size_t>(address - Ram);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(ram[offset + i]) << (i * 8u);
        return value;
    }
    int run(std::uint32_t startup_a0) {
        startup.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {
            startup_a0, 0x0f};
        initial = startup.registers;
        return nba97_game_camera_startup(&startup, &startup_progress);
    }
    static int selectIo(void* user, const Nba97GameTextMemory*,
        const Nba97GameCameraSelectEvent* event,
        Nba97GameCameraSelectRegisters*) {
        auto& c = *static_cast<Composition*>(user);
        c.select_calls.push_back(*event);
        return event->pc == c.refuse_pc ? 0 : 1;
    }
};

void expectRegisters(const Nba97GameCameraSelectRegisters& actual,
    const Nba97GameCameraSelectRegisters& expected) {
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        check(actual.gpr[i].word == expected.gpr[i].word);
        check(actual.gpr[i].known_mask == expected.gpr[i].known_mask);
    }
}

void natural_b8_composition() {
    Composition c;
    check(c.run(0) == NBA97_TEXT_COMPLETE);
    check(c.startup_progress.completed && c.binding.invocations == 1 &&
        c.binding.result == NBA97_TEXT_COMPLETE && c.binding.progress.completed);
    check(c.binding.caller_pc == 0x800796b8u);

    auto expected = c.initial;
    expected.gpr[NBA97_MATCH_INITIALIZE_V0] = {1, 0x0f};
    expected.gpr[NBA97_MATCH_INITIALIZE_V1] = {0x12, 0x0f};
    expected.gpr[NBA97_MATCH_INITIALIZE_A0] = {12, 0x0f};
    expected.gpr[NBA97_MATCH_INITIALIZE_A1] = {0, 0x0f};
    expected.gpr[NBA97_MATCH_INITIALIZE_AT] = {0x80100000u, 0x0f};
    expected.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp - 0x18u, 0x0f};
    expected.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x800796c0u, 0x0f};
    expectRegisters(c.binding.entry_registers, expected);

    check(c.select_calls.size() == 4 &&
        c.select_calls[0].pc == 0x80079ab4u &&
        c.select_calls[1].pc == 0x80079b7cu &&
        c.select_calls[2].pc == 0x80079c8cu &&
        c.select_calls[3].pc == 0x80079d0cu);
    check(c.get(0x800fc99cu) == 12 &&
        c.get(0x800fc9d0u) == 0xccccccccu &&
        c.get(0x8010607cu) == 0x11111111u &&
        c.get(0x80106080u) == 0x22222222u &&
        c.get(0x80106084u) == 0x33333333u);
    check(c.startup_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        EntrySp);
}

void natural_e4_composition() {
    Composition c;
    check(c.run(1) == NBA97_TEXT_COMPLETE);
    check(c.binding.caller_pc == 0x800796e4u &&
        c.binding.result == NBA97_TEXT_COMPLETE);

    auto expected = c.initial;
    expected.gpr[NBA97_MATCH_INITIALIZE_V0] = {1, 0x0f};
    expected.gpr[NBA97_MATCH_INITIALIZE_V1] = {0x12, 0x0f};
    expected.gpr[NBA97_MATCH_INITIALIZE_A0] = {8, 0x0f};
    expected.gpr[NBA97_MATCH_INITIALIZE_A1] = {0, 0x0f};
    expected.gpr[NBA97_MATCH_INITIALIZE_AT] = {0x80100000u, 0x0f};
    expected.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp - 0x18u, 0x0f};
    expected.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x800796ecu, 0x0f};
    expectRegisters(c.binding.entry_registers, expected);
    check(c.select_calls.size() == 4 &&
        c.select_calls[0].pc == 0x80079aa4u &&
        c.get(0x800fc99cu) == 8 &&
        c.get(0x800fc9d0u) == 0x88888888u);
}

void nested_refusal_and_budget_prefix() {
    Composition refused;
    refused.refuse_pc = 0x80079b7cu;
    check(refused.run(1) == NBA97_TEXT_IO_REFUSED);
    check(!refused.startup_progress.completed &&
        refused.startup_progress.stopped_pc == 0x800796e4u &&
        refused.binding.result == NBA97_TEXT_IO_REFUSED &&
        refused.binding.progress.stopped_pc == 0x80079b7cu &&
        refused.get(0x800fc99cu) == 8);

    Composition bounded;
    bounded.binding.operation_budget = 0;
    check(bounded.run(1) == NBA97_TEXT_IO_REFUSED);
    check(bounded.binding.result == NBA97_TEXT_LIMIT &&
        bounded.binding.progress.operations == 0 &&
        bounded.binding.progress.stopped_pc == 0x800799d0u);
    /* The adapter copies the exact child prefix back to the natural caller. */
    check(bounded.startup_progress.registers.gpr[
        NBA97_MATCH_INITIALIZE_SP].word == EntrySp - 0x70u);
}

void adapter_validation() {
    Composition c;
    Nba97GameCameraStartupEvent event{};
    event.pc = 0x800796b8u;
    event.delay_slot_pc = 0x800796bcu;
    event.entry = 0x800799ccu;
    event.kind = NBA97_GAME_CAMERA_STARTUP_CHILD_800799CC;
    event.argument_count = 2;
    auto registers = c.startup.registers;
    check(nba97_game_camera_select_from_camera_startup(nullptr,
        &c.startup.memory, &event, &registers) == 0);
    event.pc = 0x800796bcu;
    check(nba97_game_camera_select_from_camera_startup(&c.binding,
        &c.startup.memory, &event, &registers) == 0);
    check(c.binding.result == NBA97_TEXT_ARGUMENT && c.binding.invocations == 0);
    event.pc = 0x800796b8u;
    event.delay_slot_pc = 0;
    check(nba97_game_camera_select_from_camera_startup(&c.binding,
        &c.startup.memory, &event, &registers) == 0);
    check(nba97_game_camera_select_from_camera_startup(&c.binding,
        nullptr, &event, &registers) == 0);
}
}

int main() {
    natural_b8_composition();
    natural_e4_composition();
    nested_refusal_and_budget_prefix();
    adapter_validation();
    std::printf("game camera-select integration: %u checks passed\n", checks);
}
