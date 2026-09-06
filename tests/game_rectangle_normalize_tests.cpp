#include "recovered/game_rectangle_normalize.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
#define check(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      std::cerr << "check failed: " #condition " at line " << __LINE__ << '\n'; \
      std::abort();                                                             \
    }                                                                           \
  } while (0)

struct Fixture {
  static constexpr std::uint32_t Base = 0x80000000u;
  static constexpr std::uint32_t Rectangle = 0x80010000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97GameRectangleNormalizeMachine machine{};
  Nba97GameRectangleNormalizeProgress progress{};
  std::array<Nba97GameRectangleNormalizeAccess, 8> journal{};
  std::size_t budget = 8;

  Fixture(std::uint16_t width = 2, std::uint16_t height = 2) {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x11000000u + i * 0x01010101u,
                                  std::uint8_t((i * 7u) & 15u)};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[4] = {Rectangle, 15};
    machine.registers.gpr[31] = {0x80012340u, 15};
    machine.hi = {0xabcdef01u, 5};
    machine.lo = {0x12345678u, 10};
    put(Rectangle + 4u, width, 2);
    put(Rectangle + 6u, height, 2);
  }
  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i)
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[address - Base + i]) << (i * 8u);
    return value;
  }
  int run() {
    Nba97GameRectangleNormalizeContext context{};
    context.memory = {&region, 1};
    context.operation_budget = budget;
    context.machine = machine;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    return nba97_game_rectangle_normalize(&context, &progress);
  }
};

bool sameWord(const Nba97GameRectangleNormalizeWord &a,
              const Nba97GameRectangleNormalizeWord &b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}
bool sameMachine(const Nba97GameRectangleNormalizeMachine &a,
                 const Nba97GameRectangleNormalizeMachine &b) {
  for (unsigned i = 0; i < 32; ++i)
    if (!sameWord(a.registers.gpr[i], b.registers.gpr[i]))
      return false;
  return sameWord(a.hi, b.hi) && sameWord(a.lo, b.lo);
}

void evenWidthsSkipHeight() {
  for (const std::uint16_t width : {std::uint16_t(0), std::uint16_t(2),
                                    std::uint16_t(0x8000)}) {
    Fixture f(width, 0xfffeu);
    const auto before = f.machine;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
          f.progress.instruction_count == 7 && f.progress.operations == 1 &&
          f.progress.reads == 1 && f.progress.stores == 0 &&
          f.get(Fixture::Rectangle + 6u, 2) == 0xfffeu &&
          f.progress.machine.registers.gpr[2].word == 0 &&
          f.progress.machine.registers.gpr[2].known_mask == 15);
    for (unsigned i = 0; i < 32; ++i)
      if (i != 2)
        check(sameWord(f.progress.machine.registers.gpr[i],
                       before.registers.gpr[i]));
    check(sameWord(f.progress.machine.hi, before.hi) &&
          sameWord(f.progress.machine.lo, before.lo));
    check(f.journal[0].pc == 0x80094440u &&
          f.journal[0].address == Fixture::Rectangle + 4u &&
          f.journal[0].kind == NBA97_GAME_RECTANGLE_NORMALIZE_READ);
  }
}

void oddHeightRuleAndAccessOrder() {
  const std::array<std::pair<std::uint16_t, std::uint16_t>, 4> cases{{
      {std::uint16_t(0), std::uint16_t(1)},
      {std::uint16_t(1), std::uint16_t(1)},
      {std::uint16_t(0xfffeu), std::uint16_t(0xffffu)},
      {std::uint16_t(0xffffu), std::uint16_t(0xffffu)}}};
  for (const auto &values : cases) {
    Fixture f(0xffffu, values.first);
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.instruction_count == 11 &&
          f.progress.operations == 3 && f.progress.reads == 2 &&
          f.progress.stores == 1 &&
          f.get(Fixture::Rectangle + 6u, 2) == values.second &&
          f.progress.machine.registers.gpr[2].word == values.second &&
          f.progress.machine.registers.gpr[2].known_mask == 15);
    check(f.journal[0].pc == 0x80094440u &&
          f.journal[1].pc == 0x80094454u &&
          f.journal[2].pc == 0x80094460u &&
          f.journal[1].kind == NBA97_GAME_RECTANGLE_NORMALIZE_READ &&
          f.journal[2].kind == NBA97_GAME_RECTANGLE_NORMALIZE_STORE);
  }
}

void unknownnessAndFailurePrefixes() {
  Fixture unknownLow(1, 2);
  unknownLow.known[Fixture::Rectangle + 4u - Fixture::Base] = 0;
  check(unknownLow.run() == NBA97_TEXT_UNKNOWN &&
        unknownLow.progress.stopped_pc == 0x8009444cu &&
        unknownLow.progress.operations == 1 &&
        unknownLow.progress.machine.registers.gpr[2].known_mask == 14);

  Fixture unknownHigh(2, 2);
  unknownHigh.known[Fixture::Rectangle + 5u - Fixture::Base] = 0;
  check(unknownHigh.run() == NBA97_TEXT_COMPLETE &&
        unknownHigh.progress.machine.registers.gpr[2].known_mask == 15 &&
        unknownHigh.progress.operations == 1);

  Fixture unknownHeight(1, 2);
  unknownHeight.known[Fixture::Rectangle + 6u - Fixture::Base] = 0;
  check(unknownHeight.run() == NBA97_TEXT_COMPLETE &&
        unknownHeight.progress.machine.registers.gpr[2].known_mask == 14 &&
        unknownHeight.known[Fixture::Rectangle + 6u - Fixture::Base] == 0);

  Fixture malformed(1, 2);
  malformed.known[Fixture::Rectangle + 6u - Fixture::Base] = 2;
  check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x80094454u &&
        malformed.progress.reads == 1 && malformed.progress.stores == 0 &&
        malformed.progress.machine.registers.gpr[2].word == 1 &&
        malformed.progress.machine.registers.gpr[2].known_mask == 15);

  Fixture unaligned;
  unaligned.machine.registers.gpr[4].word = Fixture::Rectangle + 1u;
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80094440u);
  Fixture unknownPointer;
  unknownPointer.machine.registers.gpr[4].known_mask = 7;
  check(unknownPointer.run() == NBA97_TEXT_UNKNOWN &&
        unknownPointer.progress.stopped_pc == 0x80094440u &&
        unknownPointer.progress.operations == 0);
  Fixture unmapped;
  unmapped.machine.registers.gpr[4].word = 0x70000000u;
  check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.stopped_pc == 0x80094440u);
}

void budgetsAndReturnTargets() {
  Fixture odd(1, 2);
  check(odd.run() == NBA97_TEXT_COMPLETE);
  for (std::size_t budget = 0; budget < odd.progress.operations; ++budget) {
    Fixture limited(1, 2);
    limited.budget = budget;
    check(limited.run() == NBA97_TEXT_LIMIT &&
          limited.progress.operations == budget &&
          limited.get(Fixture::Rectangle + 6u, 2) ==
              (budget < 3 ? 2u : 3u));
  }
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture ra(1, 2);
    ra.machine.registers.gpr[31].known_mask = std::uint8_t(mask);
    const int result = ra.run();
    check((mask == 15 && result == NBA97_TEXT_COMPLETE) ||
          (mask != 15 && result == NBA97_TEXT_UNKNOWN &&
           ra.progress.stopped_pc == 0x80094464u &&
           ra.progress.instruction_count == 11 &&
           ra.get(Fixture::Rectangle + 6u, 2) == 3));
  }
  Fixture misaligned(1, 2);
  misaligned.machine.registers.gpr[31].word |= 2u;
  check(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.stopped_pc == 0x80094464u &&
        misaligned.progress.instruction_count == 11);
}

void mappingsAliasesAndValidation() {
  std::array<std::uint8_t, 4> backing{{1, 0, 2, 0}};
  std::array<std::uint8_t, 4> known{{1, 1, 1, 1}};
  std::array<Nba97GameTextRegion, 2> alias{{
      {0x1004u, backing.data(), known.data(), 2},
      {0x1006u, backing.data(), known.data(), 2},
  }};
  Nba97GameRectangleNormalizeContext context{};
  Nba97GameRectangleNormalizeProgress progress{};
  for (auto &word : context.machine.registers.gpr)
    word = {0, 15};
  context.machine.registers.gpr[4] = {0x1000u, 15};
  context.machine.registers.gpr[31] = {0x80010000u, 15};
  context.memory = {alias.data(), alias.size()};
  context.operation_budget = 3;
  check(nba97_game_rectangle_normalize(&context, &progress) ==
            NBA97_TEXT_COMPLETE &&
        backing[0] == 1 && backing[1] == 0);

  std::uint8_t byte = 0;
  Nba97GameTextRegion bad{0xffffffffu, &byte, nullptr, 2};
  context.memory = {&bad, 1};
  check(nba97_game_rectangle_normalize(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);
  bad = {0, &byte, nullptr, SIZE_MAX};
  check(nba97_game_rectangle_normalize(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);
  bad = {0, &byte, nullptr, 0};
  check(nba97_game_rectangle_normalize(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);
  std::array<std::uint8_t, 8> overlapData{};
  std::array<Nba97GameTextRegion, 2> overlap{{
      {0x2000u, overlapData.data(), nullptr, overlapData.size()},
      {0x2004u, overlapData.data(), nullptr, 4},
  }};
  context.memory = {overlap.data(), overlap.size()};
  check(nba97_game_rectangle_normalize(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);

  std::array<std::uint8_t, 6> endBytes{{1, 0, 2, 0, 0, 0}};
  std::array<std::uint8_t, 6> endKnown{{1, 1, 1, 1, 1, 1}};
  Nba97GameTextRegion end{0xfffffffau, endBytes.data(), endKnown.data(), 6};
  context.memory = {&end, 1};
  context.machine.registers.gpr[4] = {0xfffffff6u, 15};
  context.operation_budget = 3;
  check(nba97_game_rectangle_normalize(&context, &progress) ==
            NBA97_TEXT_COMPLETE &&
        endBytes[2] == 3);

  Fixture invalid;
  invalid.machine.registers.gpr[0].word = 1;
  check(invalid.run() == NBA97_TEXT_ARGUMENT);
  invalid.machine.registers.gpr[0] = {0, 15};
  invalid.machine.hi.known_mask = 16;
  check(invalid.run() == NBA97_TEXT_ARGUMENT);
  invalid.machine.hi.known_mask = 15;
  invalid.machine.registers.gpr[20].known_mask = 16;
  check(invalid.run() == NBA97_TEXT_ARGUMENT);
  invalid.machine.registers.gpr[20].known_mask = 15;
  invalid.machine.lo.known_mask = 16;
  check(invalid.run() == NBA97_TEXT_ARGUMENT);
}

void deterministicFullState() {
  Fixture left(1, 2);
  Fixture right(1, 2);
  check(left.run() == NBA97_TEXT_COMPLETE && right.run() == NBA97_TEXT_COMPLETE &&
        left.bytes == right.bytes && left.known == right.known &&
        sameMachine(left.progress.machine, right.progress.machine));
}
} // namespace

int main() {
  evenWidthsSkipHeight();
  oddHeightRuleAndAccessOrder();
  unknownnessAndFailurePrefixes();
  budgetsAndReturnTargets();
  mappingsAliasesAndValidation();
  deterministicFullState();
  std::cout << "game_rectangle_normalize_tests: PASS\n";
}
