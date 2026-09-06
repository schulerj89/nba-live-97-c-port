#include "recovered/game_stream_queue_count.h"

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
        std::fprintf(stderr, "stream queue count check %u failed at %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Head = 0x800c43a0u;
constexpr std::uint32_t Counter = 0x800c4410u;
constexpr std::uint32_t EntrySp = 0x800ff000u;
constexpr std::uint32_t Frame = EntrySp - 0x20u;
constexpr std::uint32_t NodeA = 0x80090000u;
constexpr std::uint32_t NodeB = 0x80090020u;
constexpr std::uint32_t NodeC = 0x80090040u;

bool sameWord(const Nba97GameStreamQueueCountWord& a,
    const Nba97GameStreamQueueCountWord& b) {
    return a.word == b.word && a.known_mask == b.known_mask;
}

struct Fixture {
    std::vector<std::uint8_t> bytes =
        std::vector<std::uint8_t>(0x110000u, 0xcd);
    std::vector<std::uint8_t> known =
        std::vector<std::uint8_t>(0x110000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameStreamQueueCountContext context{};
    Nba97GameStreamQueueCountProgress progress{};
    std::array<Nba97GameStreamQueueCountAccess, 256> journal{};
    std::vector<Nba97GameStreamQueueCountEvent> calls;
    std::vector<Nba97GameStreamQueueCountMachine> entries;
    int refuse_kind = 0;
    bool malformed = false;
    bool mutate_lock = false;
    bool mutate_unlock = false;
    std::uint32_t lock_s8 = 0x800fe000u;
    std::uint32_t unlock_s8 = 0x800fd000u;

    Fixture() {
        context.memory = {&region, 1};
        context.operation_budget = 500;
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.machine.registers.gpr[i] = {
                0x21000000u + i * 0x01010101u,
                static_cast<std::uint8_t>((i % 15u) + 1u)};
        context.machine.registers.gpr[0] = {0, 15};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {EntrySp, 15};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_FP] =
            {0xabcdef01u, 5};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x80088d38u, 15};
        context.machine.hi = {0x12345678u, 3};
        context.machine.lo = {0x9abcdef0u, 12};
        put(Head, NodeA);
        put(Counter, 0);
        put(NodeA, 0);
    }

    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Ram);
    }
    void put(std::uint32_t address, std::uint32_t value,
        std::uint8_t mask = 15) {
        const auto at = offset(address);
        for (unsigned i = 0; i < 4; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
            known[at + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
        }
    }
    std::uint32_t get(std::uint32_t address) const {
        const auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(bytes[at + i]) << (i * 8u);
        return value;
    }
    std::uint8_t getKnown(std::uint32_t address) const {
        const auto at = offset(address);
        std::uint8_t mask = 0;
        for (unsigned i = 0; i < 4; ++i)
            mask = static_cast<std::uint8_t>(mask | (known[at + i] << i));
        return mask;
    }
    static int io(void* user, const Nba97GameTextMemory*,
        const Nba97GameStreamQueueCountEvent* event,
        Nba97GameStreamQueueCountMachine* machine) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        f.entries.push_back(*machine);
        if (event->kind == NBA97_GAME_STREAM_QUEUE_COUNT_CHILD_80093D94 &&
            f.mutate_lock) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0] =
                {0xfeedbeefu, 9};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
                {0xdead0000u, 7};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_FP] =
                {f.lock_s8, 15};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
                {0xdeadbeefu, 6};
            machine->hi = {0x0badc0deu, 5};
            machine->lo = {0xc001d00du, 10};
            f.put(Head, 0xfffffffeu);
            f.put(Counter, 0xffffffffu);
            f.put(f.lock_s8 + 0x14u, 9, 15);
            f.put(f.lock_s8 + 0x1cu, 0x80088d38u, 15);
            f.put(f.lock_s8 + 0x18u, 0x76543210u, 7);
        }
        if (event->kind == NBA97_GAME_STREAM_QUEUE_COUNT_CHILD_80093DD4 &&
            f.mutate_unlock) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_A3] =
                {0x13572468u, 6};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
                {0xbaad0000u, 1};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_FP] =
                {f.unlock_s8, 15};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
                {0xcafebabeu, 9};
            machine->hi = {0x11112222u, 6};
            machine->lo = {0x33334444u, 9};
            f.put(f.unlock_s8 + 0x14u, 0x89abcdefu, 5);
            f.put(f.unlock_s8 + 0x1cu, 0x80088d38u, 15);
            f.put(f.unlock_s8 + 0x18u, 0x24681357u, 3);
        }
        if (f.malformed)
            machine->registers.gpr[0] = {1, 15};
        return event->kind == f.refuse_kind ? 0 : 1;
    }
    int run() {
        return nba97_game_stream_queue_count(&context, &progress);
    }
};

void emptySentinelsAndBasicLists() {
    Fixture empty;
    empty.put(Head, 0);
    const auto original = empty.context.machine;
    check(empty.run() == NBA97_TEXT_COMPLETE && empty.progress.completed &&
        empty.progress.returned_count.word == 0xffffffffu &&
        empty.progress.returned_count.known_mask == 15 && empty.calls.empty());
    check(empty.progress.operations == 6 && empty.progress.stores == 3 &&
        empty.progress.reads == 3 && empty.get(Counter) == 0 &&
        sameWord(empty.progress.machine.hi, original.hi) &&
        sameWord(empty.progress.machine.lo, original.lo));

    for (std::uint32_t sentinel : {0xfffffffeu, 0xffffffffu}) {
        Fixture f;
        f.put(Head, sentinel);
        check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
            f.progress.returned_count.word == 0 && f.calls.size() == 2 &&
            f.calls[0].kind ==
                NBA97_GAME_STREAM_QUEUE_COUNT_CHILD_80093D94 &&
            f.calls[1].kind ==
                NBA97_GAME_STREAM_QUEUE_COUNT_CHILD_80093DD4 &&
            f.get(Counter) == 0 && f.progress.links_counted == 0);
        check(f.progress.operations == (sentinel == 0xfffffffeu ? 16u : 17u));
    }

    Fixture zero_link;
    check(zero_link.run() == NBA97_TEXT_COMPLETE &&
        zero_link.progress.returned_count.word == 0 &&
        zero_link.progress.operations == 19 &&
        zero_link.progress.loop_iterations == 1 &&
        zero_link.progress.links_counted == 0);

    Fixture one;
    one.put(NodeA, NodeB);
    one.put(NodeB, 0);
    check(one.run() == NBA97_TEXT_COMPLETE &&
        one.progress.returned_count.word == 1 &&
        one.progress.operations == 30 &&
        one.progress.loop_iterations == 2 && one.progress.links_counted == 1);

    Fixture two;
    two.put(NodeA, NodeB);
    two.put(NodeB, NodeC);
    two.put(NodeC, 0);
    check(two.run() == NBA97_TEXT_COMPLETE &&
        two.progress.returned_count.word == 2 &&
        two.progress.operations == 41 &&
        two.progress.loop_iterations == 3 && two.progress.links_counted == 2);

    Fixture linked_sentinel;
    linked_sentinel.put(NodeA, 0xffffffffu);
    check(linked_sentinel.run() == NBA97_TEXT_COMPLETE &&
        linked_sentinel.progress.returned_count.word == 1 &&
        linked_sentinel.progress.loop_iterations == 2 &&
        linked_sentinel.progress.operations == 28);
}

void ExactCallsCounterWrapAndRepeatedReads() {
    Fixture f;
    f.put(NodeA, NodeB);
    f.put(NodeB, 0);
    f.put(Counter, 0xffffffffu);
    check(f.run() == NBA97_TEXT_COMPLETE && f.calls.size() == 2 &&
        f.progress.counter_after_increment.word == 0 &&
        f.progress.counter_after_decrement.word == 0xffffffffu &&
        f.get(Counter) == 0xffffffffu);
    check(f.calls[0].pc == 0x8008447cu &&
        f.calls[0].delay_slot_pc == 0x80084480u &&
        f.calls[0].entry == 0x80093d94u &&
        f.calls[0].operation == 5 && f.calls[0].invocation == 1 &&
        f.calls[0].argument_count == 0 &&
        f.entries[0].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80084484u);
    check(f.calls[1].pc == 0x8008455cu &&
        f.calls[1].delay_slot_pc == 0x80084560u &&
        f.calls[1].entry == 0x80093dd4u &&
        f.calls[1].operation == 27 && f.calls[1].invocation == 1 &&
        f.entries[1].registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80084564u);

    const std::array<std::uint32_t, 28> expected_access_pcs{{
        0x8008444cu, 0x80084450u, 0x80084458u, 0x80084460u,
        0x80084488u, 0x8008449cu, 0x800844a4u, 0x800844acu,
        0x800844b0u, 0x800844c0u, 0x800844d0u, 0x800844d8u,
        0x800844f8u, 0x80084508u, 0x8008450cu, 0x80084514u,
        0x80084524u, 0x8008452cu, 0x80084534u,
        0x800844b0u, 0x800844c0u, 0x800844d0u, 0x800844d8u,
        0x80084544u, 0x80084558u, 0x80084564u,
        0x80084574u, 0x80084578u}};
    check(f.progress.access_events == expected_access_pcs.size());
    for (std::size_t i = 0; i < expected_access_pcs.size(); ++i)
        check(f.journal[i].pc == expected_access_pcs[i] &&
            f.journal[i].width == 4);
    check(f.journal[8].address == Frame + 0x10u &&
        f.journal[9].address == Frame + 0x10u &&
        f.journal[10].address == Frame + 0x10u &&
        f.journal[11].address == NodeA &&
        f.journal[15].address == NodeA &&
        f.journal[17].address == NodeA);
}

void CallbackMutationAndPostUnlockRawCount() {
    Fixture lock;
    lock.mutate_lock = true;
    check(lock.run() == NBA97_TEXT_COMPLETE &&
        lock.progress.returned_count.word == 9 &&
        lock.progress.counter_after_increment.word == 0 &&
        lock.progress.counter_after_decrement.word == 0xffffffffu &&
        lock.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0].word ==
            0xfeedbeefu && lock.progress.machine.hi.word == 0x0badc0deu &&
        lock.progress.machine.lo.word == 0xc001d00du);
    check(lock.progress.restored_return_address.word == 0x80088d38u &&
        lock.progress.restored_s8.word == 0x76543210u &&
        lock.progress.restored_s8.known_mask == 7 &&
        lock.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            lock.lock_s8 + 0x20u);

    Fixture unlock;
    unlock.mutate_unlock = true;
    check(unlock.run() == NBA97_TEXT_COMPLETE &&
        unlock.progress.returned_count.word == 0x89abcdefu &&
        unlock.progress.returned_count.known_mask == 5 &&
        unlock.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A3].word ==
            0x13572468u && unlock.progress.machine.hi.word == 0x11112222u &&
        unlock.progress.machine.lo.word == 0x33334444u);
    check(unlock.progress.restored_s8.word == 0x24681357u &&
        unlock.progress.restored_s8.known_mask == 3 &&
        unlock.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            unlock.unlock_s8 + 0x20u);
}

void AliasesUnknownsRefusalsAndCycles() {
    /* Redirecting s8 makes local count alias node A. Its increment changes
     * all three evidenced node reads before the next unaligned dereference. */
    Fixture alias;
    alias.context.io = [](void* user, const Nba97GameTextMemory*,
        const Nba97GameStreamQueueCountEvent* event,
        Nba97GameStreamQueueCountMachine* machine) -> int {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        if (event->kind == NBA97_GAME_STREAM_QUEUE_COUNT_CHILD_80093D94)
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_FP] =
                {NodeA - 0x14u, 15};
        return 1;
    };
    alias.put(NodeA, NodeB);
    check(alias.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        alias.progress.stopped_pc == 0x800844d8u &&
        alias.progress.stopped_address == NodeB + 1u &&
        alias.get(NodeA) == NodeB + 1u && alias.calls.size() == 1);
    std::vector<std::uint32_t> aliased_values;
    for (std::size_t i = 0; i < alias.progress.access_events; ++i)
        if ((alias.journal[i].pc == 0x800844d8u ||
             alias.journal[i].pc == 0x80084514u ||
             alias.journal[i].pc == 0x8008452cu) &&
            alias.journal[i].address == NodeA)
            aliased_values.push_back(alias.journal[i].value);
    check(aliased_values.size() == 3 && aliased_values[0] == NodeB &&
        aliased_values[1] == NodeB + 1u &&
        aliased_values[2] == NodeB + 1u);

    /* The lock callback points the head at the live count local and changes
     * that local to UINT32_MAX. The first node read is nonzero, the count
     * store wraps it to zero, and the second node read therefore skips the
     * third pointer/node reread and pointer store before the next loop exits. */
    Fixture second_read_zero;
    second_read_zero.context.io = [](void* user,
        const Nba97GameTextMemory*,
        const Nba97GameStreamQueueCountEvent* event,
        Nba97GameStreamQueueCountMachine* machine) -> int {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        f.entries.push_back(*machine);
        if (event->kind == NBA97_GAME_STREAM_QUEUE_COUNT_CHILD_80093D94) {
            const auto live_frame = machine->registers.gpr[
                NBA97_MATCH_INITIALIZE_FP].word;
            f.put(Head, live_frame + 0x14u);
            f.put(live_frame + 0x14u, 0xffffffffu);
        }
        return 1;
    };
    check(second_read_zero.run() == NBA97_TEXT_COMPLETE &&
        second_read_zero.progress.returned_count.word == 0 &&
        second_read_zero.progress.returned_count.known_mask == 15 &&
        second_read_zero.progress.links_counted == 1 &&
        second_read_zero.progress.loop_iterations == 2 &&
        second_read_zero.progress.operations == 27 &&
        second_read_zero.calls.size() == 2);
    const std::array<std::uint32_t, 25> second_read_zero_pcs{{
        0x8008444cu, 0x80084450u, 0x80084458u, 0x80084460u,
        0x80084488u, 0x8008449cu, 0x800844a4u, 0x800844acu,
        0x800844b0u, 0x800844c0u, 0x800844d0u, 0x800844d8u,
        0x800844f8u, 0x80084508u, 0x8008450cu, 0x80084514u,
        0x800844b0u, 0x800844c0u, 0x800844d0u, 0x800844d8u,
        0x80084544u, 0x80084558u, 0x80084564u,
        0x80084574u, 0x80084578u}};
    check(second_read_zero.progress.access_events ==
        second_read_zero_pcs.size());
    for (std::size_t i = 0; i < second_read_zero_pcs.size(); ++i)
        check(second_read_zero.journal[i].pc == second_read_zero_pcs[i]);
    check(second_read_zero.journal[11].address == Frame + 0x14u &&
        second_read_zero.journal[11].value == 0xffffffffu &&
        second_read_zero.journal[13].address == Frame + 0x14u &&
        second_read_zero.journal[13].value == 0 &&
        second_read_zero.journal[15].address == Frame + 0x14u &&
        second_read_zero.journal[15].value == 0 &&
        second_read_zero.get(Frame + 0x10u) == Frame + 0x14u);
    for (std::size_t i = 0;
         i < second_read_zero.progress.access_events; ++i)
        check(second_read_zero.journal[i].pc != 0x80084524u &&
            second_read_zero.journal[i].pc != 0x8008452cu &&
            second_read_zero.journal[i].pc != 0x80084534u);

    Fixture unknown_head;
    unknown_head.put(Head, 0, 14);
    check(unknown_head.run() == NBA97_TEXT_UNKNOWN &&
        unknown_head.progress.stopped_pc == 0x80084468u &&
        unknown_head.progress.operations == 4 && unknown_head.calls.empty());

    Fixture callback_null;
    callback_null.context.io = [](void* user, const Nba97GameTextMemory*,
        const Nba97GameStreamQueueCountEvent* event,
        Nba97GameStreamQueueCountMachine*) -> int {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back(*event);
        if (event->kind == NBA97_GAME_STREAM_QUEUE_COUNT_CHILD_80093D94)
            f.put(Head, 0);
        return 1;
    };
    check(callback_null.run() == NBA97_TEXT_RESOURCE &&
        callback_null.progress.stopped_pc == 0x800844d8u &&
        callback_null.progress.stopped_address == 0 &&
        callback_null.calls.size() == 1 && callback_null.get(Counter) == 1);

    Fixture unknown_sentinel;
    unknown_sentinel.put(Head, 0xfffffffeu, 14);
    check(unknown_sentinel.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sentinel.progress.stopped_pc == 0x800844b8u);

    Fixture unknown_pointer;
    unknown_pointer.put(Head, NodeA, 8);
    check(unknown_pointer.run() == NBA97_TEXT_UNKNOWN &&
        unknown_pointer.progress.stopped_pc == 0x800844d8u &&
        unknown_pointer.progress.operations == 12);

    Fixture unknown_node;
    unknown_node.put(NodeA, 0, 14);
    check(unknown_node.run() == NBA97_TEXT_UNKNOWN &&
        unknown_node.progress.stopped_pc == 0x800844e0u);

    Fixture partial_count;
    partial_count.mutate_unlock = true;
    check(partial_count.run() == NBA97_TEXT_COMPLETE &&
        partial_count.progress.returned_count.known_mask == 5);

    Fixture lock_refused;
    lock_refused.refuse_kind =
        NBA97_GAME_STREAM_QUEUE_COUNT_CHILD_80093D94;
    check(lock_refused.run() == NBA97_TEXT_IO_REFUSED &&
        lock_refused.progress.operations == 5 && lock_refused.get(Counter) == 0 &&
        lock_refused.progress.stopped_pc == 0x8008447cu);

    Fixture unlock_refused;
    unlock_refused.refuse_kind =
        NBA97_GAME_STREAM_QUEUE_COUNT_CHILD_80093DD4;
    check(unlock_refused.run() == NBA97_TEXT_IO_REFUSED &&
        unlock_refused.get(Counter) == 0 && unlock_refused.calls.size() == 2 &&
        unlock_refused.progress.stopped_pc == 0x8008455cu);

    Fixture malformed;
    malformed.malformed = true;
    check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations == 5 &&
        malformed.progress.callbacks_completed == 0);

    Fixture cycle;
    cycle.put(NodeA, NodeA);
    cycle.context.operation_budget = 40;
    check(cycle.run() == NBA97_TEXT_LIMIT && !cycle.progress.completed &&
        cycle.progress.operations == 40 && cycle.calls.size() == 1 &&
        cycle.get(Counter) == 1 &&
        cycle.progress.call_count[
            NBA97_GAME_STREAM_QUEUE_COUNT_CHILD_80093DD4] == 0 &&
        cycle.progress.links_counted > 1);
}

void EveryBudgetPrefixAndMappingMetadata() {
    const std::array<std::uint32_t, 30> pcs{{
        0x8008444cu, 0x80084450u, 0x80084458u, 0x80084460u,
        0x8008447cu, 0x80084488u, 0x8008449cu, 0x800844a4u,
        0x800844acu, 0x800844b0u, 0x800844c0u, 0x800844d0u,
        0x800844d8u, 0x800844f8u, 0x80084508u, 0x8008450cu,
        0x80084514u, 0x80084524u, 0x8008452cu, 0x80084534u,
        0x800844b0u, 0x800844c0u, 0x800844d0u, 0x800844d8u,
        0x80084544u, 0x80084558u, 0x8008455cu, 0x80084564u,
        0x80084574u, 0x80084578u}};
    for (std::size_t budget = 0; budget < pcs.size(); ++budget) {
        Fixture f;
        f.put(NodeA, NodeB);
        f.put(NodeB, 0);
        f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed &&
            f.progress.operations == budget &&
            f.progress.stopped_pc == pcs[budget]);
    }
    Fixture complete;
    complete.put(NodeA, NodeB);
    complete.put(NodeB, 0);
    complete.context.operation_budget = pcs.size();
    check(complete.run() == NBA97_TEXT_COMPLETE);

    Fixture unaligned;
    unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word++;
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x8008444cu);

    Fixture unmapped;
    unmapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {0x90000000u, 15};
    check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.operations == 1);

    Fixture unknown_sp;
    unknown_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
        .known_mask = 7;
    check(unknown_sp.run() == NBA97_TEXT_UNKNOWN &&
        unknown_sp.progress.operations == 0 &&
        unknown_sp.progress.stopped_pc == 0x8008444cu);

    Fixture unknown_ra;
    unknown_ra.put(Head, 0);
    unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = 7;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x80084580u &&
        unknown_ra.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == EntrySp);

    Fixture no_bitmap;
    no_bitmap.region.known = nullptr;
    no_bitmap.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = 7;
    check(no_bitmap.run() == NBA97_TEXT_ARGUMENT &&
        no_bitmap.progress.operations == 1 && no_bitmap.progress.stores == 0);

    Fixture bad_known;
    bad_known.known[bad_known.offset(Frame + 0x1cu)] = 2;
    check(bad_known.run() == NBA97_TEXT_ARGUMENT &&
        bad_known.progress.operations == 1);

    Fixture wrapped;
    std::array<std::uint8_t, 0x10> low{};
    std::array<std::uint8_t, 0x10> low_known{};
    low_known.fill(1);
    std::array<Nba97GameTextRegion, 2> regions{{
        {0, low.data(), low_known.data(), low.size()}, wrapped.region}};
    wrapped.context.memory = {regions.data(), regions.size()};
    wrapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {0x10u, 15};
    wrapped.put(Head, 0);
    check(wrapped.run() == NBA97_TEXT_COMPLETE &&
        wrapped.progress.frame_stack_pointer == 0xfffffff0u &&
        wrapped.journal[0].address == 0x0000000cu &&
        wrapped.journal[1].address == 0x00000008u &&
        wrapped.journal[2].address == 0x00000004u &&
        wrapped.journal[4].address == 0x0000000cu &&
        wrapped.journal[5].address == 0x00000008u &&
        wrapped.progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_SP].word == 0x10u);

    Fixture overlap;
    std::array<Nba97GameTextRegion, 2> overlap_regions{{
        overlap.region, overlap.region}};
    overlap.context.memory = {overlap_regions.data(), overlap_regions.size()};
    check(overlap.run() == NBA97_TEXT_ARGUMENT);
    Fixture missing_regions;
    missing_regions.context.memory.region = nullptr;
    check(missing_regions.run() == NBA97_TEXT_ARGUMENT);
    Fixture null_data;
    null_data.region.data = nullptr;
    check(null_data.run() == NBA97_TEXT_ARGUMENT);
    Fixture zero_size;
    zero_size.region.size = 0;
    check(zero_size.run() == NBA97_TEXT_ARGUMENT);
    Fixture missing_journal;
    missing_journal.context.access_journal = nullptr;
    check(missing_journal.run() == NBA97_TEXT_ARGUMENT);
    Fixture invalid_machine;
    invalid_machine.context.machine.lo.known_mask = 16;
    check(invalid_machine.run() == NBA97_TEXT_ARGUMENT);
    Nba97GameStreamQueueCountProgress progress{};
    check(nba97_game_stream_queue_count(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_stream_queue_count(&invalid_machine.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
}

int main() {
    emptySentinelsAndBasicLists();
    ExactCallsCounterWrapAndRepeatedReads();
    CallbackMutationAndPostUnlockRawCount();
    AliasesUnknownsRefusalsAndCycles();
    EveryBudgetPrefixAndMappingMetadata();
    std::printf("%u game stream queue count checks passed\n", checks);
    return 0;
}
