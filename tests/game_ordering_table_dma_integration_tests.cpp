#include "game_ordering_table_dma_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "ordering-table DMA integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)
using U32 = std::uint32_t;

struct Block {
    U32 base;
    std::vector<std::uint8_t> data;
    std::vector<std::uint8_t> known;
    Nba97GameTextRegion region{};
    Block(U32 address, std::size_t size) : base(address), data(size),
        known(size, 1) {
        region = {base, data.data(), known.data(), data.size()};
    }
    void put(U32 address, U32 value, unsigned width = 4) {
        const std::size_t at = address - base;
        for (unsigned i = 0; i < width; ++i) {
            data[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
            known[at + i] = 1;
        }
    }
    U32 get(U32 address) const {
        U32 value = 0;
        const std::size_t at = address - base;
        for (unsigned i = 0; i < 4; ++i)
            value |= U32(data[at + i]) << (8u * i);
        return value;
    }
};

struct Fixture {
    static constexpr U32 AddressRegister = 0x1f8010e0u;
    static constexpr U32 CountRegister = 0x1f8010e4u;
    static constexpr U32 ControlRegister = 0x1f8010e8u;
    static constexpr U32 MasterRegister = 0x1f8010f0u;
    Block globals{0x800c5578u, 0x13cu};
    Block stack{0x80100000u, 512};
    Block ot{0x800f5c50u, 0x5000};
    Block mmio{0x1f8010e0u, 20};
    std::array<Nba97GameTextRegion, 4> regions{};
    std::array<Nba97GameOrderingTableDmaAccess, 64> dma_journal{};
    std::vector<Nba97GameClearOrderingTableEvent> fallback_events;
    std::vector<Nba97GameOrderingTableDmaEvent> dma_events;
    Nba97GameClearOrderingTableContext parent{};
    Nba97GameClearOrderingTableProgress parent_progress{};
    Nba97GameOrderingTableDmaBinding binding{};
    bool clear_immediately = true;
    bool wait_error = false;
    unsigned malformed_child_kind = 0;

    Fixture(U32 debug = 0, U32 count = 4096) {
        regions = {globals.region, stack.region, ot.region, mmio.region};
        globals.put(0x800c55c2u, debug, 1);
        globals.put(0x800c55bcu, 0x8009cb2cu);
        globals.put(0x800c55b8u, 0x800c5578u);
        globals.put(0x800c5578u + 0x2cu, 0x8009a97cu);
        globals.put(0x800c56a4u, AddressRegister);
        globals.put(0x800c56a8u, CountRegister);
        globals.put(0x800c56acu, ControlRegister);
        globals.put(0x800c56b0u, MasterRegister);
        mmio.put(MasterRegister, 0x12345678u);
        mmio.put(ControlRegister, 0x55667788u);
        for (unsigned r = 0; r < 32; ++r)
            parent.machine.registers.gpr[r] =
                {0x30000000u + r * 0x101u, 0x0f};
        parent.machine.registers.gpr[0] = {0, 0x0f};
        parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            {ot.base, 0x0f};
        parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
            {count, 0x0f};
        parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {0x80100100u, 0x0f};
        parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x8004909cu, 0x0f};
        parent.machine.hi = {0x89abcdefu, 0x0f};
        parent.machine.lo = {0x76543210u, 0x0f};
        parent.memory = {regions.data(), regions.size()};
        parent.operation_budget = 100;
        parent.io = fallback;
        parent.user = this;
        binding.operation_budget = 100;
        binding.io = dmaIo;
        binding.user = this;
        binding.access_journal = dma_journal.data();
        binding.access_journal_capacity = dma_journal.size();
    }

    static void putMemory(const Nba97GameTextMemory* memory, U32 address,
        U32 value) {
        for (std::size_t r = 0; r < memory->count; ++r) {
            auto& region = memory->region[r];
            if (address >= region.base &&
                std::uint64_t(address) + 4 <=
                    std::uint64_t(region.base) + region.size) {
                const std::size_t at = address - region.base;
                for (unsigned i = 0; i < 4; ++i) {
                    region.data[at + i] =
                        static_cast<std::uint8_t>(value >> (8u * i));
                    if (region.known)
                        region.known[at + i] = 1;
                }
                return;
            }
        }
        std::abort();
    }

    static int fallback(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameClearOrderingTableEvent* event,
        Nba97GameClearOrderingTableMachine*) {
        auto& f = *static_cast<Fixture*>(opaque);
        f.fallback_events.push_back(*event);
        return 1;
    }

    static int dmaIo(void* opaque, const Nba97GameTextMemory* memory,
        const Nba97GameOrderingTableDmaEvent* event,
        Nba97GameOrderingTableDmaMachine* machine) {
        auto& f = *static_cast<Fixture*>(opaque);
        f.dma_events.push_back(*event);
        if (event->kind == NBA97_GAME_ORDERING_TABLE_DMA_START) {
            if (f.clear_immediately)
                putMemory(memory, ControlRegister, 0x10000002u);
        } else {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
                {f.wait_error ? 3u : 0u, 0x0f};
            if (!f.wait_error)
                putMemory(memory, ControlRegister, 0x10000002u);
        }
        if (f.malformed_child_kind == event->kind)
            machine->registers.gpr[0].known_mask = 0;
        return 1;
    }

    int run() {
        return nba97_game_clear_ordering_table_with_dma(
            &parent, &binding, &parent_progress);
    }
};

void actualDynamicParent() {
    Fixture f;
    check(f.run() == NBA97_TEXT_COMPLETE && f.parent_progress.completed &&
        f.binding.invocations == 1 && f.binding.completions == 1 &&
        f.binding.result == NBA97_TEXT_COMPLETE &&
        f.binding.event.pc == 0x800999bcu &&
        f.binding.event.delay_slot_pc == 0x800999c0u &&
        f.binding.event.target == 0x8009a97cu &&
        f.binding.event.argument_count == 2);
    check(f.dma_events.size() == 1 &&
        f.dma_events[0].pc == 0x8009a9f0u &&
        f.dma_events[0].entry == 0x8009bafcu &&
        f.dma_events[0].argument_count == 0);
    check(f.mmio.get(Fixture::AddressRegister) ==
            f.ot.base + (4096u << 2u) - 4u &&
        f.mmio.get(Fixture::CountRegister) == 4096u &&
        f.ot.get(f.ot.base) == 0x000c567cu);
    check(f.binding.progress.return_v0.word == 4096u &&
        f.parent_progress.return_v0.word == f.ot.base &&
        f.parent_progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].word == f.ot.base &&
        f.parent_progress.restored_return_address.word == 0x8004909cu);
    check(f.fallback_events.empty() &&
        f.parent_progress.callbacks_completed == 1 &&
        f.binding.fallback_callbacks_completed == 0);
}

void debugAndBackendErrorReturn() {
    Fixture debug(2, 32);
    debug.clear_immediately = false;
    debug.wait_error = true;
    check(debug.run() == NBA97_TEXT_COMPLETE &&
        debug.fallback_events.size() == 1 &&
        debug.fallback_events[0].kind ==
            NBA97_GAME_CLEAR_ORDERING_TABLE_DEBUG &&
        debug.fallback_events[0].target == 0x8009cb2cu &&
        debug.binding.fallback_callbacks_completed == 1);
    check(debug.dma_events.size() == 2 &&
        debug.dma_events[1].kind == NBA97_GAME_ORDERING_TABLE_DMA_WAIT &&
        debug.binding.progress.return_v0.word == 0xffffffffu &&
        debug.parent_progress.return_v0.word == debug.ot.base &&
        debug.ot.get(debug.ot.base) == 0x000c567cu);
}

void exactEventValidationAndFallback() {
    Fixture f;
    Nba97GameClearOrderingTableEvent event{};
    event.pc = 0x800999bcu;
    event.delay_slot_pc = 0x800999c0u;
    event.target = 0x8009a97cu;
    event.kind = NBA97_GAME_CLEAR_ORDERING_TABLE_BACKEND;
    event.argument_count = 2;
    auto machine = f.parent.machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {0x800999c4u, 0x0f};
    check(nba97_game_ordering_table_dma_from_clear_ordering_table(
        &f.binding, &f.parent.memory, &event, &machine) == 1 &&
        f.binding.completions == 1);
    for (unsigned field = 0; field < 5; ++field) {
        Fixture bad;
        auto changed = event;
        if (field == 0) changed.pc += 4;
        if (field == 1) changed.delay_slot_pc += 4;
        if (field == 2) changed.target += 4;
        if (field == 3) changed.kind = NBA97_GAME_CLEAR_ORDERING_TABLE_DEBUG;
        if (field == 4) changed.argument_count = 3;
        auto bad_machine = bad.parent.machine;
        check(nba97_game_ordering_table_dma_from_clear_ordering_table(
            &bad.binding, &bad.parent.memory, &changed, &bad_machine) == 0 &&
            bad.binding.result == NBA97_TEXT_ARGUMENT &&
            bad.binding.invocations == 0);
    }

    Fixture other;
    other.globals.put(0x800c5578u + 0x2cu, 0x8009aa00u);
    check(other.run() == NBA97_TEXT_COMPLETE &&
        other.binding.invocations == 0 &&
        other.binding.fallback_callbacks_completed == 1 &&
        other.fallback_events.size() == 1 &&
        other.fallback_events[0].target == 0x8009aa00u &&
        other.ot.get(other.ot.base) == 0x000c567cu);
}

void nestedFailurePrefixAndArguments() {
    Fixture malformed;
    malformed.malformed_child_kind = NBA97_GAME_ORDERING_TABLE_DMA_START;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.binding.result == NBA97_TEXT_ARGUMENT &&
        malformed.binding.progress.stopped_pc == 0x8009a9f0u &&
        malformed.binding.progress.machine.registers.gpr[0].known_mask == 0 &&
        malformed.parent_progress.stopped_pc == 0x800999bcu &&
        malformed.ot.get(malformed.ot.base) == 0);

    Fixture malformed_wait;
    malformed_wait.clear_immediately = false;
    malformed_wait.malformed_child_kind = NBA97_GAME_ORDERING_TABLE_DMA_WAIT;
    check(malformed_wait.run() == NBA97_TEXT_ARGUMENT &&
        malformed_wait.binding.result == NBA97_TEXT_ARGUMENT &&
        malformed_wait.binding.progress.stopped_pc == 0x8009aa1cu &&
        malformed_wait.binding.progress.machine.registers.gpr[0].known_mask ==
            0 && malformed_wait.parent_progress.stopped_pc == 0x800999bcu &&
        malformed_wait.ot.get(malformed_wait.ot.base) == 0);

    Fixture limited;
    limited.binding.operation_budget = 0;
    check(limited.run() == NBA97_TEXT_IO_REFUSED &&
        limited.binding.result == NBA97_TEXT_LIMIT &&
        limited.binding.progress.operations == 0 &&
        limited.parent_progress.stopped_pc == 0x800999bcu);

    Fixture args;
    Nba97GameClearOrderingTableProgress progress{};
    check(nba97_game_clear_ordering_table_with_dma(
        nullptr, &args.binding, &progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_clear_ordering_table_with_dma(
        &args.parent, nullptr, &progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_clear_ordering_table_with_dma(
        &args.parent, &args.binding, nullptr) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_ordering_table_dma_from_clear_ordering_table(
        nullptr, nullptr, nullptr, nullptr) == 0);

    Fixture invalid_entry;
    Nba97GameClearOrderingTableEvent event{};
    event.pc = 0x800999bcu;
    event.delay_slot_pc = 0x800999c0u;
    event.target = 0x8009a97cu;
    event.kind = NBA97_GAME_CLEAR_ORDERING_TABLE_BACKEND;
    event.argument_count = 2;
    auto invalid_machine = invalid_entry.parent.machine;
    invalid_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {0x800999c4u, 0x0f};
    invalid_machine.hi.known_mask = 16;
    const auto preserved_invalid = invalid_machine;
    check(nba97_game_ordering_table_dma_from_clear_ordering_table(
        &invalid_entry.binding, &invalid_entry.parent.memory, &event,
        &invalid_machine) == 0 && invalid_entry.binding.invocations == 0 &&
        invalid_entry.binding.result == NBA97_TEXT_ARGUMENT &&
        std::memcmp(&invalid_machine, &preserved_invalid,
            sizeof invalid_machine) == 0);

    Fixture wrong_ra;
    auto wrong_ra_machine = wrong_ra.parent.machine;
    wrong_ra_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {0x800999c8u, 0x0f};
    const auto preserved_ra = wrong_ra_machine;
    check(nba97_game_ordering_table_dma_from_clear_ordering_table(
        &wrong_ra.binding, &wrong_ra.parent.memory, &event,
        &wrong_ra_machine) == 0 && wrong_ra.binding.invocations == 0 &&
        wrong_ra.binding.result == NBA97_TEXT_ARGUMENT &&
        std::memcmp(&wrong_ra_machine, &preserved_ra,
            sizeof wrong_ra_machine) == 0);
}
}

int main() {
    actualDynamicParent();
    debugAndBackendErrorReturn();
    exactEventValidationAndFallback();
    nestedFailurePrefixAndArguments();
    std::printf("%u ordering-table DMA integration checks passed\n", checks);
    return 0;
}
