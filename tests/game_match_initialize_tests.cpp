#include "game_match_initialize_adapter.h"

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
        std::fprintf(stderr, "game match-initialize check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x807fff00u;
constexpr std::uint32_t EntrySp = 0x807fffa8u;
constexpr std::uint32_t FrameSp = EntrySp - 0x18u;
constexpr std::uint32_t Team0 = 0x80021d74u;
constexpr std::uint32_t Team1 = 0x80021d78u;
constexpr std::uint32_t Snapshot0 = 0x80022084u;
constexpr std::uint32_t Snapshot1 = 0x80022adcu;
constexpr std::uint32_t MatchState = 0x800fdb4cu;
constexpr std::uint32_t MatchStateSize = 0xe7cu;
constexpr std::uint32_t FinalFlag = 0x80020c18u;

struct LoggedCall {
    Nba97GameMatchInitializeEvent event{};
    Nba97GameMatchInitializeRegisters registers{};
};

struct Fixture {
    std::vector<std::uint8_t> ram = std::vector<std::uint8_t>(0x100000, 0xcd);
    std::vector<std::uint8_t> ram_known =
        std::vector<std::uint8_t>(0x100000, 1);
    std::array<std::uint8_t, 0x100> stack{};
    std::array<std::uint8_t, 0x100> stack_known{};
    Nba97GameTextRegion regions[2] = {
        {Ram, ram.data(), ram_known.data(), ram.size()},
        {Stack, stack.data(), stack_known.data(), stack.size()}
    };
    std::array<Nba97GameMatchInitializeAccess, 16> journal{};
    Nba97GameMatchInitializeContext context{};
    Nba97GameMatchInitializeProgress progress{};
    Nba97GameMatchInitializeAdapterProgress adapter{};
    std::vector<LoggedCall> calls;
    std::size_t refuse_call = static_cast<std::size_t>(-1);
    std::size_t malformed_call = static_cast<std::size_t>(-1);
    bool mutate_saved_ra = false;
    bool inspect_zero_prefix = false;

    Fixture() {
        stack.fill(0xcd);
        stack_known.fill(1);
        context.memory = {regions, 2};
        context.operation_budget = 64;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.registers.gpr[i] = {0x40000000u + i, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 0x0f};
        context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
            0x8002da84u, 0x0f};
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        put(Team0, 0x01020304u);
        put(Team1, 0xa1b2c3d4u);
        put(FinalFlag, 0xfeedfaceu);
    }

    std::uint8_t* byte(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base &&
                std::uint64_t(address - region.base) < region.size)
                return region.data + (address - region.base);
        return nullptr;
    }
    std::uint8_t* known(std::uint32_t address) {
        for (auto& region : regions)
            if (address >= region.base &&
                std::uint64_t(address - region.base) < region.size)
                return region.known ? region.known + (address - region.base) :
                    nullptr;
        return nullptr;
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width = 4) {
        for (unsigned i = 0; i < width; ++i) {
            *byte(address + i) = static_cast<std::uint8_t>(value >> (i * 8u));
            if (known(address + i))
                *known(address + i) = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width = 4) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(*byte(address + i)) << (i * 8u);
        return value;
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameMatchInitializeEvent* event,
        Nba97GameMatchInitializeRegisters* registers) {
        auto& f = *static_cast<Fixture*>(user);
        const std::size_t call = f.calls.size();
        f.calls.push_back({*event, *registers});
        if (call == f.refuse_call)
            return 0;
        if (f.inspect_zero_prefix && call == 0)
            check(f.get(Snapshot0) == 0x01020304u &&
                f.get(Snapshot1) == 0xa1b2c3d4u &&
                f.get(FrameSp + 0x10u) == 0x8002da84u);
        registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {
            0x10000000u + static_cast<std::uint32_t>(call), 0x0f};
        registers->gpr[NBA97_MATCH_INITIALIZE_V1] = {
            0x20000000u + static_cast<std::uint32_t>(call), 0x0f};
        registers->gpr[NBA97_MATCH_INITIALIZE_A0] = {
            0x30000000u + static_cast<std::uint32_t>(call), 0x0f};
        registers->gpr[NBA97_MATCH_INITIALIZE_A1] = {
            0x31000000u + static_cast<std::uint32_t>(call), 0x0f};
        registers->gpr[NBA97_MATCH_INITIALIZE_A2] = {
            0x32000000u + static_cast<std::uint32_t>(call), 0x0f};
        registers->gpr[NBA97_MATCH_INITIALIZE_T0] = {
            0x50000000u + static_cast<std::uint32_t>(call), 0x0f};
        if (call == f.malformed_call)
            registers->gpr[NBA97_MATCH_INITIALIZE_T9].known_mask = 0x10;
        if (call == 11 && f.mutate_saved_ra)
            f.put(FrameSp + 0x10u, 0x81234560u);
        return 1;
    }
    int run() { return nba97_game_match_initialize(&context, &progress); }
    int runAdapted(std::size_t zero_budget = 1000) {
        return nba97_game_match_initialize_with_zero(&context, zero_budget,
            &progress, &adapter);
    }
};

void exact_order_registers_and_accesses() {
    Fixture f;
    f.inspect_zero_prefix = true;
    f.mutate_saved_ra = true;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.operations == 19 && f.progress.accesses == 7 &&
        f.progress.reads == 3 && f.progress.stores == 4 &&
        f.progress.callbacks_completed == 12 &&
        f.progress.access_events == 7);
    check(f.calls.size() == 12);
    static constexpr std::uint32_t pc[12] = {
        0x8002dbc0u, 0x8002dbc8u, 0x8002dbd0u, 0x8002dbd8u,
        0x8002dbe0u, 0x8002dbe8u, 0x8002dbf0u, 0x8002dbf8u,
        0x8002dc00u, 0x8002dc08u, 0x8002dc10u, 0x8002dc20u
    };
    static constexpr std::uint32_t entry[12] = {
        0x800a3a74u, 0x80063d58u, 0x80029114u, 0x8007fd40u,
        0x800294f8u, 0x8002ab30u, 0x800640d8u, 0x800659f0u,
        0x80065db0u, 0x80031e00u, 0x80038a18u, 0x800763f4u
    };
    for (unsigned i = 0; i < 12; ++i) {
        check(f.calls[i].event.pc == pc[i] &&
            f.calls[i].event.delay_slot_pc == pc[i] + 4u &&
            f.calls[i].event.entry == entry[i] &&
            f.calls[i].event.kind == i + 1u &&
            f.calls[i].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
                pc[i] + 8u &&
            f.calls[i].registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
                FrameSp);
    }
    check(f.calls[0].event.argument_count == 2 &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0x01020304u &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_V1].word ==
            0xa1b2c3d4u &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            MatchState &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            MatchStateSize &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_AT].word ==
            0x80020000u);
    for (unsigned i = 1; i < 10; ++i)
        check(f.calls[i].event.argument_count == 0 &&
            f.calls[i].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
                0x30000000u + i - 1u &&
            f.calls[i].registers.gpr[NBA97_MATCH_INITIALIZE_T0].word ==
                0x50000000u + i - 1u);
    check(f.calls[10].event.argument_count == 1 &&
        f.calls[10].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            UINT32_MAX);
    check(f.calls[11].event.argument_count == 0 &&
        f.calls[11].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            0x3000000au &&
        f.calls[11].registers.gpr[NBA97_MATCH_INITIALIZE_AT].word ==
            0x80020000u && f.get(FinalFlag) == 0);
    check(f.progress.frame_stack_pointer == FrameSp &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == EntrySp &&
        f.progress.restored_return_address.word == 0x81234560u &&
        f.progress.restored_return_address.known_mask == 0x0f &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x81234560u &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0x1000000bu &&
        f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_T0].word ==
            0x5000000bu);
    static constexpr std::uint32_t access_pc[7] = {
        0x8002db94u, 0x8002db9cu, 0x8002dbacu, 0x8002dbb4u,
        0x8002dbbcu, 0x8002dc1cu, 0x8002dc28u
    };
    static constexpr std::uint32_t access_address[7] = {
        Team0, Team1, FrameSp + 0x10u, Snapshot0, Snapshot1, FinalFlag,
        FrameSp + 0x10u
    };
    static constexpr std::size_t access_operation[7] = {1, 2, 3, 4, 5, 17, 19};
    for (unsigned i = 0; i < 7; ++i)
        check(f.journal[i].pc == access_pc[i] &&
            f.journal[i].address == access_address[i] &&
            f.journal[i].operation == access_operation[i] &&
            f.journal[i].width == 4 &&
            f.journal[i].kind ==
                (i < 2 || i == 6 ? NBA97_MATCH_INITIALIZE_READ :
                    NBA97_MATCH_INITIALIZE_STORE));
}

void partial_known_snapshots() {
    Fixture f;
    const std::uint8_t masks[2] = {0x0d, 0x06};
    for (unsigned team = 0; team < 2; ++team)
        for (unsigned byte = 0; byte < 4; ++byte)
            *f.known((team ? Team1 : Team0) + byte) =
                (masks[team] >> byte) & 1u;
    check(f.run() == NBA97_TEXT_COMPLETE);
    check(f.progress.team_snapshot[0].known_mask == masks[0] &&
        f.progress.team_snapshot[1].known_mask == masks[1]);
    for (unsigned byte = 0; byte < 4; ++byte) {
        check(*f.known(Snapshot0 + byte) == ((masks[0] >> byte) & 1u));
        check(*f.known(Snapshot1 + byte) == ((masks[1] >> byte) & 1u));
    }
    check(f.journal[3].known_mask == masks[0] &&
        f.journal[4].known_mask == masks[1]);
}

void recovered_zero_composition() {
    Fixture f;
    std::fill(f.ram.begin() + (MatchState - Ram),
        f.ram.begin() + (MatchState - Ram) + MatchStateSize + 1u,
        static_cast<std::uint8_t>(0x7b));
    f.calls.clear();
    check(f.runAdapted() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.adapter.memory_zero_result == NBA97_TEXT_COMPLETE &&
        f.adapter.memory_zero_invocations == 1 &&
        f.adapter.unresolved_callbacks_completed == 11 &&
        f.adapter.memory_zero.completed &&
        f.adapter.memory_zero.destination == MatchState &&
        f.adapter.memory_zero.requested_length == MatchStateSize &&
        f.adapter.memory_zero.accesses == 928);
    for (std::uint32_t i = 0; i < MatchStateSize; ++i)
        check(*f.byte(MatchState + i) == 0 && *f.known(MatchState + i) == 1);
    check(*f.byte(MatchState + MatchStateSize) == 0x7b);
    check(f.calls.size() == 11 && f.calls[0].event.entry == 0x80063d58u &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            f.adapter.memory_zero.working_destination &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            f.adapter.memory_zero.working_count &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_A2].word == 0 &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_AT].word == 0 &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_T0].word == 4 &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 1].word == 0 &&
        f.calls[0].registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 2].word == 0);

    Fixture bounded;
    std::fill(bounded.ram.begin() + (MatchState - Ram),
        bounded.ram.begin() + (MatchState - Ram) + MatchStateSize,
        static_cast<std::uint8_t>(0x6c));
    check(bounded.runAdapted(7) == NBA97_TEXT_IO_REFUSED &&
        bounded.adapter.memory_zero_result == NBA97_TEXT_LIMIT &&
        bounded.adapter.memory_zero_invocations == 1 &&
        bounded.adapter.memory_zero.operations == 7 &&
        bounded.progress.operations == 6 &&
        bounded.progress.callbacks_completed == 0 && bounded.calls.empty());
    bool changed = false;
    for (std::uint32_t i = 0; i < MatchStateSize; ++i)
        changed = changed || *bounded.byte(MatchState + i) == 0;
    check(changed);

    Fixture before_first_store;
    const auto incoming_t0 = before_first_store.context.registers
        .gpr[NBA97_MATCH_INITIALIZE_T0];
    const auto incoming_t1 = before_first_store.context.registers
        .gpr[NBA97_MATCH_INITIALIZE_T0 + 1];
    check(before_first_store.runAdapted(0) == NBA97_TEXT_IO_REFUSED &&
        before_first_store.adapter.memory_zero_result == NBA97_TEXT_LIMIT &&
        before_first_store.adapter.memory_zero.stores == 0 &&
        before_first_store.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_AT].word == 0 &&
        before_first_store.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_A2].word == 0 &&
        before_first_store.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_T0].word == incoming_t0.word &&
        before_first_store.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_T0].known_mask ==
                incoming_t0.known_mask &&
        before_first_store.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_T0 + 1].word == incoming_t1.word &&
        before_first_store.progress.registers
            .gpr[NBA97_MATCH_INITIALIZE_T0 + 2].word == 0);
}

void callback_refusal_malformed_and_budgets() {
    for (std::size_t fail = 0; fail < 12; ++fail) {
        Fixture f;
        f.refuse_call = fail;
        check(f.run() == NBA97_TEXT_IO_REFUSED && !f.progress.completed &&
            f.calls.size() == fail + 1u &&
            f.progress.callbacks_completed == fail &&
            f.progress.stopped_pc == f.calls.back().event.pc &&
            f.progress.stopped_entry == f.calls.back().event.entry);
        check((fail == 11) == (f.get(FinalFlag) == 0));
    }
    Fixture malformed;
    malformed.malformed_call = 4;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.calls.size() == 5 &&
        malformed.progress.callbacks_completed == 4);

    for (std::size_t budget = 0; budget < 19; ++budget) {
        Fixture f;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations == budget);
    }
    Fixture exact;
    exact.context.operation_budget = 19;
    check(exact.run() == NBA97_TEXT_COMPLETE && exact.progress.operations == 19);

    Fixture before_zero;
    before_zero.context.operation_budget = 5;
    check(before_zero.run() == NBA97_TEXT_LIMIT && before_zero.calls.empty() &&
        before_zero.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8002dbc8u &&
        before_zero.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            MatchStateSize);
    Fixture before_minus_one;
    before_minus_one.context.operation_budget = 15;
    check(before_minus_one.run() == NBA97_TEXT_LIMIT &&
        before_minus_one.calls.size() == 10 &&
        before_minus_one.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8002dc18u &&
        before_minus_one.progress.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            UINT32_MAX);
    Fixture before_final;
    before_final.context.operation_budget = 17;
    check(before_final.run() == NBA97_TEXT_LIMIT &&
        before_final.calls.size() == 11 && before_final.get(FinalFlag) == 0 &&
        before_final.progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8002dc28u);
}

void mapping_alignment_unknown_and_arguments() {
    Fixture no_stack;
    no_stack.context.memory.count = 1;
    check(no_stack.run() == NBA97_TEXT_RESOURCE &&
        no_stack.progress.operations == 3 &&
        no_stack.progress.stopped_pc == 0x8002dbacu &&
        no_stack.progress.stopped_address == FrameSp + 0x10u);

    Fixture misaligned;
    misaligned.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
        EntrySp + 2u;
    check(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.stopped_pc == 0x8002dbacu);

    Fixture unknown_sp;
    unknown_sp.context.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 7;
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.operations == 2 &&
        unknown_sp.progress.stopped_pc == 0x8002dba0u);

    Fixture malformed_known;
    *malformed_known.known(Team0 + 2u) = 2;
    check(malformed_known.run() == NBA97_TEXT_ARGUMENT &&
        malformed_known.progress.operations == 1 &&
        malformed_known.progress.stopped_pc == 0x8002db94u);

    Fixture unknown_ra;
    unknown_ra.context.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 3;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.operations == 19 &&
        unknown_ra.progress.stopped_pc == 0x8002dc30u &&
        unknown_ra.progress.restored_return_address.known_mask == 3 &&
        unknown_ra.progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp);

    Fixture before_first_load;
    before_first_load.context.operation_budget = 0;
    check(before_first_load.run() == NBA97_TEXT_LIMIT &&
        before_first_load.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0x80020000u);

    Fixture missing_second;
    auto* team_data = missing_second.ram.data() + (Team0 - Ram);
    auto* team_known = missing_second.ram_known.data() + (Team0 - Ram);
    missing_second.regions[0] = {Team0, team_data, team_known, 4};
    check(missing_second.run() == NBA97_TEXT_RESOURCE &&
        missing_second.progress.operations == 2 &&
        missing_second.progress.registers.gpr[NBA97_MATCH_INITIALIZE_V1].word ==
            0x80020000u);

    Fixture before_snapshot;
    before_snapshot.context.operation_budget = 3;
    check(before_snapshot.run() == NBA97_TEXT_LIMIT &&
        before_snapshot.progress.registers.gpr[NBA97_MATCH_INITIALIZE_AT].word ==
            0x80020000u);

    Fixture overlap;
    overlap.regions[1].base = 0x80000010u;
    check(overlap.run() == NBA97_TEXT_ARGUMENT && overlap.progress.operations == 0);

    Fixture bad_zero;
    bad_zero.context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word = 1;
    check(bad_zero.run() == NBA97_TEXT_ARGUMENT);

    Fixture bad_journal;
    bad_journal.context.access_journal = nullptr;
    bad_journal.context.access_journal_capacity = 1;
    check(bad_journal.run() == NBA97_TEXT_ARGUMENT);

    Nba97GameMatchInitializeContext context{};
    check(nba97_game_match_initialize(nullptr, nullptr) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_match_initialize(&context, nullptr) == NBA97_TEXT_ARGUMENT);
    Nba97GameMatchInitializeProgress progress{};
    check(nba97_game_match_initialize(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    exact_order_registers_and_accesses();
    partial_known_snapshots();
    recovered_zero_composition();
    callback_refusal_malformed_and_budgets();
    mapping_alignment_unknown_and_arguments();
    std::printf("%u game match-initialize checks passed\n", checks);
    return 0;
}
