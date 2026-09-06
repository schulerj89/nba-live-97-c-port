#include "frontend_exit_wait_adapter.h"
#include "frontend_exit_wait_capture.h"
#include "frontend_exit_cleanup_adapter.h"

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
    throw std::runtime_error("frontend-exit-wait integration failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

struct Integration {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U Sp = 0x801f0000u;
  static constexpr U Handle = 0x80145678u;
  static constexpr U Secondary = 0x80123458u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendExitCleanupContext cleanup{};
  Nba97FrontendExitCleanupProgress cleanup_progress{};
  Nba97FrontendExitWaitBinding wait{};
  std::array<Nba97FrontendExitCleanupAccess, 16> cleanup_access{};
  std::array<U, 32> cleanup_instructions{};
  std::array<Nba97FrontendExitWaitAccess, 16> wait_access{};
  std::array<U, 128> wait_instructions{};
  std::vector<Nba97FrontendExitCleanupEvent> cleanup_calls;
  std::vector<Nba97FrontendExitWaitEvent> wait_calls;
  U refuse_wait_pc = 0;
  bool relocate_frames = false;
  std::array<unsigned, NBA97_FRONTEND_EXIT_WAIT_SITE_COUNT> site_calls{};

  Integration() {
    for (unsigned i = 0; i < 32; ++i)
      cleanup.machine.registers.gpr[i] = {0x61000000u + i * 0x101u, 15};
    cleanup.machine.registers.gpr[0] = {0, 15};
    cleanup.machine.registers.gpr[16] = {0x10203040u, 15};
    cleanup.machine.registers.gpr[29] = {Sp, 15};
    cleanup.machine.registers.gpr[31] = {0x80028ab0u, 15};
    cleanup.machine.hi = {0x11223344u, 5};
    cleanup.machine.lo = {0x55667788u, 10};
    put(0x80021d6cu, UINT32_MAX);
    put(0x8001502cu, 0x80156780u);
    put(0x80017268u, Handle);
    put(0x8002149cu, Secondary);
    cleanup.memory = {&region, 1};
    cleanup.operation_budget = 10;
    cleanup.io = cleanupIo;
    cleanup.user = this;
    cleanup.access_journal = cleanup_access.data();
    cleanup.access_journal_capacity = cleanup_access.size();
    cleanup.instruction_journal = cleanup_instructions.data();
    cleanup.instruction_journal_capacity = cleanup_instructions.size();
    wait.operation_budget = 19;
    wait.io = waitIo;
    wait.user = this;
    wait.access_journal = wait_access.data();
    wait.access_journal_capacity = wait_access.size();
    wait.instruction_journal = wait_instructions.data();
    wait.instruction_journal_capacity = wait_instructions.size();
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
  U get(U address) const {
    U value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }

  static int cleanupIo(void *opaque, const Nba97GameTextMemory *memory,
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
    if (event->site == NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F08C)
      return nba97_frontend_exit_wait_from_frontend_exit_cleanup(
          &f.wait, memory, event, machine);
    return 1;
  }

  static int waitIo(void *opaque, const Nba97GameTextMemory *,
                    const Nba97FrontendExitWaitEvent *event,
                    Nba97FrontendExitWaitMachine *machine) {
    auto &f = *static_cast<Integration *>(opaque);
    Nba97FrontendExitWaitSiteContract contract{};
    if (!event || !machine ||
        !nba97_frontend_exit_wait_site_contract(event->site, &contract) ||
        event->pc != contract.pc || event->delay_slot_pc != contract.delay_slot_pc ||
        event->entry != contract.target ||
        event->argument_count != contract.argument_count)
      return 0;
    f.wait_calls.push_back(*event);
    const unsigned invocation = f.site_calls[event->site]++;
    if (f.relocate_frames && f.wait_calls.size() == 1) {
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
    if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFE4)
      machine->registers.gpr[2] = {1000, 15};
    else if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFF0 ||
             event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002F000)
      machine->registers.gpr[2] = {0, 15};
    else if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002F018)
      machine->registers.gpr[2] = {invocation == 0 ? 1361u : 1362u, 15};
    return event->pc != f.refuse_wait_pc;
  }

  int run() { return nba97_frontend_exit_cleanup(&cleanup, &cleanup_progress); }
};

void naturalCleanupComposition() {
  Integration f;
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.cleanup_progress.completed &&
        f.wait.invocations == 1 && f.wait.completions == 1 &&
        f.wait.result == NBA97_TEXT_COMPLETE && f.wait.progress.completed &&
        f.wait.progress.exit_path == NBA97_FRONTEND_EXIT_WAIT_EXIT_DEADLINE);
  CHECK(f.wait.event.pc == 0x8002f08cu &&
        f.wait.event.delay_slot_pc == 0x8002f090u &&
        f.wait.event.entry == 0x8002efbcu && f.wait_calls.size() == 10 &&
        f.wait.progress.operations == 19 && f.wait.progress.accesses == 9 &&
        f.wait.progress.instruction_count == 50 && f.cleanup_calls.size() == 5 &&
        f.get(0x80017268u) == UINT32_MAX && f.get(0x8002149cu) == 0 &&
        f.get(0x8001502cu) == 0 &&
        f.cleanup_progress.machine.registers.gpr[29].word == Integration::Sp &&
        f.cleanup_progress.machine.registers.gpr[31].word == 0x80028ab0u);
}

void relocationRefusalAndReuse() {
  for (bool known_plane : {true, false}) {
    Integration f;
    f.relocate_frames = true;
    if (!known_plane)
      f.region.known = nullptr;
    CHECK(f.run() == NBA97_TEXT_COMPLETE && f.wait.progress.completed &&
          f.wait.progress.restored_return_address.word == 0x8002f094u &&
          f.cleanup_progress.machine.registers.gpr[29].word ==
              Integration::Sp - 0x100u &&
          f.cleanup_progress.machine.registers.gpr[31].word == 0x80028ab0u);
  }

  Integration refused;
  refused.refuse_wait_pc = 0x8002f010u;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.cleanup_progress.stopped_pc == 0x8002f08cu &&
        refused.wait.result == NBA97_TEXT_IO_REFUSED &&
        refused.wait.progress.stopped_pc == 0x8002f010u &&
        refused.wait.progress.callbacks_completed == 4);

  Integration reused;
  CHECK(reused.run() == NBA97_TEXT_COMPLETE && reused.wait.invocations == 1);
  const auto prior_operations = reused.wait.progress.operations;
  reused.wait.access_journal = nullptr;
  reused.wait.access_journal_capacity = 1;
  CHECK(reused.run() == NBA97_TEXT_IO_REFUSED && reused.wait.invocations == 1 &&
        reused.wait.completions == 1 && reused.wait.result == NBA97_TEXT_ARGUMENT &&
        reused.wait.progress.operations == prior_operations);
}

void adapterGuardsAndCapture() {
  for (unsigned field = 0; field < 9; ++field) {
    Integration f;
    Nba97FrontendExitCleanupEvent event{0x8002f08cu,
                                         0x8002f090u,
                                         0x8002efbcu,
                                         1,
                                         1,
                                         NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F08C,
                                         0,
                                         NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
    auto machine = f.cleanup.machine;
    machine.registers.gpr[31] = {0x8002f094u, 15};
    if (field == 0) event.pc ^= 4;
    else if (field == 1) event.delay_slot_pc ^= 4;
    else if (field == 2) event.entry ^= 4;
    else if (field == 3) event.invocation = 2;
    else if (field == 4) event.argument_count = 1;
    else if (field == 5) event.site = NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F094;
    else if (field == 6) event.target_program = NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD;
    else if (field == 7) machine.registers.gpr[31].word ^= 4;
    else machine.registers.gpr[31].known_mask = 14;
    const auto before = machine;
    Nba97GameTextMemory memory{&f.region, 1};
    CHECK(nba97_frontend_exit_wait_from_frontend_exit_cleanup(
              &f.wait, &memory, &event, &machine) == 0 &&
          f.wait.invocations == 0 && f.wait.result == NBA97_TEXT_ARGUMENT);
    for (unsigned reg = 0; reg < 32; ++reg)
      CHECK(machine.registers.gpr[reg].word == before.registers.gpr[reg].word &&
            machine.registers.gpr[reg].known_mask ==
                before.registers.gpr[reg].known_mask);
  }

  const std::string receipt = nba97::captureFrontendExitWait();
  CHECK(!receipt.empty() && receipt.front() == '{' && receipt.back() == '}');
  for (unsigned char byte : receipt) CHECK(std::isprint(byte));
  CHECK(receipt.find("\"contract_failure\":0") != std::string::npos &&
        receipt.find("\"operations\":19,\"accesses\":9") !=
            std::string::npos &&
        receipt.find("\"callbacks\":10,\"instruction_count\":50") !=
            std::string::npos &&
        receipt.find("\"next_wait_child\":\"full-machine binding 0x8002EFDC -> 0x8007B2BC") !=
            std::string::npos);
}
} // namespace

int main() {
  try {
    naturalCleanupComposition();
    relocationRefusalAndReuse();
    adapterGuardsAndCapture();
    std::printf("frontend_exit_wait_integration_tests: PASS (%u checks)\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
