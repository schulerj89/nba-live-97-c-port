#include "recovered/game_clear_ordering_table.h"

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
        std::fprintf(stderr, "clear-ordering-table check %u failed at line %u\n",
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
    U32 get(U32 address, unsigned width = 4) const {
        U32 value = 0;
        const std::size_t at = address - base;
        for (unsigned i = 0; i < width; ++i)
            value |= U32(data[at + i]) << (8u * i);
        return value;
    }
};

struct Fixture {
    Block globals{0x800c55b8u, 16};
    Block table1{0x800c6000u, 64};
    Block table2{0x800c6100u, 64};
    Block stack{0x80100000u, 512};
    Block alternate{0x80101000u, 128};
    Block ot1{0x800fccf0u, 128};
    Block ot2{0x800f5c50u, 128};
    std::array<Nba97GameTextRegion, 7> regions{};
    std::array<Nba97GameClearOrderingTableAccess, 32> journal{};
    std::vector<Nba97GameClearOrderingTableEvent> events;
    std::vector<Nba97GameClearOrderingTableMachine> entries;
    Nba97GameClearOrderingTableContext context{};
    Nba97GameClearOrderingTableProgress progress{};
    bool reject_debug = false;
    bool reject_backend = false;
    bool corrupt_machine = false;
    bool mutate_debug_table = false;
    bool mutate_backend_machine = false;

    Fixture(U32 debug = 0, U32 object = 0x800fccf0u,
        U32 count = 32) {
        regions = {globals.region, table1.region, table2.region, stack.region,
            alternate.region, ot1.region, ot2.region};
        globals.put(0x800c55c2u, debug, 1);
        globals.put(0x800c55bcu, 0x8009cb2cu);
        globals.put(0x800c55b8u, table1.base);
        table1.put(table1.base + 0x2cu, 0x8009a97cu);
        table2.put(table2.base + 0x2cu, 0x8009aa00u);
        for (unsigned i = 0; i < 32; ++i)
            context.machine.registers.gpr[i] =
                {0x11000000u + i * 0x101u, 0x0fu};
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
        context.operation_budget = 100;
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
        const Nba97GameClearOrderingTableEvent* event,
        Nba97GameClearOrderingTableMachine* machine) {
        auto& f = *static_cast<Fixture*>(opaque);
        f.events.push_back(*event);
        f.entries.push_back(*machine);
        if (event->kind == NBA97_GAME_CLEAR_ORDERING_TABLE_DEBUG) {
            if (f.mutate_debug_table)
                putMemory(memory, 0x800c55b8u, 0x800c6100u);
            if (f.reject_debug)
                return 0;
        } else {
            if (f.mutate_backend_machine) {
                machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
                    {0x80101000u, 0x0f};
                machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] =
                    {0x800f5c50u, 0x0f};
                machine->registers.gpr[
                    NBA97_GAME_CLEAR_ORDERING_TABLE_S1] =
                    {0xfeed1234u, 0x0f};
                machine->registers.gpr[12] = {0xdecafbad, 0x0f};
                machine->hi = {0x10203040u, 0x0f};
                machine->lo = {0x50607080u, 0x0f};
                putMemory(memory, 0x80101018u, 0x90000004u);
                putMemory(memory, 0x80101014u, 0x11112222u);
                putMemory(memory, 0x80101010u, 0x33334444u);
            }
            if (f.reject_backend)
                return 0;
        }
        if (f.corrupt_machine)
            machine->hi.known_mask = 16;
        return 1;
    }

    int run() {
        return nba97_game_clear_ordering_table(&context, &progress);
    }
};

void gatesCountsAndOrdering() {
    for (U32 level : std::array<U32, 4>{{0, 1, 2, 255}}) {
        for (U32 count : std::array<U32, 7>{{0, 1, 32, 4096,
                0xffffffffu, 0x80000000u, 7}}) {
            Fixture f(level, 0x800fccf0u, count);
            check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
            check(f.progress.debug_level.word == level &&
                f.progress.debug_level.known_mask == 15 &&
                f.progress.ordering_table_head.word == 0x000c567cu &&
                f.progress.return_v0.word == 0x800fccf0u &&
                f.ot1.get(f.ot1.base) == 0x000c567cu);
            const bool diagnostic = level >= 2;
            check(f.events.size() == (diagnostic ? 2u : 1u));
            const auto& backend = f.events.back();
            check(backend.kind == NBA97_GAME_CLEAR_ORDERING_TABLE_BACKEND &&
                backend.pc == 0x800999bcu &&
                backend.delay_slot_pc == 0x800999c0u &&
                backend.target == 0x8009a97cu &&
                backend.argument_count == 2);
            const auto& backend_machine = f.entries.back();
            check(backend_machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_A0].word == 0x800fccf0u &&
                backend_machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_A1].word == count &&
                backend_machine.registers.gpr[
                    NBA97_MATCH_INITIALIZE_RA].word == 0x800999c4u);
            if (diagnostic) {
                const auto& event = f.events.front();
                const auto& machine = f.entries.front();
                check(event.kind == NBA97_GAME_CLEAR_ORDERING_TABLE_DEBUG &&
                    event.pc == 0x800999a0u &&
                    event.delay_slot_pc == 0x800999a4u &&
                    event.target == 0x8009cb2cu &&
                    event.argument_count == 3);
                check(machine.registers.gpr[
                        NBA97_MATCH_INITIALIZE_A0].word == 0x80028340u &&
                    machine.registers.gpr[
                        NBA97_MATCH_INITIALIZE_A1].word == 0x800fccf0u &&
                    machine.registers.gpr[
                        NBA97_MATCH_INITIALIZE_A2].word == count &&
                    machine.registers.gpr[
                        NBA97_MATCH_INITIALIZE_RA].word == 0x800999a8u);
            }
            check(f.journal[0].pc == 0x80099964u &&
                f.journal[1].pc == 0x8009996cu &&
                f.journal[2].pc == 0x80099974u &&
                f.journal[3].pc == 0x80099984u);
        }
    }
}

void dynamicTargetsAndLiveMachine() {
    Fixture dynamic(2);
    dynamic.mutate_debug_table = true;
    check(dynamic.run() == NBA97_TEXT_COMPLETE &&
        dynamic.progress.dispatch_table.word == 0x800c6100u &&
        dynamic.progress.backend_target.word == 0x8009aa00u &&
        dynamic.events[1].target == 0x8009aa00u);
    bool table_after_debug = false;
    for (const auto& access : dynamic.journal)
        if (access.pc == 0x800999acu)
            table_after_debug = access.operation > dynamic.events[0].operation;
    check(table_after_debug);

    Fixture live(0);
    live.mutate_backend_machine = true;
    check(live.run() == NBA97_TEXT_COMPLETE &&
        live.ot1.get(live.ot1.base) == 0 &&
        live.ot2.get(live.ot2.base) == 0x000c567cu &&
        live.progress.return_v0.word == live.ot2.base &&
        live.progress.machine.registers.gpr[12].word == 0xdecafbadu &&
        live.progress.machine.hi.word == 0x10203040u &&
        live.progress.machine.lo.word == 0x50607080u &&
        live.progress.restored_return_address.word == 0x90000004u &&
        live.progress.restored_s1.word == 0x11112222u &&
        live.progress.restored_s0.word == 0x33334444u &&
        live.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == 0x80101020u);
}

void unknownsRefusalsAndValidation() {
    Fixture predicate(0);
    predicate.globals.known[0x800c55c2u - predicate.globals.base] = 0;
    check(predicate.run() == NBA97_TEXT_UNKNOWN &&
        predicate.progress.operations == 4 &&
        predicate.progress.stopped_pc == 0x80099980u &&
        predicate.progress.debug_predicate.known_mask == 14 &&
        predicate.stack.get(0x801000f8u) == 0x81234568u);

    Fixture unknown_debug(2);
    for (unsigned i = 0; i < 4; ++i)
        unknown_debug.globals.known[0x800c55bcu -
            unknown_debug.globals.base + i] = 0;
    check(unknown_debug.run() == NBA97_TEXT_UNKNOWN &&
        unknown_debug.progress.stopped_pc == 0x800999a0u &&
        unknown_debug.progress.call_attempts[
            NBA97_GAME_CLEAR_ORDERING_TABLE_DEBUG] == 1 &&
        unknown_debug.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x800999a8u &&
        unknown_debug.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_A2].word == 32u);

    Fixture unknown_backend(0);
    for (unsigned i = 0; i < 4; ++i)
        unknown_backend.table1.known[0x2cu + i] = 0;
    check(unknown_backend.run() == NBA97_TEXT_UNKNOWN &&
        unknown_backend.progress.stopped_pc == 0x800999bcu &&
        unknown_backend.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_RA].word == 0x800999c4u &&
        unknown_backend.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_A1].word == 32u);

    Fixture unaligned(0);
    unaligned.table1.put(unaligned.table1.base + 0x2cu, 0x8009a97du);
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x800999bcu);
    Fixture missing(0);
    missing.context.io = nullptr;
    check(missing.run() == NBA97_TEXT_IO_REFUSED &&
        missing.progress.operations == 7);
    Fixture reject_debug(2);
    reject_debug.reject_debug = true;
    check(reject_debug.run() == NBA97_TEXT_IO_REFUSED &&
        reject_debug.progress.operations == 6 &&
        reject_debug.progress.callbacks_completed == 0);
    Fixture reject_backend(0);
    reject_backend.reject_backend = true;
    check(reject_backend.run() == NBA97_TEXT_IO_REFUSED &&
        reject_backend.ot1.get(reject_backend.ot1.base) == 0);
    Fixture malformed(0);
    malformed.corrupt_machine = true;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.callbacks_completed == 0);

    Fixture unknown_sp(0);
    unknown_sp.context.machine.registers.gpr[
        NBA97_MATCH_INITIALIZE_SP].known_mask = 14;
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.stopped_pc == 0x8009996cu &&
        unknown_sp.progress.operations == 1);

    Fixture unknown_jr(0);
    unknown_jr.context.machine.registers.gpr[
        NBA97_MATCH_INITIALIZE_RA].known_mask = 14;
    check(unknown_jr.run() == NBA97_TEXT_UNKNOWN &&
        unknown_jr.progress.stopped_pc == 0x800999f0u &&
        unknown_jr.ot1.get(unknown_jr.ot1.base) == 0x000c567cu);

    Fixture malformed_byte(0);
    malformed_byte.globals.known[0x800c55c2u -
        malformed_byte.globals.base] = 2;
    check(malformed_byte.run() == NBA97_TEXT_ARGUMENT &&
        malformed_byte.progress.operations == 1);

    Fixture overlap(0);
    overlap.regions[1].base = overlap.regions[0].base;
    check(overlap.run() == NBA97_TEXT_ARGUMENT &&
        overlap.progress.operations == 0);

    Fixture no_known(0);
    no_known.stack.region.known = nullptr;
    no_known.regions[3] = no_known.stack.region;
    no_known.context.machine.registers.gpr[
        NBA97_MATCH_INITIALIZE_S0] = {0, 0};
    const auto before = no_known.stack.data;
    check(no_known.run() == NBA97_TEXT_ARGUMENT &&
        no_known.stack.data == before && no_known.progress.stores == 0);
}

void budgetsAliasesAndWrap() {
    for (U32 debug : std::array<U32, 2>{{0, 2}}) {
        Fixture baseline(debug);
        check(baseline.run() == NBA97_TEXT_COMPLETE);
        const std::size_t total = baseline.progress.operations;
        for (std::size_t budget = 0; budget < total; ++budget) {
            Fixture f(debug);
            f.context.operation_budget = budget;
            check(f.run() == NBA97_TEXT_LIMIT &&
                f.progress.operations == budget && !f.progress.completed);
        }
    }

    Fixture alias(2);
    alias.regions[0].data = alias.table2.data.data();
    alias.regions[0].known = alias.table2.known.data();
    alias.table2.put(alias.table2.base + 0x0au, 2, 1);
    alias.table2.put(alias.table2.base, alias.table1.base);
    alias.table2.put(alias.table2.base + 4, 0x8009cb2cu);
    /* Native backing alias is legal even when guest ranges are disjoint. */
    check(alias.run() == NBA97_TEXT_COMPLETE);

    Block wrap_memory(0, 32);
    Fixture wrap(0);
    wrap.regions[3] = wrap_memory.region;
    wrap.context.machine.registers.gpr[
        NBA97_MATCH_INITIALIZE_SP] = {0x10u, 0x0f};
    check(wrap.run() == NBA97_TEXT_COMPLETE &&
        wrap.progress.frame_stack_pointer == 0xfffffff0u &&
        wrap.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == 0x10u);
}

void knownMasksTargetsAndAliasing() {
    for (unsigned mask = 0; mask < 16; ++mask) {
        Fixture target(0);
        for (unsigned byte = 0; byte < 4; ++byte)
            target.table1.known[0x2cu + byte] =
                static_cast<std::uint8_t>((mask >> byte) & 1u);
        const int result = target.run();
        check(result == (mask == 15 ? NBA97_TEXT_COMPLETE :
            NBA97_TEXT_UNKNOWN));
        check(target.progress.backend_target.known_mask == mask);

        Fixture saved(0);
        saved.context.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_S0].known_mask =
                static_cast<std::uint8_t>(mask);
        saved.context.machine.registers.gpr[
            NBA97_GAME_CLEAR_ORDERING_TABLE_S1].known_mask =
                static_cast<std::uint8_t>(mask);
        saved.context.machine.hi.known_mask =
            static_cast<std::uint8_t>(mask);
        saved.context.machine.lo.known_mask =
            static_cast<std::uint8_t>(15u - mask);
        check(saved.run() == NBA97_TEXT_COMPLETE &&
            saved.progress.restored_s0.known_mask == mask &&
            saved.progress.restored_s1.known_mask == mask &&
            saved.progress.machine.hi.known_mask == mask &&
            saved.progress.machine.lo.known_mask == 15u - mask);

        Fixture count(0);
        count.context.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_A1].known_mask =
                static_cast<std::uint8_t>(mask);
        check(count.run() == NBA97_TEXT_COMPLETE &&
            count.entries[0].registers.gpr[
                NBA97_MATCH_INITIALIZE_A1].known_mask == mask);
    }

    Fixture null_target(0);
    null_target.table1.put(null_target.table1.base + 0x2cu, 0);
    check(null_target.run() == NBA97_TEXT_COMPLETE &&
        null_target.events[0].target == 0);

    Fixture missing_target(0);
    missing_target.regions[1].size = 0x2cu;
    check(missing_target.run() == NBA97_TEXT_RESOURCE &&
        missing_target.progress.stopped_pc == 0x800999b4u &&
        missing_target.progress.stopped_address == 0x800c602cu);

    Fixture stack_ot(0);
    stack_ot.regions[5].data = stack_ot.stack.data.data() + 0xf0u;
    stack_ot.regions[5].known = stack_ot.stack.known.data() + 0xf0u;
    check(stack_ot.run() == NBA97_TEXT_COMPLETE &&
        stack_ot.progress.restored_s0.word == 0x000c567cu &&
        stack_ot.stack.get(0x801000f0u) == 0x000c567cu);

    Fixture stack_global(0);
    for (unsigned i = 0; i < 16; ++i) {
        stack_global.stack.data[0xe0u + i] = stack_global.globals.data[i];
        stack_global.stack.known[0xe0u + i] =
            stack_global.globals.known[i];
    }
    stack_global.regions[0].data = stack_global.stack.data.data() + 0xe0u;
    stack_global.regions[0].known = stack_global.stack.known.data() + 0xe0u;
    check(stack_global.run() == NBA97_TEXT_COMPLETE);
}

void arguments() {
    Nba97GameClearOrderingTableProgress progress{};
    check(nba97_game_clear_ordering_table(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
    Fixture f;
    check(nba97_game_clear_ordering_table(&f.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
    f.context.machine.registers.gpr[7].known_mask = 16;
    check(f.run() == NBA97_TEXT_ARGUMENT);
}
}

int main() {
    gatesCountsAndOrdering();
    dynamicTargetsAndLiveMachine();
    unknownsRefusalsAndValidation();
    budgetsAliasesAndWrap();
    knownMasksTargetsAndAliasing();
    arguments();
    std::printf("%u clear-ordering-table focused checks passed\n", checks);
    return 0;
}
