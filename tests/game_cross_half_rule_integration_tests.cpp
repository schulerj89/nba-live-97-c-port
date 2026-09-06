#include "game_cross_half_rule_adapter.h"

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
    std::fprintf(stderr,
                 "cross-half rule integration check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Actor1 = 0x80010000u;
constexpr std::uint32_t Actor2 = 0x80011000u;
constexpr std::uint32_t Ball = 0x80012000u;

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

struct ExpectedService {
  std::uint32_t pc;
  std::uint32_t entry;
  std::uint32_t a0;
  unsigned count;
};

struct NaturalFixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000u, 0u);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x110000u, 1u);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97MatchTickContext tick{};
  Nba97MatchTickProgress tick_progress{};
  Nba97GameCrossHalfRuleBinding binding{};
  std::vector<Nba97MatchTickCall> services;
  std::vector<Nba97GameCrossHalfRuleEvent> children;
  std::vector<Nba97GameCrossHalfRuleMachine> child_machines;
  std::size_t expected_index{};
  unsigned player_calls{};
  unsigned ball_calls{};
  bool unexpected{};
  bool refuse_gate{};

  static constexpr std::array<ExpectedService, 14> Expected{{
      {0x80068c24u, 0x80066f88u, 0u, 0u},
      {0x80068c2cu, 0x80079664u, 0u, 1u},
      {0x80068c4cu, 0x80067468u, 0u, 0u},
      {0x80068cecu, 0x80067550u, 0u, 0u},
      {0x80068cf4u, 0x800675e4u, 0u, 0u},
      {0x80068d40u, 0x80067a60u, 1u, 1u},
      {0x80068d64u, 0x80067d38u, 1u, 1u},
      {0x80068d6cu, 0x80067664u, 0u, 0u},
      {0x80068d7cu, 0x8002de34u, 0u, 0u},
      {0x80068e00u, 0x80060ef8u, 0u, 0u},
      {0x80068e08u, 0x80060fbcu, 0u, 0u},
      {0x80068e28u, 0x800747b0u, 0u, 0u},
      {0x80068e30u, 0x8006817cu, 0u, 0u},
      {0x80068e38u, 0x8006830cu, 0u, 0u},
  }};

  NaturalFixture() {
    tick.access = access;
    tick.service = service;
    tick.player_update = player;
    tick.ball_simulation = ball;
    tick.net_transform = net;
    tick.match_frame = frame;
    tick.user = this;
    tick.operation_budget = 500u;
    tick.incoming_s6 = {2u, 1u};

    binding.memory = {&region, 1u};
    binding.operation_budget = 100u;
    binding.entry_machine = entry_machine();
    binding.entry_machine_ready = 1u;
    binding.io = child;
    binding.user = this;

    put(0x8001edecu, 1u, 2u);
    put(0x800fdb92u, 2u, 2u);
    put(0x800fdb8au, 1u, 2u);
    put(0x80021d82u, 1u, 1u);
    put(0x800fdb7cu, 0u, 2u);
    put(0x800fe8ccu, 0u, 2u);
    put(0x800fe8c4u, 0u, 2u);
    put(0x800fdb68u, 5u, 2u);
    put(0x800fdb78u, 1u, 1u);
    put(0x800fdb6cu, 1u, 2u);
    put(0x800fdc48u, Ball, 4u);

    put(0x800fdb90u, 100u, 2u);
    put(0x800fdbccu, 0u, 2u);
    put(0x800fdc38u, Actor1, 4u);
    put(0x800fdc34u, Actor2, 4u);
    put(Actor1 + 0x10u, 0x80000000u, 4u);
    put(Actor2 + 8u, 0u, 4u);
    put(Actor2 + 0x10u, 0u, 4u);
    put(0x80021d8bu, 1u, 1u);
    put(0x800fe8e0u, 1u, 2u);
    put(0x800fdbacu, 12u, 2u);
    put(0x800fdb94u, 0u, 2u);
    put(0x800fe882u, 0u, 2u);
  }

  static Nba97GameCrossHalfRuleMachine entry_machine() {
    Nba97GameCrossHalfRuleMachine machine{};
    for (unsigned index = 0u; index != NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
         ++index)
      machine.registers.gpr[index] = {
          0x51000000u + index * 0x01010101u,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0u, 0x0f};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ff000u, 0x0f};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80068e38u, 0x0f};
    machine.hi = {0x13579bdfu, 0x03};
    machine.lo = {0x2468ace0u, 0x0c};
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

  static int access(void *opaque, std::uint32_t, std::uint32_t address,
                    unsigned width, unsigned kind,
                    Nba97PlayerFrameValue *value) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    if (value == nullptr || address < Ram ||
        static_cast<std::uint64_t>(address - Ram) + width >
            fixture.bytes.size())
      return NBA97_BODY_UNKNOWN;
    const auto at = fixture.offset(address);
    if (kind == NBA97_FRAME_READ) {
      value->word = 0u;
      value->known_mask = 0u;
      value->is_reference = 0u;
      value->reference = {};
      for (unsigned byte = 0u; byte != width; ++byte) {
        value->word |= std::uint32_t(fixture.bytes[at + byte]) << (8u * byte);
        if (fixture.known[at + byte] != 0u)
          value->known_mask =
              static_cast<std::uint8_t>(value->known_mask | (1u << byte));
      }
    } else {
      for (unsigned byte = 0u; byte != width; ++byte) {
        fixture.bytes[at + byte] =
            static_cast<std::uint8_t>(value->word >> (8u * byte));
        fixture.known[at + byte] =
            static_cast<std::uint8_t>((value->known_mask >> byte) & 1u);
      }
    }
    return NBA97_BODY_OK;
  }

  static int service(void *opaque, const Nba97MatchTickCall *call,
                     Nba97GamePeriodValue *result) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    /* Every whitelisted completion is a named synthetic fixture contract,
     * never a catch-all success or evidence of an advancing native match. */
    if (call == nullptr || fixture.expected_index >= Expected.size()) {
      fixture.unexpected = true;
      return NBA97_BODY_ARGUMENT;
    }
    const auto &expected = Expected[fixture.expected_index];
    if (call->pc != expected.pc || call->entry != expected.entry ||
        call->args[0] != expected.a0 || call->args[1] != 0u ||
        call->count != expected.count) {
      fixture.unexpected = true;
      return NBA97_BODY_ARGUMENT;
    }
    ++fixture.expected_index;
    fixture.services.push_back(*call);
    if (call->pc == 0x80068d6cu) {
      if (result == nullptr) {
        fixture.unexpected = true;
        return NBA97_BODY_ARGUMENT;
      }
      *result = {0u, 1u};
      return NBA97_BODY_OK;
    }
    if (call->pc == 0x80068e30u) {
      if (result != nullptr) {
        fixture.unexpected = true;
        return NBA97_BODY_ARGUMENT;
      }
      return nba97_game_cross_half_rule_from_match_tick(&fixture.binding, call,
                                                        result);
    }
    if (call->pc == 0x80068e38u)
      return NBA97_BODY_ARGUMENT;
    if (result != nullptr) {
      fixture.unexpected = true;
      return NBA97_BODY_ARGUMENT;
    }
    return NBA97_BODY_OK;
  }

  static int child(void *opaque, const Nba97GameTextMemory *,
                   const Nba97GameCrossHalfRuleEvent *event,
                   Nba97GameCrossHalfRuleMachine *machine) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    if (event == nullptr || machine == nullptr)
      return 0;
    fixture.children.push_back(*event);
    fixture.child_machines.push_back(*machine);
    switch (event->pc) {
    case 0x80068290u:
      if (event->entry != 0x80062d84u || event->argument_count != 0u ||
          event->delay_slot_pc != 0x80068294u)
        return 0;
      if (fixture.refuse_gate)
        return 0;
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0u, 0x0f};
      return 1;
    case 0x800682b4u:
      return event->entry == 0x80029590u && event->argument_count == 1u &&
             machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 11u;
    case 0x800682d8u:
      return event->entry == 0x80062300u && event->argument_count == 1u &&
             machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 5u;
    case 0x800682e0u:
      return event->entry == 0x80062660u && event->argument_count == 0u;
    default:
      return 0;
    }
  }

  static int player(void *opaque, std::uint32_t pc) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    if (pc != 0x80068d84u) {
      fixture.unexpected = true;
      return NBA97_BODY_ARGUMENT;
    }
    ++fixture.player_calls;
    return NBA97_BODY_OK;
  }

  static int ball(void *opaque, std::uint32_t pc, std::uint32_t pointer) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    if (pc != 0x80068d9cu || pointer != Ball) {
      fixture.unexpected = true;
      return NBA97_BODY_ARGUMENT;
    }
    ++fixture.ball_calls;
    return NBA97_BODY_OK;
  }

  static int net(void *opaque, std::uint32_t) {
    static_cast<NaturalFixture *>(opaque)->unexpected = true;
    return NBA97_BODY_ARGUMENT;
  }

  static int frame(void *opaque, std::uint32_t) {
    static_cast<NaturalFixture *>(opaque)->unexpected = true;
    return NBA97_BODY_ARGUMENT;
  }
};

void actual_match_tick_with_explicit_machine() {
  NaturalFixture fixture;
  check(nba97_game_match_tick(&fixture.tick, &fixture.tick_progress) ==
        NBA97_BODY_ARGUMENT);
  check(!fixture.unexpected && fixture.expected_index == 14u &&
        fixture.services.size() == 14u && fixture.player_calls == 1u &&
        fixture.ball_calls == 1u);
  for (std::size_t index = 0u; index != NaturalFixture::Expected.size();
       ++index) {
    const auto &actual = fixture.services[index];
    const auto &expected = NaturalFixture::Expected[index];
    check(actual.pc == expected.pc && actual.entry == expected.entry &&
          actual.args[0] == expected.a0 && actual.args[1] == 0u &&
          actual.count == expected.count);
  }
  check(fixture.tick_progress.stopped_pc == 0x80068e38u &&
        fixture.tick_progress.stopped_entry == 0x8006830cu);
  check(fixture.binding.invocations == 1u &&
        fixture.binding.completions == 1u &&
        fixture.binding.result == NBA97_TEXT_COMPLETE &&
        fixture.binding.progress.completed &&
        fixture.binding.rule_delay_invocations == 1u &&
        fixture.binding.rule_delay_result == NBA97_TEXT_COMPLETE &&
        fixture.binding.rule_delay_progress.completed &&
        fixture.binding.fallback_invocations == 4u &&
        fixture.binding.fallback_completions == 4u);
  check(fixture.children.size() == 4u && fixture.get(0x800fdbacu, 2u) == 13u &&
        fixture.get(0x800fe882u, 2u) == 8u &&
        fixture.get(0x800fe8e0u, 2u) == 0u);
  const std::uint32_t child_pc[] = {0x80068290u, 0x800682b4u, 0x800682d8u,
                                    0x800682e0u};
  const std::uint32_t child_entry[] = {0x80062d84u, 0x80029590u, 0x80062300u,
                                       0x80062660u};
  const std::uint32_t child_a0[] = {Actor2, 11u, 5u, 5u};
  const unsigned child_argc[] = {0u, 1u, 1u, 0u};
  for (std::size_t index = 0u; index != 4u; ++index) {
    const auto &event = fixture.children[index];
    const auto &machine = fixture.child_machines[index];
    check(event.pc == child_pc[index] &&
          event.delay_slot_pc == child_pc[index] + 4u &&
          event.entry == child_entry[index] &&
          event.argument_count == child_argc[index] && event.invocation == 1u &&
          event.operation != 0u);
    check(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
              child_pc[index] + 8u &&
          machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 0x0f &&
          machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
              child_a0[index] &&
          machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask == 0x0f);
  }
  check(fixture.binding.rule_delay_event.pc == 0x800682d0u &&
        fixture.binding.rule_delay_event.delay_slot_pc == 0x800682d4u &&
        fixture.binding.rule_delay_event.entry == 0x800295c8u &&
        fixture.binding.rule_delay_event.argument_count == 1u &&
        fixture.binding.rule_delay_event.invocation == 1u);
}

void direct_binding_reuse() {
  NaturalFixture fixture;
  Nba97MatchTickCall call{0x80068e30u, 0x8006817cu, {0u, 0u}, 0u};
  check(nba97_game_cross_half_rule_from_match_tick(&fixture.binding, &call,
                                                   nullptr) == 1);
  fixture.put(0x800fdbacu, 12u, 2u);
  fixture.put(0x800fe8e0u, 1u, 2u);
  fixture.put(0x800fe882u, 0u, 2u);
  fixture.binding.entry_machine = NaturalFixture::entry_machine();
  check(
      nba97_game_cross_half_rule_from_match_tick(&fixture.binding, &call,
                                                 nullptr) == 1 &&
      fixture.binding.invocations == 2u && fixture.binding.completions == 2u &&
      fixture.binding.rule_delay_invocations == 2u &&
      fixture.binding.fallback_invocations == 8u &&
      fixture.binding.fallback_completions == 8u &&
      fixture.children.size() == 8u && fixture.get(0x800fdbacu, 2u) == 13u &&
      fixture.get(0x800fe882u, 2u) == 8u && fixture.get(0x800fe8e0u, 2u) == 0u);
}

void missing_context_and_refusal_order() {
  NaturalFixture missing;
  Nba97MatchTickCall call{0x80068e30u, 0x8006817cu, {0u, 0u}, 0u};
  missing.binding.entry_machine_ready = 0u;
  check(nba97_game_cross_half_rule_from_match_tick(&missing.binding, &call,
                                                   nullptr) == 0 &&
        missing.binding.result == NBA97_TEXT_ARGUMENT &&
        missing.binding.invocations == 0u);

  NaturalFixture refused;
  refused.refuse_gate = true;
  check(nba97_game_match_tick(&refused.tick, &refused.tick_progress) ==
        NBA97_BODY_ARGUMENT);
  check(!refused.unexpected && refused.expected_index == 13u &&
        refused.services.back().pc == 0x80068e30u &&
        refused.binding.invocations == 1u &&
        refused.binding.completions == 0u &&
        refused.binding.result == NBA97_TEXT_IO_REFUSED &&
        refused.binding.progress.stopped_pc == 0x80068290u &&
        refused.binding.progress.timer_accumulated &&
        refused.get(0x800fdbacu, 2u) == 13u &&
        refused.get(0x800fe8e0u, 2u) == 1u &&
        refused.tick_progress.stopped_pc == 0x80068e30u);
}

void outer_guards_and_rule_delay_routing() {
  NaturalFixture fixture;
  Nba97MatchTickCall call{0x80068e30u, 0x8006817cu, {0u, 0u}, 0u};
  auto malformed = call;
  malformed.pc += 4u;
  check(nba97_game_cross_half_rule_from_match_tick(&fixture.binding, &malformed,
                                                   nullptr) == 0);
  malformed = call;
  malformed.entry += 4u;
  check(nba97_game_cross_half_rule_from_match_tick(&fixture.binding, &malformed,
                                                   nullptr) == 0);
  malformed = call;
  malformed.count = 1u;
  check(nba97_game_cross_half_rule_from_match_tick(&fixture.binding, &malformed,
                                                   nullptr) == 0);
  Nba97GamePeriodValue stale{7u, 1u};
  check(nba97_game_cross_half_rule_from_match_tick(&fixture.binding, &call,
                                                   &stale) == 0);
  fixture.binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
      0x80068e38u, 0x0e};
  check(nba97_game_cross_half_rule_from_match_tick(&fixture.binding, &call,
                                                   nullptr) == 0);
  check(fixture.binding.invocations == 0u);

  Nba97GameCrossHalfRuleBinding child_binding{};
  child_binding.io = NaturalFixture::child;
  child_binding.user = &fixture;
  Nba97GameCrossHalfRuleEvent event{};
  event.pc = 0x800682d0u;
  event.delay_slot_pc = 0x800682d4u;
  event.entry = 0x800295c8u;
  event.operation = 17u;
  event.invocation = 1u;
  event.kind = NBA97_GAME_CROSS_HALF_RULE_CHILD_800295C8;
  event.argument_count = 1u;
  auto machine = NaturalFixture::entry_machine();
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {5000u, 0x0f};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x800682d8u, 0x0f};
  const auto before = machine;
  check(nba97_game_cross_half_rule_compose_child(
            &child_binding, &fixture.binding.memory, &event, &machine) == 1 &&
        child_binding.rule_delay_invocations == 1u &&
        child_binding.rule_delay_result == NBA97_TEXT_COMPLETE &&
        child_binding.rule_delay_progress.completed &&
        same_machine(machine, before));

  auto bad_event = event;
  bad_event.delay_slot_pc += 4u;
  const auto before_bad = machine;
  check(nba97_game_cross_half_rule_compose_child(&child_binding,
                                                 &fixture.binding.memory,
                                                 &bad_event, &machine) == 0 &&
        child_binding.fallback_invocations == 0u &&
        same_machine(machine, before_bad));

  bad_event = event;
  bad_event.kind = NBA97_GAME_CROSS_HALF_RULE_CHILD_80062300;
  check(nba97_game_cross_half_rule_compose_child(&child_binding,
                                                 &fixture.binding.memory,
                                                 &bad_event, &machine) == 0 &&
        child_binding.fallback_invocations == 0u);

  Nba97GameCrossHalfRuleEvent unrelated{};
  unrelated.pc = 0x800682d8u;
  unrelated.delay_slot_pc = 0x800682dcu;
  unrelated.entry = 0x80062300u;
  unrelated.operation = 1u;
  unrelated.invocation = 1u;
  unrelated.kind = NBA97_GAME_CROSS_HALF_RULE_CHILD_80062300;
  unrelated.argument_count = 1u;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {5u, 0x0f};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x800682e0u, 0x0f};
  check(nba97_game_cross_half_rule_compose_child(&child_binding,
                                                 &fixture.binding.memory,
                                                 &unrelated, &machine) == 1 &&
        child_binding.fallback_invocations == 1u &&
        child_binding.fallback_completions == 1u);
}
} // namespace

int main() {
  actual_match_tick_with_explicit_machine();
  direct_binding_reuse();
  missing_context_and_refusal_order();
  outer_guards_and_rule_delay_routing();
  std::printf("%u cross-half rule integration checks passed\n", checks);
  return 0;
}
