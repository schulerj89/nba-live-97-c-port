#include "game_team_strategy_apply_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

size_t checks;

void check(bool condition, const char *message) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "game_team_strategy_apply_integration_tests: %s\n",
                 message);
    std::exit(1);
  }
}

void write16(std::vector<uint8_t> &data, uint32_t address, uint16_t value) {
  const size_t offset = address - UINT32_C(0x80000000);
  data[offset] = static_cast<uint8_t>(value);
  data[offset + 1u] = static_cast<uint8_t>(value >> 8u);
}

uint16_t read16(const std::vector<uint8_t> &data, uint32_t address) {
  const size_t offset = address - UINT32_C(0x80000000);
  return static_cast<uint16_t>(data[offset] |
                               static_cast<uint16_t>(data[offset + 1u]) << 8u);
}

bool same_word(const Nba97GameMatchStateResetWord &left,
               const Nba97GameMatchStateResetWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool same_machine(const Nba97GameMatchStateResetMachine &left,
                  const Nba97GameMatchStateResetMachine &right) {
  for (unsigned index = 0u; index != 32u; ++index)
    if (!same_word(left.registers.gpr[index], right.registers.gpr[index]))
      return false;
  return same_word(left.hi, right.hi) && same_word(left.lo, right.lo);
}

struct Fixture {
  static constexpr uint32_t base = UINT32_C(0x80000000);
  static constexpr size_t size = 0x200000u;
  static constexpr uint32_t stack = UINT32_C(0x801ff000);
  static constexpr uint32_t home = UINT32_C(0x8001edf4);
  static constexpr uint32_t away = UINT32_C(0x8001eeb8);
  std::vector<uint8_t> data = std::vector<uint8_t>(size, 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(size, 1u);
  Nba97GameTextRegion region{base, data.data(), known.data(), data.size()};
  Nba97GameTeamStrategyApplyResetBinding binding{};
  Nba97GameMatchStateResetContext context{};
  Nba97GameMatchStateResetProgress progress{};
  std::array<Nba97GameTeamStrategyApplyEvent, 4> child_events{};
  size_t child_count{};
  size_t fallback_count{};
  bool mutate_first_child_s1{};

  explicit Fixture(size_t strategy_budget = 128u) {
    write16(data, UINT32_C(0x8001edec), 0u);
    write16(data, home + 0x14u, 0u);
    write16(data, away + 0x14u, 1u);
    write16(data, home + 0x42u, 0u);
    write16(data, away + 0x42u, 0u);
    write16(data, home + 0x66u, 2u);
    write16(data, away + 0x66u, 4u);
    data[UINT32_C(0x80021ed5) - base] = 0u;
    data[UINT32_C(0x80021ed6) - base] = 4u;

    nba97_game_team_strategy_apply_reset_binding_init(
        &binding, strategy_budget, strategy_child, this, nullptr, 0u,
        reset_fallback, this);
    context.memory = {&region, 1u};
    context.operation_budget = 128u;
    context.io = nba97_game_team_strategy_apply_from_reset;
    context.user = &binding;
    for (unsigned index = 0u; index != 32u; ++index)
      context.machine.registers.gpr[index] =
          {UINT32_C(0x11000000) + index, 15u};
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {stack, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {UINT32_C(0x81234568), 15u};
    context.machine.hi = {UINT32_C(0x12345678), 7u};
    context.machine.lo = {UINT32_C(0x87654321), 11u};
  }

  static int reset_fallback(void *opaque, const Nba97GameTextMemory *,
                            const Nba97GameMatchStateResetEvent *,
                            Nba97GameMatchStateResetMachine *) {
    ++static_cast<Fixture *>(opaque)->fallback_count;
    return 1;
  }

  static int strategy_child(void *opaque, const Nba97GameTextMemory *,
                            const Nba97GameTeamStrategyApplyEvent *event,
                            Nba97GameTeamStrategyApplyMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.child_events[fixture.child_count++] = *event;
    if (fixture.mutate_first_child_s1 && fixture.child_count == 1u)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1u] =
          {UINT32_C(0x90000000), 15u};
    return 1;
  }

  int run() { return nba97_game_match_state_reset(&context, &progress); }
};

int accepting_reset_fallback(void *opaque, const Nba97GameTextMemory *,
                             const Nba97GameMatchStateResetEvent *,
                             Nba97GameMatchStateResetMachine *) {
  ++*static_cast<size_t *>(opaque);
  return 1;
}

void natural_two_calls() {
  Fixture fixture;
  check(fixture.run() == NBA97_TEXT_COMPLETE, "natural reset completes");
  check(fixture.binding.invocations == 2u &&
            fixture.binding.completions == 2u,
        "both strategy calls composed");
  check(fixture.binding.event.pc == UINT32_C(0x80065ac4) &&
            fixture.binding.event.delay_slot_pc == UINT32_C(0x80065ac8) &&
            fixture.binding.event.entry == UINT32_C(0x80065820),
        "second natural site retained");
  check(fixture.child_count == 2u &&
            fixture.child_events[0].kind ==
                NBA97_GAME_TEAM_STRATEGY_APPLY_80064DBC &&
            fixture.child_events[1].kind ==
                NBA97_GAME_TEAM_STRATEGY_APPLY_80064DBC,
        "both injury children remain typed");
  check(read16(fixture.data, Fixture::home + 0x66u) == 1u &&
            read16(fixture.data, Fixture::away + 0x66u) == 3u,
        "both team counts decremented");
  check(fixture.data[Fixture::home - Fixture::base + 0x76u] == 1u &&
            fixture.data[Fixture::home - Fixture::base + 0x77u] == 1u &&
            fixture.data[Fixture::away - Fixture::base + 0x76u] == 1u &&
            fixture.data[Fixture::away - Fixture::base + 0x77u] == 1u,
        "both CPU strategy defaults applied");
  check(fixture.fallback_count == 12u,
        "other reset children routed to typed fallback");
  check(fixture.progress.machine.hi.word == UINT32_C(0x12345678) &&
            fixture.progress.machine.hi.known_mask == 7u &&
            fixture.progress.machine.lo.word == UINT32_C(0x87654321) &&
            fixture.progress.machine.lo.known_mask == 11u,
        "natural HI LO retained");
}

void nested_failure_prefix() {
  Fixture fixture(0u);
  check(fixture.run() == NBA97_TEXT_IO_REFUSED,
        "reset reports nested strategy refusal");
  check(fixture.binding.invocations == 1u &&
            fixture.binding.result == NBA97_TEXT_LIMIT,
        "nested result and invocation retained");
  check(fixture.binding.progress.stopped_pc == UINT32_C(0x80065824) &&
            fixture.binding.progress.operations == 0u,
        "strategy zero-budget prefix");
  check(fixture.progress.stopped_pc == UINT32_C(0x80065abc) &&
            fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                    .word == UINT32_C(0x80065ac4) &&
            fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                    .word == Fixture::stack - 0x38u,
        "reset and nested frames plus first-call RA retained");
  check(read16(fixture.data, UINT32_C(0x800fdb54)) == 1u,
        "first-call delay store completed");
}

void natural_second_call_failure() {
  Fixture fixture;
  fixture.mutate_first_child_s1 = true;
  check(fixture.run() == NBA97_TEXT_IO_REFUSED,
        "mutated live S1 makes second strategy call fail");
  check(fixture.binding.invocations == 2u &&
            fixture.binding.completions == 1u &&
            fixture.binding.result == NBA97_TEXT_RESOURCE,
        "first call completes before second nested resource failure");
  check(fixture.progress.stopped_pc == UINT32_C(0x80065ac4) &&
            fixture.progress.machine.registers
                    .gpr[NBA97_MATCH_INITIALIZE_A0]
                    .word == UINT32_C(0x90000000) &&
            fixture.progress.machine.registers
                    .gpr[NBA97_MATCH_INITIALIZE_RA]
                    .word == UINT32_C(0x80065acc),
        "second JAL delay A0 and return address retained");
  check(read16(fixture.data, Fixture::home + 0x66u) == 1u &&
            fixture.data[Fixture::home - Fixture::base + 0x76u] == 1u &&
            fixture.data[Fixture::home - Fixture::base + 0x77u] == 1u &&
            read16(fixture.data, UINT32_C(0x800fdb54)) == 1u,
        "first strategy stores and first-call delay store remain");
  check(fixture.binding.progress.stopped_pc == UINT32_C(0x80065830) &&
            fixture.binding.progress.machine.registers
                    .gpr[NBA97_MATCH_INITIALIZE_SP]
                    .word == Fixture::stack - 0x38u,
        "second owner failure retains nested frame");
}

void adapter_guards_reuse_and_fallback() {
  Fixture fixture;
  Nba97GameMatchStateResetEvent event{};
  event.pc = UINT32_C(0x80065abc);
  event.delay_slot_pc = UINT32_C(0x80065ac0);
  event.entry = UINT32_C(0x80065820);
  event.kind = NBA97_GAME_MATCH_STATE_RESET_80065820;
  event.argument_count = 1u;
  event.invocation = 1u;
  auto machine = fixture.context.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {Fixture::home, 15u};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
      {UINT32_C(0x80065ac4), 15u};
  auto original = machine;
  size_t fallback = 0u;
  nba97_game_team_strategy_apply_reset_binding_init(
      &fixture.binding, 128u, Fixture::strategy_child, &fixture, nullptr, 0u,
      accepting_reset_fallback, &fallback);

  event.kind = NBA97_GAME_MATCH_STATE_RESET_800646A8;
  check(nba97_game_team_strategy_apply_from_reset(
            &fixture.binding, &fixture.context.memory, &event, &machine) == 0,
        "assigned entry wrong kind rejects");
  check(fallback == 0u && same_machine(machine, original),
        "malformed assigned event immutable");
  event.kind = NBA97_GAME_MATCH_STATE_RESET_80065820;
  event.delay_slot_pc++;
  check(nba97_game_team_strategy_apply_from_reset(
            &fixture.binding, &fixture.context.memory, &event, &machine) == 0,
        "wrong delay rejects");
  event.delay_slot_pc--;
  event.invocation = 2u;
  check(nba97_game_team_strategy_apply_from_reset(
            &fixture.binding, &fixture.context.memory, &event, &machine) == 0,
        "wrong first-site invocation rejects");
  event.invocation = 1u;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 14u;
  check(nba97_game_team_strategy_apply_from_reset(
            &fixture.binding, &fixture.context.memory, &event, &machine) == 0,
        "partial RA rejects");

  machine = original;
  event.pc = UINT32_C(0x80065aa4);
  event.delay_slot_pc = UINT32_C(0x80065aa8);
  event.kind = NBA97_GAME_MATCH_STATE_RESET_800646A8;
  event.entry = UINT32_C(0x800646a8);
  check(nba97_game_team_strategy_apply_from_reset(
            &fixture.binding, &fixture.context.memory, &event, &machine) == 1,
        "unrelated event falls back");
  check(fallback == 1u && fixture.binding.fallback_invocations == 1u,
        "fallback accounting");

  event.pc = UINT32_C(0x80065ac4);
  event.delay_slot_pc = UINT32_C(0x80065ac8);
  event.entry = UINT32_C(0x80065820);
  event.kind = NBA97_GAME_MATCH_STATE_RESET_80065820;
  event.invocation = 2u;
  machine = original;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
      {UINT32_C(0x80065acc), 15u};
  check(nba97_game_team_strategy_apply_from_reset(
            &fixture.binding, &fixture.context.memory, &event, &machine) == 1,
        "second assigned site accepted after earlier guards");
  check(fixture.binding.invocations == 1u && fixture.binding.completions == 1u,
        "adapter reuse accounting");
}

} // namespace

int main() {
  natural_two_calls();
  nested_failure_prefix();
  natural_second_call_failure();
  adapter_guards_reuse_and_fallback();
  std::printf("game_team_strategy_apply_integration_tests: %zu checks passed\n",
              checks);
  return 0;
}
