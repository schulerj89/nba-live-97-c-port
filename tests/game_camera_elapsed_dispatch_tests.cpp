#include "recovered/game_camera_elapsed_dispatch.h"

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
    std::fprintf(stderr, "camera elapsed dispatch check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define CHECK(value) checkAt((value), __LINE__)

bool sameWord(const Nba97GameCameraElapsedDispatchWord &left,
              const Nba97GameCameraElapsedDispatchWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool sameMachine(const Nba97GameCameraElapsedDispatchMachine &left,
                 const Nba97GameCameraElapsedDispatchMachine &right) {
  for (unsigned index = 0; index < 32; ++index)
    if (!sameWord(left.registers.gpr[index], right.registers.gpr[index]))
      return false;
  return sameWord(left.hi, right.hi) && sameWord(left.lo, right.lo);
}

struct Fixture {
  static constexpr std::uint32_t Ram = UINT32_C(0x80000000);
  static constexpr std::uint32_t Sp = UINT32_C(0x801ff000);
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::vector<Nba97GameCameraElapsedDispatchAccess> journal =
      std::vector<Nba97GameCameraElapsedDispatchAccess>(256);
  Nba97GameCameraElapsedDispatchContext context{};
  Nba97GameCameraElapsedDispatchProgress progress{};
  std::vector<Nba97GameCameraElapsedDispatchEvent> events;
  std::vector<Nba97GameCameraElapsedDispatchMachine> machines;
  std::uint32_t refusePc = 0;
  std::uint32_t mutatePc = 0;
  unsigned invalidKind = 0;
  Nba97GameCameraElapsedDispatchWord probeReturn{0, 15};
  Nba97GameCameraElapsedDispatchWord refreshReturn{42, 15};
  std::uint8_t mutationRaMask = 15;

  Fixture() {
    context.memory = {&region, 1};
    context.operation_budget = 500;
    context.io = callback;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned index = 0; index < 32; ++index)
      context.machine.registers.gpr[index] = {
          UINT32_C(0x31000000) + index,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[4] = {UINT32_MAX, 15};
    context.machine.registers.gpr[29] = {Sp, 15};
    context.machine.registers.gpr[31] = {UINT32_C(0x81234568), 15};
    context.machine.hi = {UINT32_C(0x11223344), 5};
    context.machine.lo = {UINT32_C(0x55667788), 10};
    put(0x800bc1f4, UINT32_MAX);
    put(0x800bc1f8, 10);
    put(0x800bc1fc, 100);
    put(0x800bc200, 1);
    put(0x800fc9d0, 0x80010000);
    put(0x8001005c, 0x80020000);
    put(0x80106074, 20);
    put(0x800d8eec, 0);
  }

  std::size_t at(std::uint32_t address) const { return address - Ram; }

  void put(std::uint32_t address, std::uint32_t value, unsigned width = 4,
           std::uint8_t mask = 15) {
    for (unsigned byte = 0; byte < width; ++byte) {
      bytes[at(address) + byte] =
          static_cast<std::uint8_t>(value >> (byte * 8u));
      known[at(address) + byte] =
          static_cast<std::uint8_t>((mask >> byte) & 1u);
    }
  }

  std::uint32_t get(std::uint32_t address, unsigned width = 4) const {
    std::uint32_t result = 0;
    for (unsigned byte = 0; byte < width; ++byte)
      result |= std::uint32_t(bytes[at(address) + byte]) << (byte * 8u);
    return result;
  }

  std::uint8_t getKnown(std::uint32_t address, unsigned width = 4) const {
    std::uint8_t result = 0;
    for (unsigned byte = 0; byte < width; ++byte)
      if (known[at(address) + byte])
        result = static_cast<std::uint8_t>(result | (1u << byte));
    return result;
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameCameraElapsedDispatchEvent *event,
                      Nba97GameCameraElapsedDispatchMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.events.push_back(*event);
    fixture.machines.push_back(*machine);
    if (fixture.refusePc == event->pc)
      return 0;
    if (event->kind == NBA97_GAME_CAMERA_ELAPSED_DISPATCH_PROBE_8007A468)
      machine->registers.gpr[2] = fixture.probeReturn;
    if (event->kind == NBA97_GAME_CAMERA_ELAPSED_DISPATCH_REFRESH_8007A410)
      machine->registers.gpr[2] = fixture.refreshReturn;
    if (fixture.invalidKind == 1)
      machine->registers.gpr[0] = {1, 15};
    else if (fixture.invalidKind == 2)
      machine->registers.gpr[8].known_mask = 16;
    else if (fixture.invalidKind == 3)
      machine->hi.known_mask = 16;
    else if (fixture.invalidKind == 4)
      machine->lo.known_mask = 16;
    if (fixture.mutatePc == event->pc) {
      for (unsigned index = 1; index < 32; ++index)
        machine->registers.gpr[index] = {
            UINT32_C(0x62000000) + index,
            static_cast<std::uint8_t>((index % 15u) + 1u)};
      machine->registers.gpr[29] = {UINT32_C(0x801fe000), 15};
      machine->hi = {UINT32_C(0x10203040), 6};
      machine->lo = {UINT32_C(0xa0b0c0d0), 9};
      fixture.put(0x801fe010, 0x82345678, 4, fixture.mutationRaMask);
      fixture.put(0x800bc1f4, 77);
    }
    return 1;
  }

  int run() { return nba97_game_camera_elapsed_dispatch(&context, &progress); }
};

void expectEvent(const Fixture &fixture, unsigned index, std::uint32_t pc,
                 std::uint32_t entry, unsigned invocation, unsigned kind) {
  const auto &event = fixture.events[index];
  const auto &machine = fixture.machines[index];
  CHECK(event.pc == pc && event.delay_slot_pc == pc + 4u &&
        event.entry == entry && event.argument_count == 0 &&
        event.invocation == invocation && event.kind == kind &&
        machine.registers.gpr[31].word == pc + 8u &&
        machine.registers.gpr[31].known_mask == 15);
}

void SentinelRefreshAndMachine() {
  Fixture fixture;
  const auto incoming = fixture.context.machine;
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE && fixture.progress.completed &&
        fixture.progress.elapsed_reset && fixture.get(0x80106074) == 0 &&
        fixture.get(0x800bc1f4) == 42 && fixture.get(0x800d8eec) == 42);
  CHECK(fixture.events.size() == 1 &&
        fixture.progress.callbacks_completed == 1 &&
        fixture.progress.instruction_count == 48 &&
        fixture.progress.frame_stack_pointer == Fixture::Sp - 0x18 &&
        fixture.progress.restored_return_address.word == 0x81234568 &&
        fixture.progress.machine.registers.gpr[29].word == Fixture::Sp);
  expectEvent(fixture, 0, 0x8007999c, 0x8007a410, 1,
              NBA97_GAME_CAMERA_ELAPSED_DISPATCH_REFRESH_8007A410);
  const std::array<std::uint32_t, 14> pcs{
      0x800798c0, 0x800798c8, 0x800798e8, 0x800798f0, 0x800798f8,
      0x80079918, 0x80079920, 0x80079938, 0x80079968, 0x800799a8,
      0x800799b0, 0x800799b8, 0x800799bc, 0};
  CHECK(fixture.progress.access_events == pcs.size() - 1);
  for (unsigned index = 0; index + 1 < pcs.size(); ++index)
    CHECK(fixture.journal[index].pc == pcs[index]);
  CHECK(sameWord(fixture.progress.machine.hi, incoming.hi) &&
        sameWord(fixture.progress.machine.lo, incoming.lo) &&
        sameWord(fixture.progress.machine.registers.gpr[4],
                 incoming.registers.gpr[4]));
  for (unsigned index = 5; index < 29; ++index)
    CHECK(sameWord(fixture.progress.machine.registers.gpr[index],
                   incoming.registers.gpr[index]));
  CHECK(sameWord(fixture.progress.machine.registers.gpr[30],
                 incoming.registers.gpr[30]));
}

void DeltaClampBoundsAndEarlyExit() {
  struct Case {
    std::uint32_t elapsed;
    std::uint32_t a0;
    std::uint32_t lower;
    std::uint32_t upper;
    std::uint32_t expectedElapsed;
    bool exits;
  };
  const std::array<Case, 11> cases{{
      {20, 1, 10, 100, 21, false},
      {20, 0, 10, 100, 20, false},
      {20, UINT32_C(0xfffffffe), 10, 100, 18, false},
      {UINT32_MAX, 1, 0, 100, 0, false},
      {UINT32_C(0x80000000), UINT32_C(0x80000000), 0, 100, 0, false},
      {99, 1, 10, 100, 100, false},
      {100, 1, 10, 100, 100, false},
      {9, 0, 10, 100, 9, true},
      {20, UINT32_C(0xffffffff), 10, 100, 10, false},
      {0, UINT32_C(0xffffffff), UINT32_C(0xfffffffb), 100, UINT32_C(0xfffffffb),
       false},
      {20, 0, 50, 40, 20, true},
  }};
  for (const auto &test : cases) {
    Fixture fixture;
    fixture.context.machine.registers.gpr[4] = {test.a0, 15};
    fixture.put(0x80106074, test.elapsed);
    fixture.put(0x800bc1f8, test.lower);
    fixture.put(0x800bc1fc, test.upper);
    const int result = fixture.run();
    CHECK(result == NBA97_TEXT_COMPLETE && fixture.progress.completed &&
          fixture.progress.elapsed_reset ==
              static_cast<unsigned>(!test.exits) &&
          fixture.get(0x80106074) == (test.exits ? test.expectedElapsed : 0u));
    if (test.exits)
      CHECK(fixture.events.empty() && fixture.get(0x800d8eec) == 0 &&
            fixture.progress.machine.registers.gpr[2].word == 1);
  }

  Fixture decidedNonSentinel;
  decidedNonSentinel.context.machine.registers.gpr[4] = {0, 1};
  CHECK(decidedNonSentinel.run() == NBA97_TEXT_UNKNOWN &&
        decidedNonSentinel.progress.stopped_pc == 0x80079904 &&
        decidedNonSentinel.progress.requested_delta.known_mask == 1);
  Fixture unknownArgument;
  unknownArgument.context.machine.registers.gpr[4] = {UINT32_MAX, 14};
  CHECK(unknownArgument.run() == NBA97_TEXT_UNKNOWN &&
        unknownArgument.progress.stopped_pc == 0x800798bc &&
        unknownArgument.progress.stores == 1);

  Fixture minimumAgainstPartial;
  minimumAgainstPartial.context.machine.registers.gpr[4] = {0, 15};
  minimumAgainstPartial.put(0x80106074, 1, 4, 1);
  minimumAgainstPartial.put(0x800bc1f8, UINT32_C(0x80000000));
  minimumAgainstPartial.put(0x800bc1fc, UINT32_C(0x80000000));
  CHECK(minimumAgainstPartial.run() == NBA97_TEXT_COMPLETE &&
        minimumAgainstPartial.progress.completed &&
        minimumAgainstPartial.progress.stopped_pc == 0);

  Fixture maximumAgainstPartial;
  maximumAgainstPartial.context.machine.registers.gpr[4] = {0, 15};
  maximumAgainstPartial.put(0x80106074, UINT32_C(0x7fffffff));
  maximumAgainstPartial.put(0x800bc1f8, UINT32_C(0x80000000));
  maximumAgainstPartial.put(0x800bc1fc, UINT32_C(0x000000fe), 4, 1);
  CHECK(maximumAgainstPartial.run() == NBA97_TEXT_COMPLETE &&
        maximumAgainstPartial.progress.completed);
  bool foundPartialClamp = false;
  for (std::size_t index = 0;
       index < maximumAgainstPartial.progress.access_events; ++index)
    if (maximumAgainstPartial.journal[index].pc == 0x80079910) {
      foundPartialClamp = true;
      CHECK(maximumAgainstPartial.journal[index].value == 0xfe &&
            maximumAgainstPartial.journal[index].known_mask == 1);
    }
  CHECK(foundPartialClamp);
}

void IndirectProbeRefreshPaths() {
  Fixture allCalls;
  allCalls.put(0x800bc200, 0);
  allCalls.put(0x800bc1f4, 55);
  const int allCallsResult = allCalls.run();
  CHECK(allCallsResult == NBA97_TEXT_COMPLETE && allCalls.events.size() == 3 &&
        allCalls.get(0x800d8eec) == 42);
  expectEvent(allCalls, 0, 0x8007995c, 0x80020000, 1,
              NBA97_GAME_CAMERA_ELAPSED_DISPATCH_INDIRECT_8007995C);
  expectEvent(allCalls, 1, 0x80079978, 0x8007a468, 1,
              NBA97_GAME_CAMERA_ELAPSED_DISPATCH_PROBE_8007A468);
  expectEvent(allCalls, 2, 0x8007999c, 0x8007a410, 1,
              NBA97_GAME_CAMERA_ELAPSED_DISPATCH_REFRESH_8007A410);

  for (std::uint32_t probe : {UINT32_C(0), UINT32_C(0x100), UINT32_C(0xff)}) {
    Fixture fixture;
    fixture.put(0x800bc1f4, 55);
    fixture.probeReturn = {probe, 15};
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE &&
          fixture.get(0x800d8eec) == ((probe & 0xffu) ? 55u : 42u) &&
          fixture.events.size() == ((probe & 0xffu) ? 1u : 2u));
  }

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture fixture;
    fixture.put(0x800bc1f4, 55);
    fixture.probeReturn = {0, static_cast<std::uint8_t>(mask)};
    const int result = fixture.run();
    if (mask & 1u)
      CHECK(result == NBA97_TEXT_COMPLETE && fixture.events.size() == 2);
    else
      CHECK(result == NBA97_TEXT_UNKNOWN &&
            fixture.progress.stopped_pc == 0x80079984 &&
            fixture.progress.machine.registers.gpr[2].known_mask == 14);
  }
}

void DynamicTargetFailuresAndCallRefusals() {
  struct TargetCase {
    std::uint32_t target;
    std::uint8_t mask;
    int result;
  };
  const std::array<TargetCase, 3> targets{{
      {0x80020000, 14, NBA97_TEXT_UNKNOWN},
      {0, 15, NBA97_TEXT_RESOURCE},
      {0x80020002, 15, NBA97_TEXT_ALIGNMENT_TRAP},
  }};
  for (const auto &test : targets) {
    Fixture fixture;
    fixture.put(0x800bc200, 0);
    fixture.put(0x8001005c, test.target, 4, test.mask);
    CHECK(fixture.run() == test.result &&
          fixture.progress.stopped_pc == 0x8007995c &&
          fixture.progress.machine.registers.gpr[31].word == 0x80079964 &&
          fixture.progress.call_attempts
                  [NBA97_GAME_CAMERA_ELAPSED_DISPATCH_INDIRECT_8007995C] == 1);
  }

  Fixture unknownDescriptor;
  unknownDescriptor.put(0x800bc200, 0);
  unknownDescriptor.put(0x800fc9d0, 0x80010000, 4, 14);
  CHECK(unknownDescriptor.run() == NBA97_TEXT_UNKNOWN &&
        unknownDescriptor.progress.stopped_pc == 0x80079954);

  Fixture unalignedDescriptor;
  unalignedDescriptor.put(0x800bc200, 0);
  unalignedDescriptor.put(0x800fc9d0, 0x80010002);
  CHECK(unalignedDescriptor.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unalignedDescriptor.progress.stopped_pc == 0x80079954 &&
        unalignedDescriptor.progress.stopped_address == 0x8001005e);

  Fixture unmappedDescriptor;
  unmappedDescriptor.put(0x800bc200, 0);
  unmappedDescriptor.put(0x800fc9d0, 0x80200000);
  CHECK(unmappedDescriptor.run() == NBA97_TEXT_RESOURCE &&
        unmappedDescriptor.progress.stopped_pc == 0x80079954 &&
        unmappedDescriptor.progress.stopped_address == 0x8020005c);

  Fixture wrappedDescriptor;
  std::array<std::uint8_t, 4> lowTarget{{0, 0, 2, 128}};
  std::array<std::uint8_t, 4> lowKnown{{1, 1, 1, 1}};
  std::array<Nba97GameTextRegion, 2> wrappedRegions{{
      {0, lowTarget.data(), lowKnown.data(), lowTarget.size()},
      wrappedDescriptor.region,
  }};
  wrappedDescriptor.context.memory = {wrappedRegions.data(),
                                      wrappedRegions.size()};
  wrappedDescriptor.put(0x800bc200, 0);
  wrappedDescriptor.put(0x800fc9d0, UINT32_C(0xffffffa4));
  CHECK(wrappedDescriptor.run() == NBA97_TEXT_COMPLETE &&
        wrappedDescriptor.events[0].pc == 0x8007995c &&
        wrappedDescriptor.events[0].entry == 0x80020000);

  for (std::uint32_t pc :
       {UINT32_C(0x8007995c), UINT32_C(0x80079978), UINT32_C(0x8007999c)}) {
    Fixture fixture;
    if (pc == 0x8007995c)
      fixture.put(0x800bc200, 0);
    if (pc == 0x80079978)
      fixture.put(0x800bc1f4, 55);
    fixture.refusePc = pc;
    CHECK(fixture.run() == NBA97_TEXT_IO_REFUSED &&
          fixture.progress.stopped_pc == pc);
  }
}

void UnknownBranchesAndAtomicMemory() {
  Fixture upper;
  upper.put(0x800bc1fc, 100, 4, 14);
  CHECK(upper.run() == NBA97_TEXT_UNKNOWN &&
        upper.progress.stopped_pc == 0x80079904 &&
        upper.progress.machine.registers.gpr[2].known_mask == 14);
  Fixture lower;
  lower.context.machine.registers.gpr[4] = {0, 15};
  lower.put(0x800bc1f8, 10, 4, 14);
  CHECK(lower.run() == NBA97_TEXT_UNKNOWN &&
        lower.progress.stopped_pc == 0x8007992c &&
        lower.progress.machine.registers.gpr[2].known_mask == 14);
  Fixture gate;
  gate.put(0x800bc200, 0, 4, 0);
  CHECK(gate.run() == NBA97_TEXT_UNKNOWN &&
        gate.progress.stopped_pc == 0x80079940);
  Fixture cached;
  cached.put(0x800bc1f4, 0, 4, 7);
  CHECK(cached.run() == NBA97_TEXT_UNKNOWN &&
        cached.progress.stopped_pc == 0x80079970);

  Fixture malformed;
  malformed.known[malformed.at(0x800bc1fc) + 2] = 2;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x800798f8 &&
        malformed.get(0x80106074) == 10 &&
        malformed.progress.machine.registers.gpr[3].word == 0x800c0000 &&
        malformed.progress.machine.registers.gpr[3].known_mask == 15);

  Fixture unknownStore;
  unknownStore.region.known = nullptr;
  unknownStore.context.machine.registers.gpr[31].known_mask = 7;
  const std::array<std::uint8_t, 4> before{
      unknownStore.bytes[unknownStore.at(Fixture::Sp - 8)],
      unknownStore.bytes[unknownStore.at(Fixture::Sp - 7)],
      unknownStore.bytes[unknownStore.at(Fixture::Sp - 6)],
      unknownStore.bytes[unknownStore.at(Fixture::Sp - 5)]};
  CHECK(unknownStore.run() == NBA97_TEXT_ARGUMENT &&
        unknownStore.progress.stopped_pc == 0x800798c0 &&
        unknownStore.bytes[unknownStore.at(Fixture::Sp - 8)] == before[0] &&
        unknownStore.bytes[unknownStore.at(Fixture::Sp - 7)] == before[1] &&
        unknownStore.bytes[unknownStore.at(Fixture::Sp - 6)] == before[2] &&
        unknownStore.bytes[unknownStore.at(Fixture::Sp - 5)] == before[3]);

  Fixture partialRefresh;
  partialRefresh.refreshReturn = {UINT32_C(0x12345678), 5};
  CHECK(partialRefresh.run() == NBA97_TEXT_COMPLETE &&
        partialRefresh.get(0x800bc1f4) == UINT32_C(0x12345678) &&
        partialRefresh.getKnown(0x800bc1f4) == 5 &&
        partialRefresh.get(0x800d8eec) == UINT32_C(0x12345678) &&
        partialRefresh.getKnown(0x800d8eec) == 5);

  Fixture unavailableKnownness;
  unavailableKnownness.region.known = nullptr;
  unavailableKnownness.refreshReturn = {UINT32_C(0x12345678), 5};
  const auto cachedBefore = unavailableKnownness.get(0x800bc1f4);
  const auto publicationBefore = unavailableKnownness.get(0x800d8eec);
  CHECK(unavailableKnownness.run() == NBA97_TEXT_ARGUMENT &&
        unavailableKnownness.progress.stopped_pc == 0x800799a8 &&
        unavailableKnownness.get(0x800bc1f4) == cachedBefore &&
        unavailableKnownness.get(0x800d8eec) == publicationBefore);
}

void BudgetsLiveStateAndReturnMasks() {
  Fixture complete;
  complete.put(0x800bc200, 0);
  CHECK(complete.run() == NBA97_TEXT_COMPLETE);
  for (std::size_t budget = 0; budget < complete.progress.operations;
       ++budget) {
    Fixture fixture;
    fixture.put(0x800bc200, 0);
    fixture.context.operation_budget = budget;
    CHECK(fixture.run() == NBA97_TEXT_LIMIT &&
          fixture.progress.operations == budget && !fixture.progress.completed);
  }

  Fixture live;
  live.put(0x800bc1f4, 55);
  live.probeReturn = {0xff, 15};
  live.mutatePc = 0x80079978;
  CHECK(live.run() == NBA97_TEXT_COMPLETE && live.get(0x800d8eec) == 77 &&
        live.progress.machine.registers.gpr[29].word == 0x801fe018 &&
        live.progress.restored_return_address.word == 0x82345678 &&
        live.progress.machine.hi.word == 0x10203040 &&
        live.progress.machine.hi.known_mask == 6 &&
        live.progress.machine.lo.word == 0xa0b0c0d0 &&
        live.progress.machine.lo.known_mask == 9);
  for (unsigned index = 3; index < 29; ++index)
    CHECK(live.progress.machine.registers.gpr[index].word ==
              UINT32_C(0x62000000) + index &&
          live.progress.machine.registers.gpr[index].known_mask ==
              static_cast<std::uint8_t>((index % 15u) + 1u));
  CHECK(live.progress.machine.registers.gpr[30].word == 0x6200001e &&
        live.progress.machine.registers.gpr[30].known_mask == 1);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture fixture;
    fixture.mutatePc = 0x8007999c;
    fixture.mutationRaMask = static_cast<std::uint8_t>(mask);
    const int result = fixture.run();
    if (mask == 15)
      CHECK(result == NBA97_TEXT_COMPLETE &&
            fixture.progress.restored_return_address.known_mask == 15);
    else
      CHECK(result == NBA97_TEXT_UNKNOWN &&
            fixture.progress.stopped_pc == 0x800799c4 &&
            fixture.progress.restored_return_address.known_mask == mask &&
            fixture.progress.machine.registers.gpr[29].word == 0x801fe018);
  }

  for (unsigned invalid = 1; invalid <= 4; ++invalid) {
    Fixture fixture;
    fixture.invalidKind = invalid;
    CHECK(fixture.run() == NBA97_TEXT_ARGUMENT &&
          fixture.progress.stopped_pc == 0x8007999c);
  }
}

void MappingAliasesWrapAndDeterminism() {
  Fixture missing;
  missing.context.memory = {nullptr, 0};
  CHECK(missing.run() == NBA97_TEXT_RESOURCE);
  Fixture unaligned;
  unaligned.context.machine.registers.gpr[29].word |= 1;
  CHECK(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x800798c0);
  Fixture overlap;
  std::array<Nba97GameTextRegion, 2> overlapping{{
      overlap.region,
      {Fixture::Ram + 4, overlap.bytes.data() + 4, overlap.known.data() + 4, 4},
  }};
  overlap.context.memory = {overlapping.data(), overlapping.size()};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture sizeMax;
  sizeMax.region.size = static_cast<std::size_t>(-1);
  CHECK(sizeMax.run() == NBA97_TEXT_ARGUMENT);

  Fixture wrap;
  std::array<std::uint8_t, 4> low{};
  std::array<std::uint8_t, 4> lowKnown{};
  lowKnown.fill(1);
  std::array<Nba97GameTextRegion, 2> wrapRegions{
      {{0, low.data(), lowKnown.data(), low.size()}, wrap.region}};
  wrap.context.memory = {wrapRegions.data(), wrapRegions.size()};
  wrap.context.machine.registers.gpr[29] = {8, 15};
  CHECK(wrap.run() == NBA97_TEXT_COMPLETE &&
        wrap.progress.frame_stack_pointer == 0xfffffff0 &&
        wrap.progress.machine.registers.gpr[29].word == 8);

  Fixture alias;
  alias.context.machine.registers.gpr[29] = {0x800bc208, 15};
  alias.context.machine.registers.gpr[31] = {1, 15};
  alias.put(0x800bc200, 0);
  CHECK(alias.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        alias.get(0x800bc200) == 1 && alias.events.size() == 1 &&
        alias.events[0].pc == 0x8007999c &&
        alias.progress.stopped_pc == 0x800799c4);

  Fixture publicationAlias;
  publicationAlias.context.machine.registers.gpr[29] = {0x800d8ef4, 15};
  publicationAlias.context.machine.registers.gpr[31] = {0x81234568, 15};
  CHECK(publicationAlias.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        publicationAlias.progress.restored_return_address.word == 42 &&
        publicationAlias.progress.machine.registers.gpr[31].word == 42 &&
        publicationAlias.progress.stopped_pc == 0x800799c4);

  Fixture first;
  Fixture second;
  CHECK(first.run() == NBA97_TEXT_COMPLETE &&
        second.run() == NBA97_TEXT_COMPLETE && first.bytes == second.bytes &&
        first.known == second.known &&
        sameMachine(first.progress.machine, second.progress.machine));
  Fixture invalid;
  invalid.context.machine.hi.known_mask = 16;
  CHECK(invalid.run() == NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_elapsed_dispatch(nullptr, &invalid.progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_elapsed_dispatch(&invalid.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  SentinelRefreshAndMachine();
  DeltaClampBoundsAndEarlyExit();
  IndirectProbeRefreshPaths();
  DynamicTargetFailuresAndCallRefusals();
  UnknownBranchesAndAtomicMemory();
  BudgetsLiveStateAndReturnMasks();
  MappingAliasesWrapAndDeterminism();
  std::printf("game camera elapsed dispatch: %u checks\n", checks);
}
