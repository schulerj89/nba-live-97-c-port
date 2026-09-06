#include "recovered/gameload_entry.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("gameload-entry failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U StackTop = 0x801f0000u;
constexpr U HeapReserve = 0x00008000u;
constexpr U EntryRa = 0x80028b70u;

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0x5a);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97GameloadEntryContext context{};
  Nba97GameloadEntryProgress progress{};
  std::vector<Nba97GameloadEntryAccess> access =
      std::vector<Nba97GameloadEntryAccess>(2100);
  std::vector<U> instructions = std::vector<U>(10500);
  std::array<Nba97GameloadEntryEvent, 2> events{};
  std::array<Nba97GameloadEntryMachine, 2> machines{};
  unsigned calls = 0;
  bool refuse = false;
  bool malformed = false;
  unsigned transfer_call = 0;
  bool mutate_saved_ra = false;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {
          0x33000000u + i * 0x10101u, std::uint8_t((i % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[31] = {EntryRa, 15};
    context.machine.hi = {0x10203040u, 5};
    context.machine.lo = {0x50607080u, 10};
    put(0x801e8b70u, StackTop);
    put(0x801e8b6cu, HeapReserve);
    context.memory = {&region, 1};
    context.operation_budget = 2081;
    context.io = callback;
    context.user = this;
    context.access_journal = access.data();
    context.access_journal_capacity = access.size();
    context.instruction_journal = instructions.data();
    context.instruction_journal_capacity = instructions.size();
  }

  bool extent(U address, U width = 4) const {
    return address >= Base && width <= Size && address - Base <= Size - width;
  }
  void put(U address, U value, std::uint8_t mask = 15) {
    if (!extent(address)) throw std::runtime_error("fixture write outside RAM");
    for (unsigned i = 0; i < 4; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
    }
  }
  U get(U address) const {
    if (!extent(address)) throw std::runtime_error("fixture read outside RAM");
    U value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }
  int run() { return nba97_gameload_entry(&context, &progress); }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameloadEntryEvent *event,
                      Nba97GameloadEntryMachine *machine,
                      Nba97GameloadEntryCalleeOutcome *outcome) {
    auto &f = *static_cast<Fixture *>(opaque);
    const unsigned index = f.calls++;
    if (!event || !machine || !outcome || index >= 2) return 0;
    f.events[index] = *event;
    f.machines[index] = *machine;
    if (index == 0 && f.mutate_saved_ra)
      f.put(0x801e903cu, 0x81234564u, 5);
    machine->registers.gpr[9] = {0xaabb0000u + index, std::uint8_t(3u + index)};
    machine->hi = {0x11223344u + index, 6};
    machine->lo = {0x55667788u + index, 9};
    if (f.malformed) machine->registers.gpr[10].known_mask = 16;
    *outcome = f.transfer_call == index + 1
                   ? NBA97_GAMELOAD_ENTRY_CALLEE_TRANSFERRED
                   : NBA97_GAMELOAD_ENTRY_CALLEE_RETURNED;
    return f.refuse ? 0 : 1;
  }
};

void normalBreakAndExactTrace() {
  Fixture f;
  CHECK(f.run() == NBA97_GAMELOAD_ENTRY_BREAK_TRAP && f.progress.trapped &&
        !f.progress.completed && f.calls == 2 &&
        f.progress.operations == 2081 && f.progress.accesses == 2079 &&
        f.progress.reads == 3 && f.progress.stores == 2076 &&
        f.progress.words_cleared == 2073 &&
        f.progress.callbacks_completed == 2 &&
        f.progress.instruction_count == 10402 &&
        f.progress.instruction_events == 10402 &&
        f.progress.stopped_pc == 0x801e14b4u);
  CHECK(f.get(0x801e903cu) == EntryRa &&
        f.get(0x801eb09cu) == 0 && f.get(0x801e8b4cu) == 0x801eb0a0u &&
        f.get(0x801e8b50u) == 0x7fffcf58u);
  CHECK(f.progress.loaded_stack_top.word == StackTop &&
        f.progress.loaded_heap_reserve.word == HeapReserve &&
        f.progress.heap_base.word == 0x801eb0a0u &&
        f.progress.heap_size.word == 0x7fffcf58u &&
        f.progress.saved_return_address.word == EntryRa &&
        f.progress.restored_return_address.word == EntryRa);
  CHECK(f.events[0].pc == 0x801e1498u &&
        f.events[0].delay_slot_pc == 0x801e149cu &&
        f.events[0].entry == 0x801e1590u &&
        f.events[0].operation == 2079 && f.events[0].invocation == 1 &&
        f.events[0].argument_count == 2 &&
        f.machines[0].registers.gpr[4].word == 0x801eb0a4u &&
        f.machines[0].registers.gpr[5].word == 0x7fffcf58u &&
        f.machines[0].registers.gpr[31].word == 0x801e14a0u);
  CHECK(f.events[1].pc == 0x801e14acu &&
        f.events[1].delay_slot_pc == 0x801e14b0u &&
        f.events[1].entry == 0x801e136cu &&
        f.events[1].operation == 2081 && f.events[1].argument_count == 0 &&
        f.machines[1].registers.gpr[31].word == 0x801e14b4u);
  CHECK(f.instructions[0] == 0x801e1410u &&
        f.instructions[4] == 0x801e1420u &&
        f.instructions[10368] == 0x801e1430u &&
        f.instructions[10369] == 0x801e1434u &&
        f.instructions[10401] == 0x801e14b4u);
  CHECK(f.access[0].address == 0x801e903cu &&
        f.access[2072].address == 0x801eb09cu &&
        f.access[2073].pc == 0x801e1438u &&
        f.access[2078].pc == 0x801e14a4u);
  CHECK(f.progress.machine.registers.gpr[9].word == 0xaabb0001u &&
        f.progress.machine.hi.word == 0x11223345u &&
        f.progress.machine.lo.word == 0x55667789u);
}

void knownnessArithmeticAndTraps() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f;
    f.context.machine.registers.gpr[31].known_mask = std::uint8_t(mask);
    const int expected = mask == 15 ? NBA97_GAMELOAD_ENTRY_BREAK_TRAP
                                    : NBA97_TEXT_COMPLETE;
    if (mask != 15) f.transfer_call = 1;
    CHECK(f.run() == expected && f.get(0x801e903cu) == EntryRa &&
          f.access[2077].known_mask == mask);
  }

  Fixture overflow;
  overflow.put(0x801e8b70u, 0x80000004u);
  CHECK(overflow.run() == NBA97_GAMELOAD_ENTRY_ARITHMETIC_TRAP &&
        overflow.progress.trapped && overflow.progress.operations == 2074 &&
        overflow.progress.stopped_pc == 0x801e1440u && overflow.calls == 0);
  Fixture ambiguous;
  ambiguous.put(0x801e8b70u, 0x80000008u, 14);
  CHECK(ambiguous.run() == NBA97_TEXT_UNKNOWN &&
        ambiguous.progress.stopped_pc == 0x801e1440u &&
        ambiguous.progress.machine.registers.gpr[2].known_mask == 14);
  Fixture safe_partial;
  safe_partial.put(0x801e8b70u, 0x81000000u, 8);
  safe_partial.transfer_call = 1;
  CHECK(safe_partial.run() == NBA97_TEXT_COMPLETE &&
        safe_partial.progress.transferred &&
        safe_partial.progress.machine.registers.gpr[29].known_mask == 0);

  for (unsigned left_mask = 0; left_mask < 16; ++left_mask) {
    Fixture f;
    f.put(0x801e8b70u, StackTop, std::uint8_t(left_mask));
    f.put(0x801e8b6cu, HeapReserve, std::uint8_t(15u - left_mask));
    if (left_mask != 15 && (left_mask & 8u)) {
      f.transfer_call = 1;
      const int result = f.run();
      CHECK(result == (left_mask < 12 ? NBA97_TEXT_UNKNOWN : NBA97_TEXT_COMPLETE));
    }
  }
}

void transfersMutationAndRefusal() {
  Fixture first;
  first.transfer_call = 1;
  CHECK(first.run() == NBA97_TEXT_COMPLETE && first.progress.completed &&
        first.progress.transferred && first.calls == 1 &&
        first.progress.callbacks_completed == 1 &&
        first.progress.operations == 2079 &&
        first.progress.machine.registers.gpr[9].word == 0xaabb0000u);
  Fixture second;
  second.transfer_call = 2;
  second.mutate_saved_ra = true;
  CHECK(second.run() == NBA97_TEXT_COMPLETE && second.progress.transferred &&
        second.calls == 2 && second.progress.restored_return_address.word ==
                                 0x81234564u &&
        second.progress.restored_return_address.known_mask == 5 &&
        second.machines[1].registers.gpr[31].word == 0x801e14b4u);
  Fixture refused;
  refused.refuse = true;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED && refused.calls == 1 &&
        refused.progress.operations == 2079 &&
        refused.progress.callbacks_completed == 0 &&
        refused.progress.machine.registers.gpr[9].word == 0xaabb0000u &&
        refused.progress.stopped_pc == 0x801e1498u);
  Fixture malformed;
  malformed.malformed = true;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT && malformed.calls == 1 &&
        malformed.progress.machine.registers.gpr[10].known_mask == 16);
}

void budgetsMemoryAndAliases() {
  for (unsigned budget = 0; budget < 2081; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    CHECK(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget &&
          !f.progress.completed);
  }
  Fixture missing_clear;
  missing_clear.region.size = 0x1e903cu;
  CHECK(missing_clear.run() == NBA97_TEXT_RESOURCE &&
        missing_clear.progress.operations == 1 &&
        missing_clear.progress.stopped_pc == 0x801e1420u);
  Fixture malformed_clear;
  malformed_clear.known[0x1e903cu + 2] = 2;
  CHECK(malformed_clear.run() == NBA97_TEXT_ARGUMENT &&
        malformed_clear.progress.operations == 1 &&
        malformed_clear.progress.stores == 0);
  Fixture absent;
  absent.region.known = nullptr;
  CHECK(absent.run() == NBA97_GAMELOAD_ENTRY_BREAK_TRAP &&
        absent.progress.accesses == 2079);
  Fixture absent_partial_ra;
  absent_partial_ra.region.known = nullptr;
  absent_partial_ra.context.machine.registers.gpr[31].known_mask = 7;
  CHECK(absent_partial_ra.run() == NBA97_TEXT_ARGUMENT &&
        absent_partial_ra.progress.operations == 2078 &&
        absent_partial_ra.progress.stopped_pc == 0x801e1488u);

  Fixture alias;
  std::vector<std::uint8_t> shared(0x2064, 0x5a);
  std::vector<std::uint8_t> shared_known(0x2064, 1);
  std::array<Nba97GameTextRegion, 4> regions{{
      {0x801e8b4cu, alias.bytes.data() + 0x1e8b4cu,
       alias.known.data() + 0x1e8b4cu, 8},
      {0x801e8b6cu, shared.data(), shared_known.data(), 4},
      {0x801e8b70u, alias.bytes.data() + 0x1e8b70u,
       alias.known.data() + 0x1e8b70u, 4},
      {0x801e903cu, shared.data(), shared_known.data(), 0x2064}}};
  alias.context.memory = {regions.data(), regions.size()};
  alias.transfer_call = 1;
  const int alias_result = alias.run();
  CHECK(alias_result == NBA97_TEXT_COMPLETE &&
        alias.progress.loaded_heap_reserve.word == 0 &&
        alias.progress.heap_size.word == 0x80004f58u);
}
} // namespace

int main() {
  try {
    normalBreakAndExactTrace();
    knownnessArithmeticAndTraps();
    transfersMutationAndRefusal();
    budgetsMemoryAndAliases();
    std::printf("gameload_entry_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
