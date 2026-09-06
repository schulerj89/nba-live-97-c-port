#include "recovered/game_draw_offset_command.h"

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

constexpr uint32_t kType = UINT32_C(0x800c55c0);

void set_word(Nba97GameDrawOffsetCommandWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

uint32_t expected(uint32_t x, uint32_t y, uint8_t type) {
  const unsigned bits = type == 1u || type == 2u ? 12u : 11u;
  const uint32_t mask = (UINT32_C(1) << bits) - 1u;
  return UINT32_C(0xe5000000) | ((y & mask) << bits) | (x & mask);
}

struct Fixture {
  uint8_t type{0u};
  uint8_t known{1u};
  Nba97GameTextRegion region{kType, &type, &known, 1u};
  std::array<Nba97GameDrawOffsetCommandAccess, 2> journal{};
  Nba97GameDrawOffsetCommandContext context{};
  Nba97GameDrawOffsetCommandProgress progress{};

  Fixture() {
    context.memory = {&region, 1u};
    context.operation_budget = 1u;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned reg = 0u; reg != 32u; ++reg)
      set_word(context.machine.registers.gpr[reg],
               UINT32_C(0x31000000) + reg * UINT32_C(0x01010101),
               static_cast<uint8_t>(reg % 16u));
    set_word(context.machine.registers.gpr[0], 0u);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
             UINT32_C(0x89abcdef));
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
             UINT32_C(0x76543210));
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x81234560));
    set_word(context.machine.hi, UINT32_C(0x12345678), 5u);
    set_word(context.machine.lo, UINT32_C(0x9abcdef0), 10u);
  }

  int run() { return nba97_game_draw_offset_command(&context, &progress); }
};

void all_types_and_raw_inputs() {
  for (unsigned type = 0u; type != 256u; ++type) {
    Fixture fixture;
    fixture.type = static_cast<uint8_t>(type);
    const auto before = fixture.context.machine;
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    CHECK(fixture.progress.completed == 1u);
    CHECK(fixture.progress.operations == 1u);
    CHECK(fixture.progress.accesses == 1u);
    CHECK(fixture.progress.reads == 1u);
    CHECK(fixture.progress.access_events == 1u);
    CHECK(fixture.progress.return_v0.word ==
          expected(UINT32_C(0x89abcdef), UINT32_C(0x76543210),
                   static_cast<uint8_t>(type)));
    CHECK(fixture.progress.return_v0.known_mask == 15u);
    CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
              .word == UINT32_C(0xe5000000));
    CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
              .word == UINT32_C(0x76543210));
    CHECK(fixture.journal[0].pc == UINT32_C(0x8009a7e4));
    CHECK(fixture.journal[0].address == kType);
    CHECK(fixture.journal[0].value == type);
    CHECK(fixture.journal[0].operation == 1u);
    CHECK(fixture.journal[0].width == 1u);
    CHECK(fixture.journal[0].known_mask == 15u);
    CHECK(fixture.journal[0].kind == NBA97_GAME_MATCH_CLOCKS_READ);
    for (unsigned reg = 0u; reg != 32u; ++reg) {
      if (reg == NBA97_MATCH_INITIALIZE_V0 ||
          reg == NBA97_MATCH_INITIALIZE_V1 || reg == NBA97_MATCH_INITIALIZE_A0)
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
}

void coordinate_extremes_and_field_widths() {
  const uint32_t values[] = {0u,
                             1u,
                             UINT32_C(0x7ff),
                             UINT32_C(0x800),
                             UINT32_C(0xfff),
                             UINT32_C(0x1000),
                             UINT32_C(0x7fffffff),
                             UINT32_C(0x80000000),
                             UINT32_C(0xffffffff),
                             UINT32_C(0x89abcdef)};
  for (uint32_t x : values) {
    for (uint32_t y : values) {
      for (uint8_t type :
           {uint8_t{0}, uint8_t{1}, uint8_t{2}, uint8_t{3}, uint8_t{255}}) {
        Fixture fixture;
        fixture.type = type;
        set_word(
            fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
            x);
        set_word(
            fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
            y);
        CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
        CHECK(fixture.progress.return_v0.word == expected(x, y, type));
        CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
                  .word == y);
      }
    }
  }

  Fixture eleven;
  eleven.type = 0u;
  set_word(eleven.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           UINT32_C(0x0abc));
  set_word(eleven.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
           UINT32_C(0x0def));
  CHECK(eleven.run() == NBA97_TEXT_COMPLETE);
  CHECK(eleven.progress.return_v0.word ==
        expected(UINT32_C(0x0abc), UINT32_C(0x0def), 0u));
  Fixture twelve;
  twelve.type = 1u;
  set_word(twelve.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           UINT32_C(0x0abc));
  set_word(twelve.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
           UINT32_C(0x0def));
  CHECK(twelve.run() == NBA97_TEXT_COMPLETE);
  CHECK(twelve.progress.return_v0.word ==
        expected(UINT32_C(0x0abc), UINT32_C(0x0def), 1u));
  CHECK(eleven.progress.return_v0.word != twelve.progress.return_v0.word);
}

uint8_t expected_output_mask(uint8_t x_mask, uint8_t y_mask) {
  uint8_t result = 0u;
  if ((x_mask & 1u) != 0u)
    result = (uint8_t)(result | 1u);
  if ((x_mask & 2u) != 0u && (y_mask & 1u) != 0u)
    result = (uint8_t)(result | 2u);
  if ((y_mask & 3u) == 3u)
    result = (uint8_t)(result | 4u);
  if ((y_mask & 2u) != 0u)
    result = (uint8_t)(result | 8u);
  return result;
}

void partial_coordinate_and_type_knownness() {
  for (uint8_t x_mask = 0u; x_mask != 16u; ++x_mask) {
    for (uint8_t y_mask = 0u; y_mask != 16u; ++y_mask) {
      Fixture fixture;
      fixture.type = 1u;
      fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
          .known_mask = x_mask;
      fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
          .known_mask = y_mask;
      CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
      CHECK(fixture.progress.return_v0.word ==
            expected(UINT32_C(0x89abcdef), UINT32_C(0x76543210), 1u));
      CHECK(fixture.progress.return_v0.known_mask ==
            expected_output_mask(x_mask, y_mask));
      CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
                .known_mask == y_mask);
    }
  }

  Fixture masked_zero;
  masked_zero.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
      .known_mask = 0u;
  masked_zero.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
      .known_mask = 0u;
  CHECK(masked_zero.run() == NBA97_TEXT_COMPLETE);
  CHECK(masked_zero.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V1]
            .known_mask == 1u);
  CHECK(masked_zero.progress.return_v0.known_mask == 0u);

  Fixture unknown_type;
  unknown_type.known = 0u;
  CHECK(unknown_type.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_type.progress.stopped_pc == UINT32_C(0x8009a7f4));
  CHECK(unknown_type.progress.operations == 1u);
  CHECK(unknown_type.progress.return_v0.known_mask == 14u);
  CHECK(unknown_type.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V1]
            .word == (UINT32_C(0x76543210) & UINT32_C(0xfff)));
  CHECK(unknown_type.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V1]
            .known_mask == 15u);
}

void failures_mapping_budget_and_final_delay() {
  Fixture bounded;
  bounded.context.operation_budget = 0u;
  CHECK(bounded.run() == NBA97_TEXT_LIMIT);
  CHECK(bounded.progress.operations == 0u);
  CHECK(bounded.progress.reads == 0u);
  CHECK(bounded.progress.stopped_pc == UINT32_C(0x8009a7e4));
  CHECK(bounded.progress.stopped_address == kType);
  CHECK(bounded.progress.return_v0.word == kType);
  CHECK(bounded.progress.return_v0.known_mask == 15u);

  Fixture missing;
  missing.context.memory.count = 0u;
  CHECK(missing.run() == NBA97_TEXT_RESOURCE);
  CHECK(missing.progress.operations == 1u);
  CHECK(missing.progress.reads == 0u);
  CHECK(missing.progress.return_v0.word == kType);

  Fixture malformed;
  malformed.known = 2u;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT);
  CHECK(malformed.progress.operations == 1u);
  CHECK(malformed.progress.reads == 0u);
  CHECK(malformed.progress.access_events == 0u);
  CHECK(malformed.progress.return_v0.word == kType);

  Fixture unknown_ra;
  unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 14u;
  CHECK(unknown_ra.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_ra.progress.stopped_pc == UINT32_C(0x8009a81c));
  CHECK(unknown_ra.progress.return_v0.word ==
        expected(UINT32_C(0x89abcdef), UINT32_C(0x76543210), 0u));
  CHECK(unknown_ra.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
            .word == UINT32_C(0xe5000000));

  std::array<uint8_t, 2> shifted_bytes{0x44u, 1u};
  std::array<uint8_t, 2> shifted_known{1u, 1u};
  Nba97GameTextRegion shifted{kType - 1u, shifted_bytes.data(),
                              shifted_known.data(), shifted_bytes.size()};
  Fixture unaligned_region;
  unaligned_region.context.memory = {&shifted, 1u};
  CHECK(unaligned_region.run() == NBA97_TEXT_COMPLETE);
  CHECK(unaligned_region.progress.return_v0.word ==
        expected(UINT32_C(0x89abcdef), UINT32_C(0x76543210), 1u));

  Fixture implicit_known;
  implicit_known.region.known = nullptr;
  CHECK(implicit_known.run() == NBA97_TEXT_COMPLETE);
  CHECK(implicit_known.progress.return_v0.known_mask == 15u);
}

void validation_journal_memory_and_determinism() {
  Fixture truncated;
  truncated.context.access_journal_capacity = 0u;
  truncated.context.access_journal = nullptr;
  const uint8_t memory_before = truncated.type;
  CHECK(truncated.run() == NBA97_TEXT_COMPLETE);
  CHECK(truncated.progress.access_events == 1u);
  CHECK(truncated.type == memory_before);
  Fixture bad_journal;
  bad_journal.context.access_journal = nullptr;
  CHECK(bad_journal.run() == NBA97_TEXT_ARGUMENT);

  Fixture overlap;
  Nba97GameTextRegion regions[2] = {overlap.region, overlap.region};
  overlap.context.memory = {regions, 2u};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture overflow;
  overflow.region.base = UINT32_C(0xffffffff);
  overflow.region.size = 2u;
  CHECK(overflow.run() == NBA97_TEXT_ARGUMENT);
  Fixture missing_regions;
  missing_regions.context.memory.region = nullptr;
  CHECK(missing_regions.run() == NBA97_TEXT_ARGUMENT);
  Fixture null_data;
  null_data.region.data = nullptr;
  CHECK(null_data.run() == NBA97_TEXT_ARGUMENT);
  Fixture zero_size;
  zero_size.region.size = 0u;
  CHECK(zero_size.run() == NBA97_TEXT_ARGUMENT);

  Fixture invalid_gpr;
  invalid_gpr.context.machine.registers.gpr[11].known_mask = 16u;
  CHECK(invalid_gpr.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalid_hi;
  invalid_hi.context.machine.hi.known_mask = 16u;
  CHECK(invalid_hi.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalid_lo;
  invalid_lo.context.machine.lo.known_mask = 16u;
  CHECK(invalid_lo.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalid_zero_word;
  invalid_zero_word.context.machine.registers.gpr[0].word = 1u;
  CHECK(invalid_zero_word.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalid_zero_mask;
  invalid_zero_mask.context.machine.registers.gpr[0].known_mask = 14u;
  CHECK(invalid_zero_mask.run() == NBA97_TEXT_ARGUMENT);

  Fixture nulls;
  CHECK(nba97_game_draw_offset_command(nullptr, &nulls.progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_draw_offset_command(&nulls.context, nullptr) ==
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
  all_types_and_raw_inputs();
  coordinate_extremes_and_field_widths();
  partial_coordinate_and_type_knownness();
  failures_mapping_budget_and_final_delay();
  validation_journal_memory_and_determinism();
  std::printf("game_draw_offset_command_tests: %zu checks passed\n", checks);
  return 0;
}
