#include "frontend_exit_cleanup_adapter.h"
#include "frontend_exit_cleanup_capture.h"
#include "frontend_main_adapter.h"
#include "user_setup_session.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("frontend-exit-cleanup integration failed at " +
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

struct Integration {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U Context = 0x800214f0u;
  static constexpr U Allocation = 0x80140000u;
  static constexpr U Roster = 0x80160000u;
  static constexpr U Handle = 0x80170000u;
  static constexpr U Destination = 0x801e0000u;
  static constexpr U LoadSize = 0x1000u;
  static constexpr U GameEntry = 0x801e1410u;
  static constexpr U Sp = 0x801f0000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendMainContext main{};
  Nba97FrontendMainProgress main_progress{};
  Nba97FrontendMainAdapterProgress main_adapter{};
  Nba97FrontendDispatchEntryBinding wrapper{};
  Nba97FrontendExitCleanupBinding cleanup{};
  std::array<Nba97FrontendMainAccess, 64> main_access{};
  std::array<U, 512> main_instructions{};
  std::array<Nba97FrontendDispatchEntryAccess, 8> wrapper_access{};
  std::array<Nba97FrontendDispatchAccess, 4096> dispatch_access{};
  std::array<Nba97FrontendExitCleanupAccess, 8> cleanup_access{};
  std::array<U, 32> cleanup_instructions{};
  std::vector<Nba97FrontendMainEvent> main_calls;
  std::vector<Nba97FrontendDispatchEvent> dispatch_calls;
  std::vector<Nba97FrontendExitCleanupEvent> cleanup_calls;
  nba97::UserSetupSession session;
  bool setup_accepted = false;
  U refuse_cleanup_pc = 0;
  bool relocate_cleanup_frames = false;

  Integration() {
    for (unsigned i = 0; i < 32; ++i)
      main.machine.registers.gpr[i] = {0x71000000u + i * 0x101u, 15};
    main.machine.registers.gpr[0] = {0, 15};
    main.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP] = {Sp, 15};
    main.machine.registers.gpr[NBA97_FRONTEND_MAIN_RA] = {0x8007b840u, 15};
    main.machine.hi = {0x11223344u, 5};
    main.machine.lo = {0x55667788u, 10};
    put(0x80021ee4u, 1);
    put(0x8001edecu, 0, 2);
    put(0x80021568u, 0, 2);
    put(0x800170c0u, 0);
    put(Context + 0x14u, 0);
    put(Context + 0x720u, 0, 2);
    put(0x8009821cu, Allocation);
    put(0x800982e0u, Allocation);
    put(0x8009352cu, Allocation);
    put(0x80015098u, 0);
    put(0x80021d74u, 0);
    put(0x80021d78u, 1);
    put(0x800ef754u, Allocation);
    put(0x80021d6cu, 0xffffffffu);
    put(0x8001502cu, 0x80145678u);
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

    main.memory = {&region, 1};
    main.operation_budget = 1000;
    main.io = mainIo;
    main.user = this;
    main.access_journal = main_access.data();
    main.access_journal_capacity = main_access.size();
    main.instruction_journal = main_instructions.data();
    main.instruction_journal_capacity = main_instructions.size();
    wrapper.operation_budget = 5;
    wrapper.access_journal = wrapper_access.data();
    wrapper.access_journal_capacity = wrapper_access.size();
    wrapper.dispatcher.operation_budget = 20000;
    wrapper.dispatcher.io = dispatchIo;
    wrapper.dispatcher.user = this;
    wrapper.dispatcher.access_journal = dispatch_access.data();
    wrapper.dispatcher.access_journal_capacity = dispatch_access.size();
    cleanup.operation_budget = 10;
    cleanup.io = cleanupIo;
    cleanup.user = this;
    cleanup.access_journal = cleanup_access.data();
    cleanup.access_journal_capacity = cleanup_access.size();
    cleanup.instruction_journal = cleanup_instructions.data();
    cleanup.instruction_journal_capacity = cleanup_instructions.size();
  }

  void put(U address, U value, unsigned width = 4) {
    if (address < Base || width > Size || address - Base > Size - width)
      throw std::runtime_error("integration fixture write outside RAM");
    for (unsigned i = 0; i < width; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known)
        known[address - Base + i] = 1;
    }
  }
  U get(U address, unsigned width = 4) const {
    U result = 0;
    for (unsigned i = 0; i < width; ++i)
      result |= U(bytes[address - Base + i]) << (i * 8u);
    return result;
  }

  static int mainIo(void *opaque, const Nba97GameTextMemory *memory,
                    const Nba97FrontendMainEvent *event,
                    Nba97FrontendMainMachine *machine,
                    Nba97FrontendMainCalleeOutcome *outcome) {
    auto &f = *static_cast<Integration *>(opaque);
    Nba97FrontendMainSiteContract contract{};
    if (!event || !machine || !outcome ||
        !nba97_frontend_main_site_contract(event->site, &contract) ||
        event->pc != contract.pc || event->delay_slot_pc != contract.delay_slot_pc ||
        event->argument_count != contract.argument_count ||
        event->target_program != contract.target_program ||
        (!contract.dynamic_target && event->entry != contract.target) ||
        (contract.dynamic_target && event->entry != GameEntry))
      return 0;
    f.main_calls.push_back(*event);
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028AA8)
      return nba97_frontend_exit_cleanup_from_frontend_main(
          &f.cleanup, memory, event, machine, outcome);
    if (event->site == NBA97_FRONTEND_MAIN_SITE_80028810)
      f.put(0x80015098u, 1);
    else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028880 ||
             event->site == NBA97_FRONTEND_MAIN_SITE_80028974)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {Allocation, 15};
    else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028ACC)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {Handle, 15};
    else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028AD8)
      machine->registers.gpr[NBA97_FRONTEND_MAIN_V0] = {LoadSize, 15};
    else if (event->site == NBA97_FRONTEND_MAIN_SITE_80028B54) {
      if (machine->registers.gpr[4].word != Handle ||
          machine->registers.gpr[5].word != Destination ||
          machine->registers.gpr[6].word != LoadSize)
        return 0;
      for (U i = 0; i < LoadSize; ++i) {
        f.bytes[Destination - Base + i] = f.bytes[Handle - Base + i];
        if (f.region.known)
          f.known[Destination - Base + i] = f.known[Handle - Base + i];
      }
    }
    return 1;
  }

  static int cleanupIo(void *opaque, const Nba97GameTextMemory *,
                       const Nba97FrontendExitCleanupEvent *event,
                       Nba97FrontendExitCleanupMachine *machine) {
    auto &f = *static_cast<Integration *>(opaque);
    Nba97FrontendExitCleanupSiteContract contract{};
    if (!event || !machine ||
        !nba97_frontend_exit_cleanup_site_contract(event->site, &contract) ||
        event->pc != contract.pc || event->delay_slot_pc != contract.delay_slot_pc ||
        event->entry != contract.target ||
        event->argument_count != contract.argument_count)
      return 0;
    f.cleanup_calls.push_back(*event);
    if (f.relocate_cleanup_frames && f.cleanup_calls.size() == 1) {
      const U old_frame = machine->registers.gpr[29].word;
      const U new_frame = old_frame - 0x100u;
      if (old_frame < Base || new_frame < Base ||
          old_frame - Base > f.bytes.size() - 64u ||
          new_frame - Base > f.bytes.size() - 64u)
        return 0;
      for (U i = 0; i < 64; ++i) {
        f.bytes[new_frame - Base + i] = f.bytes[old_frame - Base + i];
        if (f.region.known)
          f.known[new_frame - Base + i] = f.known[old_frame - Base + i];
      }
      machine->registers.gpr[29] = {new_frame, 15};
    }
    return event->pc != f.refuse_cleanup_pc;
  }

  static int dispatchIo(void *opaque, const Nba97GameTextMemory *,
                        const Nba97FrontendDispatchEvent *event,
                        Nba97FrontendDispatchMachine *machine) {
    auto &f = *static_cast<Integration *>(opaque);
    Nba97FrontendDispatchSiteContract contract{};
    if (!event || !machine ||
        !nba97_frontend_dispatch_site_contract(event->site, &contract) ||
        event->pc != contract.pc || event->delay_slot_pc != contract.delay_slot_pc ||
        event->target != contract.target ||
        event->argument_count != contract.argument_count)
      return 0;
    f.dispatch_calls.push_back(*event);
    if (event->target == 0x800770d4u)
      machine->registers.gpr[2] = {Allocation, 15};
    else if (event->target == 0x800459c8u)
      machine->registers.gpr[2] = {5, 15};
    else if (event->target == 0x80031a88u)
      f.put(Context + 0x720u, 5, 2);
    else if (event->target == 0x80037010u) {
      std::array<std::uint8_t, 8> assignments{{1, 2, 0, 0, 0, 0, 0, 0}};
      f.session.open(assignments, {}, 0);
      f.session.setControllers(0, 1);
      f.session.key(0, 0x80, true);
      const auto actions = f.session.step(100);
      f.setup_accepted =
          !actions.empty() && actions.back().event == NBA97_USER_CONFIRMED &&
          f.session.state().result == 6;
      machine->registers.gpr[2] = {f.setup_accepted ? 6u : 0u, 15};
    } else if (event->target == 0x800909a8u) {
      const U source = machine->registers.gpr[4].word;
      const U target = machine->registers.gpr[5].word;
      const U count = machine->registers.gpr[6].word;
      if (count != 0x6e || source < Base || target < Base ||
          source - Base > f.bytes.size() - count ||
          target - Base > f.bytes.size() - count)
        return 0;
      for (U i = 0; i < count; ++i) {
        f.bytes[target - Base + i] = f.bytes[source - Base + i];
        if (f.region.known)
          f.known[target - Base + i] = f.known[source - Base + i];
      }
    } else
      machine->registers.gpr[2] = {UINT32_MAX, 15};
    return 1;
  }

  int run() {
    return nba97_frontend_main_with_recovered_dispatch_entry(
        &main, &wrapper, &main_progress, &main_adapter);
  }
};

void naturalMainComposition() {
  Integration f;
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.main_progress.completed &&
        f.cleanup.invocations == 1 && f.cleanup.completions == 1 &&
        f.cleanup.result == NBA97_TEXT_COMPLETE && f.cleanup.progress.completed);
  CHECK(f.cleanup.event.pc == 0x80028aa8u &&
        f.cleanup.event.delay_slot_pc == 0x80028aacu &&
        f.cleanup.event.entry == 0x8002f084u &&
        f.cleanup.progress.operations == 10 &&
        f.cleanup.progress.accesses == 5 &&
        f.cleanup.progress.callbacks_completed == 5 &&
        f.cleanup.progress.instruction_count == 25 &&
        f.cleanup_calls.size() == 5 && f.get(0x8001502cu) == 0);
  CHECK(f.cleanup_calls[2].argument_count == 1 &&
        f.cleanup_calls[3].argument_count == 1 &&
        f.setup_accepted && f.session.state().result == 6 &&
        f.dispatch_calls.size() == 42 && f.main_adapter.wrapper_completions == 1 &&
        f.main_progress.machine.registers.gpr[29].word == Integration::Sp &&
        f.main_progress.machine.registers.gpr[31].word == 0x8007b840u);
}

void nestedRefusalAndAdapterGuards() {
  Integration refused;
  refused.refuse_cleanup_pc = 0x8002f094u;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.main_progress.stopped_pc == 0x80028aa8u &&
        refused.cleanup.result == NBA97_TEXT_IO_REFUSED &&
        refused.cleanup.progress.stopped_pc == 0x8002f094u &&
        refused.cleanup.progress.callbacks_completed == 1 &&
        refused.main_adapter.wrapper_completions == 1);

  Integration reused;
  CHECK(reused.run() == NBA97_TEXT_COMPLETE && reused.cleanup.invocations == 1);
  const auto stale_operations = reused.cleanup.progress.operations;
  reused.cleanup.access_journal = nullptr;
  reused.cleanup.access_journal_capacity = 1;
  reused.main.machine.registers.gpr[NBA97_FRONTEND_MAIN_SP] =
      {Integration::Sp, 15};
  reused.main.machine.registers.gpr[NBA97_FRONTEND_MAIN_RA] =
      {0x8007b840u, 15};
  CHECK(reused.run() == NBA97_TEXT_IO_REFUSED && reused.cleanup.invocations == 1 &&
        reused.cleanup.completions == 1 &&
        reused.cleanup.result == NBA97_TEXT_ARGUMENT &&
        reused.cleanup.progress.operations == stale_operations);

  for (unsigned field = 0; field < 9; ++field) {
    Integration f;
    Nba97FrontendMainEvent event{0x80028aa8u,
                                  0x80028aacu,
                                  0x8002f084u,
                                  1,
                                  1,
                                  NBA97_FRONTEND_MAIN_SITE_80028AA8,
                                  0,
                                  NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
    auto machine = f.main.machine;
    machine.registers.gpr[31] = {0x80028ab0u, 15};
    Nba97FrontendMainCalleeOutcome outcome =
        NBA97_FRONTEND_MAIN_CALLEE_TRANSFERRED;
    if (field == 0) event.pc ^= 4;
    else if (field == 1) event.delay_slot_pc ^= 4;
    else if (field == 2) event.entry ^= 4;
    else if (field == 3) event.invocation = 2;
    else if (field == 4) event.argument_count = 1;
    else if (field == 5) event.site = NBA97_FRONTEND_MAIN_SITE_80028AA0;
    else if (field == 6) event.target_program = NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD;
    else if (field == 7) machine.registers.gpr[31].word ^= 4;
    else machine.registers.gpr[31].known_mask = 14;
    const auto before = machine;
    Nba97GameTextMemory memory{&f.region, 1};
    CHECK(nba97_frontend_exit_cleanup_from_frontend_main(
              &f.cleanup, &memory, &event, &machine, &outcome) == 0 &&
          f.cleanup.invocations == 0 && f.cleanup.result == NBA97_TEXT_ARGUMENT &&
          outcome == NBA97_FRONTEND_MAIN_CALLEE_TRANSFERRED);
    for (unsigned reg = 0; reg < 32; ++reg)
      CHECK(machine.registers.gpr[reg].word == before.registers.gpr[reg].word &&
            machine.registers.gpr[reg].known_mask ==
                before.registers.gpr[reg].known_mask);
  }
}

void relocatedNaturalFrames() {
  for (bool known_plane : {true, false}) {
    Integration f;
    f.relocate_cleanup_frames = true;
    if (!known_plane)
      f.region.known = nullptr;
    CHECK(f.run() == NBA97_TEXT_COMPLETE && f.main_progress.completed &&
          f.cleanup.progress.completed && f.cleanup_calls.size() == 5 &&
          f.cleanup.progress.restored_return_address.word == 0x80028ab0u &&
          f.main_progress.machine.registers.gpr[29].word ==
              Integration::Sp - 0x100u &&
          f.main_progress.machine.registers.gpr[31].word == 0x8007b840u);
  }
}

void captureSmoke() {
  const std::string receipt = nba97::captureFrontendExitCleanup();
  CHECK(!receipt.empty() && receipt.front() == '{' && receipt.back() == '}');
  for (unsigned char byte : receipt) CHECK(std::isprint(byte));
  CHECK(receipt.find("\"contract_failure\":0") != std::string::npos &&
        receipt.find("\"operations\":10,\"accesses\":5") !=
            std::string::npos &&
        receipt.find("\"callbacks\":5,\"instruction_count\":25") !=
            std::string::npos &&
        receipt.find("\"next_cleanup_child\":\"0x8002F08C -> 0x8002EFBC\"") !=
            std::string::npos);
}
} // namespace

int main() {
  try {
    naturalMainComposition();
    nestedRefusalAndAdapterGuards();
    relocatedNaturalFrames();
    captureSmoke();
    std::printf("frontend_exit_cleanup_integration_tests: PASS (%u checks)\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
