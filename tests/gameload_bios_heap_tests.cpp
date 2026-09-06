#include "recovered/gameload_bios_heap.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

unsigned checks = 0u;

void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("gameload BIOS heap test failed at " +
                             std::to_string(line));
}

#define CHECK(value) checkAt((value), __LINE__)

using Word = Nba97GameloadBiosHeapWord;
using Machine = Nba97GameloadBiosHeapMachine;

void setWord(Word &word, std::uint32_t value, std::uint8_t known) {
  word.word = value;
  word.known_mask = known;
}

bool sameWord(const Word &left, const Word &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool sameMachine(const Machine &left, const Machine &right) {
  for (unsigned index = 0u; index != 32u; ++index)
    if (!sameWord(left.registers.gpr[index], right.registers.gpr[index]))
      return false;
  return sameWord(left.hi, right.hi) && sameWord(left.lo, right.lo);
}

std::uint32_t hashBytes(const std::vector<std::uint8_t> &bytes) {
  std::uint32_t hash = UINT32_C(2166136261);
  for (std::uint8_t byte : bytes) {
    hash ^= byte;
    hash *= UINT32_C(16777619);
  }
  return hash;
}

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(64u);
  std::vector<std::uint8_t> known =
      std::vector<std::uint8_t>(64u, static_cast<std::uint8_t>(1u));
  Nba97GameTextRegion region{UINT32_C(0x80010000), bytes.data(), known.data(),
                             bytes.size()};
  std::array<std::uint32_t, 5u> instructions{
      UINT32_C(0xeeeeeeee), UINT32_C(0xeeeeeeee), UINT32_C(0xeeeeeeee),
      UINT32_C(0xeeeeeeee), UINT32_C(0xeeeeeeee)};
  Nba97GameloadBiosHeapContext context{};
  Nba97GameloadBiosHeapProgress progress{};
  Machine initial{};
  Machine observed{};
  unsigned calls = 0u;
  int callback_result = 1;
  int malformed_word = -1;
  bool mutate = false;
  bool mutate_memory = false;
  bool mutate_zero_word = false;

  Fixture() {
    for (unsigned index = 0u; index != 32u; ++index)
      setWord(initial.registers.gpr[index],
              UINT32_C(0x10203040) + index * UINT32_C(0x01010101),
              static_cast<std::uint8_t>(index & 15u));
    setWord(initial.registers.gpr[0], 0u, 15u);
    setWord(initial.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_A0],
            UINT32_C(0x801eb0a4), 5u);
    setWord(initial.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_A1],
            UINT32_C(0x0060cf58), 10u);
    setWord(initial.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_RA],
            UINT32_C(0x801e14a0), 0u);
    setWord(initial.hi, UINT32_C(0x89abcdef), 3u);
    setWord(initial.lo, UINT32_C(0x76543210), 12u);
    for (std::size_t index = 0u; index != bytes.size(); ++index)
      bytes[index] = static_cast<std::uint8_t>(index * 17u + 3u);
    context.memory = {&region, 1u};
    context.operation_budget = 1u;
    context.machine = initial;
    context.io = callback;
    context.user = this;
    context.instruction_journal = instructions.data();
    context.instruction_journal_capacity = instructions.size();
  }

  static int callback(void *opaque, const Nba97GameTextMemory *memory,
                      const Nba97GameloadBiosHeapEvent *event,
                      Machine *machine) {
    Fixture &fixture = *static_cast<Fixture *>(opaque);
    ++fixture.calls;
    CHECK(memory != nullptr && event != nullptr && machine != nullptr);
    CHECK(event->pc == UINT32_C(0x801e1594) &&
          event->delay_slot_pc == UINT32_C(0x801e1598) &&
          event->entry == UINT32_C(0x000000a0) && event->operation == 1u &&
          event->invocation == 1u &&
          event->site == NBA97_GAMELOAD_BIOS_HEAP_SITE_A0_SERVICE_39 &&
          event->service == 0x39u && event->argument_count == 2u);
    fixture.observed = *machine;
    if (fixture.mutate) {
      for (unsigned index = 1u; index != 32u; ++index) {
        machine->registers.gpr[index].word ^=
            UINT32_C(0x5a000000) + index * UINT32_C(0x00010101);
        machine->registers.gpr[index].known_mask =
            static_cast<std::uint8_t>((15u - index) & 15u);
      }
      setWord(machine->hi, UINT32_C(0xaabbccdd), 6u);
      setWord(machine->lo, UINT32_C(0x11223344), 9u);
    }
    if (fixture.mutate_memory && memory->count != 0u)
      memory->region[0].data[17] ^= 0x5au;
    if (fixture.mutate_memory && memory->count != 0u &&
        memory->region[0].known != nullptr)
      memory->region[0].known[18] ^= 1u;
    if (fixture.malformed_word >= 0) {
      if (fixture.malformed_word < 32)
        machine->registers.gpr[fixture.malformed_word].known_mask = 16u;
      else if (fixture.malformed_word == 32)
        machine->hi.known_mask = 16u;
      else
        machine->lo.known_mask = 16u;
    }
    if (fixture.mutate_zero_word)
      machine->registers.gpr[0].word = 1u;
    return fixture.callback_result;
  }

  int run() { return nba97_gameload_bios_heap(&context, &progress); }
};

void exactTraceAndFullMachineMutation() {
  Fixture fixture;
  fixture.mutate = true;
  fixture.mutate_memory = true;
  const std::uint8_t before_byte = fixture.bytes[17];
  const std::uint8_t before_known = fixture.known[18];
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE && fixture.progress.completed &&
        fixture.progress.operations == 1u &&
        fixture.progress.callbacks_completed == 1u && fixture.calls == 1u &&
        fixture.progress.instruction_count == 3u &&
        fixture.progress.instruction_events == 3u &&
        fixture.progress.stopped_pc == 0u &&
        fixture.progress.stopped_entry == 0u &&
        fixture.progress.stopped_service == 0u);
  CHECK(fixture.instructions[0] == UINT32_C(0x801e1590) &&
        fixture.instructions[1] == UINT32_C(0x801e1594) &&
        fixture.instructions[2] == UINT32_C(0x801e1598) &&
        fixture.instructions[3] == UINT32_C(0xeeeeeeee));
  Machine expected_input = fixture.initial;
  setWord(expected_input.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_T2],
          UINT32_C(0x000000a0), 15u);
  setWord(expected_input.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_T1], 0x39u,
          15u);
  CHECK(sameMachine(fixture.observed, expected_input));
  CHECK(fixture.bytes[17] == static_cast<std::uint8_t>(before_byte ^ 0x5au));
  CHECK(fixture.known[18] == static_cast<std::uint8_t>(before_known ^ 1u));
  Machine expected_output = expected_input;
  for (unsigned index = 1u; index != 32u; ++index) {
    expected_output.registers.gpr[index].word ^=
        UINT32_C(0x5a000000) + index * UINT32_C(0x00010101);
    expected_output.registers.gpr[index].known_mask =
        static_cast<std::uint8_t>((15u - index) & 15u);
  }
  setWord(expected_output.hi, UINT32_C(0xaabbccdd), 6u);
  setWord(expected_output.lo, UINT32_C(0x11223344), 9u);
  CHECK(sameMachine(fixture.progress.machine, expected_output));
}

void unknownArgumentsAndReturnAddress() {
  for (unsigned selected = 0u; selected != 3u; ++selected) {
    for (unsigned mask = 0u; mask != 16u; ++mask) {
      Fixture fixture;
      const unsigned reg = selected == 0u ? NBA97_GAMELOAD_BIOS_HEAP_A0
                           : selected == 1u ? NBA97_GAMELOAD_BIOS_HEAP_A1
                                            : NBA97_GAMELOAD_BIOS_HEAP_RA;
      fixture.context.machine.registers.gpr[reg].known_mask =
          static_cast<std::uint8_t>(mask);
      CHECK(fixture.run() == NBA97_TEXT_COMPLETE && fixture.calls == 1u &&
            fixture.observed.registers.gpr[reg].word ==
                fixture.initial.registers.gpr[reg].word &&
            fixture.observed.registers.gpr[reg].known_mask == mask &&
            fixture.progress.machine.registers.gpr[reg].known_mask == mask);
    }
  }
}

void budgetRefusalAndNoDirectRam() {
  Fixture limited;
  limited.context.operation_budget = 0u;
  const std::uint32_t limited_hash = hashBytes(limited.bytes);
  CHECK(limited.run() == NBA97_TEXT_LIMIT && limited.calls == 0u &&
        limited.progress.operations == 0u &&
        limited.progress.callbacks_completed == 0u &&
        limited.progress.instruction_events == 3u &&
        limited.progress.stopped_pc == UINT32_C(0x801e1594) &&
        limited.progress.stopped_entry == UINT32_C(0x000000a0) &&
        limited.progress.stopped_service == 0x39u &&
        limited.progress.machine.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_T1]
                .word == 0x39u &&
        limited.progress.machine.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_T2]
                .word == 0xa0u &&
        hashBytes(limited.bytes) == limited_hash);

  Fixture absent;
  absent.context.io = nullptr;
  const std::uint32_t absent_hash = hashBytes(absent.bytes);
  CHECK(absent.run() == NBA97_TEXT_IO_REFUSED && absent.calls == 0u &&
        absent.progress.operations == 1u &&
        absent.progress.event.operation == 1u &&
        absent.progress.instruction_events == 3u &&
        hashBytes(absent.bytes) == absent_hash);

  Fixture refused;
  refused.callback_result = 0;
  refused.mutate = true;
  refused.mutate_memory = true;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED && refused.calls == 1u &&
        refused.progress.callbacks_completed == 0u &&
        refused.progress.machine.hi.word == UINT32_C(0xaabbccdd) &&
        refused.bytes[17] != static_cast<std::uint8_t>(17u * 17u + 3u));
  Fixture noncanonical_refusal;
  noncanonical_refusal.callback_result = 2;
  CHECK(noncanonical_refusal.run() == NBA97_TEXT_IO_REFUSED &&
        noncanonical_refusal.calls == 1u &&
        noncanonical_refusal.progress.callbacks_completed == 0u);
}

void instructionJournalPrefixes() {
  for (std::size_t capacity = 0u; capacity != 4u; ++capacity) {
    Fixture fixture;
    fixture.context.instruction_journal =
        capacity == 0u ? nullptr : fixture.instructions.data();
    fixture.context.instruction_journal_capacity = capacity;
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE &&
          fixture.progress.instruction_events == 3u &&
          fixture.progress.instruction_count == 3u);
    const std::array<std::uint32_t, 3u> expected{
        UINT32_C(0x801e1590), UINT32_C(0x801e1594),
        UINT32_C(0x801e1598)};
    for (std::size_t index = 0u; index != capacity && index != 3u; ++index)
      CHECK(fixture.instructions[index] == expected[index]);
    if (capacity < fixture.instructions.size())
      CHECK(fixture.instructions[capacity] == UINT32_C(0xeeeeeeee));
  }
}

void malformedMachineAndApiGuards() {
  for (int malformed = 0; malformed != 34; ++malformed) {
    Fixture fixture;
    fixture.malformed_word = malformed;
    CHECK(fixture.run() == NBA97_TEXT_ARGUMENT && fixture.calls == 1u &&
          fixture.progress.callbacks_completed == 0u);
  }

  Fixture bad_zero_word;
  bad_zero_word.context.machine.registers.gpr[0].word = 1u;
  CHECK(bad_zero_word.run() == NBA97_TEXT_ARGUMENT &&
        bad_zero_word.progress.instruction_events == 0u);
  Fixture bad_zero_mask;
  bad_zero_mask.context.machine.registers.gpr[0].known_mask = 14u;
  CHECK(bad_zero_mask.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_initial_mask;
  bad_initial_mask.context.machine.hi.known_mask = 16u;
  CHECK(bad_initial_mask.run() == NBA97_TEXT_ARGUMENT);
  Fixture changed_zero;
  changed_zero.mutate_zero_word = true;
  CHECK(changed_zero.run() == NBA97_TEXT_ARGUMENT && changed_zero.calls == 1u &&
        changed_zero.progress.machine.registers.gpr[0].word == 1u);
  Fixture bad_journal;
  bad_journal.context.instruction_journal = nullptr;
  bad_journal.context.instruction_journal_capacity = 1u;
  CHECK(bad_journal.run() == NBA97_TEXT_ARGUMENT);
  Nba97GameloadBiosHeapProgress progress{};
  CHECK(nba97_gameload_bios_heap(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
  Fixture null_progress;
  CHECK(nba97_gameload_bios_heap(&null_progress.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}

void optionalKnownnessAndMemoryBounds() {
  Fixture no_plane;
  no_plane.region.known = nullptr;
  CHECK(no_plane.run() == NBA97_TEXT_COMPLETE);

  Fixture opaque_plane;
  opaque_plane.known[7] = 2u;
  CHECK(opaque_plane.run() == NBA97_TEXT_COMPLETE);

  Fixture empty;
  empty.context.memory = {nullptr, 0u};
  CHECK(empty.run() == NBA97_TEXT_COMPLETE);

  Fixture missing_regions;
  missing_regions.context.memory = {nullptr, 1u};
  CHECK(missing_regions.run() == NBA97_TEXT_ARGUMENT);
  Fixture null_data;
  null_data.region.data = nullptr;
  CHECK(null_data.run() == NBA97_TEXT_ARGUMENT);
  Fixture zero_size;
  zero_size.region.size = 0u;
  CHECK(zero_size.run() == NBA97_TEXT_ARGUMENT);
  Fixture wrapping;
  wrapping.region.base = UINT32_C(0xfffffff0);
  wrapping.region.size = 32u;
  CHECK(wrapping.run() == NBA97_TEXT_ARGUMENT);
  Fixture too_large;
  too_large.region.size = static_cast<std::size_t>(UINT64_C(0x100000001));
  CHECK(too_large.run() == NBA97_TEXT_ARGUMENT);

  Fixture overlap;
  std::array<Nba97GameTextRegion, 2u> regions{
      overlap.region,
      Nba97GameTextRegion{UINT32_C(0x80010020), overlap.bytes.data() + 32u,
                          overlap.known.data() + 32u, 16u}};
  overlap.context.memory = {regions.data(), regions.size()};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
}

void deterministicRepeatability() {
  Fixture first;
  Fixture second;
  CHECK(first.run() == NBA97_TEXT_COMPLETE &&
        second.run() == NBA97_TEXT_COMPLETE &&
        sameMachine(first.progress.machine, second.progress.machine) &&
        first.progress.instruction_count == second.progress.instruction_count &&
        hashBytes(first.bytes) == hashBytes(second.bytes));
}

} // namespace

int main() {
  try {
    exactTraceAndFullMachineMutation();
    unknownArgumentsAndReturnAddress();
    budgetRefusalAndNoDirectRam();
    instructionJournalPrefixes();
    malformedMachineAndApiGuards();
    optionalKnownnessAndMemoryBounds();
    deterministicRepeatability();
    std::printf("gameload_bios_heap_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
