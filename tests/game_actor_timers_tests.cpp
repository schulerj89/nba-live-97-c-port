#include "recovered/game_actor_timers.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;

void check_impl(bool condition, unsigned line) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "actor timers check %u failed at line %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define check(condition) check_impl((condition), __LINE__)

bool same_word(const Nba97GameActorTimersWord &a,
               const Nba97GameActorTimersWord &b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

bool same_machine(const Nba97GameActorTimersMachine &a,
                  const Nba97GameActorTimersMachine &b) {
  for (unsigned i = 0; i != NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    if (!same_word(a.registers.gpr[i], b.registers.gpr[i]))
      return false;
  return same_word(a.hi, b.hi) && same_word(a.lo, b.lo);
}

struct Fixture {
  static constexpr std::uint32_t base = 0x80000000u;
  static constexpr std::size_t size = 0x110000u;
  static constexpr std::uint32_t entity_table = 0x80020becu;
  static constexpr std::uint32_t team_table = 0x800fdc70u;
  static constexpr std::uint32_t controller_table = 0x800fdc50u;
  std::vector<std::uint8_t> data;
  std::vector<std::uint8_t> known;
  std::vector<Nba97GameActorTimersAccess> journal;
  Nba97GameTextRegion region{};
  Nba97GameActorTimersContext context{};
  Nba97GameActorTimersProgress progress{};

  Fixture() : data(size, 0), known(size, 1), journal(512) {
    region = {base, data.data(), known.data(), data.size()};
    context.memory = {&region, 1};
    context.operation_budget = std::numeric_limits<std::size_t>::max();
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned i = 0; i != NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
      context.machine.registers.gpr[i] = {0x11000000u + i * 0x01010101u, 0x0fu};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0u, 0x0fu};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ff000u,
                                                                0x0fu};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80068e40u,
                                                                0x0fu};
    context.machine.hi = {0x12345678u, 0x0fu};
    context.machine.lo = {0x9abcdef0u, 0x0fu};

    put(0x800fdb6cu, 2, 1);
    put(0x800fdb58u, 4, 3600);
    put(0x800fdb74u, 2, 59);
    for (unsigned i = 0; i != 11; ++i) {
      const std::uint32_t actor = 0x80010000u + i * 0x200u;
      put(entity_table + i * 4u, 4, actor);
      put(actor + 0xe6u, 2, 5);
      put(actor + 0xe4u, 2, 1);
      put(actor + 0xb4u, 2, 2);
      put(actor + 0xd8u, 1, 0xa5);
      put(actor + 0xddu, 1, 0x5a);
      put(actor + 0xf2u, 2, 0xbeef);
      put(actor + 4u, 2, i < 10 ? 0 : 0xffffu);
    }
    for (unsigned i = 0; i != 10; ++i) {
      const std::uint32_t team = 0x80020000u + i * 0x40u;
      put(team_table + i * 4u, 4, team);
      put(team + 0x1au, 2, i);
      put(team + 0x1cu, 1, 0);
    }
    put(controller_table, 4, 0x80030000u);
    put(0x8003001eu, 2, 5);
    put(0x80030022u, 2, 0);
  }

  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - base);
  }
  void put(std::uint32_t address, unsigned width, std::uint32_t value) {
    for (unsigned i = 0; i != width; ++i)
      data[offset(address) + i] = static_cast<std::uint8_t>(value >> (i * 8));
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0;
    for (unsigned i = 0; i != width; ++i)
      value |= static_cast<std::uint32_t>(data[offset(address) + i]) << (i * 8);
    return value;
  }
  void mask(std::uint32_t address, unsigned width, std::uint8_t bits) {
    for (unsigned i = 0; i != width; ++i)
      known[offset(address) + i] = static_cast<std::uint8_t>((bits >> i) & 1u);
  }
  int run() { return nba97_game_actor_timers(&context, &progress); }
};

void normal_boundaries_and_order() {
  Fixture f;
  const auto before = f.context.machine;
  check(f.run() == NBA97_TEXT_COMPLETE);
  check(f.progress.completed && f.progress.entity_iterations == 11u &&
        f.progress.participation_iterations == 10u);
  check(f.progress.multiply_count == 11u &&
        f.progress.team_counter_updates == 10u &&
        f.progress.participation_updates == 1u);
  for (unsigned i = 0; i != 10; ++i) {
    const auto actor = 0x80010000u + i * 0x200u;
    check(f.get(actor + 0xe6u, 2) == 4u && f.get(actor + 0xe4u, 2) == 0u &&
          f.get(actor + 0xb4u, 2) == 1u);
    check(f.get(actor + 0xd8u, 1) == 0u && f.get(actor + 0xf2u, 2) == 0u &&
          f.get(actor + 0xddu, 1) == 1u);
    const auto team = 0x80020000u + i * 0x40u;
    check(f.get(team + 0x1au, 2) == i + 1u && f.get(team + 0x1cu, 1) == 1u);
  }
  const auto last = 0x80010000u + 10u * 0x200u;
  check(f.get(last + 0xe6u, 2) == 5u && f.get(last + 0xe4u, 2) == 1u &&
        f.get(last + 0xb4u, 2) == 1u);
  check(f.get(last + 0xd8u, 1) == 0xa5u && f.get(last + 0xf2u, 2) == 0xbeefu &&
        f.get(last + 0xddu, 1) == 0x5au);
  check(f.get(0x800fdb74u, 2) == 60u && f.get(0x80030022u, 2) == 1u &&
        f.get(0x8003001eu, 2) == 6u);
  check(f.progress.clock_quotient_60.word == 60u &&
        f.progress.clock_quotient_60.known_mask == 0x0fu &&
        f.progress.last_clock_quotient_3600.word == 1u);
  check(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            before.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word &&
        same_word(f.progress.return_address,
                  before.registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
  for (unsigned i = 0u; i != NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    if (i == NBA97_MATCH_INITIALIZE_ZERO || i >= 12u)
      check(same_word(f.progress.machine.registers.gpr[i],
                      before.registers.gpr[i]));
  check(f.progress.access_events == f.progress.accesses &&
        f.journal[0].pc == 0x80068328u &&
        f.journal[0].address == Fixture::entity_table &&
        f.journal[0].kind == NBA97_GAME_ACTOR_TIMERS_READ &&
        f.journal[1].pc == 0x80068338u && f.journal[1].width == 2u &&
        f.journal[2].pc == 0x8006833cu && f.journal[2].width == 1u);
}

void timer_wrap_and_zero_quirks() {
  Fixture f;
  const auto actor = 0x80010000u;
  f.put(0x800fdb6cu, 2, 1);
  f.put(actor + 0xe6u, 2, 0xffffu);
  f.put(actor + 0xe4u, 2, 1u);
  f.put(actor + 0xb4u, 2, 0x8000u);
  f.put(actor + 0xddu, 1, 0x66u);
  check(f.run() == NBA97_TEXT_COMPLETE);
  check(f.get(actor + 0xe6u, 2) == 0u);
  check(f.get(actor + 0xe4u, 2) == 0u && f.get(actor + 0xddu, 1) == 1u);
  check(f.get(actor + 0xb4u, 2) == 0x7fffu);
  bool saw_wrap = false, saw_clamp = false;
  for (std::size_t i = 0; i != f.progress.access_events; ++i) {
    const auto &event = f.journal[i];
    if (event.pc == 0x80068358u && event.value == 0xfffeu)
      saw_wrap = true;
    if (event.pc == 0x80068368u && event.value == 0u)
      saw_clamp = true;
  }
  check(saw_wrap && saw_clamp);

  Fixture zero;
  zero.put(actor + 0xe4u, 2, 0u);
  zero.put(actor + 0xddu, 1, 0x77u);
  zero.put(0x800fdb6cu, 2, 0xffffu);
  check(zero.run() == NBA97_TEXT_COMPLETE);
  check(zero.get(actor + 0xe4u, 2) == 0u &&
        zero.get(actor + 0xddu, 1) == 0x77u);

  Fixture no_delta;
  no_delta.put(0x800fdb6cu, 2, 0u);
  check(no_delta.run() == NBA97_TEXT_COMPLETE &&
        no_delta.get(actor + 0xe6u, 2) == 5u &&
        no_delta.get(actor + 0xe4u, 2) == 1u &&
        no_delta.get(actor + 0xb4u, 2) == 2u);
}

std::int64_t signed_word(std::uint32_t word) {
  return word < 0x80000000u ? word
                            : static_cast<std::int64_t>(word) - 0x100000000ll;
}

void quotient_and_duplicate_controller_cases() {
  const std::uint32_t values[] = {0u,    59u,         60u,        3599u,
                                  3600u, 0x80000000u, 0x7fffffffu};
  for (auto value : values) {
    Fixture f;
    f.put(0x800fdb58u, 4, value);
    f.put(0x800fdb74u, 2, static_cast<std::uint32_t>(signed_word(value) / 60));
    for (unsigned i = 1; i != 10; ++i)
      f.put(0x80010000u + i * 0x200u + 4u, 2, 0xffffu);
    f.put(0x80030022u, 2,
          static_cast<std::uint32_t>(signed_word(value) / 3600));
    check(f.run() == NBA97_TEXT_COMPLETE);
    check(f.progress.clock_quotient_60.word ==
              static_cast<std::uint32_t>(signed_word(value) / 60) &&
          f.progress.last_clock_quotient_3600.word ==
              static_cast<std::uint32_t>(signed_word(value) / 3600));
    check(f.progress.multiply[0].lo.word ==
          static_cast<std::uint32_t>(signed_word(value) *
                                     signed_word(0x88888889u)));
  }

  Fixture positive;
  positive.put(0x80030022u, 2, 0u);
  check(positive.run() == NBA97_TEXT_COMPLETE);
  check(positive.progress.participation_updates == 1u);

  Fixture negative;
  negative.put(0x800fdb58u, 4, 0xfffff1f0u); // -3600
  negative.put(0x800fdb74u, 2, 0xffc4u);     // -60
  negative.put(0x80030022u, 2, 0xffffu);
  check(negative.run() == NBA97_TEXT_COMPLETE);
  check(negative.progress.participation_updates == 10u &&
        negative.get(0x8003001eu, 2) == 15u);

  Fixture zero;
  zero.put(0x800fdb58u, 4, 0u);
  zero.put(0x800fdb74u, 2, 7u);
  check(zero.run() == NBA97_TEXT_COMPLETE &&
        zero.progress.team_counter_updates == 0u &&
        zero.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A3].word ==
            10u);
}

void counter_wrap_and_index_delay() {
  Fixture f;
  for (unsigned i = 0; i != 10; ++i)
    f.put(0x80020000u + i * 0x40u + 0x1au, 2, 0xffffu);
  f.put(0x80010000u + 4u, 2, 0xffffu);
  check(f.run() == NBA97_TEXT_COMPLETE);
  for (unsigned i = 0; i != 10; ++i)
    check(f.get(0x80020000u + i * 0x40u + 0x1au, 2) == 0u);
  check(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 0u);

  Fixture unknown;
  unknown.mask(0x80010004u, 2, 1u);
  check(unknown.run() == NBA97_TEXT_UNKNOWN);
  check(
      unknown.progress.stopped_pc == 0x80068490u &&
      unknown.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
          (unknown.get(0x80010004u, 2) << 2));
}

void unknown_prefixes_and_atomic_accesses() {
  Fixture f;
  const auto actor = 0x80010000u;
  f.mask(actor + 0xe6u, 2, 2u);
  check(f.run() == NBA97_TEXT_UNKNOWN);
  check(f.progress.stopped_pc == 0x80068344u && f.get(actor + 0xd8u, 1) == 0u &&
        f.get(actor + 0xf2u, 2) == 0u && f.get(actor + 0xe6u, 2) == 5u);

  Fixture malformed;
  malformed.known[malformed.offset(actor + 0xe6u) + 1u] = 2u;
  check(malformed.run() == NBA97_TEXT_ARGUMENT);
  check(malformed.progress.stopped_pc == 0x80068338u &&
        malformed.progress.machine.registers.gpr[2].word == 1u &&
        malformed.progress.machine.registers.gpr[2].known_mask == 0x0fu);

  Fixture pointer;
  pointer.mask(Fixture::entity_table, 4, 0x07u);
  check(pointer.run() == NBA97_TEXT_UNKNOWN &&
        pointer.progress.stopped_pc == 0x80068338u &&
        pointer.progress.stopped_address == 0x800100e6u);

  Fixture unknown_store;
  std::vector<std::uint8_t> target(0x200, 0);
  target[0xe6] = 5;
  target[0xe4] = 0;
  target[0xb4] = 0;
  target[0xd8] = 0xa5;
  Nba97GameTextRegion store_regions[2] = {
      unknown_store.region,
      {0x81000000u, target.data(), nullptr, target.size()},
  };
  unknown_store.context.memory = {store_regions, 2};
  unknown_store.put(Fixture::entity_table, 4, 0x81000000u);
  unknown_store.mask(0x800fdb6cu, 2, 1u);
  const auto before = target;
  check(unknown_store.run() == NBA97_TEXT_ARGUMENT);
  check(unknown_store.progress.stopped_pc == 0x80068358u);
  check(target[0xd8] == 0u && target[0xf2] == 0u && target[0xf3] == 0u &&
        target[0xe6] == before[0xe6] && target[0xe7] == before[0xe7]);
}

void budgets_and_determinism() {
  Fixture complete;
  check(complete.run() == NBA97_TEXT_COMPLETE);
  Fixture repeated;
  check(repeated.run() == NBA97_TEXT_COMPLETE &&
        complete.data == repeated.data && complete.known == repeated.known &&
        same_machine(complete.progress.machine, repeated.progress.machine));
  const auto total = complete.progress.operations;
  check(total != 0u && total == complete.progress.accesses);
  for (std::size_t budget = 0; budget != total; ++budget) {
    Fixture a;
    Fixture b;
    a.context.operation_budget = budget;
    b.context.operation_budget = budget;
    check(a.run() == NBA97_TEXT_LIMIT && b.run() == NBA97_TEXT_LIMIT);
    check(a.data == b.data && a.known == b.known &&
          same_machine(a.progress.machine, b.progress.machine));
    check(a.progress.operations == budget &&
          a.progress.stopped_pc == complete.journal[budget].pc &&
          a.progress.stopped_address == complete.journal[budget].address);
  }
}

void live_aliases_and_partial_clock() {
  Fixture pointer_alias;
  std::vector<std::uint8_t> &backing = pointer_alias.data;
  const auto table_offset = pointer_alias.offset(Fixture::team_table);
  Nba97GameTextRegion aliased_regions[2] = {
      pointer_alias.region,
      {0x81000000u, backing.data() + table_offset - 0x1au,
       pointer_alias.known.data() + table_offset - 0x1au, 0x100u},
  };
  pointer_alias.context.memory = {aliased_regions, 2};
  pointer_alias.put(Fixture::team_table, 4, 0x81000000u);
  check(pointer_alias.run() == NBA97_TEXT_COMPLETE);
  bool saw_first_pointer = false;
  bool saw_reread_pointer = false;
  bool saw_mutated_target = false;
  for (std::size_t i = 0; i != pointer_alias.progress.access_events; ++i) {
    const auto &event = pointer_alias.journal[i];
    if (event.pc == 0x80068434u && event.address == Fixture::team_table &&
        event.value == 0x81000000u)
      saw_first_pointer = true;
    if (event.pc == 0x8006844cu && event.address == Fixture::team_table &&
        event.value == 0x81000001u)
      saw_reread_pointer = true;
    if (event.pc == 0x80068454u && event.address == 0x8100001du)
      saw_mutated_target = true;
  }
  check(saw_first_pointer && saw_reread_pointer && saw_mutated_target);

  Fixture clock_alias;
  const auto clock_offset = clock_alias.offset(0x800fdb58u);
  Nba97GameTextRegion clock_regions[2] = {
      clock_alias.region,
      {0x82000000u, clock_alias.data.data() + clock_offset - 0x22u,
       clock_alias.known.data() + clock_offset - 0x22u, 0x100u},
  };
  clock_alias.context.memory = {clock_regions, 2};
  clock_alias.put(Fixture::controller_table, 4, 0x82000000u);
  clock_alias.put(0x800fdb54u, 2, 5u);
  check(clock_alias.run() == NBA97_TEXT_COMPLETE);
  check(clock_alias.progress.participation_updates == 2u &&
        clock_alias.progress.multiply[1].multiplicand.word == 3600u &&
        clock_alias.progress.multiply[2].multiplicand.word == 1u &&
        clock_alias.progress.multiply[3].multiplicand.word == 0u);

  for (unsigned mask = 0u; mask != 15u; ++mask) {
    Fixture partial;
    partial.mask(0x800fdb58u, 4, static_cast<std::uint8_t>(mask));
    const int result = partial.run();
    check(result == NBA97_TEXT_UNKNOWN || result == NBA97_TEXT_ARGUMENT);
    check(partial.progress.multiply_count >= 1u &&
          partial.progress.multiply[0].multiplicand.known_mask == mask &&
          partial.progress.machine.hi.known_mask <= 0x0fu &&
          partial.progress.machine.lo.known_mask <= 0x0fu);
  }

  Fixture high_index;
  high_index.put(0x80010004u, 2, 0x7fffu);
  check(high_index.run() == NBA97_TEXT_RESOURCE &&
        high_index.progress.stopped_pc == 0x800684a8u &&
        high_index.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 0x8011dc4cu);
}

void return_masks_and_mapping_guards() {
  for (unsigned mask = 0; mask != 16; ++mask) {
    Fixture f;
    f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask =
        static_cast<std::uint8_t>(mask);
    const int result = f.run();
    check(result == (mask == 15u ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    check(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
          0x800ff000u);
    if (mask != 15u)
      check(f.progress.stopped_pc == 0x800684fcu);
  }
  Fixture misaligned;
  misaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word |=
      1u;
  check(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.stopped_pc == 0x800684fcu &&
        misaligned.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == 0x800ff000u);

  Fixture unaligned;
  unaligned.put(Fixture::entity_table, 4, 0x80010001u);
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80068338u);
  Fixture missing;
  missing.region.size = 0x100u;
  check(missing.run() == NBA97_TEXT_RESOURCE);
  Fixture invalid;
  invalid.region.size = std::numeric_limits<std::size_t>::max();
  check(invalid.run() == NBA97_TEXT_ARGUMENT);
  Fixture overlap;
  Nba97GameTextRegion regions[2] = {overlap.region, overlap.region};
  overlap.context.memory = {regions, 2};
  check(overlap.run() == NBA97_TEXT_ARGUMENT);
}

void mapped_wrap_and_native_alias() {
  Fixture f;
  std::vector<std::uint8_t> low(0x100, 0), high(0x100, 0);
  std::vector<std::uint8_t> low_known(0x100, 1), high_known(0x100, 1);
  Nba97GameTextRegion regions[3] = {
      f.region,
      {0u, low.data(), low_known.data(), low.size()},
      {0xffffff00u, high.data(), high_known.data(), high.size()},
  };
  f.context.memory = {regions, 3};
  f.put(Fixture::entity_table, 4, 0xffffff80u);
  low[0x66] = 1u;
  low[0x64] = 0u;
  low[0x34] = 1u;
  low[0x58] = 0xa5u;
  f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {8u, 0x0fu};
  check(f.run() == NBA97_TEXT_COMPLETE &&
        f.progress.frame_stack_pointer == 0xfffffff0u &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            8u &&
        low[0x66] == 0u && low[0x34] == 0u && low[0x58] == 0u);

  Fixture alias;
  std::vector<std::uint8_t> common(0x100, 0);
  std::vector<std::uint8_t> common_known(0x100, 1);
  Nba97GameTextRegion aliases[3] = {
      alias.region,
      {0x81000000u, common.data(), common_known.data(), common.size()},
      {0x82000000u, common.data(), common_known.data(), common.size()},
  };
  alias.context.memory = {aliases, 3};
  check(alias.run() == NBA97_TEXT_COMPLETE);
}
} // namespace

int main() {
  normal_boundaries_and_order();
  timer_wrap_and_zero_quirks();
  quotient_and_duplicate_controller_cases();
  counter_wrap_and_index_delay();
  unknown_prefixes_and_atomic_accesses();
  budgets_and_determinism();
  live_aliases_and_partial_clock();
  return_masks_and_mapping_guards();
  mapped_wrap_and_native_alias();
  std::printf("%u actor timers focused checks passed\n", checks);
  return 0;
}
