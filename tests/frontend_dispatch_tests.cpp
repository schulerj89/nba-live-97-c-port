#include "frontend_dispatch_adapter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("frontend-dispatch check failed at " +
                             std::to_string(line));
}
#define check(value) checkAt((value), __LINE__)

constexpr std::array<U, 43> StateTargets{
    {0x8003fa3c, 0x8003fb6c, 0x8003fc34, 0x8003fc78, 0x8003fca8, 0x8003fcf4,
     0x8003fd74, 0x8003fe58, 0x8003fe98, 0x8003ff10, 0x8004005c, 0x8004006c,
     0x800400f0, 0x80040120, 0x80040154, 0x80040184, 0x80040194, 0x800401c0,
     0x800401fc, 0x8004028c, 0x800402d8, 0x800402e8, 0x80040350, 0x80040360,
     0x80040370, 0x80040380, 0x80040390, 0x80040410, 0x80040474, 0x800405d8,
     0x80040558, 0x80040658, 0x800406bc, 0x800406fc, 0x8004070c, 0x8004071c,
     0x8004072c, 0x8004073c, 0x8004076c, 0x8004077c, 0x8004009c, 0x800400ac,
     0x800407d4}};

struct Call {
  Nba97FrontendDispatchEvent event{};
  Nba97FrontendDispatchMachine machine{};
};

struct Fixture {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U Context = 0x80110000u;
  static constexpr U Side = 0x80120000u;
  static constexpr U Allocation = 0x80140000u;
  static constexpr U Roster = 0x80160000u;
  static constexpr U Sp = 0x801f0000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendDispatchMachine entry{};
  Nba97FrontendDispatchProgress progress{};
  std::vector<Nba97FrontendDispatchAccess> journal =
      std::vector<Nba97FrontendDispatchAccess>(4096);
  std::vector<Call> calls;
  std::size_t budget = 20000;
  unsigned selected = 5;
  unsigned queries = 0;
  U stateResult = UINT32_MAX;
  U userResult = 6;
  U refusePc = 0;
  unsigned malformed = 0;
  unsigned userCalls = 0;
  bool naturalPath = false;
  bool cancelOnce = false;
  std::function<void(Fixture &, const Nba97FrontendDispatchEvent &,
                     Nba97FrontendDispatchMachine &)>
      hook;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      entry.registers.gpr[i] = {0x11000000u + i * 0x101u, 15};
    entry.registers.gpr[0] = {0, 15};
    entry.registers.gpr[29] = {Sp, 15};
    entry.registers.gpr[31] = {0x800360fcu, 15};
    entry.hi = {0x12345678u, 5};
    entry.lo = {0x89abcdefu, 10};
    put(0x800170c0u, Context);
    put(Context + 0x14u, Side);
    put(0x8009821cu, Allocation);
    put(0x800982e0u, Allocation);
    put(0x8009352cu, Allocation);
    put(0x80015098u, 0);
    put(0x80021d74u, 0);
    put(0x80021d78u, 1);
    put(0x8001edecu, 0, 2);
    put(Context + 0x720u, selected, 2);
    put(Context + 0x722u, 0, 2);
    for (unsigned i = 0; i < StateTargets.size(); ++i)
      put(0x80024f80u + i * 4u, StateTargets[i]);
    for (int team = -1; team < 32; ++team)
      for (unsigned slot = 0; slot < 12; ++slot) {
        const U source = Roster + U(team + 1) * 0x600u + slot * 0x80u;
        put(U(0x80023ab0 + team * 104 + int(slot * 4)), source);
        for (unsigned byte = 0; byte < 110; ++byte)
          put(source + byte, U(team * 7 + int(slot * 11 + byte)), 1);
      }
  }

  void put(U address, U value, unsigned width = 4, int mask = -1) {
    const std::size_t offset = address - Base;
    for (unsigned byte = 0; byte < width; ++byte) {
      bytes[offset + byte] = std::uint8_t(value >> (byte * 8u));
      if (mask >= 0)
        known[offset + byte] = std::uint8_t((mask >> byte) & 1);
    }
  }

  U get(U address, unsigned width = 4) const {
    U value = 0;
    for (unsigned byte = 0; byte < width; ++byte)
      value |= U(bytes[address - Base + byte]) << (byte * 8u);
    return value;
  }

  static int callback(void *opaque, const Nba97GameTextMemory *memory,
                      const Nba97FrontendDispatchEvent *event,
                      Nba97FrontendDispatchMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    check(memory && event && machine);
    check(event->site > NBA97_FRONTEND_DISPATCH_SITE_NONE &&
          event->site < NBA97_FRONTEND_DISPATCH_SITE_COUNT);
    Nba97FrontendDispatchSiteContract contract{};
    check(nba97_frontend_dispatch_site_contract(event->site, &contract) == 1 &&
          event->pc == contract.pc &&
          event->delay_slot_pc == contract.delay_slot_pc &&
          event->target == contract.target &&
          event->argument_count == contract.argument_count);
    check(machine->registers.gpr[31].known_mask == 15 &&
          machine->registers.gpr[31].word == event->pc + 8u);
    f.calls.push_back({*event, *machine});
    if (f.refusePc == event->pc)
      return 0;
    machine->registers.gpr[24].word += event->pc;
    machine->hi.word ^= event->pc;
    machine->lo.word += 7;
    if (event->target == 0x800770d4u)
      machine->registers.gpr[2] = {Allocation, 15};
    else if (event->target == 0x800459c8u)
      machine->registers.gpr[2] = {f.selected, 15};
    else if (event->target == 0x80031a88u) {
      ++f.queries;
      if (!f.naturalPath)
        f.put(Context + 0x720u, f.queries == 1 ? f.selected : 5, 2);
    } else if (event->target == 0x80037010u) {
      ++f.userCalls;
      machine->registers.gpr[2] = {
          f.cancelOnce && f.userCalls == 1 ? UINT32_MAX : f.userResult, 15};
    } else if (event->target == 0x800909a8u) {
      const U source = machine->registers.gpr[4].word;
      const U target = machine->registers.gpr[5].word;
      const U count = machine->registers.gpr[6].word;
      check(count == 0x6eu);
      for (U i = 0; i < count; ++i) {
        f.bytes[target - Base + i] = f.bytes[source - Base + i];
        if (f.region.known)
          f.known[target - Base + i] = f.known[source - Base + i];
      }
    } else
      machine->registers.gpr[2] = {f.stateResult, 15};
    if (f.hook)
      f.hook(f, *event, *machine);
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
    Nba97FrontendDispatchContext context{};
    context.memory = {&region, 1};
    context.operation_budget = budget;
    context.machine = entry;
    context.io = callback;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    return nba97_frontend_dispatch(&context, &progress);
  }
};

void normalAndAllStates() {
  Fixture normal;
  check(normal.run() == NBA97_TEXT_COMPLETE);
  check(normal.progress.completed == 1 && normal.progress.stopped_pc == 0 &&
        normal.progress.stopped_address == 0 &&
        normal.progress.stopped_target == 0);
  check(normal.progress.roster_iterations == 12 && normal.queries >= 1);
  check(normal.progress.machine.registers.gpr[29].word == Fixture::Sp);
  check(normal.progress.machine.registers.gpr[31].word == 0x800360fcu);
  check(normal.get(0x8002208cu, 1) == normal.get(Fixture::Roster + 0x600u, 1));
  check(normal.get(0x800225b4u, 1) == normal.get(Fixture::Roster + 0xc00u, 1));

  for (unsigned state = 0; state < StateTargets.size(); ++state) {
    Fixture f;
    f.selected = state;
    f.put(Fixture::Context + 0x720u, state, 2);
    const int result = f.run();
    check(result == NBA97_TEXT_COMPLETE);
    check(f.progress.dispatch_iterations >= 1 && f.progress.completed == 1);
  }
}

void branchesMemoryAndCallbacks() {
  Fixture unknownLaunch;
  unknownLaunch.put(0x80015098u, 2, 4, 7);
  check(unknownLaunch.run() == NBA97_TEXT_UNKNOWN &&
        unknownLaunch.progress.stopped_pc == 0x8003f944u &&
        unknownLaunch.get(Fixture::Sp - 0x88u + 0x50u, 2) == 0xffffu);

  Fixture badTarget;
  badTarget.put(0x80024f80u + 5u * 4u, 0x8003fcf5u);
  check(badTarget.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        badTarget.progress.stopped_pc == 0x8003fa34u);
  Fixture unknownTarget;
  unknownTarget.put(0x80024f80u + 5u * 4u, 0x8003fcf4u, 4, 14);
  check(unknownTarget.run() == NBA97_TEXT_UNKNOWN &&
        unknownTarget.progress.stopped_pc == 0x8003fa34u);
  Fixture unsupported;
  unsupported.put(0x80024f80u + 5u * 4u, 0x80032000u);
  check(unsupported.run() == NBA97_TEXT_IO_REFUSED &&
        unsupported.progress.stopped_target == 0x80032000u);

  for (unsigned malformed = 1; malformed <= 4; ++malformed) {
    Fixture f;
    f.malformed = malformed;
    check(f.run() == NBA97_TEXT_ARGUMENT &&
          f.progress.callbacks_completed == 0);
  }
  Fixture refused;
  refused.refusePc = 0x8003fcf4u;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.progress.stopped_pc == 0x8003fcf4u &&
        refused.progress.stopped_target == 0x80037010u);

  Fixture lateByte;
  lateByte.known[0x80021d78u - Fixture::Base + 3u] = 2;
  check(lateByte.run() == NBA97_TEXT_ARGUMENT &&
        lateByte.progress.stopped_pc == 0x8003f814u &&
        lateByte.progress.machine.registers.gpr[2].word == 0x80020000u &&
        lateByte.progress.machine.registers.gpr[2].known_mask == 15);
  Fixture unknownByte;
  unknownByte.known[0x80021d78u - Fixture::Base + 1u] = 0;
  check(unknownByte.run() == NBA97_TEXT_UNKNOWN &&
        unknownByte.progress.stopped_pc == 0x8003f828u);
  Fixture nullKnown;
  nullKnown.region.known = nullptr;
  nullKnown.known.clear();
  check(nullKnown.run() == NBA97_TEXT_COMPLETE);
}

void budgetsValidationAndRepeatability() {
  Fixture reference;
  check(reference.run() == NBA97_TEXT_COMPLETE);
  const std::size_t operations = reference.progress.operations;
  for (std::size_t budget = 0; budget < operations; ++budget) {
    Fixture f;
    f.budget = budget;
    check(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget);
  }
  Fixture exact;
  exact.budget = operations;
  check(exact.run() == NBA97_TEXT_COMPLETE &&
        exact.progress.operations == operations);

  Nba97FrontendDispatchProgress progress{};
  check(nba97_frontend_dispatch(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
  Fixture invalidZero;
  invalidZero.entry.registers.gpr[0].word = 1;
  check(invalidZero.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalidMask;
  invalidMask.entry.hi.known_mask = 16;
  check(invalidMask.run() == NBA97_TEXT_ARGUMENT);
  Fixture huge;
  huge.region.size = SIZE_MAX;
  check(huge.run() == NBA97_TEXT_ARGUMENT);

  Fixture a;
  Fixture b;
  check(a.run() == NBA97_TEXT_COMPLETE && b.run() == NBA97_TEXT_COMPLETE);
  check(a.bytes == b.bytes && a.progress.operations == b.progress.operations &&
        a.progress.instruction_count == b.progress.instruction_count);
  for (unsigned i = 0; i < 32; ++i)
    check(a.progress.machine.registers.gpr[i].word ==
              b.progress.machine.registers.gpr[i].word &&
          a.progress.machine.registers.gpr[i].known_mask ==
              b.progress.machine.registers.gpr[i].known_mask);
  check(a.progress.machine.hi.word == b.progress.machine.hi.word &&
        a.progress.machine.hi.known_mask == b.progress.machine.hi.known_mask &&
        a.progress.machine.lo.word == b.progress.machine.lo.word &&
        a.progress.machine.lo.known_mask == b.progress.machine.lo.known_mask);
}

const Call &callAt(const Fixture &f, U pc) {
  const auto found =
      std::find_if(f.calls.begin(), f.calls.end(),
                   [pc](const Call &c) { return c.event.pc == pc; });
  check(found != f.calls.end());
  return *found;
}

void machineEquals(const Nba97FrontendDispatchMachine &a,
                   const Nba97FrontendDispatchMachine &b) {
  for (unsigned i = 0; i < 32; ++i)
    check(a.registers.gpr[i].word == b.registers.gpr[i].word &&
          a.registers.gpr[i].known_mask == b.registers.gpr[i].known_mask);
  check(a.hi.word == b.hi.word && a.hi.known_mask == b.hi.known_mask);
  check(a.lo.word == b.lo.word && a.lo.known_mask == b.lo.known_mask);
}

void acceptanceAndRegressions() {
  Fixture route;
  route.naturalPath = true;
  route.stateResult = 1;
  check(route.run() == NBA97_TEXT_COMPLETE);
  std::vector<U> states;
  for (const auto &c : route.calls)
    if (c.event.target == 0x80031a88u)
      states.push_back(c.machine.registers.gpr[4].word);
  check((states == std::vector<U>{0, 3, 5}));
  check(callAt(route, 0x8003fd3cu).machine.registers.gpr[4].word == 0);
  check(callAt(route, 0x8003fd44u).machine.registers.gpr[16].word ==
        UINT32_MAX);
  check(route.get(0x80021d70u, 1) == 0 && route.get(0x80015098u) == 0);
  check(route.get(0x800ef754u) == 0 && route.progress.roster_iterations == 12);
  unsigned home = 0, away = 0;
  for (const auto &c : route.calls) {
    if (c.event.pc == 0x80040900u) {
      check(c.machine.registers.gpr[5].word == 0x8002208cu + 110u * home++);
      check(c.machine.registers.gpr[6].word == 110);
    } else if (c.event.pc == 0x80040964u)
      check(c.machine.registers.gpr[5].word == 0x800225b4u + 110u * away++);
  }
  check(home == 12 && away == 12);
  const U frame = Fixture::Sp - 0x88u;
  const std::array<std::array<U, 4>, 16> prefix{
      {{0x8003f7ccu, frame + 0x70u, 4, NBA97_FRONTEND_DISPATCH_STORE},
       {0x8003f7d4u, 0x800170c0u, 4, NBA97_FRONTEND_DISPATCH_READ},
       {0x8003f7e0u, frame + 0x84u, 4, NBA97_FRONTEND_DISPATCH_STORE},
       {0x8003f7e4u, frame + 0x80u, 4, NBA97_FRONTEND_DISPATCH_STORE},
       {0x8003f7e8u, frame + 0x7cu, 4, NBA97_FRONTEND_DISPATCH_STORE},
       {0x8003f7ecu, frame + 0x78u, 4, NBA97_FRONTEND_DISPATCH_STORE},
       {0x8003f7f0u, frame + 0x74u, 4, NBA97_FRONTEND_DISPATCH_STORE},
       {0x8003f7f4u, frame + 0x6cu, 4, NBA97_FRONTEND_DISPATCH_STORE},
       {0x8003f7f8u, frame + 0x68u, 4, NBA97_FRONTEND_DISPATCH_STORE},
       {0x8003f7fcu, frame + 0x64u, 4, NBA97_FRONTEND_DISPATCH_STORE},
       {0x8003f800u, frame + 0x60u, 4, NBA97_FRONTEND_DISPATCH_STORE},
       {0x8003f804u, 0x80021d74u, 4, NBA97_FRONTEND_DISPATCH_READ},
       {0x8003f80cu, Fixture::Context + 0x70eu, 2,
        NBA97_FRONTEND_DISPATCH_STORE},
       {0x8003f814u, 0x80021d78u, 4, NBA97_FRONTEND_DISPATCH_READ},
       {0x8003f818u, frame + 0x58u, 2, NBA97_FRONTEND_DISPATCH_STORE},
       {0x8003f81cu, Fixture::Context + 0x710u, 2,
        NBA97_FRONTEND_DISPATCH_STORE}}};
  for (unsigned i = 0; i < prefix.size(); ++i) {
    const auto &actual = route.journal[i];
    check(actual.pc == prefix[i][0] && actual.address == prefix[i][1] &&
          actual.width == prefix[i][2] && actual.kind == prefix[i][3]);
    check(actual.operation == i + 1u);
  }
  for (unsigned i = 0; i < 12; ++i)
    for (unsigned j = 0; j < 110; ++j) {
      check(route.get(0x8002208cu + i * 110u + j, 1) ==
            route.get(Fixture::Roster + 0x600u + i * 0x80u + j, 1));
      check(route.get(0x800225b4u + i * 110u + j, 1) ==
            route.get(Fixture::Roster + 0xc00u + i * 0x80u + j, 1));
    }

  for (U launch : {0u, 1u, 2u, 3u, 4u, 5u, 6u, UINT32_MAX}) {
    Fixture f;
    f.put(0x80015098u, launch);
    f.put(0x8001ec94u, 0x12345678u);
    check(f.run() == NBA97_TEXT_COMPLETE);
    const U mode = launch >= 4u && launch <= 0x7fffffffu ? launch - 3u : launch;
    check(f.get(Fixture::Context + 0x78u, 2) == (mode & 0xffffu));
    check(f.get(0x80015098u) == (mode & 255u));
    check(f.get(0x80021d70u, 1) == (mode & 255u));
    check(f.get(0x8001ec94u) == ((mode & 65535u) == 1 ? 0x12345678u : 0u));
  }
  Fixture resultTen;
  resultTen.selected = 0;
  resultTen.stateResult = 10;
  check(resultTen.run() == NBA97_TEXT_COMPLETE);
  std::vector<U> routed;
  for (const auto &c : resultTen.calls)
    if (c.event.target == 0x80031a88u)
      routed.push_back(c.machine.registers.gpr[4].word);
  check((routed == std::vector<U>{0, 11}));
  Fixture cancel;
  cancel.cancelOnce = true;
  cancel.naturalPath = true;
  cancel.stateResult = 1;
  check(cancel.run() == NBA97_TEXT_COMPLETE && cancel.userCalls == 2);
  check(callAt(cancel, 0x8003fd10u).machine.registers.gpr[16].word == 1);
  Fixture retry;
  retry.hook = [](Fixture &f, const Nba97FrontendDispatchEvent &e,
                  Nba97FrontendDispatchMachine &m) {
    if (e.target == 0x80037010u && f.userCalls == 1)
      m.registers.gpr[2] = {0, 15};
  };
  check(retry.run() == NBA97_TEXT_COMPLETE && retry.userCalls == 2);
  check(retry.progress.call_count[NBA97_FRONTEND_DISPATCH_SITE_8003FD10] == 0);
  Fixture truncate;
  truncate.userResult = 0x76540006u;
  check(truncate.run() == NBA97_TEXT_COMPLETE);
  check(callAt(truncate, 0x8003fd3cu).machine.registers.gpr[19].word ==
        0x76540006u);

  Fixture rollback;
  rollback.selected = 1;
  for (unsigned i = 0; i < 14; ++i)
    rollback.put(0x80021da3u + i, i * 13u + 7u, 1);
  rollback.hook = [](Fixture &f, const Nba97FrontendDispatchEvent &e,
                     Nba97FrontendDispatchMachine &m) {
    if (e.pc == 0x8003fbd8u) {
      check(m.registers.gpr[4].word == 0x80098194u);
      check(m.registers.gpr[7].word == 0x8003eb1cu);
      check(f.get(m.registers.gpr[29].word + 0x10u) == 0x80044e64u);
      for (unsigned i = 0; i < 14; ++i)
        f.put(0x80021da3u + i, 0xa0u + i, 1);
    }
  };
  check(rollback.run() == NBA97_TEXT_COMPLETE);
  check(rollback.progress.backup_iterations == 14 &&
        rollback.progress.restore_iterations == 14);
  for (unsigned i = 0; i < 14; ++i)
    check(rollback.get(0x80021da3u + i, 1) == i * 13u + 7u);
  Fixture keepRules;
  keepRules.selected = 1;
  keepRules.stateResult = 0;
  keepRules.hook = rollback.hook;
  check(keepRules.run() == NBA97_TEXT_COMPLETE &&
        keepRules.progress.restore_iterations == 0);
  for (unsigned i = 0; i < 14; ++i)
    check(keepRules.get(0x80021da3u + i, 1) == 0xa0u + i);
}

void liveStateAndMemoryEdges() {
  Fixture relocated;
  relocated.hook = [](Fixture &f, const Nba97FrontendDispatchEvent &e,
                      Nba97FrontendDispatchMachine &m) {
    if (e.pc == 0x8003f8c8u) {
      const U old = m.registers.gpr[29].word;
      m.registers.gpr[29].word -= 0x100u;
      for (unsigned i = 0; i < 0x88; ++i) {
        f.bytes[old - 0x100u - Fixture::Base + i] =
            f.bytes[old - Fixture::Base + i];
        f.known[old - 0x100u - Fixture::Base + i] =
            f.known[old - Fixture::Base + i];
      }
      f.put(m.registers.gpr[29].word + 0x84u, 0x800360fcu);
      f.put(m.registers.gpr[29].word + 0x80u, 0xabcd9876u);
    }
  };
  check(relocated.run() == NBA97_TEXT_COMPLETE);
  check(relocated.progress.machine.registers.gpr[29].word ==
        Fixture::Sp - 0x100u);
  check(relocated.progress.machine.registers.gpr[30].word == 0xabcd9876u);
  check(callAt(relocated, 0x8003f8dcu).machine.registers.gpr[29].word ==
        Fixture::Sp - 0x188u);

  for (U team : {0u, 28u, 29u, 30u, UINT32_MAX}) {
    Fixture f;
    f.put(0x80021d74u, team);
    f.put(0x80021d78u, team);
    check(f.run() == NBA97_TEXT_COMPLETE);
    const bool special = team == 29 || team == 30;
    check(f.progress.call_count[NBA97_FRONTEND_DISPATCH_SITE_80040900] ==
          (special ? 0u : 12u));
    check(f.progress.call_count[NBA97_FRONTEND_DISPATCH_SITE_80040964] ==
          (special ? 0u : 12u));
    check(f.progress.call_count[NBA97_FRONTEND_DISPATCH_SITE_800409A8] ==
          (special ? 1u : 0u));
    check(f.progress.call_count[NBA97_FRONTEND_DISPATCH_SITE_800409D0] ==
          (special ? 1u : 0u));
    if (team == UINT32_MAX)
      check(callAt(f, 0x80040900u).machine.registers.gpr[4].word ==
            Fixture::Roster);
  }
  // The original context stores may alias saved registers in the same region.
  Fixture aliased;
  const U aliasContext = Fixture::Sp - 0x88u - 0x6b0u;
  aliased.put(0x800170c0u, aliasContext);
  aliased.hook = [](Fixture &f, const Nba97FrontendDispatchEvent &e,
                    Nba97FrontendDispatchMachine &m) {
    if (e.target == 0x80031a88u)
      f.put(m.registers.gpr[20].word + 0x720u, 5, 2);
  };
  check(aliased.run() == NBA97_TEXT_COMPLETE);
  check(aliased.progress.machine.registers.gpr[16].word == 0xffff0001u);
  check(aliased.progress.machine.registers.gpr[17].word == 0x1100ffffu);

  Fixture nullRoster;
  nullRoster.put(0x80023ab0u, 0);
  nullRoster.refusePc = 0x80040900u;
  check(nullRoster.run() == NBA97_TEXT_IO_REFUSED);
  check(callAt(nullRoster, 0x80040900u).machine.registers.gpr[4].word == 0);

  Fixture noKnown;
  noKnown.region.known = nullptr;
  noKnown.entry.registers.gpr[20].known_mask = 7;
  const auto before = noKnown.bytes;
  check(noKnown.run() == NBA97_TEXT_ARGUMENT &&
        noKnown.progress.stopped_pc == 0x8003f7ccu && noKnown.bytes == before);
  Fixture unaligned;
  unaligned.entry.registers.gpr[29].word += 1;
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x8003f7ccu);
  Fixture unknownSp;
  unknownSp.entry.registers.gpr[29].known_mask = 7;
  check(unknownSp.run() == NBA97_TEXT_UNKNOWN &&
        unknownSp.progress.operations == 0 &&
        unknownSp.progress.instruction_count == 2);
  Fixture unmapped;
  unmapped.put(0x800170c0u, 0x81000000u);
  check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.stopped_pc == 0x8003f80cu);

  Fixture unknownReturn;
  unknownReturn.entry.registers.gpr[31].known_mask = 7;
  check(unknownReturn.run() == NBA97_TEXT_UNKNOWN &&
        unknownReturn.progress.stopped_pc == 0x80040a14u &&
        unknownReturn.progress.machine.registers.gpr[29].word == Fixture::Sp);
  Fixture completeReturn;
  check(completeReturn.run() == NBA97_TEXT_COMPLETE);
  check(unknownReturn.progress.instruction_count ==
        completeReturn.progress.instruction_count);

  Fixture permuted;
  permuted.put(0x80024f80u + 5u * 4u, StateTargets[2]);
  permuted.refusePc = 0x8003fc58u;
  check(permuted.run() == NBA97_TEXT_IO_REFUSED);
  check(permuted.progress.stopped_target == 0x8003d930u);
}

void everyCallRefusal() {
  // Each scenario records a real source prefix, then refuses precisely the
  // selected call. All source sites must be witnessed at least once.
  struct Scenario {
    unsigned state;
    U result;
    unsigned variant;
  };
  std::map<unsigned, Scenario> witnesses;
  auto configure = [](Fixture &f, Scenario scenario) {
    f.selected = scenario.state;
    f.stateResult = scenario.result;
    f.put(0x80015098u, scenario.variant & 1u);
    f.put(0x8001edecu, 1, 2);
    f.cancelOnce = true;
    f.hook = [](Fixture &x, const Nba97FrontendDispatchEvent &e,
                Nba97FrontendDispatchMachine &m) {
      if (e.pc == 0x8003fa08u && x.queries == 1) {
        x.put(m.registers.gpr[29].word + 0x50u, 1, 2);
        x.put(m.registers.gpr[29].word + 0x58u, 2, 2);
      }
    };
    if (scenario.variant & 2u) {
      f.put(0x80021d74u, 29);
      f.put(0x80021d78u, 30);
    }
  };
  for (unsigned variant = 0; variant < 4; ++variant)
    for (unsigned state = 0; state < 43; ++state)
      for (U result : {UINT32_MAX, 0u, 1u, 2u, 4u, 5u, 8u, 9u, 12u, 28u}) {
        Fixture f;
        const Scenario scenario{state, result, variant};
        configure(f, scenario);
        check(f.run() == NBA97_TEXT_COMPLETE);
        for (const auto &c : f.calls)
          witnesses.emplace(c.event.site, scenario);
      }
  check(witnesses.size() == NBA97_FRONTEND_DISPATCH_SITE_COUNT - 1u);
  for (const auto &[site, scenario] : witnesses) {
    Fixture reference;
    configure(reference, scenario);
    check(reference.run() == NBA97_TEXT_COMPLETE);
    Nba97FrontendDispatchSiteContract contract{};
    check(nba97_frontend_dispatch_site_contract(std::uint8_t(site), &contract));
    Fixture refused;
    configure(refused, scenario);
    refused.refusePc = contract.pc;
    check(refused.run() == NBA97_TEXT_IO_REFUSED);
    check(refused.progress.stopped_pc == contract.pc &&
          refused.progress.stopped_target == contract.target);
    const auto &expected = callAt(reference, contract.pc);
    check(refused.progress.operations == expected.event.operation);
    machineEquals(refused.progress.machine, expected.machine);
    check(refused.progress.call_attempts[site] == 1 &&
          refused.progress.call_count[site] == 0);
  }
}

} // namespace

int main() {
  try {
    liveStateAndMemoryEdges();
    everyCallRefusal();
    acceptanceAndRegressions();
    normalAndAllStates();
    branchesMemoryAndCallbacks();
    budgetsValidationAndRepeatability();
    std::printf("frontend_dispatch_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
