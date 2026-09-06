#include "game_ball_acquire_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "game ball acquire integration check %u failed at line %u\n",
            checks, line);
        std::exit(1);
    }
}
#define check(x) check_at((x), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Actor = 0x80110000u;
constexpr std::uint32_t Descriptor = 0x80120000u;
constexpr std::uint32_t Stats = 0x80130000u;
constexpr std::uint32_t Team0 = 0x8001edf4u;
constexpr std::uint32_t Team1 = 0x8001eeb8u;

struct Composition {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000u);
    std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameBallAcquireContext context{};
    Nba97GameBallAcquireProgress progress{};
    Nba97GameBallAcquireAdapterProgress adapter{};
    std::vector<Nba97GameBallAcquireEvent> unresolved;

    Composition(std::uint32_t actor_id, bool same_team) {
        context.memory = {&region, 1};
        context.operation_budget = 1000;
        context.io = io;
        context.user = this;
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            context.machine.registers.gpr[i] = {
                0x50000000u + i * 0x01010101u,
                static_cast<std::uint8_t>((i % 15u) + 1u)};
        context.machine.registers.gpr[0] = {0, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {Actor, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {0x801fff00u, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x800608a4u, 0x0f};
        context.machine.hi = {0x12345678u, 0x05};
        context.machine.lo = {0x9abcdef0u, 0x0a};
        put(Actor, actor_id); put(Actor + 4, 0xffffu, 2);
        put(Actor + 8, 0); put(Actor + 0x0c, 0);
        put(Actor + 0x10, 1); put(Actor + 0x1a, 15, 1);
        put(Actor + 0x1c, Stats); put(Actor + 0x20, Descriptor);
        put(Actor + 0x46, 44, 2); put(Actor + 0xa0, 385, 2);
        put(Actor + 0xd9, 0, 1); put(Descriptor + 0x0d, 1, 1);
        put(Team0 + 4, Team1); put(Team1 + 4, Team0);
        put(0x800fdc40u, Team0); put(0x800fa034u, 0xffffffffu);
        put(0x80021d95u, 0, 1); put(0x800fdb90u, 0x7f, 2);
        put(0x800fdb94u, same_team ? 0u : 1u, 2);
        put(0x800fdb96u, same_team ? 7u : 1u, 2);
        put(0x800fdbca, 0, 2); put(0x800fdbb0u, 0, 2);
        put(0x800fdbd2u, 0, 2); put(0x800fdbd4u, 0, 2);
        put(0x800fdbd8u, same_team ? 1u : 0u, 2);
        put(0x800fe8ccu, 0, 2);
    }
    std::size_t at(std::uint32_t address) const { return address - Ram; }
    void put(std::uint32_t address, std::uint32_t value, unsigned width = 4) {
        for (unsigned i = 0; i < width; ++i)
            bytes[at(address) + i] = static_cast<std::uint8_t>(value >> (8u * i));
    }
    int run() {
        return nba97_game_ball_acquire_with_rule_delay(&context, &progress,
            &adapter);
    }
    static int io(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameBallAcquireEvent* event,
        Nba97GameBallAcquireMachine* machine) {
        auto& c = *static_cast<Composition*>(opaque);
        if (!event) return 0;
        c.unresolved.push_back(*event);
        if (event->kind == NBA97_GAME_BALL_ACQUIRE_CHILD_80072C40) {
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0] =
                {0xfeed0001u, 0x05};
            machine->hi = {0x11223344u, 0x09};
            machine->lo = {0x55667788u, 0x06};
        }
        return 1;
    }
};

struct NaturalContact {
    static constexpr std::uint32_t Ball = 0x80001000u;
    static constexpr std::uint32_t ContactActor = 0x80002000u;
    static constexpr std::uint32_t ContactStats = 0x80006000u;
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000u);
    std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000u, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97GameBallActorContactContext context{};
    Nba97GameBallActorContactProgress contact{};
    Nba97GameBallActorContactBinding binding{};
    Nba97GameBallAcquireNaturalProgress natural{};
    std::vector<Nba97GameBallActorContactEvent> contact_events;
    std::vector<Nba97GameBallAcquireEvent> acquisition_events;
    bool partial_s3_before_acquisition = false;

    NaturalContact() {
        context.memory = {&region, 1};
        context.operation_budget = 10000;
        context.io = contact_io;
        context.user = this;
        for (auto& gpr : context.machine.registers.gpr) gpr = {0, 0x0f};
        context.machine.hi = {0x13579bdfu, 0x0f};
        context.machine.lo = {0x2468ace0u, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {0x801ff000u, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {0x80060edcu, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {Ball, 0x0f};
        context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
            {ContactActor, 0x0f};
        put(0x800fdbccu, 0xffffu, 2); put(0x800fdb58u, 1);
        put(0x800fe8c4u, 0, 2); put(0x800fe8ccu, 0, 2);
        put(0x800fdb90u, 0x81, 2); put(0x800fdb94u, 0, 2);
        put(0x800fdb96u, 0, 2); put(0x800fdbd4u, 0, 2);
        put(0x800fdbd2u, 0xffffu, 2); put(0x800fdbd0u, 0xffffu, 2);
        put(0x800fdbca, 0, 2); put(0x800fdbb0u, 0, 2);
        put(0x800fdbd8u, 0, 2); put(0x800fa034u, 0xffffffffu);
        put(0x800fdc40u, Team0); put(Team0 + 4u, Team1);
        put(Team1 + 4u, Team0); put(0x800fdc48u, ContactActor);
        put(0x80020becu, ContactActor); put(0x80020c00u, ContactActor);
        put(ContactActor, 0); put(ContactActor + 4u, 0xffffu, 2);
        put(ContactActor + 0x10u, 0); put(ContactActor + 0x1au, 0, 1);
        put(ContactActor + 0x1cu, ContactStats);
        put(ContactActor + 0x20u, 0x80003000u);
        put(ContactActor + 0x46u, 0x27, 2);
        put(ContactActor + 0xa0u, 385, 2); put(ContactActor + 0xd9u, 0, 1);
        put(0x8000300du, 0, 1);
        binding.actor_resume_io = resume_io;
        binding.actor_resume_user = this;
        binding.child_operation_budget = 1000;
        natural.acquisition_operation_budget = 1000;
        natural.acquisition_io = acquisition_io;
        natural.acquisition_user = this;
    }
    std::size_t at(std::uint32_t address) const { return address - Ram; }
    void put(std::uint32_t address, std::uint32_t value, unsigned width = 4) {
        for (unsigned i = 0; i < width; ++i)
            bytes[at(address) + i] = static_cast<std::uint8_t>(value >> (8u * i));
    }
    int run() {
        return nba97_game_ball_actor_contact_with_ball_acquire(&context,
            &contact, &binding, &natural);
    }
    static int contact_io(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameBallActorContactEvent* event,
        Nba97GameBallActorContactMachine* machine) {
        auto& f = *static_cast<NaturalContact*>(opaque);
        if (!event) return 0;
        f.contact_events.push_back(*event);
        if (f.partial_s3_before_acquisition && event->pc == 0x80060894u)
            machine->registers.gpr[19] = {0x12345678u, 0x03};
        if (event->entry == 0x8007066cu)
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0f};
        if (event->entry == 0x800601b8u || event->entry == 0x80060240u ||
            event->entry == 0x80060008u)
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {1, 0x0f};
        if (event->entry == 0x8002ab70u || event->entry == 0x800a5638u ||
            event->entry == 0x800a5634u || event->entry == 0x800aa788u)
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0f};
        return 1;
    }
    static int acquisition_io(void* opaque, const Nba97GameTextMemory*,
        const Nba97GameBallAcquireEvent* event,
        Nba97GameBallAcquireMachine* machine) {
        auto& f = *static_cast<NaturalContact*>(opaque);
        if (!event) return 0;
        f.acquisition_events.push_back(*event);
        if (event->entry == 0x8002ab70u)
            machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0f};
        return 1;
    }
    static int resume_io(void*, const Nba97GameTextMemory*,
        const Nba97GameActorResumeEvent*, Nba97GameActorResumeMachine*) {
        return 1;
    }
};

void actual_leaf_at_all_four_sites() {
    struct Case { std::uint32_t id; bool same; unsigned site; std::uint32_t pc;
        std::uint32_t duration; std::uint32_t code; };
    const Case cases[] = {
        {4, false, NBA97_GAME_BALL_ACQUIRE_CHANGE_LONG_DELAY,
            0x8005d4a8u, 20000u, 2u},
        {5, false, NBA97_GAME_BALL_ACQUIRE_CHANGE_SHORT_DELAY,
            0x8005d498u, 10000u, 1u},
        {4, true, NBA97_GAME_BALL_ACQUIRE_SAME_LONG_DELAY,
            0x8005d8a8u, 20000u, 4u},
        {5, true, NBA97_GAME_BALL_ACQUIRE_SAME_SHORT_DELAY,
            0x8005d898u, 10000u, 3u}
    };
    for (const auto& expected : cases) {
        Composition c(expected.id, expected.same);
        check(c.run() == NBA97_TEXT_COMPLETE && c.progress.completed);
        check(c.adapter.delay_result == NBA97_TEXT_COMPLETE &&
            c.adapter.delay_invocations == 1 &&
            c.adapter.delay_site_invocations[expected.site] == 1);
        const auto& event = c.adapter.event[expected.site];
        const auto& leaf = c.adapter.delay[expected.site];
        check(event.pc == expected.pc && event.delay_slot_pc == expected.pc + 4u &&
            event.entry == 0x800295c8u && event.argument_count == 1);
        check(leaf.completed && leaf.operations == 0 && leaf.accesses == 0 &&
            leaf.reads == 0 && leaf.stores == 0);
        check(leaf.return_address.word == expected.pc + 8u &&
            leaf.return_address.known_mask == 0x0f);
        check(leaf.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            expected.duration);
        if (!expected.same) {
            check(leaf.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0].word ==
                0xfeed0001u);
            check(leaf.machine.hi.word == 0x11223344u &&
                leaf.machine.hi.known_mask == 0x09 &&
                leaf.machine.lo.word == 0x55667788u &&
                leaf.machine.lo.known_mask == 0x06);
        }
        bool found_effect = false;
        for (const auto& unresolved : c.unresolved)
            if (unresolved.kind == NBA97_GAME_BALL_ACQUIRE_CHILD_80029590) {
                found_effect = true;
                check(unresolved.pc == (expected.same ? 0x8005d8b4u : 0x8005d4b4u));
            }
        check(found_effect && c.adapter.unresolved_callbacks_completed ==
            c.unresolved.size());
        check((expected.duration == 10000u ?
            c.adapter.duration_10000_invocations :
            c.adapter.duration_20000_invocations) == 1);
    }
}

void adapter_guards() {
    Composition c(5, false);
    Nba97GameBallAcquireEvent event{};
    event.pc = 0x8005d498u; event.delay_slot_pc = 0x8005d49cu;
    event.entry = 0x800295c8u; event.operation = 1; event.invocation = 1;
    event.kind = NBA97_GAME_BALL_ACQUIRE_CHILD_800295C8;
    event.argument_count = 1;
    auto machine = c.context.machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {10000, 0x0f};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x8005d4a0u, 0x0f};
    Nba97GameRuleDelayProgress progress{};
    check(nba97_game_ball_acquire_rule_delay(&event, &machine, &progress) ==
        NBA97_TEXT_COMPLETE);
    event.pc = 0x8005d4a8u; event.delay_slot_pc = 0x8005d4acu;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x8005d4b0u, 0x0f};
    check(nba97_game_ball_acquire_rule_delay(&event, &machine, &progress) ==
        NBA97_TEXT_ARGUMENT); /* long site requires 20000 */
    check(nba97_game_ball_acquire_rule_delay(nullptr, &machine, &progress) ==
        NBA97_TEXT_ARGUMENT);
    check(nba97_game_ball_acquire_with_rule_delay(nullptr, &c.progress,
        &c.adapter) == NBA97_TEXT_ARGUMENT);
}

void actual_ah_natural_caller() {
    NaturalContact f;
    const int result = f.run();
    if (result != NBA97_TEXT_COMPLETE)
        std::fprintf(stderr, "natural AH result %d pc %08x entry %08x\n",
            result, f.contact.stopped_pc, f.contact.stopped_entry);
    if (f.natural.acquisition_count != 1 ||
        f.natural.acquisition_result != NBA97_TEXT_COMPLETE)
        std::fprintf(stderr, "natural acquisition count %zu result %d pc %08x entry %08x\n",
            f.natural.acquisition_count, f.natural.acquisition_result,
            f.natural.acquisition.stopped_pc,
            f.natural.acquisition.stopped_entry);
    check(result == NBA97_TEXT_COMPLETE && f.contact.completed);
    check(f.natural.acquisition_count == 1 &&
        f.natural.acquisition_result == NBA97_TEXT_COMPLETE &&
        f.natural.acquisition.completed);
    check(f.natural.acquisition_event.pc == 0x8006089cu &&
        f.natural.acquisition_event.delay_slot_pc == 0x800608a0u &&
        f.natural.acquisition_event.entry == 0x8005d140u &&
        f.natural.acquisition_event.kind ==
            NBA97_GAME_BALL_ACTOR_CONTACT_CHILD_8005D140);
    check(f.natural.acquisition_event.argument_count == 1 &&
        f.natural.acquisition.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x801fefc0u);
    bool leaked = false;
    for (const auto& event : f.contact_events)
        if (event.entry == 0x8005d140u) leaked = true;
    check(!leaked && f.natural.unresolved_contact_callbacks_completed ==
        f.contact_events.size());
    check(f.context.io == NaturalContact::contact_io && f.context.user == &f);
}

void actual_ah_argument_prefix() {
    NaturalContact f;
    f.region.known = nullptr;
    f.partial_s3_before_acquisition = true;
    const int result = f.run();
    if (result != NBA97_TEXT_IO_REFUSED) {
        std::fprintf(stderr, "natural argument result %d contact pc %08x entry %08x acquisition %d pc %08x\n",
            result, f.contact.stopped_pc, f.contact.stopped_entry,
            f.natural.acquisition_result, f.natural.acquisition.stopped_pc);
        for (const auto& event : f.contact_events)
            std::fprintf(stderr, "contact callback pc %08x entry %08x\n",
                event.pc, event.entry);
    }
    check(result == NBA97_TEXT_IO_REFUSED && !f.contact.completed);
    check(f.contact.stopped_pc == 0x8006089cu &&
        f.contact.stopped_entry == 0x8005d140u);
    check(f.natural.acquisition_count == 1 &&
        f.natural.acquisition_result == NBA97_TEXT_ARGUMENT &&
        f.natural.acquisition.operations != 0 &&
        f.natural.acquisition.stopped_pc == 0x8005d158u);
    check(f.natural.acquisition.machine.registers.gpr[19].word == 0x12345678u &&
        f.natural.acquisition.machine.registers.gpr[19].known_mask == 0x03 &&
        f.contact.machine.registers.gpr[19].word == 0x12345678u &&
        f.contact.machine.registers.gpr[19].known_mask == 0x03);
    check(f.context.io == NaturalContact::contact_io && f.context.user == &f);
}
}

int main() {
    actual_leaf_at_all_four_sites();
    adapter_guards();
    actual_ah_natural_caller();
    actual_ah_argument_prefix();
    std::printf("%u game ball acquire integration checks passed\n", checks);
    return 0;
}
