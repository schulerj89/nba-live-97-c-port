#include "recovered/game_camera_remainder_gate.h"

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
    std::fprintf(stderr, "camera remainder gate check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define CHECK(value) checkAt((value), __LINE__)

bool sameWord(const Nba97GameCameraRemainderGateWord &left,
              const Nba97GameCameraRemainderGateWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool sameMachine(const Nba97GameCameraRemainderGateMachine &left,
                 const Nba97GameCameraRemainderGateMachine &right) {
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
  std::array<Nba97GameCameraRemainderGateAccess, 4> journal{};
  Nba97GameCameraRemainderGateContext context{};
  Nba97GameCameraRemainderGateProgress progress{};

  Fixture() {
    context.memory = {&region, 1};
    context.operation_budget = 2;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned index = 0; index < 32; ++index)
      context.machine.registers.gpr[index] = {
          UINT32_C(0x21000000) + index,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[29] = {UINT32_C(0x801ff000), 15};
    context.machine.registers.gpr[31] = {UINT32_C(0x81234568), 15};
    context.machine.hi = {UINT32_C(0x11223344), 5};
    context.machine.lo = {UINT32_C(0x55667788), 10};
    putSource(0);
  }

  std::size_t at(std::uint32_t address) const { return address - Ram; }

  void putSource(std::uint32_t value, std::uint8_t mask = 15) {
    for (unsigned byte = 0; byte < 4; ++byte) {
      bytes[at(0x800fc9ac) + byte] =
          static_cast<std::uint8_t>(value >> (byte * 8u));
      known[at(0x800fc9ac) + byte] =
          static_cast<std::uint8_t>((mask >> byte) & 1u);
    }
  }

  int run() { return nba97_game_camera_remainder_gate(&context, &progress); }
};

std::uint32_t expectedRemainder(std::uint32_t source) {
  const auto signedSource = static_cast<std::int64_t>(
      source & UINT32_C(0x80000000)
          ? static_cast<std::int64_t>(source) - INT64_C(0x100000000)
          : source);
  const auto remainder = signedSource % 2048;
  return static_cast<std::uint32_t>(remainder);
}

void SignedBoundariesAndMachine() {
  const std::array<std::uint32_t, 17> sources{{
      0,
      1,
      UINT32_MAX,
      49,
      static_cast<std::uint32_t>(-49),
      50,
      static_cast<std::uint32_t>(-50),
      51,
      static_cast<std::uint32_t>(-51),
      2047,
      static_cast<std::uint32_t>(-2047),
      2048,
      static_cast<std::uint32_t>(-2048),
      static_cast<std::uint32_t>(-2049),
      UINT32_C(0x80000000),
      UINT32_C(0x7fffffff),
      UINT32_C(0xfffff800),
  }};
  for (const auto source : sources) {
    Fixture fixture;
    const auto incoming = fixture.context.machine;
    const std::uint32_t remainder = expectedRemainder(source);
    const std::uint32_t biased = remainder + 50u;
    const std::uint32_t expected = biased < 101u ? 1u : 0u;
    fixture.putSource(source);
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE && fixture.progress.completed &&
          fixture.progress.source_value.word == source &&
          fixture.progress.source_value.known_mask == 15 &&
          fixture.progress.remainder_value.word == remainder &&
          fixture.progress.returned_value.word == expected &&
          fixture.progress.returned_value.known_mask == 15 &&
          fixture.progress.negative == (source >> 31u) &&
          fixture.progress.instruction_count == ((source >> 31u) ? 12u : 11u));
    CHECK(fixture.progress.adjusted_value.word ==
              ((source >> 31u) ? source + UINT32_C(0x7ff) : source) &&
          fixture.progress.operations == 1 && fixture.progress.accesses == 1 &&
          fixture.progress.reads == 1 && fixture.progress.access_events == 1 &&
          fixture.journal[0].pc == 0x8007a46c &&
          fixture.journal[0].address == 0x800fc9ac &&
          fixture.journal[0].value == source && fixture.journal[0].width == 4 &&
          fixture.journal[0].known_mask == 15);
    CHECK(fixture.progress.machine.registers.gpr[2].word == expected &&
          fixture.progress.machine.registers.gpr[2].known_mask == 15 &&
          fixture.progress.machine.registers.gpr[3].word == source &&
          fixture.progress.machine.registers.gpr[3].known_mask == 15);
    for (unsigned index = 0; index < 32; ++index)
      if (index != 2 && index != 3)
        CHECK(sameWord(fixture.progress.machine.registers.gpr[index],
                       incoming.registers.gpr[index]));
    CHECK(sameWord(fixture.progress.machine.hi, incoming.hi) &&
          sameWord(fixture.progress.machine.lo, incoming.lo));
  }
}

void DiscardedBitsAndAllInputMasks() {
  for (const auto pair : std::array<std::array<std::uint32_t, 2>, 3>{{
           {{UINT32_C(0x00000032), UINT32_C(0x7ffff832)}},
           {{UINT32_C(0xffffffce), UINT32_C(0x800007ce)}},
           {{UINT32_C(0x00000033), UINT32_C(0x7ffff833)}},
       }}) {
    Fixture first;
    Fixture second;
    first.putSource(pair[0]);
    second.putSource(pair[1]);
    CHECK(first.run() == NBA97_TEXT_COMPLETE &&
          second.run() == NBA97_TEXT_COMPLETE &&
          first.progress.remainder_value.word ==
              second.progress.remainder_value.word &&
          first.progress.returned_value.word ==
              second.progress.returned_value.word);
  }

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture fixture;
    fixture.putSource(0, static_cast<std::uint8_t>(mask));
    const int result = fixture.run();
    if (!(mask & 8u)) {
      CHECK(result == NBA97_TEXT_UNKNOWN &&
            fixture.progress.stopped_pc == 0x8007a474 &&
            fixture.progress.instruction_count == 5 &&
            fixture.progress.machine.registers.gpr[2].word == 0 &&
            fixture.progress.machine.registers.gpr[2].known_mask == mask &&
            fixture.progress.machine.registers.gpr[3].known_mask == mask);
    } else {
      CHECK(result == NBA97_TEXT_COMPLETE &&
            fixture.progress.returned_value.word == 1 &&
            fixture.progress.returned_value.known_mask ==
                (((mask & 3u) == 3u) ? 15u : 14u) &&
            fixture.progress.remainder_value.known_mask ==
                static_cast<unsigned>(12u | (mask & 3u)));
    }
  }
}

void LoadFailuresAndMappingValidation() {
  Fixture budget;
  budget.context.operation_budget = 0;
  CHECK(budget.run() == NBA97_TEXT_LIMIT && budget.progress.operations == 0 &&
        budget.progress.accesses == 0 && budget.progress.reads == 0 &&
        budget.progress.stopped_pc == 0x8007a46c &&
        budget.progress.machine.registers.gpr[3].word == 0x80100000 &&
        budget.progress.machine.registers.gpr[3].known_mask == 15 &&
        budget.progress.instruction_count == 2);

  Fixture malformed;
  malformed.known[malformed.at(0x800fc9ac) + 3] = 2;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations == 1 &&
        malformed.progress.accesses == 1 && malformed.progress.reads == 0 &&
        malformed.progress.machine.registers.gpr[3].word == 0x80100000 &&
        malformed.progress.access_events == 0);

  Fixture missing;
  missing.context.memory = {nullptr, 0};
  CHECK(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_address == 0x800fc9ac &&
        missing.progress.machine.registers.gpr[3].word == 0x80100000);

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
  Fixture nullRegion;
  nullRegion.context.memory = {nullptr, 1};
  CHECK(nullRegion.run() == NBA97_TEXT_ARGUMENT);
  Fixture nullJournal;
  nullJournal.context.access_journal = nullptr;
  CHECK(nullJournal.run() == NBA97_TEXT_ARGUMENT);
  Fixture zeroSize;
  zeroSize.region.size = 0;
  CHECK(zeroSize.run() == NBA97_TEXT_ARGUMENT);

  Fixture nullKnown;
  nullKnown.region.known = nullptr;
  nullKnown.putSource(50);
  CHECK(nullKnown.run() == NBA97_TEXT_COMPLETE &&
        nullKnown.progress.source_value.known_mask == 15);

  Fixture wrappedRegion;
  std::array<std::uint8_t, 4> high{};
  std::array<std::uint8_t, 4> highKnown{{1, 1, 1, 1}};
  std::array<Nba97GameTextRegion, 2> wrapRegions{{
      wrappedRegion.region,
      {0xfffffffc, high.data(), highKnown.data(), high.size()},
  }};
  wrappedRegion.context.memory = {wrapRegions.data(), wrapRegions.size()};
  CHECK(wrappedRegion.run() == NBA97_TEXT_COMPLETE);
}

void ReturnDelayAndDeterminism() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture fixture;
    fixture.putSource(50);
    fixture.context.machine.registers.gpr[31].known_mask =
        static_cast<std::uint8_t>(mask);
    const int result = fixture.run();
    if (mask == 15)
      CHECK(result == NBA97_TEXT_COMPLETE);
    else
      CHECK(result == NBA97_TEXT_UNKNOWN &&
            fixture.progress.stopped_pc == 0x8007a490 &&
            fixture.progress.returned_value.word == 1 &&
            fixture.progress.returned_value.known_mask == 15 &&
            fixture.progress.machine.registers.gpr[2].word == 1 &&
            fixture.progress.instruction_count == 11);
  }
  Fixture unaligned;
  unaligned.putSource(static_cast<std::uint32_t>(-51));
  unaligned.context.machine.registers.gpr[31].word |= 1;
  CHECK(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x8007a490 &&
        unaligned.progress.returned_value.word == 0 &&
        unaligned.progress.instruction_count == 12);

  Fixture first;
  Fixture second;
  first.putSource(static_cast<std::uint32_t>(-2049), 11);
  second.putSource(static_cast<std::uint32_t>(-2049), 11);
  const auto bytes = first.bytes;
  const auto known = first.known;
  CHECK(first.run() == NBA97_TEXT_COMPLETE &&
        second.run() == NBA97_TEXT_COMPLETE && first.bytes == bytes &&
        first.known == known && first.bytes == second.bytes &&
        first.known == second.known &&
        sameMachine(first.progress.machine, second.progress.machine));

  Fixture invalid;
  invalid.context.machine.hi.known_mask = 16;
  CHECK(invalid.run() == NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_remainder_gate(nullptr, &invalid.progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_remainder_gate(&invalid.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  SignedBoundariesAndMachine();
  DiscardedBitsAndAllInputMasks();
  LoadFailuresAndMappingValidation();
  ReturnDelayAndDeterminism();
  std::printf("game camera remainder gate: %u checks\n", checks);
}
