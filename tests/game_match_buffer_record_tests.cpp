#include "game_match_buffer_record_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "match buffer record check %u failed at %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

struct Fixture {
  static constexpr std::uint32_t Ram = 0x80000000u;
  static constexpr std::uint32_t Sp = 0x800ff800u;
  static constexpr std::uint32_t Controllers = 0x80030000u;
  static constexpr std::uint32_t Ball = 0x80040000u;
  static constexpr std::uint32_t Entities = 0x80050000u;
  static constexpr std::uint32_t Buffer = 0x800f3000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::vector<Nba97GameMatchBufferRecordAccess> journal =
      std::vector<Nba97GameMatchBufferRecordAccess>(2048);
  Nba97GameMatchBufferRecordContext context{};
  Nba97GameMatchBufferRecordProgress progress{};
  std::array<Nba97GameMatchBufferRecordEvent, 4> events{};
  std::array<Nba97GameMatchBufferRecordMachine, 4> machines{};
  std::uint32_t compressionResult = Buffer + 0x100u;
  unsigned calls = 0;
  std::uint32_t refuseEntry = 0;
  unsigned invalidChild = 0;
  bool relocate = false;

  Fixture() {
    context.memory = {&region, 1};
    context.operation_budget = 4000;
    context.io = callback;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x31000000u + i, 15};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[29] = {Sp, 15};
    context.machine.registers.gpr[31] = {0x82345678u, 15};
    context.machine.hi = {0x11223344u, 3};
    context.machine.lo = {0x55667788u, 12};
    put(0x8002148cu, 0, 2);
    put(0x800f9ffcu, 0, 2);
    put(0x800fdb6cu, 0x1234u, 2);
    put(0x800fdb94u, 7, 2);
    for (unsigned i = 0; i < 8; ++i) {
      put(0x800fdc50u + i * 4u, Controllers + i * 0x40u, 4);
      put(Controllers + i * 0x40u + 0x26u, 0x120u + i, 2);
    }
    put(0x800fdbccu, 0x56, 2);
    put(0x800fdb58u, 0x12345678u, 4);
    put(0x800fdba4u, 0x87654321u, 4);
    put(0x800fdb90u, 0x89, 2);
    put(0x8001ee46u, 0x9a, 2);
    put(0x8001ef0au, 0xab, 2);
    put(0x800fdc38u, 0xdecafbadu, 4);
    for (unsigned i = 0; i < 4; ++i)
      put(0x800b7a00u + i * 4u, 0x40u + i, 4);
    put(0x801076e6u, 0xabc0u, 2);
    put(0x80108a0au, 0xdef0u, 2);
    put(0x80020c14u, Ball, 4);
    put(Ball + 0x14u, 0x1111u, 2);
    put(Ball + 0x16u, 0x2222u, 2);
    put(Ball + 0x18u, 0x3333u, 2);
    put(0x800fe860u, 0x44556677u, 4);
    put(0x800fe864u, 0xee, 1);
    put(0x80020becu, Entities, 4);
    for (unsigned entity = 0; entity < 11; ++entity) {
      const auto base = Entities + entity * 0xf4u;
      put(base + 8u, 0x00012300u + entity * 0x100u, 4);
      put(base + 0x0cu, 0xfffffe00u - entity * 0x100u, 4);
      put(base + 0x10u, 0x00034500u + entity * 0x100u, 4);
      for (unsigned field = 0; field < 0x30; field += 2)
        put(base + 0x74u + field, 0x80u + entity + field, 2);
      put(base + 0xa8u, 0x03fcu + entity * 4u, 2);
    }
    put(0x800fa004u, Buffer, 4);
    put(0x800fa008u, Buffer + 0x1000u, 4);
    put(0x800fa00cu, Buffer, 4);
    put(0x800fa010u, Buffer, 4);
    put(0x800fa014u, 0, 4);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width,
           std::uint8_t mask = 15) {
    const auto offset = static_cast<std::size_t>(address - Ram);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
      known[offset + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
    }
  }

  std::uint32_t get(std::uint32_t address, unsigned width = 1) const {
    const auto offset = static_cast<std::size_t>(address - Ram);
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[offset + i]) << (i * 8u);
    return value;
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameMatchBufferRecordEvent *event,
                      Nba97GameMatchBufferRecordMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    const unsigned index = f.calls++;
    f.events[index] = *event;
    f.machines[index] = *machine;
    if (f.refuseEntry == event->entry)
      return 0;
    if (f.invalidChild == 1) {
      machine->hi.known_mask = 16;
      return 1;
    }
    if (f.invalidChild == 2) {
      machine->lo.known_mask = 16;
      return 1;
    }
    if (f.invalidChild == 3) {
      machine->registers.gpr[0].known_mask = 14;
      return 1;
    }
    if (f.invalidChild == 4) {
      machine->registers.gpr[8].known_mask = 16;
      return 1;
    }
    if (event->entry == 0x800767fcu) {
      machine->registers.gpr[2] = {f.compressionResult, 15};
      if (f.relocate) {
        machine->registers.gpr[29] = {0x8010f000u, 15};
        machine->hi = {0x01020304u, 5};
        machine->lo = {0xa1a2a3a4u, 10};
        f.put(0x8010f010u, 0x83456780u, 4);
      }
    }
    return 1;
  }

  int run() { return nba97_game_match_buffer_record(&context, &progress); }
};

bool sameWord(const Nba97GameMatchBufferRecordWord &left,
              const Nba97GameMatchBufferRecordWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool sameMachine(const Nba97GameMatchBufferRecordMachine &left,
                 const Nba97GameMatchBufferRecordMachine &right) {
  for (unsigned i = 0; i < 32; ++i)
    if (!sameWord(left.registers.gpr[i], right.registers.gpr[i]))
      return false;
  return sameWord(left.hi, right.hi) && sameWord(left.lo, right.lo);
}

void normalSnapshotAndEntities() {
  Fixture f;
  for (unsigned reg = 8; reg < 29; ++reg)
    f.context.machine.registers.gpr[reg].known_mask =
        static_cast<std::uint8_t>((reg % 15u) + 1u);
  f.context.machine.registers.gpr[30].known_mask = 6;
  const auto incoming = f.context.machine;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
  const auto snapshot = 0x800f1814u;
  check(f.calls == 1 && f.events[0].pc == 0x80076e58u &&
        f.events[0].delay_slot_pc == 0x80076e5cu &&
        f.events[0].entry == 0x800767fcu && f.events[0].argument_count == 4 &&
        f.machines[0].registers.gpr[4].word == 0x800f1814u &&
        f.machines[0].registers.gpr[5].word == 0x800f1918u &&
        f.machines[0].registers.gpr[6].word == Fixture::Buffer &&
        f.machines[0].registers.gpr[7].word == 0x82u &&
        f.machines[0].registers.gpr[31].word == 0x80076e60u);
  check(f.get(snapshot + 9u) == 0x34u && f.get(snapshot + 8u) == 7u);
  for (unsigned i = 0; i < 8; ++i)
    check(f.get(snapshot + 0x0au + i) == 0x20u + i);
  check(f.get(snapshot + 0x12u) == 0x56u &&
        f.get(snapshot + 0x14u, 2) == 0x5678u &&
        f.get(snapshot + 0x16u, 2) == 0x4321u &&
        f.get(snapshot + 0x1eu) == 0x89u && f.get(snapshot + 0x1fu) == 0x9au &&
        f.get(snapshot + 0x20u) == 0xabu &&
        f.get(snapshot + 4u, 4) == 0xdecafbadu &&
        f.get(snapshot + 0x21u) == 0x40u && f.get(snapshot + 0x24u) == 0x43u &&
        f.get(snapshot + 0x25u) == 0xbcu && f.get(snapshot + 0x26u) == 0xefu);
  check(f.get(snapshot + 0x18u, 2) == 0x1111u &&
        f.get(snapshot + 0x1au, 2) == 0x2222u &&
        f.get(snapshot + 0x1cu, 2) == 0x3333u &&
        f.get(snapshot, 4) == 0x44556677u && f.get(0x800fe860u, 4) == 0);
  for (unsigned entity = 0; entity < 11; ++entity) {
    const auto record = snapshot + 0x28u + entity * 0x14u;
    check(f.get(record, 2) == 0x123u + entity &&
          f.get(record + 2u, 2) ==
              static_cast<std::uint16_t>(-2 - int(entity)) &&
          f.get(record + 4u, 2) == 0x345u + entity);
    if (entity < 10) {
      const std::array<unsigned, 13> expected{
          0x90u + entity, 0x98u + entity, 0x92u + entity,
          0x9au + entity, 0x94u + entity, 0x9cu + entity,
          0x96u + entity, 0x9eu + entity, 0xa0u + entity,
          0xa2u + entity, 0xffu + entity, (0xa4u + entity) >> 2u,
          0xa6u + entity};
      for (unsigned field = 0; field < expected.size(); ++field)
        check(f.get(record + 6u + field) == (expected[field] & 255u));
    } else {
      for (unsigned field = 0; field < 13; ++field)
        check(f.get(record + 6u + field) == 0xa5u);
    }
  }
  check(f.progress.entity_iterations == 11 &&
        f.get(0x800fa010u, 4) == f.compressionResult &&
        f.get(0x800fe864u) == 0 &&
        f.progress.machine.registers.gpr[29].word == Fixture::Sp &&
        f.progress.restored_return_address.word == 0x82345678u &&
        f.progress.machine.hi.word == 0x11223344u &&
        f.progress.machine.hi.known_mask == 3);
  for (unsigned reg = 0; reg < 32; ++reg) {
    const bool sourceMutates = reg <= 7 || reg == 29 || reg == 31;
    if (!sourceMutates)
      check(sameWord(f.progress.machine.registers.gpr[reg],
                     incoming.registers.gpr[reg]));
  }
  check(sameWord(f.progress.machine.hi, incoming.hi) &&
        sameWord(f.progress.machine.lo, incoming.lo));
}

void switchesClampRewindAndMutation() {
  Fixture alternate;
  alternate.put(0x800f9ffcu, 1, 2);
  alternate.put(0x800fdb94u, 0xffffu, 2);
  alternate.relocate = true;
  check(alternate.run() == NBA97_TEXT_COMPLETE &&
        alternate.get(0x800f1918u + 8u) == 0 &&
        alternate.machines[0].registers.gpr[4].word == 0x800f1918u &&
        alternate.machines[0].registers.gpr[5].word == 0x800f1814u &&
        alternate.progress.machine.registers.gpr[29].word == 0x8010f018u &&
        alternate.progress.restored_return_address.word == 0x83456780u &&
        alternate.progress.machine.hi.word == 0x01020304u &&
        alternate.progress.machine.lo.word == 0xa1a2a3a4u);

  Fixture rewind;
  rewind.put(0x8002148cu, 1, 2);
  check(rewind.run() == NBA97_TEXT_COMPLETE && rewind.calls == 2 &&
        rewind.progress.used_rewind && rewind.events[0].pc == 0x80076b50u &&
        rewind.events[0].delay_slot_pc == 0x80076b54u &&
        rewind.events[0].entry == 0x80076ad0u &&
        rewind.events[0].argument_count == 0 &&
        rewind.machines[0].registers.gpr[31].word == 0x80076b58u);
  Fixture refusedRewind;
  refusedRewind.put(0x8002148cu, 1, 2);
  refusedRewind.refuseEntry = 0x80076ad0u;
  check(refusedRewind.run() == NBA97_TEXT_IO_REFUSED &&
        refusedRewind.progress.stopped_pc == 0x80076b50u &&
        refusedRewind.progress.machine.registers.gpr[31].word == 0x80076b58u);
  Fixture refusedCompression;
  refusedCompression.refuseEntry = 0x800767fcu;
  check(refusedCompression.run() == NBA97_TEXT_IO_REFUSED &&
        refusedCompression.progress.stopped_pc == 0x80076e58u &&
        refusedCompression.progress.machine.registers.gpr[7].word == 0x82u);
  Fixture invalid;
  invalid.invalidChild = 1;
  check(invalid.run() == NBA97_TEXT_ARGUMENT &&
        invalid.progress.machine.hi.known_mask == 16);
}

void CursorRoutesAndLengths() {
  Fixture publish;
  publish.put(0x800fa014u, Fixture::Buffer + 0x200u, 4);
  publish.put(0x800fa00cu, Fixture::Buffer + 0x180u, 4);
  publish.compressionResult = Fixture::Buffer;
  check(publish.run() == NBA97_TEXT_COMPLETE &&
        publish.get(0x800fa00cu, 4) == Fixture::Buffer + 0x180u);

  for (auto length : {0u, 21u, 22u, 34u, 255u}) {
    Fixture forward;
    forward.put(0x800fa014u, Fixture::Buffer + length, 4);
    forward.put(0x800fa00cu, Fixture::Buffer, 4);
    forward.compressionResult = Fixture::Buffer;
    forward.put(Fixture::Buffer, length, 1);
    check(forward.run() == NBA97_TEXT_COMPLETE &&
          forward.get(0x800fa00cu, 4) == Fixture::Buffer);
  }

  Fixture markerMismatch;
  markerMismatch.put(0x800fa014u, Fixture::Buffer + 200u, 4);
  markerMismatch.put(0x800fa00cu, Fixture::Buffer, 4);
  markerMismatch.compressionResult = Fixture::Buffer;
  markerMismatch.put(Fixture::Buffer, 21u, 1);
  markerMismatch.put(Fixture::Buffer + 199u, 40u, 1);
  markerMismatch.put(Fixture::Buffer + 160u, 41u, 1);
  check(markerMismatch.run() == NBA97_TEXT_COMPLETE &&
        markerMismatch.get(0x800fa00cu, 4) == Fixture::Buffer);
  bool sawMismatchMarker = false;
  bool sawMismatchFollowup = false;
  for (std::size_t i = 0; i < markerMismatch.progress.access_events; ++i) {
    sawMismatchMarker |= markerMismatch.journal[i].pc == 0x80076f48u;
    sawMismatchFollowup |= markerMismatch.journal[i].pc == 0x80076f14u;
  }
  check(sawMismatchMarker && !sawMismatchFollowup);

  Fixture markerEquality;
  markerEquality.put(0x800fa014u, Fixture::Buffer + 200u, 4);
  markerEquality.put(0x800fa00cu, Fixture::Buffer, 4);
  markerEquality.compressionResult = Fixture::Buffer;
  markerEquality.put(Fixture::Buffer, 22u, 1);
  markerEquality.put(Fixture::Buffer + 199u, 40u, 1);
  markerEquality.put(Fixture::Buffer + 160u, 40u, 1);
  markerEquality.put(Fixture::Buffer + 159u, 5u, 1);
  markerEquality.put(Fixture::Buffer + 155u, 6u, 1);
  check(markerEquality.run() == NBA97_TEXT_COMPLETE &&
        markerEquality.get(0x800fa00cu, 4) == Fixture::Buffer);
  bool sawEqualityFollowup = false;
  for (std::size_t i = 0; i < markerEquality.progress.access_events; ++i)
    sawEqualityFollowup |= markerEquality.journal[i].pc == 0x80076f14u;
  check(sawEqualityFollowup);

  for (const std::size_t budget : {430u, 431u, 500u}) {
    Fixture runaway;
    runaway.put(0x800fa014u, Fixture::Buffer + 0x600u, 4);
    runaway.put(0x800fa00cu, Fixture::Buffer + 0xc0u, 4);
    runaway.compressionResult = Fixture::Buffer + 0x100u;
    for (unsigned offset = 0; offset < 0x1000; ++offset)
      runaway.put(Fixture::Buffer + offset, 40u, 1);
    runaway.put(Fixture::Buffer + 0xc0u, 21u, 1);
    runaway.context.operation_budget = budget;
    check(runaway.run() == NBA97_TEXT_LIMIT &&
          runaway.progress.operations == budget &&
          runaway.get(0x800fa00cu, 4) == Fixture::Buffer + 0xc0u);
  }

  Fixture limit;
  limit.put(0x800fa014u, Fixture::Buffer + 0x500u, 4);
  limit.put(0x800fa00cu, Fixture::Buffer + 0x400u, 4);
  limit.compressionResult = Fixture::Buffer;
  check(limit.run() == NBA97_TEXT_COMPLETE &&
        limit.get(0x800fa00cu, 4) == Fixture::Buffer + 0x400u);

  Fixture wrap;
  wrap.put(0x800fa008u, 0x10u, 4);
  wrap.compressionResult = 0xfffffff0u;
  check(wrap.run() == NBA97_TEXT_COMPLETE &&
        wrap.get(0x800fa014u, 4) == 0xfffffff0u &&
        wrap.get(0x800fa010u, 4) == Fixture::Buffer);
}

void AtomicKnownnessAliasesAndDeterminism() {
  Fixture malformedSecondByte;
  const auto snapshotByte = malformedSecondByte.get(0x800f1814u + 9u);
  malformedSecondByte.known[0x800fdb6cu - Fixture::Ram + 1u] = 2;
  check(malformedSecondByte.run() == NBA97_TEXT_ARGUMENT &&
        malformedSecondByte.progress.stopped_pc == 0x80076b80u &&
        malformedSecondByte.progress.machine.registers.gpr[2].word == 0 &&
        malformedSecondByte.get(0x800f1814u + 9u) == snapshotByte);

  Fixture untrackedStack;
  const std::uint32_t stackStore = Fixture::Sp - 8u;
  const auto split = static_cast<std::size_t>(stackStore - Fixture::Ram);
  std::array<Nba97GameTextRegion, 3> regions{{
      {Fixture::Ram, untrackedStack.bytes.data(), untrackedStack.known.data(),
       split},
      {stackStore, untrackedStack.bytes.data() + split, nullptr, 4},
      {stackStore + 4u, untrackedStack.bytes.data() + split + 4u,
       untrackedStack.known.data() + split + 4u,
       untrackedStack.bytes.size() - split - 4u},
  }};
  untrackedStack.context.memory = {regions.data(), regions.size()};
  untrackedStack.context.machine.registers.gpr[31].known_mask = 7;
  const auto beforeStack = untrackedStack.get(stackStore, 4);
  check(untrackedStack.run() == NBA97_TEXT_ARGUMENT &&
        untrackedStack.progress.stopped_pc == 0x80076b4cu &&
        untrackedStack.get(stackStore, 4) == beforeStack);

  Fixture alias;
  alias.put(0x800fdc50u, Fixture::Controllers, 4);
  alias.put(Fixture::Controllers + 0x26u, 0x42u, 2);
  alias.put(0x800fdc54u, 0x800f1814u + 0x0au - 0x26u, 4);
  check(alias.run() == NBA97_TEXT_COMPLETE &&
        alias.get(0x800f1814u + 0x0au) == 0x42u &&
        alias.get(0x800f1814u + 0x0bu) == 0x42u);

  Fixture stackWrap;
  std::array<std::uint8_t, 4> lowBytes{};
  std::array<std::uint8_t, 4> lowKnown{};
  lowKnown.fill(1);
  std::array<Nba97GameTextRegion, 2> wrapRegions{{
      {0, lowBytes.data(), lowKnown.data(), lowBytes.size()},
      stackWrap.region,
  }};
  stackWrap.context.memory = {wrapRegions.data(), wrapRegions.size()};
  stackWrap.context.machine.registers.gpr[29] = {8u, 15};
  check(stackWrap.run() == NBA97_TEXT_COMPLETE &&
        stackWrap.progress.frame_stack_pointer == 0xfffffff0u &&
        stackWrap.progress.machine.registers.gpr[29].word == 8u &&
        stackWrap.progress.restored_return_address.word == 0x82345678u);
  check(lowBytes[0] == 0x78u && lowBytes[1] == 0x56u && lowBytes[2] == 0x34u &&
        lowBytes[3] == 0x82u);

  Fixture first;
  Fixture second;
  check(first.run() == NBA97_TEXT_COMPLETE &&
        second.run() == NBA97_TEXT_COMPLETE && first.bytes == second.bytes &&
        first.known == second.known &&
        sameMachine(first.progress.machine, second.progress.machine));
}

void BudgetsKnownnessMappingsAndReturn() {
  Fixture complete;
  check(complete.run() == NBA97_TEXT_COMPLETE);
  for (std::size_t budget = 0; budget < complete.progress.operations;
       ++budget) {
    Fixture prefix;
    prefix.context.operation_budget = budget;
    check(prefix.run() == NBA97_TEXT_LIMIT &&
          prefix.progress.operations == budget);
  }
  Fixture unknownSwitch;
  unknownSwitch.put(0x8002148cu, 0, 2, 1);
  check(unknownSwitch.run() == NBA97_TEXT_UNKNOWN &&
        unknownSwitch.progress.stopped_pc == 0x80076b48u &&
        unknownSwitch.progress.stores == 1);
  Fixture unknownClock;
  unknownClock.put(0x800fdb94u, 0, 2, 1);
  check(unknownClock.run() == NBA97_TEXT_UNKNOWN &&
        unknownClock.progress.stopped_pc == 0x80076b98u);
  Fixture unknownPointer;
  unknownPointer.put(0x800fdc50u, Fixture::Controllers, 4, 7);
  check(unknownPointer.run() == NBA97_TEXT_UNKNOWN &&
        unknownPointer.progress.stopped_pc == 0x80076bb8u);
  Fixture malformed;
  malformed.known[0x800fdb6cu - Fixture::Ram] = 2;
  check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x80076b80u);
  Fixture missing;
  missing.region.size = 0x100u;
  check(missing.run() == NBA97_TEXT_RESOURCE);
  Fixture overlap;
  std::array<Nba97GameTextRegion, 2> regions{{
      overlap.region,
      {Fixture::Ram + 4u, overlap.bytes.data() + 4u, overlap.known.data() + 4u,
       8},
  }};
  overlap.context.memory = {regions.data(), regions.size()};
  check(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture oversized;
  oversized.region.size = static_cast<std::size_t>(-1);
  check(oversized.run() == NBA97_TEXT_ARGUMENT);
  Fixture badMask;
  badMask.context.machine.lo.known_mask = 16;
  check(badMask.run() == NBA97_TEXT_ARGUMENT);
  check(nba97_game_match_buffer_record(nullptr, &badMask.progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_match_buffer_record(&badMask.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);

  Fixture unknownRa;
  unknownRa.context.machine.registers.gpr[31].known_mask = 7;
  check(unknownRa.run() == NBA97_TEXT_UNKNOWN &&
        unknownRa.progress.stopped_pc == 0x80076fc0u &&
        unknownRa.progress.machine.registers.gpr[29].word == Fixture::Sp);
  Fixture misalignedRa;
  misalignedRa.context.machine.registers.gpr[31].word |= 1u;
  check(misalignedRa.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misalignedRa.progress.stopped_pc == 0x80076fc0u);
}

void AdapterGuardsAndAcceptedMalformedChildren() {
  Nba97GamePeriodStartupEvent exact{0x800674f8u,
                                    0x800674fcu,
                                    0x80076b3cu,
                                    16,
                                    NBA97_GAME_PERIOD_STARTUP_76B3C,
                                    0};
  {
    Fixture fixture;
    fixture.region.size = static_cast<std::size_t>(-1);
    Nba97GameMatchBufferRecordBinding binding{};
    nba97_game_match_buffer_record_binding_init(&binding, 4000, 20, 20);
    auto registers = fixture.context.machine.registers;
    registers.gpr[31] = {0x80067500u, 15};
    check(nba97_game_match_buffer_record_from_period_startup(
              &binding, &fixture.context.memory, &exact, &registers) == 0 &&
          binding.result == NBA97_TEXT_ARGUMENT);
  }
  for (unsigned field = 0; field < 6; ++field) {
    Fixture fixture;
    Nba97GameMatchBufferRecordBinding binding{};
    nba97_game_match_buffer_record_binding_init(&binding, 4000, 20, 20);
    binding.io = Fixture::callback;
    binding.user = &fixture;
    auto event = exact;
    auto registers = fixture.context.machine.registers;
    registers.gpr[31] = {0x80067500u, 15};
    if (field == 0)
      event.pc ^= 4u;
    else if (field == 1)
      event.delay_slot_pc ^= 4u;
    else if (field == 2)
      event.entry ^= 4u;
    else if (field == 3)
      event.kind = NBA97_GAME_PERIOD_STARTUP_A584C;
    else if (field == 4)
      event.argument_count = 1;
    else
      registers.gpr[31].word ^= 4u;
    check(nba97_game_match_buffer_record_from_period_startup(
              &binding, &fixture.context.memory, &event, &registers) == 0 &&
          binding.result == NBA97_TEXT_ARGUMENT && binding.invocations == 0);
  }

  for (unsigned invalid = 1; invalid <= 4; ++invalid) {
    Fixture fixture;
    fixture.invalidChild = invalid;
    Nba97GameMatchBufferRecordBinding binding{};
    nba97_game_match_buffer_record_binding_init(&binding, 4000, 20, 20);
    binding.io = Fixture::callback;
    binding.user = &fixture;
    auto registers = fixture.context.machine.registers;
    registers.gpr[31] = {0x80067500u, 15};
    check(nba97_game_match_buffer_record_from_period_startup(
              &binding, &fixture.context.memory, &exact, &registers) == 0 &&
          binding.result == NBA97_TEXT_ARGUMENT && binding.invocations == 1 &&
          binding.completions == 0);
    if (invalid == 1 || invalid == 2)
      check(registers.gpr[0].word == 0 && registers.gpr[0].known_mask == 15);
  }
}
} // namespace

int main() {
  normalSnapshotAndEntities();
  switchesClampRewindAndMutation();
  CursorRoutesAndLengths();
  BudgetsKnownnessMappingsAndReturn();
  AdapterGuardsAndAcceptedMalformedChildren();
  AtomicKnownnessAliasesAndDeterminism();
  std::printf("game match buffer record: %u checks\n", checks);
}
