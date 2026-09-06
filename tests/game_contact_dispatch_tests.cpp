#include "recovered/game_contact_dispatch.h"

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
        std::fprintf(stderr, "contact dispatch check %u failed at %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Sp = 0x800ff000u;
constexpr std::uint32_t Ra = 0x81234567u;
constexpr std::uint32_t Ball = 0x80018000u;

struct CallRecord {
    Nba97GameContactDispatchEvent event{};
    Nba97GameContactDispatchMachine machine{};
};

struct Fixture {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000, 0);
    std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x110000, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameContactDispatchAccess journal[256]{};
    Nba97GameContactDispatchContext context{};
    Nba97GameContactDispatchProgress progress{};
    std::vector<CallRecord> calls;
    std::uint32_t return_word{1};
    std::uint32_t refuse_pc{};
    unsigned mutate_on_call{};
    bool malformed{};
    bool relocate{};
    std::uint32_t moved_sp{0x800fe000u};
    std::uint32_t mutate_s0{};
    std::uint32_t mutate_s1{};
    std::uint32_t mutate_s2{};
    bool mutate_s0_set{};
    bool mutate_s1_set{};
    bool mutate_s2_set{};
    bool mutate_ball{};
    std::uint32_t new_ball{};

    Fixture() {
        context.memory = {&region, 1};
        context.operation_budget = 1000;
        context.io = io;
        context.user = this;
        context.access_journal = journal;
        context.access_journal_capacity = std::size(journal);
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.machine.registers.gpr[i] =
                {0x24000000u + i * 0x01010101u, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Sp, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {Ra, 0x0f};
        context.machine.hi = {0x11223344u, 0x0f};
        context.machine.lo = {0x55667788u, 0x0f};
        put(0x800fdc48u, Ball, 4);
        put(Ball + 0xb4u, 0, 2);
        for (int i = -4; i <= 11; ++i)
            put(0x800fdcbcu + std::uint32_t(i * 4), object(i), 4);
    }

    static std::uint32_t object(int index) {
        return 0x80020000u + std::uint32_t(index * 0x100);
    }
    std::size_t offset(std::uint32_t address) const {
        return static_cast<std::size_t>(address - Ram);
    }
    void put(std::uint32_t address, std::uint32_t value, unsigned width) {
        auto at = offset(address);
        for (unsigned i = 0; i < width; ++i) {
            bytes[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
            known[at + i] = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width) const {
        auto at = offset(address);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[at + i]) << (8u * i);
        return value;
    }
    void set_known(std::uint32_t address, unsigned width, std::uint8_t mask) {
        auto at = offset(address);
        for (unsigned i = 0; i < width; ++i)
            known[at + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
    }
    void place_ball(unsigned index, std::uint16_t state = 0) {
        put(0x800fdcbcu + index * 4u, Ball, 4);
        put(Ball + 0xb4u, state, 2);
    }
    static void raw_put(const Nba97GameTextMemory* memory,
        std::uint32_t address, std::uint32_t value, unsigned width) {
        for (std::size_t r = 0; r < memory->count; ++r) {
            auto& region = memory->region[r];
            if (address >= region.base && address - region.base <= region.size - width) {
                auto at = static_cast<std::size_t>(address - region.base);
                for (unsigned i = 0; i < width; ++i) {
                    region.data[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
                    if (region.known) region.known[at + i] = 1;
                }
                return;
            }
        }
    }
    static int io(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameContactDispatchEvent* event,
        Nba97GameContactDispatchMachine* machine) {
        auto& f = *static_cast<Fixture*>(user);
        f.calls.push_back({*event, *machine});
        unsigned call_number = static_cast<unsigned>(f.calls.size());
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
            {f.return_word, 0x0f};
        if (call_number == f.mutate_on_call) {
            if (f.mutate_s0_set)
                machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] =
                    {f.mutate_s0, 0x0f};
            if (f.mutate_s1_set)
                machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1] =
                    {f.mutate_s1, 0x0f};
            if (f.mutate_s2_set)
                machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S2] =
                    {f.mutate_s2, 0x0f};
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_T9] =
                {0xabcdef01u, 0x05};
            machine->hi = {0xaabbccddu, 0x03};
            machine->lo = {0x13579bdfu, 0x0a};
            if (f.relocate) {
                machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
                    {f.moved_sp, 0x0f};
                raw_put(memory, f.moved_sp + 0x1cu, 0x87654321u, 4);
                raw_put(memory, f.moved_sp + 0x18u, 0x10203040u, 4);
                raw_put(memory, f.moved_sp + 0x14u, 0x50607080u, 4);
                raw_put(memory, f.moved_sp + 0x10u, 0x90a0b0c0u, 4);
            }
            if (f.mutate_ball)
                raw_put(memory, 0x800fdc48u, f.new_ball, 4);
        }
        if (f.malformed)
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask = 0;
        return event->pc == f.refuse_pc ? 0 : 1;
    }
    int run(std::size_t budget = 1000) {
        context.operation_budget = budget;
        return nba97_game_contact_dispatch(&context, &progress);
    }
};

void complete_nonball_pair_scans() {
    Fixture all_pairs;
    all_pairs.return_word = 1;
    check(all_pairs.run() == NBA97_TEXT_COMPLETE && all_pairs.progress.completed);
    check(all_pairs.calls.size() == 55 &&
        all_pairs.progress.call_count[NBA97_GAME_CONTACT_DISPATCH_CHILD_8005FAA8] == 55 &&
        all_pairs.progress.call_count[NBA97_GAME_CONTACT_DISPATCH_CHILD_80060E8C] == 0);
    std::size_t at = 0;
    for (unsigned i = 1; i <= 10; ++i)
        for (unsigned j = i + 1; j <= 11; ++j) {
            const auto& c = all_pairs.calls[at++];
            check(c.event.pc == 0x8006104cu && c.event.delay_slot_pc == 0x80061050u &&
                c.event.entry == 0x8005faa8u && c.event.argument_count == 2);
            check(c.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == Fixture::object(i) &&
                c.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == Fixture::object(j) &&
                c.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x80061054u);
        }
    check(all_pairs.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 48 &&
        all_pairs.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V1].word == 12);
    check(all_pairs.progress.frame_stack_pointer == Sp - 0x20u &&
        all_pairs.progress.restored_return_address.word == Ra &&
        all_pairs.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == Sp);

    Fixture early;
    early.return_word = 0;
    check(early.run() == NBA97_TEXT_COMPLETE && early.calls.size() == 10);
    for (unsigned i = 0; i < early.calls.size(); ++i)
        check(early.calls[i].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            Fixture::object(static_cast<int>(i + 1)) &&
            early.calls[i].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            Fixture::object(static_cast<int>(i + 2)));
}

void all_ball_positions_and_state_gate() {
    for (unsigned ball_index = 1; ball_index <= 11; ++ball_index) {
        Fixture active;
        active.place_ball(ball_index, 1);
        active.return_word = 1;
        check(active.run() == NBA97_TEXT_COMPLETE);
        check(active.progress.call_count[NBA97_GAME_CONTACT_DISPATCH_CHILD_8005FAA8] == 45 &&
            active.progress.call_count[NBA97_GAME_CONTACT_DISPATCH_CHILD_80060E8C] == 0);

        Fixture available;
        available.place_ball(ball_index, 0);
        available.return_word = 1;
        check(available.run() == NBA97_TEXT_COMPLETE);
        check(available.progress.call_count[NBA97_GAME_CONTACT_DISPATCH_CHILD_8005FAA8] == 45 &&
            available.progress.call_count[NBA97_GAME_CONTACT_DISPATCH_CHILD_80060E8C] == 10);
        for (const auto& c : available.calls)
            if (c.event.entry == 0x80060e8cu) {
                check(c.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == Ball);
                check(c.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word != Ball);
                check((c.event.pc == 0x80061070u && c.event.delay_slot_pc == 0x80061074u) ||
                    (c.event.pc == 0x800610c4u && c.event.delay_slot_pc == 0x800610c8u));
            }
    }
}

void child_low_byte_and_early_exit() {
    struct ReturnCase { std::uint32_t word; std::size_t calls; };
    const std::array<ReturnCase, 7> cases{{
        {0,10}, {1,55}, {0xff,55}, {0x100,10},
        {0xffffffffu,55}, {0xffffff00u,10}, {0x12340000u,10}}};
    for (const auto& c : cases) {
        Fixture f;
        f.return_word = c.word;
        check(f.run() == NBA97_TEXT_COMPLETE && f.calls.size() == c.calls);
    }

    Fixture ball_early;
    ball_early.place_ball(1, 0);
    ball_early.return_word = 0;
    check(ball_early.run() == NBA97_TEXT_COMPLETE);
    check(ball_early.progress.call_count[NBA97_GAME_CONTACT_DISPATCH_CHILD_80060E8C] == 1);
}

void callback_live_machine_and_memory() {
    Fixture moved;
    moved.return_word = 0;
    moved.mutate_on_call = 1;
    moved.mutate_s0_set = true;
    moved.mutate_s0 = 0x8002f000u;
    moved.mutate_s1_set = true;
    moved.mutate_s1 = 10;
    moved.mutate_s2_set = true;
    moved.mutate_s2 = 10;
    moved.relocate = true;
    check(moved.run() == NBA97_TEXT_COMPLETE && moved.calls.size() == 1);
    check(moved.progress.restored_return_address.word == 0x87654321u &&
        moved.progress.restored_s2.word == 0x10203040u &&
        moved.progress.restored_s1.word == 0x50607080u &&
        moved.progress.restored_s0.word == 0x90a0b0c0u &&
        moved.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == moved.moved_sp + 0x20u);
    check(moved.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T9].known_mask == 0x05 &&
        moved.progress.machine.hi.word == 0xaabbccddu && moved.progress.machine.hi.known_mask == 3 &&
        moved.progress.machine.lo.word == 0x13579bdfu && moved.progress.machine.lo.known_mask == 0x0a);

    Fixture live_s0;
    live_s0.return_word = 1;
    live_s0.mutate_on_call = 1;
    live_s0.mutate_s0_set = true;
    live_s0.mutate_s0 = 0x8002f000u;
    check(live_s0.run() == NBA97_TEXT_COMPLETE && live_s0.calls.size() == 55);
    check(live_s0.calls[1].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 0x8002f000u);

    Fixture live_ball;
    live_ball.return_word = 0;
    live_ball.mutate_on_call = 1;
    live_ball.mutate_ball = true;
    live_ball.new_ball = Fixture::object(3);
    live_ball.put(Fixture::object(3) + 0xb4u, 1, 2);
    check(live_ball.run() == NBA97_TEXT_COMPLETE);
    bool saw_ball_state_read = false;
    for (std::size_t i = 0; i < live_ball.progress.access_events; ++i)
        if (live_ball.journal[i].pc == 0x8006105cu &&
            live_ball.journal[i].address == Fixture::object(3) + 0xb4u)
            saw_ball_state_read = true;
    check(saw_ball_state_read);
}

void signed_low_half_indices_and_aliases() {
    Fixture negative_inner;
    negative_inner.return_word = 1;
    negative_inner.mutate_on_call = 1;
    negative_inner.mutate_s1_set = true;
    negative_inner.mutate_s1 = 0xfffffffdu;
    check(negative_inner.run() == NBA97_TEXT_COMPLETE && negative_inner.calls.size() >= 2);
    check(negative_inner.calls[1].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
        Fixture::object(-2));

    Fixture negative_outer;
    negative_outer.return_word = 0;
    negative_outer.mutate_on_call = 1;
    negative_outer.mutate_s2_set = true;
    negative_outer.mutate_s2 = 0x0000fffeu;
    check(negative_outer.run() == NBA97_TEXT_COMPLETE && negative_outer.calls.size() >= 2);
    check(negative_outer.calls[1].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        Fixture::object(-1));

    Fixture duplicate;
    duplicate.put(0x800fdcbcu + 2u * 4u, Fixture::object(1), 4);
    duplicate.return_word = 0;
    check(duplicate.run() == NBA97_TEXT_COMPLETE &&
        duplicate.calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            duplicate.calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word);
}

void refusals_unknowns_mapping_and_metadata() {
    Fixture refused_actor;
    refused_actor.refuse_pc = 0x8006104cu;
    check(refused_actor.run() == NBA97_TEXT_IO_REFUSED &&
        refused_actor.progress.stopped_pc == 0x8006104cu &&
        refused_actor.progress.stopped_entry == 0x8005faa8u);

    Fixture refused_ai;
    refused_ai.place_ball(2, 0);
    refused_ai.refuse_pc = 0x80061070u;
    check(refused_ai.run() == NBA97_TEXT_IO_REFUSED &&
        refused_ai.progress.stopped_entry == 0x80060e8cu);

    Fixture malformed;
    malformed.malformed = true;
    check(malformed.run() == NBA97_TEXT_ARGUMENT);

    Fixture unknown_reference;
    unknown_reference.set_known(0x800fdcbcu + 4u, 4, 0);
    check(unknown_reference.run() == NBA97_TEXT_UNKNOWN &&
        unknown_reference.progress.stopped_pc == 0x80061008u &&
        unknown_reference.progress.machine.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1].word == 1);

    Fixture unknown_result;
    unknown_result.context.io = [](void*, const Nba97GameTextMemory*,
        const Nba97GameContactDispatchEvent*, Nba97GameContactDispatchMachine* m) {
        m->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0e};
        return 1;
    };
    check(unknown_result.run() == NBA97_TEXT_UNKNOWN &&
        unknown_result.progress.stopped_pc == 0x8006107cu &&
        unknown_result.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 3);

    Fixture unknown_ball_state;
    unknown_ball_state.place_ball(2, 0);
    unknown_ball_state.set_known(Ball + 0xb4u, 2, 0);
    check(unknown_ball_state.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ball_state.progress.stopped_pc == 0x80061064u);

    Fixture unaligned_ball;
    unaligned_ball.put(0x800fdc48u, Ball + 1u, 4);
    unaligned_ball.put(0x800fdcbcu + 2u * 4u, Ball + 1u, 4);
    check(unaligned_ball.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_ball.progress.stopped_pc == 0x8006105cu);

    Fixture unmapped_ball;
    unmapped_ball.put(0x800fdc48u, 0, 4);
    unmapped_ball.put(0x800fdcbcu + 2u * 4u, 0, 4);
    check(unmapped_ball.run() == NBA97_TEXT_RESOURCE &&
        unmapped_ball.progress.stopped_address == 0xb4u);

    Fixture unaligned_stack;
    unaligned_stack.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {Sp + 1u, 0x0f};
    check(unaligned_stack.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_stack.progress.stopped_pc == 0x80060fc0u);

    Fixture unknown_stack;
    unknown_stack.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {Sp, 0x0e};
    check(unknown_stack.run() == NBA97_TEXT_UNKNOWN &&
        unknown_stack.progress.stopped_pc == 0x80060fc0u &&
        unknown_stack.progress.operations == 0);

    Fixture unknown_ra;
    unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {0x81230000u, 0x0c};
    unknown_ra.return_word = 0;
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x800610f4u &&
        unknown_ra.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == Sp);

    Fixture bad_regions;
    bad_regions.region.size = 0;
    check(bad_regions.run() == NBA97_TEXT_ARGUMENT);
}

void wrapped_stack_frame() {
    Fixture f;
    std::vector<uint8_t> low(0x20), knowledge(0x20,1);
    Nba97GameTextRegion regions[2]={f.region,{0,low.data(),knowledge.data(),low.size()}};
    f.context.memory={regions,2};
    f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]={0x10,15};
    f.return_word=0;
    check(f.run()==NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.frame_stack_pointer==0xfffffff0 &&
          f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word==0x10);
    check(f.progress.restored_return_address.word==Ra);
}

void every_operation_budget_prefix() {
    Fixture complete;
    complete.return_word = 0;
    check(complete.run() == NBA97_TEXT_COMPLETE);
    auto operations = complete.progress.operations;
    check(operations > 40 && operations < std::size(complete.journal));
    for (std::size_t budget = 0; budget < operations; ++budget) {
        Fixture f;
        f.return_word = 0;
        check(f.run(budget) == NBA97_TEXT_LIMIT && f.progress.operations == budget);
        check(f.progress.access_events <= complete.progress.access_events &&
            f.calls.size() <= complete.calls.size());
        for (std::size_t i = 0; i < f.progress.access_events; ++i)
            check(f.journal[i].pc == complete.journal[i].pc &&
                f.journal[i].address == complete.journal[i].address &&
                f.journal[i].kind == complete.journal[i].kind);
    }
}
}

int main() {
    complete_nonball_pair_scans();
    all_ball_positions_and_state_gate();
    child_low_byte_and_early_exit();
    callback_live_machine_and_memory();
    signed_low_half_indices_and_aliases();
    refusals_unknowns_mapping_and_metadata();
    every_operation_budget_prefix();
    wrapped_stack_frame();
    std::printf("contact dispatch tests passed: %u checks\n", checks);
}
