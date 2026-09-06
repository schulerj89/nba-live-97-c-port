#include "frontend_exit_cleanup_capture.h"

#include "frontend_exit_cleanup_adapter.h"

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
constexpr U ParentRa = 0x80028ab0u;
constexpr U CleanupSelector = 0xffffffffu;
constexpr U ReleasePointer = 0x80145678u;

struct Call {
  Nba97FrontendExitCleanupEvent event{};
  Nba97FrontendExitCleanupMachine machine{};
};

struct Capture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendMainMachine machine{};
  Nba97FrontendMainEvent parent{0x80028aa8u,
                                0x80028aacu,
                                0x8002f084u,
                                1,
                                1,
                                NBA97_FRONTEND_MAIN_SITE_80028AA8,
                                0,
                                NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
  Nba97FrontendMainCalleeOutcome outcome =
      NBA97_FRONTEND_MAIN_CALLEE_TRANSFERRED;
  Nba97FrontendExitCleanupBinding binding{};
  std::array<Nba97FrontendExitCleanupAccess, 8> access{};
  std::array<U, 32> instructions{};
  std::vector<Call> calls;
  bool contract_failure = false;

  Capture() {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x63000000u + i * 0x101u, 15};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[NBA97_FRONTEND_MAIN_SP] = {Sp, 15};
    machine.registers.gpr[NBA97_FRONTEND_MAIN_RA] = {ParentRa, 15};
    machine.hi = {0x10203040u, 5};
    machine.lo = {0x50607080u, 10};
    put(0x80021d6cu, CleanupSelector);
    put(0x8001502cu, ReleasePointer);
    binding.operation_budget = 10;
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
                      const Nba97FrontendExitCleanupEvent *event,
                      Nba97FrontendExitCleanupMachine *machine) {
    auto &capture = *static_cast<Capture *>(opaque);
    Nba97FrontendExitCleanupSiteContract contract{};
    const std::size_t index = capture.calls.size();
    if (!event || !machine || index >= 5 ||
        !nba97_frontend_exit_cleanup_site_contract(event->site, &contract) ||
        event->site != index + 1 || event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->entry != contract.target ||
        event->argument_count != contract.argument_count ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        event->invocation != 1 ||
        machine->registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_RA].word !=
            event->pc + 8u ||
        machine->registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_RA].known_mask !=
            15) {
      capture.contract_failure = true;
      return 0;
    }
    if (event->site == NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F0A4 &&
        (machine->registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_A0].word !=
             CleanupSelector ||
         machine->registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_A0].known_mask !=
             15)) {
      capture.contract_failure = true;
      return 0;
    }
    if (event->site == NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F0C0 &&
        (machine->registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_A0].word !=
             ReleasePointer ||
         machine->registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_A0].known_mask !=
             15)) {
      capture.contract_failure = true;
      return 0;
    }
    capture.calls.push_back({*event, *machine});
    return 1;
  }
};

std::string hex(U value) {
  std::ostringstream out;
  out << "\"0x" << std::hex << std::setw(8) << std::setfill('0') << value
      << '\"';
  return out.str();
}

void word(std::ostringstream &out, const Nba97FrontendExitCleanupWord &value) {
  out << "{\"word\":" << hex(value.word)
      << ",\"known_mask\":" << unsigned(value.known_mask) << '}';
}
} // namespace

std::string captureFrontendExitCleanup() {
  try {
    Capture capture;
    const U before_release = capture.get(0x8001502cu);
    Nba97GameTextMemory memory{&capture.region, 1};
    const int accepted = nba97_frontend_exit_cleanup_from_frontend_main(
        &capture.binding, &memory, &capture.parent, &capture.machine,
        &capture.outcome);
    const auto &progress = capture.binding.progress;
    if (accepted != 1 || capture.binding.result != NBA97_TEXT_COMPLETE ||
        capture.outcome != NBA97_FRONTEND_MAIN_CALLEE_RETURNED ||
        progress.operations != 10 || progress.accesses != 5 ||
        progress.reads != 3 || progress.stores != 2 ||
        progress.callbacks_completed != 5 || progress.instruction_count != 25 ||
        progress.access_events > capture.access.size() ||
        progress.instruction_events > capture.instructions.size() ||
        capture.calls.size() != 5 || capture.get(0x8001502cu) != 0)
      capture.contract_failure = true;

    std::ostringstream out;
    out << "{\"program\":\"FEONLY\",\"address\":" << hex(0x8002f084u)
        << ",\"inclusive_end\":" << hex(0x8002f0e7u)
        << ",\"bytes\":100,\"instructions\":25,"
        << "\"source_sha256\":\"38b3b7e879958bf82f3c214dea99f4b4bdb0c69f77eae68ae32692c7c9da29ec\","
        << "\"completed\":" << unsigned(progress.completed)
        << ",\"accepted\":" << accepted << ",\"result\":"
        << capture.binding.result << ",\"contract_failure\":"
        << capture.contract_failure
        << ",\"classification\":\"no direct visual effect\","
        << "\"gameplay_shown\":\"BLOCKED\","
        << "\"fixture_contract\":\"Synthetic full machine at the recovered frontend-main 0x80028AA8 boundary. All five unowned FEONLY children accept exactly their typed call PC, delay, target, invocation and argument count, preserve all 32 GPR words and masks plus HI/LO, and perform no guest-memory effects. Call 0x80028C90 receives the complete raw signed selector word from 0x80021D6C; 0x8007760C receives the complete resource pointer from 0x8001502C. This standalone CPU receipt does not bind production services or gameplay.\","
        << "\"parent_call\":{\"pc\":" << hex(capture.parent.pc)
        << ",\"delay\":" << hex(capture.parent.delay_slot_pc)
        << ",\"entry\":" << hex(capture.parent.entry)
        << ",\"argument_count\":" << unsigned(capture.parent.argument_count)
        << ",\"program\":" << unsigned(capture.parent.target_program)
        << ",\"ra\":" << hex(ParentRa) << "},"
        << "\"memory\":{\"cleanup_selector\":" << hex(CleanupSelector)
        << ",\"release_before\":" << hex(before_release)
        << ",\"release_after\":" << hex(capture.get(0x8001502cu)) << "},"
        << "\"cleanup\":{\"operations\":" << progress.operations
        << ",\"accesses\":" << progress.accesses << ",\"reads\":"
        << progress.reads << ",\"stores\":" << progress.stores
        << ",\"callbacks\":" << progress.callbacks_completed
        << ",\"instruction_count\":" << progress.instruction_count
        << ",\"loaded_cleanup_selector\":";
    word(out, progress.loaded_cleanup_selector);
    out << ",\"loaded_release_flag\":";
    word(out, progress.loaded_release_flag);
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
      word(out, call.machine.registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_A0]);
      out << ",\"sp\":";
      word(out, call.machine.registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_SP]);
      out << ",\"ra\":";
      word(out, call.machine.registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_RA]);
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
           "\"earliest_production\":\"unowned FEONLY overlay-entry JAL 0x8007B838 and first startup service 0x80028810 -> 0x8007B844\","
           "\"next_cleanup_child\":\"0x8002F08C -> 0x8002EFBC\","
           "\"following_cleanup_children\":\"0x8002F094 -> 0x800394D4, 0x8002F0A4 -> 0x80028C90, conditional 0x8002F0C0 -> 0x8007760C, and 0x8002F0D0 -> 0x80076540\","
           "\"remaining_main_chain\":\"0x80028AB0 -> 0x80028E08, loader/copy, and GAMELOAD transfer remain unbound\"}}";
    return out.str();
  } catch (...) {
    return "{\"program\":\"FEONLY\",\"address\":\"0x8002f084\",\"bytes\":100,\"completed\":0,\"accepted\":0,\"result\":0,\"contract_failure\":true,\"classification\":\"no direct visual effect\",\"gameplay_shown\":\"BLOCKED\"}";
  }
}
} // namespace nba97
