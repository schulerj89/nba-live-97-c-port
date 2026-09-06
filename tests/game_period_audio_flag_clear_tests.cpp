#include "recovered/game_period_audio_flag_clear.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
std::size_t checks;

void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "period audio flag clear check %zu failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define CHECK(value) checkAt((value), __LINE__)

bool sameWord(const Nba97GamePeriodAudioFlagClearWord &left,
              const Nba97GamePeriodAudioFlagClearWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool sameMachine(const Nba97GamePeriodAudioFlagClearMachine &left,
                 const Nba97GamePeriodAudioFlagClearMachine &right) {
  for (unsigned index = 0u; index != 32u; ++index)
    if (!sameWord(left.registers.gpr[index], right.registers.gpr[index]))
      return false;
  return sameWord(left.hi, right.hi) && sameWord(left.lo, right.lo);
}

struct Fixture {
  static constexpr std::uint32_t Flag = UINT32_C(0x800b1fd5);
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(3u, 0xa5u);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(3u, 1u);
  Nba97GameTextRegion region{Flag - 1u, bytes.data(), known.data(),
                             bytes.size()};
  std::array<Nba97GamePeriodAudioFlagClearAccess, 2> journal{};
  Nba97GamePeriodAudioFlagClearContext context{};
  Nba97GamePeriodAudioFlagClearProgress progress{};

  explicit Fixture(std::uint8_t flag = 0xa5u) {
    bytes[1] = flag;
    context.memory = {&region, 1u};
    context.operation_budget = 1u;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned index = 0u; index != 32u; ++index) {
      context.machine.registers.gpr[index] = {
          UINT32_C(0x51000000) + index * UINT32_C(0x010101),
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    }
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        UINT32_C(0x81234568), 15u};
    context.machine.hi = {UINT32_C(0x12345678), 5u};
    context.machine.lo = {UINT32_C(0x89abcdef), 10u};
  }

  int run() { return nba97_game_period_audio_flag_clear(&context, &progress); }
};

void allFlagBytesAndExactStore() {
  for (unsigned previous = 0u; previous != 256u; ++previous) {
    Fixture fixture(static_cast<std::uint8_t>(previous));
    const auto incoming = fixture.context.machine;
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE && fixture.progress.completed);
    CHECK(fixture.bytes[0] == 0xa5u && fixture.bytes[1] == 0u &&
          fixture.bytes[2] == 0xa5u && fixture.known[0] == 1u &&
          fixture.known[1] == 1u && fixture.known[2] == 1u);
    CHECK(fixture.progress.operations == 1u &&
          fixture.progress.accesses == 1u && fixture.progress.stores == 1u &&
          fixture.progress.access_events == 1u &&
          fixture.progress.instruction_count == 4u &&
          fixture.progress.stopped_pc == 0u &&
          fixture.progress.stopped_address == 0u);
    CHECK(
        fixture.journal[0].pc == UINT32_C(0x8002a248) &&
        fixture.journal[0].address == Fixture::Flag &&
        fixture.journal[0].value == 0u && fixture.journal[0].operation == 1u &&
        fixture.journal[0].width == 1u && fixture.journal[0].known_mask == 1u &&
        fixture.journal[0].kind == NBA97_GAME_PERIOD_AUDIO_FLAG_CLEAR_STORE);
    CHECK(fixture.progress.machine.registers.gpr[1].word ==
              UINT32_C(0x800b0000) &&
          fixture.progress.machine.registers.gpr[1].known_mask == 15u);
    for (unsigned index = 0u; index != 32u; ++index)
      if (index != 1u)
        CHECK(sameWord(fixture.progress.machine.registers.gpr[index],
                       incoming.registers.gpr[index]));
    CHECK(sameWord(fixture.progress.machine.hi, incoming.hi) &&
          sameWord(fixture.progress.machine.lo, incoming.lo));
  }
}

void BudgetAndMappedKnownness() {
  Fixture budget(0x6du);
  budget.context.operation_budget = 0u;
  CHECK(budget.run() == NBA97_TEXT_LIMIT && budget.bytes[1] == 0x6du &&
        budget.progress.operations == 0u && budget.progress.accesses == 0u &&
        budget.progress.stores == 0u && budget.progress.access_events == 0u &&
        budget.progress.instruction_count == 2u &&
        budget.progress.stopped_pc == UINT32_C(0x8002a248) &&
        budget.progress.stopped_address == Fixture::Flag &&
        budget.progress.machine.registers.gpr[1].word == UINT32_C(0x800b0000));

  Fixture unknown(0xefu);
  unknown.known[1] = 0u;
  CHECK(unknown.run() == NBA97_TEXT_COMPLETE && unknown.bytes[1] == 0u &&
        unknown.known[1] == 1u);

  Fixture malformed(0x7bu);
  malformed.known[1] = 2u;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT && malformed.bytes[1] == 0x7bu &&
        malformed.known[1] == 2u && malformed.progress.operations == 1u &&
        malformed.progress.accesses == 1u && malformed.progress.stores == 0u &&
        malformed.progress.access_events == 0u &&
        malformed.progress.instruction_count == 2u);

  Fixture implicitKnown(0xccu);
  implicitKnown.region.known = nullptr;
  CHECK(implicitKnown.run() == NBA97_TEXT_COMPLETE &&
        implicitKnown.bytes[1] == 0u);

  Fixture truncatedJournal;
  truncatedJournal.context.access_journal = nullptr;
  truncatedJournal.context.access_journal_capacity = 0u;
  CHECK(truncatedJournal.run() == NBA97_TEXT_COMPLETE &&
        truncatedJournal.progress.access_events == 1u);
}

void MappingValidationAndKsegAddress() {
  Fixture emptyMap;
  emptyMap.context.memory = {nullptr, 0u};
  CHECK(emptyMap.run() == NBA97_TEXT_RESOURCE &&
        emptyMap.progress.operations == 1u &&
        emptyMap.progress.stopped_address == Fixture::Flag);

  Fixture missingRegions;
  missingRegions.context.memory.region = nullptr;
  CHECK(missingRegions.run() == NBA97_TEXT_ARGUMENT);
  Fixture nullData;
  nullData.region.data = nullptr;
  CHECK(nullData.run() == NBA97_TEXT_ARGUMENT);
  Fixture emptyRegion;
  emptyRegion.region.size = 0u;
  CHECK(emptyRegion.run() == NBA97_TEXT_ARGUMENT);
  Fixture sizeMax;
  sizeMax.region.size = static_cast<std::size_t>(-1);
  CHECK(sizeMax.run() == NBA97_TEXT_ARGUMENT);
  Fixture overflow;
  overflow.region.base = UINT32_C(0xfffffff0);
  overflow.region.size = 32u;
  CHECK(overflow.run() == NBA97_TEXT_ARGUMENT);
  Fixture overlap;
  std::array<Nba97GameTextRegion, 2> regions{{
      overlap.region,
      {Fixture::Flag, overlap.bytes.data() + 1u, overlap.known.data() + 1u, 1u},
  }};
  overlap.context.memory = {regions.data(), regions.size()};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture badJournal;
  badJournal.context.access_journal = nullptr;
  CHECK(badJournal.run() == NBA97_TEXT_ARGUMENT);

  Fixture exactKseg(0x91u);
  exactKseg.region.base = Fixture::Flag;
  exactKseg.region.data = exactKseg.bytes.data() + 1u;
  exactKseg.region.known = exactKseg.known.data() + 1u;
  exactKseg.region.size = 1u;
  exactKseg.context.memory = {&exactKseg.region, 1u};
  CHECK(exactKseg.run() == NBA97_TEXT_COMPLETE && exactKseg.bytes[1] == 0u &&
        exactKseg.journal[0].address == Fixture::Flag);
}

void ReturnPrefixesMachineValidationAndRepeatability() {
  for (unsigned mask = 0u; mask != 15u; ++mask) {
    Fixture fixture(0x39u);
    fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = static_cast<std::uint8_t>(mask);
    CHECK(fixture.run() == NBA97_TEXT_UNKNOWN && fixture.bytes[1] == 0u &&
          fixture.progress.stores == 1u &&
          fixture.progress.instruction_count == 4u &&
          fixture.progress.stopped_pc == UINT32_C(0x8002a24c));
  }
  for (std::uint32_t low = 1u; low != 4u; ++low) {
    Fixture fixture(0x4au);
    fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word =
        UINT32_C(0x81234568) | low;
    CHECK(fixture.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
          fixture.bytes[1] == 0u && fixture.progress.stores == 1u &&
          fixture.progress.instruction_count == 4u &&
          fixture.progress.stopped_pc == UINT32_C(0x8002a24c) &&
          fixture.progress.stopped_address == (UINT32_C(0x81234568) | low));
  }

  Fixture invalidZero;
  invalidZero.context.machine.registers.gpr[0].word = 1u;
  CHECK(invalidZero.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalidGpr;
  invalidGpr.context.machine.registers.gpr[19].known_mask = 16u;
  CHECK(invalidGpr.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalidHi;
  invalidHi.context.machine.hi.known_mask = 16u;
  CHECK(invalidHi.run() == NBA97_TEXT_ARGUMENT);
  Fixture invalidLo;
  invalidLo.context.machine.lo.known_mask = 16u;
  CHECK(invalidLo.run() == NBA97_TEXT_ARGUMENT);
  Fixture nulls;
  CHECK(nba97_game_period_audio_flag_clear(nullptr, &nulls.progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_period_audio_flag_clear(&nulls.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);

  Fixture first(0x87u);
  Fixture second(0x87u);
  CHECK(first.run() == NBA97_TEXT_COMPLETE &&
        second.run() == NBA97_TEXT_COMPLETE && first.bytes == second.bytes &&
        first.known == second.known &&
        sameMachine(first.progress.machine, second.progress.machine));
}
} // namespace

int main() {
  allFlagBytesAndExactStore();
  BudgetAndMappedKnownness();
  MappingValidationAndKsegAddress();
  ReturnPrefixesMachineValidationAndRepeatability();
  std::printf("game period audio flag clear: %zu checks\n", checks);
}
