#include "gameload_bios_heap_adapter.h"
#include "gameload_bios_heap_capture.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using U = std::uint32_t;
using Word = Nba97GameloadBiosHeapWord;
using Machine = Nba97GameloadBiosHeapMachine;

unsigned checks = 0u;

void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("gameload BIOS heap integration failed at " +
                             std::to_string(line));
}

#define CHECK(value) checkAt((value), __LINE__)

void setWord(Word &word, U value, std::uint8_t mask = 15u) {
  word.word = value;
  word.known_mask = mask;
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

struct Fixture {
  static constexpr U Base = UINT32_C(0x80000000);
  static constexpr U Size = UINT32_C(0x00200000);
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0u);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1u);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::vector<Nba97GameloadEntryAccess> entry_access =
      std::vector<Nba97GameloadEntryAccess>(2100u);
  std::vector<U> entry_pc = std::vector<U>(10500u);
  std::array<U, 4u> heap_pc{};
  Nba97GameloadEntryContext entry{};
  Nba97GameloadEntryProgress entry_progress{};
  Nba97GameloadBiosHeapBinding heap{};
  Nba97GameloadBiosHeapAdapterProgress adapter{};
  Machine heap_observed{};
  Machine second_observed{};
  unsigned heap_calls = 0u;
  unsigned second_calls = 0u;
  bool refuse_heap = false;
  bool mutate_heap = false;
  bool mutate_memory = false;
  bool malformed_heap = false;
  bool transfer_second = false;

  Fixture() {
    entry.memory = {&region, 1u};
    entry.operation_budget = 2081u;
    for (unsigned index = 0u; index != 32u; ++index)
      setWord(entry.machine.registers.gpr[index],
              UINT32_C(0x31000000) + index * UINT32_C(0x00010307),
              static_cast<std::uint8_t>(index & 15u));
    setWord(entry.machine.registers.gpr[0], 0u);
    setWord(entry.machine.registers.gpr[NBA97_GAMELOAD_ENTRY_RA],
            UINT32_C(0x80028b70));
    setWord(entry.machine.hi, UINT32_C(0x11223344), 5u);
    setWord(entry.machine.lo, UINT32_C(0x55667788), 10u);
    entry.io = child;
    entry.user = this;
    entry.access_journal = entry_access.data();
    entry.access_journal_capacity = entry_access.size();
    entry.instruction_journal = entry_pc.data();
    entry.instruction_journal_capacity = entry_pc.size();
    put(UINT32_C(0x801e8b70), UINT32_C(0x00800000));
    put(UINT32_C(0x801e8b6c), UINT32_C(0x00008000));
    put(UINT32_C(0x80010000), UINT32_C(0xa5a55a5a));
    nba97_gameload_bios_heap_binding_init(
        &heap, 1u, bios, this, heap_pc.data(), heap_pc.size());
  }

  void put(U address, U value) {
    if (address < Base || address - Base > Size - 4u)
      throw std::runtime_error("integration fixture write outside RAM");
    for (unsigned index = 0u; index != 4u; ++index)
      bytes[address - Base + index] =
          static_cast<std::uint8_t>(value >> (index * 8u));
  }

  U get(U address) const {
    if (address < Base || address - Base > Size - 4u)
      throw std::runtime_error("integration fixture read outside RAM");
    U value = 0u;
    for (unsigned index = 0u; index != 4u; ++index)
      value |= static_cast<U>(bytes[address - Base + index]) << (index * 8u);
    return value;
  }

  static int bios(void *opaque, const Nba97GameTextMemory *memory,
                  const Nba97GameloadBiosHeapEvent *event,
                  Machine *machine) {
    Fixture &fixture = *static_cast<Fixture *>(opaque);
    ++fixture.heap_calls;
    CHECK(memory != nullptr && event != nullptr && machine != nullptr &&
          event->site == NBA97_GAMELOAD_BIOS_HEAP_SITE_A0_SERVICE_39 &&
          event->pc == UINT32_C(0x801e1594) &&
          event->delay_slot_pc == UINT32_C(0x801e1598) &&
          event->entry == UINT32_C(0x000000a0) && event->service == 0x39u &&
          event->argument_count == 2u && event->operation == 1u);
    fixture.heap_observed = *machine;
    if (fixture.mutate_heap) {
      for (unsigned index = 1u; index != 32u; ++index) {
        machine->registers.gpr[index].word ^=
            UINT32_C(0x66000000) + index * UINT32_C(0x00001111);
        machine->registers.gpr[index].known_mask =
            static_cast<std::uint8_t>((index * 3u) & 15u);
      }
      setWord(machine->hi, UINT32_C(0x0badcafe), 6u);
      setWord(machine->lo, UINT32_C(0xdec0adde), 9u);
    }
    if (fixture.mutate_memory)
      fixture.put(UINT32_C(0x80010000), UINT32_C(0x13579bdf));
    if (fixture.mutate_memory && memory->region[0].known != nullptr)
      memory->region[0].known[UINT32_C(0x00010004)] = 0u;
    if (fixture.malformed_heap)
      machine->lo.known_mask = 16u;
    return fixture.refuse_heap ? 0 : 1;
  }

  static int child(void *opaque, const Nba97GameTextMemory *,
                   const Nba97GameloadEntryEvent *event,
                   Nba97GameloadEntryMachine *machine,
                   Nba97GameloadEntryCalleeOutcome *outcome) {
    Fixture &fixture = *static_cast<Fixture *>(opaque);
    CHECK(event != nullptr && machine != nullptr && outcome != nullptr);
    if (event->site != NBA97_GAMELOAD_ENTRY_SITE_801E14AC)
      return 0;
    ++fixture.second_calls;
    fixture.second_observed = *machine;
    *outcome = fixture.transfer_second
                   ? NBA97_GAMELOAD_ENTRY_CALLEE_TRANSFERRED
                   : NBA97_GAMELOAD_ENTRY_CALLEE_RETURNED;
    return 1;
  }

  int run() {
    return nba97_gameload_entry_with_recovered_bios_heap(
        &entry, &heap, &entry_progress, &adapter);
  }
};

void naturalReturnedHeapThenBreak() {
  for (bool plane : {true, false}) {
    Fixture fixture;
    if (!plane)
      fixture.region.known = nullptr;
    CHECK(fixture.run() == NBA97_GAMELOAD_ENTRY_BREAK_TRAP &&
          fixture.entry_progress.trapped && !fixture.entry_progress.completed &&
          fixture.heap.invocations == 1u && fixture.heap.completions == 1u &&
          fixture.adapter.invocations == 1u &&
          fixture.adapter.completions == 1u && fixture.heap_calls == 1u &&
          fixture.second_calls == 1u);
    CHECK(fixture.entry_progress.machine.registers.gpr[NBA97_GAMELOAD_ENTRY_SP]
                  .word == UINT32_C(0x807ffff8) &&
          fixture.heap.parent_event.pc == UINT32_C(0x801e1498) &&
          fixture.heap.parent_event.delay_slot_pc == UINT32_C(0x801e149c) &&
          fixture.heap.parent_event.entry == UINT32_C(0x801e1590) &&
          fixture.heap.parent_event.operation == 2079u &&
          fixture.heap.parent_event.argument_count == 2u);
    CHECK(fixture.heap_observed.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_A0]
                  .word == UINT32_C(0x801eb0a4) &&
          fixture.heap_observed.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_A1]
                  .word == UINT32_C(0x0060cf58) &&
          fixture.heap_observed.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_RA]
                  .word == UINT32_C(0x801e14a0) &&
          fixture.heap_observed.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_T1]
                  .word == 0x39u &&
          fixture.heap_observed.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_T2]
                  .word == 0xa0u);
    CHECK(fixture.heap.progress.instruction_count == 3u &&
          fixture.heap_pc[0] == UINT32_C(0x801e1590) &&
          fixture.heap_pc[1] == UINT32_C(0x801e1594) &&
          fixture.heap_pc[2] == UINT32_C(0x801e1598) &&
          sameMachine(fixture.heap.progress.machine, fixture.heap_observed));
  }
}

void fullMachineAndRamMutationReachSecondChild() {
  Fixture fixture;
  fixture.mutate_heap = true;
  fixture.mutate_memory = true;
  CHECK(fixture.run() == NBA97_GAMELOAD_ENTRY_BREAK_TRAP &&
        fixture.get(UINT32_C(0x80010000)) == UINT32_C(0x13579bdf) &&
        fixture.known[UINT32_C(0x00010004)] == 0u);
  Machine expected = fixture.heap_observed;
  for (unsigned index = 1u; index != 32u; ++index) {
    expected.registers.gpr[index].word ^=
        UINT32_C(0x66000000) + index * UINT32_C(0x00001111);
    expected.registers.gpr[index].known_mask =
        static_cast<std::uint8_t>((index * 3u) & 15u);
  }
  setWord(expected.hi, UINT32_C(0x0badcafe), 6u);
  setWord(expected.lo, UINT32_C(0xdec0adde), 9u);
  CHECK(sameMachine(fixture.heap.progress.machine, expected));
  setWord(expected.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_RA],
          UINT32_C(0x801e14b4), 15u);
  CHECK(sameMachine(fixture.second_observed, expected));
}

void heapFailurePrefixesAndSecondTransfer() {
  Fixture limited;
  limited.heap.operation_budget = 0u;
  CHECK(limited.run() == NBA97_TEXT_LIMIT && limited.heap_calls == 0u &&
        limited.second_calls == 0u && limited.heap.progress.operations == 0u &&
        limited.heap.progress.instruction_events == 3u &&
        limited.heap.progress.stopped_pc == UINT32_C(0x801e1594) &&
        limited.entry_progress.operations == 2079u);

  Fixture refused;
  refused.refuse_heap = true;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED && refused.heap_calls == 1u &&
        refused.second_calls == 0u && refused.heap.progress.operations == 1u &&
        refused.heap.progress.callbacks_completed == 0u);

  Fixture malformed;
  malformed.malformed_heap = true;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT && malformed.heap_calls == 1u &&
        malformed.second_calls == 0u &&
        malformed.heap.progress.machine.lo.known_mask == 16u);

  Fixture transferred;
  transferred.transfer_second = true;
  CHECK(transferred.run() == NBA97_TEXT_COMPLETE &&
        transferred.entry_progress.completed &&
        transferred.entry_progress.transferred &&
        !transferred.entry_progress.trapped && transferred.heap_calls == 1u &&
        transferred.second_calls == 1u);
}

void adapterContractAndGuards() {
  Nba97GameloadBiosHeapParentContract contract{};
  CHECK(nba97_gameload_bios_heap_parent_contract(&contract) == 1 &&
        contract.pc == UINT32_C(0x801e1498) &&
        contract.delay_slot_pc == UINT32_C(0x801e149c) &&
        contract.target == UINT32_C(0x801e1590) &&
        contract.return_address == UINT32_C(0x801e14a0) &&
        contract.operation == 2079u && contract.invocation == 1u &&
        contract.site == NBA97_GAMELOAD_ENTRY_SITE_801E1498 &&
        contract.argument_count == 2u &&
        contract.target_program == NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD);
  CHECK(nba97_gameload_bios_heap_parent_contract(nullptr) == 0);

  Fixture base;
  Nba97GameloadEntryEvent event{
      contract.pc, contract.delay_slot_pc, contract.target, contract.operation,
      contract.invocation, contract.site, contract.argument_count,
      contract.target_program};
  Machine machine = base.entry.machine;
  setWord(machine.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_RA],
          contract.return_address, 15u);
  machine.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_A0].known_mask = 0u;
  machine.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_A1].known_mask = 0u;
  Nba97GameloadEntryCalleeOutcome outcome =
      NBA97_GAMELOAD_ENTRY_CALLEE_TRANSFERRED;
  CHECK(nba97_gameload_bios_heap_from_gameload_entry(
            &base.heap, &base.entry.memory, &event, &machine, &outcome) == 1 &&
        outcome == NBA97_GAMELOAD_ENTRY_CALLEE_RETURNED &&
        base.heap.invocations == 1u && base.heap.completions == 1u &&
        base.heap_observed.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_A0]
                .known_mask == 0u &&
        base.heap_observed.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_A1]
                .known_mask == 0u);

  for (unsigned field = 0u; field != 11u; ++field) {
    Fixture invalid;
    Nba97GameloadEntryEvent bad = event;
    Machine bad_machine = machine;
    if (field == 0u)
      bad.pc ^= 4u;
    else if (field == 1u)
      bad.delay_slot_pc ^= 4u;
    else if (field == 2u)
      bad.entry ^= 4u;
    else if (field == 3u)
      ++bad.operation;
    else if (field == 4u)
      ++bad.invocation;
    else if (field == 5u)
      bad.site = NBA97_GAMELOAD_ENTRY_SITE_NONE;
    else if (field == 6u)
      ++bad.argument_count;
    else if (field == 7u)
      bad.target_program = NBA97_FRONTEND_MAIN_PROGRAM_FEONLY;
    else if (field == 8u)
      bad_machine.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_RA].word ^= 4u;
    else if (field == 9u)
      bad_machine.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_RA].known_mask = 14u;
    else
      bad_machine.hi.known_mask = 16u;
    outcome = NBA97_GAMELOAD_ENTRY_CALLEE_TRANSFERRED;
    CHECK(nba97_gameload_bios_heap_from_gameload_entry(
              &invalid.heap, &invalid.entry.memory, &bad, &bad_machine,
              &outcome) == 0 &&
          invalid.heap.invocations == 0u &&
          invalid.heap.result == NBA97_TEXT_ARGUMENT &&
          invalid.heap_calls == 0u);
  }

  Fixture bad_journal;
  bad_journal.heap.instruction_journal = nullptr;
  bad_journal.heap.instruction_journal_capacity = 1u;
  machine = bad_journal.entry.machine;
  setWord(machine.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_RA],
          contract.return_address, 15u);
  CHECK(nba97_gameload_bios_heap_from_gameload_entry(
            &bad_journal.heap, &bad_journal.entry.memory, &event, &machine,
            &outcome) == 0 &&
        bad_journal.heap.result == NBA97_TEXT_ARGUMENT);
}

void captureSmoke() {
  const std::string first = nba97::captureGameloadBiosHeap();
  const std::string second = nba97::captureGameloadBiosHeap();
  CHECK(first == second && !first.empty() && first.front() == '{' &&
        first.back() == '}');
  CHECK(std::all_of(first.begin(), first.end(), [](unsigned char byte) {
    return std::isprint(byte) != 0;
  }));
  CHECK(first.find("\"program\":\"GAMELOAD\"") != std::string::npos &&
        first.find("\"address\":\"0x801E1590\"") != std::string::npos &&
        first.find("\"parent_result\":-5,\"parent_completed\":0,\"parent_transferred\":0") != std::string::npos &&
        first.find("\"bytes\":12,\"instructions\":3") !=
            std::string::npos &&
        first.find("\"pc_sequence\":[2149455248,2149455252,2149455256]") !=
            std::string::npos &&
        first.find("\"callback_machine_words\":34") != std::string::npos &&
        first.find("\"final_machine_words\":34") != std::string::npos &&
        first.find("\"gameplay_shown\":\"BLOCKED\"") !=
            std::string::npos &&
        first.find("BIOS A0:39 host full-state service remains unbound") !=
            std::string::npos);
}

} // namespace

int main() {
  try {
    naturalReturnedHeapThenBreak();
    fullMachineAndRamMutationReachSecondChild();
    heapFailurePrefixesAndSecondTransfer();
    adapterContractAndGuards();
    captureSmoke();
    std::printf("gameload_bios_heap_integration_tests: PASS (%u checks)\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
