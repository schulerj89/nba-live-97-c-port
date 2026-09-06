#include "frontend_dispatch_adapter.h"
#include "user_setup_session.hpp"

#include <array>
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
    throw std::runtime_error("frontend-dispatch integration check failed at " +
                             std::to_string(line));
}
#define check(value) checkAt((value), __LINE__)

constexpr std::array<U, 43> Targets{
    {0x8003fa3c, 0x8003fb6c, 0x8003fc34, 0x8003fc78, 0x8003fca8, 0x8003fcf4,
     0x8003fd74, 0x8003fe58, 0x8003fe98, 0x8003ff10, 0x8004005c, 0x8004006c,
     0x800400f0, 0x80040120, 0x80040154, 0x80040184, 0x80040194, 0x800401c0,
     0x800401fc, 0x8004028c, 0x800402d8, 0x800402e8, 0x80040350, 0x80040360,
     0x80040370, 0x80040380, 0x80040390, 0x80040410, 0x80040474, 0x800405d8,
     0x80040558, 0x80040658, 0x800406bc, 0x800406fc, 0x8004070c, 0x8004071c,
     0x8004072c, 0x8004073c, 0x8004076c, 0x8004077c, 0x8004009c, 0x800400ac,
     0x800407d4}};

struct Fixture {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U Context = 0x80110000u;
  static constexpr U Allocation = 0x80140000u;
  static constexpr U Roster = 0x80160000u;
  static constexpr U Sp = 0x801f0000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendDispatchMachine machine{};
  Nba97FrontendDispatchBinding binding{};
  Nba97FrontendDispatchCallerEvent entry{0x800360f4u, 0x800360f8u, 0x8003f7c8u,
                                         1,           1,           0};
  std::array<Nba97FrontendDispatchAccess, 4096> journal{};
  std::vector<Nba97FrontendDispatchEvent> calls;
  nba97::UserSetupSession session;
  bool setupOpened = false;
  bool relocateSp = false;
  unsigned malformed = 0;
  U refusePc = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x22000000u + i * 0x101u, 15};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[29] = {Sp, 15};
    machine.registers.gpr[31] = {0x800360fcu, 15};
    machine.hi = {0x12345678u, 5};
    machine.lo = {0x9abcdef0u, 10};
    put(0x800170c0u, Context);
    put(Context + 0x14u, 0x80120000u);
    put(0x8009821cu, Allocation);
    put(0x800982e0u, Allocation);
    put(0x8009352cu, Allocation);
    put(0x80015098u, 0);
    put(0x80021d74u, 0);
    put(0x80021d78u, 1);
    put(0x8001edecu, 0, 2);
    put(Context + 0x720u, 5, 2);
    for (unsigned i = 0; i < Targets.size(); ++i)
      put(0x80024f80u + i * 4u, Targets[i]);
    for (unsigned team = 0; team < 2; ++team)
      for (unsigned slot = 0; slot < 12; ++slot) {
        const U source = Roster + (team + 1u) * 0x600u + slot * 0x80u;
        put(0x80023ab0u + team * 104u + slot * 4u, source);
        for (unsigned byte = 0; byte < 110; ++byte)
          put(source + byte, team * 17u + slot + byte, 1);
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

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97FrontendDispatchEvent *event,
                      Nba97FrontendDispatchMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    Nba97FrontendDispatchSiteContract contract{};
    if (!event || !machine ||
        !nba97_frontend_dispatch_site_contract(event->site, &contract) ||
        event->pc != contract.pc ||
        event->delay_slot_pc != contract.delay_slot_pc ||
        event->target != contract.target ||
        event->argument_count != contract.argument_count ||
        machine->registers.gpr[31].known_mask != 15 ||
        machine->registers.gpr[31].word != event->pc + 8u)
      return 0;
    f.calls.push_back(*event);
    if (f.refusePc == event->pc)
      return 0;
    if (f.relocateSp && f.calls.size() == 1) {
      const U oldSp = machine->registers.gpr[29].word;
      const U newSp = oldSp - 0x100u;
      for (unsigned i = 0; i < 0x88; ++i) {
        f.bytes[newSp - Base + i] = f.bytes[oldSp - Base + i];
        f.known[newSp - Base + i] = f.known[oldSp - Base + i];
      }
      machine->registers.gpr[29] = {newSp, 15};
    }
    if (event->target == 0x800770d4u)
      machine->registers.gpr[2] = {Allocation, 15};
    else if (event->target == 0x800459c8u)
      machine->registers.gpr[2] = {5, 15};
    else if (event->target == 0x80031a88u)
      f.put(Context + 0x720u, 5, 2);
    else if (event->target == 0x80037010u) {
      if (!f.setupOpened) {
        std::array<std::uint8_t, 8> assignments{{1, 2, 0, 0, 0, 0, 0, 0}};
        f.session.open(assignments, {}, 0);
        f.session.setControllers(0, 1);
        f.session.key(0, 0x80, true);
        const auto actions = f.session.step(100);
        check(!actions.empty() &&
              actions.back().event == NBA97_USER_CONFIRMED &&
              f.session.state().result == 6);
        f.setupOpened = true;
      }
      machine->registers.gpr[2] = {6, 15};
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
        f.known[target - Base + i] = f.known[source - Base + i];
      }
    } else
      machine->registers.gpr[2] = {UINT32_MAX, 15};
    machine->registers.gpr[13] = {0xabcdef01u, 15};
    if (f.malformed == 1)
      machine->registers.gpr[0].word = 1;
    else if (f.malformed == 2)
      machine->registers.gpr[10].known_mask = 16;
    else if (f.malformed == 3)
      machine->hi.known_mask = 16;
    else if (f.malformed == 4)
      machine->lo.known_mask = 16;
    return 1;
  }

  int run() {
    Nba97GameTextMemory memory{&region, 1};
    return nba97_frontend_dispatch_from_800360d4(&binding, &memory, &entry,
                                                 &machine);
  }
};

void naturalUserSetupAndReuse() {
  Fixture f;
  check(f.run() == 1 && f.binding.result == NBA97_TEXT_COMPLETE &&
        f.binding.invocations == 1 && f.binding.completions == 1 &&
        f.setupOpened && f.session.state().result == 6);
  check(f.session.state().assignment[0] == 1 &&
        f.session.state().assignment[1] == 2);
  check(f.binding.event.pc == 0x800360f4u &&
        f.binding.event.delay_slot_pc == 0x800360f8u &&
        f.machine.registers.gpr[31].word == 0x800360fcu);
  const auto firstCalls = f.calls.size();
  f.setupOpened = false;
  f.session = {};
  check(f.run() == 1 && f.binding.invocations == 2 &&
        f.binding.completions == 2 && f.calls.size() > firstCalls);

  Fixture moved;
  moved.relocateSp = true;
  check(moved.run() == 1 &&
        moved.machine.registers.gpr[29].word == Fixture::Sp - 0x100u &&
        moved.machine.registers.gpr[31].word == 0x800360fcu);
}

void entryGuardsAndNestedPrefixes() {
  for (unsigned field = 0; field < 7; ++field) {
    Fixture f;
    if (field == 0)
      f.entry.pc ^= 4;
    else if (field == 1)
      f.entry.delay_slot_pc ^= 4;
    else if (field == 2)
      f.entry.entry ^= 4;
    else if (field == 3)
      f.entry.invocation = 2;
    else if (field == 4)
      f.entry.argument_count = 1;
    else if (field == 5)
      f.machine.registers.gpr[31].word ^= 4;
    else
      f.machine.registers.gpr[31].known_mask = 14;
    const auto before = f.machine;
    check(f.run() == 0 && f.binding.result == NBA97_TEXT_ARGUMENT &&
          f.binding.invocations == 0);
    for (unsigned i = 0; i < 32; ++i)
      check(f.machine.registers.gpr[i].word == before.registers.gpr[i].word &&
            f.machine.registers.gpr[i].known_mask ==
                before.registers.gpr[i].known_mask);
  }
  for (unsigned malformed = 1; malformed <= 4; ++malformed) {
    Fixture f;
    f.malformed = malformed;
    check(f.run() == 0 && f.binding.result == NBA97_TEXT_ARGUMENT &&
          f.binding.invocations == 1 && f.binding.completions == 0 &&
          f.binding.progress.stopped_pc == 0x8003f8c8u &&
          f.machine.registers.gpr[13].word == 0xabcdef01u);
  }
  Fixture refused;
  refused.refusePc = 0x8003fcf4u;
  check(refused.run() == 0);
  check(refused.binding.result == NBA97_TEXT_IO_REFUSED);
  check(refused.binding.progress.stopped_pc == 0x8003fcf4u);
  check(refused.machine.registers.gpr[31].word == 0x8003fcfcu);
  Fixture limited;
  limited.binding.operation_budget = 1;
  check(limited.run() == 0 && limited.binding.result == NBA97_TEXT_LIMIT &&
        limited.binding.progress.operations == 1);
}
} // namespace

int main() {
  try {
    naturalUserSetupAndReuse();
    entryGuardsAndNestedPrefixes();
    std::printf("frontend_dispatch_integration_tests: PASS (%u checks)\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
