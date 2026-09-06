#include "recovered/game_ordering_table_dma.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "ordering-table DMA check %u failed at line %u\n",
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
    void put(U32 address, U32 value) {
        const std::size_t at = address - base;
        for (unsigned i = 0; i < 4; ++i) {
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
    Block globals{0x800c56a4u, 16};
    Block mmio{0x1f8010e0u, 20};
    Block stack{0x80100000u, 512};
    Block alternate{0x80101000u, 128};
    std::array<Nba97GameTextRegion, 4> regions{};
    std::array<Nba97GameOrderingTableDmaAccess, 96> journal{};
    std::vector<Nba97GameOrderingTableDmaEvent> events;
    std::vector<Nba97GameOrderingTableDmaMachine> entries;
    Nba97GameOrderingTableDmaContext context{};
    Nba97GameOrderingTableDmaProgress progress{};
    unsigned clear_after_waits = 0;
    unsigned waits = 0;
    bool clear_immediately = true;
    bool error_wait = false;
    bool unknown_wait_result = false;
    bool partial_loop_predicate = false;
    bool reject_start = false;
    bool reject_wait = false;
    bool corrupt_machine = false;
    bool mutate_live_machine = false;
    bool zero_s1 = false;
    bool move_control_after_start = false;
    unsigned control_mask_after_start = 15;

    Fixture(U32 count = 32, U32 object = 0x800fccf0u) {
        regions = {globals.region, mmio.region, stack.region,
            alternate.region};
        globals.put(0x800c56a4u, AddressRegister);
        globals.put(0x800c56a8u, CountRegister);
        globals.put(0x800c56acu, ControlRegister);
        globals.put(0x800c56b0u, MasterRegister);
        mmio.put(MasterRegister, 0x12345678u);
        mmio.put(ControlRegister, 0x55667788u);
        for (unsigned r = 0; r < 32; ++r)
            context.machine.registers.gpr[r] =
                {0x20000000u + r * 0x101u, 0x0f};
        context.machine.registers.gpr[0] = {0, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            {object, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
            {count, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {0x80100100u, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x81234568u, 0x0f};
        context.machine.hi = {0x89abcdefu, 0x0f};
        context.machine.lo = {0x76543210u, 0x0f};
        context.memory = {regions.data(), regions.size()};
        context.operation_budget = 200;
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
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

    static int io(void* opaque, const Nba97GameTextMemory* memory,
        const Nba97GameOrderingTableDmaEvent* event,
        Nba97GameOrderingTableDmaMachine* machine) {
        auto& f = *static_cast<Fixture*>(opaque);
        f.events.push_back(*event);
        f.entries.push_back(*machine);
        if (event->kind == NBA97_GAME_ORDERING_TABLE_DMA_START) {
            if (f.reject_start)
                return 0;
            if (f.clear_immediately)
                putMemory(memory, ControlRegister, 0x10000002u);
            if (f.move_control_after_start) {
                putMemory(memory, 0x800c56acu, 0x1f8010ecu);
                putMemory(memory, 0x1f8010ecu, 0x10000002u);
            }
            for (unsigned byte = 0; byte < 4; ++byte)
                f.mmio.known[ControlRegister - f.mmio.base + byte] =
                    static_cast<std::uint8_t>(
                        (f.control_mask_after_start >> byte) & 1u);
        } else {
            ++f.waits;
            if (f.reject_wait)
                return 0;
            if (f.unknown_wait_result)
                machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
                    {0, 0x0e};
            else if (f.error_wait)
                machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
                    {7, 0x0f};
            else
                machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
                    {0, 0x0f};
            if (f.zero_s1)
                machine->registers.gpr[
                    NBA97_GAME_CLEAR_ORDERING_TABLE_S1] = {0, 0x0f};
            if (f.partial_loop_predicate)
                machine->registers.gpr[
                    NBA97_GAME_CLEAR_ORDERING_TABLE_S1] =
                    {0x01000000u, 0x07};
            if (f.clear_after_waits && f.waits >= f.clear_after_waits)
                putMemory(memory, ControlRegister, 0x10000002u);
        }
        if (f.mutate_live_machine &&
            event->kind == NBA97_GAME_ORDERING_TABLE_DMA_START) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
                {0x80101000u, 0x0f};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] =
                {0x13572468u, 0x0f};
            machine->registers.gpr[
                NBA97_GAME_CLEAR_ORDERING_TABLE_S1] =
                {0x24681357u, 0x0f};
            machine->registers.gpr[12] = {0xdecafbad, 0x0f};
            machine->hi = {0x10203040u, 0x0f};
            machine->lo = {0x50607080u, 0x0f};
            putMemory(memory, 0x80101018u, 0x90000004u);
            putMemory(memory, 0x80101014u, 0x11112222u);
            putMemory(memory, 0x80101010u, 0x33334444u);
        }
        if (f.corrupt_machine)
            machine->hi.known_mask = 16;
        return 1;
    }

    int run() {
        return nba97_game_ordering_table_dma(&context, &progress);
    }
};

void setupCountsAndExactOrder() {
    const std::array<U32, 6> counts{{0, 1, 32, 4096,
        0xffffffffu, 0x80000000u}};
    const std::array<U32, 19> pcs{{
        0x8009a980u, 0x8009a98cu, 0x8009a990u, 0x8009a994u,
        0x8009a998u, 0x8009a9a4u, 0x8009a9acu, 0x8009a9b4u,
        0x8009a9c4u, 0x8009a9ccu, 0x8009a9d4u, 0x8009a9dcu,
        0x8009a9e4u, 0x8009a9ecu, 0x8009a9fcu, 0x8009aa04u,
        0x8009aa4cu, 0x8009aa50u, 0x8009aa54u}};
    for (U32 count : counts) {
        Fixture f(count);
        check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
            f.progress.operations == 20 && f.progress.access_events == 19);
        check(f.mmio.get(Fixture::MasterRegister) ==
                (0x12345678u | 0x08000000u) &&
            f.mmio.get(Fixture::AddressRegister) ==
                0x800fccf0u + (count << 2u) - 4u &&
            f.mmio.get(Fixture::CountRegister) == count &&
            f.mmio.get(Fixture::ControlRegister) == 0x10000002u);
        check(f.progress.transfer_start.word ==
                0x800fccf0u + (count << 2u) - 4u &&
            f.progress.transfer_count.word == count &&
            f.progress.started_channel_control.word == 0x11000002u &&
            f.progress.return_v0.word == count &&
            f.progress.machine.registers.gpr[
                NBA97_MATCH_INITIALIZE_V0].word == count);
        check(f.events.size() == 1 &&
            f.events[0].kind == NBA97_GAME_ORDERING_TABLE_DMA_START &&
            f.events[0].pc == 0x8009a9f0u &&
            f.events[0].delay_slot_pc == 0x8009a9f4u &&
            f.events[0].entry == 0x8009bafcu &&
            f.events[0].argument_count == 0 &&
            f.entries[0].registers.gpr[
                NBA97_MATCH_INITIALIZE_RA].word == 0x8009a9f8u);
        check(f.entries[0].registers.gpr[
                NBA97_MATCH_INITIALIZE_A0].word ==
                0x800fccf0u + (count << 2u) - 4u &&
            f.entries[0].registers.gpr[
                NBA97_MATCH_INITIALIZE_A1].word == Fixture::MasterRegister &&
            f.entries[0].registers.gpr[
                NBA97_MATCH_INITIALIZE_S0].word == count &&
            f.entries[0].registers.gpr[
                NBA97_GAME_CLEAR_ORDERING_TABLE_S1].word == 0x20001111u);
        for (unsigned i = 0; i < pcs.size(); ++i)
            check(f.journal[i].pc == pcs[i] &&
                f.journal[i].operation == i + 1u + (i >= 14u));
    }
}

void busyWaitAndErrorPaths() {
    for (unsigned waits : std::array<unsigned, 3>{{1, 2, 5}}) {
        Fixture f;
        f.clear_immediately = false;
        f.clear_after_waits = waits;
        check(f.run() == NBA97_TEXT_COMPLETE &&
            f.progress.wait_iterations == waits && f.waits == waits &&
            f.events.size() == waits + 1u &&
            f.progress.return_v0.word == 32u);
        for (unsigned i = 1; i < f.events.size(); ++i)
            check(f.events[i].kind == NBA97_GAME_ORDERING_TABLE_DMA_WAIT &&
                f.events[i].pc == 0x8009aa1cu &&
                f.events[i].delay_slot_pc == 0x8009aa20u &&
                f.events[i].entry == 0x8009bb30u &&
                f.events[i].argument_count == 0 &&
                f.entries[i].registers.gpr[
                    NBA97_MATCH_INITIALIZE_RA].word == 0x8009aa24u);
    }

    Fixture error;
    error.clear_immediately = false;
    error.error_wait = true;
    check(error.run() == NBA97_TEXT_COMPLETE &&
        error.progress.wait_iterations == 1 &&
        error.progress.last_wait_result.word == 7 &&
        error.progress.return_v0.word == 0xffffffffu &&
        error.mmio.get(Fixture::ControlRegister) == 0x11000002u);

    Fixture live_mask;
    live_mask.clear_immediately = false;
    live_mask.zero_s1 = true;
    check(live_mask.run() == NBA97_TEXT_COMPLETE &&
        live_mask.progress.wait_iterations == 1 &&
        live_mask.progress.last_busy_mask.word == 0 &&
        live_mask.mmio.get(Fixture::ControlRegister) == 0x11000002u);

    Fixture moved;
    moved.clear_immediately = false;
    moved.move_control_after_start = true;
    check(moved.run() == NBA97_TEXT_COMPLETE && moved.waits == 0 &&
        moved.globals.get(0x800c56acu) == 0x1f8010ecu);
    bool reloaded_moved_control = false;
    for (std::size_t i = 0; i < moved.progress.access_events; ++i)
        if (moved.journal[i].pc == 0x8009aa04u &&
            moved.journal[i].address == 0x1f8010ecu)
            reloaded_moved_control = true;
    check(reloaded_moved_control);

    Fixture unknown_result;
    unknown_result.clear_immediately = false;
    unknown_result.unknown_wait_result = true;
    check(unknown_result.run() == NBA97_TEXT_UNKNOWN &&
        unknown_result.progress.stopped_pc == 0x8009aa24u &&
        unknown_result.progress.last_wait_result.known_mask == 0x0eu &&
        unknown_result.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].word == 0xffffffffu &&
        unknown_result.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0fu);

    Fixture unknown_loop;
    unknown_loop.clear_immediately = false;
    unknown_loop.partial_loop_predicate = true;
    check(unknown_loop.run() == NBA97_TEXT_UNKNOWN &&
        unknown_loop.progress.stopped_pc == 0x8009aa44u &&
        unknown_loop.progress.last_wait_result.word == 0 &&
        unknown_loop.progress.last_wait_result.known_mask == 0x0fu &&
        unknown_loop.progress.last_busy_mask.known_mask == 0x07u &&
        unknown_loop.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].word == 32u &&
        unknown_loop.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_V0].known_mask == 0x0fu);
}

void liveMachineAndKnownness() {
    Fixture live;
    live.mutate_live_machine = true;
    check(live.run() == NBA97_TEXT_COMPLETE &&
        live.progress.return_v0.word == 0x13572468u &&
        live.progress.restored_return_address.word == 0x90000004u &&
        live.progress.restored_s1.word == 0x11112222u &&
        live.progress.restored_s0.word == 0x33334444u &&
        live.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == 0x80101020u &&
        live.progress.machine.registers.gpr[12].word == 0xdecafbadu &&
        live.progress.machine.hi.word == 0x10203040u &&
        live.progress.machine.lo.word == 0x50607080u);

    for (unsigned mask = 0; mask < 16; ++mask) {
        Fixture count;
        count.context.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_A1].known_mask =
                static_cast<std::uint8_t>(mask);
        const int result = count.run();
        check(result == NBA97_TEXT_COMPLETE &&
            count.progress.transfer_count.known_mask == mask);

        Fixture busy;
        busy.clear_immediately = false;
        busy.error_wait = true;
        busy.control_mask_after_start = mask;
        check(busy.run() == (mask & 8u ? NBA97_TEXT_COMPLETE :
            NBA97_TEXT_UNKNOWN));
        if (!(mask & 8u))
            check(busy.progress.stopped_pc == 0x8009aa10u &&
                busy.progress.initial_busy_mask.known_mask == 7u &&
                busy.progress.machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_V0].word == 32u);
    }
}

void refusalAddressAndMalformedPrefixes() {
    Fixture no_io;
    no_io.context.io = nullptr;
    check(no_io.run() == NBA97_TEXT_IO_REFUSED &&
        no_io.progress.stopped_pc == 0x8009a9f0u &&
        no_io.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x8009a9f8u);
    Fixture reject_start;
    reject_start.reject_start = true;
    check(reject_start.run() == NBA97_TEXT_IO_REFUSED &&
        reject_start.progress.callbacks_completed == 0);
    Fixture reject_wait;
    reject_wait.clear_immediately = false;
    reject_wait.reject_wait = true;
    check(reject_wait.run() == NBA97_TEXT_IO_REFUSED &&
        reject_wait.progress.stopped_pc == 0x8009aa1cu);
    Fixture corrupt;
    corrupt.corrupt_machine = true;
    check(corrupt.run() == NBA97_TEXT_ARGUMENT &&
        corrupt.progress.machine.hi.known_mask == 16);

    Fixture unknown_pointer;
    unknown_pointer.globals.known[0] = 0;
    check(unknown_pointer.run() == NBA97_TEXT_UNKNOWN &&
        unknown_pointer.progress.stopped_pc == 0x8009a9ccu);
    Fixture unaligned;
    unaligned.globals.put(0x800c56acu, Fixture::ControlRegister + 1u);
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x8009a9b4u);
    Fixture unmapped;
    unmapped.globals.put(0x800c56b0u, 0x1f900000u);
    check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.stopped_pc == 0x8009a998u);
    Fixture unknown_sp;
    unknown_sp.context.machine.registers.gpr[
        NBA97_MATCH_INITIALIZE_SP].known_mask = 14;
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x8009a980u);
    Fixture unknown_jr;
    unknown_jr.context.machine.registers.gpr[
        NBA97_MATCH_INITIALIZE_RA].known_mask = 14;
    check(unknown_jr.run() == NBA97_TEXT_UNKNOWN &&
        unknown_jr.progress.stopped_pc == 0x8009aa5cu);

    Fixture malformed_byte;
    malformed_byte.globals.known[0] = 2;
    check(malformed_byte.run() == NBA97_TEXT_ARGUMENT);
    Fixture overlap;
    overlap.regions[1].base = overlap.regions[0].base;
    check(overlap.run() == NBA97_TEXT_ARGUMENT &&
        overlap.progress.operations == 0);
    Fixture invalid_machine;
    invalid_machine.context.machine.lo.known_mask = 16;
    check(invalid_machine.run() == NBA97_TEXT_ARGUMENT);
    Fixture known_null;
    known_null.stack.region.known = nullptr;
    known_null.regions[2] = known_null.stack.region;
    known_null.context.machine.registers.gpr[
        NBA97_MATCH_INITIALIZE_S0] = {0, 0};
    const auto before = known_null.stack.data;
    check(known_null.run() == NBA97_TEXT_ARGUMENT &&
        known_null.stack.data == before && known_null.progress.stores == 0);
}

void budgetsRunawayAliasesAndWrap() {
    Fixture baseline;
    check(baseline.run() == NBA97_TEXT_COMPLETE);
    for (std::size_t budget = 0; budget < baseline.progress.operations;
        ++budget) {
        Fixture f;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT &&
            f.progress.operations == budget && !f.progress.completed);
    }

    Fixture finite_waits;
    finite_waits.clear_immediately = false;
    finite_waits.clear_after_waits = 3;
    check(finite_waits.run() == NBA97_TEXT_COMPLETE &&
        finite_waits.progress.wait_iterations == 3);
    for (std::size_t budget = 0;
        budget < finite_waits.progress.operations; ++budget) {
        Fixture f;
        f.clear_immediately = false;
        f.clear_after_waits = 3;
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT &&
            f.progress.operations == budget && !f.progress.completed);
    }

    Fixture runaway;
    runaway.clear_immediately = false;
    runaway.context.operation_budget = 40;
    check(runaway.run() == NBA97_TEXT_LIMIT &&
        runaway.progress.operations == 40 &&
        runaway.progress.wait_iterations > 1);

    Fixture stack_mmio;
    stack_mmio.regions[1].data = stack_mmio.stack.data.data() + 0xe0u;
    stack_mmio.regions[1].known = stack_mmio.stack.known.data() + 0xe0u;
    for (unsigned i = 0; i < stack_mmio.mmio.data.size(); ++i) {
        stack_mmio.stack.data[0xe0u + i] = stack_mmio.mmio.data[i];
        stack_mmio.stack.known[0xe0u + i] = stack_mmio.mmio.known[i];
    }
    check(stack_mmio.run() == NBA97_TEXT_COMPLETE);

    Fixture pointer_alias;
    pointer_alias.regions[0].data = pointer_alias.mmio.data.data();
    pointer_alias.regions[0].known = pointer_alias.mmio.known.data();
    pointer_alias.mmio.put(Fixture::AddressRegister, Fixture::AddressRegister);
    pointer_alias.mmio.put(Fixture::CountRegister, Fixture::CountRegister);
    pointer_alias.mmio.put(Fixture::ControlRegister, Fixture::ControlRegister);
    pointer_alias.mmio.put(0x1f8010ecu, Fixture::MasterRegister);
    check(pointer_alias.run() == NBA97_TEXT_RESOURCE &&
        pointer_alias.progress.stopped_pc == 0x8009a9ecu &&
        pointer_alias.progress.stopped_address == 0);

    Block wrapped(0, 32);
    Fixture wrap;
    wrap.regions[2] = wrapped.region;
    wrap.context.machine.registers.gpr[
        NBA97_MATCH_INITIALIZE_SP] = {0x10u, 0x0f};
    check(wrap.run() == NBA97_TEXT_COMPLETE &&
        wrap.progress.frame_stack_pointer == 0xfffffff0u &&
        wrap.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == 0x10u);
}

void arguments() {
    Nba97GameOrderingTableDmaProgress progress{};
    check(nba97_game_ordering_table_dma(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_ordering_table_dma(&f.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    setupCountsAndExactOrder();
    busyWaitAndErrorPaths();
    liveMachineAndKnownness();
    refusalAddressAndMalformedPrefixes();
    budgetsRunawayAliasesAndWrap();
    arguments();
    std::printf("%u ordering-table DMA focused checks passed\n", checks);
    return 0;
}
