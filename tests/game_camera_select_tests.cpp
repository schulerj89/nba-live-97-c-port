#include "recovered/game_camera_select.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game camera-select check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x8010f000u;

struct Fixture {
    std::vector<std::uint8_t> ram =
        std::vector<std::uint8_t>(0x110000, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000, 1);
    Nba97GameTextRegion region{Ram, ram.data(), known.data(), ram.size()};
    Nba97GameCameraSelectContext context{};
    Nba97GameCameraSelectProgress progress{};
    std::array<Nba97GameCameraSelectAccess, 512> journal{};
    std::vector<Nba97GameCameraSelectEvent> calls;
    std::vector<Nba97GameCameraSelectRegisters> call_registers;
    std::uint32_t refuse_pc = 0;
    std::uint32_t mutate_s0_pc = 0;
    std::uint32_t mutate_s0 = 0;
    std::uint32_t mutate_s1_pc = 0;
    std::uint32_t mutate_s1 = 0;
    std::uint32_t mutate_sp_pc = 0;
    std::uint32_t mutate_sp = 0;
    std::uint32_t publish_mode_pc = 0;
    std::uint32_t publish_mode = 0;
    std::uint32_t corrupt_mask_pc = 0;
    bool scribble_camera_at_798b4 = false;
    bool fill_live_stack_at_798b4 = false;

    Fixture() {
        context.memory = {&region, 1};
        context.operation_budget = std::numeric_limits<std::size_t>::max();
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {
                0x22000000u + i * 0x00010101u, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x81234567u, 0x0f};
        put(0x80021ed7u, 6, 1);
        put(0x80021ed8u, 0x5a, 1);
        for (unsigned i = 0; i < 6; ++i)
            put(0x80109aa8u + i * 4u, 0xa1000000u + i, 4);
    }

    void put(std::uint32_t address, std::uint32_t value, unsigned width) {
        auto offset = static_cast<std::size_t>(address - Ram);
        for (unsigned i = 0; i < width; ++i) {
            ram[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            known[offset + i] = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width = 4) const {
        auto offset = static_cast<std::size_t>(address - Ram);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(ram[offset + i]) << (i * 8u);
        return value;
    }
    void setKnown(std::uint32_t address, unsigned width, std::uint8_t value) {
        auto offset = static_cast<std::size_t>(address - Ram);
        std::fill(known.begin() + offset, known.begin() + offset + width, value);
    }
    int run(std::uint32_t mode, std::uint32_t selector,
        std::size_t budget = std::numeric_limits<std::size_t>::max()) {
        context.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {mode, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {selector, 0x0f};
        context.operation_budget = budget;
        return nba97_game_camera_select(&context, &progress);
    }
    std::vector<std::uint32_t> pcs() const {
        std::vector<std::uint32_t> result;
        for (const auto& call : calls)
            result.push_back(call.pc);
        return result;
    }
    bool hasPc(std::uint32_t pc) const {
        return std::any_of(calls.begin(), calls.end(), [pc](const auto& call) {
            return call.pc == pc;
        });
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameCameraSelectEvent* event,
        Nba97GameCameraSelectRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        f.call_registers.push_back(*registers);
        if (event->pc == f.mutate_s0_pc)
            registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {
                f.mutate_s0, 0x0f};
        if (event->pc == f.mutate_s1_pc)
            registers->gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = {
                f.mutate_s1, 0x0f};
        if (event->pc == f.mutate_sp_pc)
            registers->gpr[NBA97_MATCH_INITIALIZE_SP] = {
                f.mutate_sp, 0x0f};
        if (event->pc == f.publish_mode_pc)
            f.put(0x800fc99cu, f.publish_mode, 4);
        if (event->pc == f.corrupt_mask_pc)
            registers->gpr[NBA97_MATCH_INITIALIZE_T0].known_mask = 0x10;
        if (event->pc == 0x80079c2cu && f.scribble_camera_at_798b4)
            for (unsigned i = 0; i < 14; ++i)
                f.put(0x800fc99cu + i * 4u, 0xde000000u + i, 4);
        if (event->pc == 0x80079c2cu && f.fill_live_stack_at_798b4)
            for (unsigned i = 0; i < 18; ++i)
                f.put(f.mutate_sp + 0x10u + i * 4u,
                    0x91000000u + i, 4);
        return event->pc == f.refuse_pc ? 0 : 1;
    }
};

void expectComplete(const Fixture& f, std::uint8_t exit_kind) {
    check(f.progress.completed && f.progress.exit_kind == exit_kind);
    check(f.progress.stopped_pc == 0 && f.progress.stopped_address == 0 &&
        f.progress.stopped_entry == 0);
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp);
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        0x81234567u);
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
        0x22101010u);
    check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word ==
        0x22111111u);
}

void mode_zero_paths() {
    Fixture zero;
    check(zero.run(0, 0) == NBA97_TEXT_COMPLETE);
    expectComplete(zero, NBA97_GAME_CAMERA_SELECT_EXIT_MODE_ZERO);
    check(zero.pcs() == std::vector<std::uint32_t>{0x80079a0cu});
    check(zero.call_registers[0].gpr[NBA97_MATCH_INITIALIZE_A0].word == 0);
    check(zero.get(0x800fc99cu) == 0);

    Fixture old;
    old.put(0x800fc99cu, 0x44, 4);
    check(old.run(0, 7) == NBA97_TEXT_COMPLETE);
    check(old.call_registers[0].gpr[NBA97_MATCH_INITIALIZE_A0].word == 1);
    auto clear = std::find_if(old.journal.begin(),
        old.journal.begin() + old.progress.access_events, [](const auto& e) {
            return e.pc == 0x80079a04u && e.address == 0x800fc99cu &&
                e.kind == NBA97_GAME_CAMERA_SELECT_STORE;
        });
    check(clear != old.journal.begin() + old.progress.access_events);
    expectComplete(old, NBA97_GAME_CAMERA_SELECT_EXIT_MODE_ZERO);
}

void signed_dispatch_and_boundaries() {
    struct Case {
        std::uint32_t mode;
        std::uint32_t first_pc;
        std::uint32_t first_argument;
    };
    const Case child_cases[] = {
        {8, 0x80079aa4u, 0}, {9, 0x80079ac4u, 0},
        {12, 0x80079ab4u, 0}, {100, 0x80079ad4u, 0},
        {101, 0x80079ad4u, 1}, {102, 0x80079ad4u, 2},
        {103, 0x80079ad4u, 3}, {104, 0x80079ad4u, 4},
        {105, 0x80079ad4u, 5}, {106, 0x80079ad4u, 6},
        {200, 0x80079ae4u, 11}, {201, 0x80079ae4u, 11},
        {202, 0x80079ae4u, 10}, {203, 0x80079ae4u, 10}
    };
    for (const auto& test : child_cases) {
        Fixture f;
        f.put(0x800bc268u + (test.mode << 2), 0xcc000000u + test.mode, 4);
        check(f.run(test.mode, 2) == NBA97_TEXT_COMPLETE);
        check(!f.calls.empty() && f.calls[0].pc == test.first_pc);
        if (test.first_pc == 0x80079ad4u || test.first_pc == 0x80079ae4u)
            check(f.call_registers[0].gpr[NBA97_MATCH_INITIALIZE_A0].word ==
                test.first_argument);
        check(f.get(0x801029f8u, 1) == 0);
    }

    for (std::uint32_t mode : {7u, 10u, 11u, 13u, 99u, 107u, 199u,
             204u, 0xffffffffu, 0x80000000u, 0x40000000u}) {
        Fixture f;
        std::uint32_t table = 0x800bc268u + (mode << 2);
        f.put(table, 0x73000000u ^ mode, 4);
        check(f.run(mode, 2) == NBA97_TEXT_COMPLETE);
        check(f.get(0x800fc9d0u) == (0x73000000u ^ mode));
        check(f.hasPc(0x80079b7cu) || mode == 10u);
    }

    Fixture flag12;
    flag12.put(0x80021ed7u, 5, 1);
    check(flag12.run(12, 2) == NBA97_TEXT_COMPLETE);
    check(flag12.get(0x800fa62cu) == 1);
    Fixture flag11;
    check(flag11.run(11, 2) == NBA97_TEXT_COMPLETE);
    check(flag11.get(0x800fa62cu) == 1);
}

void callback_live_branches_and_busy_quirk() {
    Fixture already;
    already.put(0x800fc99cu, 10, 4);
    check(already.run(200, 0) == NBA97_TEXT_COMPLETE);
    check(already.pcs() ==
        std::vector<std::uint32_t>({0x80079ae4u, 0x80079b00u}));
    check(already.progress.exit_kind ==
        NBA97_GAME_CAMERA_SELECT_EXIT_ALREADY_SELECTED);
    check(already.get(0x801029f8u, 1) == 1);

    Fixture provider_mutates_mode;
    provider_mutates_mode.mutate_s0_pc = 0x80079b7cu;
    provider_mutates_mode.mutate_s0 = 10;
    check(provider_mutates_mode.run(8, 2) == NBA97_TEXT_COMPLETE);
    check(provider_mutates_mode.pcs() == std::vector<std::uint32_t>(
        {0x80079aa4u, 0x80079b7cu, 0x80079bd0u, 0x80079d0cu}));

    Fixture mode10_mutates;
    mode10_mutates.mutate_s0_pc = 0x80079b8cu;
    mode10_mutates.mutate_s0 = 9;
    check(mode10_mutates.run(10, 2) == NBA97_TEXT_COMPLETE);
    check(!mode10_mutates.hasPc(0x80079bd0u));

    Fixture selector_mutates;
    selector_mutates.mutate_s1_pc = 0x80079bd0u;
    selector_mutates.mutate_s1 = 2;
    check(selector_mutates.run(10, 0) == NBA97_TEXT_COMPLETE);
    check(!selector_mutates.hasPc(0x80079c8cu));
}

void reset_and_preserve_arms() {
    Fixture reset;
    check(reset.run(8, 0) == NBA97_TEXT_COMPLETE);
    for (unsigned i = 0; i < 6; ++i) {
        check(reset.get(0x800fc9a0u + i * 4u) == 0xa1000000u + i);
        check(reset.get(0x800fc9b8u + i * 4u) == 0);
    }
    check(reset.hasPc(0x80079c8cu));

    Fixture skip;
    for (unsigned i = 0; i < 12; ++i)
        skip.put(0x800fc9a0u + i * 4u, 0x64000000u + i, 4);
    check(skip.run(8, 2) == NBA97_TEXT_COMPLETE);
    for (unsigned i = 0; i < 12; ++i)
        check(skip.get(0x800fc9a0u + i * 4u) == 0x64000000u + i);
    check(!skip.hasPc(0x80079c2cu));

    Fixture preserve;
    for (unsigned i = 0; i < 14; ++i)
        preserve.put(0x800fc99cu + i * 4u, 0x51000000u + i, 4);
    preserve.put(0x800bc268u + 8u * 4u, 0x55667788u, 4);
    preserve.scribble_camera_at_798b4 = true;
    check(preserve.run(8, 1) == NBA97_TEXT_COMPLETE);
    for (unsigned i = 0; i < 14; ++i) {
        std::uint32_t expected = i == 0 ? 8u :
            (i == 13 ? 0x55667788u : 0x51000000u + i);
        check(preserve.get(0x800fc99cu + i * 4u) == expected);
    }

    std::vector<std::uint32_t> copy_pcs;
    for (std::size_t i = 0; i < preserve.progress.access_events; ++i) {
        auto pc = preserve.journal[i].pc;
        if ((pc >= 0x80079bf0u && pc <= 0x80079c28u) ||
            (pc >= 0x80079c44u && pc <= 0x80079c80u))
            copy_pcs.push_back(pc);
    }
    const std::uint32_t batch[] = {
        0x80079bf0u, 0x80079bf4u, 0x80079bf8u, 0x80079bfcu,
        0x80079c00u, 0x80079c04u, 0x80079c08u, 0x80079c0cu
    };
    std::vector<std::uint32_t> expected;
    for (unsigned i = 0; i < 3; ++i)
        expected.insert(expected.end(), std::begin(batch), std::end(batch));
    expected.insert(expected.end(), {0x80079c1cu, 0x80079c20u,
        0x80079c24u, 0x80079c28u});
    const std::uint32_t restore[] = {
        0x80079c44u, 0x80079c48u, 0x80079c4cu, 0x80079c50u,
        0x80079c54u, 0x80079c58u, 0x80079c5cu, 0x80079c60u
    };
    for (unsigned i = 0; i < 3; ++i)
        expected.insert(expected.end(), std::begin(restore), std::end(restore));
    expected.insert(expected.end(), {0x80079c70u, 0x80079c74u,
        0x80079c78u, 0x80079c80u});
    check(copy_pcs == expected);
}

void callback_live_stack_and_aliases() {
    Fixture moved;
    constexpr std::uint32_t NewSp = 0x8010e000u;
    moved.mutate_sp_pc = 0x80079c2cu;
    moved.mutate_sp = NewSp;
    for (unsigned i = 0; i < 14; ++i)
        moved.put(NewSp + 0x10u + i * 4u, 0x88000000u + i, 4);
    moved.put(NewSp + 0x48u, 0x12345678u, 4);
    moved.put(NewSp + 0x4cu, 0x23456789u, 4);
    moved.put(NewSp + 0x50u, 0x83456789u, 4);
    check(moved.run(8, 1) == NBA97_TEXT_COMPLETE);
    for (unsigned i = 0; i < 14; ++i)
        check(moved.get(0x800fc99cu + i * 4u) == 0x88000000u + i);
    check(moved.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        NewSp + 0x58u);
    check(moved.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
        0x12345678u);
    check(moved.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word ==
        0x23456789u);
    check(moved.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        0x83456789u);

    Fixture alias;
    constexpr std::uint32_t AliasSp = 0x800fc98cu;
    alias.mutate_sp_pc = 0x80079c2cu;
    alias.mutate_sp = AliasSp;
    alias.fill_live_stack_at_798b4 = true;
    check(alias.run(8, 1) == NBA97_TEXT_COMPLETE);
    /* Source and destination coincide, proving guest aliases use access order. */
    for (unsigned i = 0; i < 14; ++i)
        check(alias.get(0x800fc99cu + i * 4u) == 0x91000000u + i);
}

void unknowns_traps_and_failures() {
    Fixture entry_unknown;
    entry_unknown.context.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {0, 0x0e};
    entry_unknown.context.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {1, 0x0f};
    check(nba97_game_camera_select(&entry_unknown.context,
        &entry_unknown.progress) == NBA97_TEXT_UNKNOWN);
    check(entry_unknown.progress.stopped_pc == 0x800799e0u &&
        entry_unknown.progress.operations == 3 &&
        entry_unknown.progress.registers.gpr[NBA97_MATCH_INITIALIZE_S0].known_mask
            == 0x0e);

    Fixture signed_unknown;
    signed_unknown.context.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {
        0x01000000u, 0x08};
    signed_unknown.context.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {2, 0x0f};
    check(nba97_game_camera_select(&signed_unknown.context,
        &signed_unknown.progress) == NBA97_TEXT_UNKNOWN);
    check(signed_unknown.progress.stopped_pc == 0x80079b54u);
    check(signed_unknown.get(0x801029f8u, 1) == 1);
    check(signed_unknown.progress.registers.gpr[
        NBA97_MATCH_INITIALIZE_V0].known_mask == 0);

    Fixture byte_unknown;
    byte_unknown.setKnown(0x80021ed7u, 1, 0);
    check(byte_unknown.run(12, 2) == NBA97_TEXT_UNKNOWN);
    check(byte_unknown.progress.stopped_pc == 0x80079b28u &&
        byte_unknown.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 6);

    Fixture table_unknown;
    table_unknown.context.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {
        0x01000000u, 0x08};
    table_unknown.context.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {2, 0x0f};
    /* The known positive high byte proves every signed SLTI false; the
       shifted unknown low bytes stop only when the table address is used. */
    check(nba97_game_camera_select(&table_unknown.context,
        &table_unknown.progress) == NBA97_TEXT_UNKNOWN);
    check(table_unknown.progress.stopped_pc == 0x80079b54u);

    Fixture misaligned;
    misaligned.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
        EntrySp + 1u, 0x0f};
    check(misaligned.run(8, 0) == NBA97_TEXT_ALIGNMENT_TRAP);
    check(misaligned.progress.stopped_pc == 0x800799d0u &&
        misaligned.progress.operations == 1);

    Fixture bad_known;
    bad_known.setKnown(EntrySp - 0x10u, 4, 2);
    check(bad_known.run(8, 0) == NBA97_TEXT_ARGUMENT);
    check(bad_known.progress.stopped_pc == 0x800799d0u);

    Fixture unknown_ra;
    unknown_ra.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 0;
    check(unknown_ra.run(0, 0) == NBA97_TEXT_UNKNOWN);
    check(unknown_ra.progress.stopped_pc == 0x80079d30u &&
        unknown_ra.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp);

    Fixture corrupt_callback;
    corrupt_callback.corrupt_mask_pc = 0x80079b7cu;
    check(corrupt_callback.run(8, 2) == NBA97_TEXT_ARGUMENT);
    check(corrupt_callback.progress.stopped_pc == 0x80079b7cu);
}

void every_call_refusal() {
    struct Refusal { std::uint32_t mode, selector, pc, old_mode; };
    const Refusal cases[] = {
        {0, 0, 0x80079a0cu, 0}, {8, 2, 0x80079aa4u, 0},
        {12, 2, 0x80079ab4u, 0}, {9, 2, 0x80079ac4u, 0},
        {100, 2, 0x80079ad4u, 0}, {200, 2, 0x80079ae4u, 0},
        {200, 0, 0x80079b00u, 10}, {8, 2, 0x80079b7cu, 0},
        {10, 2, 0x80079b8cu, 0}, {10, 2, 0x80079bd0u, 0},
        {8, 1, 0x80079c2cu, 0}, {8, 0, 0x80079c8cu, 0},
        {8, 2, 0x80079d0cu, 0}
    };
    for (const auto& test : cases) {
        Fixture f;
        f.refuse_pc = test.pc;
        f.put(0x800fc99cu, test.old_mode, 4);
        check(f.run(test.mode, test.selector) == NBA97_TEXT_IO_REFUSED);
        check(!f.progress.completed && f.progress.stopped_pc == test.pc &&
            !f.calls.empty() && f.calls.back().pc == test.pc);
    }
}

void budget_prefixes_and_validation() {
    Fixture complete;
    check(complete.run(8, 1) == NBA97_TEXT_COMPLETE);
    auto operations = complete.progress.operations;
    check(operations > 60);
    for (std::size_t budget = 0; budget < operations; ++budget) {
        Fixture a;
        Fixture b;
        check(a.run(8, 1, budget) == NBA97_TEXT_LIMIT);
        check(b.run(8, 1, budget) == NBA97_TEXT_LIMIT);
        check(a.progress.operations == budget &&
            b.progress.operations == budget);
        for (unsigned gpr = 0; gpr < NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
             ++gpr) {
            check(a.progress.registers.gpr[gpr].word ==
                b.progress.registers.gpr[gpr].word);
            check(a.progress.registers.gpr[gpr].known_mask ==
                b.progress.registers.gpr[gpr].known_mask);
        }
        check(a.ram == b.ram && a.known == b.known);
    }

    Fixture missing;
    missing.region.size = 4;
    check(missing.run(8, 0) == NBA97_TEXT_RESOURCE);
    check(missing.progress.operations == 1 &&
        missing.progress.stopped_pc == 0x800799d0u);

    Fixture invalid;
    invalid.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {1, 0x0f};
    check(nba97_game_camera_select(&invalid.context, &invalid.progress) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_camera_select(nullptr, &invalid.progress) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_camera_select(&invalid.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    mode_zero_paths();
    signed_dispatch_and_boundaries();
    callback_live_branches_and_busy_quirk();
    reset_and_preserve_arms();
    callback_live_stack_and_aliases();
    unknowns_traps_and_failures();
    every_call_refusal();
    budget_prefixes_and_validation();
    std::printf("game camera-select: %u checks passed\n", checks);
}
