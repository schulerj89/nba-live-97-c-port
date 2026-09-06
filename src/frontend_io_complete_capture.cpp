#include "frontend_io_complete_capture.h"

#include "frontend_io_complete_adapter.h"

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
constexpr U Active = 0x800f84c4u;
constexpr U Busy = 0x800f43b0u;
constexpr U Status = 0x800ef840u;
constexpr U Mode = 0x80145678u;
constexpr std::array<U, 7> Pcs{{0x800394e8u, 0x800394f0u, 0x80039500u,
                                0x80039530u, 0x80039538u, 0x80039554u,
                                0x8003955cu}};
constexpr std::array<U, 7> Delays{{0x800394ecu, 0x800394f4u, 0x80039504u,
                                   0x80039534u, 0x8003953cu, 0x80039558u,
                                   0x80039560u}};
constexpr std::array<U, 7> Targets{{0x800393f0u, 0x800392a0u, 0x80038e84u,
                                    0x80029b64u, 0x8008c274u, 0x8006cde4u,
                                    0x8006ae60u}};
constexpr std::array<unsigned, 7> Args{{0, 0, 0, 2, 0, 1, 0}};

struct Call {
  Nba97FrontendExitDrainEvent event{};
  Nba97FrontendExitDrainMachine machine{};
};

struct Capture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendExitDrainContext drain{};
  Nba97FrontendExitDrainProgress drain_progress{};
  Nba97FrontendIoCompleteBinding poll{};
  Nba97FrontendIoCompleteAdapterProgress adapter{};
  std::array<Nba97FrontendExitDrainAccess, 12> drain_access{};
  std::array<U, 64> drain_instructions{};
  std::array<Nba97FrontendIoCompleteAccess, 16> poll_access{};
  std::array<U, 128> poll_instructions{};
  std::array<Nba97FrontendIoCompleteParentRecord, 4> poll_records{};
  std::array<std::size_t, NBA97_FRONTEND_EXIT_DRAIN_SITE_COUNT> site_calls{};
  std::vector<Call> fallback_calls;
  bool contract_failure = false;

  Capture() {
    for (unsigned i = 0; i < 32; ++i)
      drain.machine.registers.gpr[i] = {0x73000000u + i * 0x101u, 15};
    drain.machine.registers.gpr[0] = {0, 15};
    drain.machine.registers.gpr[29] = {Sp, 15};
    drain.machine.registers.gpr[31] = {ParentRa, 15};
    drain.machine.hi = {0x10203040u, 5};
    drain.machine.lo = {0x50607080u, 10};
    put(Active, 1);
    put(Busy, 0xabcdef01u);
    put(0x8002149cu, Mode);
    for (unsigned i = 0; i < 8; ++i) put(Status + i * 0x24u, 0);
    put(Status, 0x80000000u);
    drain.memory = {&region, 1};
    drain.operation_budget = 15;
    drain.io = callback;
    drain.user = this;
    drain.access_journal = drain_access.data();
    drain.access_journal_capacity = drain_access.size();
    drain.instruction_journal = drain_instructions.data();
    drain.instruction_journal_capacity = drain_instructions.size();
    poll.operation_budget = 9;
    poll.access_journal = poll_access.data();
    poll.access_journal_capacity = poll_access.size();
    poll.instruction_journal = poll_instructions.data();
    poll.instruction_journal_capacity = poll_instructions.size();
    poll.parent_journal = poll_records.data();
    poll.parent_journal_capacity = poll_records.size();
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
                      const Nba97FrontendExitDrainEvent *event,
                      Nba97FrontendExitDrainMachine *machine) {
    auto &capture = *static_cast<Capture *>(opaque);
    if (!event || !machine || event->site == 0 || event->site >= 8 ||
        event->pc != Pcs[event->site - 1] ||
        event->delay_slot_pc != Delays[event->site - 1] ||
        event->entry != Targets[event->site - 1] ||
        event->argument_count != Args[event->site - 1] ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        event->invocation != ++capture.site_calls[event->site] ||
        machine->registers.gpr[31].word != event->pc + 8u ||
        machine->registers.gpr[31].known_mask != 15) {
      capture.contract_failure = true;
      return 0;
    }
    if (event->site == NBA97_FRONTEND_EXIT_DRAIN_SITE_80039530 &&
        (machine->registers.gpr[4].word != 0 ||
         machine->registers.gpr[4].known_mask != 15 ||
         machine->registers.gpr[5].word != 0 ||
         machine->registers.gpr[5].known_mask != 15)) {
      capture.contract_failure = true;
      return 0;
    }
    if (event->site == NBA97_FRONTEND_EXIT_DRAIN_SITE_80039554 &&
        (machine->registers.gpr[4].word != Mode ||
         machine->registers.gpr[4].known_mask != 15)) {
      capture.contract_failure = true;
      return 0;
    }
    capture.fallback_calls.push_back({*event, *machine});
    if (event->site == NBA97_FRONTEND_EXIT_DRAIN_SITE_80039500)
      capture.put(Status, 0);
    return 1;
  }
};

std::string hex(U value) {
  std::ostringstream out;
  out << "\"0x" << std::hex << std::setw(8) << std::setfill('0') << value
      << '\"';
  return out.str();
}
void word(std::ostringstream &out, const Nba97FrontendIoCompleteWord &value) {
  out << "{\"word\":" << hex(value.word)
      << ",\"known_mask\":" << unsigned(value.known_mask) << '}';
}
void machine(std::ostringstream &out,
             const Nba97FrontendIoCompleteMachine &value) {
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
} // namespace

std::string captureFrontendIoComplete() {
  try {
    Capture capture;
    const U active_before = capture.get(Active);
    const U busy_before = capture.get(Busy);
    const U status_before = capture.get(Status);
    const int result = nba97_frontend_exit_drain_with_recovered_io_complete(
        &capture.drain, &capture.poll, &capture.drain_progress,
        &capture.adapter);
    std::vector<Call> calls = capture.fallback_calls;
    const std::size_t poll_events =
        std::min(capture.poll.parent_events, capture.poll_records.size());
    for (std::size_t i = 0; i < poll_events; ++i)
      calls.push_back({capture.poll_records[i].event,
                       capture.poll_records[i].parent_machine});
    std::sort(calls.begin(), calls.end(), [](const Call &a, const Call &b) {
      return a.event.operation < b.event.operation;
    });
    if (result != NBA97_TEXT_COMPLETE || !capture.drain_progress.completed ||
        capture.drain_progress.operations != 15 ||
        capture.drain_progress.accesses != 7 ||
        capture.drain_progress.callbacks_completed != 8 ||
        capture.drain_progress.poll_attempts != 2 ||
        capture.drain_progress.instruction_count != 44 ||
        capture.drain_progress.access_events > capture.drain_access.size() ||
        capture.drain_progress.instruction_events > capture.drain_instructions.size() ||
        capture.poll.invocations != 2 || capture.poll.completions != 2 ||
        poll_events != 2 || calls.size() != 8 ||
        capture.poll_records[0].progress.operations != 2 ||
        capture.poll_records[0].progress.instruction_count != 16 ||
        capture.poll_records[0].progress.machine.registers.gpr[2].word != 0 ||
        capture.poll_records[1].progress.operations != 9 ||
        capture.poll_records[1].progress.instruction_count != 81 ||
        capture.poll_records[1].progress.machine.registers.gpr[2].word != 1 ||
        capture.get(Active) != 0 || capture.get(Busy) != 0 ||
        capture.get(Status) != 0)
      capture.contract_failure = true;

    std::ostringstream out;
    out << "{\"program\":\"FEONLY\",\"address\":" << hex(0x800392a0u)
        << ",\"inclusive_end\":" << hex(0x800392f7u)
        << ",\"bytes\":88,\"instructions\":22,"
        << "\"source_sha256\":\"dca1d4f4bf2b7847a1175abe703ab434c4ed51efccb2af341b536de466f98d7a\","
        << "\"completed\":" << unsigned(capture.drain_progress.completed)
        << ",\"result\":" << result << ",\"contract_failure\":"
        << capture.contract_failure
        << ",\"classification\":\"no direct visual effect\","
        << "\"gameplay_shown\":\"BLOCKED\","
        << "\"fixture_contract\":\"Synthetic standalone full machine and retained memory enter the recovered frontend-exit-drain owner. Its repeated 0x800394F0 calls genuinely execute the recovered 0x800392A0 I/O-completion owner. The explicit preparation fixture for already-recovered 0x800393F0 and unbound 0x80029B64, 0x8008C274, 0x8006CDE4 and 0x8006AE60 fixtures preserve all 32 GPR words/masks plus HI/LO and make no guest-memory changes. The unbound 0x80038E84 pump likewise preserves the full machine and clears only status word 0x800EF840 from 0x80000000 to zero, causing the next actual poll to scan all eight zero slots and return V0=1. Drain-owner flag stores remain recovered source effects.\","
        << "\"status_memory\":{\"active_before\":" << hex(active_before)
        << ",\"active_after\":" << hex(capture.get(Active))
        << ",\"busy_before\":" << hex(busy_before)
        << ",\"busy_after\":" << hex(capture.get(Busy))
        << ",\"slot0_before\":" << hex(status_before)
        << ",\"slot0_after\":" << hex(capture.get(Status))
        << ",\"final_slots\":[";
    for (unsigned slot = 0; slot < 8; ++slot) {
      if (slot) out << ',';
      out << hex(capture.get(Status + slot * 0x24u));
    }
    out << "]},\"drain\":{\"operations\":"
        << capture.drain_progress.operations << ",\"accesses\":"
        << capture.drain_progress.accesses << ",\"callbacks\":"
        << capture.drain_progress.callbacks_completed
        << ",\"poll_invocations\":" << capture.poll.invocations
        << ",\"instruction_count\":"
        << capture.drain_progress.instruction_count
        << ",\"instruction_trace\":[";
    const std::size_t drain_instruction_events = std::min(
        capture.drain_progress.instruction_events,
        capture.drain_instructions.size());
    for (std::size_t i = 0; i < drain_instruction_events; ++i) {
      if (i) out << ',';
      out << hex(capture.drain_instructions[i]);
    }
    out << "],\"access_journal\":[";
    const std::size_t drain_access_events = std::min(
        capture.drain_progress.access_events, capture.drain_access.size());
    for (std::size_t i = 0; i < drain_access_events; ++i) {
      if (i) out << ',';
      access(out, capture.drain_access[i]);
    }
    out << "],\"call_sequence\":[";
    for (std::size_t i = 0; i < calls.size(); ++i) {
      const auto &call = calls[i];
      if (i) out << ',';
      out << "{\"pc\":" << hex(call.event.pc) << ",\"delay\":"
          << hex(call.event.delay_slot_pc) << ",\"target\":"
          << hex(call.event.entry) << ",\"argument_count\":"
          << unsigned(call.event.argument_count) << ",\"invocation\":"
          << call.event.invocation << ",\"operation\":"
          << call.event.operation << ",\"machine\":";
      machine(out, call.machine);
      out << '}';
    }
    out << "]},\"io_complete\":{\"invocations\":"
        << capture.poll.invocations << ",\"records\":[";
    for (std::size_t i = 0; i < poll_events; ++i) {
      const auto &record = capture.poll_records[i];
      if (i) out << ',';
      out << "{\"parent_pc\":" << hex(record.event.pc)
          << ",\"invocation\":" << record.event.invocation
          << ",\"result\":" << record.result
          << ",\"operations\":" << record.progress.operations
          << ",\"accesses\":" << record.progress.accesses
          << ",\"status_reads\":" << record.progress.status_reads
          << ",\"instruction_count\":" << record.progress.instruction_count
          << ",\"active_word\":";
      word(out, record.progress.active_word);
      out << ",\"last_status\":";
      word(out, record.progress.last_status);
      out << ",\"parent_machine\":";
      machine(out, record.parent_machine);
      out << ",\"instruction_trace\":[";
      for (std::size_t event = 0; event < record.instruction_events; ++event) {
        if (event) out << ',';
        out << hex(record.instruction_journal[event]);
      }
      out << "],\"access_journal\":[";
      for (std::size_t event = 0; event < record.access_events; ++event) {
        if (event) out << ',';
        access(out, record.access_journal[event]);
      }
      out << "],\"final_machine\":";
      machine(out, record.progress.machine);
      out << '}';
    }
    out << "]},\"final_machine\":";
    machine(out, capture.drain_progress.machine);
    out << ",\"next_unbound_boundary\":{"
           "\"earliest_production\":\"unowned FEONLY overlay-entry JAL 0x8007B838 and startup service 0x80028810 -> 0x8007B844\","
           "\"first_drain_child\":\"0x800394E8 -> 0x800393F0 is composed in the separate three-owner integration test; this standalone capture retains its explicit preparation fixture\","
           "\"after_poll\":\"a zero poll result next calls unbound full-machine pump 0x80039500 -> 0x80038E84\","
           "\"other_caller\":\"0x8003949C in the recovered I/O-drain owner also composes this poll through its exact natural-site adapter; its zero-result pump 0x80038E84 remains unbound\","
           "\"gameplay_dependency\":\"production frontend lifecycle, loader handoff, and advancing native court/player match remain unbound\"}}";
    return out.str();
  } catch (...) {
    return "{\"program\":\"FEONLY\",\"address\":\"0x800392a0\",\"bytes\":88,\"completed\":0,\"result\":0,\"contract_failure\":true,\"classification\":\"no direct visual effect\",\"gameplay_shown\":\"BLOCKED\"}";
  }
}
} // namespace nba97
