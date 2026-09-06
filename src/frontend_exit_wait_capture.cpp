#include "frontend_exit_wait_capture.h"

#include "frontend_exit_wait_adapter.h"

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
constexpr U ParentRa = 0x8002f094u;
constexpr U Handle = 0x80145678u;
constexpr U Secondary = 0x80123458u;

struct Call {
  Nba97FrontendExitWaitEvent event{};
  Nba97FrontendExitWaitMachine machine{};
};

struct Capture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendExitCleanupMachine machine{};
  Nba97FrontendExitCleanupEvent parent{0x8002f08cu,
                                       0x8002f090u,
                                       0x8002efbcu,
                                       1,
                                       1,
                                       NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F08C,
                                       0,
                                       NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
  Nba97FrontendExitWaitBinding binding{};
  std::array<Nba97FrontendExitWaitAccess, 16> access{};
  std::array<U, 64> instructions{};
  std::vector<Call> calls;
  bool contract_failure = false;

  Capture() {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x52000000u + i * 0x101u, 15};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[29] = {Sp, 15};
    machine.registers.gpr[31] = {ParentRa, 15};
    machine.registers.gpr[16] = {0x11223344u, 15};
    machine.hi = {0x10203040u, 5};
    machine.lo = {0x50607080u, 10};
    put(0x80017268u, Handle);
    put(0x8002149cu, Secondary);
    binding.operation_budget = 19;
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
                      const Nba97FrontendExitWaitEvent *event,
                      Nba97FrontendExitWaitMachine *machine) {
    auto &capture = *static_cast<Capture *>(opaque);
    Nba97FrontendExitWaitSiteContract contract{};
    const std::size_t index = capture.calls.size();
    if (!event || !machine || index >= 10 ||
        !nba97_frontend_exit_wait_site_contract(event->site, &contract) ||
        event->site != index + 1 || event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->entry != contract.target ||
        event->argument_count != contract.argument_count ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        event->invocation != 1 || machine->registers.gpr[31].word != event->pc + 8u ||
        machine->registers.gpr[31].known_mask != 15) {
      capture.contract_failure = true;
      return 0;
    }
    if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFDC &&
        (machine->registers.gpr[4].word != Handle ||
         machine->registers.gpr[5].word != 100 ||
         machine->registers.gpr[6].word != UINT32_MAX)) {
      capture.contract_failure = true;
      return 0;
    }
    if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002F034 &&
        machine->registers.gpr[4].word != Handle) {
      capture.contract_failure = true;
      return 0;
    }
    if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002F060 &&
        machine->registers.gpr[4].word != Secondary) {
      capture.contract_failure = true;
      return 0;
    }
    capture.calls.push_back({*event, *machine});
    if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFE4)
      machine->registers.gpr[2] = {1000, 15};
    else if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFF0 ||
             event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002F000)
      machine->registers.gpr[2] = {0, 15};
    else if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002F018)
      machine->registers.gpr[2] = {1361, 15};
    return 1;
  }
};

std::string hex(U value) {
  std::ostringstream out;
  out << "\"0x" << std::hex << std::setw(8) << std::setfill('0') << value
      << '\"';
  return out.str();
}
void word(std::ostringstream &out, const Nba97FrontendExitWaitWord &value) {
  out << "{\"word\":" << hex(value.word)
      << ",\"known_mask\":" << unsigned(value.known_mask) << '}';
}
} // namespace

std::string captureFrontendExitWait() {
  try {
    Capture capture;
    const U before_handle = capture.get(0x80017268u);
    const U before_secondary = capture.get(0x8002149cu);
    Nba97GameTextMemory memory{&capture.region, 1};
    const int accepted = nba97_frontend_exit_wait_from_frontend_exit_cleanup(
        &capture.binding, &memory, &capture.parent, &capture.machine);
    const auto &progress = capture.binding.progress;
    if (accepted != 1 || capture.binding.result != NBA97_TEXT_COMPLETE ||
        progress.operations != 19 || progress.accesses != 9 ||
        progress.reads != 5 || progress.stores != 4 ||
        progress.callbacks_completed != 10 || progress.instruction_count != 50 ||
        progress.loop_iterations != 1 ||
        progress.exit_path != NBA97_FRONTEND_EXIT_WAIT_EXIT_DEADLINE ||
        progress.access_events > capture.access.size() ||
        progress.instruction_events > capture.instructions.size() ||
        capture.calls.size() != 10 || capture.get(0x80017268u) != UINT32_MAX ||
        capture.get(0x8002149cu) != 0)
      capture.contract_failure = true;

    std::ostringstream out;
    out << "{\"program\":\"FEONLY\",\"address\":" << hex(0x8002efbcu)
        << ",\"inclusive_end\":" << hex(0x8002f083u)
        << ",\"bytes\":200,\"instructions\":50,"
        << "\"source_sha256\":\"45eed4157e3ece4487b1c0c8ea03ed780461937168a8d38e05853200ebf6ad53\","
        << "\"completed\":" << unsigned(progress.completed)
        << ",\"accepted\":" << accepted << ",\"result\":"
        << capture.binding.result << ",\"contract_failure\":"
        << capture.contract_failure
        << ",\"classification\":\"no direct visual effect\","
        << "\"gameplay_shown\":\"BLOCKED\","
        << "\"fixture_contract\":\"Synthetic standalone full machine at recovered cleanup call 0x8002F08C. All ten unbound full-machine FEONLY child services accept only their typed call PC, delay, target, invocation and argument count and preserve all GPR words/masks plus HI/LO except documented V0 results: initial clock 1000, both polls 0, final clock 1361. Existing semantic owners for voice handles 0x8007B2BC and 0x80092C34 and music status 0x8006B6A0 are not composed because they do not expose the retained-state and output-machine transport required by this source boundary. Children perform no guest-memory effects. The fixture drives one deadline-expiry loop and does not bind production services or gameplay.\","
        << "\"parent_call\":{\"pc\":" << hex(capture.parent.pc)
        << ",\"delay\":" << hex(capture.parent.delay_slot_pc)
        << ",\"entry\":" << hex(capture.parent.entry)
        << ",\"argument_count\":" << unsigned(capture.parent.argument_count)
        << ",\"program\":" << unsigned(capture.parent.target_program)
        << ",\"ra\":" << hex(ParentRa) << "},"
        << "\"branch_state\":{\"exit_path\":" << unsigned(progress.exit_path)
        << ",\"loop_iterations\":" << progress.loop_iterations
        << ",\"initial_handle\":";
    word(out, progress.initial_handle);
    out << ",\"deadline\":";
    word(out, progress.deadline);
    out << ",\"first_poll\":";
    word(out, progress.first_poll_result);
    out << ",\"second_poll\":";
    word(out, progress.second_poll_result);
    out << ",\"clock\":";
    word(out, progress.clock_result);
    out << "},\"memory\":{\"handle_before\":" << hex(before_handle)
        << ",\"handle_after\":" << hex(capture.get(0x80017268u))
        << ",\"secondary_before\":" << hex(before_secondary)
        << ",\"secondary_after\":" << hex(capture.get(0x8002149cu))
        << "},\"wait\":{\"operations\":" << progress.operations
        << ",\"accesses\":" << progress.accesses << ",\"reads\":"
        << progress.reads << ",\"stores\":" << progress.stores
        << ",\"callbacks\":" << progress.callbacks_completed
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
          << call.event.invocation << ",\"machine\":{\"gpr\":[";
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
           "\"next_wait_child\":\"full-machine binding 0x8002EFDC -> 0x8007B2BC; an existing semantic voice-handle owner lacks retained-state and output-machine transport\","
           "\"following_wait_children\":\"0x8008DA5C, 0x8006B6A0, 0x8006FCF0, 0x80039260, 0x80092C34, 0x80028C28, 0x8006FAA0, and 0x80028CF4 remain typed services\","
           "\"next_cleanup_child\":\"after this owner, cleanup continues at 0x8002F094 -> 0x800394D4\","
           "\"gameplay_dependency\":\"actual loader handoff and advancing native court/player loop remain unbound\"}}";
    return out.str();
  } catch (...) {
    return "{\"program\":\"FEONLY\",\"address\":\"0x8002efbc\",\"bytes\":200,\"completed\":0,\"accepted\":0,\"result\":0,\"contract_failure\":true,\"classification\":\"no direct visual effect\",\"gameplay_shown\":\"BLOCKED\"}";
  }
}
} // namespace nba97
