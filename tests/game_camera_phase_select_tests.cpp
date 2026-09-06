#include "recovered/game_camera_phase_select.h"

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
    std::fprintf(stderr, "camera phase select check %u failed at %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define CHECK(value) checkAt((value), __LINE__)

bool sameWord(const Nba97GameCameraPhaseSelectWord &left,
              const Nba97GameCameraPhaseSelectWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool sameMachine(const Nba97GameCameraPhaseSelectMachine &left,
                 const Nba97GameCameraPhaseSelectMachine &right) {
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
  std::vector<Nba97GameCameraPhaseSelectAccess> journal =
      std::vector<Nba97GameCameraPhaseSelectAccess>(256);
  Nba97GameCameraPhaseSelectContext context{};
  Nba97GameCameraPhaseSelectProgress progress{};
  std::vector<Nba97GameCameraPhaseSelectEvent> events;
  std::vector<Nba97GameCameraPhaseSelectMachine> machines;
  std::uint32_t refusePc = 0;
  std::uint32_t mutatePc = 0;
  unsigned invalidKind = 0;
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
    context.machine.registers.gpr[4] = {0, 15};
    context.machine.registers.gpr[29] = {Sp, 15};
    context.machine.registers.gpr[31] = {UINT32_C(0x81234568), 15};
    context.machine.hi = {UINT32_C(0x11223344), 5};
    context.machine.lo = {UINT32_C(0x55667788), 10};
    put(UINT32_C(0x800fc99c), 0, 4);
    put(UINT32_C(0x800f9ffe), 0, 2);
    put(UINT32_C(0x800fdb90), 0x81, 2);
    put(UINT32_C(0x800fe884), 2, 2);
    put(UINT32_C(0x800fdb68), 1, 2);
    put(UINT32_C(0x800fdb58), 10, 4);
    put(UINT32_C(0x800fdb60), 10, 4);
    put(UINT32_C(0x800bc940), 0, 4);
    put(UINT32_C(0x800bc944), 0, 4);
    put(UINT32_C(0x80021ed8), 0x5a, 1);
  }

  std::size_t at(std::uint32_t address) const { return address - Ram; }

  void put(std::uint32_t address, std::uint32_t value, unsigned width,
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
                      const Nba97GameCameraPhaseSelectEvent *event,
                      Nba97GameCameraPhaseSelectMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.events.push_back(*event);
    fixture.machines.push_back(*machine);
    if (fixture.refusePc == event->pc)
      return 0;
    if (fixture.invalidKind == 1)
      machine->registers.gpr[0].known_mask = 14;
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
      fixture.put(UINT32_C(0x801fe010), UINT32_C(0x82345678), 4,
                  fixture.mutationRaMask);
      fixture.put(UINT32_C(0x800bc940), 4, 4);
    }
    return 1;
  }

  int run() { return nba97_game_camera_phase_select(&context, &progress); }
};

void expectEvent(const Fixture &fixture, unsigned index, std::uint32_t pc,
                 std::uint32_t entry, unsigned invocation, unsigned argc,
                 std::uint32_t a0, std::uint32_t a1) {
  const auto &event = fixture.events[index];
  const auto &machine = fixture.machines[index];
  CHECK(event.pc == pc && event.delay_slot_pc == pc + 4u &&
        event.entry == entry && event.invocation == invocation &&
        event.argument_count == argc &&
        machine.registers.gpr[31].word == pc + 8u &&
        machine.registers.gpr[4].word == a0 &&
        machine.registers.gpr[4].known_mask == 15);
  if (argc == 2)
    CHECK(machine.registers.gpr[5].word == a1 &&
          machine.registers.gpr[5].known_mask == 15);
}

void PhaseOneExactSequenceAndMachine() {
  Fixture fixture;
  const auto incoming = fixture.context.machine;
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE && fixture.progress.completed &&
        fixture.progress.phase_changed && fixture.get(0x800bc940) == 1 &&
        fixture.get(0x800bc944) == 1 && fixture.get(0x800fc99c) == 0);
  CHECK(fixture.events.size() == 4 &&
        fixture.progress.callbacks_completed == 4);
  expectEvent(fixture, 0, 0x8007e3a0, 0x800799cc, 1, 2, 3, 1);
  expectEvent(fixture, 1, 0x8007e3a8, 0x80079ebc, 1, 1, 15, 0);
  expectEvent(fixture, 2, 0x8007e3b0, 0x80079ebc, 2, 1, 8, 0);
  expectEvent(fixture, 3, 0x8007e3b8, 0x80079ebc, 3, 1, 8, 0);
  CHECK(fixture.progress.selected_phase.word == 1 &&
        fixture.progress.published_phase.word == 1 &&
        fixture.progress.restored_return_address.word == 0x81234568 &&
        fixture.progress.machine.registers.gpr[29].word == Fixture::Sp);
  for (unsigned index = 6; index < 29; ++index)
    CHECK(sameWord(fixture.progress.machine.registers.gpr[index],
                   incoming.registers.gpr[index]));
  CHECK(sameWord(fixture.progress.machine.registers.gpr[30],
                 incoming.registers.gpr[30]) &&
        sameWord(fixture.progress.machine.hi, incoming.hi) &&
        sameWord(fixture.progress.machine.lo, incoming.lo));
  const std::array<std::uint32_t, 11> accessPcs{
      0x8007e270, 0x8007e27c, 0x8007e284, 0x8007e2b0, 0x8007e340, 0x8007e348,
      0x8007e390, 0x8007e440, 0x8007e448, 0x8007e450, 0x8007e454};
  CHECK(fixture.progress.access_events == accessPcs.size());
  for (unsigned index = 0; index < accessPcs.size(); ++index)
    CHECK(fixture.journal[index].pc == accessPcs[index]);
}

void PhaseTwoThreeFourAndUnchangedDelays() {
  Fixture phaseTwo;
  phaseTwo.put(0x800fdb90, 0x82, 2);
  CHECK(phaseTwo.run() == NBA97_TEXT_COMPLETE &&
        phaseTwo.get(0x800bc940) == 2 && phaseTwo.events.size() == 2);
  expectEvent(phaseTwo, 0, 0x8007e3dc, 0x800799cc, 1, 2, 7, 0);
  expectEvent(phaseTwo, 1, 0x8007e3e4, 0x80079ebc, 1, 1, 13, 0);

  Fixture phaseThree;
  phaseThree.put(0x800fdb90, 0, 2);
  CHECK(phaseThree.run() == NBA97_TEXT_COMPLETE &&
        phaseThree.get(0x800bc940) == 3 && phaseThree.events.size() == 2);
  expectEvent(phaseThree, 0, 0x8007e424, 0x800799cc, 1, 2, 1, 0);
  expectEvent(phaseThree, 1, 0x8007e434, 0x80079f78, 1, 1, 0x5a, 0);

  Fixture phaseFour;
  phaseFour.put(0x800fdb90, 0, 2);
  phaseFour.put(0x800bc940, 4, 4);
  CHECK(phaseFour.run() == NBA97_TEXT_COMPLETE &&
        phaseFour.get(0x800bc940) == 4 && phaseFour.events.size() == 2);
  expectEvent(phaseFour, 0, 0x8007e424, 0x800799cc, 1, 2, 2, 0);
  expectEvent(phaseFour, 1, 0x8007e434, 0x80079f78, 1, 1, 0x5a, 0);

  for (unsigned phase = 1; phase <= 4; ++phase) {
    Fixture unchanged;
    unchanged.put(0x800fdb90, phase == 1 ? 0x81 : 0, 2);
    unchanged.put(0x800bc940, phase, 4);
    unchanged.put(0x800bc944, phase, 4);
    if (phase == 2)
      unchanged.put(0x800fdb90, 0x82, 2);
    CHECK(unchanged.run() == NBA97_TEXT_COMPLETE && unchanged.events.empty() &&
          !unchanged.progress.phase_changed &&
          unchanged.get(0x800bc944) == phase);
    const unsigned delayA0[4] = {3, 7, 1, 2};
    CHECK(unchanged.progress.machine.registers.gpr[4].word ==
              delayA0[phase - 1] &&
          unchanged.progress.machine.registers.gpr[4].known_mask == 15);
  }
}

void SelectorBoundariesAndClearInput() {
  struct Case {
    std::uint16_t state;
    std::uint16_t threshold;
    std::uint16_t period;
    std::uint32_t firstScore;
    std::uint32_t secondScore;
    std::uint32_t current;
    std::uint32_t expected;
  };
  const std::array<Case, 11> cases{{
      {0x81, 0, 0, 1, 1, 4, 1},
      {0x82, 1, 1, 1, 1, 4, 4},
      {0x82, 2, 1, 1, 1, 4, 2},
      {0x82, 2, 0, 1, 1, 4, 4},
      {0x82, 2, 0, 1, 2, 4, 2},
      {0x82, 2, 4, 1, 1, 3, 3},
      {0x82, 2, 4, 2, 1, 3, 2},
      {0x82, 2, 5, 1, 1, 3, 2},
      {0, 2, 1, 1, 1, 4, 4},
      {0, 2, 1, 1, 1, UINT32_MAX, 3},
      {0, 2, 1, 1, 1, 0x80000000, 3},
  }};
  for (const auto &test : cases) {
    Fixture fixture;
    fixture.put(0x800fdb90, test.state, 2);
    fixture.put(0x800fe884, test.threshold, 2);
    fixture.put(0x800fdb68, test.period, 2);
    fixture.put(0x800fdb58, test.firstScore, 4);
    fixture.put(0x800fdb60, test.secondScore, 4);
    fixture.put(0x800bc940, test.current, 4);
    fixture.put(0x800bc944, test.expected, 4);
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE &&
          fixture.get(0x800bc940) == test.expected &&
          fixture.get(0x800bc944) == test.expected);
  }

  Fixture negativeThreshold;
  negativeThreshold.put(0x800fdb90, 0x82, 2);
  negativeThreshold.put(0x800fe884, 0xffff, 2);
  negativeThreshold.put(0x800bc940, 4, 4);
  negativeThreshold.put(0x800bc944, 4, 4);
  CHECK(negativeThreshold.run() == NBA97_TEXT_COMPLETE &&
        negativeThreshold.get(0x800bc940) == 4);

  Fixture clearInput;
  clearInput.context.machine.registers.gpr[4] = {0x100, 15};
  clearInput.put(0x800bc940, 1, 4);
  clearInput.put(0x800bc944, 1, 4);
  CHECK(clearInput.run() == NBA97_TEXT_COMPLETE &&
        clearInput.progress.machine.registers.gpr[4].word == 3 &&
        clearInput.get(0x800bc940) == 1 && clearInput.events.empty());
  Fixture clearLowByte;
  clearLowByte.context.machine.registers.gpr[4] = {0x101, 15};
  clearLowByte.put(0x800bc940, 1, 4);
  clearLowByte.put(0x800bc944, 1, 4);
  CHECK(clearLowByte.run() == NBA97_TEXT_COMPLETE &&
        clearLowByte.get(0x800bc940) == 1 &&
        clearLowByte.progress.phase_changed && clearLowByte.events.size() == 4);
}

void EarlyExitsUnknownPrefixesAndKnownness() {
  Fixture busy;
  busy.put(0x800fc99c, 1, 4);
  busy.put(0x800bc944, 0x12345678, 4);
  CHECK(busy.run() == NBA97_TEXT_COMPLETE && busy.events.empty() &&
        busy.get(0x800bc944) == 0x12345678 &&
        busy.progress.access_events == 3 &&
        busy.progress.instruction_count == 9);

  Fixture unknownBusy;
  unknownBusy.put(0x800fc99c, 0, 4, 0);
  CHECK(unknownBusy.run() == NBA97_TEXT_UNKNOWN &&
        unknownBusy.progress.stopped_pc == 0x8007e278 &&
        unknownBusy.progress.stores == 1);

  Fixture guard;
  guard.context.machine.registers.gpr[4] = {0x12345678, 15};
  guard.put(0x800f9ffe, 1, 2);
  CHECK(guard.run() == NBA97_TEXT_COMPLETE && guard.events.empty() &&
        guard.progress.machine.registers.gpr[2].word == 0x78 &&
        guard.get(0x800bc944) == 0);

  for (unsigned mask = 0; mask != 16; ++mask) {
    Fixture andiMask;
    andiMask.context.machine.registers.gpr[4] = {
        UINT32_C(0x123456a5), static_cast<std::uint8_t>(mask)};
    andiMask.put(0x800f9ffe, 1, 2);
    CHECK(andiMask.run() == NBA97_TEXT_COMPLETE &&
          andiMask.progress.machine.registers.gpr[2].word == 0xa5 &&
          andiMask.progress.machine.registers.gpr[2].known_mask ==
              static_cast<std::uint8_t>(14u | (mask & 1u)));
  }

  Fixture unknownGuard;
  unknownGuard.context.machine.registers.gpr[4] = {0xa5, 14};
  unknownGuard.put(0x800f9ffe, 0, 2, 0);
  CHECK(unknownGuard.run() == NBA97_TEXT_UNKNOWN &&
        unknownGuard.progress.stopped_pc == 0x8007e28c &&
        unknownGuard.progress.machine.registers.gpr[2].known_mask == 14);

  Fixture unknownLowA0;
  unknownLowA0.context.machine.registers.gpr[4] = {0, 14};
  CHECK(unknownLowA0.run() == NBA97_TEXT_UNKNOWN &&
        unknownLowA0.progress.stopped_pc == 0x8007e294 &&
        unknownLowA0.progress.machine.registers.gpr[2].known_mask == 14);

  Fixture unknownThreshold;
  unknownThreshold.put(0x800fdb90, 0x82, 2);
  unknownThreshold.put(0x800fe884, 2, 2, 1);
  CHECK(unknownThreshold.run() == NBA97_TEXT_UNKNOWN &&
        unknownThreshold.progress.stopped_pc == 0x8007e2e0 &&
        unknownThreshold.progress.machine.registers.gpr[2].known_mask == 14);
}

void CallRefusalsBudgetsAndLiveMutation() {
  const std::array<std::uint32_t, 8> pcs{0x8007e3a0, 0x8007e3a8, 0x8007e3b0,
                                         0x8007e3b8, 0x8007e3dc, 0x8007e3e4,
                                         0x8007e424, 0x8007e434};
  for (auto pc : pcs) {
    Fixture fixture;
    if (pc == 0x8007e3dc || pc == 0x8007e3e4)
      fixture.put(0x800fdb90, 0x82, 2);
    if (pc == 0x8007e424 || pc == 0x8007e434)
      fixture.put(0x800fdb90, 0, 2);
    fixture.refusePc = pc;
    CHECK(fixture.run() == NBA97_TEXT_IO_REFUSED &&
          fixture.progress.stopped_pc == pc &&
          fixture.progress.stopped_entry ==
              (pc == 0x8007e424                         ? 0x800799cc
               : pc == 0x8007e434                       ? 0x80079f78
               : (pc == 0x8007e3a0 || pc == 0x8007e3dc) ? 0x800799cc
                                                        : 0x80079ebc));
  }

  Fixture complete;
  CHECK(complete.run() == NBA97_TEXT_COMPLETE);
  for (std::size_t budget = 0; budget < complete.progress.operations;
       ++budget) {
    Fixture fixture;
    fixture.context.operation_budget = budget;
    CHECK(fixture.run() == NBA97_TEXT_LIMIT &&
          fixture.progress.operations == budget && !fixture.progress.completed);
  }

  Fixture live;
  live.mutatePc = 0x8007e3b8;
  CHECK(live.run() == NBA97_TEXT_COMPLETE && live.get(0x800bc944) == 4 &&
        live.progress.published_phase.word == 4 &&
        live.progress.machine.registers.gpr[29].word == 0x801fe018 &&
        live.progress.restored_return_address.word == 0x82345678 &&
        live.progress.restored_return_address.known_mask == 15 &&
        sameWord(live.progress.machine.registers.gpr[31],
                 live.progress.restored_return_address) &&
        live.progress.machine.hi.word == 0x10203040 &&
        live.progress.machine.lo.word == 0xa0b0c0d0);

  Fixture partialRa;
  partialRa.mutatePc = 0x8007e3b8;
  partialRa.mutationRaMask = 5;
  CHECK(partialRa.run() == NBA97_TEXT_UNKNOWN &&
        partialRa.progress.stopped_pc == 0x8007e45c &&
        partialRa.progress.machine.registers.gpr[29].word == 0x801fe018 &&
        partialRa.progress.restored_return_address.word == 0x82345678 &&
        partialRa.progress.restored_return_address.known_mask == 5 &&
        sameWord(partialRa.progress.machine.registers.gpr[31],
                 partialRa.progress.restored_return_address));

  for (unsigned invalid = 1; invalid <= 4; ++invalid) {
    Fixture fixture;
    fixture.invalidKind = invalid;
    CHECK(fixture.run() == NBA97_TEXT_ARGUMENT &&
          fixture.progress.stopped_pc == 0x8007e3a0);
  }
}

void MappingAliasesReturnsAndDeterminism() {
  Fixture malformed;
  malformed.known[malformed.at(0x800fc99c) + 2] = 2;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x8007e270 &&
        malformed.progress.machine.registers.gpr[2].word == 0x80100000);

  Fixture unknownSpill;
  unknownSpill.region.known = nullptr;
  unknownSpill.context.machine.registers.gpr[31].known_mask = 7;
  const std::array<std::uint8_t, 4> untouchedSpill{
      unknownSpill.bytes[unknownSpill.at(Fixture::Sp - 8)],
      unknownSpill.bytes[unknownSpill.at(Fixture::Sp - 7)],
      unknownSpill.bytes[unknownSpill.at(Fixture::Sp - 6)],
      unknownSpill.bytes[unknownSpill.at(Fixture::Sp - 5)]};
  CHECK(unknownSpill.run() == NBA97_TEXT_ARGUMENT &&
        unknownSpill.progress.stopped_pc == 0x8007e27c &&
        unknownSpill.bytes[unknownSpill.at(Fixture::Sp - 8)] ==
            untouchedSpill[0] &&
        unknownSpill.bytes[unknownSpill.at(Fixture::Sp - 7)] ==
            untouchedSpill[1] &&
        unknownSpill.bytes[unknownSpill.at(Fixture::Sp - 6)] ==
            untouchedSpill[2] &&
        unknownSpill.bytes[unknownSpill.at(Fixture::Sp - 5)] ==
            untouchedSpill[3]);

  Fixture unalignedSp;
  unalignedSp.context.machine.registers.gpr[29].word |= 1;
  CHECK(unalignedSp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unalignedSp.progress.stopped_pc == 0x8007e27c);
  Fixture missing;
  missing.context.memory = {nullptr, 0};
  CHECK(missing.run() == NBA97_TEXT_RESOURCE);
  Fixture overlap;
  std::array<Nba97GameTextRegion, 2> overlapRegions{{
      overlap.region,
      {Fixture::Ram + 4, overlap.bytes.data() + 4, overlap.known.data() + 4, 4},
  }};
  overlap.context.memory = {overlapRegions.data(), overlapRegions.size()};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture sizeMax;
  sizeMax.region.size = static_cast<std::size_t>(-1);
  CHECK(sizeMax.run() == NBA97_TEXT_ARGUMENT);

  Fixture alias;
  alias.context.machine.registers.gpr[29] = {0x800bc948, 15};
  alias.context.machine.registers.gpr[31] = {4, 15};
  alias.put(0x800fdb90, 0, 2);
  alias.put(0x800bc944, 4, 4);
  CHECK(alias.run() == NBA97_TEXT_COMPLETE && alias.get(0x800bc940) == 4 &&
        alias.progress.restored_return_address.word == 4 &&
        alias.progress.machine.registers.gpr[29].word == 0x800bc948);

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

  Fixture unknownRa;
  unknownRa.context.machine.registers.gpr[31].known_mask = 7;
  CHECK(unknownRa.run() == NBA97_TEXT_UNKNOWN &&
        unknownRa.progress.stopped_pc == 0x8007e45c &&
        unknownRa.get(0x800bc944) == 1 &&
        unknownRa.progress.machine.registers.gpr[29].word == Fixture::Sp);
  Fixture misalignedRa;
  misalignedRa.context.machine.registers.gpr[31].word |= 1;
  CHECK(misalignedRa.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misalignedRa.progress.stopped_pc == 0x8007e45c &&
        misalignedRa.get(0x800bc944) == 1);

  Fixture first;
  Fixture second;
  CHECK(first.run() == NBA97_TEXT_COMPLETE &&
        second.run() == NBA97_TEXT_COMPLETE && first.bytes == second.bytes &&
        first.known == second.known &&
        sameMachine(first.progress.machine, second.progress.machine));

  Fixture invalid;
  invalid.context.machine.hi.known_mask = 16;
  CHECK(invalid.run() == NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_phase_select(nullptr, &invalid.progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_phase_select(&invalid.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  PhaseOneExactSequenceAndMachine();
  PhaseTwoThreeFourAndUnchangedDelays();
  SelectorBoundariesAndClearInput();
  EarlyExitsUnknownPrefixesAndKnownness();
  CallRefusalsBudgetsAndLiveMutation();
  MappingAliasesReturnsAndDeterminism();
  std::printf("game camera phase select: %u checks\n", checks);
}
