#include "gameload_entry_capture.h"

#include "recovered/gameload_entry.h"

#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace nba97 {
namespace {
using U = std::uint32_t;
constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;

void put(std::vector<std::uint8_t> &bytes, std::vector<std::uint8_t> &known,
         U address, U value, std::uint8_t mask = 15) {
  if (address < Base || address - Base > Size - 4)
    return;
  for (unsigned i = 0; i < 4; ++i) {
    bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
    known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
  }
}

U get(const std::vector<std::uint8_t> &bytes, U address) {
  U value = 0;
  if (address < Base || address - Base > Size - 4)
    return 0;
  for (unsigned i = 0; i < 4; ++i)
    value |= U(bytes[address - Base + i]) << (i * 8u);
  return value;
}

struct Hash {
  std::uint64_t value = UINT64_C(14695981039346656037);
  void byte(std::uint8_t b) {
    value ^= b;
    value *= UINT64_C(1099511628211);
  }
  void u32(U word) {
    for (unsigned i = 0; i < 4; ++i) byte(std::uint8_t(word >> (i * 8u)));
  }
  void size(std::size_t word) {
    for (unsigned i = 0; i < 8; ++i)
      byte(std::uint8_t(std::uint64_t(word) >> (i * 8u)));
  }
};

void writeWord(std::ostringstream &out, const Nba97GameloadEntryWord &word) {
  out << "{\"word\":" << word.word << ",\"known_mask\":"
      << unsigned(word.known_mask) << '}';
}

void writeMachine(std::ostringstream &out,
                  const Nba97GameloadEntryMachine &machine) {
  out << "{\"gpr_words\":[";
  for (unsigned i = 0; i < 32; ++i) {
    if (i) out << ',';
    out << machine.registers.gpr[i].word;
  }
  out << "],\"gpr_known_masks\":[";
  for (unsigned i = 0; i < 32; ++i) {
    if (i) out << ',';
    out << unsigned(machine.registers.gpr[i].known_mask);
  }
  out << "],\"hi\":";
  writeWord(out, machine.hi);
  out << ",\"lo\":";
  writeWord(out, machine.lo);
  out << '}';
}

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97GameloadEntryContext context{};
  Nba97GameloadEntryProgress progress{};
  std::vector<Nba97GameloadEntryAccess> access =
      std::vector<Nba97GameloadEntryAccess>(2079);
  std::vector<U> instructions = std::vector<U>(10402);
  std::array<Nba97GameloadEntryEvent, 2> events{};
  std::array<Nba97GameloadEntryMachine, 2> call_machines{};
  unsigned calls = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {
          0x46000000u + i * 0x010203u, std::uint8_t((i % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[31] = {0x80028b70u, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x90abcdefu, 10};
    put(bytes, known, 0x801e8b70u, 0x00800000u);
    put(bytes, known, 0x801e8b6cu, 0x00008000u);
    put(bytes, known, 0x801e8b50u, 0);
    put(bytes, known, 0x801e8b4cu, 0);
    context.memory = {&region, 1};
    context.operation_budget = 2081;
    context.io = io;
    context.user = this;
    context.access_journal = access.data();
    context.access_journal_capacity = access.size();
    context.instruction_journal = instructions.data();
    context.instruction_journal_capacity = instructions.size();
  }

  static int io(void *opaque, const Nba97GameTextMemory *,
                const Nba97GameloadEntryEvent *event,
                Nba97GameloadEntryMachine *machine,
                Nba97GameloadEntryCalleeOutcome *outcome) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (!event || !machine || !outcome || f.calls >= 2)
      return 0;
    f.events[f.calls] = *event;
    f.call_machines[f.calls] = *machine;
    ++f.calls;
    *outcome = NBA97_GAMELOAD_ENTRY_CALLEE_RETURNED;
    return 1;
  }
};
} // namespace

std::string captureGameloadEntry() {
  Fixture f;
  const int result = nba97_gameload_entry(&f.context, &f.progress);
  unsigned contract_failure = 0;
  if (result != NBA97_GAMELOAD_ENTRY_BREAK_TRAP || f.calls != 2 ||
      f.progress.operations != 2081 || f.progress.accesses != 2079 ||
      f.progress.instruction_events != 10402 ||
      f.progress.access_events != 2079)
    contract_failure = 1;
  Hash pc_hash;
  for (std::size_t i = 0; i < f.progress.instruction_events &&
                          i < f.instructions.size();
       ++i)
    pc_hash.u32(f.instructions[i]);
  Hash access_hash;
  for (std::size_t i = 0;
       i < f.progress.access_events && i < f.access.size(); ++i) {
    const auto &event = f.access[i];
    access_hash.u32(event.pc);
    access_hash.u32(event.address);
    access_hash.u32(event.value);
    access_hash.size(event.operation);
    access_hash.byte(event.width);
    access_hash.byte(event.known_mask);
    access_hash.byte(event.kind);
  }
  std::ostringstream out;
  out << "{\"program\":\"GAMELOAD\",\"range\":\"801E1410-801E14B7\""
         ",\"bytes\":168,\"instructions\":42"
         ",\"source_sha256\":\"86de52922bd45fe1e8c5dd5768bb04d31a1a1ba8d0c9bc429d8a53b1919ae560\""
         ",\"fixture_contract\":\"Synthetic standalone GAMELOAD entry machine and retained 2MiB memory use raw static stack-top 0x00800000 and heap subtraction 0x00008000. Unbound full-machine child fixtures at 0x801E1590 and 0x801E136C both return while preserving every register, HI/LO and memory; all clears and heap/global stores are recovered owner effects. No transfer or gameplay success is fabricated.\""
         ",\"contract_failure\":"
      << contract_failure << ",\"result\":" << result
      << ",\"completed\":" << unsigned(f.progress.completed)
      << ",\"transferred\":" << unsigned(f.progress.transferred)
      << ",\"trapped\":" << unsigned(f.progress.trapped)
      << ",\"counts\":{\"operations\":" << f.progress.operations
      << ",\"accesses\":" << f.progress.accesses
      << ",\"reads\":" << f.progress.reads << ",\"stores\":"
      << f.progress.stores << ",\"words_cleared\":"
      << f.progress.words_cleared << ",\"callbacks\":"
      << f.progress.callbacks_completed << ",\"pc_events\":"
      << f.progress.instruction_events << "},\"journal_hash_contract\":\"FNV-1a-64 offset 14695981039346656037 prime 1099511628211; little-endian u32 PCs; each access is little-endian pc,address,value,u64 operation then width,known_mask,kind bytes\""
      << ",\"pc_hash_fnv1a64\":" << pc_hash.value
      << ",\"access_hash_fnv1a64\":" << access_hash.value
      << ",\"access_samples\":[";
  for (std::size_t index : {std::size_t(0), std::size_t(2072),
                            std::size_t(2073), std::size_t(2078)}) {
    const auto &event = f.access[index];
    if (index) out << ',';
    out << "{\"index\":" << index << ",\"pc\":" << event.pc
        << ",\"address\":" << event.address << ",\"value\":"
        << event.value << ",\"operation\":" << event.operation
        << ",\"kind\":" << unsigned(event.kind)
        << ",\"known_mask\":" << unsigned(event.known_mask) << '}';
  }
  out << "],\"call_sequence\":[";
  for (unsigned i = 0; i < f.calls; ++i) {
    if (i) out << ',';
    const auto &event = f.events[i];
    out << "{\"pc\":" << event.pc << ",\"target\":" << event.entry
        << ",\"delay\":" << event.delay_slot_pc
        << ",\"argument_count\":" << unsigned(event.argument_count)
        << ",\"invocation\":" << event.invocation
        << ",\"operation\":" << event.operation
        << ",\"outcome\":\"RETURNED\",\"machine\":";
    writeMachine(out, f.call_machines[i]);
    out << '}';
  }
  out << "],\"memory\":{\"bss_first\":" << get(f.bytes, 0x801e903cu)
      << ",\"bss_last\":" << get(f.bytes, 0x801eb09cu)
      << ",\"heap_size\":" << get(f.bytes, 0x801e8b50u)
      << ",\"heap_base\":" << get(f.bytes, 0x801e8b4cu)
      << "},\"final_machine\":";
  writeMachine(out, f.progress.machine);
  out << ",\"stopped_pc\":" << f.progress.stopped_pc
      << ",\"next_unbound_boundary\":\"801E1498->801E1590 InitHeap full-machine service; after a returned fixture, 801E14AC->801E136C GAMELOAD main remains unbound\""
         ",\"gameplay_shown\":\"BLOCKED\"}";
  return out.str();
}
} // namespace nba97
