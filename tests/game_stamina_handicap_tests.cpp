#include "recovered/game_stamina_handicap.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "stamina handicap check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

bool same_word(const Nba97GameStaminaHandicapWord &left,
               const Nba97GameStaminaHandicapWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool same_machine(const Nba97GameStaminaHandicapMachine &left,
                  const Nba97GameStaminaHandicapMachine &right) {
  for (unsigned i = 0u; i != NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    if (!same_word(left.registers.gpr[i], right.registers.gpr[i]))
      return false;
  return same_word(left.hi, right.hi) && same_word(left.lo, right.lo);
}

struct Fixture {
  static constexpr std::uint32_t base = 0x80000000u;
  static constexpr std::size_t size = 0x110000u;
  std::vector<std::uint8_t> data;
  std::vector<std::uint8_t> known;
  std::vector<Nba97GameStaminaHandicapAccess> journal;
  Nba97GameTextRegion region{};
  Nba97GameStaminaHandicapContext context{};
  Nba97GameStaminaHandicapProgress progress{};

  Fixture() : data(size, 0), known(size, 1), journal(512) {
    region = {base, data.data(), known.data(), data.size()};
    context.memory = {&region, 1u};
    context.operation_budget = std::numeric_limits<std::size_t>::max();
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned i = 0u; i != NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
      context.machine.registers.gpr[i] = {0x25000000u + i * 0x01010101u, 0x0fu};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0u, 0x0fu};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ff000u,
                                                                0x0fu};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80068e68u,
                                                                0x0fu};
    context.machine.hi = {0x13579bdfu, 0x03u};
    context.machine.lo = {0x2468ace0u, 0x0cu};

    put(0x80021d81u, 1u, 1u);
    put(0x80021d93u, 1u, 1u);
    put(0x800fdb58u, 4u, 7200u);
    put(0x800fdb7eu, 2u, 1u);
    put(0x800fe8ccu, 2u, 0u);
    put(0x8001ee22u, 2u, 10u);
    put(0x8001eee6u, 2u, 10u);
    for (unsigned i = 0u; i != 24u; ++i)
      put(0x8001f80cu + i * 0x22u, 2u, 100u + i);
    for (unsigned i = 0u; i != 10u; ++i) {
      const auto actor = 0x80030000u + i * 0x200u;
      const auto record = 0x80050000u + i * 0x40u;
      put(0x80020becu + i * 4u, 4u, actor);
      put(actor + 0x1cu, 4u, record);
      put(actor + 0xa0u, 2u, 1u);
      put(actor + 0xddu, 1u, 0u);
      put(actor + 0x44u, 2u, 2u);
      put(record + 0x20u, 2u, 20u);
    }
  }

  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - base);
  }
  void put(std::uint32_t address, unsigned width, std::uint32_t value) {
    for (unsigned i = 0u; i != width; ++i)
      data[offset(address) + i] = static_cast<std::uint8_t>(value >> (8u * i));
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0u;
    for (unsigned i = 0u; i != width; ++i)
      value |= static_cast<std::uint32_t>(data[offset(address) + i])
               << (8u * i);
    return value;
  }
  void mask(std::uint32_t address, unsigned width, std::uint8_t bits) {
    for (unsigned i = 0u; i != width; ++i)
      known[offset(address) + i] = static_cast<std::uint8_t>((bits >> i) & 1u);
  }
  int run() { return nba97_game_stamina_handicap(&context, &progress); }
};

void normal_path_and_machine() {
  Fixture f;
  const auto before = f.context.machine;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
  check(f.progress.score_iterations == 24u && f.progress.score_updates == 24u &&
        f.progress.actor_iterations == 10u &&
        f.progress.stamina_updates == 10u);
  check(f.get(0x800fdb98u, 2u) == 0xffffu);
  for (unsigned i = 0u; i != 24u; ++i)
    check(f.get(0x8001f80cu + i * 0x22u, 2u) == 101u + i);
  for (unsigned i = 0u; i != 10u; ++i) {
    const auto actor = 0x80030000u + i * 0x200u;
    const auto record = 0x80050000u + i * 0x40u;
    check(f.get(record + 0x20u, 2u) == 17u && f.get(actor + 0xddu, 1u) == 0u);
  }
  check(same_word(f.progress.machine.hi, before.hi) &&
        same_word(f.progress.machine.lo, before.lo) &&
        same_word(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP],
                  before.registers.gpr[NBA97_MATCH_INITIALIZE_SP]) &&
        same_word(f.progress.return_address,
                  before.registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
  for (unsigned i = 0u; i != NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    if (i == 0u || i >= 11u)
      check(same_word(f.progress.machine.registers.gpr[i],
                      before.registers.gpr[i]));
  check(f.journal[0].pc == 0x80068508u && f.journal[0].address == 0x80021d81u &&
        f.journal[1].pc == 0x8006851cu && f.journal[1].address == 0x800fdb98u &&
        f.journal[1].kind == NBA97_GAME_STAMINA_HANDICAP_STORE);
}

void handicap_gates_and_delays() {
  struct Case {
    std::uint32_t home;
    std::uint32_t away;
    std::uint32_t expected;
  } cases[] = {{0u, 3u, 0u}, {0u, 2u, 0xffffu}, {2u, 0u, 0xffffu},
               {3u, 0u, 5u}, {0u, 0xffffu, 0u}, {0xffffu, 0u, 5u}};
  for (const auto &entry : cases) {
    Fixture f;
    f.put(0x8001ee22u, 2u, entry.home);
    f.put(0x8001eee6u, 2u, entry.away);
    check(f.run() == NBA97_TEXT_COMPLETE &&
          f.get(0x800fdb98u, 2u) == entry.expected);
  }

  Fixture disabled;
  disabled.put(0x80021d81u, 1u, 0u);
  check(disabled.run() == NBA97_TEXT_COMPLETE &&
        disabled.get(0x800fdb98u, 2u) == 0xffffu);
  Fixture threshold;
  threshold.put(0x800fdb58u, 4u, 7201u);
  threshold.put(0x8001ee22u, 2u, 20u);
  check(threshold.run() == NBA97_TEXT_COMPLETE &&
        threshold.get(0x800fdb98u, 2u) == 0xffffu);
  Fixture negative;
  negative.put(0x800fdb58u, 4u, 0x80000000u);
  negative.put(0x8001ee22u, 2u, 20u);
  check(negative.run() == NBA97_TEXT_COMPLETE &&
        negative.get(0x800fdb98u, 2u) == 5u);

  Fixture unknown_flag;
  unknown_flag.mask(0x80021d81u, 1u, 0u);
  check(unknown_flag.run() == NBA97_TEXT_UNKNOWN &&
        unknown_flag.progress.stopped_pc == 0x80068518u &&
        unknown_flag.get(0x800fdb98u, 2u) == 0xffffu);
  Fixture unknown_clock;
  unknown_clock.mask(0x800fdb58u, 4u, 7u);
  check(unknown_clock.run() == NBA97_TEXT_UNKNOWN &&
        unknown_clock.progress.stopped_pc == 0x80068530u &&
        unknown_clock.progress.machine.registers.gpr[2].known_mask == 0x0eu);

  Fixture unknown_first_score_branch;
  unknown_first_score_branch.put(0x8001ee22u, 2u, 0u);
  unknown_first_score_branch.put(0x8001eee6u, 2u, 1u);
  unknown_first_score_branch.mask(0x8001ee22u, 2u, 1u);
  check(
      unknown_first_score_branch.run() == NBA97_TEXT_UNKNOWN &&
      unknown_first_score_branch.progress.stopped_pc == 0x80068554u &&
      unknown_first_score_branch.progress.machine.registers.gpr[2].known_mask ==
          0x0eu);

  Fixture unknown_second_score_branch;
  unknown_second_score_branch.put(0x8001ee22u, 2u, 2u);
  unknown_second_score_branch.put(0x8001eee6u, 2u, 0u);
  unknown_second_score_branch.mask(0x8001ee22u, 2u, 2u);
  check(unknown_second_score_branch.run() == NBA97_TEXT_UNKNOWN &&
        unknown_second_score_branch.progress.stopped_pc == 0x80068564u &&
        unknown_second_score_branch.progress.machine.registers.gpr[2].word ==
            5u &&
        unknown_second_score_branch.progress.machine.registers.gpr[2]
                .known_mask == 0x0fu);
}

void phase_and_score_boundaries() {
  Fixture phase;
  phase.put(0x800fe8ccu, 2u, 1u);
  check(phase.run() == NBA97_TEXT_COMPLETE &&
        phase.progress.score_iterations == 0u &&
        phase.progress.actor_iterations == 0u &&
        phase.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
            0x800fdb7eu &&
        phase.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            1u);
  Fixture unknown_phase;
  unknown_phase.mask(0x800fe8ccu, 2u, 2u);
  check(unknown_phase.run() == NBA97_TEXT_UNKNOWN &&
        unknown_phase.progress.stopped_pc == 0x80068580u &&
        unknown_phase.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2]
                .word == 0x800fdb7eu);

  Fixture values;
  const std::uint32_t entries[] = {0u, 0x7ffeu, 0x7fffu, 0x8000u, 0xffffu};
  for (unsigned i = 0u; i != 5u; ++i)
    values.put(0x8001f80cu + i * 0x22u, 2u, entries[i]);
  check(values.run() == NBA97_TEXT_COMPLETE);
  check(values.get(0x8001f80cu, 2u) == 1u &&
        values.get(0x8001f82eu, 2u) == 0x7fffu &&
        values.get(0x8001f850u, 2u) == 0x7fffu &&
        values.get(0x8001f872u, 2u) == 0x8000u &&
        values.get(0x8001f894u, 2u) == 0xffffu);

  Fixture underflow;
  underflow.put(0x800fdb7eu, 2u, 0xffffu);
  underflow.put(0x8001f80cu, 2u, 0u);
  check(underflow.run() == NBA97_TEXT_COMPLETE &&
        underflow.get(0x8001f80cu, 2u) == 0xffffu);
  Fixture cap;
  cap.put(0x800fdb7eu, 2u, 0x7fffu);
  cap.put(0x8001f80cu, 2u, 1u);
  check(cap.run() == NBA97_TEXT_COMPLETE &&
        cap.get(0x8001f80cu, 2u) == 0x7fffu);
  Fixture zero_delta;
  zero_delta.put(0x800fdb7eu, 2u, 0u);
  check(zero_delta.run() == NBA97_TEXT_COMPLETE &&
        zero_delta.get(0x8001f80cu, 2u) == 100u);
  Fixture minimum_delta;
  minimum_delta.put(0x800fdb7eu, 2u, 0x8000u);
  minimum_delta.put(0x8001f80cu, 2u, 1u);
  check(minimum_delta.run() == NBA97_TEXT_COMPLETE &&
        minimum_delta.get(0x8001f80cu, 2u) == 0x8001u);
}

void actor_paths_and_store_before_clamp() {
  Fixture f;
  const auto actor0 = 0x80030000u;
  const auto record0 = 0x80050000u;
  f.put(actor0 + 0xddu, 1u, 1u);
  f.put(actor0 + 0x44u, 2u, 3u);
  f.put(record0 + 0x20u, 2u, 5u);
  const auto actor1 = actor0 + 0x200u;
  const auto record1 = record0 + 0x40u;
  f.put(record1 + 0x20u, 2u, 0xffffu);
  f.put(actor1 + 0xddu, 1u, 1u);
  const auto actor2 = actor1 + 0x200u;
  f.put(actor2 + 0xa0u, 2u, 0u);
  f.put(actor2 + 0xddu, 1u, 0xa5u);
  check(f.run() == NBA97_TEXT_COMPLETE);
  check(f.get(record0 + 0x20u, 2u) == 0u && f.get(actor0 + 0xddu, 1u) == 0u);
  check(f.get(record1 + 0x20u, 2u) == 0xffffu &&
        f.get(actor1 + 0xddu, 1u) == 0u);
  check(f.get(actor2 + 0xddu, 1u) == 0u);
  bool saw_wrap = false, saw_clamp = false;
  for (std::size_t i = 0u; i != f.progress.access_events; ++i) {
    const auto &event = f.journal[i];
    if (event.pc == 0x80068688u && event.address == record0 + 0x20u &&
        event.value == 0xfffbu)
      saw_wrap = true;
    if (event.pc == 0x80068698u && event.address == record0 + 0x20u &&
        event.value == 0u)
      saw_clamp = true;
  }
  check(saw_wrap && saw_clamp);

  Fixture feature_zero;
  feature_zero.put(0x80021d93u, 1u, 0u);
  feature_zero.put(actor0 + 0xddu, 1u, 0u);
  check(feature_zero.run() == NBA97_TEXT_COMPLETE &&
        feature_zero.get(record0 + 0x20u, 2u) == 20u);
  Fixture flag_override;
  flag_override.put(0x80021d93u, 1u, 0u);
  flag_override.put(actor0 + 0xddu, 1u, 1u);
  check(flag_override.run() == NBA97_TEXT_COMPLETE &&
        flag_override.get(record0 + 0x20u, 2u) == 13u);

  Fixture triple_wrap;
  triple_wrap.put(actor0 + 0xddu, 1u, 1u);
  triple_wrap.put(actor0 + 0x44u, 2u, 0x8000u);
  bool saw_triple_wrap = false;
  check(triple_wrap.run() == NBA97_TEXT_COMPLETE &&
        triple_wrap.get(record0 + 0x20u, 2u) == 0u);
  for (std::size_t i = 0u; i != triple_wrap.progress.access_events; ++i)
    if (triple_wrap.journal[i].pc == 0x80068688u &&
        triple_wrap.journal[i].address == record0 + 0x20u &&
        triple_wrap.journal[i].value == 0x8013u)
      saw_triple_wrap = true;
  check(saw_triple_wrap);
}

void malformed_unknown_and_mapping() {
  Fixture malformed;
  malformed.known[malformed.offset(0x800fdb58u) + 3u] = 2u;
  const auto prior = malformed.context.machine.registers.gpr[2];
  check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x80068524u &&
        malformed.progress.machine.registers.gpr[2].word == 0x80100000u &&
        !same_word(malformed.progress.machine.registers.gpr[2], prior));

  Fixture pointer;
  pointer.mask(0x80020becu, 4u, 7u);
  check(pointer.run() == NBA97_TEXT_UNKNOWN &&
        pointer.progress.stopped_pc == 0x80068600u &&
        pointer.progress.stopped_address == 0x8003001cu);

  Fixture unaligned;
  unaligned.put(0x80020becu, 4u, 0x80030001u);
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80068600u);
  Fixture missing;
  missing.region.size = 0x100u;
  check(missing.run() == NBA97_TEXT_RESOURCE);
  Fixture huge;
  huge.region.size = std::numeric_limits<std::size_t>::max();
  check(huge.run() == NBA97_TEXT_ARGUMENT);
  Fixture overlap;
  Nba97GameTextRegion regions[2] = {overlap.region, overlap.region};
  overlap.context.memory = {regions, 2u};
  check(overlap.run() == NBA97_TEXT_ARGUMENT);

  Fixture unknown_store;
  unknown_store.put(0x800fdb7eu, 2u, 0xffffu);
  unknown_store.put(0x8001f80cu, 2u, 0u);
  unknown_store.mask(0x800fdb7eu, 2u, 2u);
  const auto split = unknown_store.offset(0x8001f80cu);
  Nba97GameTextRegion store_regions[3] = {
      {Fixture::base, unknown_store.data.data(), unknown_store.known.data(),
       split},
      {0x8001f80cu, unknown_store.data.data() + split, nullptr, 2u},
      {0x8001f80eu, unknown_store.data.data() + split + 2u,
       unknown_store.known.data() + split + 2u,
       unknown_store.data.size() - split - 2u},
  };
  unknown_store.context.memory = {store_regions, 3u};
  const auto before_score = unknown_store.get(0x8001f80cu, 2u);
  check(unknown_store.run() == NBA97_TEXT_ARGUMENT &&
        unknown_store.progress.stopped_pc == 0x800685c8u &&
        unknown_store.get(0x8001f80cu, 2u) == before_score &&
        unknown_store.get(0x800fdb98u, 2u) == 0xffffu);
}

void guest_wrap_and_partial_actor_predicate() {
  Fixture wrapped;
  std::vector<std::uint8_t> low(0x100u, 0u), high(0x100u, 0u);
  std::vector<std::uint8_t> low_known(0x100u, 1u), high_known(0x100u, 1u);
  Nba97GameTextRegion regions[3] = {
      wrapped.region,
      {0u, low.data(), low_known.data(), low.size()},
      {0xffffff00u, high.data(), high_known.data(), high.size()},
  };
  wrapped.context.memory = {regions, 3u};
  wrapped.put(0x80020becu, 4u, 0xffffff80u);
  high[0x9c] = 0xc0u;
  high[0x9d] = high[0x9e] = high[0x9f] = 0xffu;
  low[0x20] = 1u;
  low[0x5d] = 1u;
  high[0xc4] = 1u;
  high[0xe0] = 20u;
  check(wrapped.run() == NBA97_TEXT_COMPLETE && low[0x5d] == 0u &&
        high[0xe0] == 16u);

  Fixture partial;
  partial.mask(0x80021d93u, 1u, 0u);
  check(partial.run() == NBA97_TEXT_UNKNOWN &&
        partial.progress.stopped_pc == 0x80068630u &&
        partial.progress.machine.registers.gpr[3].known_mask == 0x0eu);
}

void live_native_aliases() {
  Fixture handicap_phase;
  const auto handicap_offset = handicap_phase.offset(0x800fdb98u);
  const auto phase_offset = handicap_phase.offset(0x800fe8ccu);
  Nba97GameTextRegion phase_regions[3] = {
      {Fixture::base, handicap_phase.data.data(), handicap_phase.known.data(),
       handicap_offset},
      {0x800fdb98u, handicap_phase.data.data() + phase_offset,
       handicap_phase.known.data() + phase_offset, 2u},
      {0x800fdb9au, handicap_phase.data.data() + handicap_offset + 2u,
       handicap_phase.known.data() + handicap_offset + 2u,
       handicap_phase.data.size() - handicap_offset - 2u},
  };
  handicap_phase.context.memory = {phase_regions, 3u};
  check(handicap_phase.run() == NBA97_TEXT_COMPLETE &&
        handicap_phase.progress.score_iterations == 0u &&
        handicap_phase.progress.machine.registers.gpr[2].word == UINT32_MAX);

  Fixture delta_alias;
  const auto score_offset = delta_alias.offset(0x8001f80cu);
  const auto delta_offset = delta_alias.offset(0x800fdb7eu);
  Nba97GameTextRegion delta_regions[3] = {
      {Fixture::base, delta_alias.data.data(), delta_alias.known.data(),
       score_offset},
      {0x8001f80cu, delta_alias.data.data() + delta_offset,
       delta_alias.known.data() + delta_offset, 2u},
      {0x8001f80eu, delta_alias.data.data() + score_offset + 2u,
       delta_alias.known.data() + score_offset + 2u,
       delta_alias.data.size() - score_offset - 2u},
  };
  delta_alias.context.memory = {delta_regions, 3u};
  check(delta_alias.run() == NBA97_TEXT_COMPLETE &&
        delta_alias.get(0x800fdb7eu, 2u) == 2u &&
        delta_alias.get(0x8001f82eu, 2u) == 103u);

  Fixture pointer_alias;
  const auto next_pointer = pointer_alias.offset(0x80020bf0u);
  Nba97GameTextRegion pointer_regions[2] = {
      pointer_alias.region,
      {0x81000000u, pointer_alias.data.data() + next_pointer - 0x20u,
       pointer_alias.known.data() + next_pointer - 0x20u, 0x100u},
  };
  pointer_alias.context.memory = {pointer_regions, 2u};
  pointer_alias.put(0x8003001cu, 4u, 0x81000000u);
  check(pointer_alias.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        pointer_alias.progress.stopped_pc == 0x80068600u &&
        pointer_alias.progress.stopped_address == 0x80030219u &&
        pointer_alias.get(0x80020bf0u, 4u) == 0x800301fdu);
}

void budgets_returns_and_repeat() {
  Fixture complete;
  check(complete.run() == NBA97_TEXT_COMPLETE);
  Fixture repeated;
  check(repeated.run() == NBA97_TEXT_COMPLETE &&
        complete.data == repeated.data && complete.known == repeated.known &&
        same_machine(complete.progress.machine, repeated.progress.machine));
  for (std::size_t budget = 0u; budget != complete.progress.operations;
       ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    check(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget &&
          f.progress.stopped_pc == complete.journal[budget].pc &&
          f.progress.stopped_address == complete.journal[budget].address);
  }
  for (unsigned mask = 0u; mask != 16u; ++mask) {
    Fixture f;
    f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask =
        static_cast<std::uint8_t>(mask);
    check(f.run() == (mask == 15u ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    check(same_word(f.progress.machine.hi, f.context.machine.hi) &&
          same_word(f.progress.machine.lo, f.context.machine.lo));
    if (mask != 15u)
      check(f.progress.stopped_pc == 0x800686b0u);
  }
  Fixture bad_ra;
  bad_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word |= 1u;
  check(bad_ra.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        bad_ra.progress.stopped_pc == 0x800686b0u);
}
} // namespace

int main() {
  normal_path_and_machine();
  handicap_gates_and_delays();
  phase_and_score_boundaries();
  actor_paths_and_store_before_clamp();
  malformed_unknown_and_mapping();
  guest_wrap_and_partial_actor_predicate();
  live_native_aliases();
  budgets_returns_and_repeat();
  std::printf("%u stamina handicap focused checks passed\n", checks);
  return 0;
}
