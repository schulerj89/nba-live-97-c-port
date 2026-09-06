#include "frontend_io_complete_adapter.h"
#include "frontend_io_complete_capture.h"
#include "frontend_io_drain_adapter.h"

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
    throw std::runtime_error("frontend-io-complete integration failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

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

struct Fixture {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U Sp = 0x801f0000u;
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
  std::array<Nba97FrontendIoCompleteParentRecord, 4> parent_records{};
  std::array<unsigned, NBA97_FRONTEND_EXIT_DRAIN_SITE_COUNT> site_calls{};
  std::vector<Nba97FrontendExitDrainEvent> fixture_calls;
  bool fail_second = false;
  U refuse_pc = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      drain.machine.registers.gpr[i] = {0x72000000u + i * 0x101u, 15};
    drain.machine.registers.gpr[0] = {0, 15};
    drain.machine.registers.gpr[29] = {Sp, 15};
    drain.machine.registers.gpr[31] = {0x8002f09cu, 15};
    drain.machine.hi = {0x10203040u, 5};
    drain.machine.lo = {0x50607080u, 10};
    put(0x800f84c4u, 1);
    put(0x800f43b0u, 0x13579bdfu);
    put(0x8002149cu, 0x80145678u);
    for (unsigned i = 0; i < 8; ++i) put(0x800ef840u + i * 0x24u, 0);
    put(0x800ef840u, 0x80000000u);
    drain.memory = {&region, 1};
    drain.operation_budget = 15;
    drain.io = fixtureIo;
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
    poll.parent_journal = parent_records.data();
    poll.parent_journal_capacity = parent_records.size();
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
                       const Nba97FrontendExitDrainEvent *event,
                       Nba97FrontendExitDrainMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (!event || !machine || event->site == 0 || event->site >= 8 ||
        event->pc != Pcs[event->site - 1] ||
        event->delay_slot_pc != Delays[event->site - 1] ||
        event->entry != Targets[event->site - 1] ||
        event->argument_count != Args[event->site - 1] ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        event->invocation != ++f.site_calls[event->site] ||
        machine->registers.gpr[31].word != event->pc + 8u ||
        machine->registers.gpr[31].known_mask != 15)
      return 0;
    f.fixture_calls.push_back(*event);
    if (event->site == NBA97_FRONTEND_EXIT_DRAIN_SITE_80039500) {
      f.put(0x800ef840u, 0);
      if (f.fail_second) f.poll.operation_budget = 0;
    }
    return event->pc != f.refuse_pc;
  }

  int run() {
    return nba97_frontend_exit_drain_with_recovered_io_complete(
        &drain, &poll, &drain_progress, &adapter);
  }
};

struct CombinedDrain : Fixture {
  Nba97FrontendIoDrainBinding preparation{};
  Nba97FrontendIoCompleteBinding inner_poll{};
  std::array<Nba97FrontendIoCompleteParentRecord, 3> inner_records{};
  std::array<Nba97FrontendIoCompleteAccess, 9> inner_access{};
  std::array<U, 81> inner_instructions{};
  unsigned pumps = 0;
  bool fail_after_pump = false;

  CombinedDrain() {
    drain.io = outer;
    preparation.operation_budget = 64;
    preparation.io = inner;
    preparation.user = this;
    inner_poll.operation_budget = 9;
    inner_poll.parent_journal = inner_records.data();
    inner_poll.parent_journal_capacity = inner_records.size();
    inner_poll.access_journal = inner_access.data();
    inner_poll.access_journal_capacity = inner_access.size();
    inner_poll.instruction_journal = inner_instructions.data();
    inner_poll.instruction_journal_capacity = inner_instructions.size();
  }
  static int outer(void *opaque, const Nba97GameTextMemory *memory,
                   const Nba97FrontendExitDrainEvent *event,
                   Nba97FrontendExitDrainMachine *machine) {
    auto &f = *static_cast<CombinedDrain *>(opaque);
    if (!Fixture::fixtureIo(opaque, memory, event, machine)) return 0;
    if (event->site == NBA97_FRONTEND_EXIT_DRAIN_SITE_800394E8)
      return nba97_frontend_io_drain_from_frontend_exit_drain(
          &f.preparation, memory, event, machine);
    return 1;
  }
  static int inner(void *opaque, const Nba97GameTextMemory *memory,
                   const Nba97FrontendIoDrainEvent *event,
                   Nba97FrontendIoDrainMachine *machine) {
    auto &f = *static_cast<CombinedDrain *>(opaque);
    if (!event || !machine) return 0;
    if (event->site == NBA97_FRONTEND_IO_DRAIN_SITE_8003949C)
      return nba97_frontend_io_complete_from_frontend_io_drain(
          &f.inner_poll, memory, event, machine);
    // The only remaining child reached by this negative-status fixture is
    // the I/O pump. It preserves CPU state and clears just the occupied slot.
    if (event->site != NBA97_FRONTEND_IO_DRAIN_SITE_800394AC ||
        event->pc != 0x800394acu || event->delay_slot_pc != 0x800394b0u ||
        event->entry != 0x80038e84u || event->argument_count != 0 ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        event->invocation != 1 || machine->registers.gpr[31].word != 0x800394b4u ||
        machine->registers.gpr[31].known_mask != 15) return 0;
    ++f.pumps;
    f.put(0x800ef840u, 0);
    if (f.fail_after_pump) f.inner_poll.operation_budget = 0;
    return 1;
  }
};

void threeRecoveredOwners() {
  for (bool plane : {true, false}) {
    CombinedDrain f;
    if (!plane) f.region.known = nullptr;
    CHECK(f.run() == NBA97_TEXT_COMPLETE && f.drain_progress.completed);
    CHECK(f.preparation.invocations == 1 && f.preparation.completions == 1 &&
          f.preparation.progress.completed && f.preparation.progress.slot_iterations == 8);
    CHECK(f.inner_poll.invocations == 2 && f.inner_poll.completions == 2 && f.pumps == 1);
    CHECK(f.poll.invocations == 1 && f.poll.completions == 1 &&
          f.drain_progress.poll_attempts == 1 && f.drain_progress.zero_poll_results == 0);
    for (unsigned i = 0; i < 2; ++i) {
      const auto &r = f.inner_records[i];
      CHECK(r.event.pc == 0x8003949cu && r.event.delay_slot_pc == 0x800394a0u &&
            r.event.entry == 0x800392a0u && r.event.invocation == i + 1 &&
            r.parent_machine.registers.gpr[31].word == 0x800394a4u && r.completed &&
            r.progress.machine.registers.gpr[2].word == i &&
            r.progress.status_reads == (i ? 8u : 1u));
    }
    CHECK(f.parent_records[0].event.pc == 0x800394f0u &&
          f.parent_records[0].progress.status_reads == 8 &&
          f.parent_records[0].progress.machine.registers.gpr[2].word == 1);
    CHECK(f.drain_progress.machine.registers.gpr[29].word == Fixture::Sp &&
          f.drain_progress.machine.registers.gpr[31].word == 0x8002f09cu &&
          f.drain_progress.machine.registers.gpr[16].word == 0x72001010u &&
          f.drain_progress.machine.registers.gpr[17].word == 0x72001111u &&
          f.get(0x800f84c4u) == 0 && f.get(0x800f43b0u) == 0 && f.get(0x800ef840u) == 0);
  }
  CombinedDrain failure;
  failure.fail_after_pump = true;
  CHECK(failure.run() == NBA97_TEXT_IO_REFUSED &&
        failure.drain_progress.stopped_pc == 0x800394e8u &&
        failure.preparation.progress.stopped_pc == 0x8003949cu &&
        failure.inner_poll.progress.stopped_pc == 0x800392a4u &&
        failure.inner_poll.result == NBA97_TEXT_LIMIT &&
        failure.inner_poll.invocations == 2 && failure.inner_poll.completions == 1 &&
        failure.inner_records[1].access_events == 0 &&
        failure.inner_records[1].first_access.pc == 0);
  for (unsigned field = 0; field < 9; ++field) {
    CombinedDrain f;
    Nba97FrontendIoDrainEvent e{0x8003949cu,0x800394a0u,0x800392a0u,1,1,
        NBA97_FRONTEND_IO_DRAIN_SITE_8003949C,0,NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
    auto m = f.drain.machine;
    m.registers.gpr[31] = {0x800394a4u,15};
    if (field == 0) e.pc ^= 4;
    if (field == 1) e.delay_slot_pc ^= 4;
    if (field == 2) e.entry ^= 4;
    if (field == 3) e.invocation = 0;
    if (field == 4) e.site = NBA97_FRONTEND_IO_DRAIN_SITE_800394AC;
    if (field == 5) e.argument_count = 1;
    if (field == 6) e.target_program = NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD;
    if (field == 7) m.registers.gpr[31].word ^= 4;
    if (field == 8) m.registers.gpr[31].known_mask = 14;
    CHECK(nba97_frontend_io_complete_from_frontend_io_drain(
        &f.inner_poll,&f.drain.memory,&e,&m) == 0 &&
        f.inner_poll.result == NBA97_TEXT_ARGUMENT && f.inner_poll.invocations == 0);
  }
}

void naturalDrainComposition() {
  Fixture f;
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.drain_progress.completed &&
        f.drain_progress.operations == 15 && f.drain_progress.accesses == 7 &&
        f.drain_progress.callbacks_completed == 8 &&
        f.drain_progress.poll_attempts == 2 &&
        f.drain_progress.zero_poll_results == 1 &&
        f.drain_progress.instruction_count == 44);
  CHECK(f.poll.invocations == 2 && f.poll.completions == 2 &&
        f.poll.parent_events == 2 && f.adapter.invocations == 2 &&
        f.adapter.completions == 2 && f.fixture_calls.size() == 6);
  CHECK(f.parent_records[0].event.pc == 0x800394f0u &&
        f.parent_records[0].event.invocation == 1 &&
        f.parent_records[0].parent_machine.registers.gpr[31].word ==
            0x800394f8u &&
        f.parent_records[0].progress.operations == 2 &&
        f.parent_records[0].progress.status_reads == 1 &&
        f.parent_records[0].access_events == 2 &&
        f.parent_records[0].instruction_events == 16 &&
        f.parent_records[0].access_journal[0].address == 0x800f84c4u &&
        f.parent_records[0].access_journal[1].address == 0x800ef840u &&
        f.parent_records[0].progress.machine.registers.gpr[2].word == 0 &&
        f.parent_records[1].event.invocation == 2 &&
        f.parent_records[1].progress.operations == 9 &&
        f.parent_records[1].progress.status_reads == 8 &&
        f.parent_records[1].access_events == 9 &&
        f.parent_records[1].instruction_events == 81 &&
        f.parent_records[1].instruction_journal[0] == 0x800392a0u &&
        f.parent_records[1].instruction_journal[80] == 0x800392f4u &&
        f.parent_records[1].access_journal[8].address == 0x800ef93cu &&
        f.parent_records[1].progress.machine.registers.gpr[2].word == 1);
  CHECK(f.adapter.first_progress.last_status.word == 0x80000000u &&
        f.adapter.latest_progress.last_status.word == 0 &&
        f.get(0x800ef840u) == 0 && f.get(0x800f84c4u) == 0 &&
        f.get(0x800f43b0u) == 0 &&
        f.drain_progress.machine.registers.gpr[29].word == Fixture::Sp &&
        f.drain_progress.machine.registers.gpr[31].word == 0x8002f09cu);

  Fixture no_plane;
  no_plane.region.known = nullptr;
  CHECK(no_plane.run() == NBA97_TEXT_COMPLETE &&
        no_plane.poll.invocations == 2 &&
        no_plane.parent_records[1].progress.last_status.known_mask == 15);
}

void nestedFailureAndGuards() {
  Fixture failed;
  failed.fail_second = true;
  CHECK(failed.run() == NBA97_TEXT_IO_REFUSED &&
        failed.drain_progress.stopped_pc == 0x800394f0u &&
        failed.poll.invocations == 2 && failed.poll.completions == 1 &&
        failed.adapter.invocations == 2 && failed.adapter.completions == 1 &&
        failed.adapter.latest_result == NBA97_TEXT_LIMIT &&
        failed.adapter.latest_progress.instruction_count == 2 &&
        failed.adapter.latest_progress.access_events == 0 &&
        failed.adapter.latest_access.pc == 0 &&
        failed.parent_records[1].first_access.pc == 0);

  Fixture f;
  Nba97FrontendExitDrainEvent event{0x800394f0u,
                                     0x800394f4u,
                                     0x800392a0u,
                                     2,
                                     1,
                                     NBA97_FRONTEND_EXIT_DRAIN_SITE_800394F0,
                                     0,
                                     NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
  auto machine = f.drain.machine;
  machine.registers.gpr[31] = {0x800394f8u, 15};
  Nba97GameTextMemory memory{&f.region, 1};
  for (unsigned field = 0; field < 9; ++field) {
    Fixture bad;
    auto bad_event = event;
    auto bad_machine = machine;
    if (field == 0) bad_event.pc ^= 4;
    else if (field == 1) bad_event.delay_slot_pc ^= 4;
    else if (field == 2) bad_event.entry ^= 4;
    else if (field == 3) bad_event.invocation = 0;
    else if (field == 4) bad_event.argument_count = 1;
    else if (field == 5) bad_event.site = NBA97_FRONTEND_EXIT_DRAIN_SITE_80039500;
    else if (field == 6) bad_event.target_program = NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD;
    else if (field == 7) bad_machine.registers.gpr[31].word ^= 4;
    else bad_machine.registers.gpr[31].known_mask = 14;
    const auto before = bad_machine;
    CHECK(nba97_frontend_io_complete_from_frontend_exit_drain(
              &bad.poll, &memory, &bad_event, &bad_machine) == 0 &&
          bad.poll.invocations == 0 && bad.poll.parent_events == 0 &&
          bad.poll.result == NBA97_TEXT_ARGUMENT);
    for (unsigned reg = 0; reg < 32; ++reg)
      CHECK(bad_machine.registers.gpr[reg].word ==
                before.registers.gpr[reg].word &&
            bad_machine.registers.gpr[reg].known_mask ==
                before.registers.gpr[reg].known_mask);
  }

  CHECK(nba97_frontend_io_complete_from_frontend_exit_drain(
            &f.poll, &memory, &event, &machine) == 1 &&
        f.poll.invocations == 1);
  const auto prior = f.poll.progress;
  f.poll.parent_journal = nullptr;
  f.poll.parent_journal_capacity = 1;
  CHECK(nba97_frontend_io_complete_from_frontend_exit_drain(
            &f.poll, &memory, &event, &machine) == 0 &&
        f.poll.invocations == 1 && f.poll.parent_events == 1 &&
        f.poll.result == NBA97_TEXT_ARGUMENT &&
        f.poll.progress.operations == prior.operations);

  Nba97FrontendIoCompleteParentContract contract{};
  CHECK(nba97_frontend_io_complete_parent_contract(&contract) == 1 &&
        contract.pc == 0x800394f0u && contract.delay_slot_pc == 0x800394f4u &&
        contract.target == 0x800392a0u &&
        contract.return_address == 0x800394f8u &&
        contract.argument_count == 0);
}

void captureSmoke() {
  const std::string receipt = nba97::captureFrontendIoComplete();
  CHECK(!receipt.empty() && receipt.front() == '{' && receipt.back() == '}');
  for (unsigned char byte : receipt) CHECK(std::isprint(byte));
  CHECK(receipt.find("\"contract_failure\":0") != std::string::npos &&
        receipt.find("\"bytes\":88,\"instructions\":22") !=
            std::string::npos &&
        receipt.find("\"poll_invocations\":2") != std::string::npos &&
        receipt.find("\"gameplay_shown\":\"BLOCKED\"") !=
            std::string::npos);
}
} // namespace

int main() {
  try {
    threeRecoveredOwners();
    naturalDrainComposition();
    nestedFailureAndGuards();
    captureSmoke();
    std::printf("frontend_io_complete_integration_tests: PASS (%u checks)\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
