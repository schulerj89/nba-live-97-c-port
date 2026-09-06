#include "recovered/game_countdown_ui_update.h"

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
    throw std::runtime_error("countdown check failed at " +
                             std::to_string(line));
}
#define check(value) checkAt((value), __LINE__)

bool wordEq(const Nba97GameCountdownUiUpdateWord &a,
            const Nba97GameCountdownUiUpdateWord &b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}
bool machineEq(const Nba97GameCountdownUiUpdateMachine &a,
               const Nba97GameCountdownUiUpdateMachine &b) {
  for (unsigned i = 0; i < 32; ++i)
    if (!wordEq(a.registers.gpr[i], b.registers.gpr[i]))
      return false;
  return wordEq(a.hi, b.hi) && wordEq(a.lo, b.lo);
}

struct Call {
  Nba97GameCountdownUiUpdateEvent event{};
  Nba97GameCountdownUiUpdateMachine machine{};
};

struct Fixture {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U Sp = 0x801ff000u;
  static constexpr U Ra = 0x81234568u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97GameCountdownUiUpdateMachine entry{};
  Nba97GameCountdownUiUpdateProgress progress{};
  std::array<Nba97GameCountdownUiUpdateAccess, 256> journal{};
  std::vector<Call> calls;
  std::size_t budget = 512;
  U refusePc = 0;
  U mutatePc = 0;
  U relocatedSp = 0x801fe000u;
  U liveS0 = 0x80110080u;
  bool invalidReturned = false;
  bool invalidReturnedMask = false;
  bool partialUploadS2 = false;
  U poisonKnownPc = 0;
  U poisonKnownAddress = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      entry.registers.gpr[i] = {0x11110000u + i * 0x101u, 15};
    entry.registers.gpr[0] = {0, 15};
    entry.registers.gpr[29] = {Sp, 15};
    entry.registers.gpr[31] = {Ra, 15};
    entry.hi = {0x12345678u, 5};
    entry.lo = {0x89abcdefu, 10};
    put(0x800fdba4u, 120, 4);
    put(0x800fe8ccu, 0, 2);
    put(0x800fdb58u, 120, 4);
    put(0x80021d92u, 1, 1);
    put(0x800fea2eu, 0xffffu, 2);
    put(0x800b2048u, 0x80110000u, 4);
    for (unsigned i = 0; i < 22; ++i)
      put(0x800249e4u + i, 0x40u + i, 1);
    put(0x800249e8u, 0x1555u, 2);
  }

  std::size_t at(U address, unsigned width = 1) const {
    if (address < Base || std::uint64_t(address) + width > Base + Size)
      throw std::out_of_range("unmapped");
    return address - Base;
  }
  void put(U address, U value, unsigned width, std::uint8_t mask = 15) {
    const auto offset = at(address, width);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = std::uint8_t(value >> (8u * i));
      known[offset + i] = std::uint8_t((mask >> i) & 1u);
    }
  }
  U get(U address, unsigned width) const {
    const auto offset = at(address, width);
    U value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= U(bytes[offset + i]) << (8u * i);
    return value;
  }

  static int io(void *opaque, const Nba97GameTextMemory *,
                const Nba97GameCountdownUiUpdateEvent *event,
                Nba97GameCountdownUiUpdateMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.calls.push_back({*event, *machine});
    if (event->pc == f.refusePc)
      return 0;
    if (event->pc == f.mutatePc) {
      for (unsigned i = 1; i < 32; ++i)
        machine->registers.gpr[i] = {0x90000000u + i,
                                     std::uint8_t((i * 5u) & 15u)};
      machine->registers.gpr[16] = {f.liveS0, 15};
      machine->registers.gpr[29] = {f.relocatedSp, 15};
      machine->hi = {0x24681357u, 3};
      machine->lo = {0x13572468u, 12};
    }
    if (f.invalidReturned)
      machine->registers.gpr[0].word = 1;
    if (f.invalidReturnedMask)
      machine->registers.gpr[13].known_mask = 16;
    if (f.partialUploadS2 && event->pc == 0x80032ae4u)
      machine->registers.gpr[18] = {0x12345678u, 12};
    if (event->pc == f.poisonKnownPc)
      f.known[f.at(f.poisonKnownAddress)] = 2;
    machine->registers.gpr[2] = {0xabcdef01u, 15};
    return 1;
  }

  int run() {
    Nba97GameCountdownUiUpdateContext context{};
    context.memory = {&region, 1};
    context.operation_budget = budget;
    context.machine = entry;
    context.io = io;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    return nba97_game_countdown_ui_update(&context, &progress);
  }
};

void activeRecordAndCalls() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.active_gate && f.progress.record_uploaded);
  check(f.progress.instruction_count == 300);
  check(f.calls.size() == 2 && f.calls[0].event.pc == 0x800329e8u &&
        f.calls[0].event.entry == 0x80030d18u &&
        f.calls[1].event.pc == 0x80032ae4u &&
        f.calls[1].event.entry == 0x80094540u);
  check(f.calls[0].machine.registers.gpr[4].word == 0xc9u &&
        f.calls[0].machine.registers.gpr[5].word == 0x800249fcu &&
        f.calls[0].machine.registers.gpr[6].word == 0x1ecu &&
        f.calls[0].machine.registers.gpr[7].word == 0x14u &&
        f.get(Fixture::Sp - 0x40u + 0x10u, 4) == 0xf0u);
  check(f.calls[1].machine.registers.gpr[4].word == 0x800fb5c0u &&
        f.calls[1].machine.registers.gpr[5].word == 0 &&
        f.calls[1].machine.registers.gpr[6].word == 0 &&
        f.calls[1].machine.registers.gpr[7].word == 0x340u &&
        f.calls[1].machine.registers.gpr[18].word == 2u);
  check(f.get(0x800b2048u, 4) == 0x80110000u &&
        f.get(0x80110026u, 2) == 0x300u && f.get(0x800fea2eu, 2) == 2u);
  check(f.get(0x800fb5c0u, 4) == 0x23u && f.get(0x800fb5c4u, 2) == 0x10u &&
        f.get(0x800fb5c6u, 2) == 1u && f.get(0x800fb5d0u, 2) == 0x421u &&
        f.get(0x800fb5ecu, 2) == 0x1a98u && f.get(0x800fb5eeu, 2) == 0x4210u);
  check(f.progress.palette_iterations == 13 &&
        f.progress.machine.hi.known_mask == 15 &&
        f.progress.machine.lo.known_mask == 15 &&
        f.progress.machine.registers.gpr[29].word == Fixture::Sp &&
        f.progress.machine.registers.gpr[31].word == Fixture::Ra);
  for (unsigned bit = 0; bit < 13; ++bit)
    check(f.get(0x800fb5eau - bit * 2u, 2) ==
          (((0x1555u >> 2u) >> bit) & 1u ? 0x1fu : 8u));
  const std::array<U, 12> earlyPc{0x80032880u, 0x80032888u, 0x8003288cu,
                                  0x80032890u, 0x80032894u, 0x800328a0u,
                                  0x800328a4u, 0x800328a8u, 0x800328acu,
                                  0x800328b0u, 0x800328b4u, 0x800328b8u};
  for (std::size_t i = 0; i < earlyPc.size(); ++i)
    check(f.journal[i].pc == earlyPc[i]);
}

void gatesAndCallbackLiveStores() {
  const std::array<std::pair<U, U>, 4> gates{{{0x800fdba4u, 601u},
                                              {0x800fe8ccu, 1u},
                                              {0x800fdb58u, 119u},
                                              {0x80021d92u, 0u}}};
  for (const auto &gate : gates) {
    Fixture f;
    f.put(gate.first, gate.second,
          gate.first == 0x80021d92u   ? 1
          : gate.first == 0x800fe8ccu ? 2
                                      : 4);
    f.put(0x800fea2eu, 7, 2);
    check(f.run() == NBA97_TEXT_COMPLETE && !f.progress.active_gate &&
          f.calls.size() == 1 && f.calls[0].event.pc == 0x8003295cu &&
          f.get(0x800fea2eu, 2) == 0xffffu);
  }
  Fixture cached;
  cached.put(0x800fdba4u, 601, 4);
  check(cached.run() == NBA97_TEXT_COMPLETE && cached.calls.empty() &&
        cached.get(0x800fea2eu, 2) == 0xffffu);

  Fixture live;
  live.put(0x800fdba4u, 601, 4);
  live.put(0x800fea2eu, 7, 2);
  live.mutatePc = 0x8003295cu;
  live.put(live.liveS0, 0x7777u, 2);
  live.put(live.relocatedSp + 0x30u, 0x11111110u, 4);
  live.put(live.relocatedSp + 0x34u, 0x22222220u, 4);
  live.put(live.relocatedSp + 0x38u, 0x33333330u, 4);
  live.put(live.relocatedSp + 0x3cu, 0x44444440u, 4);
  check(live.run() == NBA97_TEXT_COMPLETE &&
        live.get(live.liveS0, 2) == 0xffffu && live.get(0x800fea2eu, 2) == 7u &&
        live.progress.machine.registers.gpr[16].word == 0x11111110u &&
        live.progress.machine.registers.gpr[17].word == 0x22222220u &&
        live.progress.machine.registers.gpr[18].word == 0x33333330u &&
        live.progress.machine.registers.gpr[31].word == 0x44444440u &&
        live.progress.machine.registers.gpr[29].word ==
            live.relocatedSp + 0x40u);
  check(live.progress.machine.hi.word == 0x24681357u &&
        live.progress.machine.lo.word == 0x13572468u);

  Fixture equal;
  equal.put(0x800fea2eu, 2, 2);
  check(equal.run() == NBA97_TEXT_COMPLETE && equal.progress.active_gate &&
        !equal.progress.record_uploaded && equal.calls.empty() &&
        equal.progress.machine.registers.gpr[3].word == 0x300u);

  Fixture boundary;
  boundary.put(0x800fdba4u, 600, 4);
  boundary.put(0x800fdb58u, 600, 4);
  check(boundary.run() == NBA97_TEXT_COMPLETE && boundary.progress.active_gate);
  Fixture negativeCountdown;
  negativeCountdown.put(0x800fdba4u, 0xffffffffu, 4);
  negativeCountdown.put(0x800fdb58u, 0xffffffffu, 4);
  check(negativeCountdown.run() == NBA97_TEXT_COMPLETE &&
        negativeCountdown.progress.active_gate);
  Fixture wrappingClock;
  wrappingClock.put(0x800fdba4u, 600u, 4);
  wrappingClock.put(0x800fdb58u, 0x80000000u, 4);
  check(wrappingClock.run() == NBA97_TEXT_COMPLETE &&
        !wrappingClock.progress.active_gate);

  Fixture negativeCache;
  negativeCache.put(0x800fea2eu, 0xfffeu, 2);
  check(negativeCache.run() == NBA97_TEXT_COMPLETE &&
        negativeCache.calls.size() == 2 &&
        negativeCache.calls[0].event.pc == 0x800329e8u);
}

void copyOrderingAliasesAndRecordOrdering() {
  Fixture copy;
  copy.known[copy.at(0x800249e6u)] = 0;
  check(copy.run() == NBA97_TEXT_COMPLETE);
  const std::array<U, 10> pairPc{
      0x800328a0u, 0x800328a4u, 0x800328a8u, 0x800328acu, 0x800328b0u,
      0x800328b4u, 0x800328b8u, 0x800328bcu, 0x800328c0u, 0x800328c4u};
  for (std::size_t i = 0; i < pairPc.size(); ++i)
    check(copy.journal[5 + i].pc == pairPc[i]);
  check(copy.known[copy.at(Fixture::Sp - 0x40u + 0x18u) + 2] == 0);

  Fixture alias;
  alias.entry.registers.gpr[29] = {0x80024a24u, 15};
  alias.entry.registers.gpr[31] = {0x80024a10u, 15};
  check(alias.run() == NBA97_TEXT_COMPLETE && alias.progress.completed &&
        alias.progress.frame_stack_pointer == 0x800249e4u);

  Fixture record;
  check(record.run() == NBA97_TEXT_COMPLETE);
  std::size_t sb = record.progress.access_events;
  std::size_t lbu = record.progress.access_events;
  std::size_t sw = record.progress.access_events;
  for (std::size_t i = 0; i < record.progress.access_events; ++i) {
    if (record.journal[i].pc == 0x80032a10u)
      sb = i;
    if (record.journal[i].pc == 0x80032a14u)
      lbu = i;
    if (record.journal[i].pc == 0x80032a84u)
      sw = i;
  }
  check(sb < lbu && lbu < sw && record.journal[sb].value == 0x23u &&
        record.journal[lbu].value == 0x23u &&
        record.journal[sw].value == 0x23u);
}

void refusalsBudgetsKnownnessAndMetadata() {
  for (U pc : {0x8003295cu, 0x800329e8u, 0x80032ae4u}) {
    Fixture f;
    if (pc == 0x8003295cu) {
      f.put(0x800fdba4u, 601, 4);
      f.put(0x800fea2eu, 7, 2);
    }
    f.refusePc = pc;
    check(f.run() == NBA97_TEXT_IO_REFUSED && f.progress.stopped_pc == pc &&
          f.progress.stopped_entry == f.calls.back().event.entry);
  }
  Fixture baseline;
  check(baseline.run() == NBA97_TEXT_COMPLETE);
  for (std::size_t budget = 0; budget < baseline.progress.operations;
       ++budget) {
    Fixture f;
    f.budget = budget;
    check(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget &&
          !f.progress.completed);
  }

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f;
    f.entry.registers.gpr[31].known_mask = std::uint8_t(mask);
    const int result = f.run();
    check(result == (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    if (mask != 15)
      check(f.progress.stopped_pc == 0x80032b08u &&
            f.progress.machine.registers.gpr[29].word == Fixture::Sp);
  }
  Fixture badReturn;
  badReturn.entry.registers.gpr[31] = {Fixture::Ra | 1u, 15};
  check(badReturn.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        badReturn.progress.stopped_pc == 0x80032b08u);

  Fixture partialMult;
  partialMult.known[partialMult.at(0x800fdba4u) + 3] = 0;
  check(partialMult.run() == NBA97_TEXT_UNKNOWN &&
        partialMult.progress.stopped_pc == 0x800328fcu);

  Fixture boundedGate;
  boundedGate.known[boundedGate.at(0x800fdba4u)] = 0;
  boundedGate.put(0x800fdb58u, 1000, 4);
  check(boundedGate.run() == NBA97_TEXT_UNKNOWN &&
        boundedGate.progress.active_gate &&
        boundedGate.progress.stopped_pc != 0x800328fcu &&
        boundedGate.progress.stopped_pc != 0x80032928u &&
        boundedGate.progress.machine.lo.known_mask == 0 &&
        boundedGate.progress.machine.hi.known_mask == 0);

  Fixture malformed;
  malformed.known[malformed.at(0x800249e7u)] = 2;
  check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x800328a0u);
  Fixture invalidReturned;
  invalidReturned.invalidReturned = true;
  check(invalidReturned.run() == NBA97_TEXT_ARGUMENT &&
        invalidReturned.progress.machine.registers.gpr[0].word == 1);
  Fixture invalidMask;
  invalidMask.invalidReturnedMask = true;
  check(invalidMask.run() == NBA97_TEXT_ARGUMENT &&
        invalidMask.progress.machine.registers.gpr[13].known_mask == 16);

  Fixture lateMalformed;
  lateMalformed.poisonKnownPc = 0x80032ae4u;
  lateMalformed.poisonKnownAddress = Fixture::Sp - 4u;
  check(lateMalformed.run() == NBA97_TEXT_ARGUMENT);
  check(lateMalformed.progress.stopped_pc == 0x80032af4u);
  check(lateMalformed.progress.machine.registers.gpr[31].word == 0x80032aecu);
  check(lateMalformed.get(0x800fea2eu, 2) == 2u);

  Fixture nullKnown;
  nullKnown.region.known = nullptr;
  nullKnown.partialUploadS2 = true;
  const U cacheBefore = nullKnown.get(0x800fea2eu, 2);
  check(nullKnown.run() == NBA97_TEXT_ARGUMENT &&
        nullKnown.progress.stopped_pc == 0x80032af0u &&
        nullKnown.get(0x800fea2eu, 2) == cacheBefore);

  Fixture unaligned;
  unaligned.entry.registers.gpr[29].word |= 1u;
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80032888u);

  Fixture unmapped;
  unmapped.region.size = 0x1000;
  check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.stopped_pc == 0x80032880u);

  Nba97GameCountdownUiUpdateProgress progress{};
  check(nba97_game_countdown_ui_update(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
  Fixture invalid;
  Nba97GameCountdownUiUpdateContext context{};
  context.memory = {&invalid.region, 1};
  context.machine = invalid.entry;
  context.access_journal_capacity = 1;
  check(nba97_game_countdown_ui_update(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);
  Nba97GameTextRegion overlap[2]{
      {Fixture::Base, invalid.bytes.data(), invalid.known.data(),
       invalid.bytes.size()},
      {Fixture::Base + 1u, invalid.bytes.data(), invalid.known.data(), 1}};
  context = {};
  context.memory = {overlap, 2};
  context.machine = invalid.entry;
  check(nba97_game_countdown_ui_update(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);
  Nba97GameTextRegion oversized{0, invalid.bytes.data(), invalid.known.data(),
                                SIZE_MAX};
  context = {};
  context.memory = {&oversized, 1};
  context.machine = invalid.entry;
  check(nba97_game_countdown_ui_update(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);

  Fixture a;
  Fixture b;
  check(a.run() == NBA97_TEXT_COMPLETE && b.run() == NBA97_TEXT_COMPLETE &&
        a.bytes == b.bytes && a.known == b.known &&
        machineEq(a.progress.machine, b.progress.machine));
}
} // namespace

int main() {
  try {
    activeRecordAndCalls();
    gatesAndCallbackLiveStores();
    copyOrderingAliasesAndRecordOrdering();
    refusalsBudgetsKnownnessAndMetadata();
    std::printf("game_countdown_ui_update_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
