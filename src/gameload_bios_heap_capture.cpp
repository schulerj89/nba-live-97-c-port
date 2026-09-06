#include "gameload_bios_heap_adapter.h"
#include "gameload_bios_heap_capture.h"

#include <array>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

using U = std::uint32_t;
using Word = Nba97GameloadBiosHeapWord;
using Machine = Nba97GameloadBiosHeapMachine;

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

U hashMemory(const Nba97GameTextMemory &memory) {
  U hash = UINT32_C(2166136261);
  for (std::size_t region_index = 0u; region_index != memory.count;
       ++region_index) {
    const Nba97GameTextRegion &region = memory.region[region_index];
    for (std::size_t index = 0u; index != region.size; ++index) {
      hash ^= region.data[index];
      hash *= UINT32_C(16777619);
      if (region.known != nullptr) {
        hash ^= region.known[index];
        hash *= UINT32_C(16777619);
      }
    }
  }
  return hash;
}

void emitMachine(std::ostringstream &stream, const Machine &machine) {
  stream << '[';
  for (unsigned index = 0u; index != 34u; ++index) {
    const Word &word = index < 32u ? machine.registers.gpr[index]
                       : index == 32u ? machine.hi
                                      : machine.lo;
    if (index != 0u)
      stream << ',';
    stream << "{\"word\":" << word.word
           << ",\"known_mask\":" << static_cast<unsigned>(word.known_mask)
           << '}';
  }
  stream << ']';
}

struct CaptureFixture {
  static constexpr U Base = UINT32_C(0x80000000);
  static constexpr U Size = UINT32_C(0x00200000);
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0u);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1u);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::vector<Nba97GameloadEntryAccess> entry_access =
      std::vector<Nba97GameloadEntryAccess>(2100u);
  std::vector<U> entry_pc = std::vector<U>(10500u);
  std::array<U, 3u> heap_pc{};
  Nba97GameloadEntryContext entry{};
  Nba97GameloadEntryProgress entry_progress{};
  Nba97GameloadBiosHeapBinding heap{};
  Nba97GameloadBiosHeapAdapterProgress adapter{};
  Machine callback_machine{};
  Machine final_machine{};
  U callback_memory_before = 0u;
  U callback_memory_after = 0u;
  unsigned heap_calls = 0u;
  unsigned main_calls = 0u;

  CaptureFixture() {
    entry.memory = {&region, 1u};
    entry.operation_budget = 2081u;
    for (unsigned index = 0u; index != 32u; ++index)
      setWord(entry.machine.registers.gpr[index],
              UINT32_C(0x47000000) + index * UINT32_C(0x00010113),
              static_cast<std::uint8_t>((index * 5u) & 15u));
    setWord(entry.machine.registers.gpr[0], 0u);
    setWord(entry.machine.registers.gpr[NBA97_GAMELOAD_ENTRY_RA],
            UINT32_C(0x80028b70));
    setWord(entry.machine.hi, UINT32_C(0x12345678), 5u);
    setWord(entry.machine.lo, UINT32_C(0x9abcdef0), 10u);
    entry.io = child;
    entry.user = this;
    entry.access_journal = entry_access.data();
    entry.access_journal_capacity = entry_access.size();
    entry.instruction_journal = entry_pc.data();
    entry.instruction_journal_capacity = entry_pc.size();
    put(UINT32_C(0x801e8b70), UINT32_C(0x00800000));
    put(UINT32_C(0x801e8b6c), UINT32_C(0x00008000));
    nba97_gameload_bios_heap_binding_init(
        &heap, 1u, bios, this, heap_pc.data(), heap_pc.size());
  }

  void put(U address, U value) {
    if (address < Base || address - Base > Size - 4u)
      throw std::runtime_error("capture fixture write outside RAM");
    for (unsigned index = 0u; index != 4u; ++index)
      bytes[address - Base + index] =
          static_cast<std::uint8_t>(value >> (index * 8u));
  }

  static int bios(void *opaque, const Nba97GameTextMemory *memory,
                  const Nba97GameloadBiosHeapEvent *event,
                  Machine *machine) {
    CaptureFixture &fixture = *static_cast<CaptureFixture *>(opaque);
    if (memory == nullptr || event == nullptr || machine == nullptr ||
        event->pc != UINT32_C(0x801e1594) ||
        event->delay_slot_pc != UINT32_C(0x801e1598) ||
        event->entry != UINT32_C(0x000000a0) || event->service != 0x39u ||
        event->argument_count != 2u || event->operation != 1u)
      return 0;
    ++fixture.heap_calls;
    fixture.callback_machine = *machine;
    fixture.callback_memory_before = hashMemory(*memory);
    /* Explicit synthetic contract: the fixture represents a returned BIOS
     * call while preserving all CPU words, memory bytes and knownness. */
    fixture.callback_memory_after = hashMemory(*memory);
    return 1;
  }

  static int child(void *opaque, const Nba97GameTextMemory *,
                   const Nba97GameloadEntryEvent *event,
                   Nba97GameloadEntryMachine *machine,
                   Nba97GameloadEntryCalleeOutcome *outcome) {
    CaptureFixture &fixture = *static_cast<CaptureFixture *>(opaque);
    if (event == nullptr || machine == nullptr || outcome == nullptr ||
        event->site != NBA97_GAMELOAD_ENTRY_SITE_801E14AC)
      return 0;
    ++fixture.main_calls;
    fixture.final_machine = *machine;
    *outcome = NBA97_GAMELOAD_ENTRY_CALLEE_RETURNED;
    return 0;
  }

  int run() {
    return nba97_gameload_entry_with_recovered_bios_heap(
        &entry, &heap, &entry_progress, &adapter);
  }
};

} // namespace

namespace nba97 {

std::string captureGameloadBiosHeap() {
  CaptureFixture fixture;
  if (fixture.run() != NBA97_TEXT_IO_REFUSED || fixture.entry_progress.completed ||
      fixture.entry_progress.transferred || fixture.heap_calls != 1u ||
      fixture.main_calls != 1u || fixture.heap.progress.instruction_count != 3u ||
      fixture.heap_pc[0] != UINT32_C(0x801e1590) ||
      fixture.heap_pc[1] != UINT32_C(0x801e1594) ||
      fixture.heap_pc[2] != UINT32_C(0x801e1598) ||
      !sameMachine(fixture.callback_machine, fixture.heap.progress.machine) ||
      !sameMachine(fixture.final_machine, fixture.entry_progress.machine) ||
      fixture.callback_memory_before != fixture.callback_memory_after)
    throw std::runtime_error("GAMELOAD BIOS heap capture contract failed");

  std::ostringstream stream;
  stream
      << "{\"program\":\"GAMELOAD\",\"address\":\"0x801E1590\","
         "\"inclusive_end\":\"0x801E159B\",\"bytes\":12,\"instructions\":3,"
         "\"classification\":\"no direct visual effect\","
         "\"fixture_contract\":\"Synthetic BIOS A0 service 0x39 returns "
         "through live ra while preserving all 34 CPU words, retained RAM "
         "bytes, and optional knownness; GAMELOAD main fixture refuses the unbound continuation\","
         "\"operations\":"
      << fixture.heap.progress.operations
      << ",\"source_sha256\":\"4487ee3019aae533a71d191483e6876aa40c2530923670ec0e012a78204fb863\""
      << ",\"parent_result\":-5,\"parent_completed\":0,\"parent_transferred\":0,\"parent_stopped_pc\":" << fixture.entry_progress.stopped_pc
      << ",\"parent_stopped_target\":" << fixture.entry_progress.stopped_target
      << ",\"callbacks_completed\":"
      << fixture.heap.progress.callbacks_completed
      << ",\"pc_sequence\":[" << fixture.heap_pc[0] << ','
      << fixture.heap_pc[1] << ',' << fixture.heap_pc[2]
      << "],\"call_pc\":" << fixture.heap.progress.event.pc
      << ",\"delay_pc\":" << fixture.heap.progress.event.delay_slot_pc
      << ",\"bios_vector\":" << fixture.heap.progress.event.entry
      << ",\"service\":"
      << static_cast<unsigned>(fixture.heap.progress.event.service)
      << ",\"argument_count\":"
      << static_cast<unsigned>(fixture.heap.progress.event.argument_count)
      << ",\"callback_machine_words\":34,\"callback_machine\":";
  emitMachine(stream, fixture.callback_machine);
  stream << ",\"final_machine_words\":34,\"final_machine\":";
  emitMachine(stream, fixture.final_machine);
  stream << ",\"callback_memory_before\":"
         << fixture.callback_memory_before
         << ",\"callback_memory_after\":" << fixture.callback_memory_after
         << ",\"stack_pointer\":"
         << fixture.entry_progress.machine
                .registers.gpr[NBA97_GAMELOAD_ENTRY_SP]
                .word
         << ",\"synthetic_raster\":false,"
            "\"pixel_expectation\":\"matching native before/after frames\","
            "\"gameplay_shown\":\"BLOCKED\","
            "\"next_unbound_boundary\":\"BIOS A0:39 host full-state service "
            "remains unbound; after return the capture leaves 0x801E14AC -> GAMELOAD main "
            "0x801E136C unbound; its recovered owner and services require production wiring\"}";
  return stream.str();
}

} // namespace nba97
