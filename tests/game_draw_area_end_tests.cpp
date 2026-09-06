#include "recovered/game_draw_area_end.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

size_t checks;

void check(bool condition, const char *expression, int line) {
  ++checks;
  if (condition)
    return;
  std::fprintf(stderr, "check failed at line %d: %s\n", line, expression);
  std::exit(1);
}

#define CHECK(expression) check((expression), #expression, __LINE__)

constexpr uint32_t kGlobals = UINT32_C(0x800c55c0);

void put16(uint8_t *bytes, size_t offset, uint16_t value) {
  bytes[offset] = static_cast<uint8_t>(value);
  bytes[offset + 1u] = static_cast<uint8_t>(value >> 8u);
}

void set_word(Nba97GameDrawAreaEndWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

struct Fixture {
  std::array<uint8_t, 8> bytes{};
  std::array<uint8_t, 8> known{1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u};
  Nba97GameTextRegion region{kGlobals, bytes.data(), known.data(),
                             bytes.size()};
  std::array<Nba97GameDrawAreaEndAccess, 4> journal{};
  Nba97GameDrawAreaEndContext context{};
  Nba97GameDrawAreaEndProgress progress{};

  Fixture() {
    context.memory = {&region, 1u};
    context.operation_budget = 3u;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned reg = 0u; reg != 32u; ++reg)
      set_word(context.machine.registers.gpr[reg],
               UINT32_C(0x21000000) + reg * UINT32_C(0x01010101),
               static_cast<uint8_t>(reg % 16u));
    set_word(context.machine.registers.gpr[0], 0u);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x81234560));
    set_word(context.machine.hi, UINT32_C(0x12345678), 5u);
    set_word(context.machine.lo, UINT32_C(0x9abcdef0), 10u);
    coordinates(10, 20);
    limits(640u, 480u);
    bytes[0] = 0u;
  }

  void coordinates(int16_t x, int16_t y, uint16_t x_high = 0xa55au,
                   uint16_t y_high = 0x5aa5u) {
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
             static_cast<uint32_t>(static_cast<uint16_t>(x)) |
                 (static_cast<uint32_t>(x_high) << 16u));
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
             static_cast<uint32_t>(static_cast<uint16_t>(y)) |
                 (static_cast<uint32_t>(y_high) << 16u));
  }

  void limits(uint16_t x, uint16_t y) {
    put16(bytes.data(), 4u, x);
    put16(bytes.data(), 6u, y);
  }

  int run() { return nba97_game_draw_area_end(&context, &progress); }
};

uint32_t expected_command(int32_t x, int32_t y, uint8_t type) {
  const unsigned bits = type == 1u || type == 2u ? 12u : 10u;
  const uint32_t mask = (UINT32_C(1) << bits) - 1u;
  return UINT32_C(0xe4000000) | ((static_cast<uint32_t>(y) & mask) << bits) |
         (static_cast<uint32_t>(x) & mask);
}

void normal_negative_clamps_and_types() {
  for (uint8_t type :
       {uint8_t{0}, uint8_t{1}, uint8_t{2}, uint8_t{3}, uint8_t{255}}) {
    Fixture fixture;
    fixture.bytes[0] = type;
    const auto before = fixture.context.machine;
    const auto memory_before = fixture.bytes;
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    CHECK(fixture.progress.completed == 1u);
    CHECK(fixture.progress.operations == 3u);
    CHECK(fixture.progress.reads == 3u);
    CHECK(fixture.progress.return_v0.word == expected_command(10, 20, type));
    CHECK(fixture.progress.return_v0.known_mask == 15u);
    CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
              .word == UINT32_C(0xe4000000));
    CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
              .word == 20u);
    CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2]
              .word == 639u);
    CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V1]
              .word == (20u << (type == 1u || type == 2u ? 12u : 10u)));
    CHECK(fixture.journal[0].address == kGlobals + 4u);
    CHECK(fixture.journal[1].address == kGlobals + 6u);
    CHECK(fixture.journal[2].address == kGlobals);
    CHECK(fixture.journal[0].operation == 1u);
    CHECK(fixture.journal[2].operation == 3u);
    CHECK(fixture.journal[0].width == 2u);
    CHECK(fixture.journal[2].width == 1u);
    CHECK(fixture.bytes == memory_before);
    for (unsigned reg = 0u; reg != 32u; ++reg) {
      if (reg == NBA97_MATCH_INITIALIZE_V0 ||
          reg == NBA97_MATCH_INITIALIZE_V1 ||
          reg == NBA97_MATCH_INITIALIZE_A0 ||
          reg == NBA97_MATCH_INITIALIZE_A1 || reg == NBA97_MATCH_INITIALIZE_A2)
        continue;
      CHECK(fixture.progress.machine.registers.gpr[reg].word ==
            before.registers.gpr[reg].word);
      CHECK(fixture.progress.machine.registers.gpr[reg].known_mask ==
            before.registers.gpr[reg].known_mask);
    }
    CHECK(std::memcmp(&fixture.progress.machine.hi, &before.hi,
                      sizeof(before.hi)) == 0);
    CHECK(std::memcmp(&fixture.progress.machine.lo, &before.lo,
                      sizeof(before.lo)) == 0);
  }

  Fixture negative;
  negative.coordinates(-1, INT16_MIN);
  CHECK(negative.run() == NBA97_TEXT_COMPLETE);
  CHECK(negative.progress.operations == 1u);
  CHECK(negative.progress.return_v0.word == expected_command(0, 0, 0u));
  CHECK(
      negative.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
      negative.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word);

  Fixture clamps;
  clamps.coordinates(INT16_MAX, 30000);
  clamps.limits(101u, 201u);
  CHECK(clamps.run() == NBA97_TEXT_COMPLETE);
  CHECK(clamps.progress.return_v0.word == expected_command(100, 200, 0u));
}

void signed_limit_extremes_and_coordinate_garbage() {
  const uint16_t limits[] = {0u, 1u, 0x7fffu, 0x8000u, 0xffffu};
  for (uint16_t limit : limits) {
    Fixture fixture;
    fixture.coordinates(7, 7, 0xffffu, 0x8000u);
    fixture.limits(limit, limit);
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    const int32_t last = static_cast<int16_t>(limit) - 1;
    const int32_t expected = last < 7 ? last : 7;
    CHECK(fixture.progress.return_v0.word ==
          expected_command(expected, expected, 0u));
    CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
              .word == static_cast<uint32_t>(expected));
  }

  const int16_t coordinates[] = {INT16_MIN, int16_t{-1}, int16_t{0}, int16_t{1},
                                 INT16_MAX};
  for (int16_t coordinate : coordinates) {
    Fixture fixture;
    fixture.coordinates(coordinate, coordinate, 0x1357u, 0x2468u);
    fixture.limits(0x7fffu, 0x7fffu);
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    const int32_t expected = coordinate < 0       ? 0
                             : coordinate > 32766 ? 32766
                                                  : coordinate;
    CHECK(fixture.progress.return_v0.word ==
          expected_command(expected, expected, 0u));
  }

  Fixture raw10;
  raw10.coordinates(0x07ff, 0x07ff);
  raw10.limits(0x7fffu, 0x7fffu);
  CHECK(raw10.run() == NBA97_TEXT_COMPLETE);
  CHECK(raw10.progress.return_v0.word == expected_command(0x07ff, 0x07ff, 0u));
  Fixture raw12;
  raw12.coordinates(0x07ff, 0x07ff);
  raw12.limits(0x7fffu, 0x7fffu);
  raw12.bytes[0] = 1u;
  CHECK(raw12.run() == NBA97_TEXT_COMPLETE);
  CHECK(raw12.progress.return_v0.word == expected_command(0x07ff, 0x07ff, 1u));
  CHECK(raw10.progress.return_v0.word != raw12.progress.return_v0.word);

  for (uint8_t type : {uint8_t{0}, uint8_t{1}, uint8_t{2}, uint8_t{3}}) {
    Fixture exposed;
    exposed.coordinates(1234, 2345);
    exposed.limits(0x7fffu, 0x7fffu);
    exposed.bytes[0] = type;
    CHECK(exposed.run() == NBA97_TEXT_COMPLETE);
    CHECK(exposed.progress.return_v0.word ==
          expected_command(1234, 2345, type));
  }
}

void partial_knownness_and_branch_prefixes() {
  Fixture x_sign;
  x_sign.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask =
      13u;
  CHECK(x_sign.run() == NBA97_TEXT_UNKNOWN);
  CHECK(x_sign.progress.stopped_pc == UINT32_C(0x8009a718));
  CHECK(x_sign.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
            .known_mask == 1u);
  CHECK(x_sign.progress.return_v0.word == 0u);
  CHECK(x_sign.progress.return_v0.known_mask == 15u);

  Fixture y_sign;
  y_sign.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].known_mask =
      13u;
  CHECK(y_sign.run() == NBA97_TEXT_UNKNOWN);
  CHECK(y_sign.progress.stopped_pc == UINT32_C(0x8009a75c));
  CHECK(y_sign.progress.operations == 1u);
  CHECK(y_sign.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
        0u);
  CHECK(y_sign.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
            .known_mask == 15u);
  CHECK(y_sign.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V1]
            .known_mask == 1u);

  Fixture x_limit_sign;
  x_limit_sign.known[5] = 0u;
  CHECK(x_limit_sign.run() == NBA97_TEXT_UNKNOWN);
  CHECK(x_limit_sign.progress.stopped_pc == UINT32_C(0x8009a740));
  CHECK(x_limit_sign.progress.operations == 1u);
  CHECK(x_limit_sign.progress.return_v0.word == 10u);
  CHECK(x_limit_sign.progress.return_v0.known_mask == 15u);
  CHECK(x_limit_sign.journal[0].known_mask == 13u);

  Fixture y_limit_sign;
  y_limit_sign.known[7] = 0u;
  CHECK(y_limit_sign.run() == NBA97_TEXT_UNKNOWN);
  CHECK(y_limit_sign.progress.stopped_pc == UINT32_C(0x8009a784));
  CHECK(y_limit_sign.progress.operations == 2u);
  CHECK(y_limit_sign.progress.return_v0.known_mask == 14u);
  CHECK(y_limit_sign.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
            .known_mask != 15u);

  Fixture type;
  type.known[0] = 0u;
  CHECK(type.run() == NBA97_TEXT_UNKNOWN);
  CHECK(type.progress.stopped_pc == UINT32_C(0x8009a7ac));
  CHECK(type.progress.operations == 3u);
  CHECK(type.progress.return_v0.known_mask == 14u);
  CHECK(type.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V1].word ==
        20u);
  CHECK(type.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V1]
            .known_mask == 15u);

  Fixture low_partial;
  low_partial.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
      .known_mask = 14u;
  low_partial.limits(101u, 480u);
  CHECK(low_partial.run() == NBA97_TEXT_UNKNOWN);
  CHECK(low_partial.progress.stopped_pc == UINT32_C(0x8009a740));
}

void skipped_reads_failures_budgets_and_atomicity() {
  std::array<uint8_t, 1> type_byte{2u};
  std::array<uint8_t, 1> type_known{1u};
  Nba97GameTextRegion type_only{kGlobals, type_byte.data(), type_known.data(),
                                type_byte.size()};
  Fixture negative;
  negative.coordinates(-2, -3);
  negative.context.memory = {&type_only, 1u};
  CHECK(negative.run() == NBA97_TEXT_COMPLETE);
  CHECK(negative.progress.operations == 1u);
  CHECK(negative.progress.return_v0.word == UINT32_C(0xe4000000));

  Fixture missing_x;
  missing_x.context.memory.count = 0u;
  CHECK(missing_x.run() == NBA97_TEXT_RESOURCE);
  CHECK(missing_x.progress.stopped_pc == UINT32_C(0x8009a728));
  CHECK(missing_x.progress.stopped_address == kGlobals + 4u);
  CHECK(missing_x.progress.return_v0.word == kGlobals + 4u);

  std::array<uint8_t, 2> x_bytes{0x80u, 0x02u};
  std::array<uint8_t, 2> x_known{1u, 1u};
  Nba97GameTextRegion x_only{kGlobals + 4u, x_bytes.data(), x_known.data(),
                             x_bytes.size()};
  Fixture missing_y;
  missing_y.context.memory = {&x_only, 1u};
  CHECK(missing_y.run() == NBA97_TEXT_RESOURCE);
  CHECK(missing_y.progress.stopped_pc == UINT32_C(0x8009a76c));
  CHECK(missing_y.progress.operations == 2u);
  CHECK(missing_y.progress.return_v0.word == kGlobals + 6u);

  std::array<uint8_t, 4> limits{0x80u, 0x02u, 0xe0u, 0x01u};
  std::array<uint8_t, 4> limits_known{1u, 1u, 1u, 1u};
  Nba97GameTextRegion limits_only{kGlobals + 4u, limits.data(),
                                  limits_known.data(), limits.size()};
  Fixture missing_type;
  missing_type.context.memory = {&limits_only, 1u};
  CHECK(missing_type.run() == NBA97_TEXT_RESOURCE);
  CHECK(missing_type.progress.stopped_pc == UINT32_C(0x8009a79c));
  CHECK(missing_type.progress.operations == 3u);
  CHECK(missing_type.progress.return_v0.word == kGlobals);

  for (size_t budget = 0u; budget != 3u; ++budget) {
    Fixture fixture;
    fixture.context.operation_budget = budget;
    CHECK(fixture.run() == NBA97_TEXT_LIMIT);
    CHECK(fixture.progress.operations == budget);
    CHECK(fixture.progress.reads == budget);
  }
  Fixture budget_three;
  CHECK(budget_three.run() == NBA97_TEXT_COMPLETE);

  for (size_t malformed = 0u; malformed != 8u; ++malformed) {
    if (malformed == 1u || malformed == 2u || malformed == 3u)
      continue;
    Fixture fixture;
    fixture.known[malformed] = 2u;
    const auto before_v0 =
        fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0];
    const int result = fixture.run();
    CHECK(result == NBA97_TEXT_ARGUMENT);
    CHECK(fixture.progress.access_events == fixture.progress.reads);
    if (malformed == 4u || malformed == 5u) {
      CHECK(fixture.progress.stopped_pc == UINT32_C(0x8009a728));
      CHECK(fixture.progress.return_v0.word == kGlobals + 4u);
    } else if (malformed == 6u || malformed == 7u) {
      CHECK(fixture.progress.stopped_pc == UINT32_C(0x8009a76c));
      CHECK(fixture.progress.return_v0.word == kGlobals + 6u);
    } else {
      CHECK(fixture.progress.stopped_pc == UINT32_C(0x8009a79c));
      CHECK(fixture.progress.return_v0.word == kGlobals);
    }
    CHECK(fixture.progress.return_v0.word != before_v0.word);
  }
}

void unknown_ra_mapping_journal_and_determinism() {
  Fixture unknown_ra;
  unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 14u;
  CHECK(unknown_ra.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_ra.progress.stopped_pc == UINT32_C(0x8009a7d4));
  CHECK(unknown_ra.progress.return_v0.word == expected_command(10, 20, 0u));
  CHECK(unknown_ra.progress.return_v0.known_mask == 15u);

  Fixture truncated;
  truncated.context.access_journal_capacity = 1u;
  CHECK(truncated.run() == NBA97_TEXT_COMPLETE);
  CHECK(truncated.progress.access_events == 3u);
  CHECK(truncated.journal[0].pc == UINT32_C(0x8009a728));

  Fixture no_journal;
  no_journal.context.access_journal = nullptr;
  no_journal.context.access_journal_capacity = 0u;
  CHECK(no_journal.run() == NBA97_TEXT_COMPLETE);
  CHECK(no_journal.progress.access_events == 3u);
  Fixture bad_journal;
  bad_journal.context.access_journal = nullptr;
  CHECK(bad_journal.run() == NBA97_TEXT_ARGUMENT);

  Fixture overlap;
  Nba97GameTextRegion overlap_regions[2] = {overlap.region,
                                            {kGlobals + 7u,
                                             overlap.bytes.data() + 7u,
                                             overlap.known.data() + 7u, 1u}};
  overlap.context.memory = {overlap_regions, 2u};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture overflow;
  overflow.region.base = UINT32_C(0xfffffffc);
  overflow.region.size = 8u;
  CHECK(overflow.run() == NBA97_TEXT_ARGUMENT);
  Fixture missing_regions;
  missing_regions.context.memory.region = nullptr;
  CHECK(missing_regions.run() == NBA97_TEXT_ARGUMENT);
  Fixture missing_data;
  missing_data.region.data = nullptr;
  CHECK(missing_data.run() == NBA97_TEXT_ARGUMENT);
  Fixture zero_size;
  zero_size.region.size = 0u;
  CHECK(zero_size.run() == NBA97_TEXT_ARGUMENT);

  std::array<uint8_t, 9> shifted_bytes{};
  std::array<uint8_t, 9> shifted_known{};
  shifted_known.fill(1u);
  shifted_bytes[1] = 1u;
  put16(shifted_bytes.data(), 5u, 640u);
  put16(shifted_bytes.data(), 7u, 480u);
  Nba97GameTextRegion shifted{kGlobals - 1u, shifted_bytes.data(),
                              shifted_known.data(), shifted_bytes.size()};
  Fixture unaligned_region;
  unaligned_region.context.memory = {&shifted, 1u};
  CHECK(unaligned_region.run() == NBA97_TEXT_COMPLETE);
  CHECK(unaligned_region.progress.return_v0.word ==
        expected_command(10, 20, 1u));

  Fixture invalid_gpr;
  invalid_gpr.context.machine.registers.gpr[12].known_mask = 16u;
  CHECK(invalid_gpr.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalid_hi;
  invalid_hi.context.machine.hi.known_mask = 16u;
  CHECK(invalid_hi.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalid_lo;
  invalid_lo.context.machine.lo.known_mask = 16u;
  CHECK(invalid_lo.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalid_zero;
  invalid_zero.context.machine.registers.gpr[0].word = 1u;
  CHECK(invalid_zero.run() == NBA97_TEXT_ARGUMENT);
  Fixture nulls;
  CHECK(nba97_game_draw_area_end(nullptr, &nulls.progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_draw_area_end(&nulls.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);

  Fixture first;
  Fixture second;
  CHECK(first.run() == NBA97_TEXT_COMPLETE);
  CHECK(second.run() == NBA97_TEXT_COMPLETE);
  CHECK(std::memcmp(&first.progress, &second.progress,
                    sizeof(first.progress)) == 0);
  CHECK(std::memcmp(first.journal.data(), second.journal.data(),
                    sizeof(first.journal)) == 0);
}

} // namespace

int main() {
  normal_negative_clamps_and_types();
  signed_limit_extremes_and_coordinate_garbage();
  partial_knownness_and_branch_prefixes();
  skipped_reads_failures_budgets_and_atomicity();
  unknown_ra_mapping_journal_and_determinism();
  std::printf("game_draw_area_end_tests: %zu checks passed\n", checks);
  return 0;
}
