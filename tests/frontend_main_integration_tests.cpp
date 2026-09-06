#include "frontend_main_adapter.h"
#include "frontend_main_capture.h"
#include "user_setup_session.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("frontend-main integration failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

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

struct Composition {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U Context = 0x800214f0u;
  static constexpr U Allocation = 0x80140000u;
  static constexpr U Roster = 0x80160000u;
  static constexpr U Sp = 0x801f0000u;
  static constexpr U Handle = 0x80170000u;
  static constexpr U LoadSize = 0x1000u;
  static constexpr U GameEntry = 0x801e1410u;
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
  std::vector<Nba97FrontendMainEvent> main_calls;
  std::vector<Nba97FrontendDispatchEvent> dispatch_calls;
  nba97::UserSetupSession session;
  bool setup_accepted = false;
  bool transfer = false;
  bool relocate_all_frames = false;
  U refuse_dispatch_pc = 0;

  Composition() {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x22000000u + i * 0x101u, 15};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[NBA97_FRONTEND_MAIN_SP] = {Sp, 15};
    machine.registers.gpr[NBA97_FRONTEND_MAIN_RA] = {0x8007b840u, 15};
    machine.hi = {0x12345678u, 5};
    machine.lo = {0x9abcdef0u, 10};
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
    for (unsigned i = 0; i < DispatchTargets.size(); ++i)
      put(0x80024f80u + i * 4u, DispatchTargets[i]);
    for (unsigned team = 0; team < 2; ++team)
      for (unsigned slot = 0; slot < 12; ++slot) {
        const U source = Roster + (team + 1u) * 0x600u + slot * 0x80u;
        put(0x80023ab0u + team * 104u + slot * 4u, source);
        for (unsigned byte = 0; byte < 110; ++byte)
          put(source + byte, team * 17u + slot + byte, 1);
      }
    put(0x800ef754u, Allocation);
    for (U i = 0; i < LoadSize; ++i)
      put(Handle + i, i * 37u + 11u, 1);
    put(Handle, GameEntry);

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

  void put(U address, U value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i)
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
  }
  U get(U address, unsigned width = 4) const {
    U value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }

  static int mainIo(void *opaque, const Nba97GameTextMemory *,
                    const Nba97FrontendMainEvent *event,
                    Nba97FrontendMainMachine *machine,
                    Nba97FrontendMainCalleeOutcome *outcome) {
    auto &c = *static_cast<Composition *>(opaque);
    Nba97FrontendMainSiteContract contract{};
    if (!event || !machine || !outcome ||
        !nba97_frontend_main_site_contract(event->site, &contract) ||
        event->pc != contract.pc || event->delay_slot_pc != contract.delay_slot_pc ||
        event->argument_count != contract.argument_count ||
        event->target_program != contract.target_program ||
        (!contract.dynamic_target && event->entry != contract.target) ||
        (contract.dynamic_target && event->entry != GameEntry))
      return 0;
    c.main_calls.push_back(*event);
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
          machine->registers.gpr[NBA97_FRONTEND_MAIN_A2].word != LoadSize)
        return 0;
      for (U i = 0; i < LoadSize; ++i) {
        c.bytes[0x801e0000u - Base + i] = c.bytes[Handle - Base + i];
        if (c.region.known)
          c.known[0x801e0000u - Base + i] = c.known[Handle - Base + i];
      }
    } else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028B68) {
      *outcome = c.transfer ? NBA97_FRONTEND_MAIN_CALLEE_TRANSFERRED
                            : NBA97_FRONTEND_MAIN_CALLEE_RETURNED;
    }
    return 1;
  }

  static int dispatchIo(void *opaque, const Nba97GameTextMemory *,
                        const Nba97FrontendDispatchEvent *event,
                        Nba97FrontendDispatchMachine *machine) {
    auto &c = *static_cast<Composition *>(opaque);
    Nba97FrontendDispatchSiteContract contract{};
    if (!event || !machine ||
        !nba97_frontend_dispatch_site_contract(event->site, &contract) ||
        event->pc != contract.pc || event->delay_slot_pc != contract.delay_slot_pc ||
        event->target != contract.target ||
        event->argument_count != contract.argument_count)
      return 0;
    c.dispatch_calls.push_back(*event);
    if (c.refuse_dispatch_pc == event->pc)
      return 0;
    if (c.relocate_all_frames && c.dispatch_calls.size() == 1) {
      const U old_frame = machine->registers.gpr[29].word;
      const U new_frame = old_frame - 0x200u;
      for (unsigned i = 0; i < 0xc8; ++i) {
        c.bytes[new_frame - Base + i] = c.bytes[old_frame - Base + i];
        if (c.region.known)
          c.known[new_frame - Base + i] = c.known[old_frame - Base + i];
      }
      machine->registers.gpr[29] = {new_frame, 15};
    }
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
          target - Base > c.bytes.size() - count)
        return 0;
      for (U i = 0; i < count; ++i) {
        c.bytes[target - Base + i] = c.bytes[source - Base + i];
        if (c.region.known)
          c.known[target - Base + i] = c.known[source - Base + i];
      }
    } else
      machine->registers.gpr[2] = {UINT32_MAX, 15};
    return 1;
  }

  int run() {
    Nba97GameTextMemory memory{&region, 1};
    return nba97_frontend_main_from_overlay_entry(&binding, &memory, &parent,
                                                   &machine);
  }
};

void syntheticParentNaturalOwnerChain() {
  Composition c;
  CHECK(c.run() == 1 && c.binding.result == NBA97_TEXT_COMPLETE &&
        c.binding.progress.completed && !c.binding.progress.transferred &&
        c.setup_accepted && c.session.state().result == 6);
  CHECK(c.binding.event.pc == 0x8007b838u &&
        c.binding.event.delay_slot_pc == 0x8007b83cu &&
        c.binding.event.entry == 0x80028800u &&
        c.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP].word == Composition::Sp &&
        c.machine.registers.gpr[NBA97_FRONTEND_MAIN_RA].word == 0x8007b840u);
  CHECK(c.binding.adapter.wrapper_invocations == 1 &&
        c.binding.adapter.wrapper_completions == 1 &&
        c.binding.adapter.wrapper_result == NBA97_TEXT_COMPLETE &&
        c.binding.adapter.wrapper_progress.completed &&
        c.binding.adapter.wrapper_adapter.dispatcher_result ==
            NBA97_TEXT_COMPLETE &&
        c.binding.adapter.wrapper_adapter.dispatcher_progress.completed &&
        c.dispatch_calls.size() == 42);
  CHECK(c.binding.progress.gameload_handle.word == Composition::Handle &&
        c.binding.progress.gameload_size.word == Composition::LoadSize &&
        c.binding.progress.dynamic_entry.word == Composition::GameEntry &&
        c.binding.progress.operations == 98 &&
        c.binding.progress.accesses == 33 &&
        c.binding.progress.callbacks_completed == 65 &&
        c.binding.progress.instruction_count == 299 &&
        c.main_calls.size() == 64 &&
        c.main_calls.back().target_program ==
            NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD);
  CHECK(c.get(0x800170c0u) == Composition::Context &&
        c.get(Composition::Context + 0x14u) == 0x8001726cu &&
        c.get(0x80015098u) == 1 &&
        c.get(0x801e0000u) == Composition::GameEntry);
  for (U i = 0; i < Composition::LoadSize; ++i)
    CHECK(c.bytes[0x801e0000u - Composition::Base + i] ==
          c.bytes[Composition::Handle - Composition::Base + i]);

  Composition transferred;
  transferred.transfer = true;
  CHECK(transferred.run() == 1 && transferred.binding.progress.completed &&
        transferred.binding.progress.transferred &&
        transferred.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP].word ==
            Composition::Sp - 40u);
}

void nestedRelocationFailureAndFreshReuse() {
  for (bool known_plane : {true, false}) {
    Composition moved;
    moved.relocate_all_frames = true;
    if (!known_plane)
      moved.region.known = nullptr;
    CHECK(moved.run() == 1 && moved.binding.progress.completed &&
          moved.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP].word ==
              Composition::Sp - 0x200u &&
          moved.machine.registers.gpr[NBA97_FRONTEND_MAIN_RA].word ==
              0x8007b840u);
  }

  Composition refused;
  refused.refuse_dispatch_pc = 0x8003fcf4u;
  CHECK(refused.run() == 0 &&
        refused.binding.result == NBA97_TEXT_IO_REFUSED &&
        refused.binding.progress.stopped_pc == 0x80028aa0u &&
        refused.binding.adapter.wrapper_result == NBA97_TEXT_IO_REFUSED &&
        refused.binding.adapter.wrapper_adapter.dispatcher_result ==
            NBA97_TEXT_IO_REFUSED &&
        refused.binding.adapter.wrapper_adapter.dispatcher_progress.stopped_pc ==
            0x8003fcf4u);

  Composition reused;
  CHECK(reused.run() == 1 && reused.binding.wrapper.invocations == 1);
  reused.binding.wrapper.dispatcher.access_journal = nullptr;
  reused.binding.wrapper.dispatcher.access_journal_capacity = 1;
  reused.machine = {};
  for (unsigned i = 0; i < 32; ++i)
    reused.machine.registers.gpr[i] = {0x44000000u + i, 15};
  reused.machine.registers.gpr[0] = {0, 15};
  reused.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP] = {Composition::Sp, 15};
  reused.machine.registers.gpr[NBA97_FRONTEND_MAIN_RA] = {0x8007b840u, 15};
  CHECK(reused.run() == 0 && reused.binding.invocations == 2 &&
        reused.binding.completions == 1 &&
        reused.binding.adapter.wrapper_result == NBA97_TEXT_IO_REFUSED &&
        reused.binding.adapter.wrapper_progress.callbacks_completed == 0 &&
        reused.binding.adapter.wrapper_adapter.dispatcher_result ==
            NBA97_TEXT_ARGUMENT &&
        reused.binding.adapter.wrapper_adapter.dispatcher_progress.operations ==
            0);
}

void parentGuardsAndCaptureSmoke() {
  for (unsigned field = 0; field < 8; ++field) {
    Composition c;
    if (field == 0) c.parent.pc ^= 4;
    else if (field == 1) c.parent.delay_slot_pc ^= 4;
    else if (field == 2) c.parent.entry ^= 4;
    else if (field == 3) c.parent.invocation = 2;
    else if (field == 4) c.parent.argument_count = 1;
    else if (field == 5) c.parent.target_program = NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD;
    else if (field == 6) c.machine.registers.gpr[NBA97_FRONTEND_MAIN_RA].word ^= 4;
    else c.machine.registers.gpr[NBA97_FRONTEND_MAIN_RA].known_mask = 14;
    const auto before = c.machine;
    CHECK(c.run() == 0 && c.binding.result == NBA97_TEXT_ARGUMENT &&
          c.binding.invocations == 0);
    for (unsigned i = 0; i < 32; ++i)
      CHECK(c.machine.registers.gpr[i].word == before.registers.gpr[i].word &&
            c.machine.registers.gpr[i].known_mask ==
                before.registers.gpr[i].known_mask);
  }

  const std::string receipt = nba97::captureFrontendMain();
  CHECK(!receipt.empty() && receipt.front() == '{' && receipt.back() == '}');
  for (unsigned char byte : receipt)
    CHECK(std::isprint(byte));
  const auto bytes = receipt.find("\"bytes\":");
  CHECK(bytes != std::string::npos &&
        std::isdigit(static_cast<unsigned char>(receipt[bytes + 8])));
  CHECK(receipt.find("\"contract_failure\":0") != std::string::npos &&
        receipt.find("\"operations\":98,\"accesses\":33") !=
            std::string::npos &&
        receipt.find("\"callbacks\":65,\"instruction_count\":299") !=
            std::string::npos &&
        receipt.find("\"copy_size\":4096") != std::string::npos &&
        receipt.find("\"result\":6") != std::string::npos &&
        receipt.find("\"earliest_production\"") != std::string::npos);
}
} // namespace

int main() {
  try {
    syntheticParentNaturalOwnerChain();
    nestedRelocationFailureAndFreshReuse();
    parentGuardsAndCaptureSmoke();
    std::printf("frontend_main_integration_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
