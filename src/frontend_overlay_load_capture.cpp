#include "frontend_overlay_load_capture.h"

#include "frontend_overlay_load_adapter.h"

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
constexpr U Handle = 0x80170000u;

struct Call {
  Nba97FrontendOverlayLoadEvent event{};
  Nba97FrontendOverlayLoadMachine machine{};
};

struct Capture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendMainMachine machine{};
  Nba97FrontendMainEvent parent{0x80028accu,
                                0x80028ad0u,
                                0x8007b11cu,
                                0,
                                1,
                                NBA97_FRONTEND_MAIN_SITE_80028ACC,
                                2,
                                NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
  Nba97FrontendOverlayLoadBinding binding{};
  std::array<Nba97FrontendOverlayLoadAccess, 4> access{};
  std::array<U, 8> instructions{};
  std::vector<Call> calls;
  bool contract_failure = false;

  Capture() {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x61000000u + i * 0x101u, 15};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A0] = {Filename, 15};
    machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A1] = {0, 15};
    machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_SP] = {Sp, 15};
    machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_RA] = {ParentRa, 15};
    machine.hi = {0x10203040u, 5};
    machine.lo = {0x50607080u, 10};
    binding.operation_budget = 3;
    binding.io = callback;
    binding.user = this;
    binding.access_journal = access.data();
    binding.access_journal_capacity = access.size();
    binding.instruction_journal = instructions.data();
    binding.instruction_journal_capacity = instructions.size();
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendOverlayLoadEvent *event,
                      Nba97FrontendOverlayLoadMachine *machine) {
    auto &capture = *static_cast<Capture *>(opaque);
    Nba97FrontendOverlayLoadSiteContract contract{};
    if (!event || !machine ||
        !nba97_frontend_overlay_load_site_contract(event->site, &contract) ||
        event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->entry != contract.target ||
        event->argument_count != contract.argument_count ||
        event->target_program != contract.target_program ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A0].word !=
            Filename ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A0].known_mask !=
            15 ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A1].word != 0 ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A1].known_mask !=
            15 ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A2].word != 1 ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_A2].known_mask !=
            15 ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_RA].word !=
            0x8007b12cu ||
        machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_RA].known_mask !=
            15) {
      capture.contract_failure = true;
      return 0;
    }
    capture.calls.push_back({*event, *machine});
    machine->registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_V0] = {Handle, 15};
    return 1;
  }
};

std::string hex(U value) {
  std::ostringstream out;
  out << "\"0x" << std::hex << std::setw(8) << std::setfill('0') << value
      << '\"';
  return out.str();
}

void word(std::ostringstream &out, const Nba97FrontendOverlayLoadWord &value) {
  out << "{\"word\":" << hex(value.word)
      << ",\"known_mask\":" << unsigned(value.known_mask) << '}';
}

void machine(std::ostringstream &out,
             const Nba97FrontendOverlayLoadMachine &value) {
  out << "{\"gpr\":[";
  for (unsigned reg = 0; reg < 32; ++reg) {
    if (reg)
      out << ',';
    word(out, value.registers.gpr[reg]);
  }
  out << "],\"hi\":";
  word(out, value.hi);
  out << ",\"lo\":";
  word(out, value.lo);
  out << '}';
}
} // namespace

std::string captureFrontendOverlayLoad() {
  try {
    Capture capture;
    Nba97GameTextMemory memory_value{&capture.region, 1};
    Nba97FrontendMainCalleeOutcome outcome =
        NBA97_FRONTEND_MAIN_CALLEE_TRANSFERRED;
    const int accepted = nba97_frontend_overlay_load_from_frontend_main(
        &capture.binding, &memory_value, &capture.parent, &capture.machine,
        &outcome);
    const auto &progress = capture.binding.progress;
    if (accepted != 1 || outcome != NBA97_FRONTEND_MAIN_CALLEE_RETURNED ||
        capture.binding.result != NBA97_TEXT_COMPLETE ||
        capture.binding.invocations != 1 || capture.binding.completions != 1 ||
        progress.operations != 3 || progress.accesses != 2 ||
        progress.reads != 1 || progress.stores != 1 ||
        progress.callbacks_completed != 1 || progress.instruction_count != 8 ||
        progress.instruction_events != 8 || progress.access_events != 2 ||
        capture.calls.size() != 1 ||
        progress.machine.registers.gpr[NBA97_FRONTEND_OVERLAY_LOAD_V0].word !=
            Handle)
      capture.contract_failure = true;

    std::ostringstream out;
    out << "{\"program\":\"FEONLY\",\"address\":" << hex(0x8007b11cu)
        << ",\"inclusive_end\":" << hex(0x8007b13bu)
        << ",\"bytes\":32,\"instructions\":8,"
        << "\"source_sha256\":\"97d8f0e4eb51bd581d1431e5995abb4ea56b67408568f334d91a8b93e61029e2\","
        << "\"accepted\":" << accepted
        << ",\"result\":" << capture.binding.result
        << ",\"completed\":" << unsigned(progress.completed)
        << ",\"contract_failure\":" << capture.contract_failure
        << ",\"classification\":\"no direct visual effect\","
        << "\"gameplay_shown\":\"BLOCKED\","
        << "\"fixture_contract\":\"A synthetic standalone full machine and parent event reproduce the committed frontend_main 0x80028ACC boundary with fully-known a0=0x80024854, a1=0 and ra=0x80028AD4. The synthetic unresolved 0x8007B15C child receives live a0/a1 plus delay-slot a2=1, preserves the full machine and guest memory, and supplies only fully-known v0=0x80170000. The return is fixture data, not a recovered child ABI or production loader service.\","
        << "\"parent_call\":{\"pc\":" << hex(capture.parent.pc)
        << ",\"delay\":" << hex(capture.parent.delay_slot_pc)
        << ",\"entry\":" << hex(capture.parent.entry)
        << ",\"argument_count\":" << unsigned(capture.parent.argument_count)
        << ",\"program\":" << unsigned(capture.parent.target_program)
        << ",\"a0\":";
    word(out, capture.binding.progress.forwarded_a0);
    out << ",\"a1\":";
    word(out, capture.binding.progress.forwarded_a1);
    out << "},\"owner\":{\"operations\":" << progress.operations
        << ",\"accesses\":" << progress.accesses
        << ",\"reads\":" << progress.reads
        << ",\"stores\":" << progress.stores
        << ",\"callbacks\":" << progress.callbacks_completed
        << ",\"instruction_count\":" << progress.instruction_count
        << ",\"delay_a2\":";
    word(out, progress.delay_a2);
    out << ",\"child_return\":";
    word(out, progress.child_return);
    out << ",\"instruction_trace\":[";
    for (std::size_t i = 0;
         i < std::min(progress.instruction_events, capture.instructions.size());
         ++i) {
      if (i)
        out << ',';
      out << hex(capture.instructions[i]);
    }
    out << "],\"access_journal\":[";
    for (std::size_t i = 0;
         i < std::min(progress.access_events, capture.access.size()); ++i) {
      if (i)
        out << ',';
      const auto &event = capture.access[i];
      out << "{\"pc\":" << hex(event.pc)
          << ",\"address\":" << hex(event.address)
          << ",\"value\":" << hex(event.value)
          << ",\"operation\":" << event.operation
          << ",\"width\":" << unsigned(event.width)
          << ",\"known_mask\":" << unsigned(event.known_mask)
          << ",\"kind\":" << unsigned(event.kind) << '}';
    }
    out << "],\"call_sequence\":[";
    for (std::size_t i = 0; i < capture.calls.size(); ++i) {
      if (i)
        out << ',';
      const auto &call = capture.calls[i];
      out << "{\"pc\":" << hex(call.event.pc)
          << ",\"delay\":" << hex(call.event.delay_slot_pc)
          << ",\"target\":" << hex(call.event.entry)
          << ",\"argument_count\":" << unsigned(call.event.argument_count)
          << ",\"invocation\":" << call.event.invocation
          << ",\"machine\":";
      machine(out, call.machine);
      out << '}';
    }
    out << "]},\"final_machine\":";
    machine(out, progress.machine);
    out << ",\"next_unbound_boundary\":{"
           "\"loader_child\":\"0x8007B124 -> 0x8007B15C, whose transitive 0x8007B1D0 callee consumes a0/a1/a2\","
           "\"production\":\"filesystem-backed FEONLY overlay loading and the later advancing GAMELOAD match handoff remain unbound\","
           "\"gameplay\":\"actual advancing native match loop and rendered court/player state remain unbound\"}}";
    return out.str();
  } catch (...) {
    return "{\"program\":\"FEONLY\",\"address\":\"0x8007b11c\",\"bytes\":32,\"completed\":0,\"accepted\":0,\"result\":0,\"contract_failure\":true,\"classification\":\"no direct visual effect\",\"gameplay_shown\":\"BLOCKED\"}";
  }
}
} // namespace nba97
