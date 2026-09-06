#include "game_controller_profile_reset_adapter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "controller profile reset check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Records = 0x8001ee00u;
constexpr std::uint32_t RecordBase = 0x8001ef7cu;
constexpr std::uint32_t Profiles = 0x80020c00u;
constexpr std::uint32_t ProfileBase = 0x80020c1cu;
constexpr std::uint32_t Selections = 0x80021ddeu;
constexpr std::uint32_t Defaults = 0x800bc900u;
constexpr std::uint32_t DefaultTail = 0x800bc94cu;
constexpr std::uint32_t Stack = 0x800fee00u;
constexpr std::uint32_t EntrySp = 0x800ff000u;
constexpr std::uint32_t AlternateStack = 0x8010ee00u;

struct Segment {
  std::uint32_t base;
  std::vector<std::uint8_t> bytes;
  std::vector<std::uint8_t> known;

  Segment(std::uint32_t segmentBase, std::size_t size)
      : base(segmentBase), bytes(size), known(size) {}
};

struct Fixture {
  Segment records{Records, 0x1000};
  Segment profiles{Profiles, 0x8000};
  Segment defaults{Defaults, 0x200};
  Segment stack{Stack, 0x400};
  Segment alternateStack{AlternateStack, 0x400};
  std::array<Nba97GameTextRegion, 5> regions{};
  std::vector<Nba97GameControllerProfileResetAccess> journal =
      std::vector<Nba97GameControllerProfileResetAccess>(1100);
  std::vector<Nba97GameControllerProfileResetEvent> events =
      std::vector<Nba97GameControllerProfileResetEvent>(32);
  std::vector<Nba97GameControllerProfileResetMachine> machines =
      std::vector<Nba97GameControllerProfileResetMachine>(32);
  Nba97GameControllerProfileResetContext context{};
  Nba97GameControllerProfileResetProgress progress{};
  unsigned calls = 0;
  unsigned refuseInvocation = 0;
  bool invalidMachine = false;
  bool mutateLive = false;
  bool runaway = false;
  bool partialS0 = false;
  bool partialS2 = false;
  std::uint32_t liveS0 = 7;
  std::uint32_t liveS1 = 7;
  std::uint32_t liveS2 = 0x8001ef00u;
  std::uint32_t liveS3 = 1;
  std::uint32_t liveS4 = 0x80020d00u;

  Fixture() {
    std::fill(records.bytes.begin(), records.bytes.end(), std::uint8_t{0xa5});
    std::fill(profiles.bytes.begin(), profiles.bytes.end(), std::uint8_t{0x5a});
    std::fill(defaults.bytes.begin(), defaults.bytes.end(), std::uint8_t{0xc3});
    std::fill(stack.bytes.begin(), stack.bytes.end(), std::uint8_t{0x66});
    std::fill(alternateStack.bytes.begin(), alternateStack.bytes.end(),
              std::uint8_t{0x77});
    std::fill(records.known.begin(), records.known.end(), std::uint8_t{1});
    std::fill(profiles.known.begin(), profiles.known.end(), std::uint8_t{1});
    std::fill(defaults.known.begin(), defaults.known.end(), std::uint8_t{1});
    std::fill(stack.known.begin(), stack.known.end(), std::uint8_t{1});
    std::fill(alternateStack.known.begin(), alternateStack.known.end(),
              std::uint8_t{1});
    regions = {{{records.base, records.bytes.data(), records.known.data(),
                 records.bytes.size()},
                {profiles.base, profiles.bytes.data(), profiles.known.data(),
                 profiles.bytes.size()},
                {defaults.base, defaults.bytes.data(), defaults.known.data(),
                 defaults.bytes.size()},
                {stack.base, stack.bytes.data(), stack.known.data(),
                 stack.bytes.size()},
                {alternateStack.base, alternateStack.bytes.data(),
                 alternateStack.known.data(), alternateStack.bytes.size()}}};
    context.memory = {regions.data(), regions.size()};
    context.operation_budget = 2000;
    context.io = callback;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned reg = 0; reg < 32; ++reg)
      context.machine.registers.gpr[reg] = {0x41000000u + reg, 15};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[4] = {1, 15};
    context.machine.registers.gpr[29] = {EntrySp, 15};
    context.machine.registers.gpr[31] = {0x81234568u, 15};
    context.machine.hi = {0x10203040u, 5};
    context.machine.lo = {0xa0b0c0d0u, 10};
    for (unsigned i = 0; i < 256; ++i)
      put(DefaultTail + i, 0x80u + i, 1);
    for (unsigned i = 0; i < 8; ++i) {
      put(Selections + i, i, 1);
      put(ProfileBase + i * 108u + 0x6bu, 1, 1);
      for (unsigned byte = 0; byte < 59; ++byte)
        put(ProfileBase + i * 108u + 0x22u + byte, 0x20u + i * 3u + byte, 1);
    }
  }

  Nba97GameTextRegion *regionFor(std::uint32_t address, unsigned width = 1) {
    for (auto &region : regions) {
      const std::uint64_t offset = std::uint64_t(address) - region.base;
      if (address >= region.base && offset <= region.size &&
          width <= region.size - static_cast<std::size_t>(offset))
        return &region;
    }
    return nullptr;
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width,
           std::uint8_t mask = 15) {
    auto *region = regionFor(address, width);
    check(region != nullptr);
    const auto offset = static_cast<std::size_t>(address - region->base);
    for (unsigned i = 0; i < width; ++i) {
      region->data[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
      if (region->known)
        region->known[offset + i] = static_cast<std::uint8_t>((mask >> i) & 1u);
    }
  }

  std::uint32_t get(std::uint32_t address, unsigned width = 1) {
    auto *region = regionFor(address, width);
    check(region != nullptr);
    const auto offset = static_cast<std::size_t>(address - region->base);
    std::uint32_t result = 0;
    for (unsigned i = 0; i < width; ++i)
      result |= std::uint32_t(region->data[offset + i]) << (i * 8u);
    return result;
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameControllerProfileResetEvent *event,
                      Nba97GameControllerProfileResetMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    const unsigned index = fixture.calls++;
    if (index < fixture.events.size()) {
      fixture.events[index] = *event;
      fixture.machines[index] = *machine;
    }
    if (fixture.refuseInvocation == event->invocation)
      return 0;
    if (fixture.invalidMachine) {
      machine->lo.known_mask = 16;
      return 1;
    }
    check(machine->registers.gpr[4].known_mask == 15);
    for (unsigned byte = 0; byte < 0x24; ++byte)
      if (fixture.regionFor(machine->registers.gpr[4].word + byte))
        fixture.put(machine->registers.gpr[4].word + byte, 0, 1);
    if (fixture.runaway) {
      machine->registers.gpr[16] = {0, 15};
      machine->registers.gpr[17] = {UINT32_MAX, 15};
    }
    if (fixture.partialS0)
      machine->registers.gpr[16] = {0, 14};
    if (fixture.partialS2)
      machine->registers.gpr[18] = {RecordBase, 14};
    if (fixture.mutateLive && event->invocation == 1) {
      machine->registers.gpr[16] = {fixture.liveS0, 15};
      machine->registers.gpr[17] = {fixture.liveS1, 15};
      machine->registers.gpr[18] = {fixture.liveS2, 15};
      machine->registers.gpr[19] = {fixture.liveS3, 15};
      machine->registers.gpr[20] = {fixture.liveS4, 15};
      machine->registers.gpr[29] = {AlternateStack + 0x100u, 15};
      machine->registers.gpr[31] = {0x800834e0u, 15};
      machine->hi = {0x55667788u, 3};
      machine->lo = {0x99aabbccu, 12};
      fixture.put(Selections + 7, 0, 1);
      fixture.put(AlternateStack + 0x124u, 0x82345678u, 4);
      fixture.put(AlternateStack + 0x120u, 0x70000014u, 4);
      fixture.put(AlternateStack + 0x11cu, 0x70000013u, 4);
      fixture.put(AlternateStack + 0x118u, 0x70000012u, 4);
      fixture.put(AlternateStack + 0x114u, 0x70000011u, 4);
      fixture.put(AlternateStack + 0x110u, 0x70000010u, 4);
    }
    return 1;
  }

  int run() { return nba97_game_controller_profile_reset(&context, &progress); }
};

void canonicalOverrideAndProfiles() {
  Fixture overrideFixture;
  check(overrideFixture.run() == NBA97_TEXT_COMPLETE &&
        overrideFixture.progress.completed);
  check(overrideFixture.calls == 8 &&
        overrideFixture.progress.call_attempts == 8 &&
        overrideFixture.progress.call_count == 8 &&
        overrideFixture.progress.records_started == 8 &&
        overrideFixture.progress.records_copied == 8 &&
        overrideFixture.progress.bytes_copied == 8 * 59);
  check(overrideFixture.progress.operations == 972 &&
        overrideFixture.progress.accesses == 964 &&
        overrideFixture.progress.reads == 486 &&
        overrideFixture.progress.stores == 478);
  for (unsigned record = 0; record < 8; ++record) {
    for (unsigned byte = 0; byte < 0x24; ++byte)
      check(overrideFixture.get(RecordBase + record * 120u + byte) == 0);
    check(overrideFixture.get(RecordBase + record * 120u + 0x3bu) == 0xa5);
    for (unsigned byte = 0; byte < 59; ++byte)
      check(overrideFixture.get(RecordBase + record * 120u + 0x3cu + byte) ==
            static_cast<std::uint8_t>(0x80u + byte));
    check(overrideFixture.get(RecordBase + record * 120u + 0x77u) == 0xa5);
    check(overrideFixture.events[record].pc == 0x800834d8u &&
          overrideFixture.events[record].delay_slot_pc == 0x800834dcu &&
          overrideFixture.events[record].entry == 0x800a3a74u &&
          overrideFixture.events[record].argument_count == 2 &&
          overrideFixture.events[record].invocation == record + 1 &&
          overrideFixture.machines[record].registers.gpr[5].word == 0x24u &&
          overrideFixture.machines[record].registers.gpr[31].word ==
              0x800834e0u);
  }
  check(overrideFixture.progress.machine.registers.gpr[29].word == EntrySp &&
        overrideFixture.progress.restored_return_address.word == 0x81234568u &&
        overrideFixture.progress.machine.hi.word == 0x10203040u &&
        overrideFixture.progress.machine.hi.known_mask == 5 &&
        overrideFixture.progress.machine.lo.word == 0xa0b0c0d0u &&
        overrideFixture.progress.machine.lo.known_mask == 10 &&
        overrideFixture.progress.stopped_pc == 0);

  Fixture selected;
  selected.context.machine.registers.gpr[4] = {0, 15};
  selected.put(ProfileBase + 108u + 0x6bu, 0, 1);
  selected.put(Selections + 0, 0, 1);
  selected.put(Selections + 1, 1, 1);
  check(selected.run() == NBA97_TEXT_COMPLETE);
  for (unsigned byte = 0; byte < 59; ++byte) {
    check(selected.get(RecordBase + 0x3cu + byte) ==
          static_cast<std::uint8_t>(0x20u + byte));
    check(selected.get(RecordBase + 120u + 0x3cu + byte) ==
          static_cast<std::uint8_t>(0x80u + byte));
  }
  bool sawSelection = false;
  bool sawProfileFlag = false;
  for (unsigned i = 0; i < selected.progress.access_events; ++i) {
    sawSelection |= selected.journal[i].pc == 0x800834e8u;
    sawProfileFlag |= selected.journal[i].pc == 0x80083524u;
  }
  check(sawSelection && sawProfileFlag);
}

void SignedSelectionsAndForwardAliases() {
  for (auto selection : {127u, 128u, 255u}) {
    Fixture fixture;
    fixture.context.machine.registers.gpr[4] = {0, 15};
    for (unsigned i = 0; i < 8; ++i)
      fixture.put(Selections + i, selection, 1);
    if (selection == 127u) {
      fixture.put(ProfileBase + selection * 108u + 0x6bu, 1, 1);
      for (unsigned byte = 0; byte < 59; ++byte)
        fixture.put(ProfileBase + selection * 108u + 0x22u + byte, 0xd0u + byte,
                    1);
    }
    check(fixture.run() == NBA97_TEXT_COMPLETE);
    check(fixture.progress.records_copied == (selection == 127u ? 8u : 0u));
    check(fixture.get(RecordBase + 0x3cu) ==
          (selection == 127u ? 0xd0u : 0xa5u));
    check(fixture.get(RecordBase) == 0);
  }

  constexpr auto destination = 0x8001f400u;
  Fixture forward;
  forward.context.machine.registers.gpr[4] = {0, 15};
  forward.mutateLive = true;
  forward.liveS2 = destination - 7u * 120u - 0x3cu;
  forward.liveS3 = 0;
  forward.liveS4 = destination - 0x23u;
  forward.put(Selections + 7u, 0, 1);
  /* Source starts one byte before destination; forward byte order propagates
   * the first source byte through the entire overlapping tail. */
  for (unsigned i = 0; i < 60; ++i)
    forward.put(destination - 1u + i, 0x40u + i, 1);
  forward.put(forward.liveS4 + 0x6bu, 1, 1);
  check(forward.run() == NBA97_TEXT_COMPLETE && forward.calls == 1);
  for (unsigned i = 0; i < 59; ++i)
    check(forward.get(destination + i) == 0x40u);

  Fixture backward;
  backward.context.machine.registers.gpr[4] = {0, 15};
  backward.mutateLive = true;
  backward.liveS2 = destination - 7u * 120u - 0x3cu;
  backward.liveS3 = 0;
  backward.liveS4 = destination - 0x21u;
  backward.put(Selections + 7u, 0, 1);
  for (unsigned i = 0; i < 60; ++i)
    backward.put(destination + 1u + i, 0x50u + i, 1);
  backward.put(backward.liveS4 + 0x6bu, 1, 1);
  check(backward.run() == NBA97_TEXT_COMPLETE && backward.calls == 1);
  for (unsigned i = 0; i < 59; ++i)
    check(backward.get(destination + i) ==
          static_cast<std::uint8_t>(0x50u + i));
  std::size_t firstLoad = 0;
  std::size_t firstStore = 0;
  for (std::size_t i = 0; i < backward.progress.access_events; ++i) {
    if (!firstLoad && backward.journal[i].pc == 0x8008355cu)
      firstLoad = i + 1;
    if (!firstStore && backward.journal[i].pc == 0x80083578u)
      firstStore = i + 1;
  }
  check(firstLoad && firstStore == firstLoad + 1);

  Fixture signedIndex;
  signedIndex.context.machine.registers.gpr[4] = {0, 15};
  signedIndex.mutateLive = true;
  signedIndex.liveS0 = 0;
  signedIndex.liveS1 = UINT32_MAX;
  signedIndex.liveS2 = RecordBase + 120u;
  signedIndex.liveS3 = 0;
  signedIndex.liveS4 = ProfileBase;
  signedIndex.put(Selections, 0, 1);
  check(signedIndex.run() == NBA97_TEXT_COMPLETE &&
        signedIndex.progress.records_started == 9 && signedIndex.calls == 9);
}

void CallbackLivenessAndFailures() {
  Fixture live;
  live.mutateLive = true;
  check(live.run() == NBA97_TEXT_COMPLETE && live.calls == 1);
  check(live.progress.frame_stack_pointer == EntrySp - 0x28u &&
        live.progress.machine.registers.gpr[29].word ==
            AlternateStack + 0x128u &&
        live.progress.restored_return_address.word == 0x82345678u &&
        live.progress.restored_s4.word == 0x70000014u &&
        live.progress.restored_s3.word == 0x70000013u &&
        live.progress.restored_s2.word == 0x70000012u &&
        live.progress.restored_s1.word == 0x70000011u &&
        live.progress.restored_s0.word == 0x70000010u &&
        live.progress.machine.hi.word == 0x55667788u &&
        live.progress.machine.hi.known_mask == 3 &&
        live.progress.machine.lo.word == 0x99aabbccu &&
        live.progress.machine.lo.known_mask == 12);

  for (unsigned invocation = 1; invocation <= 8; ++invocation) {
    Fixture refused;
    refused.refuseInvocation = invocation;
    check(refused.run() == NBA97_TEXT_IO_REFUSED &&
          refused.calls == invocation &&
          refused.progress.stopped_pc == 0x800834d8u &&
          refused.progress.stopped_entry == 0x800a3a74u &&
          refused.progress.machine.registers.gpr[31].word == 0x800834e0u &&
          refused.progress.machine.registers.gpr[5].word == 0x24u);
  }
  Fixture missing;
  missing.context.io = nullptr;
  check(missing.run() == NBA97_TEXT_IO_REFUSED && missing.calls == 0);
  Fixture invalid;
  invalid.invalidMachine = true;
  check(invalid.run() == NBA97_TEXT_ARGUMENT &&
        invalid.progress.machine.lo.known_mask == 16);
}

void UnknownsAtomicityAndRunaway() {
  Fixture unknownOverride;
  unknownOverride.context.machine.registers.gpr[4].known_mask = 14;
  check(unknownOverride.run() == NBA97_TEXT_UNKNOWN &&
        unknownOverride.progress.stopped_pc == 0x800834f4u &&
        unknownOverride.progress.machine.registers.gpr[2].known_mask == 15);

  Fixture unknownSelection;
  unknownSelection.context.machine.registers.gpr[4] = {0, 15};
  unknownSelection.put(Selections, 0, 1, 0);
  check(unknownSelection.run() == NBA97_TEXT_UNKNOWN &&
        unknownSelection.progress.stopped_pc == 0x80083504u &&
        unknownSelection.progress.machine.registers.gpr[2].word == 1);

  Fixture unknownFlag;
  unknownFlag.context.machine.registers.gpr[4] = {0, 15};
  unknownFlag.put(Selections, 0, 1);
  unknownFlag.put(ProfileBase + 0x6bu, 0, 1, 0);
  check(unknownFlag.run() == NBA97_TEXT_UNKNOWN &&
        unknownFlag.progress.stopped_pc == 0x8008352cu &&
        unknownFlag.progress.machine.registers.gpr[6].word ==
            ProfileBase + 0x22u);

  Fixture unknownLoadAddress;
  unknownLoadAddress.partialS0 = true;
  check(unknownLoadAddress.run() == NBA97_TEXT_UNKNOWN &&
        unknownLoadAddress.progress.stopped_pc == 0x800834e8u &&
        unknownLoadAddress.progress.stopped_address == Selections);

  Fixture unknownStoreAddress;
  unknownStoreAddress.partialS2 = true;
  check(unknownStoreAddress.run() == NBA97_TEXT_UNKNOWN &&
        unknownStoreAddress.progress.stopped_pc == 0x80083578u);

  Fixture partialStore;
  partialStore.context.machine.registers.gpr[4] = {1, 15};
  partialStore.defaults.known[DefaultTail - Defaults] = 0;
  partialStore.regions[0].known = nullptr;
  const auto before = partialStore.get(RecordBase + 0x3cu);
  check(partialStore.run() == NBA97_TEXT_ARGUMENT &&
        partialStore.progress.stopped_pc == 0x80083578u &&
        partialStore.get(RecordBase + 0x3cu) == before);

  Fixture malformedLater;
  malformedLater.records.known[RecordBase - Records + 0x3cu + 9u] = 2;
  const auto laterBefore = malformedLater.get(RecordBase + 0x3cu + 9u);
  check(malformedLater.run() == NBA97_TEXT_ARGUMENT &&
        malformedLater.progress.stopped_pc == 0x80083578u &&
        malformedLater.progress.bytes_copied == 9 &&
        malformedLater.get(RecordBase + 0x3cu + 9u) == laterBefore);

  Fixture runaway;
  runaway.runaway = true;
  runaway.context.machine.registers.gpr[4] = {0, 15};
  runaway.put(Selections, 128, 1);
  runaway.context.operation_budget = 400;
  const int runawayResult = runaway.run();
  check(runawayResult == NBA97_TEXT_LIMIT &&
        runaway.progress.operations == 400 && runaway.calls > 1 &&
        !runaway.progress.completed);
}

void BudgetsMapsMasksAndStackWrap() {
  Fixture complete;
  check(complete.run() == NBA97_TEXT_COMPLETE);
  Fixture repeat;
  check(repeat.run() == NBA97_TEXT_COMPLETE);
  for (unsigned reg = 0; reg < 32; ++reg) {
    check(complete.progress.machine.registers.gpr[reg].word ==
              repeat.progress.machine.registers.gpr[reg].word &&
          complete.progress.machine.registers.gpr[reg].known_mask ==
              repeat.progress.machine.registers.gpr[reg].known_mask);
  }
  check(complete.progress.machine.hi.word == repeat.progress.machine.hi.word &&
        complete.progress.machine.hi.known_mask ==
            repeat.progress.machine.hi.known_mask &&
        complete.progress.machine.lo.word == repeat.progress.machine.lo.word &&
        complete.progress.machine.lo.known_mask ==
            repeat.progress.machine.lo.known_mask);
  for (unsigned reg : {7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 21u, 22u, 23u,
                       24u, 25u, 26u, 27u, 28u, 30u})
    check(complete.progress.machine.registers.gpr[reg].word ==
              0x41000000u + reg &&
          complete.progress.machine.registers.gpr[reg].known_mask == 15);
  for (unsigned reg = 16; reg <= 20; ++reg)
    check(complete.progress.machine.registers.gpr[reg].word ==
              0x41000000u + reg &&
          complete.progress.machine.registers.gpr[reg].known_mask == 15);

  for (std::uint8_t mask = 0; mask < 16; ++mask) {
    Fixture knownness;
    knownness.context.machine.registers.gpr[4] = {0, mask};
    const int result = knownness.run();
    check(result == (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    if (mask != 15)
      check(knownness.progress.stopped_pc == 0x800834f4u &&
            knownness.progress.machine.registers.gpr[2].known_mask == 15);
  }
  const auto operations = complete.progress.operations;
  for (std::size_t budget = 0; budget < operations; ++budget) {
    Fixture prefix;
    prefix.context.operation_budget = budget;
    check(prefix.run() == NBA97_TEXT_LIMIT &&
          prefix.progress.operations == budget);
  }

  Fixture unknownSp;
  unknownSp.context.machine.registers.gpr[29].known_mask = 14;
  check(unknownSp.run() == NBA97_TEXT_UNKNOWN &&
        unknownSp.progress.stopped_pc == 0x80083494u);
  Fixture unaligned;
  ++unaligned.context.machine.registers.gpr[29].word;
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80083494u);
  Fixture missing;
  missing.regions[0].size = 8;
  check(missing.run() == NBA97_TEXT_RESOURCE);
  Fixture malformed;
  malformed.stack.known[EntrySp - 0x28u + 0x1cu - Stack] = 2;
  check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x80083494u);
  Fixture overlap;
  overlap.regions[4] = overlap.regions[3];
  check(overlap.run() == NBA97_TEXT_ARGUMENT);
  check(nba97_game_controller_profile_reset(nullptr, &overlap.progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_controller_profile_reset(&overlap.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
  Fixture badMask;
  badMask.context.machine.hi.known_mask = 16;
  check(badMask.run() == NBA97_TEXT_ARGUMENT);

  Fixture unknownRa;
  unknownRa.context.machine.registers.gpr[31].known_mask = 7;
  check(unknownRa.run() == NBA97_TEXT_UNKNOWN &&
        unknownRa.progress.stopped_pc == 0x800835bcu &&
        unknownRa.progress.instruction_count ==
            complete.progress.instruction_count);

  Fixture wrap;
  wrap.context.machine.registers.gpr[29] = {0x10u, 15};
  std::array<std::uint8_t, 24> high{};
  std::array<std::uint8_t, 24> highKnown{};
  std::array<std::uint8_t, 32> low{};
  std::array<std::uint8_t, 32> lowKnown{};
  highKnown.fill(1);
  lowKnown.fill(1);
  std::array<Nba97GameTextRegion, 7> wrapRegions{
      {wrap.regions[0],
       wrap.regions[1],
       wrap.regions[2],
       wrap.regions[3],
       wrap.regions[4],
       {0xffffffe8u, high.data(), highKnown.data(), high.size()},
       {0, low.data(), lowKnown.data(), low.size()}}};
  wrap.context.memory = {wrapRegions.data(), wrapRegions.size()};
  check(wrap.run() == NBA97_TEXT_COMPLETE &&
        wrap.progress.frame_stack_pointer == 0xffffffe8u &&
        wrap.progress.machine.registers.gpr[29].word == 0x10u &&
        wrap.progress.restored_return_address.word == 0x81234568u);
}

void AdapterGuardsAndZeroComposition() {
  Fixture fixture;
  Nba97GameControllerProfileResetBinding binding{};
  nba97_game_controller_profile_reset_binding_init(&binding, 2000, 32);
  Nba97GameMatchStateResetEvent event{};
  event.pc = 0x80065a38u;
  event.delay_slot_pc = 0x80065a3cu;
  event.entry = 0x80083490u;
  event.invocation = 1;
  event.kind = NBA97_GAME_MATCH_STATE_RESET_80083490;
  event.argument_count = 1;
  Nba97GameMatchStateResetMachine machine{};
  for (unsigned i = 0; i < 32; ++i) {
    machine.registers.gpr[i].word =
        fixture.context.machine.registers.gpr[i].word;
    machine.registers.gpr[i].known_mask = 15;
  }
  machine.registers.gpr[0] = {0, 15};
  machine.registers.gpr[4] = {0, 15};
  machine.registers.gpr[29] = {EntrySp, 15};
  machine.registers.gpr[31] = {0x80065a40u, 15};
  machine.hi = {0x11223344u, 5};
  machine.lo = {0x55667788u, 10};
  check(nba97_game_controller_profile_reset_from_match_state_reset(
            &binding, &fixture.context.memory, &event, &machine) == 1 &&
        binding.result == NBA97_TEXT_COMPLETE &&
        binding.zero_invocations == 8 && binding.completions == 1 &&
        machine.registers.gpr[31].word == 0x80065a40u &&
        machine.hi.word == 0x11223344u && machine.hi.known_mask == 5);
  check(binding.zero_progress.completed &&
        binding.zero_progress.bytes_stored >= 36);

  machine.registers.gpr[4] = {0, 15};
  machine.registers.gpr[29] = {EntrySp, 15};
  machine.registers.gpr[31] = {0x80065a40u, 15};
  check(nba97_game_controller_profile_reset_from_match_state_reset(
            &binding, &fixture.context.memory, &event, &machine) == 1 &&
        binding.invocations == 2 && binding.completions == 2 &&
        binding.zero_invocations == 16);

  event.pc ^= 4u;
  const auto before = machine;
  check(!nba97_game_controller_profile_reset_from_match_state_reset(
            &binding, &fixture.context.memory, &event, &machine) &&
        binding.result == NBA97_TEXT_ARGUMENT &&
        std::memcmp(&before, &machine, sizeof before) == 0);
  event.pc ^= 4u;
  machine.lo.known_mask = 16;
  check(!nba97_game_controller_profile_reset_from_match_state_reset(
      &binding, &fixture.context.memory, &event, &machine));

  Fixture limited;
  nba97_game_controller_profile_reset_binding_init(&binding, 2000, 0);
  machine.lo = {0x55667788u, 10};
  machine.registers.gpr[4] = {0, 15};
  machine.registers.gpr[29] = {EntrySp, 15};
  machine.registers.gpr[31] = {0x80065a40u, 15};
  check(!nba97_game_controller_profile_reset_from_match_state_reset(
            &binding, &limited.context.memory, &event, &machine) &&
        binding.result == NBA97_TEXT_LIMIT &&
        binding.nested_result == NBA97_TEXT_LIMIT &&
        machine.registers.gpr[1].word == 0 &&
        machine.registers.gpr[1].known_mask == 15 &&
        machine.registers.gpr[6].word == 0 &&
        machine.registers.gpr[10].word == 0);
}
} // namespace

int main() {
  canonicalOverrideAndProfiles();
  SignedSelectionsAndForwardAliases();
  CallbackLivenessAndFailures();
  UnknownsAtomicityAndRunaway();
  BudgetsMapsMasksAndStackWrap();
  AdapterGuardsAndZeroComposition();
  std::printf("game controller profile reset: %u checks\n", checks);
}
