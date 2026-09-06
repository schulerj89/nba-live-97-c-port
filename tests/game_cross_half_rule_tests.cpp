#include "recovered/game_cross_half_rule.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "cross-half rule check %u failed at line %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Actor1 = 0x80010000u;
constexpr std::uint32_t Actor2 = 0x80011000u;
constexpr std::uint32_t Phase = 0x800fdb90u;
constexpr std::uint32_t Team = 0x800fdb94u;
constexpr std::uint32_t Delta = 0x800fdb6cu;
constexpr std::uint32_t Owner = 0x800fdbccu;
constexpr std::uint32_t Timer = 0x800fdbacu;
constexpr std::uint32_t ArmValue = 0x800fdbaau;
constexpr std::uint32_t Pointer1 = 0x800fdc38u;
constexpr std::uint32_t Pointer2 = 0x800fdc34u;
constexpr std::uint32_t Enable = 0x80021d8bu;
constexpr std::uint32_t ActorBlock = 0x800fe8ccu;
constexpr std::uint32_t CrossingBlock = 0x800fe8e0u;
constexpr std::uint32_t RuleCode = 0x800fe882u;

bool same_word(const Nba97GameCrossHalfRuleWord &left,
               const Nba97GameCrossHalfRuleWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool same_machine(const Nba97GameCrossHalfRuleMachine &left,
                  const Nba97GameCrossHalfRuleMachine &right) {
  for (unsigned index = 0u; index != NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
       ++index)
    if (!same_word(left.registers.gpr[index], right.registers.gpr[index]))
      return false;
  return same_word(left.hi, right.hi) && same_word(left.lo, right.lo);
}

struct CallRecord {
  Nba97GameCrossHalfRuleEvent event{};
  Nba97GameCrossHalfRuleMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000u, 0u);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x110000u, 1u);
  std::array<std::uint8_t, 32> low_bytes{};
  std::array<std::uint8_t, 32> low_known{};
  std::array<std::uint8_t, 8> high_bytes{};
  std::array<std::uint8_t, 8> high_known{};
  std::array<Nba97GameTextRegion, 3> regions{};
  std::array<Nba97GameCrossHalfRuleAccess, 64> journal{};
  Nba97GameCrossHalfRuleContext context{};
  Nba97GameCrossHalfRuleProgress progress{};
  std::vector<CallRecord> calls;
  std::size_t refuse_ordinal{};
  std::size_t invalid_ordinal{};
  std::uint32_t gate_return{};
  std::uint8_t gate_return_mask{0x0f};
  bool relocate_stack{};
  bool mutate_rule_start_a0{};
  bool mutate_before_refusal{};

  Fixture() {
    low_known.fill(1u);
    high_known.fill(1u);
    regions[0] = {Ram, bytes.data(), known.data(), bytes.size()};
    context.memory = {regions.data(), 1u};
    context.operation_budget = 1000u;
    context.machine = initial_machine();
    context.io = io;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    put(Phase, 100u, 2u);
    put(ActorBlock, 0u, 2u);
    put(Owner, 0u, 2u);
    put(Pointer1, Actor1, 4u);
    put(Pointer2, Actor2, 4u);
    put(Actor1 + 0x10u, 0x80000000u, 4u);
    put(Actor2 + 8u, 0u, 4u);
    put(Actor2 + 0x10u, 0u, 4u);
    put(Enable, 1u, 1u);
    put(CrossingBlock, 1u, 2u);
    put(Timer, 12u, 2u);
    put(Delta, 1u, 2u);
    put(Team, 0u, 2u);
    put(ArmValue, 0u, 2u);
    put(RuleCode, 0u, 2u);
  }

  static Nba97GameCrossHalfRuleMachine initial_machine() {
    Nba97GameCrossHalfRuleMachine machine{};
    for (unsigned index = 0u; index != NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
         ++index)
      machine.registers.gpr[index] = {
          0x41000000u + index * 0x01010101u,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0u, 0x0f};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ff000u, 0x0f};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80068e38u, 0x0f};
    machine.hi = {0x12345678u, 0x05};
    machine.lo = {0x9abcdef0u, 0x0a};
    return machine;
  }

  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    const auto at = offset(address);
    for (unsigned byte = 0u; byte != width; ++byte) {
      bytes[at + byte] = static_cast<std::uint8_t>(value >> (8u * byte));
      known[at + byte] = 1u;
    }
  }

  std::uint32_t get(std::uint32_t address, unsigned width) const {
    const auto at = offset(address);
    std::uint32_t value = 0u;
    for (unsigned byte = 0u; byte != width; ++byte)
      value |= std::uint32_t(bytes[at + byte]) << (8u * byte);
    return value;
  }

  int run() { return nba97_game_cross_half_rule(&context, &progress); }

  static int io(void *opaque, const Nba97GameTextMemory *,
                const Nba97GameCrossHalfRuleEvent *event,
                Nba97GameCrossHalfRuleMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.calls.push_back({*event, *machine});
    const std::size_t ordinal = fixture.calls.size();
    if (ordinal == fixture.refuse_ordinal) {
      if (fixture.mutate_before_refusal) {
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0] = {0xdeadbeefu, 0x06};
        machine->hi = {0x11223344u, 0x09};
      }
      return 0;
    }
    if (event->kind == NBA97_GAME_CROSS_HALF_RULE_CHILD_80062D84)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {
          fixture.gate_return, fixture.gate_return_mask};
    if (fixture.relocate_stack && ordinal == 1u) {
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ee000u, 0x0f};
      fixture.put(0x800ee010u, 0x8006abc0u, 4u);
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {0x87654321u, 0x03};
      machine->hi = {0x0badf00du, 0x06};
      machine->lo = {0xfeedfaceu, 0x09};
    }
    if (fixture.mutate_rule_start_a0 &&
        event->kind == NBA97_GAME_CROSS_HALF_RULE_CHILD_80062300)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {0xcafebabeu, 0x07};
    if (ordinal == fixture.invalid_ordinal)
      machine->hi.known_mask = 0x10u;
    return 1;
  }
};

void check_call(const CallRecord &call, std::uint32_t pc, std::uint32_t entry,
                unsigned kind, unsigned argc, std::uint32_t a0) {
  check(call.event.pc == pc && call.event.delay_slot_pc == pc + 4u &&
        call.event.entry == entry && call.event.kind == kind &&
        call.event.argument_count == argc && call.event.operation != 0u &&
        call.event.invocation == 1u);
  check(call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word == pc + 8u &&
        call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask ==
            0x0f);
  check(call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == a0 &&
        call.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask ==
            0x0f);
}

void normal_paths_and_determinism() {
  Fixture first;
  Fixture second;
  const auto initial = first.context.machine;
  check(first.run() == NBA97_TEXT_COMPLETE && first.progress.completed &&
        first.progress.timer_accumulated && first.progress.rule_dispatched &&
        first.progress.blocker_cleared && !first.progress.armed);
  check(second.run() == NBA97_TEXT_COMPLETE);
  check(first.progress.operations == 23u && first.progress.reads == 14u &&
        first.progress.stores == 4u &&
        first.progress.callbacks_completed == 5u);
  check(first.calls.size() == 5u);
  check_call(
      first.calls[0], 0x80068290u, 0x80062d84u,
      NBA97_GAME_CROSS_HALF_RULE_CHILD_80062D84, 0u,
      first.calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word);
  check_call(first.calls[1], 0x800682b4u, 0x80029590u,
             NBA97_GAME_CROSS_HALF_RULE_CHILD_80029590, 1u, 11u);
  check_call(first.calls[2], 0x800682d0u, 0x800295c8u,
             NBA97_GAME_CROSS_HALF_RULE_CHILD_800295C8, 1u, 5000u);
  check_call(first.calls[3], 0x800682d8u, 0x80062300u,
             NBA97_GAME_CROSS_HALF_RULE_CHILD_80062300, 1u, 5u);
  check_call(first.calls[4], 0x800682e0u, 0x80062660u,
             NBA97_GAME_CROSS_HALF_RULE_CHILD_80062660, 0u, 5u);
  check(first.get(Timer, 2u) == 13u && first.get(RuleCode, 2u) == 8u &&
        first.get(CrossingBlock, 2u) == 0u);
  const std::uint32_t access_pc[] = {
      0x80068180u, 0x80068190u, 0x80068198u, 0x800681acu, 0x800681c0u,
      0x800681c8u, 0x800681ccu, 0x800681d0u, 0x800681e8u, 0x8006823cu,
      0x80068250u, 0x80068258u, 0x80068268u, 0x80068280u, 0x800682a4u,
      0x800682f0u, 0x800682f8u, 0x800682fcu};
  const std::uint32_t access_address[] = {
      Phase,    0x800feff8u,    ActorBlock,  Owner,          Pointer1,
      Pointer2, Actor1 + 0x10u, Actor2 + 8u, Enable,         CrossingBlock,
      Timer,    Delta,          Timer,       Actor2 + 0x10u, Team,
      RuleCode, CrossingBlock,  0x800feff8u};
  check(first.progress.access_events == std::size(access_pc));
  for (std::size_t index = 0u; index != std::size(access_pc); ++index)
    check(first.journal[index].pc == access_pc[index] &&
          first.journal[index].address == access_address[index] &&
          first.journal[index].operation != 0u);
  check(first.progress.frame_stack_pointer == 0x800fefe8u &&
        first.progress.restored_return_address.word == 0x80068e38u &&
        first.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x800ff000u);
  check(same_word(first.progress.machine.hi, initial.hi) &&
        same_word(first.progress.machine.lo, initial.lo));
  for (unsigned index = 0u; index != NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
       ++index)
    if (index != NBA97_MATCH_INITIALIZE_AT &&
        index != NBA97_MATCH_INITIALIZE_V0 &&
        index != NBA97_MATCH_INITIALIZE_V1 &&
        index != NBA97_MATCH_INITIALIZE_A0 &&
        index != NBA97_MATCH_INITIALIZE_SP &&
        index != NBA97_MATCH_INITIALIZE_RA)
      check(same_word(first.progress.machine.registers.gpr[index],
                      initial.registers.gpr[index]));
  check(first.bytes == second.bytes && first.known == second.known &&
        same_machine(first.progress.machine, second.progress.machine));

  Fixture away;
  away.put(Team, 0xffffu, 2u);
  check(away.run() == NBA97_TEXT_COMPLETE && away.calls.size() == 5u);
  check_call(away.calls[1], 0x800682c4u, 0x80029590u,
             NBA97_GAME_CROSS_HALF_RULE_CHILD_80029590, 1u, 12u);
  check_call(away.calls[2], 0x800682d0u, 0x800295c8u,
             NBA97_GAME_CROSS_HALF_RULE_CHILD_800295C8, 1u, 20000u);
}

void clear_and_uncleared_exits() {
  Fixture phase;
  phase.put(Phase, 128u, 2u);
  phase.put(CrossingBlock, 9u, 2u);
  check(phase.run() == NBA97_TEXT_COMPLETE &&
        phase.get(CrossingBlock, 2u) == 0u && phase.progress.blocker_cleared &&
        phase.calls.empty());

  Fixture negative_phase;
  negative_phase.put(Phase, 0x8000u, 2u);
  negative_phase.put(Owner, 0xffffu, 2u);
  negative_phase.put(CrossingBlock, 9u, 2u);
  check(negative_phase.run() == NBA97_TEXT_COMPLETE &&
        negative_phase.get(CrossingBlock, 2u) == 9u &&
        !negative_phase.progress.blocker_cleared);

  Fixture actor_block;
  actor_block.put(ActorBlock, 1u, 2u);
  actor_block.put(CrossingBlock, 9u, 2u);
  check(actor_block.run() == NBA97_TEXT_COMPLETE &&
        actor_block.get(CrossingBlock, 2u) == 0u);

  Fixture owner;
  owner.put(Owner, 0x8000u, 2u);
  owner.put(CrossingBlock, 9u, 2u);
  check(owner.run() == NBA97_TEXT_COMPLETE &&
        owner.get(CrossingBlock, 2u) == 9u && owner.calls.empty());

  Fixture disabled;
  disabled.put(Enable, 0u, 1u);
  disabled.put(CrossingBlock, 9u, 2u);
  check(disabled.run() == NBA97_TEXT_COMPLETE &&
        disabled.get(CrossingBlock, 2u) == 0u);

  Fixture zero_block;
  zero_block.put(CrossingBlock, 0u, 2u);
  check(zero_block.run() == NBA97_TEXT_COMPLETE &&
        zero_block.get(CrossingBlock, 2u) == 0u &&
        !zero_block.progress.timer_accumulated && zero_block.calls.empty());

  Fixture actor_gate;
  actor_gate.put(Actor2 + 0x10u, 1u, 4u);
  check(actor_gate.run() == NBA97_TEXT_COMPLETE &&
        actor_gate.get(CrossingBlock, 2u) == 0u && actor_gate.calls.empty());

  Fixture service_gate;
  service_gate.gate_return = 1u;
  check(service_gate.run() == NBA97_TEXT_COMPLETE &&
        service_gate.get(CrossingBlock, 2u) == 0u &&
        service_gate.calls.size() == 1u);

  Fixture partial_nonzero;
  partial_nonzero.gate_return = 0x00001200u;
  partial_nonzero.gate_return_mask = 0x02u;
  check(partial_nonzero.run() == NBA97_TEXT_COMPLETE &&
        partial_nonzero.calls.size() == 1u &&
        partial_nonzero.progress.blocker_cleared);
}

void arm_store_order_and_timer_edges() {
  Fixture arm;
  arm.put(Actor1 + 0x10u, 7u, 4u);
  arm.put(Actor2 + 8u, 3u, 4u);
  arm.put(CrossingBlock, 0u, 2u);
  check(arm.run() == NBA97_TEXT_COMPLETE && arm.progress.armed &&
        arm.get(CrossingBlock, 2u) == 1u && arm.get(Timer, 2u) == 0u &&
        arm.get(ArmValue, 2u) == 0x7fffu);
  std::vector<Nba97GameCrossHalfRuleAccess> stores;
  for (std::size_t index = 0u; index != arm.progress.access_events; ++index)
    if (arm.journal[index].kind == NBA97_GAME_CROSS_HALF_RULE_STORE)
      stores.push_back(arm.journal[index]);
  check(stores.size() == 4u);
  check(stores[0].pc == 0x80068190u && stores[1].pc == 0x80068218u &&
        stores[1].address == CrossingBlock && stores[1].value == 1u &&
        stores[2].pc == 0x80068224u && stores[2].address == Timer &&
        stores[2].value == 0u && stores[3].pc == 0x8006822cu &&
        stores[3].address == ArmValue && stores[3].value == 0x7fffu);

  Fixture already_armed;
  already_armed.put(Actor1 + 0x10u, 7u, 4u);
  already_armed.put(Actor2 + 8u, 3u, 4u);
  already_armed.put(CrossingBlock, 2u, 2u);
  check(already_armed.run() == NBA97_TEXT_COMPLETE &&
        already_armed.get(CrossingBlock, 2u) == 2u &&
        already_armed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 1u);

  struct TimerCase {
    std::uint16_t timer;
    std::uint16_t delta;
    std::uint16_t stored;
    bool dispatch;
  };
  const TimerCase cases[] = {
      {0u, 0u, 0u, false},           {12u, 0u, 12u, false},
      {12u, 1u, 13u, true},          {13u, 0u, 13u, true},
      {0x7fffu, 0u, 0x7fffu, true},  {0x7fffu, 1u, 0x8000u, false},
      {0x8000u, 0u, 0x8000u, false}, {0xffffu, 1u, 0u, false}};
  for (const auto &item : cases) {
    Fixture fixture;
    fixture.put(Timer, item.timer, 2u);
    fixture.put(Delta, item.delta, 2u);
    check(fixture.run() == NBA97_TEXT_COMPLETE &&
          fixture.get(Timer, 2u) == item.stored &&
          (fixture.progress.rule_dispatched != 0u) == item.dispatch &&
          fixture.calls.size() == (item.dispatch ? 5u : 0u));
    if (!item.dispatch)
      check(fixture.get(CrossingBlock, 2u) == 1u);
  }
}

void unknown_and_malformed_prefixes() {
  Fixture phase;
  phase.put(Phase, 64u, 2u);
  phase.known[phase.offset(Phase)] = 0u;
  check(phase.run() == NBA97_TEXT_UNKNOWN &&
        phase.progress.stopped_pc == 0x8006818cu &&
        phase.progress.stores == 1u &&
        phase.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 0x0eu &&
        phase.get(0x800feff8u, 4u) == 0x80068e38u);

  Fixture sign;
  sign.known[sign.offset(Actor1 + 0x10u) + 3u] = 0u;
  check(sign.run() == NBA97_TEXT_UNKNOWN &&
        sign.progress.stopped_pc == 0x800681dcu);

  Fixture service;
  service.gate_return = 0u;
  service.gate_return_mask = 0x0eu;
  check(service.run() == NBA97_TEXT_UNKNOWN &&
        service.progress.stopped_pc == 0x80068298u &&
        service.calls.size() == 1u);

  Fixture team;
  team.known[team.offset(Team)] = 0u;
  team.known[team.offset(Team) + 1u] = 0u;
  check(team.run() == NBA97_TEXT_UNKNOWN &&
        team.progress.stopped_pc == 0x800682acu && team.calls.size() == 1u);

  Fixture timer;
  timer.known[timer.offset(Timer) + 1u] = 0u;
  check(timer.run() == NBA97_TEXT_UNKNOWN &&
        timer.progress.stopped_pc == 0x80068278u &&
        timer.progress.timer_accumulated &&
        timer.known[timer.offset(Timer)] == 1u &&
        timer.known[timer.offset(Timer) + 1u] == 0u);

  Fixture arm_branch;
  arm_branch.put(Actor1 + 0x10u, 7u, 4u);
  arm_branch.put(Actor2 + 8u, 3u, 4u);
  arm_branch.put(CrossingBlock, 0u, 2u);
  arm_branch.known[arm_branch.offset(CrossingBlock)] = 0u;
  check(arm_branch.run() == NBA97_TEXT_UNKNOWN &&
        arm_branch.progress.stopped_pc == 0x8006820cu &&
        arm_branch.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 1u &&
        arm_branch.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 0x0fu);

  Fixture malformed;
  const auto before_v0 =
      malformed.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0];
  const auto before_sp =
      malformed.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP];
  malformed.known[malformed.offset(Phase) + 1u] = 2u;
  check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x80068180u &&
        malformed.progress.reads == 0u &&
        malformed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 0x80100000u &&
        malformed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 0x0fu &&
        same_word(
            malformed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP],
            before_sp) &&
        !same_word(
            malformed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0],
            before_v0));

  Fixture null_store;
  null_store.regions[0].known = nullptr;
  null_store.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 0x0eu;
  const auto saved_before = null_store.get(0x800feff8u, 4u);
  check(null_store.run() == NBA97_TEXT_ARGUMENT &&
        null_store.progress.stopped_pc == 0x80068190u &&
        null_store.get(0x800feff8u, 4u) == saved_before);
}

void operation_budgets_and_refusals() {
  Fixture complete;
  check(complete.run() == NBA97_TEXT_COMPLETE);
  const auto operations = complete.progress.operations;
  check(operations == 23u);
  for (std::size_t budget = 0u; budget != operations; ++budget) {
    Fixture first;
    Fixture second;
    first.context.operation_budget = budget;
    second.context.operation_budget = budget;
    check(first.run() == NBA97_TEXT_LIMIT && second.run() == NBA97_TEXT_LIMIT &&
          first.progress.operations == budget &&
          first.progress.stopped_pc == second.progress.stopped_pc &&
          first.progress.stopped_address == second.progress.stopped_address &&
          first.progress.stopped_entry == second.progress.stopped_entry &&
          first.bytes == second.bytes && first.known == second.known &&
          same_machine(first.progress.machine, second.progress.machine));
  }

  const std::uint32_t sites[] = {0x80068290u, 0x800682b4u, 0x800682d0u,
                                 0x800682d8u, 0x800682e0u};
  for (std::size_t ordinal = 1u; ordinal <= 5u; ++ordinal) {
    Fixture fixture;
    fixture.refuse_ordinal = ordinal;
    check(fixture.run() == NBA97_TEXT_IO_REFUSED &&
          fixture.calls.size() == ordinal &&
          fixture.progress.stopped_pc == sites[ordinal - 1u] &&
          fixture.progress.callbacks_completed == ordinal - 1u);
  }
  Fixture alternate;
  alternate.put(Team, 1u, 2u);
  alternate.refuse_ordinal = 2u;
  check(alternate.run() == NBA97_TEXT_IO_REFUSED &&
        alternate.progress.stopped_pc == 0x800682c4u);

  Fixture refusal_prefix;
  refusal_prefix.refuse_ordinal = 1u;
  refusal_prefix.mutate_before_refusal = true;
  check(refusal_prefix.run() == NBA97_TEXT_IO_REFUSED &&
        refusal_prefix.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0]
                .word == 0xdeadbeefu &&
        refusal_prefix.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0]
                .known_mask == 0x06u &&
        refusal_prefix.progress.machine.hi.word == 0x11223344u &&
        refusal_prefix.progress.machine.hi.known_mask == 0x09u);

  Fixture invalid;
  invalid.invalid_ordinal = 1u;
  check(invalid.run() == NBA97_TEXT_ARGUMENT &&
        invalid.progress.stopped_pc == 0x80068290u &&
        invalid.progress.machine.hi.known_mask == 0x10u);
}

void live_callback_state_aliases_and_addresses() {
  Fixture live;
  live.relocate_stack = true;
  live.mutate_rule_start_a0 = true;
  check(live.run() == NBA97_TEXT_COMPLETE && live.calls.size() == 5u);
  check(live.progress.restored_return_address.word == 0x8006abc0u &&
        live.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8006abc0u &&
        live.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x800ee018u &&
        live.progress.machine.hi.word == 0x0badf00du &&
        live.progress.machine.hi.known_mask == 0x06u &&
        live.progress.machine.lo.word == 0xfeedfaceu &&
        live.progress.machine.lo.known_mask == 0x09u);
  check(live.calls[4].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            0xcafebabeu &&
        live.calls[4]
                .machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                .known_mask == 0x07u);

  Fixture alias;
  alias.put(Pointer2, Timer - 0x10u, 4u);
  alias.put(Timer - 8u, 0u, 4u);
  alias.put(Timer, 12u, 2u);
  alias.put(Timer + 2u, 0u, 2u);
  check(alias.run() == NBA97_TEXT_COMPLETE &&
        alias.progress.timer_accumulated && alias.calls.empty() &&
        alias.get(Timer, 2u) == 13u && alias.progress.blocker_cleared);

  Fixture saved_alias;
  saved_alias.put(Actor1 + 0x10u, 7u, 4u);
  saved_alias.put(Actor2 + 8u, 3u, 4u);
  saved_alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
      CrossingBlock + 8u, 0x0f};
  check(saved_alias.run() == NBA97_TEXT_COMPLETE &&
        saved_alias.get(CrossingBlock, 4u) == 0x80068e38u &&
        saved_alias.progress.restored_return_address.word == 0x80068e38u &&
        !saved_alias.progress.armed);

  Fixture misaligned;
  misaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
      0x800ff002u;
  check(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.stopped_pc == 0x80068190u);

  Fixture resource;
  resource.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
      0x90000000u;
  check(resource.run() == NBA97_TEXT_RESOURCE &&
        resource.progress.stopped_pc == 0x80068190u);

  Fixture pointer_alignment;
  pointer_alignment.put(Pointer1, Actor1 + 2u, 4u);
  check(pointer_alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        pointer_alignment.progress.stopped_pc == 0x800681ccu);

  Fixture pointer_unknown;
  pointer_unknown.known[pointer_unknown.offset(Pointer1) + 3u] = 0u;
  check(pointer_unknown.run() == NBA97_TEXT_UNKNOWN &&
        pointer_unknown.progress.stopped_pc == 0x800681ccu);

  Fixture wrapped;
  wrapped.regions[1] = {0u, wrapped.low_bytes.data(), wrapped.low_known.data(),
                        wrapped.low_bytes.size()};
  wrapped.context.memory.count = 2u;
  wrapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x10u,
                                                                      0x0f};
  check(
      wrapped.run() == NBA97_TEXT_COMPLETE &&
      wrapped.progress.frame_stack_pointer == 0xfffffff8u &&
      wrapped.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
          0x10u);

  Fixture high;
  high.regions[1] = {0xfffffff8u, high.high_bytes.data(),
                     high.high_known.data(), high.high_bytes.size()};
  high.context.memory.count = 2u;
  high.relocate_stack = true;
  high.context.io = [](void *opaque, const Nba97GameTextMemory *,
                       const Nba97GameCrossHalfRuleEvent *event,
                       Nba97GameCrossHalfRuleMachine *machine) -> int {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.calls.push_back({*event, *machine});
    if (fixture.calls.size() == 1u) {
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0u, 0x0f};
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0xffffffe8u, 0x0f};
      for (unsigned byte = 0u; byte != 4u; ++byte)
        fixture.high_bytes[byte] =
            static_cast<std::uint8_t>(0x8006c000u >> (8u * byte));
    }
    return 1;
  };
  check(high.run() == NBA97_TEXT_COMPLETE &&
        high.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0u &&
        high.progress.restored_return_address.word == 0x8006c000u);

  Fixture bad_metadata;
  bad_metadata.regions[0].size = std::numeric_limits<std::size_t>::max();
  check(bad_metadata.run() == NBA97_TEXT_ARGUMENT &&
        bad_metadata.progress.operations == 0u);

  Fixture overlap;
  overlap.regions[1] = {Ram + 16u, overlap.low_bytes.data(),
                        overlap.low_known.data(), overlap.low_bytes.size()};
  overlap.context.memory.count = 2u;
  check(overlap.run() == NBA97_TEXT_ARGUMENT &&
        overlap.progress.operations == 0u);
}

void return_masks_and_delay_prefix() {
  for (unsigned mask = 0u; mask != 16u; ++mask) {
    Fixture fixture;
    fixture.put(Owner, 0xffffu, 2u);
    fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        0x80068e38u, static_cast<std::uint8_t>(mask)};
    const int result = fixture.run();
    if (mask == 15u)
      check(result == NBA97_TEXT_COMPLETE && fixture.progress.completed);
    else
      check(result == NBA97_TEXT_UNKNOWN &&
            fixture.progress.stopped_pc == 0x80068304u &&
            fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                    .word == 0x800ff000u &&
            fixture.progress.restored_return_address.known_mask == mask);
  }

  Fixture unaligned;
  unaligned.put(Owner, 0xffffu, 2u);
  unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
      0x80068e3au, 0x0f};
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80068304u &&
        unaligned.progress.stopped_address == 0x80068e3au &&
        unaligned.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == 0x800ff000u);
}
} // namespace

int main() {
  normal_paths_and_determinism();
  clear_and_uncleared_exits();
  arm_store_order_and_timer_edges();
  unknown_and_malformed_prefixes();
  operation_budgets_and_refusals();
  live_callback_state_aliases_and_addresses();
  return_masks_and_delay_prefix();
  std::printf("%u cross-half rule checks passed\n", checks);
  return 0;
}
