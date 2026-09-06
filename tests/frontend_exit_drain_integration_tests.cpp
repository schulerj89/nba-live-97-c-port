#include "frontend_exit_drain_adapter.h"
#include "frontend_exit_drain_capture.h"
#include "frontend_exit_cleanup_adapter.h"
#include "frontend_exit_wait_adapter.h"

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
    throw std::runtime_error("frontend-exit-drain integration failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr std::array<U, 5> CleanupPcs{{0x8002f08cu, 0x8002f094u,
                                        0x8002f0a4u, 0x8002f0c0u,
                                        0x8002f0d0u}};
constexpr std::array<U, 7> DrainPcs{{0x800394e8u, 0x800394f0u, 0x80039500u,
                                      0x80039530u, 0x80039538u, 0x80039554u,
                                      0x8003955cu}};
constexpr std::array<unsigned, 7> DrainArgs{{0, 0, 0, 2, 0, 1, 0}};

struct Integration {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U Sp = 0x801f0000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendExitCleanupContext cleanup{};
  Nba97FrontendExitCleanupProgress cleanup_progress{};
  Nba97FrontendExitDrainBinding drain{};
  Nba97FrontendExitWaitBinding wait{};
  std::array<Nba97FrontendExitCleanupAccess, 12> cleanup_access{};
  std::array<U, 32> cleanup_instructions{};
  std::array<Nba97FrontendExitDrainAccess, 12> drain_access{};
  std::array<U, 64> drain_instructions{};
  std::vector<Nba97FrontendExitCleanupEvent> cleanup_calls;
  std::vector<Nba97FrontendExitDrainEvent> drain_calls;
  std::array<std::size_t, NBA97_FRONTEND_EXIT_DRAIN_SITE_COUNT>
      drain_invocations{};
  U refuse_drain_pc = 0;
  bool relocate_combined_frames = false;

  Integration() {
    // The actual recovered wait owner takes its explicit retained sentinel
    // path before cleanup reaches this drain. No wait child ABI is invented.
    wait.operation_budget = 5;
    put(0x80017268u, UINT32_MAX);
    for (unsigned i = 0; i < 32; ++i)
      cleanup.machine.registers.gpr[i] = {0x71000000u + i * 0x101u, 15};
    cleanup.machine.registers.gpr[0] = {0, 15};
    cleanup.machine.registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_SP] = {Sp, 15};
    cleanup.machine.registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_RA] = {
        0x80028ab0u, 15};
    cleanup.machine.hi = {0x11223344u, 5};
    cleanup.machine.lo = {0x55667788u, 10};
    put(0x80021d6cu, 0xffffffffu);
    put(0x8001502cu, 0);
    put(0x800f84c4u, 1);
    put(0x800f43b0u, 0x13579bdfu);
    put(0x8002149cu, 0x80145678u);
    cleanup.memory = {&region, 1};
    cleanup.operation_budget = 8;
    cleanup.io = cleanupIo;
    cleanup.user = this;
    cleanup.access_journal = cleanup_access.data();
    cleanup.access_journal_capacity = cleanup_access.size();
    cleanup.instruction_journal = cleanup_instructions.data();
    cleanup.instruction_journal_capacity = cleanup_instructions.size();
    drain.operation_budget = 16;
    drain.io = drainIo;
    drain.user = this;
    drain.access_journal = drain_access.data();
    drain.access_journal_capacity = drain_access.size();
    drain.instruction_journal = drain_instructions.data();
    drain.instruction_journal_capacity = drain_instructions.size();
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

  static int cleanupIo(void *opaque, const Nba97GameTextMemory *memory,
                       const Nba97FrontendExitCleanupEvent *event,
                       Nba97FrontendExitCleanupMachine *machine) {
    auto &f = *static_cast<Integration *>(opaque);
    Nba97FrontendExitCleanupSiteContract contract{};
    if (!event || !machine ||
        !nba97_frontend_exit_cleanup_site_contract(event->site, &contract) ||
        event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->entry != contract.target ||
        event->argument_count != contract.argument_count ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        event->invocation != 1 ||
        machine->registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_RA].word !=
            event->pc + 8u ||
        machine->registers.gpr[NBA97_FRONTEND_EXIT_CLEANUP_RA].known_mask != 15)
      return 0;
    f.cleanup_calls.push_back(*event);
    if (event->site == NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F08C)
      return nba97_frontend_exit_wait_from_frontend_exit_cleanup(
          &f.wait, memory, event, machine);
    if (event->site == NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F094)
      return nba97_frontend_exit_drain_from_frontend_exit_cleanup(
          &f.drain, memory, event, machine);
    return 1;
  }

  static int drainIo(void *opaque, const Nba97GameTextMemory *,
                     const Nba97FrontendExitDrainEvent *event,
                     Nba97FrontendExitDrainMachine *machine) {
    auto &f = *static_cast<Integration *>(opaque);
    Nba97FrontendExitDrainSiteContract contract{};
    if (!event || !machine ||
        !nba97_frontend_exit_drain_site_contract(event->site, &contract) ||
        event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->entry != contract.target ||
        event->argument_count != contract.argument_count ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        event->invocation != ++f.drain_invocations[event->site] ||
        machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_RA].word !=
            event->pc + 8u ||
        machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_RA].known_mask != 15)
      return 0;
    f.drain_calls.push_back(*event);
    if (event->site == NBA97_FRONTEND_EXIT_DRAIN_SITE_800394F0)
      machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_V0] = {
          event->invocation == 1 ? 0u : 1u, 15};
    if (f.relocate_combined_frames &&
        event->site == NBA97_FRONTEND_EXIT_DRAIN_SITE_800394E8) {
      const U old_frame = machine->registers.gpr[29].word;
      const U new_frame = old_frame - 0x100u;
      if (old_frame < Base || new_frame < Base ||
          old_frame - Base > f.bytes.size() - 48u ||
          new_frame - Base > f.bytes.size() - 48u)
        return 0;
      for (U i = 0; i < 48; ++i) {
        f.bytes[new_frame - Base + i] = f.bytes[old_frame - Base + i];
        if (f.region.known)
          f.known[new_frame - Base + i] = f.known[old_frame - Base + i];
      }
      machine->registers.gpr[29] = {new_frame, 15};
    }
    return event->pc != f.refuse_drain_pc;
  }

  int run() {
    return nba97_frontend_exit_cleanup(&cleanup, &cleanup_progress);
  }
};

void naturalCleanupComposition() {
  Integration f;
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.cleanup_progress.completed &&
        f.drain.invocations == 1 && f.drain.completions == 1 &&
        f.drain.result == NBA97_TEXT_COMPLETE && f.drain.progress.completed);
  CHECK(f.wait.invocations == 1 && f.wait.completions == 1 &&
        f.wait.progress.completed && f.wait.progress.callbacks_completed == 0 &&
        f.wait.progress.exit_path == NBA97_FRONTEND_EXIT_WAIT_EXIT_SENTINEL);
  CHECK(f.cleanup_calls.size() == 4 &&
        f.cleanup_calls[0].pc == CleanupPcs[0] &&
        f.cleanup_calls[1].pc == CleanupPcs[1] &&
        f.cleanup_calls[2].pc == CleanupPcs[2] &&
        f.cleanup_calls[3].pc == CleanupPcs[4]);
  CHECK(f.drain.event.pc == 0x8002f094u &&
        f.drain.event.delay_slot_pc == 0x8002f098u &&
        f.drain.event.entry == 0x800394d4u &&
        f.drain.event.argument_count == 0 &&
        f.drain.progress.operations == 15 &&
        f.drain.progress.accesses == 7 &&
        f.drain.progress.callbacks_completed == 8 &&
        f.drain.progress.instruction_count == 44 &&
        f.drain_calls.size() == 8 && f.get(0x800f84c4u) == 0 &&
        f.get(0x800f43b0u) == 0);
  const std::array<U, 8> expected{{DrainPcs[0], DrainPcs[1], DrainPcs[2],
                                   DrainPcs[1], DrainPcs[3], DrainPcs[4],
                                   DrainPcs[5], DrainPcs[6]}};
  for (unsigned i = 0; i < f.drain_calls.size(); ++i) {
    CHECK(f.drain_calls[i].pc == expected[i] &&
          f.drain_calls[i].argument_count ==
              DrainArgs[f.drain_calls[i].site - 1]);
  }
  CHECK(f.drain_calls[4].argument_count == 2 &&
        f.drain_calls[4].pc == 0x80039530u &&
        f.drain_calls[6].argument_count == 1 &&
        f.drain_calls[6].pc == 0x80039554u &&
        f.cleanup_progress.machine.registers.gpr[29].word == Integration::Sp &&
        f.cleanup_progress.machine.registers.gpr[31].word == 0x80028ab0u);
}

void nestedRefusalAndReuse() {
  Integration refused;
  refused.refuse_drain_pc = 0x80039538u;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.cleanup_progress.stopped_pc == 0x8002f094u &&
        refused.drain.result == NBA97_TEXT_IO_REFUSED &&
        refused.drain.progress.stopped_pc == 0x80039538u &&
        refused.drain.progress.callbacks_completed == 5 &&
        refused.cleanup_progress.callbacks_completed == 1);

  Integration reused;
  CHECK(reused.run() == NBA97_TEXT_COMPLETE && reused.drain.invocations == 1);
  reused.cleanup_calls.clear();
  reused.drain_calls.clear();
  reused.drain_invocations.fill(0);
  reused.cleanup.machine.registers.gpr[29] = {Integration::Sp, 15};
  reused.cleanup.machine.registers.gpr[31] = {0x80028ab0u, 15};
  reused.put(0x800f84c4u, 1);
  reused.put(0x800f43b0u, 1);
  CHECK(reused.run() == NBA97_TEXT_COMPLETE && reused.drain.invocations == 2 &&
        reused.drain.completions == 2 && reused.drain.progress.completed);
}

void adapterContractsAndGuards() {
  for (unsigned site = 1; site < NBA97_FRONTEND_EXIT_DRAIN_SITE_COUNT; ++site) {
    Nba97FrontendExitDrainSiteContract contract{};
    CHECK(nba97_frontend_exit_drain_site_contract(
              static_cast<std::uint8_t>(site), &contract) == 1 &&
          contract.pc == DrainPcs[site - 1] &&
          contract.delay_slot_pc == DrainPcs[site - 1] + 4u &&
          contract.argument_count == DrainArgs[site - 1] &&
          contract.target_program == NBA97_FRONTEND_MAIN_PROGRAM_FEONLY);
  }
  Nba97FrontendExitDrainSiteContract ignored{};
  CHECK(nba97_frontend_exit_drain_site_contract(0, &ignored) == 0 &&
        nba97_frontend_exit_drain_site_contract(
            NBA97_FRONTEND_EXIT_DRAIN_SITE_COUNT, &ignored) == 0 &&
        nba97_frontend_exit_drain_site_contract(1, nullptr) == 0);

  for (unsigned field = 0; field < 9; ++field) {
    Integration f;
    Nba97FrontendExitCleanupEvent event{
        0x8002f094u, 0x8002f098u, 0x800394d4u, 2, 1,
        NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F094, 0,
        NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
    auto machine = f.cleanup.machine;
    machine.registers.gpr[31] = {0x8002f09cu, 15};
    if (field == 0) event.pc ^= 4;
    else if (field == 1) event.delay_slot_pc ^= 4;
    else if (field == 2) event.entry ^= 4;
    else if (field == 3) event.invocation = 2;
    else if (field == 4) event.argument_count = 1;
    else if (field == 5) event.site = NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F08C;
    else if (field == 6)
      event.target_program = NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD;
    else if (field == 7) machine.registers.gpr[31].word ^= 4;
    else machine.registers.gpr[31].known_mask = 14;
    const auto before = machine;
    Nba97GameTextMemory memory{&f.region, 1};
    CHECK(nba97_frontend_exit_drain_from_frontend_exit_cleanup(
              &f.drain, &memory, &event, &machine) == 0 &&
          f.drain.invocations == 0 &&
          f.drain.result == NBA97_TEXT_ARGUMENT);
    for (unsigned reg = 0; reg < 32; ++reg)
      CHECK(machine.registers.gpr[reg].word == before.registers.gpr[reg].word &&
            machine.registers.gpr[reg].known_mask ==
                before.registers.gpr[reg].known_mask);
  }
}

void relocatedCombinedFrames() {
  for (bool known_plane : {true, false}) {
    Integration f;
    f.relocate_combined_frames = true;
    if (!known_plane)
      f.region.known = nullptr;
    CHECK(f.run() == NBA97_TEXT_COMPLETE && f.cleanup_progress.completed &&
          f.drain.progress.completed && f.drain_calls.size() == 8 &&
          f.drain.progress.restored_return_address.word == 0x8002f09cu &&
          f.cleanup_progress.restored_return_address.word == 0x80028ab0u &&
          f.cleanup_progress.machine.registers.gpr[29].word ==
              Integration::Sp - 0x100u &&
          f.cleanup_progress.machine.registers.gpr[31].word == 0x80028ab0u);
  }
}

void captureSmoke() {
  const std::string receipt = nba97::captureFrontendExitDrain();
  CHECK(!receipt.empty() && receipt.front() == '{' && receipt.back() == '}');
  for (unsigned char byte : receipt) CHECK(std::isprint(byte));
  CHECK(receipt.find("\"contract_failure\":0") != std::string::npos &&
        receipt.find("\"operations\":15,\"accesses\":7") !=
            std::string::npos &&
        receipt.find("\"callbacks\":8,\"poll_attempts\":2") !=
            std::string::npos &&
        receipt.find("\"gameplay_shown\":\"BLOCKED\"") !=
            std::string::npos &&
        receipt.find("0x80039554 -> 0x8006CDE4") != std::string::npos);
}
} // namespace

int main() {
  try {
    naturalCleanupComposition();
    nestedRefusalAndReuse();
    adapterContractsAndGuards();
    relocatedCombinedFrames();
    captureSmoke();
    std::printf("frontend_exit_drain_integration_tests: PASS (%u checks)\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
