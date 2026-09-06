#include "recovered/game_video_mode.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

size_t checks = 0u;
void check(bool condition, const char *expression, int line) {
  ++checks;
  if (condition)
    return;
  std::fprintf(stderr, "check failed at line %d: %s\n", line, expression);
  std::exit(1);
}
#define CHECK(expression) check((expression), #expression, __LINE__)

constexpr uint32_t kVideoAddress = UINT32_C(0x800c54ac);

void set_word(Nba97GameVideoModeWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

struct Fixture {
  uint8_t bytes[4]{};
  uint8_t known[4] = {1u, 1u, 1u, 1u};
  Nba97GameTextRegion region{kVideoAddress, bytes, known, sizeof(bytes)};
  Nba97GameVideoModeAccess journal[2]{};
  Nba97GameVideoModeContext context{};
  Nba97GameVideoModeProgress progress{};

  Fixture() {
    context.memory = {&region, 1u};
    context.operation_budget = 1u;
    context.access_journal = journal;
    context.access_journal_capacity = 2u;
    for (unsigned reg = 0u; reg != 32u; ++reg)
      set_word(context.machine.registers.gpr[reg],
               UINT32_C(0x21000000) + reg * UINT32_C(0x01010101),
               static_cast<uint8_t>(reg % 16u));
    set_word(context.machine.registers.gpr[0], 0u);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x81234560));
    set_word(context.machine.hi, UINT32_C(0x12345678), 5u);
    set_word(context.machine.lo, UINT32_C(0x9abcdef0), 10u);
    put(UINT32_C(0x80000000));
  }

  void put(uint32_t value) {
    for (unsigned byte = 0u; byte != 4u; ++byte)
      bytes[byte] = static_cast<uint8_t>(value >> (8u * byte));
  }

  int run() { return nba97_game_video_mode(&context, &progress); }
};

void test_raw_values_and_full_machine_preservation() {
  const uint32_t values[] = {
      0u, 1u, 2u, 255u, UINT32_C(0xffffffff), UINT32_C(0x80000000)};
  for (uint32_t value : values) {
    Fixture fixture;
    fixture.put(value);
    const Nba97GameVideoModeMachine before = fixture.context.machine;
    const uint8_t memory_before[4] = {fixture.bytes[0], fixture.bytes[1],
                                      fixture.bytes[2], fixture.bytes[3]};
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    CHECK(fixture.progress.completed == 1u);
    CHECK(fixture.progress.operations == 1u);
    CHECK(fixture.progress.accesses == 1u);
    CHECK(fixture.progress.reads == 1u);
    CHECK(fixture.progress.access_events == 1u);
    CHECK(fixture.progress.return_v0.word == value);
    CHECK(fixture.progress.return_v0.known_mask == 15u);
    CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
              .word == value);
    CHECK(fixture.journal[0].pc == UINT32_C(0x800985d0));
    CHECK(fixture.journal[0].address == kVideoAddress);
    CHECK((fixture.journal[0].address & 3u) == 0u);
    CHECK(fixture.journal[0].value == value);
    CHECK(fixture.journal[0].operation == 1u);
    CHECK(fixture.journal[0].width == 4u);
    CHECK(fixture.journal[0].known_mask == 15u);
    CHECK(fixture.journal[0].kind == NBA97_GAME_MATCH_CLOCKS_READ);
    for (unsigned reg = 0u; reg != 32u; ++reg) {
      if (reg == NBA97_MATCH_INITIALIZE_V0)
        continue;
      CHECK(fixture.progress.machine.registers.gpr[reg].word ==
            before.registers.gpr[reg].word);
      CHECK(fixture.progress.machine.registers.gpr[reg].known_mask ==
            before.registers.gpr[reg].known_mask);
    }
    CHECK(fixture.progress.machine.hi.word == before.hi.word);
    CHECK(fixture.progress.machine.hi.known_mask == before.hi.known_mask);
    CHECK(fixture.progress.machine.lo.word == before.lo.word);
    CHECK(fixture.progress.machine.lo.known_mask == before.lo.known_mask);
    CHECK(std::memcmp(fixture.bytes, memory_before, sizeof(memory_before)) ==
          0);
  }
}

void test_all_source_known_masks() {
  for (uint8_t mask = 0u; mask != 16u; ++mask) {
    Fixture fixture;
    fixture.put(UINT32_C(0xa1b2c3d4));
    for (unsigned byte = 0u; byte != 4u; ++byte)
      fixture.known[byte] = static_cast<uint8_t>((mask >> byte) & 1u);
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    CHECK(fixture.progress.return_v0.word == UINT32_C(0xa1b2c3d4));
    CHECK(fixture.progress.return_v0.known_mask == mask);
    CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
              .known_mask == mask);
    CHECK(fixture.journal[0].known_mask == mask);
  }
}

void test_budget_and_unknown_return_prefixes() {
  Fixture bounded;
  set_word(bounded.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0],
           UINT32_C(0xdeadbeef), 3u);
  bounded.context.operation_budget = 0u;
  CHECK(bounded.run() == NBA97_TEXT_LIMIT);
  CHECK(bounded.progress.operations == 0u);
  CHECK(bounded.progress.accesses == 0u);
  CHECK(bounded.progress.reads == 0u);
  CHECK(bounded.progress.stopped_pc == UINT32_C(0x800985d0));
  CHECK(bounded.progress.stopped_address == kVideoAddress);
  CHECK(bounded.progress.return_v0.word == UINT32_C(0x800c0000));
  CHECK(bounded.progress.return_v0.known_mask == 15u);

  Fixture unknown_ra;
  unknown_ra.put(UINT32_C(0xffffffff));
  unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 7u;
  CHECK(unknown_ra.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_ra.progress.operations == 1u);
  CHECK(unknown_ra.progress.reads == 1u);
  CHECK(unknown_ra.progress.stopped_pc == UINT32_C(0x800985d4));
  CHECK(unknown_ra.progress.stopped_address == 0u);
  CHECK(unknown_ra.progress.return_v0.word == UINT32_C(0xffffffff));
  CHECK(unknown_ra.progress.return_v0.known_mask == 15u);

  Fixture partial_unknown_ra;
  partial_unknown_ra.known[0] = 0u;
  partial_unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 0u;
  CHECK(partial_unknown_ra.run() == NBA97_TEXT_UNKNOWN);
  CHECK(partial_unknown_ra.progress.reads == 1u);
  CHECK(partial_unknown_ra.progress.return_v0.known_mask == 14u);
}

void test_mapping_validation_and_atomic_load() {
  Fixture unmapped;
  unmapped.context.memory.count = 0u;
  CHECK(unmapped.run() == NBA97_TEXT_RESOURCE);
  CHECK(unmapped.progress.operations == 1u);
  CHECK(unmapped.progress.accesses == 1u);
  CHECK(unmapped.progress.reads == 0u);
  CHECK(unmapped.progress.return_v0.word == UINT32_C(0x800c0000));

  for (unsigned malformed_byte = 0u; malformed_byte != 4u; ++malformed_byte) {
    Fixture malformed;
    malformed.known[malformed_byte] = 2u;
    CHECK(malformed.run() == NBA97_TEXT_ARGUMENT);
    CHECK(malformed.progress.operations == 1u);
    CHECK(malformed.progress.accesses == 1u);
    CHECK(malformed.progress.reads == 0u);
    CHECK(malformed.progress.access_events == 0u);
    CHECK(malformed.progress.return_v0.word == UINT32_C(0x800c0000));
    CHECK(malformed.progress.return_v0.known_mask == 15u);
  }

  Fixture implicit_known;
  implicit_known.region.known = nullptr;
  implicit_known.put(UINT32_C(0x01020304));
  CHECK(implicit_known.run() == NBA97_TEXT_COMPLETE);
  CHECK(implicit_known.progress.return_v0.word == UINT32_C(0x01020304));
  CHECK(implicit_known.progress.return_v0.known_mask == 15u);

  Fixture truncated;
  truncated.context.access_journal_capacity = 0u;
  truncated.context.access_journal = nullptr;
  CHECK(truncated.run() == NBA97_TEXT_COMPLETE);
  CHECK(truncated.progress.access_events == 1u);

  Fixture bad_journal;
  bad_journal.context.access_journal = nullptr;
  CHECK(bad_journal.run() == NBA97_TEXT_ARGUMENT);
  Fixture missing_regions;
  missing_regions.context.memory.region = nullptr;
  CHECK(missing_regions.run() == NBA97_TEXT_ARGUMENT);
  Fixture null_data;
  null_data.region.data = nullptr;
  CHECK(null_data.run() == NBA97_TEXT_ARGUMENT);
  Fixture empty;
  empty.region.size = 0u;
  CHECK(empty.run() == NBA97_TEXT_ARGUMENT);
  Fixture overflow;
  overflow.region.base = UINT32_C(0xfffffff0);
  overflow.region.size = 32u;
  CHECK(overflow.run() == NBA97_TEXT_ARGUMENT);
  Fixture overlap;
  Nba97GameTextRegion regions[2] = {
      overlap.region,
      {kVideoAddress + 2u, overlap.bytes + 2u, overlap.known + 2u, 2u}};
  overlap.context.memory = {regions, 2u};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
}

void test_invalid_machine_nulls_and_repeatability() {
  Fixture invalid_gpr;
  invalid_gpr.context.machine.registers.gpr[20].known_mask = 16u;
  CHECK(invalid_gpr.run() == NBA97_TEXT_ARGUMENT);
  CHECK(invalid_gpr.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .word ==
        invalid_gpr.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .word);
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
  CHECK(nba97_game_video_mode(nullptr, &nulls.progress) == NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_video_mode(&nulls.context, nullptr) == NBA97_TEXT_ARGUMENT);

  Fixture first;
  Fixture second;
  first.put(UINT32_C(0x76543210));
  second.put(UINT32_C(0x76543210));
  CHECK(first.run() == NBA97_TEXT_COMPLETE);
  CHECK(second.run() == NBA97_TEXT_COMPLETE);
  CHECK(first.progress.return_v0.word == second.progress.return_v0.word);
  CHECK(first.progress.return_v0.known_mask ==
        second.progress.return_v0.known_mask);
  for (unsigned reg = 0u; reg != 32u; ++reg) {
    CHECK(first.progress.machine.registers.gpr[reg].word ==
          second.progress.machine.registers.gpr[reg].word);
    CHECK(first.progress.machine.registers.gpr[reg].known_mask ==
          second.progress.machine.registers.gpr[reg].known_mask);
  }
}

} // namespace

int main() {
  test_raw_values_and_full_machine_preservation();
  test_all_source_known_masks();
  test_budget_and_unknown_return_prefixes();
  test_mapping_validation_and_atomic_load();
  test_invalid_machine_nulls_and_repeatability();
  std::printf("game_video_mode_tests: %zu checks passed\n", checks);
  return 0;
}
