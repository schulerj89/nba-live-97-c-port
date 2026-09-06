#include "frontend_load_payload_capture.h"

#include "frontend_load_payload_adapter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <vector>

namespace nba97 {
namespace {
using U = std::uint32_t;
constexpr U Base = 0x80000000u;
constexpr U Size = 0x200000u;
constexpr U Sp = 0x801f0000u;
constexpr U ParentRa = 0x80028ad4u;
constexpr U Filename = 0x80024854u;
constexpr U Descriptor = 0x80170000u;
constexpr U Payload = 0x801e1410u;

struct Path {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendOverlayLoadContext parent{};
  Nba97FrontendOverlayLoadProgress parent_progress{};
  Nba97FrontendLoadPayloadBinding payload{};
  Nba97FrontendLoadPayloadAdapterProgress adapter{};
  std::array<Nba97FrontendOverlayLoadAccess, 4> parent_access{};
  std::array<U, 8> parent_instructions{};
  std::array<Nba97FrontendLoadPayloadAccess, 4> payload_access{};
  std::array<U, 13> payload_instructions{};
  Nba97FrontendLoadPayloadEvent child_event{};
  Nba97FrontendLoadPayloadMachine child_machine{};
  Nba97FrontendLoadPayloadWord child_return{};
  unsigned child_calls = 0;
  bool contract_failure = false;
  int result = NBA97_TEXT_ARGUMENT;

  explicit Path(bool nonnull) {
    for (unsigned i = 0; i < 32; ++i)
      parent.machine.registers.gpr[i] = {0x75000000u + i * 0x101u, 15};
    parent.machine.registers.gpr[0] = {0, 15};
    parent.machine.registers.gpr[4] = {Filename, 15};
    parent.machine.registers.gpr[5] = {0, 15};
    parent.machine.registers.gpr[6] = {0x55667788u, 5};
    parent.machine.registers.gpr[29] = {Sp, 15};
    parent.machine.registers.gpr[31] = {ParentRa, 15};
    parent.machine.hi = {0x10203040u, 6};
    parent.machine.lo = {0x50607080u, 9};
    put(Descriptor, Payload, 7);
    child_return = nonnull ? Nba97FrontendLoadPayloadWord{Descriptor, 15}
                           : Nba97FrontendLoadPayloadWord{0, 15};
    parent.memory = {&region, 1};
    parent.operation_budget = 3;
    parent.access_journal = parent_access.data();
    parent.access_journal_capacity = parent_access.size();
    parent.instruction_journal = parent_instructions.data();
    parent.instruction_journal_capacity = parent_instructions.size();
    payload.operation_budget = nonnull ? 4 : 3;
    payload.io = childIo;
    payload.user = this;
    payload.access_journal = payload_access.data();
    payload.access_journal_capacity = payload_access.size();
    payload.instruction_journal = payload_instructions.data();
    payload.instruction_journal_capacity = payload_instructions.size();
  }

  bool extent(U address, U width = 4) const {
    return address >= Base && width <= Size && address - Base <= Size - width;
  }
  void put(U address, U value, std::uint8_t mask = 15) {
    if (!extent(address)) {
      contract_failure = true;
      return;
    }
    for (unsigned i = 0; i < 4; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
    }
  }
  U get(U address) {
    if (!extent(address)) {
      contract_failure = true;
      return 0;
    }
    U value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }

  static int childIo(void *opaque, const Nba97GameTextMemory *,
                     const Nba97FrontendLoadPayloadEvent *event,
                     Nba97FrontendLoadPayloadMachine *machine) {
    auto &path = *static_cast<Path *>(opaque);
    ++path.child_calls;
    if (!event || !machine || event->pc != 0x8007b164u ||
        event->delay_slot_pc != 0x8007b168u ||
        event->entry != 0x8007b1d0u || event->operation != 2 ||
        event->invocation != 1 || event->argument_count != 3 ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        machine->registers.gpr[4].word != Filename ||
        machine->registers.gpr[4].known_mask != 15 ||
        machine->registers.gpr[5].word != 0 ||
        machine->registers.gpr[5].known_mask != 15 ||
        machine->registers.gpr[6].word != 1 ||
        machine->registers.gpr[6].known_mask != 15 ||
        machine->registers.gpr[31].word != 0x8007b16cu ||
        machine->registers.gpr[31].known_mask != 15) {
      path.contract_failure = true;
      return 0;
    }
    path.child_event = *event;
    path.child_machine = *machine;
    machine->registers.gpr[2] = path.child_return;
    return 1;
  }

  void run(bool nonnull) {
    result = nba97_frontend_overlay_load_with_recovered_payload(
        &parent, &payload, &parent_progress, &adapter);
    if (result != NBA97_TEXT_COMPLETE || !parent_progress.completed ||
        !payload.progress.completed || payload.invocations != 1 ||
        payload.completions != 1 || adapter.invocations != 1 ||
        adapter.completions != 1 || child_calls != 1 ||
        parent_progress.operations != 3 || parent_progress.accesses != 2 ||
        parent_progress.callbacks_completed != 1 ||
        parent_progress.instruction_count != 8 ||
        payload.progress.operations != (nonnull ? 4u : 3u) ||
        payload.progress.accesses != (nonnull ? 3u : 2u) ||
        payload.progress.callbacks_completed != 1 ||
        payload.progress.instruction_count != (nonnull ? 11u : 12u) ||
        payload.progress.access_events > payload_access.size() ||
        payload.progress.instruction_events > payload_instructions.size() ||
        parent_progress.access_events > parent_access.size() ||
        parent_progress.instruction_events > parent_instructions.size() ||
        payload.progress.payload_result.word != (nonnull ? Payload : 0u) ||
        payload.progress.payload_result.known_mask != (nonnull ? 7u : 15u) ||
        parent_progress.machine.registers.gpr[2].word !=
            (nonnull ? Payload : 0u) ||
        get(Descriptor) != Payload)
      contract_failure = true;
  }
};

std::string hex(U value) {
  std::ostringstream out;
  out << "\"0x" << std::hex << std::setw(8) << std::setfill('0') << value
      << '\"';
  return out.str();
}
void word(std::ostringstream &out, const Nba97FrontendLoadPayloadWord &value) {
  out << "{\"word\":" << hex(value.word)
      << ",\"known_mask\":" << unsigned(value.known_mask) << '}';
}
void machine(std::ostringstream &out,
             const Nba97FrontendLoadPayloadMachine &value) {
  out << "{\"gpr\":[";
  for (unsigned reg = 0; reg < 32; ++reg) {
    if (reg) out << ',';
    word(out, value.registers.gpr[reg]);
  }
  out << "],\"hi\":";
  word(out, value.hi);
  out << ",\"lo\":";
  word(out, value.lo);
  out << '}';
}
template <typename Access>
void access(std::ostringstream &out, const Access &event) {
  out << "{\"pc\":" << hex(event.pc) << ",\"address\":"
      << hex(event.address) << ",\"value\":" << hex(event.value)
      << ",\"operation\":" << event.operation
      << ",\"width\":" << unsigned(event.width)
      << ",\"known_mask\":" << unsigned(event.known_mask)
      << ",\"kind\":" << unsigned(event.kind) << '}';
}
void path(std::ostringstream &out, const char *kind, const Path &value) {
  out << "{\"kind\":\"" << kind << "\",\"result\":" << value.result
      << ",\"descriptor\":{\"address\":" << hex(Descriptor)
      << ",\"word\":" << hex(Payload) << ",\"known_mask\":7},"
      << "\"parent_call\":{\"pc\":" << hex(value.adapter.parent_event.pc)
      << ",\"delay\":" << hex(value.adapter.parent_event.delay_slot_pc)
      << ",\"target\":" << hex(value.adapter.parent_event.entry)
      << ",\"argument_count\":"
      << unsigned(value.adapter.parent_event.argument_count)
      << ",\"machine\":";
  machine(out, value.adapter.parent_machine);
  out << "},\"overlay_load\":{\"operations\":"
      << value.parent_progress.operations << ",\"accesses\":"
      << value.parent_progress.accesses << ",\"callbacks\":"
      << value.parent_progress.callbacks_completed
      << ",\"instruction_count\":" << value.parent_progress.instruction_count
      << ",\"child_return\":";
  word(out, value.parent_progress.child_return);
  out << ",\"instruction_trace\":[";
  for (std::size_t i = 0;
       i < std::min(value.parent_progress.instruction_events,
                    value.parent_instructions.size());
       ++i) {
    if (i) out << ',';
    out << hex(value.parent_instructions[i]);
  }
  out << "],\"access_journal\":[";
  for (std::size_t i = 0;
       i < std::min(value.parent_progress.access_events,
                    value.parent_access.size());
       ++i) {
    if (i) out << ',';
    access(out, value.parent_access[i]);
  }
  out << "]},\"load_payload\":{\"operations\":"
      << value.payload.progress.operations << ",\"accesses\":"
      << value.payload.progress.accesses << ",\"reads\":"
      << value.payload.progress.reads << ",\"stores\":"
      << value.payload.progress.stores << ",\"callbacks\":"
      << value.payload.progress.callbacks_completed
      << ",\"instruction_count\":"
      << value.payload.progress.instruction_count << ",\"forwarded_a0\":";
  word(out, value.payload.progress.forwarded_a0);
  out << ",\"forwarded_a1\":";
  word(out, value.payload.progress.forwarded_a1);
  out << ",\"forwarded_a2\":";
  word(out, value.payload.progress.forwarded_a2);
  out << ",\"child_return\":";
  word(out, value.payload.progress.child_return);
  out << ",\"payload_result\":";
  word(out, value.payload.progress.payload_result);
  out << ",\"instruction_trace\":[";
  for (std::size_t i = 0;
       i < std::min(value.payload.progress.instruction_events,
                    value.payload_instructions.size());
       ++i) {
    if (i) out << ',';
    out << hex(value.payload_instructions[i]);
  }
  out << "],\"access_journal\":[";
  for (std::size_t i = 0;
       i < std::min(value.payload.progress.access_events,
                    value.payload_access.size());
       ++i) {
    if (i) out << ',';
    access(out, value.payload_access[i]);
  }
  out << "],\"call_sequence\":[{\"pc\":" << hex(value.child_event.pc)
      << ",\"delay\":" << hex(value.child_event.delay_slot_pc)
      << ",\"target\":" << hex(value.child_event.entry)
      << ",\"argument_count\":" << unsigned(value.child_event.argument_count)
      << ",\"invocation\":" << value.child_event.invocation
      << ",\"machine\":";
  machine(out, value.child_machine);
  out << "}],\"final_machine\":";
  machine(out, value.payload.progress.machine);
  out << "},\"final_machine\":";
  machine(out, value.parent_progress.machine);
  out << '}';
}
} // namespace

std::string captureFrontendLoadPayload() {
  try {
    Path null_path(false);
    Path nonnull_path(true);
    null_path.run(false);
    nonnull_path.run(true);
    const bool contract_failure =
        null_path.contract_failure || nonnull_path.contract_failure;
    const unsigned completed =
        unsigned(null_path.parent_progress.completed &&
                 nonnull_path.parent_progress.completed);
    std::ostringstream out;
    out << "{\"program\":\"FEONLY\",\"address\":" << hex(0x8007b15cu)
        << ",\"inclusive_end\":" << hex(0x8007b18fu)
        << ",\"bytes\":52,\"instructions\":13,"
        << "\"source_sha256\":\"aaf6935467d7d6bad48e084fafaf71528d7b8e6ebb23deca4bef4e2f2f9b3ebf\","
        << "\"completed\":" << completed << ",\"result\":"
        << (completed ? NBA97_TEXT_COMPLETE : NBA97_TEXT_ARGUMENT)
        << ",\"contract_failure\":" << contract_failure
        << ",\"classification\":\"no direct visual effect\","
        << "\"gameplay_shown\":\"BLOCKED\","
        << "\"fixture_contract\":\"Two synthetic standalone full machines and retained-memory maps enter the committed recovered 0x8007B11C overlay-load owner. Its natural 0x8007B124 call genuinely executes the recovered 0x8007B15C loaded-payload wrapper. The sole unbound 0x8007B1D0 fixture validates and preserves every GPR word/mask plus HI/LO and guest memory, then changes only V0: fully-known zero on the null path or fully-known descriptor address 0x80170000 on the nonnull path. The synthetic retained descriptor already contains word 0x801E1410 with known mask 7; the fixture does not create or alter descriptor bytes.\","
        << "\"paths\":[";
    path(out, "null", null_path);
    out << ',';
    path(out, "nonnull", nonnull_path);
    out << "],\"next_unbound_boundary\":{"
           "\"loader_child\":\"0x8007B164 -> 0x8007B1D0(a0,a1,a2) remains an unbound full-machine filesystem/descriptor service\","
           "\"other_caller\":\"unowned 0x8007B144 also calls this wrapper\","
           "\"production\":\"actual filesystem-backed overlay loading, GAMELOAD lifecycle, and advancing native court/player match remain unbound\"}}";
    return out.str();
  } catch (...) {
    return "{\"program\":\"FEONLY\",\"address\":\"0x8007b15c\",\"bytes\":52,\"completed\":0,\"contract_failure\":true,\"classification\":\"no direct visual effect\",\"gameplay_shown\":\"BLOCKED\"}";
  }
}
} // namespace nba97
