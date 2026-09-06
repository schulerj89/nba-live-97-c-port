#include "recovered/game_ball_acquire.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "game ball acquire check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(x) check_at((x), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Actor = 0x80110000u;
constexpr std::uint32_t Descriptor = 0x80120000u;
constexpr std::uint32_t Stats = 0x80130000u;
constexpr std::uint32_t ControllerStats = 0x80140000u;
constexpr std::uint32_t OriginalControllerStats = 0x80150000u;
constexpr std::uint32_t Team0 = 0x8001edf4u;
constexpr std::uint32_t Team1 = 0x8001eeb8u;
constexpr std::uint32_t AlternateTeam = 0x8001f000u;
constexpr std::uint32_t UnselectedTeam = 0x8001f100u;
constexpr std::uint32_t AlternateControllerTable = 0x800fda02u;

struct Call {
    Nba97GameBallAcquireEvent event{};
    Nba97GameBallAcquireMachine machine{};
};
struct Fixture {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000u);
    std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameBallAcquireContext context{};
    Nba97GameBallAcquireProgress progress{};
    std::vector<Nba97GameBallAcquireAccess> journal =
        std::vector<Nba97GameBallAcquireAccess>(1024);
    std::vector<Call> calls;
    bool refuse = false;
    bool relocate_on_effect = false;
    bool malformed_zero = false;
    bool malformed_mask = false;
    bool mutate_saved_on_stat = false;
    std::uint32_t rng = 7;

    Fixture() {
        context.memory = {&region, 1};
        context.operation_budget = 2000;
        context.io = io;
        context.user = this;
        context.access_journal = journal.data();
        context.access_journal_capacity = journal.size();
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.machine.registers.gpr[i] = {
                0x11000000u + i * 0x01010101u,
                static_cast<std::uint8_t>((i % 15u) + 1u)};
        context.machine.registers.gpr[0] = {0, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
            {Actor, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {0x801fff00u, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x800608a4u, 0x0f};
        context.machine.hi = {0x13579bdfu, 0x05};
        context.machine.lo = {0x2468ace0u, 0x0a};

        put(Actor, 4); put(Actor + 4, 0xffffu, 2);
        put(Actor + 8, 0x1000u); put(Actor + 0x0c, 0x2000u);
        put(Actor + 0x10, 0); put(Actor + 0x14, 0xfffdu, 2);
        put(Actor + 0x16, 7, 2); put(Actor + 0x1a, 0, 1);
        put(Actor + 0x1c, Stats); put(Actor + 0x20, Descriptor);
        put(Actor + 0x46, 44, 2); put(Actor + 0x9a, 0xa5a4u, 2);
        put(Actor + 0xa0, 384, 2); put(Actor + 0xd9, 0, 1);
        put(Actor + 0xde, 255, 1); put(Actor + 0xdf, 255, 1);
        put(Descriptor + 0x0d, 1, 1);
        for (auto off : {0x0cu, 0x0eu, 0x14u, 0x16u, 0x18u})
            put(Stats + off, 998, 2);
        put(Team0 + 4, Team1); put(Team1 + 4, Team0);
        put(Team0 + 0x10, 0x1000u); put(Team1 + 0x10, 0x2000u);
        put(Team0 + 0x52, 5, 2); put(Team0 + 0x54, 2, 2);
        put(Team1 + 0x52, 6, 2); put(Team1 + 0x54, 3, 2);
        put(0x80020becu + 5u * 4u, Actor);
        put(0x800fdc40u, Team0);
        put(0x800fa034u, 0xffffffffu); put(0x800fa038u, 0, 2);
        put(0x80021d95u, 0, 1); put(0x800fdb58u, 0x12345678u);
        put(0x800fdb84u, 1, 2); put(0x800fdb90u, 0x7f, 2);
        put(0x800fdb94u, 1, 2); put(0x800fdb96u, 1, 2);
        put(0x800fdbb0u, 0, 2); put(0x800fdbca, 0, 2);
        put(0x800fdbd2u, 0, 2); put(0x800fdbd4u, 0, 2);
        put(0x800fdbd8u, 0, 2); put(0x800fe882u, 0, 2);
        put(0x800fe8ccu, 0, 2);
    }
    std::size_t at(std::uint32_t address) const { return address - Ram; }
    void put(std::uint32_t address, std::uint32_t value, unsigned width = 4) {
        for (unsigned i = 0; i < width; ++i) {
            bytes[at(address) + i] = static_cast<std::uint8_t>(value >> (8u * i));
            known[at(address) + i] = 1;
        }
    }
    std::uint32_t get(std::uint32_t address, unsigned width = 4) const {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= std::uint32_t(bytes[at(address) + i]) << (8u * i);
        return value;
    }
    void hide(std::uint32_t address, unsigned width) {
        for (unsigned i = 0; i < width; ++i) known[at(address) + i] = 0;
    }
    int run() { return nba97_game_ball_acquire(&context, &progress); }
    static int io(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameBallAcquireEvent* event,
        Nba97GameBallAcquireMachine* machine) {
        auto& f = *static_cast<Fixture*>(opaque);
        if (!event) return 0;
        f.calls.push_back({*event, *machine});
        if (f.refuse) return 0;
        if (event->kind == NBA97_GAME_BALL_ACQUIRE_CHILD_8002AB70)
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {f.rng, 0x0f};
        if (f.relocate_on_effect &&
            event->kind == NBA97_GAME_BALL_ACQUIRE_CHILD_80029590) {
            const auto old_sp = machine->registers.gpr[
                NBA97_MATCH_INITIALIZE_SP].word;
            const auto new_sp = old_sp - 0x40u;
            for (unsigned i = 0; i < 0x30u; ++i) {
                f.bytes[f.at(new_sp + i)] = f.bytes[f.at(old_sp + i)];
                f.known[f.at(new_sp + i)] = f.known[f.at(old_sp + i)];
            }
            const std::uint32_t restored[] = {
                0xaaa00010u, 0xaaa00011u, 0xaaa00012u,
                0xaaa00013u, 0xaaa00031u
            };
            for (unsigned i = 0; i < 4; ++i)
                f.put(new_sp + 0x18u + i * 4u, restored[i]);
            f.put(new_sp + 0x28u, restored[4]);
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {new_sp, 0x0f};
            for (unsigned i = 16; i <= 19; ++i)
                machine->registers.gpr[i] = {0xbbbb0000u + i, 0x03};
        }
        if (f.mutate_saved_on_stat && event->pc == 0x8005d68cu) {
            machine->registers.gpr[16] = {AlternateTeam, 0x0f};
            machine->registers.gpr[18] = {AlternateControllerTable, 0x0f};
        }
        if (f.malformed_zero)
            machine->registers.gpr[0] = {1, 0x0f};
        if (f.malformed_mask)
            machine->registers.gpr[7].known_mask = 0x10;
        machine->hi = {event->pc ^ 0x11111111u, 0x09};
        machine->lo = {event->pc ^ 0x22222222u, 0x06};
        return 1;
    }
};

void change_team_and_publication() {
    Fixture f;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.possession_changed && !f.progress.same_team_claim);
    check(f.get(0x800fdc34u) == Actor && f.get(0x800fdc38u) == Team0);
    check(f.get(0x800fdbccu, 2) == 4 && f.get(Actor + 0xcau, 1) == 1);
    check((f.get(Actor + 0x9au, 2) & 3u) == 3u);
    check(f.get(Actor + 0xb8u, 2) == 100 && f.get(Actor + 0xe8u, 2) == 30);
    check(f.get(Actor + 0x14u, 2) == 0 && f.get(Actor + 0x16u, 2) == 0);
    check(f.get(Team0 + 0x52u, 2) == 4 && f.get(Team0 + 0x54u, 2) == 0xffffu);
    check(f.get(Team0 + 0x56u, 2) == 0xffffu && f.get(Team0 + 0x58u, 2) == 0xffffu);
    check(f.get(0x800fdb94u, 2) == 0 && f.get(0x800fdb96u, 2) == 0);
    check(f.calls.size() == 3);
    check(f.calls[0].event.pc == 0x8005d464u &&
        f.calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == Team1);
    check(f.calls[1].event.pc == 0x8005d4a8u &&
        f.calls[1].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 20000u);
    check(f.calls[2].event.pc == 0x8005d4b4u &&
        f.calls[2].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 2u);
    check(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 0);
    check(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word == 0x801fff00u);
}

void random_rule_and_signed_team_split() {
    for (auto side : {0u, 1u, 255u}) {
        Fixture f;
        f.put(0x800fa034u, 0); f.put(0x800fdb90u, 0x82, 2);
        f.put(0x80021d95u, 8, 1); f.put(Actor + 0xd9u, side, 1);
        f.put(0x800fdb94u, side, 2);
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(f.progress.random_rule_set && f.get(0x800fa038u, 2) == 1);
        check(!f.calls.empty() && f.calls[0].event.pc == 0x8005d1a4u &&
            f.calls[0].event.argument_count == 0);
        check(f.get(0x800fdc38u) == (side ? Team1 : Team0));
    }
    for (auto rule : {0u, 1u, 8u, 255u}) {
        Fixture f;
        f.put(0x800fa034u, 0); f.put(0x800fdb90u, 0x82, 2);
        f.put(0x80021d95u, rule, 1); f.rng = 7;
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(bool(f.progress.random_rule_set) == (rule != 0 && 7u < rule));
    }
}

void descriptor_unknown_prefix_and_refusal() {
    Fixture f;
    f.hide(Descriptor + 0x0d, 1);
    check(f.run() == NBA97_TEXT_UNKNOWN && !f.progress.completed);
    check(f.progress.stopped_pc == 0x8005d29cu);
    check(f.get(0x800fdc34u) == Actor && f.get(0x800fdc38u) == Team0);
    check(f.get(0x800fdbccu, 2) == 4);
    check(f.known[f.at(Actor + 0xcau)] == 0);

    Fixture refused;
    refused.refuse = true;
    check(refused.run() == NBA97_TEXT_IO_REFUSED);
    check(refused.progress.stopped_pc == 0x8005d464u &&
        refused.progress.stopped_entry == 0x80072c40u);
    check(refused.get(0x800fdb94u, 2) == 0 &&
        refused.get(0x800fdb96u, 2) == 0);
}

void state_preservation_and_mapping_failures() {
    for (auto state : {15u, 19u}) {
        Fixture f;
        f.put(Actor + 0x1au, state, 1);
        f.put(Actor + 0xdau, 0x5au, 1);
        f.put(Actor + 0xb6u, 0x1234u, 2);
        check(f.run() == NBA97_TEXT_COMPLETE);
        check(f.get(Actor + 0x1au, 1) == state &&
            f.get(Actor + 0xdau, 1) == 0x5au &&
            f.get(Actor + 0xb6u, 2) == 0x1234u &&
            f.get(Actor + 0x98u, 2) == 0);
    }
    Fixture reset;
    reset.put(Actor + 0x1au, 7, 1); reset.put(Actor + 0xdau, 0x5au, 1);
    reset.put(Actor + 0xb6u, 0x1234u, 2);
    check(reset.run() == NBA97_TEXT_COMPLETE);
    check(reset.get(Actor + 0x1au, 1) == 11 &&
        reset.get(Actor + 0xdau, 1) == 0 && reset.get(Actor + 0xb6u, 2) == 0 &&
        reset.get(0x800fa034u) == 0xffffffffu);

    Fixture nested;
    nested.put(Actor + 0x20u, 0x80200000u);
    check(nested.run() == NBA97_TEXT_RESOURCE &&
        nested.progress.stopped_pc == 0x8005d294u);
    check(nested.get(0x800fdc34u) == Actor &&
        nested.get(0x800fdc38u) == Team0 && nested.get(0x800fdbccu, 2) == 4);

    Fixture unaligned;
    unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        {Actor + 1u, 0x0f};
    check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x8005d218u);

    Fixture wrapped;
    wrapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {0x20u, 0x0f};
    check(wrapped.run() == NBA97_TEXT_RESOURCE &&
        wrapped.progress.stopped_pc == 0x8005d14cu &&
        wrapped.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0xfffffff0u);

    Fixture metadata;
    metadata.known[metadata.at(0x800fa034u)] = 2;
    check(metadata.run() == NBA97_TEXT_ARGUMENT &&
        metadata.progress.stopped_pc == 0x8005d144u);
}

void unknown_compare_and_delay_slots() {
    Fixture slti;
    slti.hide(Actor + 0xa0u, 1);
    check(slti.run() == NBA97_TEXT_UNKNOWN &&
        slti.progress.stopped_pc == 0x8005d224u);
    check(slti.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 1 &&
        slti.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask ==
            0x0e);

    Fixture delayed;
    delayed.put(Actor + 0xa0u, 385, 2);
    delayed.put(0x800fdb90u, 0x7f, 2);
    delayed.hide(0x800fdb90u, 1);
    delayed.put(Actor + 0x0cu, 0x76543210u);
    check(delayed.run() == NBA97_TEXT_UNKNOWN &&
        delayed.progress.stopped_pc == 0x8005d2e4u);
    check(delayed.get(0x800fdbf8u) == 0x76543210u);
    check(delayed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0xffffffffu &&
        delayed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask ==
            0x0f);

    Fixture bltz;
    bltz.put(Actor + 0xa0u, 385, 2);
    bltz.put(0x800fdbca, 1, 2); bltz.put(0x800fdbd2u, 0xffffu, 2);
    bltz.put(0x800fdbd4u, 0xffffu, 2); bltz.put(0x800fdb84u, 0, 2);
    bltz.put(Stats + 0x0cu, 999, 2);
    bltz.put(Actor + 4u, 1, 2); bltz.hide(Actor + 5u, 1);
    check(bltz.run() == NBA97_TEXT_UNKNOWN &&
        bltz.progress.stopped_pc == 0x8005d580u);
    check(bltz.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 4 &&
        bltz.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask ==
            0x01);
}

void same_team_velocity_and_caps() {
    Fixture f;
    f.put(Actor, 5); f.put(Actor + 4, 0xffffu, 2);
    f.put(Actor + 0xa0u, 385, 2);
    f.put(0x800fdb94u, 0, 2); f.put(0x800fdb96u, 9, 2);
    f.put(0x800fdbd8u, 1, 2); f.put(Actor + 0x46u, 43, 2);
    f.put(Actor + 0x14u, 0xfffdu, 2); f.put(Actor + 0x16u, 7, 2);
    f.put(Stats + 0x0eu, 999, 2);
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.same_team_claim);
    check(f.get(Actor + 0xdeu, 1) == 0);
    check(f.get(Stats + 0x0eu, 2) == 999);
    check(f.get(Actor + 0x14u, 2) == 0xfffeu && f.get(Actor + 0x16u, 2) == 3);
    check(f.calls.size() == 3 && f.calls[0].event.pc == 0x8005d898u &&
        f.calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 10000u);
    check(f.calls[1].event.pc == 0x8005d8b4u &&
        f.calls[1].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 3u);
    check(f.calls[2].event.pc == 0x8005d9c0u &&
        f.calls[2].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == Actor);
}

void budgets_and_validation() {
    Fixture baseline;
    check(baseline.run() == NBA97_TEXT_COMPLETE);
    const auto operations = baseline.progress.operations;
    for (std::size_t budget = 0; budget < operations; ++budget) {
        Fixture f; f.context.operation_budget = budget;
        check(f.run() == NBA97_TEXT_LIMIT && !f.progress.completed);
        check(f.progress.operations == budget);
    }
    Fixture bad;
    bad.context.machine.registers.gpr[0].known_mask = 0;
    check(bad.run() == NBA97_TEXT_ARGUMENT);
    Fixture overlap;
    Nba97GameTextRegion regions[2] = {overlap.region, overlap.region};
    overlap.context.memory = {regions, 2};
    check(overlap.run() == NBA97_TEXT_ARGUMENT);
    check(nba97_game_ball_acquire(nullptr, &bad.progress) == NBA97_TEXT_ARGUMENT);
    check(nba97_game_ball_acquire(&bad.context, nullptr) == NBA97_TEXT_ARGUMENT);

    Fixture no_provenance;
    no_provenance.region.known = nullptr;
    check(no_provenance.run() == NBA97_TEXT_ARGUMENT);
    check(no_provenance.progress.stopped_pc == 0x8005d14cu &&
        no_provenance.progress.operations == 2 &&
        no_provenance.progress.accesses == 2 &&
        no_provenance.progress.reads == 1 &&
        no_provenance.progress.stores == 0);
    check(no_provenance.get(0x801ffeecu) == 0);

    Fixture bad_zero;
    bad_zero.malformed_zero = true;
    check(bad_zero.run() == NBA97_TEXT_ARGUMENT);
    check(bad_zero.progress.stopped_pc == 0x8005d464u &&
        bad_zero.progress.stopped_entry == 0x80072c40u &&
        bad_zero.progress.callbacks_completed == 0 &&
        bad_zero.progress.call_count[
            NBA97_GAME_BALL_ACQUIRE_CHILD_80072C40] == 1);
    check(bad_zero.progress.machine.registers.gpr[0].word == 1);

    Fixture bad_mask;
    bad_mask.malformed_mask = true;
    check(bad_mask.run() == NBA97_TEXT_ARGUMENT);
    check(bad_mask.progress.stopped_pc == 0x8005d464u &&
        bad_mask.progress.stopped_entry == 0x80072c40u &&
        bad_mask.progress.callbacks_completed == 0);
    check(bad_mask.progress.machine.registers.gpr[7].known_mask == 0x10);
}

void change_team_stat_branches_and_wraps() {
    Fixture turnover;
    turnover.put(0x800fdbca, 1, 2);
    turnover.put(0x800fdbd2u, 0xffffu, 2);
    turnover.put(0x800fdbd4u, 0xffffu, 2);
    turnover.put(0x800fdb84u, 0, 2);
    turnover.put(Actor + 4u, 1, 2);
    turnover.put(Actor + 0xdfu, 0xffu, 1);
    turnover.put(Stats + 0x0cu, 998, 2);
    turnover.put(0x800fdc54u, ControllerStats);
    turnover.put(ControllerStats + 0x0cu, 0xffffu, 2);
    check(turnover.run() == NBA97_TEXT_COMPLETE);
    check(turnover.get(Actor + 0xdfu, 1) == 0 &&
        turnover.get(Stats + 0x0cu, 2) == 999 &&
        turnover.get(ControllerStats + 0x0cu, 2) == 0);

    Fixture steal_cap;
    steal_cap.put(0x800fdbca, 1, 2);
    steal_cap.put(0x800fdbd2u, 0xffffu, 2);
    steal_cap.put(0x800fdbd4u, 0, 2);
    steal_cap.put(0x800fdbd8u, 1, 2);
    steal_cap.put(0x800fdb84u, 0, 2);
    steal_cap.put(Actor + 4u, 1, 2);
    steal_cap.put(Stats + 0x16u, 999, 2);
    steal_cap.put(0x800fdc54u, ControllerStats);
    steal_cap.put(ControllerStats + 0x16u, 41, 2);
    check(steal_cap.run() == NBA97_TEXT_COMPLETE);
    check(steal_cap.get(Stats + 0x16u, 2) == 999 &&
        steal_cap.get(ControllerStats + 0x16u, 2) == 42);

    Fixture assist;
    assist.put(0x800fdbca, 1, 2);
    assist.put(0x800fdbd2u, 0xffffu, 2);
    assist.put(0x800fdbd4u, 1, 2);
    assist.put(0x800fdb84u, 0, 2);
    assist.put(Actor + 4u, 1, 2);
    assist.put(Stats + 0x14u, 998, 2);
    assist.put(Team1 + 0x52u, 0xffffu, 2);
    assist.put(0x800fdc54u, ControllerStats);
    assist.put(ControllerStats + 0x14u, 12, 2);
    check(assist.run() == NBA97_TEXT_COMPLETE);
    check(assist.get(Stats + 0x14u, 2) == 999 &&
        assist.get(ControllerStats + 0x14u, 2) == 13);

    Fixture other_cap;
    other_cap.put(0x800fdbca, 1, 2);
    other_cap.put(0x800fdbd2u, 0, 2);
    other_cap.put(0x800fdbb0u, 1, 2);
    other_cap.put(Actor + 4u, 1, 2);
    other_cap.put(Team1 + 0x52u, 5, 2);
    other_cap.put(Stats + 0x14u, 999, 2);
    other_cap.put(Stats + 0x18u, 999, 2);
    check(other_cap.run() == NBA97_TEXT_COMPLETE);
    check(other_cap.get(Stats + 0x14u, 2) == 999 &&
        other_cap.get(Stats + 0x18u, 2) == 999);

    Fixture live_saved;
    live_saved.put(0x800fdbca, 1, 2);
    live_saved.put(0x800fdbd2u, 0, 2);
    live_saved.put(0x800fdb84u, 0, 2);
    live_saved.put(Actor + 4u, 1, 2);
    live_saved.put(Stats + 0x14u, 998, 2);
    live_saved.put(Stats + 0x18u, 998, 2);
    live_saved.put(Team0 + 4u, UnselectedTeam);
    live_saved.put(UnselectedTeam + 0x52u, 0xffffu, 2);
    live_saved.put(AlternateTeam + 4u, Team1);
    live_saved.put(Team1 + 0x52u, 5, 2);
    live_saved.put(AlternateControllerTable + 0xbeu, ControllerStats);
    live_saved.put(ControllerStats + 0x14u, 70, 2);
    live_saved.put(0x800fdc54u, OriginalControllerStats);
    live_saved.put(OriginalControllerStats + 0x14u, 30, 2);
    live_saved.put(OriginalControllerStats + 0x18u, 80, 2);
    live_saved.mutate_saved_on_stat = true;
    const int live_result = live_saved.run();
    if (live_result != NBA97_TEXT_COMPLETE)
        std::fprintf(stderr, "live saved result %d pc %08x address %08x\n",
            live_result, live_saved.progress.stopped_pc,
            live_saved.progress.stopped_address);
    check(live_result == NBA97_TEXT_COMPLETE);
    check(live_saved.get(ControllerStats + 0x14u, 2) == 71 &&
        live_saved.get(OriginalControllerStats + 0x14u, 2) == 30 &&
        live_saved.get(Stats + 0x18u, 2) == 999 &&
        live_saved.get(OriginalControllerStats + 0x18u, 2) == 81);
}

void callback_live_frame_and_hilo() {
    Fixture f;
    f.relocate_on_effect = true;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        0x801ffec0u);
    check(f.progress.machine.registers.gpr[16].word == 0xaaa00010u &&
        f.progress.machine.registers.gpr[17].word == 0xaaa00011u &&
        f.progress.machine.registers.gpr[18].word == 0xaaa00012u &&
        f.progress.machine.registers.gpr[19].word == 0xaaa00013u &&
        f.progress.machine.registers.gpr[31].word == 0xaaa00031u);
    check(f.progress.machine.hi.word == (0x8005d4b4u ^ 0x11111111u) &&
        f.progress.machine.hi.known_mask == 0x09 &&
        f.progress.machine.lo.word == (0x8005d4b4u ^ 0x22222222u) &&
        f.progress.machine.lo.known_mask == 0x06);
}
}

int main() {
    change_team_and_publication();
    random_rule_and_signed_team_split();
    descriptor_unknown_prefix_and_refusal();
    state_preservation_and_mapping_failures();
    unknown_compare_and_delay_slots();
    same_team_velocity_and_caps();
    budgets_and_validation();
    change_team_stat_branches_and_wraps();
    callback_live_frame_and_hilo();
    std::printf("%u game ball acquire checks passed\n", checks);
    return 0;
}
