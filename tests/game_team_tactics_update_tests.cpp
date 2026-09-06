#include "recovered/game_team_tactics_update.h"

#include <array>
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
    std::fprintf(stderr, "team tactics focused check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;

bool same_word(const Nba97GameTeamTacticsWord &left,
               const Nba97GameTeamTacticsWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}
bool same_machine(const Nba97GameTeamTacticsMachine &left,
                  const Nba97GameTeamTacticsMachine &right) {
  for (unsigned index = 0u; index != NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
       ++index)
    if (!same_word(left.registers.gpr[index], right.registers.gpr[index]))
      return false;
  return same_word(left.hi, right.hi) && same_word(left.lo, right.lo);
}

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000u, 0u);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000u, 1u);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97GameTeamTacticsContext context{};
  Nba97GameTeamTacticsProgress progress{};
  std::array<Nba97GameTeamTacticsAccess, 256> journal{};
  Nba97GameTeamTacticsEvent event{};
  std::vector<Nba97GameTeamTacticsEvent> events;
  unsigned calls{};
  bool scripted{};
  std::uint32_t rng_default{1u};
  std::uint32_t rng_751d0{1u};
  std::uint32_t rng_751e4{0u};
  std::uint32_t zero_from_pc{};
  std::uint32_t result_74688{0x800306acu};
  std::uint32_t result_74714{0x800307a0u};
  std::vector<std::uint32_t> callback_a0;
  int callback_result{1};
  bool invalidate_zero{};
  bool mutate{};

  Fixture() {
    context.memory = {&region, 1u};
    context.operation_budget = std::numeric_limits<std::size_t>::max();
    context.machine = machine();
    context.io = callback;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    put(0x800fdb68u, 2u, 0u);
    put(0x800fdb58u, 4u, 7200u);
    put(0x800fdbccu, 2u, 0u);
    put(0x800fdbd2u, 2u, 0u);
    put(0x800fdb90u, 2u, 0u);
    put(0x800fdb94u, 2u, 0xffffu);
  }

  static Nba97GameTeamTacticsMachine machine() {
    Nba97GameTeamTacticsMachine value{};
    for (unsigned index = 0u; index != NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
         ++index)
      value.registers.gpr[index] = {
          0x41000000u + index * 0x01010101u,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    value.registers.gpr[0] = {0u, 0x0fu};
    value.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x801ff000u, 0x0fu};
    value.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x81234560u, 0x0fu};
    value.hi = {0x13579bdfu, 0x05u};
    value.lo = {0x2468ace0u, 0x0au};
    return value;
  }
  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }
  void put(std::uint32_t address, unsigned width, std::uint32_t value) {
    for (unsigned byte = 0u; byte != width; ++byte) {
      bytes[offset(address) + byte] =
          static_cast<std::uint8_t>(value >> (8u * byte));
      known[offset(address) + byte] = 1u;
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0u;
    for (unsigned byte = 0u; byte != width; ++byte)
      value |= static_cast<std::uint32_t>(bytes[offset(address) + byte])
               << (8u * byte);
    return value;
  }
  int run() { return nba97_game_team_tactics_update(&context, &progress); }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameTeamTacticsEvent *event,
                      Nba97GameTeamTacticsMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    ++fixture.calls;
    fixture.event = *event;
    fixture.events.push_back(*event);
    fixture.callback_a0.push_back(machine->registers.gpr[4].word);
    if (fixture.scripted) {
      if (event->entry == 0x800706e4u) {
        const auto address = machine->registers.gpr[6].word;
        fixture.put(address, 2u, 100u);
        machine->registers.gpr[2] = {100u << 8u, 0x0fu};
      } else if (event->entry == 0x8007066cu) {
        machine->registers.gpr[2] = {120u << 8u, 0x0fu};
      } else if (event->entry == 0x8002ab70u) {
        std::uint32_t value = fixture.rng_default;
        if (event->pc == 0x800751d0u)
          value = fixture.rng_751d0;
        if (event->pc == 0x800751e4u)
          value = fixture.rng_751e4;
        machine->registers.gpr[2] = {value, 0x0fu};
      } else {
        machine->registers.gpr[2] = {1u, 0x0fu};
      }
    }
    if (fixture.scripted && event->pc == 0x80075a68u)
      machine->registers.gpr[2] = {0x800304c4u, 0x0fu};
    if (fixture.scripted && event->pc == 0x80075c28u)
      machine->registers.gpr[2] = {fixture.result_74688, 0x0fu};
    if (fixture.scripted && event->pc == 0x80075c44u)
      machine->registers.gpr[2] = {fixture.result_74714, 0x0fu};
    if (fixture.zero_from_pc != 0u && event->pc >= fixture.zero_from_pc)
      machine->registers.gpr[2] = {0u, 0x0fu};
    if (fixture.mutate) {
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {0x87654321u, 0x09u};
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x80070000u, 0x0fu};
      machine->hi = {0xabcdef01u, 0x03u};
      machine->lo = {0x10203040u, 0x0cu};
    }
    if (fixture.invalidate_zero)
      machine->registers.gpr[0] = {1u, 0x0fu};
    return fixture.callback_result;
  }
};

void check_early_exit_and_gates() {
  Fixture fixture;
  const auto before = fixture.context.machine;
  check(fixture.run() == NBA97_TEXT_COMPLETE);
  check(fixture.progress.completed == 1u);
  check(fixture.progress.stopped_pc == 0u &&
        fixture.progress.stopped_address == 0u &&
        fixture.progress.stopped_entry == 0u);
  check(fixture.get(0x800fdbbau, 2u) == 0u);
  check(fixture.get(0x800fe8acu, 2u) == 0u);
  check(fixture.get(0x800fe8aau, 2u) == 0u);
  check(fixture.progress.frame_stack_pointer == 0x801fef68u);
  check(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
      0x801ff000u);
  check(same_word(fixture.progress.machine.registers.gpr[31],
                  before.registers.gpr[31]));
  for (unsigned index = 16u; index != 24u; ++index)
    check(same_word(fixture.progress.machine.registers.gpr[index],
                    before.registers.gpr[index]));
  check(same_word(fixture.progress.machine.registers.gpr[30],
                  before.registers.gpr[30]));
  check(same_word(fixture.progress.machine.hi, before.hi));
  check(same_word(fixture.progress.machine.lo, before.lo));

  Fixture exact;
  exact.put(0x800fdb68u, 2u, 3u);
  exact.put(0x800fdb58u, 4u, 7200u);
  check(exact.run() == NBA97_TEXT_COMPLETE);
  check(exact.get(0x800fdbbau, 2u) == 1u);

  Fixture late;
  late.put(0x800fdb68u, 2u, 3u);
  late.put(0x800fdb58u, 4u, 7201u);
  check(late.run() == NBA97_TEXT_COMPLETE);
  check(late.get(0x800fdbbau, 2u) == 0u);

  Fixture negatives;
  negatives.put(0x800fdbccu, 2u, 0xffffu);
  negatives.put(0x800fdbd2u, 2u, 0x8000u);
  check(negatives.run() == NBA97_TEXT_COMPLETE);
  check(negatives.get(0x800fe8aau, 2u) == 1u);
}

void check_unknown_and_atomic_failures() {
  Fixture unknown;
  unknown.known[unknown.offset(0x800fdb94u) + 1u] = 0u;
  check(unknown.run() == NBA97_TEXT_UNKNOWN);
  check(unknown.progress.stopped_pc == 0x8007487cu);
  check(unknown.progress.machine.registers.gpr[2].known_mask == 0x01u);
  check(unknown.get(0x800fe8acu, 2u) == 0u);

  Fixture malformed;
  malformed.known[malformed.offset(0x800fdb68u) + 1u] = 2u;
  check(malformed.run() == NBA97_TEXT_ARGUMENT);
  check(malformed.progress.stopped_pc == 0x800747b4u);
  check(malformed.progress.machine.registers.gpr[2].word == 0x80100000u);
  check(malformed.progress.machine.registers.gpr[2].known_mask == 0x0fu);

  Fixture store;
  store.region.known = nullptr;
  store.context.machine.registers.gpr[31].known_mask = 0x07u;
  const auto at = store.offset(0x801fef68u + 0x94u);
  std::array<std::uint8_t, 4> before{};
  for (unsigned byte = 0u; byte != 4u; ++byte)
    before[byte] = store.bytes[at + byte];
  check(store.run() == NBA97_TEXT_ARGUMENT);
  check(store.progress.stopped_pc == 0x800747bcu);
  for (unsigned byte = 0u; byte != 4u; ++byte)
    check(store.bytes[at + byte] == before[byte]);
}

void check_budget_prefixes() {
  Fixture reference;
  const auto initial = reference.bytes;
  check(reference.run() == NBA97_TEXT_COMPLETE);
  const auto operations = reference.progress.operations;
  check(operations > 20u);
  for (std::size_t budget = 0u; budget != operations; ++budget) {
    Fixture limited;
    limited.context.operation_budget = budget;
    check(limited.run() == NBA97_TEXT_LIMIT);
    check(limited.progress.operations == budget);
    auto expected = initial;
    for (std::size_t event = 0u; event != reference.progress.access_events;
         ++event) {
      const auto &access = reference.journal[event];
      if (access.operation > budget ||
          access.kind != NBA97_GAME_TEAM_TACTICS_STORE)
        continue;
      for (unsigned byte = 0u; byte != access.width; ++byte)
        expected[static_cast<std::size_t>(access.address - Ram) + byte] =
            static_cast<std::uint8_t>(access.value >> (8u * byte));
    }
    check(limited.bytes == expected);
  }
}

void prepare_first_callback(Fixture &fixture) {
  fixture.put(0x800fdb94u, 2u, 0u);
  fixture.put(0x8001edf8u, 4u, 0x80030000u);
  fixture.put(0x8001ee68u, 2u, 100u);
  fixture.put(0x800fdb6cu, 2u, 1u);
  fixture.put(0x800fe890u, 2u, 4u);
  fixture.put(0x800fe872u, 2u, 0u);
  fixture.put(0x800fdba4u, 4u, 0u);
}

void check_callback_boundary() {
  Fixture refused;
  prepare_first_callback(refused);
  refused.callback_result = 0;
  refused.mutate = true;
  check(refused.run() == NBA97_TEXT_IO_REFUSED);
  check(refused.calls == 1u);
  check(refused.event.pc == 0x800748ecu);
  check(refused.event.delay_slot_pc == 0x800748f0u);
  check(refused.event.entry == 0x8002ab70u);
  check(refused.event.kind == NBA97_GAME_TEAM_TACTICS_CHILD_8002AB70);
  check(refused.event.argument_count == 0u);
  check(refused.event.invocation == 1u);
  check(refused.progress.machine.registers.gpr[31].word == 0x800748f4u);
  check(refused.progress.machine.registers.gpr[16].word == 0x87654321u);
  check(refused.progress.machine.registers.gpr[29].word == 0x80070000u);
  check(refused.progress.machine.hi.word == 0xabcdef01u);
  check(refused.progress.machine.lo.word == 0x10203040u);

  Fixture invalid;
  prepare_first_callback(invalid);
  invalid.invalidate_zero = true;
  check(invalid.run() == NBA97_TEXT_ARGUMENT);
  check(invalid.progress.machine.registers.gpr[0].word == 1u);
  check(invalid.progress.callbacks_completed == 0u);

  Fixture uncertain;
  prepare_first_callback(uncertain);
  uncertain.known[uncertain.offset(0x800fdba4u) + 3u] = 0u;
  check(uncertain.run() == NBA97_TEXT_UNKNOWN);
  check(uncertain.progress.stopped_pc == 0x800748e4u);
  check(uncertain.calls == 0u);
}

void check_mapping_and_determinism() {
  Fixture overlap;
  Nba97GameTextRegion regions[2] = {overlap.region, overlap.region};
  overlap.context.memory = {regions, 2u};
  check(overlap.run() == NBA97_TEXT_ARGUMENT);

  Fixture first;
  Fixture second;
  check(first.run() == NBA97_TEXT_COMPLETE);
  check(second.run() == NBA97_TEXT_COMPLETE);
  check(first.bytes == second.bytes);
  check(first.known == second.known);
  check(same_machine(first.progress.machine, second.progress.machine));
}

void check_geometry_call_boundary() {
  Fixture fixture;
  fixture.put(0x800fdb94u, 2u, 0u);
  fixture.put(0x800fe872u, 2u, 1u);
  fixture.put(0x800fdb6cu, 2u, 1u);
  fixture.put(0x8001edf8u, 4u, 0x80040000u);
  fixture.put(0x8001ee04u, 4u, 0x00000400u);
  fixture.put(0x8001ee08u, 2u, 0u);
  fixture.put(0x800fdc34u, 4u, 0x80050000u);
  fixture.put(0x80050008u, 4u, 1000u);
  fixture.put(0x8005000cu, 4u, 2000u);
  fixture.put(0x80020becu, 4u, 0x80030000u);
  fixture.put(0x80030008u, 4u, 100u);
  fixture.put(0x8003000cu, 4u, 200u);
  fixture.callback_result = 0;
  check(fixture.run() == NBA97_TEXT_IO_REFUSED);
  check(fixture.calls == 1u);
  check(fixture.event.pc == 0x800749ccu);
  check(fixture.event.delay_slot_pc == 0x800749d0u);
  check(fixture.event.entry == 0x800706e4u);
  check(fixture.event.kind == NBA97_GAME_TEAM_TACTICS_CHILD_800706E4);
  check(fixture.event.argument_count == 3u);
  check(fixture.progress.machine.registers.gpr[4].word == 900u);
  check(fixture.progress.machine.registers.gpr[5].word == 1800u);
  check(fixture.progress.machine.registers.gpr[6].word == 0x800300c0u);
  check(fixture.progress.machine.registers.gpr[31].word == 0x800749d4u);
}

void check_return_masks_and_wrapped_stack() {
  for (unsigned mask = 0u; mask != 16u; ++mask) {
    Fixture fixture;
    fixture.context.machine.registers.gpr[31].known_mask =
        static_cast<std::uint8_t>(mask);
    const int result = fixture.run();
    check(result == (mask == 15u ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    if (mask != 15u) {
      check(fixture.progress.stopped_pc == 0x80075d38u);
      check(fixture.progress.machine.registers.gpr[29].word == 0x801ff000u);
      check(fixture.progress.machine.registers.gpr[31].known_mask == mask);
    }
  }
  Fixture misaligned;
  misaligned.context.machine.registers.gpr[31] = {0x81234562u, 0x0fu};
  check(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  check(misaligned.progress.stopped_pc == 0x80075d38u);

  Fixture wrapped;
  std::array<std::uint8_t, 0x100> low_bytes{};
  std::array<std::uint8_t, 0x100> low_known{};
  low_known.fill(1u);
  Nba97GameTextRegion regions[2] = {
      {0u, low_bytes.data(), low_known.data(), low_bytes.size()},
      wrapped.region};
  wrapped.context.memory = {regions, 2u};
  wrapped.context.machine.registers.gpr[29] = {0x60u, 0x0fu};
  const auto before = wrapped.context.machine;
  check(wrapped.run() == NBA97_TEXT_COMPLETE);
  check(wrapped.progress.frame_stack_pointer == 0xffffffc8u);
  check(wrapped.progress.machine.registers.gpr[29].word == 0x60u);
  for (unsigned index = 16u; index != 24u; ++index)
    check(same_word(wrapped.progress.machine.registers.gpr[index],
                    before.registers.gpr[index]));
}

void prepare_deep(Fixture &fixture) {
  fixture.scripted = true;
  fixture.put(0x800fdb68u, 2u, 2u);
  fixture.put(0x800fdb58u, 4u, 1000u);
  fixture.put(0x800fdbccu, 2u, 0xffffu);
  fixture.put(0x800fdbd2u, 2u, 0u);
  fixture.put(0x800fdb90u, 2u, 0u);
  fixture.put(0x800fdb94u, 2u, 0u);
  fixture.put(0x800fdb6cu, 2u, 1u);
  fixture.put(0x800fdba4u, 4u, 360u);
  fixture.put(0x800fdbc0u, 4u, 1000u);
  fixture.put(0x800fdbc4u, 4u, 2000u);
  fixture.put(0x800fdc34u, 4u, 0x80030000u);
  fixture.put(0x800fdc48u, 4u, 0x80030988u);
  fixture.put(0x800fe872u, 2u, 1u);
  fixture.put(0x800fe86eu, 2u, 0u);
  fixture.put(0x800fe894u, 2u, 0u);
  fixture.put(0x800fe866u, 2u, 0u);
  fixture.put(0x800fe868u, 2u, 0xffffu);
  fixture.put(0x800fe86au, 2u, 5u);
  fixture.put(0x800fe86cu, 2u, 0u);
  fixture.put(0x800fe870u, 2u, 1u);
  fixture.put(0x800fe8a8u, 2u, 0u);
  fixture.put(0x800fe8a4u, 2u, 10u);
  for (int index = -128; index != 256; ++index)
    fixture.put(static_cast<std::uint32_t>(0x80020becu + index * 4), 4u,
                0x80030000u +
                    static_cast<unsigned>((index % 10 + 10) % 10) * 0xf4u);
  for (unsigned team_index = 0u; team_index != 2u; ++team_index) {
    const auto team = team_index == 0u ? 0x8001edf4u : 0x8001eeb8u;
    fixture.put(team + 4u, 4u, team_index == 0u ? 0x8001eeb8u : 0x8001edf4u);
    fixture.put(team + 0x10u, 4u, 500u);
    fixture.put(team + 0x14u, 2u, team_index * 5u);
    fixture.put(team + 0x38u, 1u, 7u);
    for (unsigned slot = 0u; slot != 5u; ++slot)
      fixture.put(team + 0x5cu + slot, 1u, team_index * 5u + slot);
    fixture.put(team + 0x6cu, 4u, 0x80130000u + team_index * 0x100u);
    fixture.put(team + 0x72u, 2u, 20u);
    fixture.put(team + 0x74u, 2u, 100u);
    fixture.put(team + 0x78u, 1u, 1u);
    fixture.put(team + 0xa4u, 2u, 0u);
    fixture.put(team + 0xb8u, 1u, 0u);
    fixture.put(team + 0xbau, 1u, 0u);
    for (unsigned index = 0u; index != 5u; ++index)
      fixture.put(0x80120000u + team_index * 0x100u + index, 1u, index);
    for (unsigned index = 0u; index != 0x100u; ++index)
      fixture.put(0x80130000u + team_index * 0x100u + index, 1u, index % 5u);
  }
  for (unsigned index = 0u; index != 10u; ++index) {
    const auto actor = 0x80030000u + index * 0xf4u;
    fixture.put(actor, 4u, index);
    fixture.put(actor + 4u, 2u, 0xffffu);
    fixture.put(actor + 8u, 4u, 100u + index * 10u);
    fixture.put(actor + 0xcu, 4u, 200u + index * 10u);
    fixture.put(actor + 0x1au, 1u, 1u);
    fixture.put(actor + 0xa2u, 2u, 0u);
    fixture.put(actor + 0xccu, 2u, index);
    fixture.put(actor + 0xd4u, 2u, 0xffffu);
    fixture.put(actor + 0xd6u, 2u, 0xffffu);
    fixture.put(actor + 0xd8u, 1u, 0u);
    fixture.put(actor + 0xdau, 1u, 0u);
  }
  fixture.put(0x80030990u, 4u, 600u);
  fixture.put(0x80030994u, 4u, 0u);
  for (unsigned mode = 0u; mode != 32u; ++mode) {
    fixture.put(0x800bb68cu + mode * 4u, 2u, 0u);
    fixture.put(0x800bb68eu + mode * 4u, 2u, 7u);
    fixture.put(0x800bb7f8u + mode * 4u, 4u, 0x80140000u + mode * 0x100u);
    for (unsigned command = 0u; command != 32u; ++command) {
      const auto at = 0x80140000u + mode * 0x100u + command * 8u;
      fixture.put(at, 2u, 120u);
      fixture.put(at + 2u, 2u, 0xffffu);
      fixture.put(at + 4u, 2u, 0xffffu);
      fixture.put(at + 6u, 2u, 0xffffu);
    }
  }
}

void check_complete_geometry_and_defense_paths() {
  Fixture geometry;
  prepare_deep(geometry);
  check(geometry.run() == NBA97_TEXT_COMPLETE);
  check(geometry.progress.actor_iterations == 5u);
  check(geometry.progress.opposing_actor_iterations == 5u);
  check(geometry.events.size() >= 23u);
  for (unsigned index = 0u; index != 5u; ++index) {
    const auto actor = 0x80030000u + index * 0xf4u;
    check(geometry.get(actor + 0xbeu, 2u) == 100u);
    check(geometry.get(actor + 0xbau, 2u) == 100u);
  }
  check(geometry.get(0x800fdbe0u, 2u) == 100u);
  check(geometry.events[0].pc == 0x800749ccu);
  check(geometry.events[1].pc == 0x800749f0u);
  check(geometry.events[10].pc == 0x80074ae8u);

  Fixture forced;
  prepare_deep(forced);
  forced.put(0x800fe8a8u, 2u, 1u);
  check(forced.run() == NBA97_TEXT_COMPLETE);
  bool saw_757f0 = false, saw_75820 = false;
  for (const auto &event : forced.events) {
    saw_757f0 = saw_757f0 || event.pc == 0x800757f0u;
    saw_75820 = saw_75820 || event.pc == 0x80075820u;
  }
  check(saw_757f0 && saw_75820);
  Fixture normal;
  prepare_deep(normal);
  normal.put(0x800fe8a4u, 2u, 0u);
  normal.zero_from_pc = 0x800759b8u;
  check(normal.run() == NBA97_TEXT_COMPLETE);
  bool saw_normal = false, saw_zero_exit = false;
  for (const auto &event : normal.events) {
    saw_normal = saw_normal || event.pc == 0x800759b8u;
    saw_zero_exit = saw_zero_exit || event.pc == 0x80075ce0u;
  }
  check(saw_normal && saw_zero_exit);
  check(normal.get(0x800fe8a4u, 2u) == 19u);
}

void check_offense_multu_and_bounded_loops() {
  Fixture offense;
  prepare_deep(offense);
  offense.put(0x800fe86eu, 2u, 1u);
  offense.put(0x800fe8a8u, 2u, 0u);
  offense.rng_751d0 = 1u;
  check(offense.run() == NBA97_TEXT_COMPLETE);
  bool saw_75208 = false, saw_73134 = false;
  for (const auto &event : offense.events) {
    saw_75208 = saw_75208 || event.pc == 0x80075208u;
    saw_73134 = saw_73134 || event.entry == 0x80073134u;
  }
  check(saw_75208);
  check(saw_73134);
  check(offense.progress.machine.hi.known_mask == 0x0fu);
  check(offense.progress.machine.lo.known_mask == 0x0fu);

  Fixture rejection;
  prepare_deep(rejection);
  rejection.put(0x800fe86eu, 2u, 1u);
  rejection.rng_751d0 = 0u;
  rejection.rng_751e4 = 0x78u;
  rejection.context.operation_budget = 500u;
  check(rejection.run() == NBA97_TEXT_LIMIT);
  unsigned repeats = 0u;
  for (const auto &event : rejection.events)
    repeats += event.pc == 0x800751e4u;
  check(repeats > 1u);

  Fixture marker;
  prepare_deep(marker);
  marker.put(0x80140000u, 2u, 0x23bau);
  marker.context.operation_budget = 500u;
  check(marker.run() == NBA97_TEXT_LIMIT);
  check(marker.progress.stopped_pc == 0x80075570u ||
        marker.progress.stopped_pc == 0x80075580u ||
        marker.progress.stopped_pc == 0x800755a8u);
}

void prepare_late_defense(Fixture &fixture) {
  prepare_deep(fixture);
  fixture.put(0x800fdbccu, 2u, 0u);
  fixture.put(0x800fe8a8u, 2u, 0u);
  fixture.put(0x800fe8a4u, 2u, 0u);
  fixture.put(0x80021d72u, 1u, 2u);
  fixture.put(0x8003001cu, 4u, 0x80050000u);
  fixture.put(0x8005001eu, 1u, 0u);
  fixture.put(0x800300d4u, 2u, 0xffffu);
  const std::uint32_t list = 0x8001eeb8u + 0x5cu;
  fixture.put(list + 0u, 1u, 0xffu);
  fixture.put(list + 1u, 1u, 0u);
  fixture.put(list + 2u, 1u, 1u);
  fixture.put(list + 3u, 1u, 2u);
  fixture.put(list + 4u, 1u, 3u);
}

void check_late_defensive_pointer_results_and_signed_list() {
  Fixture direct;
  prepare_late_defense(direct);
  direct.put(0x800fe8b4u, 2u, 10u);
  check(direct.run() == NBA97_TEXT_COMPLETE);
  bool saw_74688 = false, saw_74714 = false;
  std::vector<std::uint32_t> late_actors;
  for (std::size_t index = 0u; index != direct.events.size(); ++index) {
    saw_74688 = saw_74688 || direct.events[index].pc == 0x80075c28u;
    saw_74714 = saw_74714 || direct.events[index].pc == 0x80075c44u;
    if (direct.events[index].pc == 0x80075ce0u)
      late_actors.push_back(direct.callback_a0[index]);
  }
  check(saw_74688 && !saw_74714);
  check(direct.get(0x800306acu + 0x1au, 1u) == 6u);
  check(direct.get(0x800fe8aeu, 2u) == 1u);
  const std::vector<std::uint32_t> expected_late{
      0x80030894u, 0x80030000u, 0x800300f4u, 0x800301e8u, 0x800302dcu};
  check(late_actors == expected_late);

  Fixture fallback;
  prepare_late_defense(fallback);
  fallback.put(0x800fe8b4u, 2u, 10u);
  fallback.result_74688 = 0u;
  check(fallback.run() == NBA97_TEXT_COMPLETE);
  bool fallback_call = false;
  for (const auto &event : fallback.events)
    fallback_call = fallback_call || event.pc == 0x80075c44u;
  check(fallback_call);
  check(fallback.get(0x800307a0u + 0x1au, 1u) == 6u);
  check(fallback.get(0x800fe8aeu, 2u) == 1u);
}

void check_late_defensive_timers() {
  Fixture positive;
  prepare_late_defense(positive);
  positive.put(0x800fe8b4u, 2u, 10u);
  check(positive.run() == NBA97_TEXT_COMPLETE);
  check(positive.get(0x800fe8b4u, 2u) == 9u);
  bool positive_rng = false;
  for (const auto &event : positive.events)
    positive_rng = positive_rng || event.pc == 0x80075c08u;
  check(!positive_rng);

  Fixture expired;
  prepare_late_defense(expired);
  expired.put(0x800fdb6cu, 2u, 2u);
  expired.put(0x800fe8b4u, 2u, 1u);
  expired.rng_default = 5u;
  check(expired.run() == NBA97_TEXT_COMPLETE);
  check(expired.get(0x800fe8b4u, 2u) == 0xffffu);
  check(expired.get(0x800fe8b6u, 2u) == 65u);

  Fixture secondary;
  prepare_late_defense(secondary);
  secondary.put(0x800fe8b4u, 2u, 0u);
  secondary.put(0x800fe8b6u, 2u, 10u);
  check(secondary.run() == NBA97_TEXT_COMPLETE);
  check(secondary.get(0x800fe8b6u, 2u) == 9u);

  Fixture start;
  prepare_late_defense(start);
  start.put(0x800fe8b4u, 2u, 0u);
  start.put(0x800fe8b6u, 2u, 0u);
  start.rng_default = 7u;
  check(start.run() == NBA97_TEXT_COMPLETE);
  check(start.get(0x800fe8b4u, 2u) == 37u);
}
} // namespace

int main() {
  check(nba97_game_team_tactics_update(nullptr, nullptr) ==
        NBA97_TEXT_ARGUMENT);
  check_early_exit_and_gates();
  check_unknown_and_atomic_failures();
  check_budget_prefixes();
  check_callback_boundary();
  check_geometry_call_boundary();
  check_return_masks_and_wrapped_stack();
  check_complete_geometry_and_defense_paths();
  check_offense_multu_and_bounded_loops();
  check_late_defensive_pointer_results_and_signed_list();
  check_late_defensive_timers();
  check_mapping_and_determinism();
  std::printf("team tactics focused checks: %u\n", checks);
  return 0;
}
