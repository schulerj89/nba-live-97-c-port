#include "frontend_clock_read_capture.h"

#include "frontend_clock_read_adapter.h"

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
constexpr U ClockAddress = 0x800d9ab8u;
constexpr std::array<U, 10> Pcs{{
    0x8002efdcu, 0x8002efe4u, 0x8002eff0u, 0x8002f000u, 0x8002f010u,
    0x8002f018u, 0x8002f034u, 0x8002f048u, 0x8002f050u, 0x8002f060u}};
constexpr std::array<U, 10> Delays{{
    0x8002efe0u, 0x8002efe8u, 0x8002eff4u, 0x8002f004u, 0x8002f014u,
    0x8002f01cu, 0x8002f038u, 0x8002f04cu, 0x8002f054u, 0x8002f064u}};
constexpr std::array<U, 10> Targets{{
    0x8007b2bcu, 0x8008da5cu, 0x8006b6a0u, 0x8006fcf0u, 0x80039260u,
    0x8008da5cu, 0x80092c34u, 0x80028c28u, 0x8006faa0u, 0x80028cf4u}};
constexpr std::array<unsigned, 10> Args{{3, 0, 0, 0, 0, 0, 1, 0, 0, 1}};

struct Call {
  Nba97FrontendExitWaitEvent event{};
  Nba97FrontendExitWaitMachine machine{};
  bool present = false;
};

struct Capture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendExitWaitContext wait{};
  Nba97FrontendExitWaitProgress wait_progress{};
  Nba97FrontendClockReadBinding clock{};
  Nba97FrontendClockReadAdapterProgress adapter{};
  std::array<Nba97FrontendExitWaitAccess, 16> wait_access{};
  std::array<U, 64> wait_instructions{};
  std::array<Nba97FrontendClockReadAccess, 2> clock_access{};
  std::array<U, 8> clock_instructions{};
  std::array<Call, 10> calls{};
  bool contract_failure = false;

  Capture() {
    for (unsigned i = 0; i < 32; ++i)
      wait.machine.registers.gpr[i] = {0x63000000u + i * 0x101u, 15};
    wait.machine.registers.gpr[0] = {0, 15};
    wait.machine.registers.gpr[16] = {0x11223344u, 5};
    wait.machine.registers.gpr[29] = {Sp, 15};
    wait.machine.registers.gpr[31] = {ParentRa, 15};
    wait.machine.hi = {0x10203040u, 6};
    wait.machine.lo = {0x50607080u, 9};
    put(0x80017268u, Handle);
    put(0x8002149cu, Secondary);
    put(ClockAddress, 1000);
    wait.memory = {&region, 1};
    wait.operation_budget = 19;
    wait.io = callback;
    wait.user = this;
    wait.access_journal = wait_access.data();
    wait.access_journal_capacity = wait_access.size();
    wait.instruction_journal = wait_instructions.data();
    wait.instruction_journal_capacity = wait_instructions.size();
    clock.operation_budget = 1;
    clock.access_journal = clock_access.data();
    clock.access_journal_capacity = clock_access.size();
    clock.instruction_journal = clock_instructions.data();
    clock.instruction_journal_capacity = clock_instructions.size();
  }

  bool extent(U address, U width = 4) const {
    return address >= Base && width <= Size && address - Base <= Size - width;
  }
  void put(U address, U value, U width = 4) {
    if (!extent(address, width)) {
      contract_failure = true;
      return;
    }
    for (U i = 0; i < width; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known) known[address - Base + i] = 1;
    }
  }
  U get(U address, U width = 4) {
    if (!extent(address, width)) {
      contract_failure = true;
      return 0;
    }
    U result = 0;
    for (U i = 0; i < width; ++i)
      result |= U(bytes[address - Base + i]) << (i * 8u);
    return result;
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendExitWaitEvent *event,
                      Nba97FrontendExitWaitMachine *machine) {
    auto &capture = *static_cast<Capture *>(opaque);
    if (!event || !machine || event->site == 0 || event->site >= 11 ||
        event->pc != Pcs[event->site - 1] ||
        event->delay_slot_pc != Delays[event->site - 1] ||
        event->entry != Targets[event->site - 1] ||
        event->argument_count != Args[event->site - 1] ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        event->invocation != 1 ||
        machine->registers.gpr[31].word != event->pc + 8u ||
        machine->registers.gpr[31].known_mask != 15 ||
        capture.calls[event->site - 1].present) {
      capture.contract_failure = true;
      return 0;
    }
    capture.calls[event->site - 1] = {*event, *machine, true};
    if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFDC &&
        (machine->registers.gpr[4].word != Handle ||
         machine->registers.gpr[5].word != 100 ||
         machine->registers.gpr[6].word != UINT32_MAX)) {
      capture.contract_failure = true;
      return 0;
    }
    if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFF0 ||
        event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002F000)
      machine->registers.gpr[2] = {0, 15};
    if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002F010)
      capture.put(ClockAddress, 1361);
    return 1;
  }
};

std::string hex(U value) {
  std::ostringstream out;
  out << "\"0x" << std::hex << std::setw(8) << std::setfill('0') << value
      << '\"';
  return out.str();
}
void word(std::ostringstream &out, const Nba97FrontendClockReadWord &value) {
  out << "{\"word\":" << hex(value.word)
      << ",\"known_mask\":" << unsigned(value.known_mask) << '}';
}
void machine(std::ostringstream &out,
             const Nba97FrontendClockReadMachine &value) {
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
void access(std::ostringstream &out,
            const Nba97FrontendClockReadAccess &event) {
  out << "{\"pc\":" << hex(event.pc) << ",\"address\":"
      << hex(event.address) << ",\"value\":" << hex(event.value)
      << ",\"operation\":" << event.operation
      << ",\"width\":" << unsigned(event.width)
      << ",\"known_mask\":" << unsigned(event.known_mask)
      << ",\"kind\":" << unsigned(event.kind) << '}';
}
} // namespace

std::string captureFrontendClockRead() {
  try {
    Capture capture;
    const U clock_before = capture.get(ClockAddress);
    const int result = nba97_frontend_exit_wait_with_recovered_clock(
        &capture.wait, &capture.clock, &capture.wait_progress,
        &capture.adapter);
    const U clock_after = capture.get(ClockAddress);
    capture.calls[1] = {capture.adapter.initial_event,
                        capture.adapter.initial_parent_machine, true};
    capture.calls[5] = {capture.adapter.loop_event,
                        capture.adapter.loop_parent_machine, true};
    if (result != NBA97_TEXT_COMPLETE || !capture.wait_progress.completed ||
        capture.wait_progress.operations != 19 ||
        capture.wait_progress.accesses != 9 ||
        capture.wait_progress.callbacks_completed != 10 ||
        capture.wait_progress.instruction_count != 50 ||
        capture.wait_progress.instruction_events > capture.wait_instructions.size() ||
        capture.wait_progress.access_events > capture.wait_access.size() ||
        capture.clock.invocations != 2 || capture.clock.completions != 2 ||
        capture.adapter.initial_invocations != 1 ||
        capture.adapter.loop_invocations != 1 ||
        capture.adapter.initial_progress.instruction_count != 4 ||
        capture.adapter.loop_progress.instruction_count != 4 ||
        capture.adapter.initial_progress.loaded_clock.word != 1000 ||
        capture.adapter.loop_progress.loaded_clock.word != 1361 ||
        clock_before != 1000 || clock_after != 1361)
      capture.contract_failure = true;
    for (const auto &call : capture.calls)
      if (!call.present) capture.contract_failure = true;

    std::ostringstream out;
    out << "{\"program\":\"FEONLY\",\"address\":" << hex(0x8008da5cu)
        << ",\"inclusive_end\":" << hex(0x8008da6bu)
        << ",\"bytes\":16,\"instructions\":4,"
        << "\"source_sha256\":\"9bf283cf0c65c4bd13e3e94df28927dc756088764e78bf2e59298f9faeef85c0\","
        << "\"completed\":" << unsigned(capture.wait_progress.completed)
        << ",\"result\":" << result << ",\"contract_failure\":"
        << capture.contract_failure
        << ",\"classification\":\"no direct visual effect\","
        << "\"gameplay_shown\":\"BLOCKED\","
        << "\"fixture_contract\":\"Synthetic standalone machine and retained memory enter the recovered frontend-exit-wait owner, which genuinely executes both natural clock call sites through the recovered 0x8008DA5C reader. Guest clock memory is 1000 for the first call and an explicit unbound 0x80039260 fixture writes only clock RAM with 1361 before the loop clock call. The other seven fallback full-machine child services validate typed PC, delay, target, invocation and argument count, preserve all GPR words/masks plus HI/LO, and make no guest-memory changes; poll fixtures return V0=0. The wait owner's handle and secondary-global stores remain recovered source effects. Existing semantic owners for 0x8007B2BC, 0x80092C34, and 0x8006B6A0 lack the retained-state and output-machine transport required by these full-machine boundaries.\","
        << "\"clock_memory\":{\"address\":" << hex(ClockAddress)
        << ",\"before\":" << hex(clock_before)
        << ",\"after_fixture_update\":" << hex(clock_after) << "},"
        << "\"wait\":{\"operations\":" << capture.wait_progress.operations
        << ",\"accesses\":" << capture.wait_progress.accesses
        << ",\"callbacks\":" << capture.wait_progress.callbacks_completed
        << ",\"clock_callbacks\":" << capture.adapter.invocations
        << ",\"instruction_count\":"
        << capture.wait_progress.instruction_count
        << ",\"instruction_trace\":[";
    const std::size_t wait_instructions = std::min(
        capture.wait_progress.instruction_events,
        capture.wait_instructions.size());
    for (std::size_t i = 0; i < wait_instructions; ++i) {
      if (i) out << ',';
      out << hex(capture.wait_instructions[i]);
    }
    out << "],\"access_journal\":[";
    const std::size_t wait_accesses = std::min(
        capture.wait_progress.access_events, capture.wait_access.size());
    for (std::size_t i = 0; i < wait_accesses; ++i) {
      const auto &event = capture.wait_access[i];
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
          << call.event.invocation << ",\"machine\":";
      machine(out, call.machine);
      out << '}';
    }
    out << "]},\"clock\":{\"reader_instruction_trace\":[";
    const std::size_t clock_instructions = std::min(
        capture.adapter.loop_progress.instruction_events,
        capture.clock_instructions.size());
    for (std::size_t i = 0; i < clock_instructions; ++i) {
      if (i) out << ',';
      out << hex(capture.clock_instructions[i]);
    }
    out << "],\"initial\":{\"parent_pc\":"
        << hex(capture.adapter.initial_event.pc) << ",\"result\":"
        << capture.adapter.initial_result << ",\"operations\":"
        << capture.adapter.initial_progress.operations
        << ",\"instruction_count\":"
        << capture.adapter.initial_progress.instruction_count
        << ",\"loaded_clock\":";
    word(out, capture.adapter.initial_progress.loaded_clock);
    out << ",\"access\":";
    access(out, capture.adapter.initial_access);
    out << ",\"final_machine\":";
    machine(out, capture.adapter.initial_progress.machine);
    out << "},\"loop\":{\"parent_pc\":"
        << hex(capture.adapter.loop_event.pc) << ",\"result\":"
        << capture.adapter.loop_result << ",\"operations\":"
        << capture.adapter.loop_progress.operations
        << ",\"instruction_count\":"
        << capture.adapter.loop_progress.instruction_count
        << ",\"loaded_clock\":";
    word(out, capture.adapter.loop_progress.loaded_clock);
    out << ",\"access\":";
    access(out, capture.adapter.loop_access);
    out << ",\"final_machine\":";
    machine(out, capture.adapter.loop_progress.machine);
    out << "}},\"final_machine\":";
    machine(out, capture.wait_progress.machine);
    out << ",\"next_unbound_boundary\":{"
           "\"earliest_production\":\"unowned FEONLY overlay-entry JAL 0x8007B838 and first startup service 0x80028810 -> 0x8007B844\","
           "\"next_wait_child\":\"full-machine binding 0x8002EFDC -> 0x8007B2BC; the existing semantic voice-handle owner lacks retained-state and output-machine transport\","
           "\"after_clock\":\"0x8002EFF0 -> 0x8006B6A0 remains an unbound full-machine boundary; its semantic music-status owner lacks output-machine transport\","
           "\"gameplay_dependency\":\"actual loader handoff and advancing native court/player lifecycle remain unbound\"}}";
    return out.str();
  } catch (...) {
    return "{\"program\":\"FEONLY\",\"address\":\"0x8008da5c\",\"bytes\":16,\"completed\":0,\"result\":0,\"contract_failure\":true,\"classification\":\"no direct visual effect\",\"gameplay_shown\":\"BLOCKED\"}";
  }
}
} // namespace nba97
