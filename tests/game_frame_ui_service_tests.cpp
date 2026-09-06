#include "recovered/game_frame_ui_service.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;

void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("frame UI service check failed at line " +
                             std::to_string(line));
}
#define check(value) checkAt((value), __LINE__)

bool sameWord(const Nba97GameFrameUiServiceWord &a,
              const Nba97GameFrameUiServiceWord &b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

bool sameMachine(const Nba97GameFrameUiServiceMachine &a,
                 const Nba97GameFrameUiServiceMachine &b) {
  for (unsigned i = 0; i < 32; ++i)
    if (!sameWord(a.registers.gpr[i], b.registers.gpr[i]))
      return false;
  return sameWord(a.hi, b.hi) && sameWord(a.lo, b.lo);
}

struct Call {
  Nba97GameFrameUiServiceEvent event{};
  Nba97GameFrameUiServiceMachine machine{};
};

struct Fixture {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U EntrySp = 0x801ff000u;
  static constexpr U EntryRa = 0x81234568u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0x5a);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97GameFrameUiServiceMachine entry{};
  Nba97GameFrameUiServiceProgress progress{};
  std::array<Nba97GameFrameUiServiceAccess, 16> journal{};
  std::vector<Call> calls;
  std::array<Nba97GameFrameUiServiceWord, 2> query{{{1, 15}, {1, 15}}};
  Nba97GameFrameUiServiceWord frame_result{0x11111111u, 15};
  Nba97GameFrameUiServiceWord command_result{0x22222222u, 15};
  Nba97GameFrameUiServiceWord idle_result{0x33333333u, 15};
  U refuse_pc = 0;
  U malformed_pc = 0;
  U corrupt_saved_pc = 0;
  U mutate_pc = 0;
  U relocated_sp = 0x801fe000u;
  std::size_t budget = 64;
  bool no_io = false;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i) {
      entry.registers.gpr[i].word = 0x10203040u + i * 0x01010101u;
      entry.registers.gpr[i].known_mask = 15;
    }
    entry.registers.gpr[0] = {0, 15};
    entry.registers.gpr[29] = {EntrySp, 15};
    entry.registers.gpr[31] = {EntryRa, 15};
    entry.hi = {0xa1b2c3d4u, 11};
    entry.lo = {0x55667788u, 13};
    put(0x800fa038u, 1, 2);
    put(0x800eb680u, 0, 1);
  }

  std::size_t at(U address, unsigned width = 1) const {
    if (address < Base || std::uint64_t(address) + width > Base + Size)
      throw std::out_of_range("unmapped fixture address");
    return address - Base;
  }

  void put(U address, U value, unsigned width, std::uint8_t mask = 15) {
    const auto offset = at(address, width);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = std::uint8_t(value >> (i * 8u));
      known[offset + i] = std::uint8_t((mask >> i) & 1u);
    }
  }

  U get(U address, unsigned width) const {
    const auto offset = at(address, width);
    U value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= U(bytes[offset + i]) << (i * 8u);
    return value;
  }

  static int io(void *opaque, const Nba97GameTextMemory *,
                const Nba97GameFrameUiServiceEvent *event,
                Nba97GameFrameUiServiceMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.calls.push_back({*event, *machine});
    if (event->pc == f.refuse_pc)
      return 0;
    if (event->pc == f.mutate_pc) {
      for (unsigned i = 1; i < 32; ++i) {
        machine->registers.gpr[i].word = 0x90000000u + i;
        machine->registers.gpr[i].known_mask = std::uint8_t((i * 7u) & 15u);
      }
      machine->registers.gpr[29] = {f.relocated_sp, 15};
      machine->hi = {0x13579bdfu, 3};
      machine->lo = {0x2468ace0u, 12};
    }
    if (event->pc == f.malformed_pc)
      machine->lo.known_mask = 16;
    if (event->pc == f.corrupt_saved_pc) {
      const auto offset = f.at(machine->registers.gpr[29].word + 0x10u, 4);
      f.known[offset + 3] = 2;
    }
    Nba97GameFrameUiServiceWord result{};
    switch (event->kind) {
    case NBA97_GAME_FRAME_UI_SERVICE_CHILD_8003287C:
      result = f.frame_result;
      break;
    case NBA97_GAME_FRAME_UI_SERVICE_CHILD_80031C5C:
      result = f.query[event->invocation - 1];
      break;
    case NBA97_GAME_FRAME_UI_SERVICE_CHILD_8003066C:
      result = f.command_result;
      break;
    case NBA97_GAME_FRAME_UI_SERVICE_CHILD_80032774:
      result = f.idle_result;
      break;
    default:
      return 0;
    }
    machine->registers.gpr[2] = result;
    return 1;
  }

  int run() {
    Nba97GameFrameUiServiceContext context{};
    context.memory = {&region, 1};
    context.operation_budget = budget;
    context.machine = entry;
    context.io = no_io ? nullptr : io;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    return nba97_game_frame_ui_service(&context, &progress);
  }
};

void normalPathsAndCalls() {
  Fixture first;
  first.query = {{{0xffffffffu, 15}, {0, 15}}};
  check(first.run() == NBA97_TEXT_COMPLETE && first.progress.completed);
  check(first.calls.size() == 4 && first.progress.operations == 7 &&
        first.progress.accesses == 3 && first.progress.reads == 2 &&
        first.progress.stores == 1);
  const std::array<U, 4> firstPc{0x80032b18u, 0x80032b34u, 0x80032b48u,
                                 0x80032b50u};
  const std::array<U, 4> firstEntry{0x8003287cu, 0x80031c5cu, 0x8003066cu,
                                    0x8003066cu};
  const std::array<U, 4> firstArg{0x14243444u, 0xd4u, 0xd3u, 0xd4u};
  for (std::size_t i = 0; i < first.calls.size(); ++i) {
    const auto &call = first.calls[i];
    check(call.event.pc == firstPc[i] &&
          call.event.delay_slot_pc == firstPc[i] + 4u &&
          call.event.entry == firstEntry[i]);
    check(call.event.argument_count == (i ? 1 : 0) &&
          call.machine.registers.gpr[31].word == firstPc[i] + 8u &&
          call.machine.registers.gpr[31].known_mask == 15);
    check(!i || (call.machine.registers.gpr[4].word == firstArg[i] &&
                 call.machine.registers.gpr[4].known_mask == 15));
  }
  check(first.progress.call_count[NBA97_GAME_FRAME_UI_SERVICE_CHILD_8003287C] ==
            1 &&
        first.progress.call_count[NBA97_GAME_FRAME_UI_SERVICE_CHILD_80031C5C] ==
            1 &&
        first.progress.call_count[NBA97_GAME_FRAME_UI_SERVICE_CHILD_8003066C] ==
            2);
  check(first.progress.machine.registers.gpr[2].word ==
            first.command_result.word &&
        first.progress.machine.registers.gpr[4].word == 0xd4u &&
        first.progress.machine.registers.gpr[29].word == Fixture::EntrySp &&
        first.progress.machine.registers.gpr[31].word == Fixture::EntryRa);
  check(first.progress.saved_return_address.word == Fixture::EntryRa &&
        first.progress.restored_return_address.word == Fixture::EntryRa &&
        first.progress.instruction_count == 24);
  for (unsigned i = 0; i < 32; ++i)
    if (i != 2 && i != 4)
      check(sameWord(first.progress.machine.registers.gpr[i],
                     first.entry.registers.gpr[i]));
  check(sameWord(first.progress.machine.hi, first.entry.hi) &&
        sameWord(first.progress.machine.lo, first.entry.lo));

  Fixture second;
  second.query = {{{0x100u, 15}, {0xffu, 15}}};
  check(second.run() == NBA97_TEXT_COMPLETE && second.calls.size() == 5);
  const std::array<U, 5> secondPc{0x80032b18u, 0x80032b34u, 0x80032b60u,
                                  0x80032b74u, 0x80032b7cu};
  const std::array<U, 5> secondArg{0u, 0xd4u, 0xc8u, 0xc8u, 2u};
  for (std::size_t i = 0; i < second.calls.size(); ++i) {
    check(second.calls[i].event.pc == secondPc[i]);
    if (i)
      check(second.calls[i].machine.registers.gpr[4].word == secondArg[i]);
  }
  check(second.progress.instruction_count == 29 &&
        second.progress.machine.registers.gpr[4].word == 2u);

  Fixture queryExit;
  queryExit.query = {{{0, 15}, {0x100u, 15}}};
  check(queryExit.run() == NBA97_TEXT_COMPLETE && queryExit.calls.size() == 3 &&
        queryExit.progress.machine.registers.gpr[2].word == 0 &&
        queryExit.progress.instruction_count == 23);

  Fixture byteExit;
  byteExit.put(0x800fa038u, 0, 2);
  byteExit.put(0x800eb680u, 0xff, 1);
  check(byteExit.run() == NBA97_TEXT_COMPLETE && byteExit.calls.size() == 1 &&
        byteExit.progress.machine.registers.gpr[2].word == 0xffu &&
        byteExit.progress.instruction_count == 18);

  Fixture idle;
  idle.put(0x800fa038u, 0, 2);
  idle.idle_result = {0xffffffffu, 15};
  check(idle.run() == NBA97_TEXT_COMPLETE && idle.calls.size() == 2 &&
        idle.calls[1].event.pc == 0x80032ba0u &&
        idle.calls[1].event.delay_slot_pc == 0x80032ba4u &&
        idle.calls[1].event.argument_count == 0 &&
        idle.progress.machine.registers.gpr[2].word == 0xffffffffu &&
        idle.progress.instruction_count == 20);
}

void ModeAndLowByteKnownness() {
  Fixture negative;
  negative.put(0x800fa038u, 0x8000u, 2);
  negative.query[0] = {0, 15};
  negative.query[1] = {0, 15};
  check(negative.run() == NBA97_TEXT_COMPLETE && negative.calls.size() == 3);

  const std::array<U, 4> values{0u, 0x100u, 0xffffffffu, 0xffu};
  const std::array<std::size_t, 4> counts{3, 3, 4, 4};
  for (std::size_t i = 0; i < values.size(); ++i) {
    Fixture f;
    f.query[0] = {values[i], 15};
    f.query[1] = {0, 15};
    check(f.run() == NBA97_TEXT_COMPLETE && f.calls.size() == counts[i]);
  }

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f;
    f.query[0] = {1, std::uint8_t(mask)};
    const int result = f.run();
    if (mask & 1u) {
      check(result == NBA97_TEXT_COMPLETE && f.calls.size() == 4);
    } else {
      check(result == NBA97_TEXT_UNKNOWN &&
            f.progress.stopped_pc == 0x80032b40u && f.calls.size() == 2 &&
            f.progress.machine.registers.gpr[2].word == 1u &&
            f.progress.machine.registers.gpr[2].known_mask == 14);
    }
  }

  Fixture unknownMode;
  unknownMode.put(0x800fa038u, 0, 2, 1);
  check(unknownMode.run() == NBA97_TEXT_UNKNOWN &&
        unknownMode.progress.stopped_pc == 0x80032b2cu &&
        unknownMode.progress.machine.registers.gpr[2].known_mask == 1);

  Fixture knownNonzeroMode;
  knownNonzeroMode.put(0x800fa038u, 0x80u, 2, 1);
  knownNonzeroMode.query[0] = {1, 15};
  check(knownNonzeroMode.run() == NBA97_TEXT_COMPLETE &&
        knownNonzeroMode.calls.size() == 4);

  Fixture unknownByte;
  unknownByte.put(0x800fa038u, 0, 2);
  unknownByte.put(0x800eb680u, 0, 1, 0);
  check(unknownByte.run() == NBA97_TEXT_UNKNOWN &&
        unknownByte.progress.stopped_pc == 0x80032b98u &&
        unknownByte.progress.machine.registers.gpr[2].word == 0 &&
        unknownByte.progress.machine.registers.gpr[2].known_mask == 14);

  Fixture unknownSecondQuery;
  unknownSecondQuery.query = {{{0, 15}, {0x80u, 14}}};
  check(unknownSecondQuery.run() == NBA97_TEXT_UNKNOWN &&
        unknownSecondQuery.progress.stopped_pc == 0x80032b6cu &&
        unknownSecondQuery.progress.machine.registers.gpr[2].word == 0x80u &&
        unknownSecondQuery.progress.machine.registers.gpr[2].known_mask == 14 &&
        unknownSecondQuery.progress.machine.registers.gpr[4].word == 0xc8u &&
        unknownSecondQuery.progress.machine.registers.gpr[31].word ==
            0x80032b68u);
}

void CallbackLiveMachineAndAliases() {
  Fixture live;
  live.put(live.relocated_sp + 0x10u, 0x89abcdecu, 4);
  live.mutate_pc = 0x80032b18u;
  live.query[0] = {1, 15};
  check(live.run() == NBA97_TEXT_COMPLETE);
  for (unsigned i = 1; i < 32; ++i) {
    if (i == 2 || i == 4 || i == 29 || i == 31)
      continue;
    check(live.progress.machine.registers.gpr[i].word == 0x90000000u + i &&
          live.progress.machine.registers.gpr[i].known_mask ==
              std::uint8_t((i * 7u) & 15u));
  }
  check(live.progress.machine.hi.word == 0x13579bdfu &&
        live.progress.machine.hi.known_mask == 3 &&
        live.progress.machine.lo.word == 0x2468ace0u &&
        live.progress.machine.lo.known_mask == 12);
  check(live.progress.restored_return_address.word == 0x89abcdecu &&
        live.progress.machine.registers.gpr[29].word ==
            live.relocated_sp + 0x18u &&
        live.progress.stopped_pc == 0);

  Fixture alias;
  alias.entry.registers.gpr[29] = {0x800fa040u, 15};
  alias.entry.registers.gpr[31] = {0x81230004u, 15};
  alias.query[0] = {1, 15};
  check(alias.run() == NBA97_TEXT_COMPLETE && alias.calls.size() == 4);
  check(alias.journal[0].pc == 0x80032b14u &&
        alias.journal[0].address == 0x800fa038u &&
        alias.journal[1].pc == 0x80032b24u &&
        alias.journal[1].address == 0x800fa038u &&
        alias.progress.machine.registers.gpr[31].word == 0x81230004u);
}

void BudgetsAndStaticRefusals() {
  Fixture baseline;
  baseline.query[0] = {1, 15};
  check(baseline.run() == NBA97_TEXT_COMPLETE);
  const auto expectedCalls = baseline.calls;
  const auto expectedJournal = baseline.journal;
  for (std::size_t budget = 0; budget < baseline.progress.operations;
       ++budget) {
    Fixture f;
    f.query[0] = {1, 15};
    f.budget = budget;
    check(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget &&
          !f.progress.completed);
    for (std::size_t i = 0; i < f.calls.size(); ++i)
      check(f.calls[i].event.pc == expectedCalls[i].event.pc);
    for (std::size_t i = 0; i < f.progress.access_events; ++i)
      check(f.journal[i].pc == expectedJournal[i].pc &&
            f.journal[i].address == expectedJournal[i].address);
  }

  Fixture longest;
  longest.query = {{{0, 15}, {1, 15}}};
  check(longest.run() == NBA97_TEXT_COMPLETE &&
        longest.progress.operations == 8);
  for (std::size_t budget = 0; budget < longest.progress.operations; ++budget) {
    Fixture f;
    f.query = longest.query;
    f.budget = budget;
    check(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget &&
          !f.progress.completed);
    for (std::size_t i = 0; i < f.calls.size(); ++i)
      check(f.calls[i].event.pc == longest.calls[i].event.pc);
    for (std::size_t i = 0; i < f.progress.access_events; ++i)
      check(f.journal[i].pc == longest.journal[i].pc &&
            f.journal[i].address == longest.journal[i].address);
  }

  const std::array<U, 8> sites{0x80032b18u, 0x80032b34u, 0x80032b48u,
                               0x80032b50u, 0x80032b60u, 0x80032b74u,
                               0x80032b7cu, 0x80032ba0u};
  for (U pc : sites) {
    Fixture f;
    f.refuse_pc = pc;
    if (pc >= 0x80032b60u && pc <= 0x80032b7cu)
      f.query = {{{0, 15}, {1, 15}}};
    else if (pc == 0x80032ba0u)
      f.put(0x800fa038u, 0, 2);
    else
      f.query[0] = {1, 15};
    check(f.run() == NBA97_TEXT_IO_REFUSED && f.progress.stopped_pc == pc &&
          f.progress.stopped_entry == f.calls.back().event.entry);
    check(f.progress.machine.registers.gpr[31].word == pc + 8u);
    if (f.calls.back().event.argument_count)
      check(f.progress.machine.registers.gpr[4].word ==
            f.calls.back().machine.registers.gpr[4].word);
  }

  Fixture absent;
  absent.no_io = true;
  check(absent.run() == NBA97_TEXT_IO_REFUSED &&
        absent.progress.stopped_pc == 0x80032b18u);
  Fixture malformed;
  malformed.malformed_pc = 0x80032b34u;
  check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x80032b34u &&
        malformed.progress.machine.lo.known_mask == 16);
}

void MemoryErrorsAndReturnMasks() {
  Fixture lateGlobal;
  lateGlobal.known[lateGlobal.at(0x800fa039u)] = 2;
  check(lateGlobal.run() == NBA97_TEXT_ARGUMENT &&
        lateGlobal.progress.stopped_pc == 0x80032b24u &&
        lateGlobal.progress.machine.registers.gpr[2].word == 0x80100000u &&
        lateGlobal.progress.machine.registers.gpr[2].known_mask == 15);

  Fixture lateReturn;
  lateReturn.query[0] = {1, 15};
  lateReturn.corrupt_saved_pc = 0x80032b50u;
  check(lateReturn.run() == NBA97_TEXT_ARGUMENT &&
        lateReturn.progress.stopped_pc == 0x80032ba8u &&
        lateReturn.progress.machine.registers.gpr[31].word == 0x80032b58u &&
        lateReturn.progress.stores == 1 && lateReturn.calls.size() == 4);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f;
    f.entry.registers.gpr[31].known_mask = std::uint8_t(mask);
    f.query[0] = {1, 15};
    const int result = f.run();
    check(result == (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    check(f.progress.restored_return_address.word == Fixture::EntryRa &&
          f.progress.restored_return_address.known_mask == mask);
    if (mask != 15)
      check(f.progress.stopped_pc == 0x80032bb0u &&
            f.progress.machine.registers.gpr[29].word == Fixture::EntrySp &&
            f.progress.instruction_count == 24);
  }

  Fixture misaligned;
  misaligned.entry.registers.gpr[31] = {Fixture::EntryRa | 1u, 15};
  check(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.stopped_pc == 0x80032bb0u &&
        misaligned.progress.machine.registers.gpr[29].word ==
            Fixture::EntrySp &&
        misaligned.progress.instruction_count == 24);

  Fixture unknownStore;
  unknownStore.entry.registers.gpr[31].known_mask = 7;
  unknownStore.region.known = nullptr;
  const auto before = unknownStore.get(Fixture::EntrySp - 8u, 4);
  check(unknownStore.run() == NBA97_TEXT_ARGUMENT &&
        unknownStore.progress.stopped_pc == 0x80032b14u &&
        unknownStore.get(Fixture::EntrySp - 8u, 4) == before);
}

void MappingMetadataAndDeterminism() {
  Fixture wrap;
  std::array<std::uint8_t, 32> low{};
  std::array<std::uint8_t, 32> lowKnown{};
  lowKnown.fill(1);
  wrap.entry.registers.gpr[29] = {0x10u, 15};
  wrap.entry.registers.gpr[31] = {0x100u, 15};
  wrap.put(0x800fa038u, 0, 2);
  wrap.put(0x800eb680u, 1, 1);
  Nba97GameTextRegion wrapRegions[2]{
      {0, low.data(), lowKnown.data(), low.size()}, wrap.region};
  Nba97GameFrameUiServiceContext wrapContext{};
  wrapContext.memory = {wrapRegions, 2};
  wrapContext.operation_budget = wrap.budget;
  wrapContext.machine = wrap.entry;
  wrapContext.io = Fixture::io;
  wrapContext.user = &wrap;
  wrapContext.access_journal = wrap.journal.data();
  wrapContext.access_journal_capacity = wrap.journal.size();
  check(nba97_game_frame_ui_service(&wrapContext, &wrap.progress) ==
            NBA97_TEXT_COMPLETE &&
        wrap.progress.frame_stack_pointer == 0xfffffff8u &&
        wrap.progress.machine.registers.gpr[29].word == 0x10u &&
        wrap.journal[0].address == 8u && wrap.journal[3].address == 8u);

  Fixture unaligned;
  unaligned.entry.registers.gpr[29] = {0x801ff001u, 15};
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80032b14u);

  Fixture unmapped;
  unmapped.region.size = 0x1000;
  check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.stopped_pc == 0x80032b14u);

  Fixture invalidMachine;
  invalidMachine.entry.registers.gpr[0].known_mask = 0;
  check(invalidMachine.run() == NBA97_TEXT_ARGUMENT &&
        invalidMachine.progress.operations == 0);

  Nba97GameFrameUiServiceProgress progress{};
  check(nba97_game_frame_ui_service(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
  Fixture invalidJournal;
  Nba97GameFrameUiServiceContext context{};
  context.memory = {&invalidJournal.region, 1};
  context.machine = invalidJournal.entry;
  context.access_journal_capacity = 1;
  check(nba97_game_frame_ui_service(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_frame_ui_service(&context, nullptr) == NBA97_TEXT_ARGUMENT);

  Fixture overlap;
  Nba97GameTextRegion regions[2]{
      {Fixture::Base, overlap.bytes.data(), overlap.known.data(),
       overlap.bytes.size()},
      {Fixture::Base + 1u, overlap.bytes.data(), overlap.known.data(), 1}};
  context = {};
  context.memory = {regions, 2};
  context.machine = overlap.entry;
  check(nba97_game_frame_ui_service(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);

  Fixture huge;
  Nba97GameTextRegion hugeRegion{0, huge.bytes.data(), huge.known.data(),
                                 std::numeric_limits<std::size_t>::max()};
  context = {};
  context.memory = {&hugeRegion, 1};
  context.machine = huge.entry;
  check(nba97_game_frame_ui_service(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);

  Fixture a;
  Fixture b;
  a.query[0] = b.query[0] = {1, 15};
  check(a.run() == NBA97_TEXT_COMPLETE && b.run() == NBA97_TEXT_COMPLETE);
  check(a.bytes == b.bytes && a.known == b.known &&
        sameMachine(a.progress.machine, b.progress.machine));
  check(a.progress.operations == b.progress.operations &&
        a.progress.instruction_count == b.progress.instruction_count &&
        a.progress.access_events == b.progress.access_events);
  for (std::size_t i = 0; i < a.progress.access_events; ++i)
    check(a.journal[i].pc == b.journal[i].pc &&
          a.journal[i].address == b.journal[i].address &&
          a.journal[i].value == b.journal[i].value &&
          a.journal[i].known_mask == b.journal[i].known_mask);
}
} // namespace

int main() {
  try {
    normalPathsAndCalls();
    ModeAndLowByteKnownness();
    CallbackLiveMachineAndAliases();
    BudgetsAndStaticRefusals();
    MemoryErrorsAndReturnMasks();
    MappingMetadataAndDeterminism();
    std::printf("game_frame_ui_service_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
