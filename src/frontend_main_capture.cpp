#include "frontend_main_capture.h"

#include "frontend_main_adapter.h"
#include "user_setup_session.hpp"

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
constexpr U Context = 0x800214f0u;
constexpr U Allocation = 0x80140000u;
constexpr U Roster = 0x80160000u;
constexpr U Sp = 0x801f0000u;
constexpr U Handle = 0x80170000u;
constexpr U LoadSize = 0x1000u;
constexpr U GameEntry = 0x801e1410u;
constexpr std::array<U, 43> DispatchTargets{
    {0x8003fa3c, 0x8003fb6c, 0x8003fc34, 0x8003fc78, 0x8003fca8,
     0x8003fcf4, 0x8003fd74, 0x8003fe58, 0x8003fe98, 0x8003ff10,
     0x8004005c, 0x8004006c, 0x800400f0, 0x80040120, 0x80040154,
     0x80040184, 0x80040194, 0x800401c0, 0x800401fc, 0x8004028c,
     0x800402d8, 0x800402e8, 0x80040350, 0x80040360, 0x80040370,
     0x80040380, 0x80040390, 0x80040410, 0x80040474, 0x800405d8,
     0x80040558, 0x80040658, 0x800406bc, 0x800406fc, 0x8004070c,
     0x8004071c, 0x8004072c, 0x8004073c, 0x8004076c, 0x8004077c,
     0x8004009c, 0x800400ac, 0x800407d4}};

struct MainCall {
  Nba97FrontendMainEvent event{};
  Nba97FrontendMainMachine machine{};
};

struct ExpectedDispatchCall {
  U pc;
  U target;
  U delay;
  std::uint8_t arguments;
  std::size_t invocation;
};

constexpr std::array<ExpectedDispatchCall, 42> ExpectedDispatchCalls{{
    {0x8003f8c8u, 0x8003f7b0u, 0x8003f8ccu, 2, 1},
    {0x8003f8dcu, 0x800770d4u, 0x8003f8e0u, 3, 1},
    {0x8003f8f4u, 0x80030cdcu, 0x8003f8f8u, 0, 1},
    {0x8003f8fcu, 0x80030308u, 0x8003f900u, 0, 1},
    {0x8003f92cu, 0x8003d2a4u, 0x8003f930u, 0, 1},
    {0x8003f97cu, 0x800459c8u, 0x8003f980u, 0, 1},
    {0x8003fa08u, 0x80031a88u, 0x8003fa0cu, 1, 1},
    {0x8003fcf4u, 0x80037010u, 0x8003fcf8u, 0, 1},
    {0x8003fd3cu, 0x80061674u, 0x8003fd40u, 1, 1},
    {0x8003fd44u, 0x80046d24u, 0x8003fd48u, 0, 1},
    {0x8003fd4cu, 0x8003e7a8u, 0x8003fd50u, 0, 1},
    {0x800407e8u, 0x80028b8cu, 0x800407ecu, 0, 1},
    {0x800407f0u, 0x800804e8u, 0x800407f4u, 1, 1},
    {0x800407f8u, 0x80028b8cu, 0x800407fcu, 0, 1},
    {0x80040850u, 0x8005851cu, 0x80040854u, 1, 1},
    {0x80040868u, 0x8005851cu, 0x8004086cu, 1, 1},
    {0x80040900u, 0x800909a8u, 0x80040904u, 3, 1},
    {0x80040964u, 0x800909a8u, 0x80040968u, 3, 1},
    {0x80040900u, 0x800909a8u, 0x80040904u, 3, 2},
    {0x80040964u, 0x800909a8u, 0x80040968u, 3, 2},
    {0x80040900u, 0x800909a8u, 0x80040904u, 3, 3},
    {0x80040964u, 0x800909a8u, 0x80040968u, 3, 3},
    {0x80040900u, 0x800909a8u, 0x80040904u, 3, 4},
    {0x80040964u, 0x800909a8u, 0x80040968u, 3, 4},
    {0x80040900u, 0x800909a8u, 0x80040904u, 3, 5},
    {0x80040964u, 0x800909a8u, 0x80040968u, 3, 5},
    {0x80040900u, 0x800909a8u, 0x80040904u, 3, 6},
    {0x80040964u, 0x800909a8u, 0x80040968u, 3, 6},
    {0x80040900u, 0x800909a8u, 0x80040904u, 3, 7},
    {0x80040964u, 0x800909a8u, 0x80040968u, 3, 7},
    {0x80040900u, 0x800909a8u, 0x80040904u, 3, 8},
    {0x80040964u, 0x800909a8u, 0x80040968u, 3, 8},
    {0x80040900u, 0x800909a8u, 0x80040904u, 3, 9},
    {0x80040964u, 0x800909a8u, 0x80040968u, 3, 9},
    {0x80040900u, 0x800909a8u, 0x80040904u, 3, 10},
    {0x80040964u, 0x800909a8u, 0x80040968u, 3, 10},
    {0x80040900u, 0x800909a8u, 0x80040904u, 3, 11},
    {0x80040964u, 0x800909a8u, 0x80040968u, 3, 11},
    {0x80040900u, 0x800909a8u, 0x80040904u, 3, 12},
    {0x80040964u, 0x800909a8u, 0x80040968u, 3, 12},
    {0x800409d8u, 0x80029dd0u, 0x800409dcu, 0, 1},
    {0x800409e0u, 0x8002fc30u, 0x800409e4u, 0, 1},
}};

struct Capture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendMainMachine machine{};
  Nba97FrontendMainBinding binding{};
  Nba97FrontendMainCallerEvent parent{
      0x8007b838u, 0x8007b83cu, 0x80028800u, 1, 1, 0,
      NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
  std::array<Nba97FrontendMainAccess, 256> main_journal{};
  std::array<U, 1024> instruction_journal{};
  std::array<Nba97FrontendDispatchEntryAccess, 8> wrapper_journal{};
  std::array<Nba97FrontendDispatchAccess, 4096> dispatch_journal{};
  std::vector<MainCall> main_calls;
  std::vector<Nba97FrontendDispatchEvent> dispatch_calls;
  UserSetupSession session;
  bool setup_accepted = false;
  bool contract_failure = false;
  U copy_count = 0;
  U source_checksum = 0;
  U destination_checksum = 0;

  Capture() {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x33000000u + i * 0x101u, 15};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[NBA97_FRONTEND_MAIN_SP] = {Sp, 15};
    machine.registers.gpr[NBA97_FRONTEND_MAIN_RA] = {0x8007b840u, 15};
    machine.hi = {0x10203040u, 5};
    machine.lo = {0x50607080u, 10};
    put(0x80021ee4u, 1);
    put(0x8001edecu, 0, 2);
    put(0x80021568u, 0, 2);
    put(0x800170c0u, 0);
    put(Context + 0x14u, 0);
    put(0x8009821cu, Allocation);
    put(0x800982e0u, Allocation);
    put(0x8009352cu, Allocation);
    put(0x80015098u, 0);
    put(0x80021d74u, 0);
    put(0x80021d78u, 1);
    put(Context + 0x720u, 0, 2);
    put(0x800ef754u, Allocation);
    for (unsigned i = 0; i < DispatchTargets.size(); ++i)
      put(0x80024f80u + i * 4u, DispatchTargets[i]);
    for (unsigned team = 0; team < 2; ++team)
      for (unsigned slot = 0; slot < 12; ++slot) {
        const U source = Roster + (team + 1u) * 0x600u + slot * 0x80u;
        put(0x80023ab0u + team * 104u + slot * 4u, source);
        for (unsigned byte = 0; byte < 110; ++byte)
          put(source + byte, team * 31u + slot * 7u + byte, 1);
      }
    for (U i = 0; i < LoadSize; ++i)
      put(Handle + i, i * 37u + 11u, 1);
    put(Handle, GameEntry);
    source_checksum = checksum(Handle, LoadSize);
    binding.operation_budget = 1000;
    binding.io = mainIo;
    binding.user = this;
    binding.access_journal = main_journal.data();
    binding.access_journal_capacity = main_journal.size();
    binding.instruction_journal = instruction_journal.data();
    binding.instruction_journal_capacity = instruction_journal.size();
    binding.wrapper.operation_budget = 5;
    binding.wrapper.access_journal = wrapper_journal.data();
    binding.wrapper.access_journal_capacity = wrapper_journal.size();
    binding.wrapper.dispatcher.operation_budget = 20000;
    binding.wrapper.dispatcher.io = dispatchIo;
    binding.wrapper.dispatcher.user = this;
    binding.wrapper.dispatcher.access_journal = dispatch_journal.data();
    binding.wrapper.dispatcher.access_journal_capacity = dispatch_journal.size();
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
    U value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }
  U checksum(U address, U count) {
    if (!extent(address, count)) {
      contract_failure = true;
      return 0;
    }
    U value = 2166136261u;
    for (U i = 0; i < count; ++i) {
      value ^= bytes[address - Base + i];
      value *= 16777619u;
    }
    return value;
  }

  static int mainIo(void *opaque, const Nba97GameTextMemory *,
                    const Nba97FrontendMainEvent *event,
                    Nba97FrontendMainMachine *machine,
                    Nba97FrontendMainCalleeOutcome *outcome) {
    auto &c = *static_cast<Capture *>(opaque);
    Nba97FrontendMainSiteContract contract{};
    if (!event || !machine || !outcome ||
        !nba97_frontend_main_site_contract(event->site, &contract) ||
        event->pc != contract.pc || event->delay_slot_pc != contract.delay_slot_pc ||
        event->argument_count != contract.argument_count ||
        event->target_program != contract.target_program ||
        (!contract.dynamic_target && event->entry != contract.target) ||
        (contract.dynamic_target && event->entry != GameEntry) ||
        machine->registers.gpr[NBA97_FRONTEND_MAIN_RA].known_mask != 15 ||
        machine->registers.gpr[NBA97_FRONTEND_MAIN_RA].word != event->pc + 8u) {
      c.contract_failure = true;
      return 0;
    }
    c.main_calls.push_back({*event, *machine});
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028810)
      c.put(0x80015098u, 1);
    else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028880 ||
        event->site == NBA97_FRONTEND_MAIN_SITE_80028974)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {Allocation, 15};
    else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028ACC)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {Handle, 15};
    else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028AD8)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {LoadSize, 15};
    else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028B54) {
      if (machine->registers.gpr[NBA97_FRONTEND_MAIN_A0].word != Handle ||
          machine->registers.gpr[NBA97_FRONTEND_MAIN_A1].word != 0x801e0000u ||
          machine->registers.gpr[NBA97_FRONTEND_MAIN_A2].word != LoadSize) {
        c.contract_failure = true;
        return 0;
      }
      if (!c.extent(Handle, LoadSize) || !c.extent(0x801e0000u, LoadSize)) {
        c.contract_failure = true;
        return 0;
      }
      for (U i = 0; i < LoadSize; ++i) {
        c.bytes[0x801e0000u - Base + i] = c.bytes[Handle - Base + i];
        c.known[0x801e0000u - Base + i] = c.known[Handle - Base + i];
      }
      c.copy_count = LoadSize;
      c.destination_checksum = c.checksum(0x801e0000u, LoadSize);
    } else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028B68) {
      *outcome = NBA97_FRONTEND_MAIN_CALLEE_RETURNED;
    }
    return 1;
  }

  static int dispatchIo(void *opaque, const Nba97GameTextMemory *,
                        const Nba97FrontendDispatchEvent *event,
                        Nba97FrontendDispatchMachine *machine) {
    auto &c = *static_cast<Capture *>(opaque);
    Nba97FrontendDispatchSiteContract contract{};
    const std::size_t call_index = c.dispatch_calls.size();
    if (!event || !machine ||
        call_index >= ExpectedDispatchCalls.size() ||
        !nba97_frontend_dispatch_site_contract(event->site, &contract) ||
        event->pc != contract.pc || event->delay_slot_pc != contract.delay_slot_pc ||
        event->target != contract.target ||
        event->argument_count != contract.argument_count ||
        event->pc != ExpectedDispatchCalls[call_index].pc ||
        event->target != ExpectedDispatchCalls[call_index].target ||
        event->delay_slot_pc != ExpectedDispatchCalls[call_index].delay ||
        event->argument_count != ExpectedDispatchCalls[call_index].arguments ||
        event->invocation != ExpectedDispatchCalls[call_index].invocation ||
        machine->registers.gpr[31].known_mask != 15 ||
        machine->registers.gpr[31].word != event->pc + 8u) {
      c.contract_failure = true;
      return 0;
    }
    c.dispatch_calls.push_back(*event);
    if (event->target == 0x800770d4u)
      machine->registers.gpr[2] = {Allocation, 15};
    else if (event->target == 0x800459c8u)
      machine->registers.gpr[2] = {5, 15};
    else if (event->target == 0x80031a88u)
      c.put(Context + 0x720u, 5, 2);
    else if (event->target == 0x80037010u) {
      std::array<std::uint8_t, 8> assignments{{1, 2, 0, 0, 0, 0, 0, 0}};
      c.session.open(assignments, {}, 0);
      c.session.setControllers(0, 1);
      c.session.key(0, 0x80, true);
      const auto actions = c.session.step(100);
      c.setup_accepted =
          !actions.empty() && actions.back().event == NBA97_USER_CONFIRMED &&
          c.session.state().result == 6;
      machine->registers.gpr[2] = {c.setup_accepted ? 6u : 0u, 15};
    } else if (event->target == 0x800909a8u) {
      const U source = machine->registers.gpr[4].word;
      const U target = machine->registers.gpr[5].word;
      const U count = machine->registers.gpr[6].word;
      if (count != 0x6e || source < Base || target < Base ||
          source - Base > c.bytes.size() - count ||
          target - Base > c.bytes.size() - count) {
        c.contract_failure = true;
        return 0;
      }
      for (U i = 0; i < count; ++i) {
        c.bytes[target - Base + i] = c.bytes[source - Base + i];
        c.known[target - Base + i] = c.known[source - Base + i];
      }
    } else
      machine->registers.gpr[2] = {UINT32_MAX, 15};
    return 1;
  }
};

std::string hex(U value) {
  std::ostringstream out;
  out << "\"0x" << std::hex << std::setw(8) << std::setfill('0') << value
      << '\"';
  return out.str();
}
void word(std::ostringstream &out, const Nba97FrontendMainWord &value) {
  out << "{\"word\":" << hex(value.word)
      << ",\"known_mask\":" << unsigned(value.known_mask) << '}';
}
} // namespace

std::string captureFrontendMain() {
  try {
    Capture c;
    const U before_flag = c.get(0x80021ee4u);
    const U before_busy = c.get(0x800d9b40u);
    const U before_entry = c.get(0x801e0000u);
    Nba97GameTextMemory memory{&c.region, 1};
    const int accepted = nba97_frontend_main_from_overlay_entry(
        &c.binding, &memory, &c.parent, &c.machine);
    auto &main = c.binding.progress;
    auto &adapter = c.binding.adapter;
    auto &wrapper = adapter.wrapper_progress;
    auto &dispatcher = adapter.wrapper_adapter.dispatcher_progress;
    if (c.dispatch_calls.size() != ExpectedDispatchCalls.size() ||
        c.main_calls.size() + 1 != main.callbacks_completed ||
        main.operations != 98 || main.accesses != 33 || main.reads != 9 ||
        main.stores != 24 || main.callbacks_completed != 65 ||
        main.instruction_count != 299 || main.wait_iterations != 20 ||
        main.instruction_events > c.instruction_journal.size() ||
        main.access_events > c.main_journal.size() ||
        c.copy_count != LoadSize || c.source_checksum == 0 ||
        c.destination_checksum != c.source_checksum)
      c.contract_failure = true;

    std::ostringstream out;
    out << "{\"program\":\"FEONLY\",\"address\":" << hex(0x80028800u)
        << ",\"inclusive_end\":" << hex(0x80028b8bu)
        << ",\"bytes\":908,\"instructions\":227,"
        << "\"source_sha256\":\"a9325ac1de6cf8da7bd5a43d95da2f2e61bfc586cd3f265824d3e519cb42b208\","
        << "\"completed\":" << unsigned(main.completed)
        << ",\"accepted\":" << accepted << ",\"result\":"
        << c.binding.result << ",\"contract_failure\":" << c.contract_failure
        << ",\"classification\":\"UI/menu\",\"gameplay_shown\":\"BLOCKED\","
        << "\"fixture_contract\":\"Synthetic full machine at unowned FEONLY overlay-entry JAL 0x8007B838. The synthetic 0x8007B844 startup service publishes dispatcher launch flag 1 at 0x80015098 and otherwise preserves all 32 GPR words and masks plus HI/LO. The two 0x8008BFB0 calls return V0=0x80140000; loader 0x8007B11C returns V0=0x80170000; size 0x80077CD4 returns V0=4096; those callbacks otherwise preserve the full machine. Main copy 0x800909A8 copies exactly 4096 generated bytes and preserves the full machine. All other main FEONLY services preserve the full machine. The GAMELOAD callback at the copied dynamic entry returns and preserves the full machine, so no gameplay executes. Dispatcher uses its exact 42-call whitelist: 0x800770D4 returns V0=0x80140000, 0x800459C8 returns V0=5, 0x80031A88 publishes context state 5 while preserving V0, UserSetupSession supplies native input acceptance V0=6, 0x800909A8 copies exactly 110 bytes with knownness, and all other dispatcher sites return V0=0xFFFFFFFF; every dispatcher callback preserves all other GPR words and masks plus HI/LO. Only main-to-wrapper-to-dispatcher is natural recovered-owner composition.\","
        << "\"parent_call\":{\"pc\":" << hex(c.parent.pc)
        << ",\"delay\":" << hex(c.parent.delay_slot_pc)
        << ",\"entry\":" << hex(c.parent.entry)
        << ",\"ra\":" << hex(0x8007b840u) << "},"
        << "\"branch_state\":{\"initial_frontend_flag\":";
    word(out, main.loaded_initial_frontend_flag);
    out << ",\"menu_frontend_flag\":";
    word(out, main.loaded_menu_frontend_flag);
    out << ",\"intro_flag\":";
    word(out, main.loaded_intro_flag);
    out << ",\"context_selector\":";
    word(out, main.loaded_context_selector);
    out << ",\"intro_iterations\":" << main.intro_iterations
        << ",\"wait_iterations\":" << main.wait_iterations << "},"
        << "\"loader_state\":{\"handle\":";
    word(out, main.gameload_handle);
    out << ",\"size\":";
    word(out, main.gameload_size);
    out << ",\"copy_source\":" << hex(Handle)
        << ",\"copy_destination\":" << hex(0x801e0000u)
        << ",\"copy_size\":" << c.copy_count
        << ",\"source_checksum\":" << hex(c.source_checksum)
        << ",\"destination_checksum\":" << hex(c.destination_checksum)
        << ",\"dynamic_entry\":";
    word(out, main.dynamic_entry);
    out << ",\"dynamic_program\":\"GAMELOAD\",\"dynamic_outcome\":\"RETURNED\"},"
        << "\"memory\":{\"before\":{\"frontend_flag\":" << before_flag
        << ",\"frontend_busy\":" << before_busy
        << ",\"gameload_entry\":" << before_entry
        << "},\"after\":{\"frontend_flag\":" << c.get(0x80021ee4u)
        << ",\"frontend_busy\":" << c.get(0x800d9b40u)
        << ",\"frontend_scalar\":" << c.get(0x800c6e68u)
        << ",\"gameload_entry\":" << c.get(0x801e0000u) << "}},"
        << "\"main\":{\"operations\":" << main.operations
        << ",\"accesses\":" << main.accesses << ",\"reads\":"
        << main.reads << ",\"stores\":" << main.stores
        << ",\"callbacks\":" << main.callbacks_completed
        << ",\"instruction_count\":" << main.instruction_count
        << ",\"intro_iterations\":" << main.intro_iterations
        << ",\"wait_iterations\":" << main.wait_iterations
        << ",\"instruction_trace\":[";
    const std::size_t instruction_events =
        std::min(main.instruction_events, c.instruction_journal.size());
    for (std::size_t i = 0; i < instruction_events; ++i) {
      if (i) out << ',';
      out << hex(c.instruction_journal[i]);
    }
    out << "],\"access_journal\":[";
    const std::size_t access_events =
        std::min(main.access_events, c.main_journal.size());
    for (std::size_t i = 0; i < access_events; ++i) {
      const auto &event = c.main_journal[i];
      if (i) out << ',';
      out << "{\"pc\":" << hex(event.pc) << ",\"address\":"
          << hex(event.address) << ",\"value\":" << hex(event.value)
          << ",\"operation\":" << event.operation
          << ",\"width\":" << unsigned(event.width)
          << ",\"known_mask\":" << unsigned(event.known_mask)
          << ",\"kind\":" << unsigned(event.kind) << '}';
    }
    out << "],\"call_sequence\":[";
    bool first_call = true;
    const auto emit_call = [&](const Nba97FrontendMainEvent &event,
                               const Nba97FrontendMainMachine &machine) {
      if (!first_call) out << ',';
      first_call = false;
      out << "{\"pc\":" << hex(event.pc) << ",\"delay\":"
          << hex(event.delay_slot_pc) << ",\"target\":"
          << hex(event.entry) << ",\"program\":"
          << unsigned(event.target_program) << ",\"argument_count\":"
          << unsigned(event.argument_count) << ",\"invocation\":"
          << event.invocation << ",\"a0\":";
      word(out, machine.registers.gpr[NBA97_FRONTEND_MAIN_A0]);
      out << ",\"a1\":";
      word(out, machine.registers.gpr[NBA97_FRONTEND_MAIN_A1]);
      out << ",\"a2\":";
      word(out, machine.registers.gpr[NBA97_FRONTEND_MAIN_A2]);
      out << ",\"sp\":";
      word(out, machine.registers.gpr[NBA97_FRONTEND_MAIN_SP]);
      out << ",\"ra\":";
      word(out, machine.registers.gpr[NBA97_FRONTEND_MAIN_RA]);
      out << '}';
    };
    bool wrapper_emitted = false;
    for (const auto &call : c.main_calls) {
      if (!wrapper_emitted && call.event.pc > adapter.wrapper_event.pc) {
        emit_call(adapter.wrapper_event, adapter.wrapper_machine);
        wrapper_emitted = true;
      }
      emit_call(call.event, call.machine);
    }
    if (!wrapper_emitted)
      emit_call(adapter.wrapper_event, adapter.wrapper_machine);
    out << "]},\"wrapper\":{\"result\":" << adapter.wrapper_result
        << ",\"completed\":" << unsigned(wrapper.completed)
        << ",\"operations\":" << wrapper.operations
        << ",\"accesses\":" << wrapper.accesses
        << ",\"reads\":" << wrapper.reads << ",\"stores\":"
        << wrapper.stores << ",\"callbacks\":"
        << wrapper.callbacks_completed << ",\"instruction_count\":"
        << wrapper.instruction_count << "},\"dispatcher\":{\"result\":"
        << adapter.wrapper_adapter.dispatcher_result
        << ",\"completed\":" << unsigned(dispatcher.completed)
        << ",\"operations\":" << dispatcher.operations
        << ",\"reads\":" << dispatcher.reads << ",\"stores\":"
        << dispatcher.stores << ",\"callbacks\":"
        << dispatcher.callbacks_completed << ",\"instruction_count\":"
        << dispatcher.instruction_count << ",\"call_sequence\":[";
    for (std::size_t i = 0; i < c.dispatch_calls.size(); ++i) {
      const auto &event = c.dispatch_calls[i];
      if (i) out << ',';
      out << "{\"pc\":" << hex(event.pc) << ",\"target\":"
          << hex(event.target) << ",\"delay\":" << hex(event.delay_slot_pc)
          << ",\"argument_count\":" << unsigned(event.argument_count)
          << ",\"invocation\":" << event.invocation << '}';
    }
    out << "]},\"user_setup\":{\"accepted\":" << c.setup_accepted
        << ",\"result\":" << unsigned(c.session.state().result)
        << "},\"final_machine\":{\"gpr\":[";
    for (unsigned i = 0; i < 32; ++i) {
      if (i) out << ',';
      word(out, main.machine.registers.gpr[i]);
    }
    out << "],\"hi\":";
    word(out, main.machine.hi);
    out << ",\"lo\":";
    word(out, main.machine.lo);
    out << "},\"next_unbound_boundary\":{"
           "\"earliest_production\":\"unowned FEONLY overlay-entry JAL 0x8007B838 and first startup service 0x80028810 -> 0x8007B844\","
           "\"post_acceptance\":\"cleanup 0x80028AA8 -> 0x8002F084 and 0x80028AB0 -> 0x80028E08, loader 0x80028ACC -> 0x8007B11C, copy 0x80028B54, and GAMELOAD transfer 0x80028B68\"}}";
    return out.str();
  } catch (...) {
    return "{\"program\":\"FEONLY\",\"address\":\"0x80028800\","
           "\"bytes\":908,\"completed\":0,\"accepted\":0,\"result\":0,"
           "\"contract_failure\":true,\"classification\":\"UI/menu\","
           "\"gameplay_shown\":\"BLOCKED\"}";
  }
}
} // namespace nba97
