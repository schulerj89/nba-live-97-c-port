#include "frontend_clock_read_adapter.h"
#include "frontend_clock_read_capture.h"

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
    throw std::runtime_error("frontend-clock-read integration failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

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

struct Fixture {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U Sp = 0x801f0000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendExitWaitContext wait{};
  Nba97FrontendExitWaitProgress wait_progress{};
  Nba97FrontendClockReadBinding clock{};
  Nba97FrontendClockReadAdapterProgress adapter{};
  std::array<Nba97FrontendExitWaitAccess, 16> wait_access{};
  std::array<U, 128> wait_instructions{};
  std::array<Nba97FrontendClockReadAccess, 2> clock_access{};
  std::array<U, 8> clock_instructions{};
  std::array<unsigned, NBA97_FRONTEND_EXIT_WAIT_SITE_COUNT> site_calls{};
  std::vector<Nba97FrontendExitWaitEvent> fixture_calls;
  U next_clock = 1361;
  bool repeat_equal = false;
  bool fail_second_clock = false;
  U refuse_pc = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      wait.machine.registers.gpr[i] = {
          0x62000000u + i * 0x101u, std::uint8_t((i % 15u) + 1u)};
    wait.machine.registers.gpr[0] = {0, 15};
    wait.machine.registers.gpr[16] = {0x12345678u, 5};
    wait.machine.registers.gpr[29] = {Sp, 15};
    wait.machine.registers.gpr[31] = {0x8002f094u, 15};
    wait.machine.hi = {0x0badc0deu, 6};
    wait.machine.lo = {0xc001d00du, 9};
    put(0x80017268u, 0x80145678u);
    put(0x8002149cu, 0x80123458u);
    put(0x800d9ab8u, 1000u);
    wait.memory = {&region, 1};
    wait.operation_budget = 19;
    wait.io = fixtureIo;
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
    if (!extent(address, width))
      throw std::runtime_error("integration fixture write outside RAM");
    for (U i = 0; i < width; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known) known[address - Base + i] = 1;
    }
  }
  U get(U address) const {
    if (!extent(address))
      throw std::runtime_error("integration fixture read outside RAM");
    U value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= U(bytes[address - Base + i]) << (i * 8u);
    return value;
  }

  static int fixtureIo(void *opaque, const Nba97GameTextMemory *,
                       const Nba97FrontendExitWaitEvent *event,
                       Nba97FrontendExitWaitMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (!event || !machine || event->site == 0 || event->site >= 11 ||
        event->pc != Pcs[event->site - 1] ||
        event->delay_slot_pc != Delays[event->site - 1] ||
        event->entry != Targets[event->site - 1] ||
        event->argument_count != Args[event->site - 1] ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        machine->registers.gpr[31].word != event->pc + 8u ||
        machine->registers.gpr[31].known_mask != 15)
      return 0;
    f.fixture_calls.push_back(*event);
    const unsigned invocation = f.site_calls[event->site]++;
    if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFF0 ||
        event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002F000)
      machine->registers.gpr[2] = {0, 15};
    if (event->site == NBA97_FRONTEND_EXIT_WAIT_SITE_8002F010) {
      U value = f.next_clock;
      if (f.repeat_equal) value += invocation;
      f.put(0x800d9ab8u, value);
      if (f.fail_second_clock && invocation == 1)
        f.known[0x800d9ab8u - Base] = 2;
    }
    return event->pc != f.refuse_pc;
  }

  int run() {
    return nba97_frontend_exit_wait_with_recovered_clock(
        &wait, &clock, &wait_progress, &adapter);
  }
};

void naturalWaitComposition() {
  Fixture f;
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.wait_progress.completed &&
        f.wait_progress.exit_path == NBA97_FRONTEND_EXIT_WAIT_EXIT_DEADLINE &&
        f.wait_progress.operations == 19 && f.wait_progress.accesses == 9 &&
        f.wait_progress.callbacks_completed == 10 &&
        f.wait_progress.instruction_count == 50 &&
        f.wait_progress.deadline.word == 1360 &&
        f.wait_progress.clock_result.word == 1361);
  CHECK(f.clock.invocations == 2 && f.clock.completions == 2 &&
        f.adapter.invocations == 2 && f.adapter.completions == 2 &&
        f.adapter.initial_invocations == 1 &&
        f.adapter.loop_invocations == 1 && f.fixture_calls.size() == 8);
  CHECK(f.adapter.initial_event.pc == 0x8002efe4u &&
        f.adapter.initial_event.delay_slot_pc == 0x8002efe8u &&
        f.adapter.initial_parent_machine.registers.gpr[31].word ==
            0x8002efecu &&
        f.adapter.initial_progress.loaded_clock.word == 1000 &&
        f.adapter.initial_access.address == 0x800d9ab8u &&
        f.adapter.loop_event.pc == 0x8002f018u &&
        f.adapter.loop_parent_machine.registers.gpr[31].word == 0x8002f020u &&
        f.adapter.loop_progress.loaded_clock.word == 1361 &&
        f.adapter.loop_access.value == 1361);
  CHECK(f.get(0x80017268u) == UINT32_MAX && f.get(0x8002149cu) == 0 &&
        f.wait_progress.machine.registers.gpr[29].word == Fixture::Sp &&
        f.wait_progress.machine.registers.gpr[31].word == 0x8002f094u);

  Fixture no_plane;
  for (auto &reg : no_plane.wait.machine.registers.gpr)
    reg.known_mask = 15;
  no_plane.region.known = nullptr;
  CHECK(no_plane.run() == NBA97_TEXT_COMPLETE);
  CHECK(no_plane.adapter.initial_progress.loaded_clock.known_mask == 15);
  CHECK(no_plane.adapter.loop_progress.loaded_clock.known_mask == 15);
}

void repeatedLoopAndFailure() {
  Fixture repeated;
  repeated.put(0x800d9ab8u, 100);
  repeated.next_clock = 460;
  repeated.repeat_equal = true;
  repeated.wait.operation_budget = 25;
  CHECK(repeated.run() == NBA97_TEXT_COMPLETE &&
        repeated.wait_progress.loop_iterations == 2 &&
        repeated.adapter.initial_invocations == 1 &&
        repeated.adapter.loop_invocations == 2 &&
        repeated.clock.invocations == 3 && repeated.clock.completions == 3 &&
        repeated.adapter.loop_event.invocation == 2 &&
        repeated.adapter.loop_progress.loaded_clock.word == 461);

  Fixture failed;
  failed.clock.operation_budget = 0;
  CHECK(failed.run() == NBA97_TEXT_IO_REFUSED &&
        failed.wait_progress.stopped_pc == 0x8002efe4u &&
        failed.clock.result == NBA97_TEXT_LIMIT &&
        failed.clock.invocations == 1 && failed.clock.completions == 0 &&
        failed.adapter.initial_invocations == 1 &&
        failed.adapter.initial_result == NBA97_TEXT_LIMIT &&
        failed.adapter.initial_progress.instruction_count == 2 &&
        failed.adapter.initial_progress.machine.registers.gpr[2].word ==
            0x800e0000u);

  Fixture stale;
  stale.put(0x800d9ab8u, 100);
  stale.next_clock = 460;
  stale.repeat_equal = true;
  stale.fail_second_clock = true;
  stale.wait.operation_budget = 25;
  CHECK(stale.run() == NBA97_TEXT_IO_REFUSED &&
        stale.wait_progress.stopped_pc == 0x8002f018u &&
        stale.adapter.loop_invocations == 2 &&
        stale.adapter.loop_result == NBA97_TEXT_ARGUMENT &&
        stale.adapter.loop_progress.access_events == 0 &&
        stale.adapter.loop_access.pc == 0 && stale.adapter.loop_access.address == 0 &&
        stale.adapter.loop_access.value == 0);
}

void adapterContractsReuseAndCapture() {
  Fixture f;
  Nba97FrontendExitWaitEvent event{0x8002efe4u,
                                    0x8002efe8u,
                                    0x8008da5cu,
                                    2,
                                    1,
                                    NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFE4,
                                    0,
                                    NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
  auto machine = f.wait.machine;
  machine.registers.gpr[31] = {0x8002efecu, 15};
  Nba97GameTextMemory memory{&f.region, 1};
  CHECK(nba97_frontend_clock_read_from_frontend_exit_wait(
            &f.clock, &memory, &event, &machine) == 1 &&
        f.clock.invocations == 1 && f.clock.completions == 1 &&
        f.clock.parent_machine.registers.gpr[31].word == 0x8002efecu);
  const auto prior = f.clock.progress;
  f.clock.access_journal = nullptr;
  f.clock.access_journal_capacity = 1;
  const auto before = machine;
  CHECK(nba97_frontend_clock_read_from_frontend_exit_wait(
            &f.clock, &memory, &event, &machine) == 0 &&
        f.clock.invocations == 1 && f.clock.completions == 1 &&
        f.clock.result == NBA97_TEXT_ARGUMENT &&
        f.clock.progress.operations == prior.operations);
  for (unsigned reg = 0; reg < 32; ++reg)
    CHECK(machine.registers.gpr[reg].word == before.registers.gpr[reg].word &&
          machine.registers.gpr[reg].known_mask ==
              before.registers.gpr[reg].known_mask);

  for (unsigned field = 0; field < 9; ++field) {
    Fixture bad;
    auto bad_event = event;
    auto bad_machine = bad.wait.machine;
    bad_machine.registers.gpr[31] = {0x8002efecu, 15};
    if (field == 0) bad_event.pc ^= 4;
    else if (field == 1) bad_event.delay_slot_pc ^= 4;
    else if (field == 2) bad_event.entry ^= 4;
    else if (field == 3) bad_event.invocation = 2;
    else if (field == 4) bad_event.argument_count = 1;
    else if (field == 5) bad_event.site = NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFF0;
    else if (field == 6) bad_event.target_program = NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD;
    else if (field == 7) bad_machine.registers.gpr[31].word ^= 4;
    else bad_machine.registers.gpr[31].known_mask = 14;
    CHECK(nba97_frontend_clock_read_from_frontend_exit_wait(
              &bad.clock, &memory, &bad_event, &bad_machine) == 0 &&
          bad.clock.invocations == 0 && bad.clock.result == NBA97_TEXT_ARGUMENT);
  }

  Nba97FrontendClockReadParentContract contract{};
  CHECK(nba97_frontend_clock_read_parent_contract(
            NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFE4, &contract) == 1 &&
        contract.pc == 0x8002efe4u && contract.return_address == 0x8002efecu);
  CHECK(nba97_frontend_clock_read_parent_contract(
            NBA97_FRONTEND_EXIT_WAIT_SITE_8002F018, &contract) == 1 &&
        contract.pc == 0x8002f018u && contract.return_address == 0x8002f020u);

  const std::string receipt = nba97::captureFrontendClockRead();
  CHECK(!receipt.empty() && receipt.front() == '{' && receipt.back() == '}');
  for (unsigned char byte : receipt) CHECK(std::isprint(byte));
  CHECK(receipt.find("\"contract_failure\":0") != std::string::npos &&
        receipt.find("\"bytes\":16,\"instructions\":4") !=
            std::string::npos &&
        receipt.find("\"clock_callbacks\":2") != std::string::npos &&
        receipt.find("\"gameplay_shown\":\"BLOCKED\"") !=
            std::string::npos);
}
} // namespace

int main() {
  try {
    naturalWaitComposition();
    repeatedLoopAndFailure();
    adapterContractsReuseAndCapture();
    std::printf("frontend_clock_read_integration_tests: PASS (%u checks)\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
