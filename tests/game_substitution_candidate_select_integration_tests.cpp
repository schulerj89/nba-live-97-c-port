#include "game_substitution_candidate_select_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
size_t checks;
void check(bool value, const char *message) {
  ++checks;
  if (!value) {
    std::fprintf(stderr,
                 "game_substitution_candidate_select_integration_tests: %s\n",
                 message);
    std::exit(1);
  }
}
void put(std::vector<uint8_t> &data, uint32_t address, uint32_t value,
         unsigned width) {
  for (unsigned b = 0u; b != width; ++b)
    data[address - UINT32_C(0x80000000) + b] =
        static_cast<uint8_t>(value >> (b * 8u));
}
uint32_t get(const std::vector<uint8_t> &data, uint32_t address,
             unsigned width) {
  uint32_t value = 0u;
  for (unsigned b = 0u; b != width; ++b)
    value |= static_cast<uint32_t>(data[address - UINT32_C(0x80000000) + b])
             << (b * 8u);
  return value;
}
bool same_word(const Nba97GameTeamStrategyApplyWord &a,
               const Nba97GameTeamStrategyApplyWord &b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}
bool same_machine(const Nba97GameTeamStrategyApplyMachine &a,
                  const Nba97GameTeamStrategyApplyMachine &b) {
  for (unsigned i = 0u; i != 32u; ++i)
    if (!same_word(a.registers.gpr[i], b.registers.gpr[i]))
      return false;
  return same_word(a.hi, b.hi) && same_word(a.lo, b.lo);
}
struct Fixture {
  static constexpr uint32_t base = UINT32_C(0x80000000);
  static constexpr size_t size = 0x200000u;
  static constexpr uint32_t team = UINT32_C(0x80010000);
  static constexpr uint32_t pointers = UINT32_C(0x80011000);
  static constexpr uint32_t player = UINT32_C(0x80012000);
  static constexpr uint32_t stack = UINT32_C(0x801ff000);
  std::vector<uint8_t> data = std::vector<uint8_t>(size, 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(size, 1u);
  Nba97GameTextRegion region{base, data.data(), known.data(), data.size()};
  Nba97GameSubstitutionCandidateSelectStrategyBinding binding{};
  Nba97GameTeamStrategyApplyContext context{};
  Nba97GameTeamStrategyApplyProgress progress{};
  Nba97GameSubstitutionCandidateSelectEvent child{};
  Nba97GameSubstitutionCandidateSelectMachine child_machine{};
  size_t child_calls{};
  size_t fallback_calls{};
  bool corrupt_outer_return{};

  explicit Fixture(size_t candidate_budget = 512u) {
    put(data, team + 0x14u, 0u, 2u);
    put(data, team + 0x42u, 0u, 2u);
    put(data, team + 0x66u, 3u, 2u);
    put(data, team + 0x68u, 1u, 2u);
    put(data, team + 0x7cu, pointers, 4u);
    put(data, team + 0x80u, 5u, 2u);
    put(data, pointers, player, 4u);
    put(data, player + 8u, 0u, 1u);
    put(data, UINT32_C(0x8001edec), 0u, 2u);
    data[UINT32_C(0x80021ed5) - base] = 0u;
    put(data, UINT32_C(0x8001f80c), 0x7332u, 2u);
    nba97_game_substitution_candidate_select_strategy_binding_init(
        &binding, candidate_budget, child_service, this, nullptr, 0u, fallback,
        this);
    context.memory = {&region, 1u};
    context.operation_budget = 256u;
    context.io = nba97_game_substitution_candidate_select_from_strategy;
    context.user = &binding;
    for (unsigned i = 0u; i != 32u; ++i)
      context.machine.registers.gpr[i] = {UINT32_C(0x31000000) + i, 15u};
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {team, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {stack, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        UINT32_C(0x80065ac4), 15u};
    context.machine.hi = {UINT32_C(0x11223344), 5u};
    context.machine.lo = {UINT32_C(0x55667788), 10u};
  }
  static int
  child_service(void *opaque, const Nba97GameTextMemory *,
                const Nba97GameSubstitutionCandidateSelectEvent *event,
                Nba97GameSubstitutionCandidateSelectMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.child = *event;
    f.child_machine = *machine;
    ++f.child_calls;
    if (f.corrupt_outer_return)
      put(f.data, stack - 4, 0x80065ac5u, 4);
    return 1;
  }
  static int fallback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameTeamStrategyApplyEvent *,
                      Nba97GameTeamStrategyApplyMachine *) {
    ++static_cast<Fixture *>(opaque)->fallback_calls;
    return 1;
  }
  int run() { return nba97_game_team_strategy_apply(&context, &progress); }
};

void natural_hit_and_no_call() {
  Fixture hit;
  check(hit.run() == NBA97_TEXT_COMPLETE, "natural BR to BU hit completes");
  check(hit.binding.invocations == 1u && hit.binding.completions == 1u &&
            hit.child_calls == 1u,
        "candidate owner and typed substitution invoked");
  check(hit.binding.event.pc == UINT32_C(0x800659c4) &&
            hit.binding.event.delay_slot_pc == UINT32_C(0x800659c8) &&
            hit.binding.event.entry == UINT32_C(0x80064dbc) &&
            hit.binding.event.invocation == 1u,
        "natural exact BR event");
  check(hit.child.pc == UINT32_C(0x80065038) &&
            hit.child.entry == UINT32_C(0x800649d8) &&
            hit.child_machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
                5u,
        "natural candidate child and lineup argument");
  check(get(hit.data, Fixture::team + 0x66u, 2u) == 2u,
        "BR continues with count decrement");

  Fixture no_call;
  no_call.data[UINT32_C(0x80021ed5) - Fixture::base] = 12u;
  check(no_call.run() == NBA97_TEXT_COMPLETE &&
            no_call.binding.invocations == 0u && no_call.child_calls == 0u,
        "BR injury branch does not invoke BU");
}

void nested_failure_and_guards() {
  Fixture limited(0u);
  check(limited.run() == NBA97_TEXT_IO_REFUSED,
        "BR reports nested candidate failure");
  check(limited.binding.result == NBA97_TEXT_LIMIT &&
            limited.binding.progress.stopped_pc == UINT32_C(0x80064dc0) &&
            limited.progress.stopped_pc == UINT32_C(0x800659c4),
        "nested zero-budget prefix retained");
  check(
      limited.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
          UINT32_C(0x800659cc),
      "BR call RA remains live");

  Fixture guards;
  Nba97GameTeamStrategyApplyEvent event{};
  event.pc = UINT32_C(0x800659c4);
  event.delay_slot_pc = UINT32_C(0x800659c8);
  event.entry = UINT32_C(0x80064dbc);
  event.kind = NBA97_GAME_TEAM_STRATEGY_APPLY_80064DBC;
  event.argument_count = 4u;
  event.invocation = 1u;
  auto machine = guards.context.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {UINT32_C(0x800659cc),
                                                      15u};
  auto original = machine;
  event.kind = NBA97_GAME_TEAM_STRATEGY_APPLY_800646A8;
  check(nba97_game_substitution_candidate_select_from_strategy(
            &guards.binding, &guards.context.memory, &event, &machine) == 0,
        "assigned PC wrong kind rejects");
  check(guards.fallback_calls == 0u && same_machine(machine, original),
        "malformed assigned event immutable and not forwarded");
  event.kind = NBA97_GAME_TEAM_STRATEGY_APPLY_80064DBC;
  event.invocation = 2u;
  check(nba97_game_substitution_candidate_select_from_strategy(
            &guards.binding, &guards.context.memory, &event, &machine) == 0,
        "wrong invocation rejects");
  event.pc = UINT32_C(0x80065998);
  event.delay_slot_pc = UINT32_C(0x8006599c);
  event.entry = UINT32_C(0x800646a8);
  event.kind = NBA97_GAME_TEAM_STRATEGY_APPLY_800646A8;
  check(nba97_game_substitution_candidate_select_from_strategy(
            &guards.binding, &guards.context.memory, &event, &machine) == 1 &&
            guards.fallback_calls == 1u,
        "unrelated strategy child falls back");
}

void outer_return_trap_and_reuse() {
  Fixture f;
  f.corrupt_outer_return = true;
  check(f.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
            f.progress.stopped_pc == 0x800659e8u,
        "BR refuses corrupted outer return after real BU child");
  check(f.binding.progress.completed &&
            f.binding.progress.machine.registers.gpr[31].word == 0x800659ccu &&
            get(f.data, Fixture::team + 0x66, 2) == 2,
        "BU completes and BR count store precedes outer trap");
  check(f.progress.machine.registers.gpr[29].word == Fixture::stack &&
            same_word(f.progress.machine.registers.gpr[16],
                      f.context.machine.registers.gpr[16]),
        "outer trap follows S0 and SP restoration");
  Fixture repeat;
  check(repeat.run() == NBA97_TEXT_COMPLETE &&
            repeat.run() == NBA97_TEXT_COMPLETE &&
            repeat.binding.invocations == 2 && repeat.binding.completions == 2,
        "binding repeated natural calls complete");
  for (unsigned field = 0; field < 8; ++field) {
    Fixture g;
    Nba97GameTeamStrategyApplyEvent e{0x800659c4u,
                                      0x800659c8u,
                                      0x80064dbcu,
                                      1,
                                      1,
                                      NBA97_GAME_TEAM_STRATEGY_APPLY_80064DBC,
                                      4};
    auto m = g.context.machine;
    m.registers.gpr[31] = {0x800659ccu, 15};
    if (field == 0)
      ++e.pc;
    else if (field == 1)
      ++e.delay_slot_pc;
    else if (field == 2)
      ++e.entry;
    else if (field == 3)
      ++e.invocation;
    else if (field == 4)
      e.kind = 1;
    else if (field == 5)
      e.argument_count = 3;
    else if (field == 6)
      m.registers.gpr[31].known_mask = 14;
    else
      m.hi.known_mask = 16;
    const auto before = m;
    check(!nba97_game_substitution_candidate_select_from_strategy(
              &g.binding, &g.context.memory, &e, &m) &&
              g.binding.result == NBA97_TEXT_ARGUMENT &&
              same_machine(before, m) && g.fallback_calls == 0 &&
              g.binding.invocations == 0,
          "all malformed assigned identifiers refuse without fallback");
  }
}
} // namespace

int main() {
  outer_return_trap_and_reuse();
  natural_hit_and_no_call();
  nested_failure_and_guards();
  std::printf("game_substitution_candidate_select_integration_tests: %zu "
              "checks passed\n",
              checks);
  return 0;
}
