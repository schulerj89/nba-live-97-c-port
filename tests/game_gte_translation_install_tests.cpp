#include "recovered/game_gte_translation_install.h"

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

constexpr uint32_t kObject = UINT32_C(0x80100000);

void put32(uint8_t *data, size_t offset, uint32_t value) {
  for (unsigned byte = 0u; byte != 4u; ++byte)
    data[offset + byte] = static_cast<uint8_t>(value >> (8u * byte));
}
void set_word(Nba97GameGteTranslationInstallWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

struct Fixture {
  uint8_t data[48]{};
  uint8_t known[48]{};
  Nba97GameTextRegion region{};
  Nba97GameGteTranslationInstallWord control[32]{};
  Nba97GameGteTranslationInstallAccess access[5]{};
  Nba97GameGteTranslationInstallControlWrite writes[5]{};
  Nba97GameGteTranslationInstallContext context{};
  Nba97GameGteTranslationInstallProgress progress{};

  Fixture() {
    std::memset(known, 1, sizeof(known));
    region.base = kObject;
    region.data = data;
    region.known = known;
    region.size = sizeof(data);
    context.memory.region = &region;
    context.memory.count = 1u;
    context.operation_budget = 6u;
    context.control = control;
    context.access_journal = access;
    context.access_journal_capacity = 5u;
    context.control_journal = writes;
    context.control_journal_capacity = 5u;
    for (unsigned reg = 0u; reg != 32u; ++reg)
      set_word(context.machine.registers.gpr[reg],
               UINT32_C(0x10000000) + reg * UINT32_C(0x01010101),
               static_cast<uint8_t>(reg % 16u));
    set_word(context.machine.registers.gpr[0], 0u);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0], kObject);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x81234560));
    set_word(context.machine.hi, UINT32_C(0x89abcdef), 5u);
    set_word(context.machine.lo, UINT32_C(0x76543210), 10u);
    for (unsigned index = 0u; index != 32u; ++index)
      set_word(control[index], UINT32_C(0xa0000000) + index,
               static_cast<uint8_t>(index % 16u));
    put32(data, 0x14u, UINT32_C(0x80000000));
    put32(data, 0x18u, UINT32_C(0x7fffffff));
    put32(data, 0x1cu, UINT32_C(0xffff8000));
  }

  int run() { return nba97_game_gte_translation_install(&context, &progress); }
};

void test_exact_order_raw_values_and_preservation() {
  Fixture fixture;
  const Nba97GameGteTranslationInstallMachine before = fixture.context.machine;
  Nba97GameGteTranslationInstallWord controls_before[32];
  uint8_t memory_before[sizeof(fixture.data)];
  std::memcpy(controls_before, fixture.control, sizeof(controls_before));
  std::memcpy(memory_before, fixture.data, sizeof(memory_before));

  CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
  CHECK(fixture.progress.completed == 1u);
  CHECK(fixture.progress.operations == 6u);
  CHECK(fixture.progress.accesses == 3u);
  CHECK(fixture.progress.reads == 3u);
  CHECK(fixture.progress.control_writes == 3u);
  CHECK(fixture.progress.access_events == 3u);
  CHECK(fixture.progress.control_events == 3u);
  for (unsigned index = 0u; index != 3u; ++index) {
    const uint32_t expected = index == 0u   ? UINT32_C(0x80000000)
                              : index == 1u ? UINT32_C(0x7fffffff)
                                            : UINT32_C(0xffff8000);
    CHECK(fixture.access[index].pc == UINT32_C(0x80055f44) + index * 4u);
    CHECK(fixture.access[index].address == kObject + 0x14u + index * 4u);
    CHECK(fixture.access[index].operation == index + 1u);
    CHECK(fixture.access[index].width == 4u);
    CHECK(fixture.access[index].kind == NBA97_GAME_MATCH_CLOCKS_READ);
    CHECK(fixture.writes[index].pc ==
          (index == 2u ? UINT32_C(0x80055f5c)
                       : UINT32_C(0x80055f50) + index * 4u));
    CHECK(fixture.writes[index].operation == index + 4u);
    CHECK(fixture.writes[index].index == 5u + index);
    CHECK(fixture.writes[index].value.word == expected);
    CHECK(fixture.writes[index].value.known_mask == 15u);
    CHECK(fixture.progress.machine.registers
              .gpr[NBA97_MATCH_INITIALIZE_T0 + index]
              .word == expected);
    CHECK(fixture.control[5u + index].word == expected);
    CHECK(fixture.control[5u + index].known_mask == 15u);
  }
  CHECK(fixture.writes[0].operation > fixture.access[2].operation);
  for (unsigned reg = 0u; reg != 32u; ++reg) {
    if (reg >= NBA97_MATCH_INITIALIZE_T0 &&
        reg < NBA97_MATCH_INITIALIZE_T0 + 3u)
      continue;
    CHECK(std::memcmp(&fixture.progress.machine.registers.gpr[reg],
                      &before.registers.gpr[reg],
                      sizeof(before.registers.gpr[reg])) == 0);
  }
  CHECK(std::memcmp(&fixture.progress.machine.hi, &before.hi,
                    sizeof(before.hi)) == 0);
  CHECK(std::memcmp(&fixture.progress.machine.lo, &before.lo,
                    sizeof(before.lo)) == 0);
  for (unsigned index = 0u; index != 32u; ++index)
    if (index < 5u || index > 7u)
      CHECK(std::memcmp(&fixture.control[index], &controls_before[index],
                        sizeof(controls_before[index])) == 0);
  CHECK(std::memcmp(fixture.data, memory_before, sizeof(memory_before)) == 0);
}

void test_all_byte_masks_and_raw_upper_bits() {
  for (uint8_t mask = 0u; mask != 16u; ++mask) {
    Fixture fixture;
    const uint32_t values[3] = {UINT32_C(0x80000001), UINT32_C(0x7fff8000),
                                UINT32_C(0xffff7fff)};
    for (unsigned index = 0u; index != 3u; ++index) {
      put32(fixture.data, 0x14u + index * 4u, values[index]);
      for (unsigned byte = 0u; byte != 4u; ++byte)
        fixture.known[0x14u + index * 4u + byte] =
            static_cast<uint8_t>((mask >> byte) & 1u);
    }
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    for (unsigned index = 0u; index != 3u; ++index) {
      CHECK(fixture.progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_T0 + index]
                .word == values[index]);
      CHECK(fixture.progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_T0 + index]
                .known_mask == mask);
      CHECK(fixture.control[5u + index].word == values[index]);
      CHECK(fixture.control[5u + index].known_mask == mask);
    }
  }
}

void test_every_budget_and_return_delay() {
  const uint32_t stop_pc[6] = {UINT32_C(0x80055f44), UINT32_C(0x80055f48),
                               UINT32_C(0x80055f4c), UINT32_C(0x80055f50),
                               UINT32_C(0x80055f54), UINT32_C(0x80055f5c)};
  for (size_t budget = 0u; budget != 6u; ++budget) {
    Fixture fixture;
    fixture.context.operation_budget = budget;
    CHECK(fixture.run() == NBA97_TEXT_LIMIT);
    CHECK(fixture.progress.operations == budget);
    CHECK(fixture.progress.stopped_pc == stop_pc[budget]);
    CHECK(fixture.progress.reads == (budget < 3u ? budget : 3u));
    CHECK(fixture.progress.control_writes == (budget < 3u ? 0u : budget - 3u));
  }
  Fixture exact;
  CHECK(exact.run() == NBA97_TEXT_COMPLETE);

  Fixture delay_limit;
  delay_limit.context.operation_budget = 5u;
  delay_limit.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 7u;
  CHECK(delay_limit.run() == NBA97_TEXT_LIMIT);
  CHECK(delay_limit.progress.stopped_pc == UINT32_C(0x80055f5c));
  CHECK(delay_limit.progress.control_writes == 2u);

  Fixture unknown_return;
  unknown_return.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 7u;
  CHECK(unknown_return.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_return.progress.stopped_pc == UINT32_C(0x80055f58));
  CHECK(unknown_return.progress.control_writes == 3u);
  CHECK(unknown_return.control[7].word == UINT32_C(0xffff8000));
}

void test_addresses_mapping_and_aliases() {
  Fixture unknown_pointer;
  unknown_pointer.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
      .known_mask = 14u;
  CHECK(unknown_pointer.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_pointer.progress.operations == 0u);
  CHECK(unknown_pointer.progress.stopped_pc == UINT32_C(0x80055f44));
  CHECK(unknown_pointer.progress.stopped_address == kObject + 0x14u);

  Fixture unaligned;
  set_word(unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           kObject + 2u);
  CHECK(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  CHECK(unaligned.progress.operations == 1u);
  CHECK(unaligned.progress.stopped_address == kObject + 0x16u);

  Fixture unmapped;
  unmapped.context.memory.count = 0u;
  CHECK(unmapped.run() == NBA97_TEXT_RESOURCE);
  CHECK(unmapped.progress.stopped_address == kObject + 0x14u);

  Fixture wrapped;
  uint8_t low[12]{};
  uint8_t low_known[12];
  std::memset(low_known, 1, sizeof(low_known));
  put32(low, 0u, UINT32_C(0x11111111));
  put32(low, 4u, UINT32_C(0x22222222));
  put32(low, 8u, UINT32_C(0x33333333));
  Nba97GameTextRegion low_region{0u, low, low_known, sizeof(low)};
  wrapped.context.memory.region = &low_region;
  wrapped.context.memory.count = 1u;
  set_word(wrapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           UINT32_C(0xffffffec));
  CHECK(wrapped.run() == NBA97_TEXT_COMPLETE);
  CHECK(wrapped.control[5].word == UINT32_C(0x11111111));
  CHECK(wrapped.control[6].word == UINT32_C(0x22222222));
  CHECK(wrapped.control[7].word == UINT32_C(0x33333333));

  Fixture alias;
  uint8_t shared[4] = {1u, 2u, 3u, 4u};
  uint8_t shared_known[4] = {1u, 1u, 1u, 1u};
  Nba97GameTextRegion alias_regions[3];
  for (unsigned index = 0u; index != 3u; ++index) {
    alias_regions[index].base = kObject + 0x14u + index * 4u;
    alias_regions[index].data = shared;
    alias_regions[index].known = shared_known;
    alias_regions[index].size = sizeof(shared);
  }
  alias.context.memory.region = alias_regions;
  alias.context.memory.count = 3u;
  CHECK(alias.run() == NBA97_TEXT_COMPLETE);
  for (unsigned index = 5u; index != 8u; ++index)
    CHECK(alias.control[index].word == UINT32_C(0x04030201));
}

void test_invalid_inputs_and_failure_prefixes() {
  Fixture malformed_late_byte;
  const Nba97GameGteTranslationInstallWord original_t2 =
      malformed_late_byte.context.machine.registers
          .gpr[NBA97_MATCH_INITIALIZE_T0 + 2u];
  malformed_late_byte.known[0x1cu + 3u] = 2u;
  CHECK(malformed_late_byte.run() == NBA97_TEXT_ARGUMENT);
  CHECK(malformed_late_byte.progress.operations == 3u);
  CHECK(malformed_late_byte.progress.reads == 2u);
  CHECK(malformed_late_byte.progress.control_writes == 0u);
  CHECK(malformed_late_byte.progress.machine.registers
            .gpr[NBA97_MATCH_INITIALIZE_T0]
            .word == UINT32_C(0x80000000));
  CHECK(malformed_late_byte.progress.machine.registers
            .gpr[NBA97_MATCH_INITIALIZE_T0 + 1u]
            .word == UINT32_C(0x7fffffff));
  CHECK(std::memcmp(&malformed_late_byte.progress.machine.registers
                         .gpr[NBA97_MATCH_INITIALIZE_T0 + 2u],
                    &original_t2, sizeof(original_t2)) == 0);

  Fixture malformed_control;
  malformed_control.control[31].known_mask = 16u;
  CHECK(malformed_control.run() == NBA97_TEXT_ARGUMENT);
  Fixture malformed_machine;
  malformed_machine.context.machine.registers.gpr[27].known_mask = 16u;
  CHECK(malformed_machine.run() == NBA97_TEXT_ARGUMENT);
  Fixture malformed_zero;
  malformed_zero.context.machine.registers.gpr[0].word = 1u;
  CHECK(malformed_zero.run() == NBA97_TEXT_ARGUMENT);
  Fixture malformed_access_journal;
  malformed_access_journal.context.access_journal = nullptr;
  CHECK(malformed_access_journal.run() == NBA97_TEXT_ARGUMENT);
  Fixture malformed_control_journal;
  malformed_control_journal.context.control_journal = nullptr;
  CHECK(malformed_control_journal.run() == NBA97_TEXT_ARGUMENT);
  Fixture null_region;
  null_region.region.data = nullptr;
  CHECK(null_region.run() == NBA97_TEXT_ARGUMENT);
  Fixture malformed_region_list;
  malformed_region_list.context.memory.region = nullptr;
  CHECK(malformed_region_list.run() == NBA97_TEXT_ARGUMENT);
  Fixture overlapping;
  Nba97GameTextRegion regions[2] = {overlapping.region,
                                    {kObject + 0x10u, overlapping.data + 0x10u,
                                     overlapping.known + 0x10u, 8u}};
  overlapping.context.memory.region = regions;
  overlapping.context.memory.count = 2u;
  CHECK(overlapping.run() == NBA97_TEXT_ARGUMENT);

  Fixture short_journals;
  short_journals.context.access_journal_capacity = 1u;
  short_journals.context.control_journal_capacity = 1u;
  CHECK(short_journals.run() == NBA97_TEXT_COMPLETE);
  CHECK(short_journals.progress.access_events == 3u);
  CHECK(short_journals.progress.control_events == 3u);
  CHECK(short_journals.access[0].pc == UINT32_C(0x80055f44));
  CHECK(short_journals.writes[0].pc == UINT32_C(0x80055f50));

  Fixture no_journals;
  no_journals.context.access_journal = nullptr;
  no_journals.context.access_journal_capacity = 0u;
  no_journals.context.control_journal = nullptr;
  no_journals.context.control_journal_capacity = 0u;
  CHECK(no_journals.run() == NBA97_TEXT_COMPLETE);
  CHECK(no_journals.progress.access_events == 3u);
  CHECK(no_journals.progress.control_events == 3u);

  Fixture nulls;
  CHECK(nba97_game_gte_translation_install(nullptr, &nulls.progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_gte_translation_install(&nulls.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}

void test_repeatability() {
  Fixture first;
  Fixture second;
  CHECK(first.run() == NBA97_TEXT_COMPLETE);
  CHECK(second.run() == NBA97_TEXT_COMPLETE);
  CHECK(first.progress.operations == second.progress.operations);
  CHECK(first.progress.reads == second.progress.reads);
  CHECK(first.progress.control_writes == second.progress.control_writes);
  CHECK(first.progress.completed == second.progress.completed);
  for (unsigned reg = 0u; reg != 32u; ++reg)
    CHECK(std::memcmp(&first.progress.machine.registers.gpr[reg],
                      &second.progress.machine.registers.gpr[reg],
                      sizeof(first.progress.machine.registers.gpr[reg])) == 0);
  CHECK(std::memcmp(&first.progress.machine.hi, &second.progress.machine.hi,
                    sizeof(first.progress.machine.hi)) == 0);
  CHECK(std::memcmp(&first.progress.machine.lo, &second.progress.machine.lo,
                    sizeof(first.progress.machine.lo)) == 0);
  for (unsigned index = 0u; index != 32u; ++index) {
    CHECK(first.control[index].word == second.control[index].word);
    CHECK(first.control[index].known_mask == second.control[index].known_mask);
  }
}

} // namespace

int main() {
  test_exact_order_raw_values_and_preservation();
  test_all_byte_masks_and_raw_upper_bits();
  test_every_budget_and_return_delay();
  test_addresses_mapping_and_aliases();
  test_invalid_inputs_and_failure_prefixes();
  test_repeatability();
  std::printf("game_gte_translation_install_tests: %zu checks passed\n",
              checks);
  return 0;
}
