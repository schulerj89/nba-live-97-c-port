#include "recovered/feload_entry.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

unsigned checks;

void check(bool value) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "feload entry check %u failed\n", checks);
        std::abort();
    }
}

constexpr std::uint32_t Base = 0x801e8000u;
constexpr std::uint32_t Bss = 0x801e903cu;
constexpr std::uint32_t BssEnd = 0x801eb088u;
constexpr std::uint32_t MemoryTop = 0x801e8b70u;
constexpr std::uint32_t StackReserve = 0x801e8b6cu;
constexpr std::uint32_t HeapSize = 0x801e8b50u;
constexpr std::uint32_t HeapBase = 0x801e8b4cu;
constexpr std::size_t BssWords = (BssEnd - Bss) / 4u;

struct Fixture {
    enum Mode {
        TransferSecond,
        ReturnSecond,
        TransferFirst,
        RefuseFirst,
        RefuseSecond,
        InvalidFirstOutcome,
        InvalidSecondOutcome,
        InvalidFirstRegisters,
        UnknownSavedRa
    } mode = TransferSecond;

    std::vector<std::uint8_t> storage =
        std::vector<std::uint8_t>(0x4001u, 0xcd);
    std::vector<std::uint8_t> known_storage =
        std::vector<std::uint8_t>(0x4001u, 1);
    Nba97GameTextRegion region{Base, storage.data(), known_storage.data(),
        0x4000u};
    Nba97FeloadEntryContext context{};
    Nba97FeloadEntryProgress progress{};
    std::vector<Nba97FeloadEntryAccess> accesses;
    std::vector<Nba97FeloadEntryEvent> calls;

    Fixture() {
        context.memory = {&region, 1};
        context.operation_budget = 10000;
        context.io = io;
        context.observe_access = observe;
        context.user = this;
        for (unsigned i = 1; i < NBA97_FELOAD_REGISTER_COUNT; ++i) {
            context.registers.gpr[i].word = 0x11000000u + i;
            context.registers.gpr[i].known = 1;
        }
        context.registers.gpr[NBA97_FELOAD_R_RA] = {0x12345678u, 1};
        context.registers.gpr[NBA97_FELOAD_R_A2] = {0xa2a2a2a2u, 1};
        context.registers.gpr[NBA97_FELOAD_R_A3] = {0xa3a3a3a3u, 1};
        put(MemoryTop, 0x00800000u);
        put(StackReserve, 0x00008000u);
    }

    std::uint8_t* data(std::uint32_t address) {
        check(address >= region.base &&
            std::uint64_t(address - region.base) < region.size);
        return region.data + (address - region.base);
    }

    std::uint8_t* known(std::uint32_t address) {
        check(region.known != nullptr);
        return region.known + (address - region.base);
    }

    void put(std::uint32_t address, std::uint32_t value,
        std::uint8_t value_known = 1) {
        for (unsigned i = 0; i < 4; ++i) {
            data(address)[i] = std::uint8_t(value >> (i * 8u));
            known(address)[i] = value_known;
        }
    }

    std::uint32_t get(std::uint32_t address) {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(data(address)[i]) << (i * 8u);
        return value;
    }

    static void observe(void* user, const Nba97FeloadEntryAccess* access) {
        static_cast<Fixture*>(user)->accesses.push_back(*access);
    }

    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97FeloadEntryEvent* event,
        Nba97FeloadEntryRegisters* returned,
        Nba97FeloadEntryCalleeOutcome* outcome) {
        auto& fixture = *static_cast<Fixture*>(user);
        fixture.calls.push_back(*event);
        if (event->kind == NBA97_FELOAD_ENTRY_CHILD_801E1590) {
            if (fixture.mode == RefuseFirst)
                return 0;
            if (fixture.mode == InvalidFirstOutcome) {
                *outcome = static_cast<Nba97FeloadEntryCalleeOutcome>(9);
                return 1;
            }
            if (fixture.mode == InvalidFirstRegisters) {
                returned->gpr[NBA97_FELOAD_R_V0].known = 2;
                *outcome = NBA97_FELOAD_ENTRY_CALLEE_RETURNED;
                return 1;
            }
            if (fixture.mode == TransferFirst) {
                returned->gpr[NBA97_FELOAD_R_V0] = {0xface0001u, 1};
                *outcome = NBA97_FELOAD_ENTRY_CALLEE_TRANSFERRED;
                return 1;
            }
            if (fixture.mode == UnknownSavedRa) {
                for (unsigned i = 0; i < 4; ++i)
                    *fixture.known(Bss + i) = 0;
            } else {
                fixture.put(Bss, 0xcafebabeu);
            }
            returned->gpr[NBA97_FELOAD_R_V0] = {0x02020202u, 1};
            returned->gpr[NBA97_FELOAD_R_V1] = {0x03030303u, 1};
            returned->gpr[NBA97_FELOAD_R_A0] = {0x04040404u, 1};
            returned->gpr[NBA97_FELOAD_R_A1] = {0x05050505u, 1};
            returned->gpr[NBA97_FELOAD_R_A2] = {0x06060606u, 1};
            returned->gpr[NBA97_FELOAD_R_A3] = {0x07070707u, 1};
            returned->gpr[NBA97_FELOAD_R_T1] = {0x09090909u, 1};
            returned->gpr[NBA97_FELOAD_R_GP] = {0x1c1c1c1cu, 1};
            returned->gpr[NBA97_FELOAD_R_SP] = {0x1d1d1d1du, 1};
            returned->gpr[NBA97_FELOAD_R_S8] = {0x1e1e1e1eu, 1};
            *outcome = NBA97_FELOAD_ENTRY_CALLEE_RETURNED;
            return 1;
        }
        if (fixture.mode == RefuseSecond)
            return 0;
        if (fixture.mode == InvalidSecondOutcome) {
            *outcome = static_cast<Nba97FeloadEntryCalleeOutcome>(9);
            return 1;
        }
        returned->gpr[NBA97_FELOAD_R_V0] = {0xfeed0002u, 1};
        *outcome = fixture.mode == ReturnSecond
            ? NBA97_FELOAD_ENTRY_CALLEE_RETURNED
            : NBA97_FELOAD_ENTRY_CALLEE_TRANSFERRED;
        return 1;
    }

    int run() {
        return nba97_feload_entry(&context, &progress);
    }
};

void transferred_path_and_order() {
    Fixture fixture;
    check(fixture.run() == NBA97_TEXT_COMPLETE);
    check(fixture.progress.completed && fixture.progress.transferred &&
        !fixture.progress.trapped && fixture.progress.first_child_entered &&
        fixture.progress.second_child_entered &&
        fixture.progress.delay_slot_completed);
    check(fixture.progress.words_cleared == BssWords &&
        fixture.progress.stores == BssWords + 3u &&
        fixture.progress.reads == 3 &&
        fixture.progress.accesses == BssWords + 6u &&
        fixture.progress.operations == BssWords + 8u &&
        fixture.progress.callbacks_completed == 2);
    check(fixture.progress.heap_base == BssEnd &&
        fixture.progress.heap_size == 0x0060cf70u &&
        fixture.get(HeapBase) == BssEnd &&
        fixture.get(HeapSize) == 0x0060cf70u);
    check(fixture.progress.saved_return_address.word == 0x12345678u &&
        fixture.progress.saved_return_address.known &&
        fixture.progress.restored_return_address.word == 0xcafebabeu &&
        fixture.progress.restored_return_address.known &&
        fixture.get(Bss) == 0xcafebabeu);
    for (std::uint32_t address = Bss + 4u; address < BssEnd;
         address += 4u)
        check(fixture.get(address) == 0);

    check(fixture.accesses.size() == BssWords + 6u);
    for (std::size_t i = 0; i < BssWords; ++i) {
        const auto& access = fixture.accesses[i];
        check(access.kind == NBA97_FELOAD_ENTRY_WRITE &&
            access.pc == 0x801e1420u && access.address == Bss + i * 4u &&
            access.value == 0 && access.width == 4 &&
            access.known_mask == 0x0fu);
    }
    check(fixture.accesses[BssWords].kind == NBA97_FELOAD_ENTRY_READ &&
        fixture.accesses[BssWords].pc == 0x801e1438u &&
        fixture.accesses[BssWords].address == MemoryTop);
    check(fixture.accesses[BssWords + 1u].pc == 0x801e1460u &&
        fixture.accesses[BssWords + 1u].address == StackReserve);
    check(fixture.accesses[BssWords + 2u].pc == 0x801e1474u &&
        fixture.accesses[BssWords + 2u].address == HeapSize);
    check(fixture.accesses[BssWords + 3u].pc == 0x801e1480u &&
        fixture.accesses[BssWords + 3u].address == HeapBase);
    check(fixture.accesses[BssWords + 4u].pc == 0x801e1488u &&
        fixture.accesses[BssWords + 4u].address == Bss &&
        fixture.accesses[BssWords + 4u].value == 0x12345678u);
    check(fixture.accesses[BssWords + 5u].kind == NBA97_FELOAD_ENTRY_READ &&
        fixture.accesses[BssWords + 5u].pc == 0x801e14a4u &&
        fixture.accesses[BssWords + 5u].address == Bss &&
        fixture.accesses[BssWords + 5u].value == 0xcafebabeu);

    check(fixture.calls.size() == 2);
    const auto& first = fixture.calls[0];
    check(first.kind == NBA97_FELOAD_ENTRY_CHILD_801E1590 &&
        first.pc == 0x801e1498u && first.entry == 0x801e1590u &&
        first.argument_count == 2);
    check(first.registers.gpr[NBA97_FELOAD_R_A0].word == 0x801eb08cu &&
        first.registers.gpr[NBA97_FELOAD_R_A1].word == 0x0060cf70u &&
        first.registers.gpr[NBA97_FELOAD_R_A2].word == 0xa2a2a2a2u &&
        first.registers.gpr[NBA97_FELOAD_R_A3].word == 0xa3a3a3a3u &&
        first.registers.gpr[NBA97_FELOAD_R_V0].word == 0x007ffff8u &&
        first.registers.gpr[NBA97_FELOAD_R_V1].word == 0x00008000u &&
        first.registers.gpr[NBA97_FELOAD_R_T0].word == 0x80000000u);
    check(first.registers.gpr[NBA97_FELOAD_R_SP].word == 0x807ffff8u &&
        first.registers.gpr[NBA97_FELOAD_R_GP].word == Bss &&
        first.registers.gpr[NBA97_FELOAD_R_S8].word == 0x807ffff8u &&
        first.registers.gpr[NBA97_FELOAD_R_RA].word == 0x801e14a0u);

    const auto& second = fixture.calls[1];
    check(second.kind == NBA97_FELOAD_ENTRY_CHILD_801E136C &&
        second.pc == 0x801e14acu && second.entry == 0x801e136cu &&
        second.argument_count == 0);
    check(second.registers.gpr[NBA97_FELOAD_R_V0].word == 0x02020202u &&
        second.registers.gpr[NBA97_FELOAD_R_A0].word == 0x04040404u &&
        second.registers.gpr[NBA97_FELOAD_R_A1].word == 0x05050505u &&
        second.registers.gpr[NBA97_FELOAD_R_A2].word == 0x06060606u &&
        second.registers.gpr[NBA97_FELOAD_R_A3].word == 0x07070707u &&
        second.registers.gpr[NBA97_FELOAD_R_T1].word == 0x09090909u);
    check(second.registers.gpr[NBA97_FELOAD_R_GP].word == 0x1c1c1c1cu &&
        second.registers.gpr[NBA97_FELOAD_R_SP].word == 0x1d1d1d1du &&
        second.registers.gpr[NBA97_FELOAD_R_S8].word == 0x1e1e1e1eu &&
        second.registers.gpr[NBA97_FELOAD_R_RA].word == 0x801e14b4u);
    check(fixture.progress.registers.gpr[NBA97_FELOAD_R_V0].word ==
        0xfeed0002u && !fixture.progress.stopped_pc &&
        !fixture.progress.stopped_address && !fixture.progress.stopped_entry);
}

void terminal_break_and_child_failures() {
    {
        Fixture fixture;
        fixture.mode = Fixture::ReturnSecond;
        check(fixture.run() == NBA97_FELOAD_ENTRY_BREAK_TRAP &&
            fixture.progress.trapped && !fixture.progress.completed &&
            !fixture.progress.transferred && fixture.progress.second_child_entered &&
            fixture.progress.stopped_pc == 0x801e14b4u &&
            fixture.progress.callbacks_completed == 2 &&
            fixture.progress.registers.gpr[NBA97_FELOAD_R_V0].word ==
                0xfeed0002u);
    }
    {
        Fixture fixture;
        fixture.mode = Fixture::TransferFirst;
        check(fixture.run() == NBA97_TEXT_COMPLETE &&
            fixture.progress.completed && fixture.progress.transferred &&
            fixture.progress.first_child_entered &&
            !fixture.progress.second_child_entered &&
            fixture.progress.callbacks_completed == 1 &&
            fixture.progress.reads == 2 &&
            fixture.progress.registers.gpr[NBA97_FELOAD_R_V0].word ==
                0xface0001u);
    }
    {
        Fixture fixture;
        fixture.mode = Fixture::RefuseFirst;
        check(fixture.run() == NBA97_TEXT_IO_REFUSED &&
            !fixture.progress.first_child_entered &&
            fixture.progress.stopped_pc == 0x801e1498u &&
            fixture.progress.stopped_entry == 0x801e1590u &&
            !fixture.progress.callbacks_completed);
    }
    {
        Fixture fixture;
        fixture.mode = Fixture::RefuseSecond;
        check(fixture.run() == NBA97_TEXT_IO_REFUSED &&
            fixture.progress.first_child_entered &&
            !fixture.progress.second_child_entered &&
            fixture.progress.stopped_pc == 0x801e14acu &&
            fixture.progress.stopped_entry == 0x801e136cu &&
            fixture.progress.callbacks_completed == 1);
    }
    for (auto mode : {Fixture::InvalidFirstOutcome,
                      Fixture::InvalidSecondOutcome,
                      Fixture::InvalidFirstRegisters}) {
        Fixture fixture;
        fixture.mode = mode;
        check(fixture.run() == NBA97_TEXT_ARGUMENT &&
            fixture.progress.stopped_pc ==
                (mode == Fixture::InvalidSecondOutcome ? 0x801e14acu
                                                       : 0x801e1498u));
    }
    {
        Fixture fixture;
        fixture.context.io = nullptr;
        check(fixture.run() == NBA97_TEXT_IO_REFUSED &&
            fixture.progress.stopped_pc == 0x801e1498u);
    }
}

void arithmetic_unknown_mapping_and_alignment() {
    for (std::uint32_t top : {0x80000000u, 0x80000007u}) {
        Fixture fixture;
        fixture.put(MemoryTop, top);
        check(fixture.run() == NBA97_FELOAD_ENTRY_ARITHMETIC_TRAP &&
            fixture.progress.trapped &&
            fixture.progress.stopped_pc == 0x801e1440u &&
            fixture.progress.words_cleared == BssWords &&
            fixture.progress.stores == BssWords &&
            fixture.progress.reads == 1);
    }
    {
        Fixture fixture;
        fixture.put(MemoryTop, 8);
        fixture.put(StackReserve, 1);
        check(fixture.run() == NBA97_TEXT_COMPLETE &&
            fixture.progress.heap_size == 0xffe14f77u &&
            fixture.progress.heap_base == BssEnd &&
            fixture.calls[0].registers.gpr[NBA97_FELOAD_R_V0].word == 0 &&
            fixture.calls[0].registers.gpr[NBA97_FELOAD_R_SP].word ==
                0x80000000u);
    }
    {
        Fixture fixture;
        *fixture.known(MemoryTop + 2u) = 0;
        check(fixture.run() == NBA97_TEXT_UNKNOWN &&
            fixture.progress.stopped_pc == 0x801e1438u &&
            fixture.accesses.back().known_mask == 0x0bu);
    }
    {
        Fixture fixture;
        *fixture.known(StackReserve) = 0;
        check(fixture.run() == NBA97_TEXT_UNKNOWN &&
            fixture.progress.stopped_pc == 0x801e1460u);
    }
    {
        Fixture fixture;
        fixture.mode = Fixture::UnknownSavedRa;
        check(fixture.run() == NBA97_TEXT_COMPLETE &&
            fixture.progress.completed && fixture.progress.transferred &&
            fixture.progress.saved_return_address.known &&
            fixture.progress.saved_return_address.word == 0x12345678u &&
            fixture.progress.restored_return_address.known == 0 &&
            fixture.calls.size() == 2 &&
            fixture.calls[1].registers.gpr[NBA97_FELOAD_R_RA].known &&
            fixture.calls[1].registers.gpr[NBA97_FELOAD_R_RA].word ==
                0x801e14b4u && fixture.accesses.back().known_mask == 0);
    }
    {
        Fixture fixture;
        *fixture.known(Bss) = 2;
        check(fixture.run() == NBA97_TEXT_ARGUMENT &&
            fixture.progress.stopped_pc == 0x801e1420u &&
            !fixture.progress.stores);
    }
    {
        Fixture fixture;
        fixture.region.base = Bss + 4u;
        fixture.region.data = fixture.storage.data() + (Bss + 4u - Base);
        fixture.region.known = fixture.known_storage.data() + (Bss + 4u - Base);
        fixture.region.size = 0x100u;
        check(fixture.run() == NBA97_TEXT_RESOURCE &&
            fixture.progress.stopped_pc == 0x801e1420u &&
            fixture.progress.stopped_address == Bss);
    }
    {
        Fixture fixture;
        fixture.region.data = fixture.storage.data() + 1u;
        fixture.region.known = fixture.known_storage.data() + 1u;
        fixture.put(MemoryTop, 0x00800000u);
        fixture.put(StackReserve, 0x8000u);
        check(fixture.run() == NBA97_TEXT_COMPLETE &&
            fixture.progress.completed);
    }
    {
        Fixture fixture;
        Nba97GameTextRegion overlap[2] = {fixture.region, fixture.region};
        fixture.context.memory = {overlap, 2};
        check(fixture.run() == NBA97_TEXT_ARGUMENT &&
            !fixture.progress.operations);
    }
    {
        Fixture fixture;
        fixture.region.known = nullptr;
        fixture.context.registers.gpr[NBA97_FELOAD_R_RA] = {0, 0};
        check(fixture.run() == NBA97_TEXT_UNKNOWN &&
            fixture.progress.stopped_pc == 0x801e1488u &&
            fixture.progress.stores == BssWords + 2u);
    }
}

void every_clear_prefix_and_late_budgets() {
    for (std::size_t budget = 0; budget <= BssWords; ++budget) {
        Fixture fixture;
        fixture.context.operation_budget = budget;
        check(fixture.run() == NBA97_TEXT_LIMIT &&
            fixture.progress.words_cleared == budget &&
            fixture.progress.stores == budget &&
            fixture.progress.operations == budget &&
            fixture.progress.stopped_pc ==
                (budget < BssWords ? 0x801e1420u : 0x801e1438u));
        if (budget)
            check(fixture.get(Bss + std::uint32_t(budget - 1u) * 4u) == 0);
        if (budget < BssWords)
            check(fixture.get(Bss + std::uint32_t(budget) * 4u) ==
                0xcdcdcdcdu);
    }

    struct Late {
        std::size_t budget;
        std::uint32_t pc;
        std::uint32_t address;
        std::uint32_t entry;
        std::size_t callbacks;
    } cases[] = {
        {BssWords + 1u, 0x801e1460u, StackReserve, 0, 0},
        {BssWords + 2u, 0x801e1474u, HeapSize, 0, 0},
        {BssWords + 3u, 0x801e1480u, HeapBase, 0, 0},
        {BssWords + 4u, 0x801e1488u, Bss, 0, 0},
        {BssWords + 5u, 0x801e1498u, 0, 0x801e1590u, 0},
        {BssWords + 6u, 0x801e14a4u, Bss, 0, 1},
        {BssWords + 7u, 0x801e14acu, 0, 0x801e136cu, 1}
    };
    for (const auto& expected : cases) {
        Fixture fixture;
        fixture.context.operation_budget = expected.budget;
        check(fixture.run() == NBA97_TEXT_LIMIT &&
            fixture.progress.operations == expected.budget &&
            fixture.progress.stopped_pc == expected.pc &&
            fixture.progress.stopped_address == expected.address &&
            fixture.progress.stopped_entry == expected.entry &&
            fixture.progress.callbacks_completed == expected.callbacks);
    }
}

void argument_validation() {
    Nba97FeloadEntryProgress progress{};
    check(nba97_feload_entry(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
    Fixture fixture;
    check(nba97_feload_entry(&fixture.context, nullptr) == NBA97_TEXT_ARGUMENT);
    fixture.context.registers.gpr[NBA97_FELOAD_R_S0] = {1, 0};
    check(fixture.run() == NBA97_TEXT_ARGUMENT && !fixture.progress.operations);
    Fixture missing;
    missing.context.memory = {nullptr, 1};
    check(missing.run() == NBA97_TEXT_ARGUMENT);
}

} // namespace

int main() {
    transferred_path_and_order();
    terminal_break_and_child_failures();
    arithmetic_unknown_mapping_and_alignment();
    every_clear_prefix_and_late_budgets();
    argument_validation();
    std::printf("feload_entry: %u checks passed\n", checks);
}
