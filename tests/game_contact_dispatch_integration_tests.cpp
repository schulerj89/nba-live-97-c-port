#include "game_contact_dispatch_adapter.h"

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
    ++checks;
    if (!value) {
        std::fprintf(stderr,
            "contact dispatch integration check %u failed at %u\n", checks,
            line);
        std::exit(1);
    }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Ball = 0x80018000u;

bool same_machine(const Nba97GameContactDispatchMachine& left,
    const Nba97GameContactDispatchMachine& right) {
    return std::memcmp(&left, &right, sizeof left) == 0;
}

struct ContactRecord {
    Nba97GameBallContactGateEvent event{};
    Nba97GameBallContactGateMachine machine{};
};

struct Composition {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000, 0);
    std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x110000, 1);
    Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
    Nba97MatchTickContext tick{};
    Nba97MatchTickProgress tick_progress{};
    Nba97GameContactDispatchBinding dispatch{};
    Nba97GameContactDispatchChildren children{};
    Nba97GameBallContactGateAccess gate_journal[16]{};
    std::vector<Nba97MatchTickCall> tick_calls;
    std::vector<Nba97GameContactDispatchEvent> actor_calls;
    std::vector<ContactRecord> contact_calls;
    Nba97MatchTickCall natural{};
    bool prepare_machine{true};

    Composition() {
        tick.access = access;
        tick.service = service;
        tick.player_update = player;
        tick.ball_simulation = ball;
        tick.net_transform = net;
        tick.match_frame = frame;
        tick.user = this;
        tick.operation_budget = 2000;
        tick.incoming_s6 = {2, 1};

        dispatch.memory = {&region, 1};
        dispatch.operation_budget = 1000;
        dispatch.io = nba97_game_contact_dispatch_compose_children;
        dispatch.user = &children;
        children.ball_gate_operation_budget = 100;
        children.child_800602CC = contact;
        children.child_8005FAA8 = actor;
        children.user = this;
        children.ball_gate_access_journal = gate_journal;
        children.ball_gate_access_journal_capacity = std::size(gate_journal);

        put(0x8001edecu, 1, 2);
        put(0x800fdb92u, 2, 2);
        put(0x800fdb8au, 1, 2);
        put(0x80021d82u, 1, 1);
        put(0x800fdb7cu, 0, 2);
        put(0x800fe8ccu, 0, 2);
        put(0x800fe8c4u, 0, 2);
        put(0x800fdb68u, 5, 2);
        put(0x800fdb78u, 1, 1);
        put(0x800fdb6cu, 1, 2);
        put(0x800fdb9cu, 0, 2);
        put(0x800fdbaeu, 10, 2);

        put(0x800fdc48u, Ball, 4);
        put(Ball + 0xb4u, 0, 2);
        put(Ball + 0, 3, 4);
        put(Ball + 8, 0x1000, 4);
        for (unsigned i = 1; i <= 11; ++i) {
            std::uint32_t object = 0x80020000u + i * 0x100u;
            put(0x800fdcbcu + i * 4u, i == 2 ? Ball : object, 4);
            put(object + 0, i == 1 ? 10 : i, 4);
            put(object + 8, 0x1000u + i * 0x100u, 4);
        }
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
    void prepare(const Nba97MatchTickCall& call) {
        for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
            dispatch.entry_machine.registers.gpr[i] =
                {0x51000000u + i * 0x01010101u, 0x0f};
        dispatch.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] =
            {0, 0x0f};
        dispatch.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
            {0x800ff000u, 0x0f};
        dispatch.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
            {call.pc + 8u, 0x0f};
        dispatch.entry_machine.hi = {0x12345678u, 0x0f};
        dispatch.entry_machine.lo = {0x9abcdef0u, 0x0f};
        dispatch.entry_machine_ready = 1;
    }
    static int access(void* user, std::uint32_t, std::uint32_t address,
        unsigned width, unsigned kind, Nba97PlayerFrameValue* value) {
        auto& c = *static_cast<Composition*>(user);
        if (address < Ram || std::uint64_t(address) + width >
                std::uint64_t(Ram) + c.bytes.size())
            return NBA97_BODY_BOUNDS;
        auto at = c.offset(address);
        if (kind == NBA97_FRAME_READ) {
            *value = {};
            for (unsigned i = 0; i < width; ++i)
                if (c.known[at + i]) {
                    value->word |= std::uint32_t(c.bytes[at + i]) << (8u * i);
                    value->known_mask = static_cast<std::uint8_t>(
                        value->known_mask | (1u << i));
                }
        } else {
            for (unsigned i = 0; i < width; ++i) {
                c.bytes[at + i] = static_cast<std::uint8_t>(
                    value->word >> (8u * i));
                c.known[at + i] = static_cast<std::uint8_t>(
                    (value->known_mask >> i) & 1u);
            }
        }
        return NBA97_BODY_OK;
    }
    static int actor(void* user, const Nba97GameTextMemory*,
        const Nba97GameContactDispatchEvent* event,
        Nba97GameContactDispatchMachine* machine) {
        auto& c = *static_cast<Composition*>(user);
        c.actor_calls.push_back(*event);
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0, 0x0f};
        return 1;
    }
    static int contact(void* user, const Nba97GameTextMemory* memory,
        const Nba97GameBallContactGateEvent* event,
        Nba97GameBallContactGateMachine* machine) {
        auto& c = *static_cast<Composition*>(user);
        c.contact_calls.push_back({*event, *machine});
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
            {0xdeadbeefu, 0x0f};
        auto at = c.offset(Ball + 0xb4u);
        memory->region[0].data[at] = 1;
        memory->region[0].data[at + 1] = 0;
        memory->region[0].known[at] = memory->region[0].known[at + 1] = 1;
        return 1;
    }
    static int service(void* user, const Nba97MatchTickCall* call,
        Nba97GamePeriodValue* result) {
        auto& c = *static_cast<Composition*>(user);
        c.tick_calls.push_back(*call);
        if (call->entry == 0x80060fbcu) {
            c.natural = *call;
            if (c.prepare_machine) c.prepare(*call);
            return nba97_game_contact_dispatch_from_match_tick(
                &c.dispatch, call, result);
        }
        if (result) *result = {0, 1};
        return NBA97_BODY_OK;
    }
    static int player(void*, std::uint32_t) { return NBA97_BODY_OK; }
    static int ball(void*, std::uint32_t, std::uint32_t) {
        return NBA97_BODY_OK;
    }
    static int net(void*, std::uint32_t) { return NBA97_BODY_OK; }
    static int frame(void*, std::uint32_t) { return NBA97_BODY_OK; }
    int run() { return nba97_game_match_tick(&tick, &tick_progress); }
};

void natural_tick_and_actual_ai_owner() {
    Composition c;
    check(c.run() == NBA97_BODY_OK && c.tick_progress.completed);
    check(c.natural.pc == 0x80068e08u && c.natural.entry == 0x80060fbcu &&
        c.natural.count == 0 && c.natural.args[0] == 0 && c.natural.args[1] == 0);
    check(c.dispatch.result == NBA97_TEXT_COMPLETE && c.dispatch.progress.completed &&
        c.dispatch.invocations == 1 &&
        c.dispatch.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == 0x80068e10u);
    check(c.children.ball_gate_invocations == 1 &&
        c.children.ball_gate_result == NBA97_TEXT_COMPLETE &&
        c.children.ball_gate_progress.completed && c.contact_calls.size() == 1);
    check(c.contact_calls[0].event.pc == 0x80060ed4u &&
        c.contact_calls[0].event.entry == 0x800602ccu &&
        c.contact_calls[0].event.argument_count == 3);
    check(c.contact_calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        0x80020100u &&
        c.contact_calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == Ball &&
        c.contact_calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word == 1);
    check(c.get(Ball + 0xb4u, 2) == 1 &&
        c.children.child_8005FAA8_invocations == c.actor_calls.size());
    check(c.dispatch.progress.machine.hi.word == 0x12345678u &&
        c.dispatch.progress.machine.lo.word == 0x9abcdef0u);
}

void actual_contact_chain_from_tick() {
    Composition c;
    Nba97GameBallContactGateBinding binding{};
    binding.child_operation_budget=1000;
    binding.contact_binding.child_operation_budget=1000;
    binding.io=[](void*,const Nba97GameTextMemory*,const Nba97GameBallActorContactEvent* e,Nba97GameBallActorContactMachine* m) {
        if(e->entry==0x8007066c || e->entry==0x8005d140 || e->entry==0x8002ab70)
            m->registers.gpr[2]={0,15};
        if(e->entry==0x800601b8 || e->entry==0x80060240 || e->entry==0x80060008)
            m->registers.gpr[2]={1,15};
        return 1; // Explicit remaining contact services in a synthetic fixture.
    };
    c.children.contact_binding=&binding;
    c.put(0x800fdb58u,1,4);
    c.put(0x800fdbccu,0xffff,2);
    c.put(0x800fdbd0u,0xffff,2);
    c.put(0x800fdc40u,0x8001edf4,4);
    for(unsigned i=1;i<=11;++i) {
        const uint32_t actor=0x80020000u+i*0x100u;
        c.put(actor, i,4);
        c.put(actor+4,0xffff,2);
        c.put(actor+0x20,0x80030000,4);
    }
    c.put(Ball,10,4);
    check(c.run()==NBA97_BODY_OK && c.tick_progress.completed);
    check(c.dispatch.progress.completed && c.children.ball_gate_invocations==10);
    check(binding.contact_progress.completed && binding.child_result==NBA97_TEXT_COMPLETE);
    check(binding.contact_progress.callbacks_completed>0);
    check(c.contact_calls.empty()); // Every contact call used the real AH owner.
    check(binding.contact_progress.restored_return_address.word==0x80060edc);
}

void adapter_and_nested_failure_prefixes() {
    Nba97MatchTickCall call{0x80068e08u, 0x80060fbcu, {0, 0}, 0};
    Nba97GameContactDispatchBinding binding{};
    check(!nba97_game_contact_dispatch_from_match_tick(&binding, &call, nullptr) &&
        binding.result == NBA97_TEXT_ARGUMENT);

    Composition missing;
    missing.prepare_machine = false;
    check(missing.run() != NBA97_BODY_OK &&
        missing.dispatch.result == NBA97_TEXT_ARGUMENT &&
        missing.tick_progress.stopped_pc == 0x80068e08u);

    Composition parent_limit;
    parent_limit.dispatch.operation_budget = 0;
    check(parent_limit.run() != NBA97_BODY_OK &&
        parent_limit.dispatch.result == NBA97_TEXT_LIMIT &&
        parent_limit.dispatch.progress.stopped_pc == 0x80060fc0u);

    Composition child_limit;
    child_limit.children.ball_gate_operation_budget = 0;
    check(child_limit.run() != NBA97_BODY_OK &&
        child_limit.children.ball_gate_result == NBA97_TEXT_LIMIT &&
        child_limit.children.ball_gate_progress.stopped_pc == 0x80060e94u &&
        child_limit.dispatch.result == NBA97_TEXT_IO_REFUSED &&
        child_limit.dispatch.progress.stopped_pc == 0x80061070u);

    Composition wrong;
    wrong.prepare(call);
    auto bad = call;
    bad.pc = 0x80068e20u;
    check(!nba97_game_contact_dispatch_from_match_tick(
        &wrong.dispatch, &bad, nullptr));
    Nba97GamePeriodValue unexpected{};
    check(!nba97_game_contact_dispatch_from_match_tick(
        &wrong.dispatch, &call, &unexpected));
}

void child_mux_rejects_malformed_entry_without_clobbering() {
    Composition c;
    Nba97GameContactDispatchEvent actor_event{};
    actor_event.pc = 0x8006104cu;
    actor_event.delay_slot_pc = 0x80061050u;
    actor_event.entry = 0x8005faa8u;
    actor_event.kind = NBA97_GAME_CONTACT_DISPATCH_CHILD_8005FAA8;
    actor_event.argument_count = 2;
    Nba97GameContactDispatchEvent ai_event{};
    ai_event.pc = 0x80061070u;
    ai_event.delay_slot_pc = 0x80061074u;
    ai_event.entry = 0x80060e8cu;
    ai_event.kind = NBA97_GAME_CONTACT_DISPATCH_CHILD_80060E8C;
    ai_event.argument_count = 2;
    Nba97MatchTickCall tick_call{0x80068e08u, 0x80060fbcu, {0, 0}, 0};
    c.prepare(tick_call);

    auto machine = c.dispatch.entry_machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {actor_event.pc + 4u, 0x0f};
    auto before = machine;
    check(!nba97_game_contact_dispatch_compose_children(
        &c.children, &c.dispatch.memory, &actor_event, &machine));
    check(same_machine(machine, before) && c.actor_calls.empty() &&
        c.children.child_8005FAA8_invocations == 0);

    machine = c.dispatch.entry_machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {actor_event.pc + 8u, 0x07};
    before = machine;
    check(!nba97_game_contact_dispatch_compose_children(
        &c.children, &c.dispatch.memory, &actor_event, &machine));
    check(same_machine(machine, before) && c.actor_calls.empty());

    machine = c.dispatch.entry_machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {ai_event.pc + 4u, 0x0f};
    before = machine;
    check(!nba97_game_contact_dispatch_compose_children(
        &c.children, &c.dispatch.memory, &ai_event, &machine));
    check(same_machine(machine, before) &&
        c.children.ball_gate_invocations == 0);

    ai_event.pc = 0x800610c4u;
    ai_event.delay_slot_pc = 0x800610c8u;
    machine = c.dispatch.entry_machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {0x800610c8u, 0x0f};
    before = machine;
    check(!nba97_game_contact_dispatch_compose_children(
        &c.children, &c.dispatch.memory, &ai_event, &machine));
    check(same_machine(machine, before) &&
        c.children.ball_gate_invocations == 0);

    machine = c.dispatch.entry_machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {0x800610ccu, 0x07};
    before = machine;
    check(!nba97_game_contact_dispatch_compose_children(
        &c.children, &c.dispatch.memory, &ai_event, &machine));
    check(same_machine(machine, before) &&
        c.children.ball_gate_invocations == 0);

    machine = c.dispatch.entry_machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {ai_event.pc + 8u, 0x00};
    before = machine;
    check(!nba97_game_contact_dispatch_compose_children(
        &c.children, &c.dispatch.memory, &ai_event, &machine));
    check(same_machine(machine, before) &&
        c.children.ball_gate_invocations == 0);

    machine = c.dispatch.entry_machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {ai_event.pc + 8u, 0x0f};
    machine.hi.known_mask = 0x10;
    before = machine;
    check(!nba97_game_contact_dispatch_compose_children(
        &c.children, &c.dispatch.memory, &ai_event, &machine));
    check(same_machine(machine, before) &&
        c.children.ball_gate_invocations == 0);

    machine = c.dispatch.entry_machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {ai_event.pc + 8u, 0x0f};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask = 0;
    before = machine;
    check(!nba97_game_contact_dispatch_compose_children(
        &c.children, &c.dispatch.memory, &ai_event, &machine));
    check(same_machine(machine, before) &&
        c.children.ball_gate_invocations == 0);

    machine = c.dispatch.entry_machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {ai_event.pc + 8u, 0x0f};
    before = machine;
    auto invalid_region = c.region;
    invalid_region.size = 0;
    Nba97GameTextMemory invalid_memory{&invalid_region, 1};
    check(!nba97_game_contact_dispatch_compose_children(
        &c.children, &invalid_memory, &ai_event, &machine));
    check(same_machine(machine, before) &&
        c.children.ball_gate_invocations == 0 &&
        c.children.ball_gate_progress.machine.registers.gpr[
            NBA97_MATCH_INITIALIZE_ZERO].known_mask == 0);

    Composition outer;
    outer.prepare(tick_call);
    ai_event.pc = 0x800610c4u;
    ai_event.delay_slot_pc = 0x800610c8u;
    machine = outer.dispatch.entry_machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {Ball, 0x0f};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
        {0x80020300u, 0x0f};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {0x800610ccu, 0x0f};
    check(nba97_game_contact_dispatch_compose_children(
        &outer.children, &outer.dispatch.memory, &ai_event, &machine));
    check(outer.children.ball_gate_invocations == 1 &&
        outer.children.ball_gate_result == NBA97_TEXT_COMPLETE &&
        outer.contact_calls.size() == 1);
}
}

int main() {
    natural_tick_and_actual_ai_owner();
    actual_contact_chain_from_tick();
    adapter_and_nested_failure_prefixes();
    child_mux_rejects_malformed_entry_without_clobbering();
    std::printf("contact dispatch integration tests passed: %u checks\n",
        checks);
}
