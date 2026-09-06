#include "frontend_dispatch_entry_capture.h"

#include "frontend_dispatch_entry_adapter.h"
#include "user_setup_session.hpp"

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
constexpr U Context = 0x80110000u;
constexpr U Allocation = 0x80140000u;
constexpr U Roster = 0x80160000u;
constexpr U Sp = 0x801f0000u;
constexpr std::array<U, 43> Targets{
    {0x8003fa3c, 0x8003fb6c, 0x8003fc34, 0x8003fc78, 0x8003fca8,
     0x8003fcf4, 0x8003fd74, 0x8003fe58, 0x8003fe98, 0x8003ff10,
     0x8004005c, 0x8004006c, 0x800400f0, 0x80040120, 0x80040154,
     0x80040184, 0x80040194, 0x800401c0, 0x800401fc, 0x8004028c,
     0x800402d8, 0x800402e8, 0x80040350, 0x80040360, 0x80040370,
     0x80040380, 0x80040390, 0x80040410, 0x80040474, 0x800405d8,
     0x80040558, 0x80040658, 0x800406bc, 0x800406fc, 0x8004070c,
     0x8004071c, 0x8004072c, 0x8004073c, 0x8004076c, 0x8004077c,
     0x8004009c, 0x800400ac, 0x800407d4}};

struct ExpectedCall {
  U pc;
  U target;
  U delay;
  std::uint8_t arguments;
  std::size_t invocation;
};

constexpr std::array<ExpectedCall, 42> ExpectedCalls{{
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
  Nba97FrontendDispatchEntryMachine machine{};
  Nba97FrontendDispatchEntryBinding binding{};
  Nba97FrontendDispatchEntryCallerEvent parent{
      0x80028aa0u, 0x80028aa4u, 0x800360d4u, 1, 1, 0};
  std::array<Nba97FrontendDispatchEntryAccess, 8> wrapper_journal{};
  std::array<Nba97FrontendDispatchAccess, 4096> dispatch_journal{};
  std::vector<Nba97FrontendDispatchEvent> calls;
  UserSetupSession session;
  bool accepted = false;
  bool contract_failure = false;

  Capture() {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x33000000u + i * 0x101u, 15};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_S0] = {0, 15};
    machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_SP] = {Sp, 15};
    machine.registers.gpr[NBA97_FRONTEND_DISPATCH_ENTRY_RA] = {0x80028aa8u,
                                                                15};
    machine.hi = {0x10203040u, 5};
    machine.lo = {0x50607080u, 10};
    put(0x800170c0u, Context);
    put(Context + 0x14u, 0x80120000u);
    put(0x8009821cu, Allocation);
    put(0x800982e0u, Allocation);
    put(0x8009352cu, Allocation);
    put(0x80015098u, 1);
    put(0x80021d70u, 0, 1);
    put(0x80021d74u, 0);
    put(0x80021d78u, 1);
    put(0x8001edecu, 0, 2);
    put(Context + 0x720u, 5, 2);
    put(0x800ef754u, Allocation);
    for (unsigned i = 0; i < Targets.size(); ++i)
      put(0x80024f80u + i * 4u, Targets[i]);
    for (unsigned team = 0; team < 2; ++team)
      for (unsigned slot = 0; slot < 12; ++slot) {
        const U source = Roster + (team + 1u) * 0x600u + slot * 0x80u;
        put(0x80023ab0u + team * 104u + slot * 4u, source);
        for (unsigned byte = 0; byte < 110; ++byte)
          put(source + byte, team * 31u + slot * 7u + byte, 1);
      }
    binding.operation_budget = 5;
    binding.access_journal = wrapper_journal.data();
    binding.access_journal_capacity = wrapper_journal.size();
    binding.dispatcher.operation_budget = 20000;
    binding.dispatcher.io = callback;
    binding.dispatcher.user = this;
    binding.dispatcher.access_journal = dispatch_journal.data();
    binding.dispatcher.access_journal_capacity = dispatch_journal.size();
  }

  void put(U address, U value, unsigned width = 4) {
    for (unsigned byte = 0; byte < width; ++byte)
      bytes[address - Base + byte] = std::uint8_t(value >> (byte * 8u));
  }
  U get(U address, unsigned width = 4) const {
    U value = 0;
    for (unsigned byte = 0; byte < width; ++byte)
      value |= U(bytes[address - Base + byte]) << (byte * 8u);
    return value;
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendDispatchEvent *event,
                      Nba97FrontendDispatchMachine *machine) {
    auto &capture = *static_cast<Capture *>(opaque);
    Nba97FrontendDispatchSiteContract contract{};
    const std::size_t call_index = capture.calls.size();
    if (!event || !machine ||
        call_index >= ExpectedCalls.size() ||
        !nba97_frontend_dispatch_site_contract(event->site, &contract) ||
        event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->target != contract.target ||
        event->argument_count != contract.argument_count ||
        event->pc != ExpectedCalls[call_index].pc ||
        event->target != ExpectedCalls[call_index].target ||
        event->delay_slot_pc != ExpectedCalls[call_index].delay ||
        event->argument_count != ExpectedCalls[call_index].arguments ||
        event->invocation != ExpectedCalls[call_index].invocation ||
        machine->registers.gpr[31].known_mask != 15 ||
        machine->registers.gpr[31].word != event->pc + 8u) {
      capture.contract_failure = true;
      return 0;
    }
    capture.calls.push_back(*event);
    if (event->target == 0x800770d4u)
      machine->registers.gpr[2] = {Allocation, 15};
    else if (event->target == 0x800459c8u)
      machine->registers.gpr[2] = {5, 15};
    else if (event->target == 0x80031a88u)
      capture.put(Context + 0x720u, 5, 2);
    else if (event->target == 0x80037010u) {
      std::array<std::uint8_t, 8> assignments{{1, 2, 0, 0, 0, 0, 0, 0}};
      capture.session.open(assignments, {}, 0);
      capture.session.setControllers(0, 1);
      capture.session.key(0, 0x80, true);
      const auto actions = capture.session.step(100);
      capture.accepted =
          !actions.empty() && actions.back().event == NBA97_USER_CONFIRMED &&
          capture.session.state().result == 6;
      machine->registers.gpr[2] = {capture.accepted ? 6u : 0u, 15};
    } else if (event->target == 0x800909a8u) {
      const U source = machine->registers.gpr[4].word;
      const U target = machine->registers.gpr[5].word;
      const U count = machine->registers.gpr[6].word;
      if (count != 0x6e || source < Base || target < Base ||
          source - Base > capture.bytes.size() - count ||
          target - Base > capture.bytes.size() - count) {
        capture.contract_failure = true;
        return 0;
      }
      for (U i = 0; i < count; ++i) {
        capture.bytes[target - Base + i] = capture.bytes[source - Base + i];
        capture.known[target - Base + i] = capture.known[source - Base + i];
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

void wordJson(std::ostringstream &out,
              const Nba97FrontendDispatchEntryWord &word) {
  out << "{\"word\":" << hex(word.word)
      << ",\"known_mask\":" << unsigned(word.known_mask) << '}';
}
} // namespace

std::string captureFrontendDispatchEntry() {
  try {
    Capture capture;
    const U before_flag = capture.get(0x80021ee4u);
    const U before_scalar = capture.get(0x800c6e68u);
    const U before_saved_ra = capture.get(Sp - 8u);
    Nba97GameTextMemory memory{&capture.region, 1};
    const int parent_accepted = nba97_frontend_dispatch_entry_from_frontend_main(
        &capture.binding, &memory, &capture.parent, &capture.machine);
    auto &wrapper = capture.binding.progress;
    auto &adapter = capture.binding.adapter;
    auto &dispatcher = adapter.dispatcher_progress;
    if (capture.calls.size() != 42)
      capture.contract_failure = true;

    std::ostringstream out;
    out << '{' << "\"program\":\"FEONLY\"," << "\"address\":"
        << hex(0x800360d4u) << ",\"inclusive_end\":" << hex(0x8003610bu)
        << ",\"bytes\":56,\"instructions\":14,"
        << "\"source_sha256\":"
           "\"6af71d91fded3e2b5260c84bb86fd101539e86fca86ffef2b9e06b93e32dbce0\","
        << "\"completed\":" << unsigned(wrapper.completed)
        << ",\"accepted\":" << parent_accepted
        << ",\"result\":" << capture.binding.result
        << ",\"contract_failure\":" << capture.contract_failure << ','
        << "\"classification\":\"UI/menu\","
        << "\"gameplay_shown\":\"BLOCKED\","
        << "\"fixture_contract\":\"Synthetic full machine at the unowned frontend-main parent. Exact 42-call whitelist: each child preserves all machine words except documented V0; 770D4 returns allocation80140000, 459C8 returns state5, 31A88 publishes state5 while preserving V0, UserSetupSession supplies native input acceptance6, 909A8 copies exactly110 bytes preserving machine, all other listed sites return V0=-1. Only wrapper-to-dispatcher is natural recovered-owner composition.\","
        << "\"parent_call\":{\"pc\":" << hex(capture.parent.pc)
        << ",\"delay\":" << hex(capture.parent.delay_slot_pc)
        << ",\"entry\":" << hex(capture.parent.entry)
        << ",\"ra\":" << hex(0x80028aa8u)
        << ",\"s0\":" << hex(0) << "},"
        << "\"globals\":{\"before\":{\"initialized\":" << before_flag
        << ",\"scalar\":" << before_scalar
        << ",\"saved_ra_slot\":" << before_saved_ra
        << "},\"after\":{\"initialized\":" << capture.get(0x80021ee4u)
        << ",\"scalar\":" << capture.get(0x800c6e68u)
        << ",\"saved_ra_slot\":" << capture.get(Sp - 8u) << "}},"
        << "\"wrapper\":{\"operations\":" << wrapper.operations
        << ",\"accesses\":" << wrapper.accesses
        << ",\"reads\":" << wrapper.reads << ",\"stores\":"
        << wrapper.stores << ",\"callbacks\":"
        << wrapper.callbacks_completed << ",\"instruction_count\":"
        << wrapper.instruction_count << ",\"frame_sp\":"
        << hex(wrapper.frame_stack_pointer) << ",\"saved_ra\":";
    wordJson(out, wrapper.saved_return_address);
    out << ",\"restored_ra\":";
    wordJson(out, wrapper.restored_return_address);
    out << ",\"access_journal\":[";
    for (std::size_t i = 0; i < wrapper.access_events; ++i) {
      const auto &event = capture.wrapper_journal[i];
      if (i)
        out << ',';
      out << "{\"pc\":" << hex(event.pc) << ",\"address\":"
          << hex(event.address) << ",\"value\":" << hex(event.value)
          << ",\"operation\":" << event.operation
          << ",\"width\":" << unsigned(event.width)
          << ",\"known_mask\":" << unsigned(event.known_mask)
          << ",\"kind\":" << unsigned(event.kind) << '}';
    }
    out << "]},\"child_call\":{\"pc\":"
        << hex(adapter.dispatcher_event.pc) << ",\"delay\":"
        << hex(adapter.dispatcher_event.delay_slot_pc) << ",\"entry\":"
        << hex(adapter.dispatcher_event.entry) << ",\"ra\":"
        << hex(0x800360fcu) << ",\"operation\":"
        << adapter.dispatcher_event.operation << "},"
        << "\"dispatcher\":{\"result\":" << adapter.dispatcher_result
        << ",\"completed\":" << unsigned(dispatcher.completed)
        << ",\"operations\":" << dispatcher.operations
        << ",\"reads\":" << dispatcher.reads << ",\"stores\":"
        << dispatcher.stores << ",\"callbacks\":"
        << dispatcher.callbacks_completed << ",\"instruction_count\":"
        << dispatcher.instruction_count << ",\"call_sequence\":[";
    for (std::size_t i = 0; i < capture.calls.size(); ++i) {
      const auto &event = capture.calls[i];
      if (i)
        out << ',';
      out << "{\"pc\":" << hex(event.pc) << ",\"target\":"
          << hex(event.target) << ",\"delay\":" << hex(event.delay_slot_pc)
          << ",\"argument_count\":" << unsigned(event.argument_count)
          << ",\"invocation\":" << event.invocation << '}';
    }
    out << "]},"
        << "\"user_setup\":{\"accepted\":" << capture.accepted
        << ",\"result\":" << int(capture.session.state().result) << "},"
        << "\"final_machine\":{\"gpr\":[";
    for (unsigned i = 0; i < 32; ++i) {
      if (i)
        out << ',';
      wordJson(out, wrapper.machine.registers.gpr[i]);
    }
    out << "],\"hi\":";
    wordJson(out, wrapper.machine.hi);
    out << ",\"lo\":";
    wordJson(out, wrapper.machine.lo);
    out << "},\"next_unbound_boundary\":"
           "\"0x80028AA8..0x80028B68 FEONLY frontend-main return, gameload.bin services, copy to 0x801E0000, and dynamic GAMELOAD transfer\"}";
    return out.str();
  } catch (...) {
    return "{\"program\":\"FEONLY\",\"address\":\"0x800360d4\","
           "\"completed\":0,\"accepted\":0,\"result\":0,"
           "\"contract_failure\":true,\"classification\":\"UI/menu\","
           "\"gameplay_shown\":\"BLOCKED\"}";
  }
}
} // namespace nba97
