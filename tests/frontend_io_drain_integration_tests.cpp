#include "frontend_io_drain_adapter.h"
#include "frontend_io_drain_capture.h"
#include "frontend_exit_drain_adapter.h"

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
    throw std::runtime_error("frontend-io-drain integration failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr U StatusBase = 0x800ef840u;
constexpr U PointerBase = 0x800ef844u;
constexpr U AuxBase = 0x800ef830u;

struct Integration {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U Sp = 0x801f0000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendExitDrainContext drain{};
  Nba97FrontendExitDrainProgress drain_progress{};
  Nba97FrontendIoDrainBinding io{};
  std::array<Nba97FrontendExitDrainAccess, 16> drain_access{};
  std::array<U, 64> drain_instructions{};
  std::array<Nba97FrontendIoDrainAccess, 32> io_access{};
  std::array<U, 192> io_instructions{};
  std::vector<Nba97FrontendExitDrainEvent> drain_calls;
  std::vector<Nba97FrontendIoDrainEvent> io_calls;
  std::array<std::size_t, NBA97_FRONTEND_IO_DRAIN_SITE_COUNT> io_invocations{};
  U refuse_io_pc = 0;
  bool relocate_combined_frames = false;

  Integration() {
    for (unsigned i = 0; i < 32; ++i)
      drain.machine.registers.gpr[i] = {0x71000000u + i * 0x101u, 15};
    drain.machine.registers.gpr[0] = {0, 15};
    drain.machine.registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_SP] = {Sp, 15};
    drain.machine.registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_RA] = {
        0x8002f09cu, 15};
    drain.machine.hi = {0x11223344u, 5};
    drain.machine.lo = {0x55667788u, 10};
    put(0x800f84c4u, 1);
    put(0x800f43b0u, 0x13579bdfu);
    put(0x8002149cu, 0);
    const std::array<U, 8> statuses{{3, 1, 4, 5, U(-1), 0, 2, 6}};
    for (unsigned slot = 0; slot < statuses.size(); ++slot) {
      put(StatusBase + slot * 36u, statuses[slot]);
      put(PointerBase + slot * 36u, 0x81000000u + slot * 0x100u);
      put(AuxBase + slot * 36u, 0xa0000000u + slot);
    }
    drain.memory = {&region, 1};
    drain.operation_budget = 10;
    drain.io = drainIo;
    drain.user = this;
    drain.access_journal = drain_access.data();
    drain.access_journal_capacity = drain_access.size();
    drain.instruction_journal = drain_instructions.data();
    drain.instruction_journal_capacity = drain_instructions.size();
    io.operation_budget = 24;
    io.io = ioIo;
    io.user = this;
    io.access_journal = io_access.data();
    io.access_journal_capacity = io_access.size();
    io.instruction_journal = io_instructions.data();
    io.instruction_journal_capacity = io_instructions.size();
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

  static int drainIo(void *opaque, const Nba97GameTextMemory *memory,
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
        event->invocation != 1 ||
        machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_RA].word !=
            event->pc + 8u ||
        machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_RA].known_mask != 15)
      return 0;
    f.drain_calls.push_back(*event);
    if (event->site == NBA97_FRONTEND_EXIT_DRAIN_SITE_800394E8)
      return nba97_frontend_io_drain_from_frontend_exit_drain(
          &f.io, memory, event, machine);
    if (event->site == NBA97_FRONTEND_EXIT_DRAIN_SITE_800394F0)
      machine->registers.gpr[NBA97_FRONTEND_EXIT_DRAIN_V0] = {1, 15};
    return 1;
  }

  static int ioIo(void *opaque, const Nba97GameTextMemory *,
                  const Nba97FrontendIoDrainEvent *event,
                  Nba97FrontendIoDrainMachine *machine) {
    auto &f = *static_cast<Integration *>(opaque);
    Nba97FrontendIoDrainSiteContract contract{};
    if (!event || !machine ||
        !nba97_frontend_io_drain_site_contract(event->site, &contract) ||
        event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->entry != contract.target ||
        event->argument_count != contract.argument_count ||
        event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
        event->invocation != ++f.io_invocations[event->site] ||
        machine->registers.gpr[NBA97_FRONTEND_IO_DRAIN_RA].word !=
            event->pc + 8u ||
        machine->registers.gpr[NBA97_FRONTEND_IO_DRAIN_RA].known_mask != 15)
      return 0;
    f.io_calls.push_back(*event);
    if (event->site == NBA97_FRONTEND_IO_DRAIN_SITE_8003949C)
      machine->registers.gpr[NBA97_FRONTEND_IO_DRAIN_V0] = {
          event->invocation == 1 ? 0u : 1u, 15};
    if (f.relocate_combined_frames &&
        event->site == NBA97_FRONTEND_IO_DRAIN_SITE_80039458) {
      const U old_frame = machine->registers.gpr[29].word;
      const U new_frame = old_frame - 0x100u;
      if (old_frame < Base || new_frame < Base ||
          old_frame - Base > f.bytes.size() - 56u ||
          new_frame - Base > f.bytes.size() - 56u)
        return 0;
      for (U i = 0; i < 56; ++i) {
        f.bytes[new_frame - Base + i] = f.bytes[old_frame - Base + i];
        if (f.region.known)
          f.known[new_frame - Base + i] = f.known[old_frame - Base + i];
      }
      machine->registers.gpr[29] = {new_frame, 15};
    }
    return event->pc != f.refuse_io_pc;
  }

  int run() { return nba97_frontend_exit_drain(&drain, &drain_progress); }
};

void naturalDrainComposition() {
  Integration f;
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.drain_progress.completed &&
        f.io.invocations == 1 && f.io.completions == 1 &&
        f.io.result == NBA97_TEXT_COMPLETE && f.io.progress.completed);
  CHECK(f.drain_calls.size() == 3 &&
        f.drain_calls[0].pc == 0x800394e8u &&
        f.drain_calls[1].pc == 0x800394f0u &&
        f.drain_calls[2].pc == 0x80039538u &&
        f.io.event.pc == 0x800394e8u &&
        f.io.event.delay_slot_pc == 0x800394ecu &&
        f.io.event.entry == 0x800393f0u && f.io.event.argument_count == 0);
  CHECK(f.io.progress.operations == 24 && f.io.progress.accesses == 20 &&
        f.io.progress.callbacks_completed == 4 &&
        f.io.progress.instruction_count == 164 && f.io_calls.size() == 4 &&
        f.get(StatusBase) == 0 && f.get(PointerBase) == 0 &&
        f.get(StatusBase + 36u) == 0 && f.get(AuxBase + 72u) == 0 &&
        f.get(AuxBase + 108u) == 0);
  CHECK(f.drain_progress.operations == 10 &&
        f.drain_progress.callbacks_completed == 3 &&
        f.drain_progress.machine.registers.gpr[29].word == Integration::Sp &&
        f.drain_progress.machine.registers.gpr[31].word == 0x8002f09cu);
}

void nestedRefusalAndReuse() {
  Integration refused;
  refused.refuse_io_pc = 0x800394acu;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.drain_progress.stopped_pc == 0x800394e8u &&
        refused.io.result == NBA97_TEXT_IO_REFUSED &&
        refused.io.progress.stopped_pc == 0x800394acu &&
        refused.io.progress.callbacks_completed == 2 &&
        refused.drain_progress.callbacks_completed == 0);

  Integration reused;
  CHECK(reused.run() == NBA97_TEXT_COMPLETE && reused.io.invocations == 1);
  reused.drain_calls.clear();
  reused.io_calls.clear();
  reused.io_invocations.fill(0);
  reused.drain.machine.registers.gpr[29] = {Integration::Sp, 15};
  reused.drain.machine.registers.gpr[31] = {0x8002f09cu, 15};
  reused.put(0x800f84c4u, 1);
  reused.put(0x800f43b0u, 1);
  const std::array<U, 8> statuses{{3, 1, 4, 5, U(-1), 0, 2, 6}};
  for (unsigned slot = 0; slot < statuses.size(); ++slot) {
    reused.put(StatusBase + slot * 36u, statuses[slot]);
    reused.put(PointerBase + slot * 36u, 0x81000000u + slot * 0x100u);
    reused.put(AuxBase + slot * 36u, 0xa0000000u + slot);
  }
  CHECK(reused.run() == NBA97_TEXT_COMPLETE && reused.io.invocations == 2 &&
        reused.io.completions == 2 && reused.io.progress.completed);
}

void adapterGuards() {
  for (unsigned site = 1; site < NBA97_FRONTEND_IO_DRAIN_SITE_COUNT; ++site) {
    Nba97FrontendIoDrainSiteContract contract{};
    CHECK(nba97_frontend_io_drain_site_contract(
              static_cast<std::uint8_t>(site), &contract) == 1 &&
          contract.pc == (site == 1 ? 0x80039458u
                                    : site == 2 ? 0x8003949cu : 0x800394acu) &&
          contract.delay_slot_pc == contract.pc + 4u &&
          contract.argument_count == (site == 1 ? 1 : 0) &&
          contract.target_program == NBA97_FRONTEND_MAIN_PROGRAM_FEONLY);
  }
  Nba97FrontendIoDrainSiteContract ignored{};
  CHECK(nba97_frontend_io_drain_site_contract(0, &ignored) == 0 &&
        nba97_frontend_io_drain_site_contract(
            NBA97_FRONTEND_IO_DRAIN_SITE_COUNT, &ignored) == 0 &&
        nba97_frontend_io_drain_site_contract(1, nullptr) == 0);

  for (unsigned field = 0; field < 9; ++field) {
    Integration f;
    Nba97FrontendExitDrainEvent event{
        0x800394e8u, 0x800394ecu, 0x800393f0u, 3, 1,
        NBA97_FRONTEND_EXIT_DRAIN_SITE_800394E8, 0,
        NBA97_FRONTEND_MAIN_PROGRAM_FEONLY};
    auto machine = f.drain.machine;
    machine.registers.gpr[31] = {0x800394f0u, 15};
    if (field == 0) event.pc ^= 4;
    else if (field == 1) event.delay_slot_pc ^= 4;
    else if (field == 2) event.entry ^= 4;
    else if (field == 3) event.invocation = 2;
    else if (field == 4) event.argument_count = 1;
    else if (field == 5) event.site = NBA97_FRONTEND_EXIT_DRAIN_SITE_800394F0;
    else if (field == 6)
      event.target_program = NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD;
    else if (field == 7) machine.registers.gpr[31].word ^= 4;
    else machine.registers.gpr[31].known_mask = 14;
    const auto before = machine;
    Nba97GameTextMemory memory{&f.region, 1};
    CHECK(nba97_frontend_io_drain_from_frontend_exit_drain(
              &f.io, &memory, &event, &machine) == 0 &&
          f.io.invocations == 0 && f.io.result == NBA97_TEXT_ARGUMENT);
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
    CHECK(f.run() == NBA97_TEXT_COMPLETE && f.drain_progress.completed &&
          f.io.progress.completed && f.io_calls.size() == 4 &&
          f.io.progress.restored_return_address.word == 0x800394f0u &&
          f.drain_progress.restored_return_address.word == 0x8002f09cu &&
          f.drain_progress.machine.registers.gpr[29].word ==
              Integration::Sp - 0x100u &&
          f.drain_progress.machine.registers.gpr[31].word == 0x8002f09cu);
  }
}

void captureSmoke() {
  const std::string receipt = nba97::captureFrontendIoDrain();
  CHECK(!receipt.empty() && receipt.front() == '{' && receipt.back() == '}');
  for (unsigned char byte : receipt) CHECK(std::isprint(byte));
  CHECK(receipt.find("\"contract_failure\":0") != std::string::npos &&
        receipt.find("\"operations\":24,\"accesses\":20") !=
            std::string::npos &&
        receipt.find("\"callbacks\":4,\"slot_iterations\":8") !=
            std::string::npos &&
        receipt.find("\"status_fixture\":[3,1,4,5,-1,0,2,6]") !=
            std::string::npos &&
        receipt.find("\"gameplay_shown\":\"BLOCKED\"") !=
            std::string::npos);
}
} // namespace

int main() {
  try {
    naturalDrainComposition();
    nestedRefusalAndReuse();
    adapterGuards();
    relocatedCombinedFrames();
    captureSmoke();
    std::printf("frontend_io_drain_integration_tests: PASS (%u checks)\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
