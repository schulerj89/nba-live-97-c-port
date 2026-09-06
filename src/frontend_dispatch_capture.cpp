#include "frontend_dispatch_capture.h"

#include "frontend_dispatch_adapter.h"
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
    {0x8003fa3c, 0x8003fb6c, 0x8003fc34, 0x8003fc78, 0x8003fca8, 0x8003fcf4,
     0x8003fd74, 0x8003fe58, 0x8003fe98, 0x8003ff10, 0x8004005c, 0x8004006c,
     0x800400f0, 0x80040120, 0x80040154, 0x80040184, 0x80040194, 0x800401c0,
     0x800401fc, 0x8004028c, 0x800402d8, 0x800402e8, 0x80040350, 0x80040360,
     0x80040370, 0x80040380, 0x80040390, 0x80040410, 0x80040474, 0x800405d8,
     0x80040558, 0x80040658, 0x800406bc, 0x800406fc, 0x8004070c, 0x8004071c,
     0x8004072c, 0x8004073c, 0x8004076c, 0x8004077c, 0x8004009c, 0x800400ac,
     0x800407d4}};

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
  Nba97FrontendDispatchMachine machine{};
  Nba97FrontendDispatchBinding binding{};
  Nba97FrontendDispatchCallerEvent entry{0x800360f4u, 0x800360f8u, 0x8003f7c8u,
                                         1,           1,           0};
  std::array<Nba97FrontendDispatchAccess, 4096> journal{};
  std::vector<Nba97FrontendDispatchEvent> calls;
  UserSetupSession session;
  bool accepted = false;
  bool contractFailure = false;

  Capture() {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x33000000u + i * 0x101u, 15};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[29] = {Sp, 15};
    machine.registers.gpr[31] = {0x800360fcu, 15};
    machine.hi = {0x10203040u, 15};
    machine.lo = {0x50607080u, 15};
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
    binding.operation_budget = 20000;
    binding.io = callback;
    binding.user = this;
    binding.access_journal = journal.data();
    binding.access_journal_capacity = journal.size();
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

  U checksum(U address, std::size_t size) const {
    U value = 2166136261u;
    for (std::size_t i = 0; i < size; ++i) {
      value ^= bytes[address - Base + i];
      value *= 16777619u;
    }
    return value;
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendDispatchEvent *event,
                      Nba97FrontendDispatchMachine *machine) {
    auto &capture = *static_cast<Capture *>(opaque);
    Nba97FrontendDispatchSiteContract contract{};
    const std::size_t callIndex = capture.calls.size();
    if (!event || !machine || callIndex >= ExpectedCalls.size() ||
        !nba97_frontend_dispatch_site_contract(event->site, &contract) ||
        event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->target != contract.target ||
        event->argument_count != contract.argument_count ||
        machine->registers.gpr[31].known_mask != 15 ||
        machine->registers.gpr[31].word != event->pc + 8u ||
        event->pc != ExpectedCalls[callIndex].pc ||
        event->target != ExpectedCalls[callIndex].target ||
        event->delay_slot_pc != ExpectedCalls[callIndex].delay ||
        event->argument_count != ExpectedCalls[callIndex].arguments ||
        event->invocation != ExpectedCalls[callIndex].invocation) {
      capture.contractFailure = true;
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
      capture.accepted = !actions.empty() &&
                         actions.back().event == NBA97_USER_CONFIRMED &&
                         capture.session.state().result == 6;
      machine->registers.gpr[2] = {capture.accepted ? 6u : 0u, 15};
    } else if (event->target == 0x800909a8u) {
      const U source = machine->registers.gpr[4].word;
      const U target = machine->registers.gpr[5].word;
      const U count = machine->registers.gpr[6].word;
      const U record = U(event->invocation - 1u);
      const U expectedSource = Roster +
                               (event->pc == 0x80040900u ? 0x600u : 0xc00u) +
                               record * 0x80u;
      const U expectedTarget =
          (event->pc == 0x80040900u ? 0x8002208cu : 0x800225b4u) +
          record * 0x6eu;
      if (count != 0x6e || source < Base || target < Base ||
          source != expectedSource || target != expectedTarget ||
          source - Base > capture.bytes.size() - count ||
          target - Base > capture.bytes.size() - count) {
        capture.contractFailure = true;
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
} // namespace

std::string captureFrontendDispatch() {
  try {
    Capture capture;
    const U beforeLaunch = capture.get(0x80015098u);
    const U beforeMode = capture.get(0x80021d70u, 1);
    const U beforeBuffer = capture.get(0x800ef754u);
    const U beforeAway = capture.checksum(Roster + 0xc00u, 110);
    const U beforeHome = capture.checksum(Roster + 0x600u, 110);
    Nba97GameTextMemory memory{&capture.region, 1};
    const int accepted = nba97_frontend_dispatch_from_800360d4(
        &capture.binding, &memory, &capture.entry, &capture.machine);
    if (capture.calls.size() != ExpectedCalls.size())
      capture.contractFailure = true;
    std::ostringstream out;
    out << '{' << "\"program\":\"FEONLY\","
        << "\"address\":" << hex(0x8003f7c8u) << ','
        << "\"end\":" << hex(0x80040a1bu) << ',' << "\"instructions\":1173,"
        << "\"source_sha256\":"
           "\"a42d7d2d97ab00ad7ddb214677b743dfd5d98d05119f9e6894fd092a6ccf1b9f"
           "\","
        << "\"entry_call\":{\"pc\":" << hex(capture.entry.pc)
        << ",\"delay\":" << hex(capture.entry.delay_slot_pc)
        << ",\"ra\":" << hex(0x800360fcu) << "},"
        << "\"completed\":" << unsigned(capture.binding.progress.completed)
        << ",\"accepted\":" << accepted
        << ",\"result\":" << capture.binding.result
        << ",\"operations\":" << capture.binding.progress.operations
        << ",\"reads\":" << capture.binding.progress.reads
        << ",\"stores\":" << capture.binding.progress.stores
        << ",\"callbacks\":" << capture.binding.progress.callbacks_completed
        << ",\"instruction_count\":"
        << capture.binding.progress.instruction_count << ','
        << "\"before\":{\"launch\":" << beforeLaunch
        << ",\"mode\":" << beforeMode
        << ",\"retained_buffer\":" << hex(beforeBuffer)
        << ",\"home_roster_checksum\":" << beforeHome
        << ",\"away_roster_checksum\":" << beforeAway << "},"
        << "\"after\":{\"launch\":" << capture.get(0x80015098u)
        << ",\"mode\":" << capture.get(0x80021d70u, 1)
        << ",\"retained_buffer\":" << hex(capture.get(0x800ef754u))
        << ",\"home_roster_checksum\":" << capture.checksum(0x8002208cu, 110)
        << ",\"away_roster_checksum\":" << capture.checksum(0x800225b4u, 110)
        << "},\"user_setup\":{\"accepted_result\":"
        << int(capture.session.state().result) << ",\"assignments\":[";
    for (unsigned i = 0; i < 8; ++i) {
      if (i)
        out << ',';
      out << unsigned(capture.session.state().assignment[i]);
    }
    out << "]},\"contract_failure\":" << capture.contractFailure
        << ",\"call_sequence\":[";
    for (std::size_t i = 0; i < capture.calls.size(); ++i) {
      const auto &call = capture.calls[i];
      if (i)
        out << ',';
      out << "{\"pc\":" << hex(call.pc) << ",\"target\":" << hex(call.target)
          << ",\"delay\":" << hex(call.delay_slot_pc)
          << ",\"argc\":" << unsigned(call.argument_count)
          << ",\"invocation\":" << call.invocation << '}';
    }
    out << "],\"fixture_contract\":\"Exact 42-call/20-site whitelist. Each "
           "synthetic child preserves the complete incoming machine except "
           "its declared V0 result; 3F7B0/30CDC/30308/3D2A4/61674/46D24/"
           "3E7A8/28B8C/804E8/5851C/29DD0/2FC30 return -1, 770D4 returns "
           "allocation 80140000, 459C8 returns state5, 31A88 publishes "
           "state5, native UserSetupSession returns6, and 909A8 copies "
           "exactly110 bytes while preserving the machine.\","
        << "\"next_unbound_boundary\":\"800360D4 wrapper -> 80028AA8 return -> "
           "FEONLY 80028ACC gameload.bin load -> 80028B54 copy to 801E0000 -> "
           "80028B68 JALR GAMELOAD; FELOAD and GAMELOAD overlays sharing "
           "addresses remain distinct\","
        << "\"visual_class\":\"UI/menu\",\"gameplay_shown\":false}";
    return out.str();
  } catch (...) {
    return "{\"program\":\"FEONLY\",\"address\":\"0x8003f7c8\",\"completed\":0,"
           "\"result\":0,\"error\":\"frontend dispatch capture failed\"}";
  }
}
} // namespace nba97
