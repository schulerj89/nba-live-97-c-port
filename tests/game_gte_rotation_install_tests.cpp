#include "recovered/game_gte_rotation_install.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

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

constexpr uint32_t kMatrix = UINT32_C(0x80100000);

void put32(uint8_t *data, size_t offset, uint32_t value) {
  for (unsigned byte = 0u; byte != 4u; ++byte)
    data[offset + byte] = static_cast<uint8_t>(value >> (8u * byte));
}
void set_word(Nba97GameGteRotationInstallWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

struct Fixture {
  uint8_t data[32]{};
  uint8_t known[32]{};
  Nba97GameTextRegion region{};
  Nba97GameGteRotationInstallWord control[32]{};
  Nba97GameGteRotationInstallAccess access[8]{};
  Nba97GameGteRotationInstallControlWrite writes[8]{};
  Nba97GameGteRotationInstallContext context{};
  Nba97GameGteRotationInstallProgress progress{};

  Fixture() {
    std::memset(known, 1, sizeof(known));
    region.base = kMatrix;
    region.data = data;
    region.known = known;
    region.size = sizeof(data);
    context.memory.region = &region;
    context.memory.count = 1u;
    context.operation_budget = 10u;
    context.control = control;
    context.access_journal = access;
    context.access_journal_capacity = 8u;
    context.control_journal = writes;
    context.control_journal_capacity = 8u;
    for (unsigned reg = 0u; reg != 32u; ++reg)
      set_word(context.machine.registers.gpr[reg],
               UINT32_C(0x10000000) + reg * UINT32_C(0x01010101),
               static_cast<uint8_t>(reg % 16u));
    set_word(context.machine.registers.gpr[0], 0u);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0], kMatrix);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x81234560));
    set_word(context.machine.hi, UINT32_C(0x89abcdef), 5u);
    set_word(context.machine.lo, UINT32_C(0x76543210), 10u);
    for (unsigned index = 0u; index != 32u; ++index)
      set_word(control[index], UINT32_C(0xa0000000) + index,
               static_cast<uint8_t>(index % 16u));
    for (unsigned index = 0u; index != 5u; ++index)
      put32(data, index * 4u,
            UINT32_C(0x11223344) + index * UINT32_C(0x11111111));
  }

  int run() { return nba97_game_gte_rotation_install(&context, &progress); }
};

void test_exact_order_and_preservation() {
  Fixture fixture;
  Nba97GameGteRotationInstallMachine before = fixture.context.machine;
  Nba97GameGteRotationInstallWord controls_before[32];
  std::memcpy(controls_before, fixture.control, sizeof(controls_before));
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
  CHECK(fixture.progress.completed == 1u);
  CHECK(fixture.progress.operations == 10u);
  CHECK(fixture.progress.reads == 5u);
  CHECK(fixture.progress.control_writes == 5u);
  for (unsigned index = 0u; index != 5u; ++index) {
    CHECK(fixture.access[index].pc == UINT32_C(0x80055f18) + index * 4u);
    CHECK(fixture.access[index].address == kMatrix + index * 4u);
    CHECK(fixture.access[index].width == 4u);
    CHECK(fixture.access[index].kind == NBA97_GAME_MATCH_CLOCKS_READ);
    CHECK(fixture.writes[index].pc ==
          (index == 4u ? UINT32_C(0x80055f40)
                       : UINT32_C(0x80055f2c) + index * 4u));
    CHECK(fixture.writes[index].index == index);
    CHECK(fixture.writes[index].operation == 6u + index);
  }
  for (unsigned reg = 0u; reg != 32u; ++reg) {
    if (reg >= NBA97_MATCH_INITIALIZE_T0 &&
        reg < NBA97_MATCH_INITIALIZE_T0 + 5u)
      continue;
    CHECK(std::memcmp(&fixture.progress.machine.registers.gpr[reg],
                      &before.registers.gpr[reg],
                      sizeof(before.registers.gpr[reg])) == 0);
  }
  CHECK(std::memcmp(&fixture.progress.machine.hi, &before.hi,
                    sizeof(before.hi)) == 0);
  CHECK(std::memcmp(&fixture.progress.machine.lo, &before.lo,
                    sizeof(before.lo)) == 0);
  for (unsigned index = 5u; index != 32u; ++index)
    CHECK(std::memcmp(&fixture.control[index], &controls_before[index],
                      sizeof(fixture.control[index])) == 0);
}

void test_rt33_sign_extension_and_raw_t4() {
  const uint32_t values[] = {UINT32_C(0xabcd0000), UINT32_C(0xabcd7fff),
                             UINT32_C(0xabcd8000), UINT32_C(0xabcdffff)};
  const uint32_t expected[] = {0u, UINT32_C(0x00007fff),
                               UINT32_C(0xffff8000), UINT32_C(0xffffffff)};
  for (unsigned index = 0u; index != 4u; ++index) {
    Fixture fixture;
    put32(fixture.data, 16u, values[index]);
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    CHECK(fixture.control[4].word == expected[index]);
    CHECK(fixture.control[4].known_mask == 15u);
    CHECK(fixture.progress.machine.registers
              .gpr[NBA97_MATCH_INITIALIZE_T0 + 4u]
              .word == values[index]);
  }

  for (uint8_t mask = 0u; mask != 16u; ++mask) {
    Fixture fixture;
    for (unsigned byte = 0u; byte != 4u; ++byte)
      fixture.known[16u + byte] = static_cast<uint8_t>((mask >> byte) & 1u);
    put32(fixture.data, 16u, UINT32_C(0xdead8001));
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    CHECK(fixture.progress.machine.registers
              .gpr[NBA97_MATCH_INITIALIZE_T0 + 4u]
              .known_mask == mask);
    CHECK(fixture.control[4].known_mask ==
          static_cast<uint8_t>((mask & 3u) | ((mask & 2u) ? 12u : 0u)));
  }
}

void test_partial_loads_and_every_budget() {
  Fixture partial;
  const uint8_t masks[] = {0u, 1u, 3u, 7u, 15u};
  for (unsigned word = 0u; word != 5u; ++word)
    for (unsigned byte = 0u; byte != 4u; ++byte)
      partial.known[word * 4u + byte] =
          static_cast<uint8_t>((masks[word] >> byte) & 1u);
  CHECK(partial.run() == NBA97_TEXT_COMPLETE);
  for (unsigned index = 0u; index != 4u; ++index)
    CHECK(partial.control[index].known_mask == masks[index]);
  CHECK(partial.progress.machine.registers
            .gpr[NBA97_MATCH_INITIALIZE_T0 + 4u]
            .known_mask == 15u);

  for (size_t budget = 0u; budget != 10u; ++budget) {
    Fixture bounded;
    bounded.context.operation_budget = budget;
    CHECK(bounded.run() == NBA97_TEXT_LIMIT);
    CHECK(bounded.progress.operations == budget);
    CHECK(bounded.progress.reads == (budget < 5u ? budget : 5u));
    CHECK(bounded.progress.control_writes ==
          (budget < 5u ? 0u : budget - 5u));
  }
  Fixture exact;
  CHECK(exact.run() == NBA97_TEXT_COMPLETE);

  Fixture delay_limit;
  delay_limit.context.operation_budget = 9u;
  delay_limit.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 7u;
  CHECK(delay_limit.run() == NBA97_TEXT_LIMIT);
  CHECK(delay_limit.progress.stopped_pc == UINT32_C(0x80055f40));
  CHECK(delay_limit.progress.control_writes == 4u);

  Fixture unknown_return;
  unknown_return.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 7u;
  CHECK(unknown_return.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_return.progress.stopped_pc == UINT32_C(0x80055f3c));
  CHECK(unknown_return.progress.control_writes == 5u);
}

void test_addresses_metadata_and_repeatability() {
  Fixture unknown_pointer;
  unknown_pointer.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
      .known_mask = 14u;
  CHECK(unknown_pointer.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_pointer.progress.operations == 0u);
  CHECK(unknown_pointer.progress.stopped_pc == UINT32_C(0x80055f18));

  Fixture unaligned;
  set_word(unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           kMatrix + 2u);
  CHECK(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  CHECK(unaligned.progress.operations == 1u);

  Fixture unmapped;
  unmapped.context.memory.count = 0u;
  CHECK(unmapped.run() == NBA97_TEXT_RESOURCE);
  CHECK(unmapped.progress.stopped_address == kMatrix);

  Fixture malformed_known;
  malformed_known.known[0] = 2u;
  CHECK(malformed_known.run() == NBA97_TEXT_ARGUMENT);
  CHECK(malformed_known.progress.operations == 1u);

  Fixture malformed_late_byte;
  const Nba97GameGteRotationInstallWord original_t2 =
      malformed_late_byte.context.machine.registers
          .gpr[NBA97_MATCH_INITIALIZE_T0 + 2u];
  malformed_late_byte.known[8u + 3u] = 2u;
  CHECK(malformed_late_byte.run() == NBA97_TEXT_ARGUMENT);
  CHECK(malformed_late_byte.progress.operations == 3u);
  CHECK(malformed_late_byte.progress.reads == 2u);
  CHECK(malformed_late_byte.progress.control_writes == 0u);
  CHECK(malformed_late_byte.progress.machine.registers
            .gpr[NBA97_MATCH_INITIALIZE_T0]
            .word == UINT32_C(0x11223344));
  CHECK(malformed_late_byte.progress.machine.registers
            .gpr[NBA97_MATCH_INITIALIZE_T0 + 1u]
            .word == UINT32_C(0x22334455));
  CHECK(std::memcmp(&malformed_late_byte.progress.machine.registers
                         .gpr[NBA97_MATCH_INITIALIZE_T0 + 2u],
                    &original_t2, sizeof(original_t2)) == 0);

  Fixture malformed_control;
  malformed_control.control[31].known_mask = 16u;
  CHECK(malformed_control.run() == NBA97_TEXT_ARGUMENT);

  Fixture malformed_machine;
  malformed_machine.context.machine.registers.gpr[27].known_mask = 16u;
  CHECK(malformed_machine.run() == NBA97_TEXT_ARGUMENT);

  Fixture malformed_journal;
  malformed_journal.context.access_journal = nullptr;
  CHECK(malformed_journal.run() == NBA97_TEXT_ARGUMENT);

  Fixture malformed_control_journal;
  malformed_control_journal.context.control_journal = nullptr;
  CHECK(malformed_control_journal.run() == NBA97_TEXT_ARGUMENT);

  Fixture null_region;
  null_region.region.data = nullptr;
  CHECK(null_region.run() == NBA97_TEXT_ARGUMENT);

  Fixture overlapping;
  Nba97GameTextRegion overlap_regions[2] = {
      overlapping.region,
      {kMatrix + 16u, overlapping.data + 16u, overlapping.known + 16u, 8u}};
  overlapping.context.memory.region = overlap_regions;
  overlapping.context.memory.count = 2u;
  CHECK(overlapping.run() == NBA97_TEXT_ARGUMENT);

  Fixture first;
  Fixture second;
  CHECK(first.run() == NBA97_TEXT_COMPLETE);
  CHECK(second.run() == NBA97_TEXT_COMPLETE);
  CHECK(std::memcmp(&first.progress.machine, &second.progress.machine,
                    sizeof(first.progress.machine)) == 0);
  CHECK(std::memcmp(first.control, second.control, sizeof(first.control)) == 0);
}

void test_wrapping_and_native_backing_alias() {
  Fixture wrapped;
  uint8_t high[16]{};
  uint8_t low[4]{};
  uint8_t high_known[16];
  uint8_t low_known[4];
  std::memset(high_known, 1, sizeof(high_known));
  std::memset(low_known, 1, sizeof(low_known));
  for (unsigned index = 0u; index != 4u; ++index)
    put32(high, index * 4u, UINT32_C(0x11111111) * (index + 1u));
  put32(low, 0u, UINT32_C(0x55555555));
  Nba97GameTextRegion regions[2] = {
      {UINT32_C(0xfffffff0), high, high_known, sizeof(high)},
      {0u, low, low_known, sizeof(low)}};
  wrapped.context.memory.region = regions;
  wrapped.context.memory.count = 2u;
  set_word(wrapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           UINT32_C(0xfffffff0));
  CHECK(wrapped.run() == NBA97_TEXT_COMPLETE);
  CHECK(wrapped.progress.machine.registers
            .gpr[NBA97_MATCH_INITIALIZE_T0 + 4u]
            .word == UINT32_C(0x55555555));

  Fixture alias;
  uint8_t shared[4] = {1u, 2u, 3u, 4u};
  uint8_t shared_known[4] = {1u, 1u, 1u, 1u};
  Nba97GameTextRegion alias_regions[5];
  for (unsigned index = 0u; index != 5u; ++index) {
    alias_regions[index].base = kMatrix + index * 4u;
    alias_regions[index].data = shared;
    alias_regions[index].known = shared_known;
    alias_regions[index].size = sizeof(shared);
  }
  alias.context.memory.region = alias_regions;
  alias.context.memory.count = 5u;
  CHECK(alias.run() == NBA97_TEXT_COMPLETE);
  for (unsigned index = 0u; index != 5u; ++index)
    CHECK(alias.progress.machine.registers
              .gpr[NBA97_MATCH_INITIALIZE_T0 + index]
              .word == UINT32_C(0x04030201));
}

} // namespace

int main() {
  test_exact_order_and_preservation();
  test_rt33_sign_extension_and_raw_t4();
  test_partial_loads_and_every_budget();
  test_addresses_metadata_and_repeatability();
  test_wrapping_and_native_backing_alias();
  std::printf("game_gte_rotation_install_tests: %zu checks passed\n", checks);
  return 0;
}
