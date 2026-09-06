#include "recovered/game_camera_startup.h"

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
        std::fprintf(stderr, "game camera-startup check %u failed\n", checks);
        std::exit(1);
    }
}

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807ffe00u;
constexpr std::uint32_t EntrySp = 0x807fff00u;

struct Fixture {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x110000, 0);
    std::vector<std::uint8_t> ram_known =
        std::vector<std::uint8_t>(0x110000, 1);
    std::array<std::uint8_t, 0x400> stack{};
    std::array<std::uint8_t, 0x400> stack_known{};
    Nba97GameTextRegion regions[2] = {
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}
    };
    std::array<Nba97GameCameraStartupAccess, 64> journal{};
    Nba97GameCameraStartupContext context{};
    Nba97GameCameraStartupProgress progress{};
    std::vector<Nba97GameCameraStartupEvent> calls;
    std::vector<Nba97GameCameraStartupRegisters> call_registers;
    int callback_result = 1;
    bool mutate = false;
    bool malformed = false;

    explicit Fixture(std::uint32_t a0 = 0) {
        stack.fill(0);
        stack_known.fill(1);
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {0x11000000u + i * 0x01010101u,
                0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {a0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x12345678u, 0x0f};
        context.memory = {regions, 2};
        context.operation_budget = journal.size();
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        put(0x80021ed7u, 0xa5u, 1);
        put(0x80021ed9u, 0x34u, 1);
        put(0x80021edau, 0x78u, 1);
        put(0x800bc3d4u, 0x10203040u, 4);
        put(0x800bc3d8u, 0x50607080u, 4);
        put(0x800bc3dcu, 0x90a0b0c0u, 4);
    }

    std::uint8_t* data(std::uint32_t address) {
        if (address >= Ram && address < Ram + ram.size())
            return ram.data() + (address - Ram);
        return stack.data() + (address - Stack);
    }
    std::uint8_t* known(std::uint32_t address) {
        if (address >= Ram && address < Ram + ram_known.size())
            return ram_known.data() + (address - Ram);
        return stack_known.data() + (address - Stack);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width,
        std::uint8_t known_mask = 0x0f) {
        for (unsigned i = 0; i < width; ++i) {
            data(address)[i] = static_cast<std::uint8_t>(value >> (8u * i));
            known(address)[i] = static_cast<std::uint8_t>(
                (known_mask >> i) & 1u);
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(data(address)[i]) << (8u * i);
        return value;
    }
    int run() { return nba97_game_camera_startup(&context, &progress); }

    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameCameraStartupEvent* event,
        Nba97GameCameraStartupRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        f.call_registers.push_back(*registers);
        if (f.mutate) {
            registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {
                0xcafef00du, 0x0f};
            registers->gpr[NBA97_MATCH_INITIALIZE_A2] = {
                0x01020304u, 0x05};
            registers->gpr[NBA97_MATCH_INITIALIZE_T0] = {
                0x55667788u, 0x0a};
            registers->gpr[NBA97_MATCH_INITIALIZE_GP] = {
                0x800d1234u, 0x0f};
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
                EntrySp + 0x80u, 0x0f};
            f.put(EntrySp + 0x80u + 0x10u, 0xdeadbeefu, 4);
            f.put(0x800bc3d4u, 0x11112222u, 4);
            f.put(0x800bc3d8u, 0x33334444u, 4);
            f.put(0x800bc3dcu, 0x55556666u, 4);
            f.put(0x80104744u, 0x77778888u, 4);
        }
        if (f.malformed)
            registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
        return f.callback_result;
    }
};

void check_event(const Fixture& f, std::uint32_t pc, std::uint32_t a0,
    std::uint8_t a0_mask = 0x0f) {
    check(f.calls.size() == 1 && f.call_registers.size() == 1);
    const auto& event = f.calls[0];
    const auto& r = f.call_registers[0];
    check(event.pc == pc && event.delay_slot_pc == pc + 4u &&
        event.entry == 0x800799ccu &&
        event.kind == NBA97_GAME_CAMERA_STARTUP_CHILD_800799CC &&
        event.argument_count == 2);
    check(r.gpr[NBA97_MATCH_INITIALIZE_A0].word == a0 &&
        r.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask == a0_mask &&
        r.gpr[NBA97_MATCH_INITIALIZE_A1].word == 0 &&
        r.gpr[NBA97_MATCH_INITIALIZE_A1].known_mask == 0x0f);
    check(r.gpr[NBA97_MATCH_INITIALIZE_RA].word == pc + 8u &&
        r.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 0x0f);
    for (unsigned i = NBA97_MATCH_INITIALIZE_A2;
         i < NBA97_MATCH_INITIALIZE_RA; ++i) {
        const std::uint32_t expected = i == NBA97_MATCH_INITIALIZE_SP ?
            EntrySp - 0x18u : 0x11000000u + i * 0x01010101u;
        check(r.gpr[i].word == expected && r.gpr[i].known_mask == 0x0f);
    }
}

void check_common_result(Fixture& f) {
    check(f.get(0x80104744u, 4) == 0xffffffffu);
    check(f.get(0x800bc258u, 4) == 0x100u &&
        f.get(0x800fc9b4u, 4) == 0x100u);
    check(f.get(0x800fa378u, 1) == 0x34u &&
        f.get(0x800fabc4u, 1) == 0x78u);
    check(f.get(0x801042acu, 4) == 0 &&
        f.get(0x801042b0u, 4) == 0 &&
        f.get(0x801042b4u, 4) == 0 &&
        f.get(0x80106074u, 4) == 0);
    check(f.get(0x800bc1f4u, 4) == 0xffffffffu);
    check(f.get(0x8010607cu, 4) == 0x10203040u &&
        f.get(0x80106080u, 4) == 0x50607080u &&
        f.get(0x80106084u, 4) == 0x90a0b0c0u);
    check(f.progress.completed && f.progress.stopped_pc == 0 &&
        f.progress.callbacks_completed == 1 &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x12345678u);
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
        std::uint32_t expected = 0x11000000u + i * 0x01010101u;
        if (i == NBA97_MATCH_INITIALIZE_ZERO) expected = 0;
        if (i == NBA97_MATCH_INITIALIZE_AT) expected = 0x80100000u;
        if (i == NBA97_MATCH_INITIALIZE_V0) expected = 0xffffffffu;
        if (i == NBA97_MATCH_INITIALIZE_V1) expected = 0x10203040u;
        if (i == NBA97_MATCH_INITIALIZE_A0) expected = 0x50607080u;
        if (i == NBA97_MATCH_INITIALIZE_A1) expected = 0x90a0b0c0u;
        if (i == NBA97_MATCH_INITIALIZE_SP) expected = EntrySp;
        if (i == NBA97_MATCH_INITIALIZE_RA) expected = 0x12345678u;
        check(f.progress.registers.gpr[i].word == expected &&
            f.progress.registers.gpr[i].known_mask == 0x0f);
    }
}

void both_source_branches() {
    Fixture other(0);
    check(other.run() == NBA97_TEXT_COMPLETE);
    check_event(other, 0x800796b8u, 12);
    check(other.progress.operations == 23 && other.progress.reads == 6 &&
        other.progress.stores == 16);
    check(other.get(0x801029bcu, 1) == 1 &&
        other.get(0x800dce00u, 4) == 0);
    check_common_result(other);

    Fixture exact(1);
    exact.put(0x801029bcu, 0x5au, 1);
    exact.put(0x800dce00u, 0xaabbccddu, 4);
    check(exact.run() == NBA97_TEXT_COMPLETE);
    check_event(exact, 0x800796e4u, 0xa5u);
    check(exact.progress.operations == 22 && exact.progress.reads == 7 &&
        exact.progress.stores == 14);
    check(exact.get(0x801029bcu, 1) == 0x5au &&
        exact.get(0x800dce00u, 4) == 0xaabbccddu);
    check_common_result(exact);

    Fixture high(0x80000001u);
    check(high.run() == NBA97_TEXT_COMPLETE);
    check_event(high, 0x800796b8u, 12);
}

void exact_access_order() {
    Fixture f(0);
    check(f.run() == NBA97_TEXT_COMPLETE);
    const std::uint32_t pcs[] = {
        0x80079668u, 0x80079670u, 0x80079680u, 0x8007968cu,
        0x80079694u, 0x8007969cu, 0x800796a4u, 0x800796acu,
        0x800796c8u, 0x800796d0u, 0x800796f0u, 0x800796f8u,
        0x80079700u, 0x8007970cu, 0x80079714u, 0x8007971cu,
        0x80079724u, 0x8007972cu, 0x80079734u, 0x8007973cu,
        0x80079744u, 0x80079748u
    };
    const std::uint32_t addresses[] = {
        0x80021ed9u, 0x80021edau, 0x80104744u, 0x800bc258u,
        0x800fc9b4u, EntrySp - 8u, 0x800fa378u, 0x800fabc4u,
        0x801029bcu, 0x800dce00u, 0x800bc3d4u, 0x800bc3d8u,
        0x800bc3dcu, 0x801042acu, 0x801042b0u, 0x801042b4u,
        0x80106074u, 0x800bc1f4u, 0x8010607cu, 0x80106080u,
        0x80106084u, EntrySp - 8u
    };
    check(f.progress.access_events == std::size(pcs));
    for (std::size_t i = 0; i < std::size(pcs); ++i) {
        check(f.journal[i].pc == pcs[i] &&
            f.journal[i].address == addresses[i]);
        const std::size_t expected_operation = i < 8 ? i + 1 : i + 2;
        check(f.journal[i].operation == expected_operation);
    }
    check(f.calls[0].operation == 9);
    check(f.journal[0].kind == NBA97_GAME_CAMERA_STARTUP_READ &&
        f.journal[0].width == 1 && f.journal[0].known_mask == 1);
    check(f.journal[f.progress.access_events - 1].kind ==
            NBA97_GAME_CAMERA_STARTUP_READ &&
        f.journal[f.progress.access_events - 1].width == 4);
}

void child_mutation_and_refusal() {
    Fixture f(0);
    f.mutate = true;
    check(f.run() == NBA97_TEXT_COMPLETE);
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
        0xcafef00du);
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
            0x01020304u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A2].known_mask ==
            0x05 &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_T0].word ==
            0x55667788u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_T0].known_mask ==
            0x0a &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_GP].word ==
            0x800d1234u);
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        EntrySp + 0x80u + 0x18u);
    check(f.progress.restored_return_address.word == 0xdeadbeefu &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0xdeadbeefu);
    check(f.get(0x8010607cu, 4) == 0x11112222u &&
        f.get(0x80106080u, 4) == 0x33334444u &&
        f.get(0x80106084u, 4) == 0x55556666u);
    check(f.get(0x80104744u, 4) == 0x77778888u);

    for (std::uint32_t a0 : {0u, 1u}) {
        Fixture refused(a0);
        refused.callback_result = 0;
        check(refused.run() == NBA97_TEXT_IO_REFUSED);
        check(refused.progress.stopped_pc ==
            (a0 ? 0x800796e4u : 0x800796b8u));
        check(refused.progress.stopped_entry == 0x800799ccu &&
            refused.progress.callbacks_completed == 0 &&
            !refused.progress.completed);
    }

    Fixture malformed(0);
    malformed.malformed = true;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations == 9 &&
        malformed.progress.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word ==
            1);
}

void byte_knownness_and_unknown_branch() {
    Fixture bytes(0);
    bytes.put(0x80021ed9u, 0x34u, 1, 0);
    bytes.put(0x800bc3d4u, 0x10203040u, 4, 0x0d);
    check(bytes.run() == NBA97_TEXT_COMPLETE);
    check(bytes.known(0x800fa378u)[0] == 0 &&
        bytes.progress.initial_camera_byte.known_mask == 0x0e);
    check(bytes.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V1].known_mask ==
        0x0d);
    check(bytes.known(0x8010607cu)[0] == 1 &&
        bytes.known(0x8010607cu)[1] == 0 &&
        bytes.known(0x8010607cu)[2] == 1 &&
        bytes.known(0x8010607cu)[3] == 1);

    Fixture unknown(1);
    unknown.context.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {1, 0};
    check(unknown.run() == NBA97_TEXT_UNKNOWN);
    check(unknown.progress.operations == 8 &&
        unknown.progress.stopped_pc == 0x800796b0u &&
        unknown.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 12 &&
        unknown.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask ==
            0x0f && unknown.calls.empty());

    Fixture known_false(0);
    known_false.context.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {0, 0x01};
    check(known_false.run() == NBA97_TEXT_COMPLETE);
    check_event(known_false, 0x800796b8u, 12);

    Fixture mode_unknown(1);
    mode_unknown.put(0x80021ed7u, 0xa5u, 1, 0);
    check(mode_unknown.run() == NBA97_TEXT_COMPLETE);
    check_event(mode_unknown, 0x800796e4u, 0xa5u, 0x0e);
}

void all_unsigned_mode_bytes() {
    for (unsigned byte = 0; byte <= 255; ++byte) {
        Fixture f(1);
        f.put(0x80021ed7u, byte, 1);
        check(f.run() == NBA97_TEXT_COMPLETE && f.calls.size() == 1);
        check(f.call_registers[0].gpr[NBA97_MATCH_INITIALIZE_A0].word == byte &&
            f.call_registers[0].gpr[NBA97_MATCH_INITIALIZE_A0].known_mask ==
                0x0f);
    }
}

void stack_unknown_alignment_wrap_and_mapping() {
    Fixture unknown_sp(0);
    unknown_sp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 0;
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.operations == 5 &&
        unknown_sp.progress.stopped_pc == 0x8007969cu);

    Fixture settled_carry(0);
    settled_carry.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
        0x801f0000u, 0x0d};
    check(settled_carry.run() == NBA97_TEXT_UNKNOWN &&
        settled_carry.progress.operations == 5 &&
        settled_carry.progress.stores == 3 &&
        settled_carry.progress.stopped_pc == 0x8007969cu &&
        settled_carry.progress.stopped_address == 0x801efff8u);
    check(settled_carry.progress.frame_stack_pointer == 0x801effe8u &&
        settled_carry.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x801effe8u &&
        settled_carry.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask ==
            0x09);
    check(settled_carry.get(0x80104744u, 4) == 0xffffffffu &&
        settled_carry.get(0x800bc258u, 4) == 0x100u &&
        settled_carry.get(0x800fc9b4u, 4) == 0x100u);

    Fixture alignment(0);
    alignment.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
        EntrySp + 1u, 0x0f};
    check(alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        alignment.progress.operations == 6 &&
        alignment.progress.stopped_address == EntrySp - 7u);

    Fixture unmapped(0);
    unmapped.context.memory.count = 1;
    check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.stopped_pc == 0x8007969cu);

    Fixture first_unmapped(0);
    first_unmapped.context.memory.region = &first_unmapped.regions[1];
    first_unmapped.context.memory.count = 1;
    check(first_unmapped.run() == NBA97_TEXT_RESOURCE &&
        first_unmapped.progress.operations == 1 &&
        first_unmapped.progress.stopped_pc == 0x80079668u &&
        first_unmapped.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V1].word ==
            0x80020000u &&
        first_unmapped.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V1].known_mask ==
            0x0f);

    Fixture unknown_ra(0);
    unknown_ra.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        0x12345678u, 0x07};
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.operations == 23 &&
        unknown_ra.progress.stopped_pc == 0x80079750u &&
        unknown_ra.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp &&
        unknown_ra.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask ==
            0x07);

    Fixture wrap(0);
    std::array<std::uint8_t, 0x20> low{}, low_known{};
    low_known.fill(1);
    Nba97GameTextRegion wrap_regions[3] = {
        wrap.regions[0], wrap.regions[1],
        {0, low.data(), low_known.data(), low.size()}
    };
    wrap.context.memory = {wrap_regions, 3};
    wrap.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x10u, 0x0f};
    check(wrap.run() == NBA97_TEXT_COMPLETE &&
        wrap.progress.frame_stack_pointer == 0xfffffff8u &&
        wrap.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 0x10u);
    std::uint32_t low_ra = 0;
    for (unsigned i = 0; i < 4; ++i)
        low_ra |= std::uint32_t(low[8 + i]) << (8u * i);
    check(low_ra == 0x12345678u);
}

void validation_and_aliasing() {
    Fixture bad_zero;
    bad_zero.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask = 0;
    check(bad_zero.run() == NBA97_TEXT_ARGUMENT);

    Fixture bad_mask;
    bad_mask.context.registers.gpr[NBA97_MATCH_INITIALIZE_T0].known_mask = 0x10;
    check(bad_mask.run() == NBA97_TEXT_ARGUMENT);

    Fixture overlap;
    Nba97GameTextRegion overlapping[2] = {
        overlap.regions[0],
        {Ram + 0x100u, overlap.stack.data(), overlap.stack_known.data(), 0x100}
    };
    overlap.context.memory = {overlapping, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT);

    Fixture alias(0);
    alias.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
        0x800bc3dcu, 0x0f};
    /* The saved-ra word aliases source word 0x800BC3D4. The later source load
       must observe the earlier stack store, proving ordinary retained-memory
       alias behavior rather than a protected native stack abstraction. */
    check(alias.run() == NBA97_TEXT_COMPLETE);
    check(alias.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        0x800bc3dcu);
    check(alias.get(0x8010607cu, 4) == 0x12345678u);
}

void every_operation_budget_prefix() {
    Fixture baseline(0);
    check(baseline.run() == NBA97_TEXT_COMPLETE &&
        baseline.progress.operations == 23);
    const std::uint32_t stopped_pcs[] = {
        0x80079668u, 0x80079670u, 0x80079680u, 0x8007968cu,
        0x80079694u, 0x8007969cu, 0x800796a4u, 0x800796acu,
        0x800796b8u, 0x800796c8u, 0x800796d0u, 0x800796f0u,
        0x800796f8u, 0x80079700u, 0x8007970cu, 0x80079714u,
        0x8007971cu, 0x80079724u, 0x8007972cu, 0x80079734u,
        0x8007973cu, 0x80079744u, 0x80079748u
    };
    for (std::size_t budget = 0; budget < baseline.progress.operations;
         ++budget) {
        Fixture f(0);
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT);
        check(f.progress.operations == budget && !f.progress.completed);
        check(f.progress.stopped_pc == stopped_pcs[budget] &&
            f.progress.stopped_entry ==
                (budget == 8 ? 0x800799ccu : 0));
        check(f.progress.access_events <= baseline.progress.access_events);
        for (std::size_t i = 0; i < f.progress.access_events; ++i) {
            check(f.journal[i].pc == baseline.journal[i].pc &&
                f.journal[i].address == baseline.journal[i].address &&
                f.journal[i].kind == baseline.journal[i].kind &&
                f.journal[i].known_mask == baseline.journal[i].known_mask);
        }
    }

    Fixture branch_baseline(1);
    check(branch_baseline.run() == NBA97_TEXT_COMPLETE &&
        branch_baseline.progress.operations == 22);
    const std::uint32_t branch_stopped_pcs[] = {
        0x80079668u, 0x80079670u, 0x80079680u, 0x8007968cu,
        0x80079694u, 0x8007969cu, 0x800796a4u, 0x800796acu,
        0x800796e0u, 0x800796e4u, 0x800796f0u, 0x800796f8u,
        0x80079700u, 0x8007970cu, 0x80079714u, 0x8007971cu,
        0x80079724u, 0x8007972cu, 0x80079734u, 0x8007973cu,
        0x80079744u, 0x80079748u
    };
    for (std::size_t budget = 0;
         budget < branch_baseline.progress.operations; ++budget) {
        Fixture f(1);
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT &&
            f.progress.operations == budget && !f.progress.completed);
        check(f.progress.stopped_pc == branch_stopped_pcs[budget] &&
            f.progress.stopped_entry ==
                (budget == 9 ? 0x800799ccu : 0));
    }
}
}

int main() {
    both_source_branches();
    exact_access_order();
    child_mutation_and_refusal();
    byte_knownness_and_unknown_branch();
    all_unsigned_mode_bytes();
    stack_unknown_alignment_wrap_and_mapping();
    validation_and_aliasing();
    every_operation_budget_prefix();
    std::printf("game camera-startup: %u checks passed\n", checks);
}
