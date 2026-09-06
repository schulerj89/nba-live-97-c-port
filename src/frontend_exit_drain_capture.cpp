#include "frontend_exit_drain_capture.h"

#include "frontend_exit_drain_adapter.h"

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
constexpr U ParentRa = 0x8002f09cu;
constexpr U Mode = 0x80145678u;

struct Call {
  Nba97FrontendExitDrainEvent event{};
  Nba97FrontendExitDrainMachine machine{};
};

struct Capture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendExitCleanupMachine machine{};
  Nba97FrontendExitCleanupEvent parent{0x8002f094u,
                                       0x8002f098u,
                                       0x800394d4u,
                                       2,
                                       1,
                                       NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F094,
                                       0,
                                       NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
  Nba97FrontendExitDrainBinding binding{};
  std::array<Nba97FrontendExitDrainAccess, 12> access{};
  std::array<U, 64> instructions{};
  std::vector<Call> calls;
  bool contract_failure = false;

  Capture() {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x63000000u + i * 0x101u, 15};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_SP] = {Sp, 15};
    machine.registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_RA] = {ParentRa, 15};
    machine.hi = {0x10203040u, 5};
    machine.lo = {0x50607080u, 10};
    put(0x800f84c4u, 1);
    put(0x800f43b0u, 0xabcdef01u);
    put(0x8002149cu, Mode);
    binding.operation_budget = 16;
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
                      const Nba97FrontendExitDrainEvent *event,
                      Nba97FrontendExitDrainMachine *machine) {
    auto &capture = *static_cast<Capture *>(opaque);
    Nba97FrontendExitDrainSiteContract contract{};
    if (!event || !machine ||
        !nba97_frontend_exit_drain_site_contract(event->site, &contract) ||
        event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->entry != contract.target ||
        event->argument_count != contract.argument_count ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_RA].word !=
            event->pc + 8u ||
        machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_RA].known_mask != 15) {
      capture.contract_failure = true;
      return 0;
    }
    if (event->site == NBA97_FRONTEND_EXIT_DRAIN_SITE_80039530 &&
        (machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_A0].word != 0 ||
         machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_A0].known_mask != 15 ||
         machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_A1].word != 0 ||
         machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_A1].known_mask !=
             15)) {
      capture.contract_failure = true;
      return 0;
    }
    if (event->site == NBA97_FRONTEND_EXIT_DRAIN_SITE_80039554 &&
        (machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_A0].word != Mode ||
         machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_A0].known_mask !=
             15)) {
      capture.contract_failure = true;
      return 0;
    }
    capture.calls.push_back({*event, *machine});
    if (event->site == NBA97_FRONTEND_EXIT_DRAIN_SITE_800394F0)
      machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_V0] = {
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

void word(std::ostringstream &out, const Nba97FrontendExitDrainWord &value) {
  out << "{\"word\":" << hex(value.word)
      << ",\"known_mask\":" << unsigned(value.known_mask) << '}';
}
} // namespace

std::string captureFrontendExitDrain() {
  try {
    Capture capture;
    const U active_before = capture.get(0x800f84c4u);
    const U busy_before = capture.get(0x800f43b0u);
    Nba97GameTextMemory memory{&capture.region, 1};
    const int accepted = nba97_frontend_exit_drain_from_frontend_exit_cleanup(
        &capture.binding, &memory, &capture.parent, &capture.machine);
    const auto &progress = capture.binding.progress;
    if (accepted != 1 || capture.binding.result != NBA97_TEXT_COMPLETE ||
        progress.operations != 15 || progress.accesses != 7 ||
        progress.reads != 4 || progress.stores != 3 ||
        progress.callbacks_completed != 8 || progress.poll_attempts != 2 ||
        progress.zero_poll_results != 1 || progress.instruction_count != 44 ||
        progress.access_events > capture.access.size() ||
        progress.instruction_events > capture.instructions.size() ||
        capture.calls.size() != 8 || capture.get(0x800f84c4u) != 0 ||
        capture.get(0x800f43b0u) != 0)
      capture.contract_failure = true;

    std::ostringstream out;
    out << "{\"program\":\"FEONLY\",\"address\":" << hex(0x800394d4u)
        << ",\"inclusive_end\":" << hex(0x80039573u)
        << ",\"bytes\":160,\"instructions\":40,"
        << "\"source_sha256\":\"5b71620fae4987715d545936b770ac78df30d0b8501120fd3ae5e3abb1a61617\","
        << "\"completed\":" << unsigned(progress.completed)
        << ",\"accepted\":" << accepted << ",\"result\":"
        << capture.binding.result << ",\"contract_failure\":"
        << capture.contract_failure
        << ",\"classification\":\"no direct visual effect\","
        << "\"gameplay_shown\":\"BLOCKED\","
        << "\"fixture_contract\":\"Synthetic full machine at the recovered frontend-exit-cleanup 0x8002F094 boundary. Children 0x800393F0, 0x80038E84, 0x80029B64, 0x8008C274, 0x8006CDE4 and 0x8006AE60 preserve all 32 GPR words and masks plus HI/LO and perform no guest-memory effects. Poll child 0x800392A0 preserves that state except for explicit fully-known V0 results zero then one. 0x80029B64 receives known a0=a1=0; 0x8006CDE4 receives the independently reloaded mode word in a0. These are synthetic callback contracts, not recovered child ABIs or production services.\","
        << "\"parent_call\":{\"pc\":" << hex(capture.parent.pc)
        << ",\"delay\":" << hex(capture.parent.delay_slot_pc)
        << ",\"entry\":" << hex(capture.parent.entry)
        << ",\"argument_count\":" << unsigned(capture.parent.argument_count)
        << ",\"program\":" << unsigned(capture.parent.target_program)
        << ",\"ra\":" << hex(ParentRa) << "},"
        << "\"memory\":{\"active_before\":" << hex(active_before)
        << ",\"active_after\":" << hex(capture.get(0x800f84c4u))
        << ",\"busy_before\":" << hex(busy_before)
        << ",\"busy_after\":" << hex(capture.get(0x800f43b0u))
        << ",\"mode\":" << hex(Mode) << "},"
        << "\"drain\":{\"operations\":" << progress.operations
        << ",\"accesses\":" << progress.accesses << ",\"reads\":"
        << progress.reads << ",\"stores\":" << progress.stores
        << ",\"callbacks\":" << progress.callbacks_completed
        << ",\"poll_attempts\":" << progress.poll_attempts
        << ",\"zero_poll_results\":" << progress.zero_poll_results
        << ",\"instruction_count\":" << progress.instruction_count
        << ",\"initial_active_flag\":";
    word(out, progress.initial_active_flag);
    out << ",\"first_mode_flag\":";
    word(out, progress.first_mode_flag);
    out << ",\"second_mode_flag\":";
    word(out, progress.second_mode_flag);
    out << ",\"instruction_trace\":[";
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
      word(out, call.machine.registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_A0]);
      out << ",\"a1\":";
      word(out, call.machine.registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_A1]);
      out << ",\"sp\":";
      word(out, call.machine.registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_SP]);
      out << ",\"ra\":";
      word(out, call.machine.registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_RA]);
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
           "\"first_drain_child\":\"0x800394E8 -> 0x800393F0\","
           "\"poll_children\":\"0x800394F0 -> 0x800392A0 and zero-result 0x80039500 -> 0x80038E84\","
           "\"remaining_drain_children\":\"0x80039530 -> 0x80029B64, 0x80039538 -> 0x8008C274, 0x80039554 -> 0x8006CDE4, and 0x8003955C -> 0x8006AE60\","
           "\"gameplay\":\"actual advancing native match handoff remains unbound\"}}";
    return out.str();
  } catch (...) {
    return "{\"program\":\"FEONLY\",\"address\":\"0x800394d4\",\"bytes\":160,\"completed\":0,\"accepted\":0,\"result\":0,\"contract_failure\":true,\"classification\":\"no direct visual effect\",\"gameplay_shown\":\"BLOCKED\"}";
  }
}
} // namespace nba97
