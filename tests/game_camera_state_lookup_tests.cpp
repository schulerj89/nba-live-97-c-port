#include "recovered/game_camera_state_lookup.h"

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
    std::fprintf(stderr, "camera state lookup check %u failed at %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define CHECK(value) checkAt((value), __LINE__)

bool sameWord(const Nba97GameCameraStateLookupWord &left,
              const Nba97GameCameraStateLookupWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool sameMachine(const Nba97GameCameraStateLookupMachine &left,
                 const Nba97GameCameraStateLookupMachine &right) {
  for (unsigned index = 0; index < 32; ++index)
    if (!sameWord(left.registers.gpr[index], right.registers.gpr[index]))
      return false;
  return sameWord(left.hi, right.hi) && sameWord(left.lo, right.lo);
}

struct Fixture {
  static constexpr std::uint32_t Ram = UINT32_C(0x80000000);
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameCameraStateLookupAccess, 8> journal{};
  Nba97GameCameraStateLookupContext context{};
  Nba97GameCameraStateLookupProgress progress{};

  Fixture() {
    context.memory = {&region, 1};
    context.operation_budget = 8;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned index = 0; index < 32; ++index)
      context.machine.registers.gpr[index] = {
          UINT32_C(0x31000000) + index,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[29] = {UINT32_C(0x801ff000), 15};
    context.machine.registers.gpr[31] = {UINT32_C(0x81234568), 15};
    context.machine.hi = {UINT32_C(0x11223344), 5};
    context.machine.lo = {UINT32_C(0x55667788), 10};
    put(0x800fc9ac, 0);
    put(0x800bc204, UINT32_C(0x11223344));
    put(0x800bc208, UINT32_C(0x55667788));
    put(0x800bc240, UINT32_C(0x99aabbcc));
  }

  std::size_t at(std::uint32_t address) const { return address - Ram; }

  void put(std::uint32_t address, std::uint32_t value, std::uint8_t mask = 15) {
    for (unsigned byte = 0; byte < 4; ++byte) {
      bytes[at(address) + byte] =
          static_cast<std::uint8_t>(value >> (byte * 8u));
      known[at(address) + byte] =
          static_cast<std::uint8_t>((mask >> byte) & 1u);
    }
  }

  std::uint32_t get(std::uint32_t address) const {
    std::uint32_t result = 0;
    for (unsigned byte = 0; byte < 4; ++byte)
      result |= std::uint32_t(bytes[at(address) + byte]) << (byte * 8u);
    return result;
  }

  int run() { return nba97_game_camera_state_lookup(&context, &progress); }
};

void PositiveZeroAndNegativeTables() {
  struct Case {
    std::uint32_t source;
    std::uint32_t address;
    std::uint32_t value;
    std::uint32_t index;
    bool negative;
  };
  const std::array<Case, 4> cases{{
      {0, 0x800bc204, 0x11223344, 0, false},
      {0x00000100, 0x800bc208, 0x55667788, 1, false},
      {0xffffffff, 0x800bc240, 0x99aabbcc, 7, true},
      {0x0fffffff, 0x800bc240, 0x99aabbcc, 7, true},
  }};
  for (const auto &test : cases) {
    Fixture fixture;
    const auto incoming = fixture.context.machine;
    fixture.put(0x800fc9ac, test.source);
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE && fixture.progress.completed &&
          fixture.progress.negative_table ==
              static_cast<unsigned>(test.negative) &&
          fixture.progress.signed_index.word == test.index &&
          fixture.progress.lookup_address.word == test.address &&
          fixture.progress.returned_value.word == test.value &&
          fixture.progress.machine.registers.gpr[2].word == test.value);
    CHECK(fixture.progress.operations == 2 && fixture.progress.reads == 2 &&
          fixture.progress.access_events == 2 &&
          fixture.journal[0].pc == 0x8007a414 &&
          fixture.journal[0].address == 0x800fc9ac &&
          fixture.journal[1].pc == (test.negative ? 0x8007a45c : 0x8007a43c) &&
          fixture.journal[1].address == test.address);
    CHECK(fixture.progress.machine.registers.gpr[1].word ==
              UINT32_C(0x800c0000) + (test.index << 2u) &&
          fixture.progress.machine.registers.gpr[1].known_mask == 15);
    for (unsigned index = 4; index < 31; ++index)
      CHECK(sameWord(fixture.progress.machine.registers.gpr[index],
                     incoming.registers.gpr[index]));
    CHECK(sameWord(fixture.progress.machine.registers.gpr[31],
                   incoming.registers.gpr[31]) &&
          sameWord(fixture.progress.machine.hi, incoming.hi) &&
          sameWord(fixture.progress.machine.lo, incoming.lo));
  }
}

void SignTransitionsDroppedBitsAndUncheckedRange() {
  Fixture positiveEdge;
  positiveEdge.put(0x800fc9ac, 0x07ffffff);
  CHECK(positiveEdge.run() == NBA97_TEXT_RESOURCE &&
        positiveEdge.progress.negative_table == 0 &&
        positiveEdge.progress.signed_index.word == 0x0007ffff &&
        positiveEdge.progress.lookup_address.word == 0x802bc200 &&
        positiveEdge.progress.stopped_pc == 0x8007a43c);

  Fixture negativeEdge;
  negativeEdge.put(0x800fc9ac, 0x08000000);
  CHECK(negativeEdge.run() == NBA97_TEXT_RESOURCE &&
        negativeEdge.progress.negative_table == 1 &&
        negativeEdge.progress.signed_index.word == 0xfff80008 &&
        negativeEdge.progress.lookup_address.word == 0x7febc244 &&
        negativeEdge.progress.stopped_pc == 0x8007a45c);

  for (std::uint32_t source : {UINT32_C(0x1fffffff), UINT32_C(0xffffffff)}) {
    Fixture dropped;
    dropped.put(0x800fc9ac, source);
    CHECK(dropped.run() == NBA97_TEXT_COMPLETE &&
          dropped.progress.lookup_address.word == 0x800bc240 &&
          dropped.progress.machine.registers.gpr[2].word == 0x99aabbcc);
  }
  Fixture highNibble;
  highNibble.put(0x800fc9ac, 0xf0000000);
  CHECK(highNibble.run() == NBA97_TEXT_COMPLETE &&
        highNibble.progress.lookup_address.word == 0x800bc204 &&
        highNibble.progress.machine.registers.gpr[2].word == 0x11223344);
}

void PartialKnownnessAndBranchDelay() {
  Fixture unknownSign;
  unknownSign.put(0x800fc9ac, 0x08000100, 7);
  CHECK(unknownSign.run() == NBA97_TEXT_UNKNOWN &&
        unknownSign.progress.stopped_pc == 0x8007a424 &&
        unknownSign.progress.machine.registers.gpr[2].word == 0x8000 &&
        unknownSign.progress.machine.registers.gpr[2].known_mask == 15 &&
        unknownSign.progress.masked_value.known_mask == 7 &&
        unknownSign.progress.instruction_count == 7);

  for (unsigned lowMask = 0; lowMask < 2; ++lowMask) {
    Fixture droppedLow;
    droppedLow.put(0x800fc9ac, 0x00000100,
                   static_cast<std::uint8_t>(14u | lowMask));
    CHECK(droppedLow.run() == NBA97_TEXT_COMPLETE &&
          droppedLow.progress.signed_index.word == 1 &&
          droppedLow.progress.signed_index.known_mask == 15 &&
          droppedLow.progress.lookup_address.known_mask == 15 &&
          droppedLow.progress.machine.registers.gpr[2].word == 0x55667788);
  }

  for (unsigned missing = 1; missing <= 2; ++missing) {
    Fixture partialIndex;
    partialIndex.put(0x800fc9ac, 0x00000100,
                     static_cast<std::uint8_t>(15u & ~(1u << missing)));
    const int result = partialIndex.run();
    CHECK(result == NBA97_TEXT_UNKNOWN &&
          partialIndex.progress.stopped_pc == 0x8007a43c &&
          partialIndex.progress.lookup_address.known_mask != 15);
  }

  Fixture partialTable;
  partialTable.put(0x800bc204, 0x12345678, 5);
  CHECK(partialTable.run() == NBA97_TEXT_COMPLETE &&
        partialTable.progress.returned_value.word == 0x12345678 &&
        partialTable.progress.returned_value.known_mask == 5 &&
        partialTable.progress.machine.registers.gpr[2].known_mask == 5);
}

void BudgetsMalformedAndReturns() {
  for (std::size_t budget = 0; budget <= 1; ++budget) {
    Fixture fixture;
    fixture.context.operation_budget = budget;
    CHECK(fixture.run() == NBA97_TEXT_LIMIT &&
          fixture.progress.operations == budget &&
          fixture.progress.stopped_pc ==
              (budget == 0 ? 0x8007a414 : 0x8007a43c));
  }

  Fixture malformedInput;
  malformedInput.known[malformedInput.at(0x800fc9ac) + 2] = 2;
  CHECK(malformedInput.run() == NBA97_TEXT_ARGUMENT &&
        malformedInput.progress.stopped_pc == 0x8007a414 &&
        malformedInput.progress.machine.registers.gpr[2].word == 0x80100000 &&
        malformedInput.progress.machine.registers.gpr[2].known_mask == 15);
  Fixture malformedTable;
  malformedTable.known[malformedTable.at(0x800bc204) + 3] = 2;
  CHECK(malformedTable.run() == NBA97_TEXT_ARGUMENT &&
        malformedTable.progress.stopped_pc == 0x8007a43c &&
        malformedTable.progress.machine.registers.gpr[2].word == 0 &&
        malformedTable.progress.machine.registers.gpr[2].known_mask == 15);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture fixture;
    fixture.context.machine.registers.gpr[31].known_mask =
        static_cast<std::uint8_t>(mask);
    const int result = fixture.run();
    if (mask == 15)
      CHECK(result == NBA97_TEXT_COMPLETE);
    else
      CHECK(result == NBA97_TEXT_UNKNOWN &&
            fixture.progress.stopped_pc == 0x8007a460 &&
            fixture.progress.returned_value.word == 0x11223344 &&
            fixture.progress.instruction_count == 16);
  }
  Fixture unalignedRa;
  unalignedRa.context.machine.registers.gpr[31].word |= 1;
  CHECK(unalignedRa.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unalignedRa.progress.stopped_pc == 0x8007a460 &&
        unalignedRa.progress.instruction_count == 16);
}

void MappingValidationAndDeterminism() {
  Fixture missing;
  missing.context.memory = {nullptr, 0};
  CHECK(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_address == 0x800fc9ac);
  Fixture overlap;
  std::array<Nba97GameTextRegion, 2> regions{{
      overlap.region,
      {Fixture::Ram + 4, overlap.bytes.data() + 4, overlap.known.data() + 4, 4},
  }};
  overlap.context.memory = {regions.data(), regions.size()};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture sizeMax;
  sizeMax.region.size = static_cast<std::size_t>(-1);
  CHECK(sizeMax.run() == NBA97_TEXT_ARGUMENT);
  Fixture nullJournal;
  nullJournal.context.access_journal = nullptr;
  CHECK(nullJournal.run() == NBA97_TEXT_ARGUMENT);

  Fixture wrappedRegion;
  std::array<std::uint8_t, 4> high{};
  std::array<std::uint8_t, 4> highKnown{{1, 1, 1, 1}};
  std::array<Nba97GameTextRegion, 2> wrapRegions{{
      wrappedRegion.region,
      {0xfffffffc, high.data(), highKnown.data(), high.size()},
  }};
  wrappedRegion.context.memory = {wrapRegions.data(), wrapRegions.size()};
  CHECK(wrappedRegion.run() == NBA97_TEXT_COMPLETE);

  Fixture first;
  Fixture second;
  const auto firstBytes = first.bytes;
  const auto firstKnown = first.known;
  CHECK(first.run() == NBA97_TEXT_COMPLETE &&
        second.run() == NBA97_TEXT_COMPLETE && first.bytes == firstBytes &&
        first.known == firstKnown && first.bytes == second.bytes &&
        first.known == second.known &&
        sameMachine(first.progress.machine, second.progress.machine));
  Fixture invalid;
  invalid.context.machine.lo.known_mask = 16;
  CHECK(invalid.run() == NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_state_lookup(nullptr, &invalid.progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_state_lookup(&invalid.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  PositiveZeroAndNegativeTables();
  SignTransitionsDroppedBitsAndUncheckedRange();
  PartialKnownnessAndBranchDelay();
  BudgetsMalformedAndReturns();
  MappingValidationAndDeterminism();
  std::printf("game camera state lookup: %u checks\n", checks);
}
