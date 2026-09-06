#include "frontend_io_drain_capture.h"

#include "frontend_io_drain_adapter.h"

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
constexpr U ParentRa = 0x800394f0u;
constexpr U StatusBase = 0x800ef840u;
constexpr U PointerBase = 0x800ef844u;
constexpr U AuxBase = 0x800ef830u;
constexpr U Handle = 0x80145678u;

struct Call {
  Nba97FrontendIoDrainEvent event{};
  Nba97FrontendIoDrainMachine machine{};
};

struct Capture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendExitDrainMachine machine{};
  Nba97FrontendExitDrainEvent parent{
      0x800394e8u, 0x800394ecu, 0x800393f0u, 3, 1,
      NBA97_FRONTEND_EXIT_DRAIN_SITE_800394E8, 0,
      NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
  Nba97FrontendIoDrainBinding binding{};
  std::array<Nba97FrontendIoDrainAccess, 24> access{};
  std::array<U, 192> instructions{};
  std::vector<Call> calls;
  bool contract_failure = false;

  Capture() {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x63000000u + i * 0x101u, 15};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_SP] = {Sp, 15};
    machine.registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_RA] = {ParentRa, 15};
    machine.hi = {0x10203040u, 5};
    machine.lo = {0x50607080u, 10};
    const std::array<U, 8> statuses{{3, 1, 4, 5, U(-1), 0, 2, 6}};
    for (unsigned slot = 0; slot < statuses.size(); ++slot) {
      put(StatusBase + slot * 36u, statuses[slot]);
      put(PointerBase + slot * 36u,
          slot == 0 ? Handle : 0x81000000u + slot * 0x100u);
      put(AuxBase + slot * 36u, 0xa0000000u + slot);
    }
    binding.operation_budget = 24;
    binding.io = callback;
    binding.user = this;
    binding.access_journal = access.data();
    binding.access_journal_capacity = access.size();
    binding.instruction_journal = instructions.data();
    binding.instruction_journal_capacity = instructions.size();
  }

  bool extent(U address, U width) const {
    return address >= Base && width <= Size && address - Base <= Size - width;
  }
  void put(U address, U value, unsigned width = 4) {
    if (!extent(address, width)) {
      contract_failure = true;
      return;
    }
    for (unsigned i = 0; i < width; ++i)
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
  }
  U get(U address, unsigned width = 4) {
    if (!extent(address, width)) {
      contract_failure = true;
      return 0;
    }
    U result = 0;
    for (unsigned i = 0; i < width; ++i)
      result |= U(bytes[address - Base + i]) << (i * 8u);
    return result;
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendIoDrainEvent *event,
                      Nba97FrontendIoDrainMachine *machine) {
    auto &capture = *static_cast<Capture *>(opaque);
    Nba97FrontendIoDrainSiteContract contract{};
    if (!event || !machine ||
        !nba97_frontend_io_drain_site_contract(event->site, &contract) ||
        event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->entry != contract.target ||
        event->argument_count != contract.argument_count ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        machine->registers.gpr[NBA97_FRONTEND_IO_DRAIN_RA].word !=
            event->pc + 8u ||
        machine->registers.gpr[NBA97_FRONTEND_IO_DRAIN_RA].known_mask != 15) {
      capture.contract_failure = true;
      return 0;
    }
    if (event->site == NBA97_FRONTEND_IO_DRAIN_SITE_80039458 &&
        (event->invocation != 1 ||
         machine->registers.gpr[NBA97_FRONTEND_IO_DRAIN_A0].word != Handle ||
         machine->registers.gpr[NBA97_FRONTEND_IO_DRAIN_A0].known_mask != 15)) {
      capture.contract_failure = true;
      return 0;
    }
    capture.calls.push_back({*event, *machine});
    if (event->site == NBA97_FRONTEND_IO_DRAIN_SITE_8003949C)
      machine->registers.gpr[NBA97_FRONTEND_IO_DRAIN_V0] = {
          event->invocation == 1 ? 0u : 1u, 15};
    return 1;
  }
};

std::string hex(U value) {
  std::ostringstream out;
  out << "\"0x" << std::hex << std::setw(8) << std::setfill('0') << value
      << '\"';
  return out.str();
}

void word(std::ostringstream &out, const Nba97FrontendIoDrainWord &value) {
  out << "{\"word\":" << hex(value.word)
      << ",\"known_mask\":" << unsigned(value.known_mask) << '}';
}
} // namespace

std::string captureFrontendIoDrain() {
  try {
    Capture capture;
    Nba97GameTextMemory memory{&capture.region, 1};
    const int accepted = nba97_frontend_io_drain_from_frontend_exit_drain(
        &capture.binding, &memory, &capture.parent, &capture.machine);
    const auto &progress = capture.binding.progress;
    if (accepted != 1 || capture.binding.result != NBA97_TEXT_COMPLETE ||
        progress.operations != 24 || progress.accesses != 20 ||
        progress.reads != 12 || progress.stores != 8 ||
        progress.callbacks_completed != 4 || progress.slot_iterations != 8 ||
        progress.poll_attempts != 2 || progress.zero_poll_results != 1 ||
        progress.instruction_count != 164 ||
        progress.access_events > capture.access.size() ||
        progress.instruction_events > capture.instructions.size() ||
        capture.calls.size() != 4 || capture.get(StatusBase) != 0 ||
        capture.get(PointerBase) != 0 ||
        capture.get(StatusBase + 36u) != 0 ||
        capture.get(AuxBase + 72u) != 0 ||
        capture.get(AuxBase + 108u) != 0)
      capture.contract_failure = true;

    std::ostringstream out;
    out << "{\"program\":\"FEONLY\",\"address\":" << hex(0x800393f0u)
        << ",\"inclusive_end\":" << hex(0x800394d3u)
        << ",\"bytes\":228,\"instructions\":57,"
        << "\"source_sha256\":\"ddd6a228f2ddfecfebe23641b1c36c549e82172f38dfe659484b2d9e521ea50c\","
        << "\"completed\":" << unsigned(progress.completed)
        << ",\"accepted\":" << accepted << ",\"result\":"
        << capture.binding.result << ",\"contract_failure\":"
        << capture.contract_failure
        << ",\"classification\":\"no direct visual effect\","
        << "\"gameplay_shown\":\"BLOCKED\","
        << "\"fixture_contract\":\"Synthetic standalone full machine at the recovered frontend-exit-drain 0x800394E8 boundary. Handle child 0x80077638 receives the exact slot pointer in a0 and preserves all 32 GPR words and masks plus HI/LO with no guest-memory effects. Poll child 0x800392A0 preserves that state except for explicit fully-known V0 results zero then one. Pump child 0x80038E84 preserves the full machine and memory. These are synthetic callback contracts, not recovered child ABIs or production services.\","
        << "\"parent_call\":{\"pc\":" << hex(capture.parent.pc)
        << ",\"delay\":" << hex(capture.parent.delay_slot_pc)
        << ",\"entry\":" << hex(capture.parent.entry)
        << ",\"argument_count\":" << unsigned(capture.parent.argument_count)
        << ",\"program\":" << unsigned(capture.parent.target_program)
        << ",\"ra\":" << hex(ParentRa) << "},"
        << "\"status_fixture\":[3,1,4,5,-1,0,2,6],"
        << "\"drain\":{\"operations\":" << progress.operations
        << ",\"accesses\":" << progress.accesses << ",\"reads\":"
        << progress.reads << ",\"stores\":" << progress.stores
        << ",\"callbacks\":" << progress.callbacks_completed
        << ",\"slot_iterations\":" << progress.slot_iterations
        << ",\"poll_attempts\":" << progress.poll_attempts
        << ",\"zero_poll_results\":" << progress.zero_poll_results
        << ",\"instruction_count\":" << progress.instruction_count
        << ",\"instruction_trace\":[";
    const std::size_t instruction_events =
        std::min(progress.instruction_events, capture.instructions.size());
    for (std::size_t i = 0; i < instruction_events; ++i) {
      if (i) out << ',';
      out << hex(capture.instructions[i]);
    }
    out << "],\"access_journal\":[";
    const std::size_t access_events =
        std::min(progress.access_events, capture.access.size());
    for (std::size_t i = 0; i < access_events; ++i) {
      const auto &event = capture.access[i];
      if (i) out << ',';
      out << "{\"pc\":" << hex(event.pc) << ",\"address\":"
          << hex(event.address) << ",\"value\":" << hex(event.value)
          << ",\"operation\":" << event.operation
          << ",\"width\":" << unsigned(event.width)
          << ",\"known_mask\":" << unsigned(event.known_mask)
          << ",\"kind\":" << unsigned(event.kind) << '}';
    }
    out << "],\"call_sequence\":[";
    for (std::size_t i = 0; i < capture.calls.size(); ++i) {
      const auto &call = capture.calls[i];
      if (i) out << ',';
      out << "{\"pc\":" << hex(call.event.pc) << ",\"delay\":"
          << hex(call.event.delay_slot_pc) << ",\"target\":"
          << hex(call.event.entry) << ",\"argument_count\":"
          << unsigned(call.event.argument_count) << ",\"invocation\":"
          << call.event.invocation << ",\"a0\":";
      word(out, call.machine.registers.gpr[NBA97_FRONTEND_IO_DRAIN_A0]);
      out << ",\"s0\":";
      word(out, call.machine.registers.gpr[NBA97_FRONTEND_IO_DRAIN_S0]);
      out << ",\"s1\":";
      word(out, call.machine.registers.gpr[NBA97_FRONTEND_IO_DRAIN_S1]);
      out << ",\"sp\":";
      word(out, call.machine.registers.gpr[NBA97_FRONTEND_IO_DRAIN_SP]);
      out << ",\"ra\":";
      word(out, call.machine.registers.gpr[NBA97_FRONTEND_IO_DRAIN_RA]);
      out << ",\"machine\":{\"gpr\":[";
      for (unsigned reg = 0; reg < 32; ++reg) {
        if (reg) out << ',';
        word(out, call.machine.registers.gpr[reg]);
      }
      out << "],\"hi\":";
      word(out, call.machine.hi);
      out << ",\"lo\":";
      word(out, call.machine.lo);
      out << "}}";
    }
    out << "]},\"final_machine\":{\"gpr\":[";
    for (unsigned reg = 0; reg < 32; ++reg) {
      if (reg) out << ',';
      word(out, progress.machine.registers.gpr[reg]);
    }
    out << "],\"hi\":";
    word(out, progress.machine.hi);
    out << ",\"lo\":";
    word(out, progress.machine.lo);
    out << "},\"next_unbound_boundary\":{"
           "\"earliest_production\":\"unowned FEONLY overlay-entry JAL 0x8007B838 and startup service 0x80028810 -> 0x8007B844\","
           "\"io_children\":\"0x80039458 -> 0x80077638, 0x8003949C -> 0x800392A0, and zero-result 0x800394AC -> 0x80038E84 remain typed services\","
           "\"gameplay\":\"actual advancing native lifecycle, loader, and match handoff remain unbound\"}}";
    return out.str();
  } catch (...) {
    return "{\"program\":\"FEONLY\",\"address\":\"0x800393f0\",\"bytes\":228,\"completed\":0,\"accepted\":0,\"result\":0,\"contract_failure\":true,\"classification\":\"no direct visual effect\",\"gameplay_shown\":\"BLOCKED\"}";
  }
}
} // namespace nba97
