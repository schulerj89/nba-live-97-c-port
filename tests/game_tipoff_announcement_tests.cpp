#include "game_tipoff_announcement.h"

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
        std::fprintf(stderr, "tip-off announcement check %u failed at %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x800ff800u;
constexpr std::uint32_t FrameSp = EntrySp - 0x20u;
constexpr std::uint32_t EntryRa = 0x80067458u;

constexpr std::array<std::uint32_t, 23> ModeTwoPrefixPcs{{
    0x8007ef50u, 0x8007ef54u, 0x8007ef58u, 0x8007ef60u,
    0x8007ef5cu, 0x8007ef70u, 0x8007ef7cu, 0x8007ef8cu,
    0x8007ef98u, 0x8007efa4u, 0x8007efacu, 0x8007efb8u,
    0x8007efbcu, 0x8007efc8u, 0x8007efd0u, 0x8007efdcu,
    0x8007efe8u, 0x8007effcu, 0x8007f050u, 0x8007f058u,
    0x8007f05cu, 0x8007f060u, 0x8007f064u
}};

constexpr std::array<std::uint32_t, 12> ModeTwoEntries{{
    0x800887e8u, 0x8007fa50u, 0x8007eea8u, 0x8007fa9cu,
    0x8007eca4u, 0x800b1e14u, 0x80083748u, 0x80083748u,
    0x8007fa9cu, 0x8007fa9cu, 0x8007ececu, 0x800b1e14u
}};

struct Fixture {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u, 0);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameTipoffAnnouncementContext context{};
    Nba97GameTipoffAnnouncementProgress progress{};
    std::array<Nba97GameTipoffAnnouncementAccess, 32> journal{};
    std::vector<Nba97GameTipoffAnnouncementEvent> calls;
    std::vector<Nba97GameTipoffAnnouncementRegisters> call_registers;
    Nba97GameTipoffAnnouncementRegisters entry{};
    Nba97GameTipoffAnnouncementWord gate_result{8u, 0x0f};
    unsigned refuse_call{};
    bool mutate_mode_two_live = false;
    bool relocate_stack = false;
    bool unknown_saved_ra = false;
    bool malformed_callback = false;
    std::uint32_t relocated_sp = 0x800ff700u;

    explicit Fixture(std::uint8_t mode = 2, std::int32_t mode_one = 1) {
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {
                0x21000000u + i * 0x01010101u, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {EntryRa, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_S0] =
            {0x10203040u, 0x0f};
        context.registers.gpr[NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1] =
            {0x50607080u, 0x0f};
        context.registers.gpr[NBA97_GAME_TIPOFF_ANNOUNCEMENT_S2] =
            {0x90a0b0c0u, 0x0f};
        entry = context.registers;
        context.memory = {&region, 1};
        context.operation_budget = 100;
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        put(0x80021d70u, mode, 1);
        put(0x80021d74u, 0x11223344u, 4);
        put(0x80021d78u, 0x55667788u, 4);
        put(0x8001ec94u, static_cast<std::uint32_t>(mode_one), 4);
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Ram);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width) {
        const auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            known[at + i] = 1;
        }
    }
    std::uint32_t get(std::uint32_t address) const {
        const auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(bytes[at + i]) << (i * 8u);
        return value;
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameTipoffAnnouncementEvent* event,
        Nba97GameTipoffAnnouncementRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        f.call_registers.push_back(*registers);
        if (f.refuse_call == f.calls.size())
            return 0;
        if (f.malformed_callback && f.calls.size() == 1)
            registers->gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
        switch (event->pc) {
        case 0x8007ef5cu:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = f.gate_result;
            break;
        case 0x8007ef8cu:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x22u, 0x0f};
            break;
        case 0x8007ef98u:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x10u, 0x0f};
            if (f.mutate_mode_two_live)
                registers->gpr[NBA97_GAME_TIPOFF_ANNOUNCEMENT_S2] =
                    {0x44u, 0x0f};
            break;
        case 0x8007efbcu:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] =
                {0xfffffff0u, 0x0f};
            break;
        case 0x8007efd0u:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x30u, 0x0f};
            if (f.mutate_mode_two_live)
                registers->gpr[NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1] =
                    {0xfffffff0u, 0x0f};
            break;
        case 0x8007efdcu:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x40u, 0x0f};
            break;
        case 0x8007efe8u:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x50u, 0x0f};
            if (f.mutate_mode_two_live)
                registers->gpr[NBA97_MATCH_INITIALIZE_S0] = {0x77u, 0x0f};
            break;
        case 0x8007f02cu:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x60u, 0x0f};
            break;
        case 0x8007f038u:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {0x70u, 0x0f};
            break;
        case 0x8007f050u:
            registers->gpr[NBA97_MATCH_INITIALIZE_V0] =
                {0xdecafbadu, 0x05};
            if (f.relocate_stack) {
                registers->gpr[NBA97_MATCH_INITIALIZE_SP] =
                    {f.relocated_sp, 0x0f};
                f.put(f.relocated_sp + 0x1cu, 0x80123456u, 4);
                f.put(f.relocated_sp + 0x18u, 0x11111111u, 4);
                f.put(f.relocated_sp + 0x14u, 0x22222222u, 4);
                f.put(f.relocated_sp + 0x10u, 0x33333333u, 4);
            }
            if (f.unknown_saved_ra) {
                const std::uint32_t sp =
                    registers->gpr[NBA97_MATCH_INITIALIZE_SP].word;
                f.known[f.offset(sp + 0x1cu)] = 0;
            }
            break;
        }
        return 1;
    }
    int run() {
        return nba97_game_tipoff_announcement(&context, &progress);
    }
};

void exact_mode_two_path_and_mutable_delays() {
    Fixture f;
    f.mutate_mode_two_live = true;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations == 23 && f.progress.accesses == 11 &&
        f.progress.reads == 7 && f.progress.stores == 4 &&
        f.progress.callbacks_completed == 12 && f.progress.mode_path == 2);
    check(f.calls.size() == ModeTwoEntries.size());
    for (std::size_t i = 0; i < f.calls.size(); ++i) {
        check(f.calls[i].entry == ModeTwoEntries[i]);
        check(f.calls[i].delay_slot_pc == f.calls[i].pc + 4u);
        check(f.calls[i].operation > (i ? f.calls[i - 1].operation : 0));
    }
    check(f.call_registers[0].gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        0x8007ef64u);
    check(f.get(FrameSp + 0x1cu) == EntryRa &&
        f.get(FrameSp + 0x18u) == 0x90a0b0c0u &&
        f.get(FrameSp + 0x14u) == 0x50607080u &&
        f.get(FrameSp + 0x10u) == 0x10203040u);
    check(f.call_registers[1].gpr[NBA97_MATCH_INITIALIZE_A0].word == 0 &&
        f.call_registers[1].gpr[NBA97_MATCH_INITIALIZE_A0].known_mask == 0x0f);
    check(f.call_registers[3].gpr[NBA97_GAME_TIPOFF_ANNOUNCEMENT_S2].word ==
        0x22u);
    check(f.call_registers[4].gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x10u &&
        f.call_registers[4].gpr[NBA97_MATCH_INITIALIZE_A1].word == 0x44u);
    check(f.call_registers[6].gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        0x11223344u &&
        f.call_registers[6].gpr[NBA97_MATCH_INITIALIZE_A1].word == 0);
    check(f.call_registers[7].gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        0x55667788u &&
        f.call_registers[7].gpr[NBA97_MATCH_INITIALIZE_A1].word == 1 &&
        f.call_registers[7].gpr[NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1].word ==
            0xfffffff0u);
    check(f.call_registers[8].gpr[NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1].word ==
        0x20u);
    check(f.call_registers[9].gpr[NBA97_MATCH_INITIALIZE_S0].word == 0x40u);
    check(f.call_registers[10].gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x77u &&
        f.call_registers[10].gpr[NBA97_MATCH_INITIALIZE_A1].word == 0x50u &&
        f.call_registers[10].gpr[NBA97_MATCH_INITIALIZE_A2].word == 0x20u &&
        f.call_registers[10].gpr[NBA97_MATCH_INITIALIZE_A3].word == 0x44u);
    check(f.progress.restored_return_address.word == EntryRa &&
        f.progress.restored_s2.word == 0x90a0b0c0u &&
        f.progress.restored_s1.word == 0x50607080u &&
        f.progress.restored_s0.word == 0x10203040u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0xdecafbadu &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 0x05);
    const std::array<std::uint32_t, 11> access_pcs{{
        0x8007ef50u, 0x8007ef54u, 0x8007ef58u, 0x8007ef60u,
        0x8007ef7cu, 0x8007efb8u, 0x8007efc8u, 0x8007f058u,
        0x8007f05cu, 0x8007f060u, 0x8007f064u}};
    for (std::size_t i = 0; i < access_pcs.size(); ++i)
        check(f.journal[i].pc == access_pcs[i] &&
            f.journal[i].operation >= i + 1u);
}

void signed_gate_boundaries_and_unknown_slt() {
    const std::array<std::uint32_t, 5> values{{
        0xffffffffu, 7u, 8u, 0x80000000u, 0x7fffffffu}};
    for (std::uint32_t value : values) {
        Fixture f;
        f.gate_result = {value, 0x0f};
        check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
        const bool early = static_cast<std::int32_t>(value) < 8;
        check((f.calls.size() == 1) == early);
        check(f.progress.gate.word == (early ? 1u : 0u) &&
            f.progress.gate.known_mask == 0x0f);
        check(f.progress.operations == (early ? 9u : 23u));
    }
    Fixture unknown;
    unknown.gate_result = {8u, 0x08};
    check(unknown.run() == NBA97_TEXT_UNKNOWN && !unknown.progress.completed &&
        unknown.progress.operations == 5 &&
        unknown.progress.stopped_pc == 0x8007ef68u &&
        unknown.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 0 &&
        unknown.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask ==
            0x0e);
}

void modes_and_signed_mode_one_flag() {
    struct Case { std::uint8_t mode; std::int32_t flag; unsigned selector; };
    const std::array<Case, 6> cases{{
        {0, 1, 3}, {1, -1, 3}, {1, 0, 3}, {1, 1, 5}, {2, 1, 0}, {255, 1, 3}}};
    for (const auto& c : cases) {
        Fixture f(c.mode, c.flag);
        check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
        if (c.mode == 2) {
            check(f.calls.size() == 12 && f.progress.mode_path == 2);
        } else {
            check(f.calls.size() == 6 &&
                f.call_registers[4].gpr[NBA97_MATCH_INITIALIZE_A2].word ==
                    c.selector &&
                f.progress.mode_path == (c.mode == 1 ? 1 : 0));
            check(f.progress.operations == (c.mode == 1 ? 16u : 15u));
        }
    }
    Fixture unknown_mode;
    unknown_mode.known[unknown_mode.offset(0x80021d70u)] = 0;
    check(unknown_mode.run() == NBA97_TEXT_UNKNOWN &&
        unknown_mode.progress.stopped_pc == 0x8007ef84u &&
        unknown_mode.progress.operations == 7 &&
        unknown_mode.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            1u &&
        unknown_mode.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .known_mask == 0x0f);
    for (unsigned reg = 0; reg < NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
         ++reg) {
        auto expected = unknown_mode.entry.gpr[reg];
        if (reg == NBA97_MATCH_INITIALIZE_SP)
            expected = {FrameSp, 0x0f};
        if (reg == NBA97_MATCH_INITIALIZE_RA)
            expected = {0x8007ef78u, 0x0f};
        if (reg == NBA97_MATCH_INITIALIZE_V0)
            expected = {1u, 0x0f};
        if (reg == NBA97_MATCH_INITIALIZE_V1)
            expected = {2u, 0x0e};
        if (reg == NBA97_MATCH_INITIALIZE_A0)
            expected = {0u, 0x0f};
        check(unknown_mode.progress.registers.gpr[reg].word == expected.word &&
            unknown_mode.progress.registers.gpr[reg].known_mask ==
                expected.known_mask);
    }
    Fixture unknown_flag(1);
    for (unsigned i = 0; i < 4; ++i)
        unknown_flag.known[unknown_flag.offset(0x8001ec94u) + i] = 0;
    check(unknown_flag.run() == NBA97_TEXT_UNKNOWN &&
        unknown_flag.progress.stopped_pc == 0x8007f020u &&
        unknown_flag.progress.registers.gpr[
            NBA97_GAME_TIPOFF_ANNOUNCEMENT_S1].word ==
            3u);
}

void mutable_stack_partial_data_alias_and_wrap() {
    Fixture moved;
    moved.relocate_stack = true;
    check(moved.run() == NBA97_TEXT_COMPLETE &&
        moved.progress.restored_return_address.word == 0x80123456u &&
        moved.progress.restored_s2.word == 0x11111111u &&
        moved.progress.restored_s1.word == 0x22222222u &&
        moved.progress.restored_s0.word == 0x33333333u &&
        moved.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            moved.relocated_sp + 0x20u);

    Fixture unknown_ra;
    unknown_ra.unknown_saved_ra = true;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x8007f06cu &&
        unknown_ra.progress.operations == 23 &&
        unknown_ra.progress.restored_return_address.known_mask == 0x0e &&
        unknown_ra.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp);

    Fixture partial_sum;
    partial_sum.mutate_mode_two_live = true;
    partial_sum.gate_result = {8, 0x0f};
    partial_sum.known[partial_sum.offset(0x80021d74u)] = 0;
    check(partial_sum.run() == NBA97_TEXT_COMPLETE);

    Fixture alias(0);
    alias.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {0x80021d80u, 0x0f};
    alias.context.registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {2u, 0x0f};
    check(alias.run() == NBA97_TEXT_COMPLETE && alias.progress.mode_path == 2 &&
        alias.progress.mode.word == 2u);

    Fixture wrap;
    std::array<std::uint8_t, 32> high{};
    std::array<std::uint8_t, 32> high_known{};
    high_known.fill(1);
    Nba97GameTextRegion regions[2] = {
        wrap.region, {0xffffffe0u, high.data(), high_known.data(), high.size()}};
    wrap.context.memory = {regions, 2};
    wrap.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0u, 0x0f};
    check(wrap.run() == NBA97_TEXT_COMPLETE &&
        wrap.progress.frame_stack_pointer == 0xffffffe0u &&
        wrap.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 0u &&
        high[28] == static_cast<std::uint8_t>(EntryRa));
}

void every_refusal_and_operation_budget_prefix() {
    for (unsigned fail = 1; fail <= ModeTwoEntries.size(); ++fail) {
        Fixture f;
        f.refuse_call = fail;
        check(f.run() == NBA97_TEXT_IO_REFUSED && !f.progress.completed &&
            f.progress.callbacks_completed == fail - 1u &&
            f.calls.size() == fail &&
            f.progress.stopped_entry == ModeTwoEntries[fail - 1u]);
    }
    for (std::size_t budget = 0; budget < ModeTwoPrefixPcs.size(); ++budget) {
        Fixture f;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations == budget &&
            f.progress.stopped_pc == ModeTwoPrefixPcs[budget]);
        for (unsigned reg = 0; reg < NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
             ++reg)
            check(f.progress.registers.gpr[reg].known_mask <= 0x0f);
        check(f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word == 0 &&
            f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask ==
                0x0f);
    }
    Fixture exact;
    exact.context.operation_budget = ModeTwoPrefixPcs.size();
    check(exact.run() == NBA97_TEXT_COMPLETE && exact.progress.completed);
}

void traps_mapping_and_validation() {
    Fixture alignment;
    alignment.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {EntrySp + 1u, 0x0f};
    check(alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        alignment.progress.operations == 1 &&
        alignment.progress.stopped_pc == 0x8007ef50u);

    Fixture missing;
    std::array<std::uint8_t, 32> tiny{};
    std::array<std::uint8_t, 32> tiny_known{};
    tiny_known.fill(1);
    Nba97GameTextRegion tiny_region{Ram, tiny.data(), tiny_known.data(),
        tiny.size()};
    missing.context.memory = {&tiny_region, 1};
    check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_address == FrameSp + 0x1cu);

    Fixture malformed_memory;
    malformed_memory.known[malformed_memory.offset(FrameSp + 0x1cu)] = 2;
    check(malformed_memory.run() == NBA97_TEXT_ARGUMENT &&
        malformed_memory.progress.operations == 1);

    Fixture unknown_sp;
    unknown_sp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 0;
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x8007ef50u);

    Fixture malformed_callback;
    malformed_callback.malformed_callback = true;
    check(malformed_callback.run() == NBA97_TEXT_ARGUMENT &&
        malformed_callback.progress.callbacks_completed == 0);

    Fixture no_callback;
    no_callback.context.io = nullptr;
    check(no_callback.run() == NBA97_TEXT_IO_REFUSED &&
        no_callback.progress.stopped_pc == 0x8007ef5cu);

    Nba97GameTipoffAnnouncementProgress progress{};
    check(nba97_game_tipoff_announcement(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
    Fixture no_out;
    check(nba97_game_tipoff_announcement(&no_out.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
    Fixture bad_zero;
    bad_zero.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
    check(bad_zero.run() == NBA97_TEXT_ARGUMENT);
    Fixture bad_mask;
    bad_mask.context.registers.gpr[NBA97_MATCH_INITIALIZE_T0].known_mask = 0x10;
    check(bad_mask.run() == NBA97_TEXT_ARGUMENT);
    Fixture overlap;
    Nba97GameTextRegion overlap_regions[2] = {overlap.region, overlap.region};
    overlap.context.memory = {overlap_regions, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    exact_mode_two_path_and_mutable_delays();
    signed_gate_boundaries_and_unknown_slt();
    modes_and_signed_mode_one_flag();
    mutable_stack_partial_data_alias_and_wrap();
    every_refusal_and_operation_budget_prefix();
    traps_mapping_and_validation();
    std::printf("game tip-off announcement: %u checks passed\n", checks);
    return 0;
}
