#include "recovered/game_bios_memory_copy.h"

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

constexpr uint32_t kBase = UINT32_C(0x80010000);
constexpr uint32_t kDestination = kBase;
constexpr uint32_t kSource = kBase + UINT32_C(0x20);

void set_word(Nba97GameBiosMemoryCopyWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

struct Service {
  Nba97GameBiosMemoryCopyEvent event{};
  Nba97GameBiosMemoryCopyMachine incoming{};
  size_t calls = 0u;
  bool accepted = true;
  bool copy = false;
  bool mutate = false;
  unsigned malformed = 0u;

  static uint8_t *find(const Nba97GameTextMemory &memory, uint32_t address) {
    for (size_t index = 0u; index != memory.count; ++index) {
      const Nba97GameTextRegion &region = memory.region[index];
      if (address >= region.base &&
          static_cast<uint64_t>(address - region.base) < region.size)
        return region.data + (address - region.base);
    }
    return nullptr;
  }

  static int call(void *opaque, const Nba97GameTextMemory *memory,
                  const Nba97GameBiosMemoryCopyEvent *event,
                  Nba97GameBiosMemoryCopyMachine *machine) {
    Service &self = *static_cast<Service *>(opaque);
    ++self.calls;
    self.event = *event;
    self.incoming = *machine;
    if (self.copy) {
      const uint32_t destination =
          machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word;
      const uint32_t source =
          machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word;
      const uint32_t count =
          machine->registers.gpr[NBA97_MATCH_INITIALIZE_A2].word;
      for (uint32_t offset = 0u; offset != count; ++offset) {
        uint8_t *from = find(*memory, source + offset);
        uint8_t *to = find(*memory, destination + offset);
        CHECK(from != nullptr && to != nullptr);
        *to = *from;
      }
    }
    if (self.mutate) {
      set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0],
               UINT32_C(0xcafebabe), 7u);
      set_word(machine->hi, UINT32_C(0x11223344), 3u);
      set_word(machine->lo, UINT32_C(0x55667788), 12u);
      uint8_t *byte = find(*memory, kDestination + 15u);
      CHECK(byte != nullptr);
      *byte = 0x5au;
    }
    if (self.malformed == 1u)
      machine->registers.gpr[0].word = 1u;
    else if (self.malformed == 2u)
      machine->registers.gpr[17].known_mask = 16u;
    else if (self.malformed == 3u)
      machine->hi.known_mask = 16u;
    else if (self.malformed == 4u)
      machine->lo.known_mask = 16u;
    return self.accepted ? 1 : 0;
  }
};

struct Fixture {
  uint8_t bytes[64]{};
  uint8_t known[64]{};
  Nba97GameTextRegion region{};
  Service service{};
  Nba97GameBiosMemoryCopyContext context{};
  Nba97GameBiosMemoryCopyProgress progress{};

  Fixture() {
    std::memset(known, 1, sizeof(known));
    for (unsigned index = 0u; index != 16u; ++index)
      bytes[0x20u + index] = static_cast<uint8_t>(0x80u + index);
    region.base = kBase;
    region.data = bytes;
    region.known = known;
    region.size = sizeof(bytes);
    context.memory.region = &region;
    context.memory.count = 1u;
    context.operation_budget = 1u;
    context.io = Service::call;
    context.user = &service;
    for (unsigned reg = 0u; reg != 32u; ++reg)
      set_word(context.machine.registers.gpr[reg],
               UINT32_C(0x20000000) + reg * UINT32_C(0x01010101),
               static_cast<uint8_t>(reg % 16u));
    set_word(context.machine.registers.gpr[0], 0u);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
             kDestination);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1], kSource);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2], 8u);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x81234560));
    set_word(context.machine.hi, UINT32_C(0x12345678), 5u);
    set_word(context.machine.lo, UINT32_C(0x9abcdef0), 10u);
  }

  int run() { return nba97_game_bios_memory_copy(&context, &progress); }
};

void test_exact_tail_event_and_machine() {
  Fixture fixture;
  const Nba97GameBiosMemoryCopyMachine before = fixture.context.machine;
  fixture.service.copy = true;
  fixture.service.mutate = true;
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
  CHECK(fixture.progress.completed == 1u);
  CHECK(fixture.progress.operations == 1u);
  CHECK(fixture.progress.callbacks_completed == 1u);
  CHECK(fixture.service.calls == 1u);
  CHECK(fixture.service.event.pc == UINT32_C(0x8009cb10));
  CHECK(fixture.service.event.delay_slot_pc == UINT32_C(0x8009cb14));
  CHECK(fixture.service.event.entry == UINT32_C(0x000000a0));
  CHECK(fixture.service.event.service == 0x2au);
  CHECK(fixture.service.event.argument_count == 3u);
  CHECK(fixture.service.event.operation == 1u);
  CHECK(fixture.service.event.invocation == 1u);
  CHECK(fixture.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 2u]
            .word == UINT32_C(0x000000a0));
  CHECK(fixture.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 2u]
            .known_mask == 15u);
  CHECK(fixture.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 1u]
            .word == 0x2au);
  CHECK(fixture.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 1u]
            .known_mask == 15u);
  for (unsigned reg = 0u; reg != 32u; ++reg) {
    if (reg == NBA97_MATCH_INITIALIZE_T0 + 1u ||
        reg == NBA97_MATCH_INITIALIZE_T0 + 2u)
      continue;
    CHECK(fixture.service.incoming.registers.gpr[reg].word ==
          before.registers.gpr[reg].word);
    CHECK(fixture.service.incoming.registers.gpr[reg].known_mask ==
          before.registers.gpr[reg].known_mask);
  }
  CHECK(fixture.service.incoming.hi.word == before.hi.word);
  CHECK(fixture.service.incoming.hi.known_mask == before.hi.known_mask);
  CHECK(fixture.service.incoming.lo.word == before.lo.word);
  CHECK(fixture.service.incoming.lo.known_mask == before.lo.known_mask);
  for (unsigned index = 0u; index != 8u; ++index)
    CHECK(fixture.bytes[index] == static_cast<uint8_t>(0x80u + index));
  CHECK(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
      UINT32_C(0xcafebabe));
  CHECK(fixture.progress.machine.hi.word == UINT32_C(0x11223344));
  CHECK(fixture.progress.machine.lo.word == UINT32_C(0x55667788));
  CHECK(fixture.bytes[15] == 0x5au);
}

void test_overwritten_temporaries_and_unknown_arguments() {
  for (uint8_t mask = 0u; mask != 16u; ++mask) {
    Fixture fixture;
    set_word(
        fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 1u],
        UINT32_C(0xdeadbeef), mask);
    set_word(
        fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 2u],
        UINT32_C(0xabcdef01), static_cast<uint8_t>(15u - mask));
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    CHECK(fixture.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 1u]
              .word == 0x2au);
    CHECK(fixture.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 1u]
              .known_mask == 15u);
    CHECK(fixture.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 2u]
              .word == 0xa0u);
    CHECK(fixture.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 2u]
              .known_mask == 15u);
  }

  Fixture unknown;
  set_word(unknown.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           UINT32_C(0xfffffffc), 0u);
  set_word(unknown.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
           UINT32_C(0x00000004), 3u);
  set_word(unknown.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2],
           UINT32_C(0xffffffff), 8u);
  set_word(unknown.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
           UINT32_C(0x00000000), 0u);
  CHECK(unknown.run() == NBA97_TEXT_COMPLETE);
  CHECK(
      unknown.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
      UINT32_C(0xfffffffc));
  CHECK(unknown.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
            .known_mask == 0u);
  CHECK(
      unknown.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
      4u);
  CHECK(
      unknown.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
      UINT32_C(0xffffffff));
  CHECK(unknown.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
            .known_mask == 0u);

  Fixture zero;
  set_word(zero.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2], 0u,
           0u);
  CHECK(zero.run() == NBA97_TEXT_COMPLETE);
  CHECK(zero.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
        0u);
  CHECK(zero.service.incoming.registers.gpr[NBA97_MATCH_INITIALIZE_A2]
            .known_mask == 0u);
}

void test_budget_refusal_and_mutated_prefixes() {
  Fixture bounded;
  bounded.context.operation_budget = 0u;
  CHECK(bounded.run() == NBA97_TEXT_LIMIT);
  CHECK(bounded.progress.operations == 0u);
  CHECK(bounded.progress.stopped_pc == UINT32_C(0x8009cb10));
  CHECK(bounded.progress.stopped_entry == UINT32_C(0x000000a0));
  CHECK(bounded.progress.stopped_service == 0x2au);
  CHECK(bounded.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 1u]
            .word == 0x2au);
  CHECK(bounded.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 2u]
            .word == 0xa0u);
  CHECK(bounded.service.calls == 0u);

  Fixture missing;
  missing.context.io = nullptr;
  CHECK(missing.run() == NBA97_TEXT_IO_REFUSED);
  CHECK(missing.progress.operations == 1u);
  CHECK(missing.progress.callbacks_completed == 0u);
  CHECK(missing.progress.event.service == 0x2au);

  Fixture refused;
  refused.service.accepted = false;
  refused.service.mutate = true;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED);
  CHECK(refused.progress.callbacks_completed == 0u);
  CHECK(
      refused.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
      UINT32_C(0xcafebabe));
  CHECK(refused.progress.machine.hi.word == UINT32_C(0x11223344));
  CHECK(refused.bytes[15] == 0x5au);
}

void test_accepted_malformed_machine() {
  for (unsigned malformed = 1u; malformed != 5u; ++malformed) {
    Fixture fixture;
    fixture.service.malformed = malformed;
    CHECK(fixture.run() == NBA97_TEXT_ARGUMENT);
    CHECK(fixture.progress.operations == 1u);
    CHECK(fixture.progress.callbacks_completed == 0u);
    if (malformed == 1u)
      CHECK(fixture.progress.machine.registers.gpr[0].word == 1u);
    else if (malformed == 2u)
      CHECK(fixture.progress.machine.registers.gpr[17].known_mask == 16u);
    else if (malformed == 3u)
      CHECK(fixture.progress.machine.hi.known_mask == 16u);
    else
      CHECK(fixture.progress.machine.lo.known_mask == 16u);
  }
}

void test_invalid_context_and_repeatability() {
  Fixture invalid_gpr;
  invalid_gpr.context.machine.registers.gpr[18].known_mask = 16u;
  CHECK(invalid_gpr.run() == NBA97_TEXT_ARGUMENT);
  CHECK(invalid_gpr.service.calls == 0u);
  Fixture invalid_hi;
  invalid_hi.context.machine.hi.known_mask = 16u;
  CHECK(invalid_hi.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalid_zero;
  invalid_zero.context.machine.registers.gpr[0].word = 1u;
  CHECK(invalid_zero.run() == NBA97_TEXT_ARGUMENT);
  Fixture missing_regions;
  missing_regions.context.memory.region = nullptr;
  CHECK(missing_regions.run() == NBA97_TEXT_ARGUMENT);
  Fixture null_data;
  null_data.region.data = nullptr;
  CHECK(null_data.run() == NBA97_TEXT_ARGUMENT);
  Fixture empty_region;
  empty_region.region.size = 0u;
  CHECK(empty_region.run() == NBA97_TEXT_ARGUMENT);
  Fixture overflow;
  overflow.region.base = UINT32_C(0xfffffff0);
  overflow.region.size = 32u;
  CHECK(overflow.run() == NBA97_TEXT_ARGUMENT);
  Fixture overlap;
  Nba97GameTextRegion regions[2] = {
      overlap.region,
      {kBase + 16u, overlap.bytes + 16u, overlap.known + 16u, 16u}};
  overlap.context.memory.region = regions;
  overlap.context.memory.count = 2u;
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture nulls;
  CHECK(nba97_game_bios_memory_copy(nullptr, &nulls.progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_bios_memory_copy(&nulls.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);

  Fixture first;
  Fixture second;
  CHECK(first.run() == NBA97_TEXT_COMPLETE);
  CHECK(second.run() == NBA97_TEXT_COMPLETE);
  CHECK(first.progress.operations == second.progress.operations);
  CHECK(first.progress.callbacks_completed ==
        second.progress.callbacks_completed);
  for (unsigned reg = 0u; reg != 32u; ++reg) {
    CHECK(first.progress.machine.registers.gpr[reg].word ==
          second.progress.machine.registers.gpr[reg].word);
    CHECK(first.progress.machine.registers.gpr[reg].known_mask ==
          second.progress.machine.registers.gpr[reg].known_mask);
  }
}

} // namespace

int main() {
  test_exact_tail_event_and_machine();
  test_overwritten_temporaries_and_unknown_arguments();
  test_budget_refusal_and_mutated_prefixes();
  test_accepted_malformed_machine();
  test_invalid_context_and_repeatability();
  std::printf("game_bios_memory_copy_tests: %zu checks passed\n", checks);
  return 0;
}
