#include "recovered/game_team_header_initialize.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
using U32 = std::uint32_t;
unsigned checks;
void check(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "team header initialize check %u line %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define CHECK(value) check((value), __LINE__)

struct Fixture {
  static constexpr U32 Base = 0x80000000u;
  static constexpr std::size_t Size = 0x200000u;
  static constexpr U32 Home = 0x80030000u;
  static constexpr U32 Away = 0x80030100u;
  static constexpr U32 Metadata = 0x80040000u;
  static constexpr U32 Stack = 0x801ff000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameTeamHeaderInitializeAccess, 512> journal{};
  Nba97GameTeamHeaderInitializeContext context{};
  Nba97GameTeamHeaderInitializeProgress progress{};

  Fixture(U32 side = 0, U32 opponent = 5, unsigned count = 3,
          unsigned injury = 1, unsigned difficulty = 2, unsigned rank54 = 4,
          unsigned rank57 = 11) {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x11000000u + i, 15};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {Home, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {Away, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x81234568u,
                                                                15};
    context.machine.hi = {0x13579bdfu, 5};
    context.machine.lo = {0x2468ace0u, 10};
    context.memory = {&region, 1};
    context.operation_budget = 1000;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    put(Home, 3, 2);
    put(Home + 0x14, side, 2);
    put(Away + 0x14, opponent, 2);
    for (unsigned i = 0; i < 5; ++i)
      put(Home + 0x16 + 2 * i, 0x1200u + i, 2);
    put(0x80020b0cu + 3 * 4, Metadata);
    put(0x80023aecu + 3 * 104, count, 1);
    put(0x80021ed5u, injury, 1);
    put(0x80021ed6u, injury, 1);
    put(0x80021d72u, difficulty, 1);
    put(Metadata + 0x54, rank54, 1);
    put(Metadata + 0x57, rank57, 1);
    for (unsigned i = 0; i < 32; ++i)
      put(0x80020becu + 4 * i, 0x90000000u + i);
  }

  void put(U32 address, U32 value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i) {
      bytes[address - Base + i] = static_cast<std::uint8_t>(value >> (8 * i));
      known[address - Base + i] = 1;
    }
  }
  U32 get(U32 address, unsigned width = 4) const {
    U32 value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= U32(bytes[address - Base + i]) << (8 * i);
    return value;
  }
  int run() { return nba97_game_team_header_initialize(&context, &progress); }
};

void verifyActorLinks(const Fixture &fixture, U32 side, U32 opponent) {
  for (unsigned local = 0; local < 5; ++local) {
    U32 slot = side + local;
    U32 actor = 0x800fdcecu + slot * 244u;
    CHECK(fixture.get(0x80020becu + slot * 4) == actor);
    CHECK(fixture.get(actor + 0xd6, 2) ==
          static_cast<std::uint16_t>(opponent + local));
    CHECK(fixture.get(Fixture::Home + 0x98 + local * 2, 2) == 0x1200u + local);
  }
}

void side_paths_counts_and_thresholds() {
  for (unsigned count : {0u, 1u, 11u, 12u, 255u}) {
    Fixture fixture(0, 5, count, 1, 2, 4, 11);
    auto entry_machine = fixture.context.machine;
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    unsigned active = count < 12 ? count : 12;
    CHECK(fixture.progress.completed);
    CHECK(fixture.progress.stopped_pc == 0 &&
          fixture.progress.stopped_address == 0);
    CHECK(fixture.progress.status_iterations == active);
    CHECK(fixture.progress.unused_iterations == 12 - active);
    CHECK(fixture.progress.actor_iterations == 5);
    CHECK(fixture.get(Fixture::Home + 0x7c) == 0x80020b8cu);
    CHECK(fixture.get(Fixture::Home + 0x6c) == Fixture::Metadata);
    CHECK(fixture.get(Fixture::Home + 0x66, 2) == active);
    CHECK(fixture.get(Fixture::Home + 0x68, 2) == active);
    for (unsigned i = 0; i < 12; ++i)
      CHECK(fixture.get(0x8001f7ecu + 0x20 + 0x22 * i, 2) ==
            (i < active && i != 1 ? 0x7fffu : 0xfffeu));
    CHECK(fixture.get(Fixture::Home + 4) == Fixture::Away);
    CHECK(fixture.get(Fixture::Home + 8) == 0x800fdcecu);
    CHECK(fixture.get(Fixture::Home + 0xc) == 0x9000000cu);
    CHECK(fixture.get(Fixture::Home + 0x10) == 0xfffeb200u);
    CHECK(fixture.get(Fixture::Home + 0x34, 1) == 7);
    CHECK(fixture.get(Fixture::Home + 0x38, 1) == 7);
    CHECK(fixture.get(Fixture::Home + 0x39, 1) == 5);
    CHECK(fixture.get(Fixture::Home + 0x72, 2) == 19);
    CHECK(fixture.get(Fixture::Home + 0x62, 2) == 98);
    CHECK(fixture.get(Fixture::Home + 0x74, 2) == 1132);
    CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
              .word == Fixture::Stack);
    CHECK(fixture.progress.machine.hi.word == 0x13579bdfu &&
          fixture.progress.machine.hi.known_mask == 5);
    CHECK(fixture.progress.machine.lo.word == 0x2468ace0u &&
          fixture.progress.machine.lo.known_mask == 10);
    for (unsigned reg = 0; reg < 32; ++reg) {
      bool source_mutates = (reg >= NBA97_MATCH_INITIALIZE_AT &&
                             reg <= NBA97_MATCH_INITIALIZE_T0 + 5) ||
                            reg == NBA97_MATCH_INITIALIZE_SP;
      if (!source_mutates) {
        CHECK(fixture.progress.machine.registers.gpr[reg].word ==
              entry_machine.registers.gpr[reg].word);
        CHECK(fixture.progress.machine.registers.gpr[reg].known_mask ==
              entry_machine.registers.gpr[reg].known_mask);
      }
      CHECK(fixture.progress.machine.registers.gpr[reg].known_mask <= 15);
    }
    verifyActorLinks(fixture, 0, 5);
  }

  Fixture nonzero(7, 9, 3, 255, 0, 255, 255);
  CHECK(nonzero.run() == NBA97_TEXT_COMPLETE);
  CHECK(nonzero.get(Fixture::Home + 0x7c) == 0x80020bbcu);
  CHECK(nonzero.get(Fixture::Home + 8) == 0x9000000cu);
  CHECK(nonzero.get(Fixture::Home + 0xc) == 0x90000018u);
  CHECK(nonzero.get(Fixture::Home + 0x10) == 0x00014e00u);
  CHECK(nonzero.get(Fixture::Home + 0x72, 2) == 283u);
  CHECK(nonzero.get(Fixture::Home + 0x62, 2) ==
        static_cast<std::uint16_t>(120u - 510u));
  CHECK(nonzero.get(Fixture::Home + 0x74, 2) ==
        static_cast<std::uint16_t>(1260u - 8160u));
  for (unsigned i = 0; i < 12; ++i)
    CHECK(nonzero.get(0x8001f984u + 0x20 + 0x22 * i, 2) ==
          (i < 3 ? 0x7fffu : 0xfffeu));
  verifyActorLinks(nonzero, 7, 9);
}

void exact_access_order_and_budgets() {
  Fixture baseline;
  auto initial_bytes = baseline.bytes;
  auto initial_known = baseline.known;
  CHECK(baseline.run() == NBA97_TEXT_COMPLETE);
  CHECK(baseline.progress.operations == baseline.progress.access_events);
  CHECK(baseline.journal[0].pc == 0x800655b4u);
  CHECK(baseline.journal[3].pc == 0x80065608u);
  CHECK(baseline.journal[4].pc == 0x8006560cu);
  CHECK(baseline.journal[3].address == Fixture::Home &&
        baseline.journal[4].address == Fixture::Home);
  CHECK(baseline.journal[5].pc == 0x8006561cu);
  CHECK(baseline.journal[6].pc == 0x80065634u);
  CHECK(baseline.journal[7].pc == 0x80065640u);
  for (std::size_t budget = 0; budget < baseline.progress.operations;
       ++budget) {
    Fixture limited;
    limited.context.operation_budget = budget;
    CHECK(limited.run() == NBA97_TEXT_LIMIT);
    CHECK(limited.progress.operations == budget);
    CHECK(limited.progress.access_events == budget);
    auto expected_bytes = initial_bytes;
    auto expected_known = initial_known;
    for (std::size_t i = 0; i < budget; ++i) {
      const auto &event = baseline.journal[i];
      if (event.kind != NBA97_GAME_TEAM_HEADER_INITIALIZE_STORE)
        continue;
      for (unsigned byte = 0; byte < event.width; ++byte) {
        std::size_t offset = event.address - Fixture::Base + byte;
        expected_bytes[offset] =
            static_cast<std::uint8_t>(event.value >> (8 * byte));
        expected_known[offset] = (event.known_mask >> byte) & 1u;
      }
    }
    CHECK(limited.bytes == expected_bytes);
    CHECK(limited.known == expected_known);
  }
}

void unknowns_atomicity_and_no_known_store() {
  Fixture address;
  address.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask =
      14;
  CHECK(address.run() == NBA97_TEXT_UNKNOWN);
  CHECK(address.progress.stopped_pc == 0x800655b4u);
  CHECK(address.progress.operations == 0);

  Fixture side;
  side.known[Fixture::Home - Fixture::Base + 0x14] = 0;
  CHECK(side.run() == NBA97_TEXT_UNKNOWN);
  CHECK(side.progress.stopped_pc == 0x800655bcu);
  CHECK(side.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        Fixture::Stack - 0x10);

  Fixture count;
  count.known[0x80023aecu + 3 * 104 - Fixture::Base] = 0;
  CHECK(count.run() == NBA97_TEXT_UNKNOWN);
  CHECK(count.progress.stopped_pc == 0x8006564cu);
  CHECK(count.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V1].word ==
        12);

  Fixture injury;
  injury.known[0x80021ed5u - Fixture::Base] = 0;
  CHECK(injury.run() == NBA97_TEXT_UNKNOWN);
  CHECK(injury.progress.stopped_pc == 0x80065678u);

  Fixture malformed;
  auto before_t3 =
      malformed.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 3];
  malformed.known[0x80020b0cu + 3 * 4 - Fixture::Base + 3] = 2;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT);
  CHECK(malformed.progress.stopped_pc == 0x8006561cu);
  CHECK(malformed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 3]
                .word == before_t3.word &&
        malformed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 3]
                .known_mask == before_t3.known_mask);

  std::vector<std::uint8_t> globals_data(0x30000, 0);
  std::vector<std::uint8_t> globals_known(0x30000, 1);
  std::vector<std::uint8_t> header_data(0x1000, 0);
  Nba97GameTextRegion regions[2] = {
      {0x80000000u, globals_data.data(), globals_known.data(),
       globals_data.size()},
      {Fixture::Home, header_data.data(), nullptr, header_data.size()}};
  auto put_global = [&](U32 at, U32 value, unsigned width) {
    for (unsigned i = 0; i < width; ++i)
      globals_data[at - 0x80000000u + i] =
          static_cast<std::uint8_t>(value >> (8 * i));
  };
  put_global(0x80020b0cu + 3 * 4, Fixture::Metadata, 4);
  globals_known[0x80020b0cu + 3 * 4 - 0x80000000u] = 0;
  header_data[0] = 3;
  Nba97GameTeamHeaderInitializeContext no_known{};
  for (unsigned i = 0; i < 32; ++i)
    no_known.machine.registers.gpr[i] = {0x33000000u + i, 15};
  no_known.machine.registers.gpr[0] = {0, 15};
  no_known.machine.registers.gpr[4] = {Fixture::Home, 15};
  no_known.machine.registers.gpr[5] = {Fixture::Home + 0x100, 15};
  no_known.machine.registers.gpr[29] = {Fixture::Stack, 15};
  no_known.machine.registers.gpr[31] = {0x81234568u, 15};
  no_known.memory = {regions, 2};
  no_known.operation_budget = 100;
  Nba97GameTeamHeaderInitializeProgress no_known_progress{};
  CHECK(nba97_game_team_header_initialize(&no_known, &no_known_progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(no_known_progress.stopped_pc == 0x80065634u);
  for (unsigned i = 0; i < 4; ++i)
    CHECK(header_data[0x6c + i] == 0);
}

void aliases_alignment_wrap_and_return() {
  Fixture alias;
  /* header+0x66 aliases the second status halfword at status+0x42. */
  U32 aliased_header = 0x8001f7c8u;
  alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {
      aliased_header, 15};
  alias.put(aliased_header, 3, 2);
  alias.put(aliased_header + 0x14, 0, 2);
  for (unsigned i = 0; i < 5; ++i)
    alias.put(aliased_header + 0x16 + i * 2, 0x2200u + i, 2);
  alias.context.operation_budget = 40;
  CHECK(alias.run() == NBA97_TEXT_LIMIT);
  CHECK(alias.progress.status_iterations > 3);
  CHECK(alias.get(aliased_header + 0x66, 2) == 0xfffeu);

  Fixture actor_alias;
  U32 table_header = 0x80020b80u;
  actor_alias.context.machine.registers.gpr[4] = {table_header, 15};
  actor_alias.put(table_header, 3, 2);
  actor_alias.put(table_header + 0x14, 0, 2);
  for (unsigned i = 0; i < 5; ++i)
    actor_alias.put(table_header + 0x16 + 2 * i, 0x3300u + i, 2);
  CHECK(actor_alias.run() == NBA97_TEXT_COMPLETE);
  CHECK(actor_alias.get(0x80020becu) == 0x800fdcecu);
  CHECK(actor_alias.get(0x80020bf0u) == 0x0013dde0u);
  CHECK(actor_alias.get(0x80020bf4u) == 0x800f046cu);
  CHECK(actor_alias.get(0x80020bf8u) == 0x800fdfc8u);
  CHECK(actor_alias.get(0x80020bfcu) == 0x800fe0bcu);

  Fixture unaligned;
  unaligned.context.machine.registers.gpr[4].word = Fixture::Home + 1;
  CHECK(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  CHECK(unaligned.progress.stopped_pc == 0x800655b4u);

  Fixture opponent_unaligned;
  opponent_unaligned.context.machine.registers.gpr[5].word = Fixture::Away + 1;
  CHECK(opponent_unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  CHECK(opponent_unaligned.progress.stopped_pc == 0x8006571cu);

  Fixture unknown_ra;
  unknown_ra.context.machine.registers.gpr[31].known_mask = 7;
  CHECK(unknown_ra.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_ra.progress.stopped_pc == 0x80065818u);
  CHECK(unknown_ra.progress.machine.registers.gpr[29].word == Fixture::Stack);

  Fixture bad_ra;
  bad_ra.context.machine.registers.gpr[31].word |= 1;
  CHECK(bad_ra.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  CHECK(bad_ra.progress.stopped_pc == 0x80065818u);

  std::array<std::uint8_t, 0x100> low{};
  std::array<std::uint8_t, 0x100> low_known{};
  low_known.fill(1);
  low[4] = 0;
  low[5] = 0;
  Nba97GameTextRegion low_region{0, low.data(), low_known.data(), low.size()};
  Fixture wrap;
  Nba97GameTextRegion regions[2] = {low_region, wrap.region};
  wrap.context.memory = {regions, 2};
  wrap.context.machine.registers.gpr[4] = {0xfffffff0u, 15};
  wrap.context.operation_budget = 1;
  CHECK(wrap.run() == NBA97_TEXT_LIMIT);
  CHECK(wrap.journal[0].address == 4 && wrap.journal[0].pc == 0x800655b4u);
}

void all_masks_and_argument_guards() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture fixture;
    fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
        .known_mask = static_cast<std::uint8_t>(mask);
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
              .word == Fixture::Stack);
    CHECK(fixture.progress.machine.hi.known_mask == 5 &&
          fixture.progress.machine.lo.known_mask == 10);
  }

  Nba97GameTeamHeaderInitializeProgress progress{};
  CHECK(nba97_game_team_header_initialize(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
  Fixture fixture;
  CHECK(nba97_game_team_header_initialize(&fixture.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
  fixture.context.machine.registers.gpr[0].word = 1;
  CHECK(fixture.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_mask;
  bad_mask.context.machine.hi.known_mask = 16;
  CHECK(bad_mask.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_journal;
  bad_journal.context.access_journal = nullptr;
  CHECK(bad_journal.run() == NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  side_paths_counts_and_thresholds();
  exact_access_order_and_budgets();
  unknowns_atomicity_and_no_known_store();
  aliases_alignment_wrap_and_return();
  all_masks_and_argument_guards();
  std::printf("game team header initialize tests passed (%u checks)\n", checks);
}
