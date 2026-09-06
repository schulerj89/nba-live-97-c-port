#include "recovered/game_match_buffer_compress.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
int failures;
unsigned checks;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      std::cerr << __FILE__ << ':' << __LINE__ << ": " #x "\n";                \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

constexpr std::uint32_t kBase = 0x80000000u;
constexpr std::uint32_t kOld = 0x80010000u;
constexpr std::uint32_t kNew = 0x80030000u;
constexpr std::uint32_t kOut = 0x80050000u;
constexpr std::uint32_t kToggle = 0x800f9ffcu;

struct Fixture {
  std::vector<std::uint8_t> data = std::vector<std::uint8_t>(0x200000, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(data.size(), 1);
  Nba97GameTextRegion region{kBase, data.data(), known.data(), data.size()};
  std::vector<Nba97GameMatchBufferCompressAccess> journal =
      std::vector<Nba97GameMatchBufferCompressAccess>(160000);
  Nba97GameMatchBufferCompressContext context{};
  Nba97GameMatchBufferCompressProgress progress{};

  Fixture() {
    context.memory = {&region, 1};
    context.operation_budget = std::numeric_limits<std::size_t>::max();
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned i = 0; i < 32; ++i) {
      context.machine.registers.gpr[i].word = 0x12340000u + i * 0x101u;
      context.machine.registers.gpr[i].known_mask =
          static_cast<std::uint8_t>(i & 15u);
    }
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[4] = {kOld, 15};
    context.machine.registers.gpr[5] = {kNew, 15};
    context.machine.registers.gpr[6] = {kOut, 15};
    context.machine.registers.gpr[7] = {1, 15};
    context.machine.registers.gpr[31] = {0x80001234u, 15};
    context.machine.hi = {0x89abcdefu, 5};
    context.machine.lo = {0x10203040u, 10};
    put16(kToggle, 2);
  }

  std::size_t at(std::uint32_t address) const { return address - kBase; }
  void put16(std::uint32_t address, std::uint16_t value) {
    data[at(address)] = static_cast<std::uint8_t>(value);
    data[at(address) + 1] = static_cast<std::uint8_t>(value >> 8);
  }
  std::uint16_t get16(std::uint32_t address) const {
    return static_cast<std::uint16_t>(data[at(address)] |
                                      (data[at(address) + 1] << 8));
  }
  int run(std::uint32_t count = 1) {
    context.machine.registers.gpr[7] = {count, 15};
    return nba97_game_match_buffer_compress(&context, &progress);
  }
};

std::vector<std::uint8_t> expected(const std::vector<std::uint16_t> &old_values,
                                   const std::vector<std::uint16_t> &new_values,
                                   std::uint32_t count) {
  const std::uint32_t skip = ((count + 7u) >> 2) & 0xffffu;
  std::vector<std::uint8_t> output(skip + old_values.size() * 2 + 2, 0);
  std::size_t data = skip;
  std::size_t flag = 1;
  unsigned left = 4;
  unsigned flags = 0;
  for (std::size_t i = 0; i < old_values.size(); ++i) {
    const std::uint16_t difference =
        static_cast<std::uint16_t>(old_values[i] - new_values[i]);
    if (difference == 0) {
      flags <<= 2;
    } else if (static_cast<std::uint16_t>(difference + 0x80u) < 0x100u) {
      flags = (flags << 2) | 1u;
      output[data++] = static_cast<std::uint8_t>(difference);
    } else {
      flags = (flags << 2) | 3u;
      output[data++] = static_cast<std::uint8_t>(difference);
      output[data++] = static_cast<std::uint8_t>(difference >> 8);
    }
    if (--left == 0 && i + 1 != old_values.size()) {
      output[flag++] = static_cast<std::uint8_t>(flags);
      flags = 0;
      left = 4;
    }
  }
  flags <<= left * 2;
  output[flag] = static_cast<std::uint8_t>(flags);
  const std::uint8_t length = static_cast<std::uint8_t>(data + 1);
  output[0] = length;
  output[data] = length;
  output.resize(data + 1);
  return output;
}

void encodeCases() {
  const std::vector<std::uint16_t> differences = {0, 0xff7f, 0xff80, 0xffff,
                                                  1, 127,    128,    0xffff};
  Fixture f;
  std::vector<std::uint16_t> old_values, new_values;
  for (std::size_t i = 0; i < differences.size(); ++i) {
    const std::uint16_t newer = static_cast<std::uint16_t>(0x2100 + i * 17);
    const std::uint16_t older =
        static_cast<std::uint16_t>(newer + differences[i]);
    old_values.push_back(older);
    new_values.push_back(newer);
    f.put16(kOld + static_cast<std::uint32_t>(i * 2), older);
    f.put16(kNew + static_cast<std::uint32_t>(i * 2), newer);
  }
  CHECK(f.run(static_cast<std::uint32_t>(differences.size())) ==
        NBA97_TEXT_COMPLETE);
  const auto want = expected(old_values, new_values,
                             static_cast<std::uint32_t>(differences.size()));
  CHECK(std::equal(want.begin(), want.end(), f.data.begin() + f.at(kOut)));
  CHECK(f.progress.machine.registers.gpr[2].word == kOut + want.size());
  CHECK(f.get16(kToggle) == 3);
  CHECK(f.progress.completed && f.progress.stopped_pc == 0 &&
        f.progress.stopped_address == 0);
  CHECK(f.progress.element_iterations == differences.size());
  CHECK(f.progress.completed_flag_groups == 1);
}

void countsAndMasks() {
  for (std::uint32_t count : {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 130u}) {
    Fixture f;
    for (std::uint32_t i = 0; i < count; ++i) {
      f.put16(kOld + i * 2, static_cast<std::uint16_t>(i));
      f.put16(kNew + i * 2, static_cast<std::uint16_t>(i));
    }
    CHECK(f.run(count) == NBA97_TEXT_COMPLETE);
    CHECK(f.progress.element_iterations == count);
    const auto want = expected(std::vector<std::uint16_t>(count),
                               std::vector<std::uint16_t>(count), count);
    CHECK(std::equal(want.begin(), want.end(), f.data.begin() + f.at(kOut)));
  }

  for (std::uint32_t count : {1u, 2u, 3u, 5u, 6u, 7u, 9u}) {
    Fixture f;
    std::vector<std::uint16_t> old_values(count, 0x1001u);
    std::vector<std::uint16_t> new_values(count, 0x1000u);
    for (std::uint32_t i = 0; i < count; ++i) {
      f.put16(kOld + i * 2, old_values[i]);
      f.put16(kNew + i * 2, new_values[i]);
    }
    CHECK(f.run(count) == NBA97_TEXT_COMPLETE);
    const auto want = expected(old_values, new_values, count);
    CHECK(std::equal(want.begin(), want.end(), f.data.begin() + f.at(kOut)));
  }

  for (std::uint32_t count : {65535u, 65536u, 0x10001u}) {
    Fixture f;
    const std::uint32_t iterations = count == 0x10001u ? 1u : count;
    for (std::uint32_t i = 0; i < iterations; ++i) {
      f.put16(kOld + i * 2, 7);
      f.put16(kNew + i * 2, 7);
    }
    CHECK(f.run(count) == NBA97_TEXT_COMPLETE);
    CHECK(f.progress.element_iterations == iterations);
    if (count == 0x10001u)
      CHECK(f.progress.machine.registers.gpr[2].word == kOut + 0x4003u);
  }

  Fixture zero;
  zero.context.operation_budget = 9;
  CHECK(zero.run(0) == NBA97_TEXT_LIMIT);
  CHECK(zero.progress.element_iterations == 5);
  CHECK(zero.progress.stopped_pc == 0x80076820u);

  Fixture wrap;
  wrap.context.machine.registers.gpr[7] = {0xfffffffdu, 15};
  wrap.context.operation_budget = 0;
  CHECK(nba97_game_match_buffer_compress(&wrap.context, &wrap.progress) ==
        NBA97_TEXT_LIMIT);
  CHECK(wrap.progress.machine.registers.gpr[9].word == 0xfffffffdu);
  CHECK(wrap.progress.machine.registers.gpr[6].word == kOut + 1);
}

void machineKnownnessAndDeterminism() {
  Fixture first;
  Fixture second;
  const auto entry = first.context.machine;
  first.put16(kOld, 0x1234);
  first.put16(kNew, 0x1234);
  second.put16(kOld, 0x1234);
  second.put16(kNew, 0x1234);
  CHECK(first.run() == NBA97_TEXT_COMPLETE);
  CHECK(second.run() == NBA97_TEXT_COMPLETE);
  for (unsigned i = 0; i < 32; ++i) {
    CHECK(first.progress.machine.registers.gpr[i].word ==
          second.progress.machine.registers.gpr[i].word);
    CHECK(first.progress.machine.registers.gpr[i].known_mask ==
          second.progress.machine.registers.gpr[i].known_mask);
    if (i != 2 && i != 3 && i != 4 && i != 5 && i != 6 && i != 7 && i != 8 &&
        i != 9 && i != 10 && i != 11 && i != 12)
      CHECK(first.progress.machine.registers.gpr[i].word ==
                entry.registers.gpr[i].word &&
            first.progress.machine.registers.gpr[i].known_mask ==
                entry.registers.gpr[i].known_mask);
  }
  CHECK(first.progress.machine.hi.word == entry.hi.word &&
        first.progress.machine.hi.known_mask == entry.hi.known_mask);
  CHECK(first.progress.machine.lo.word == entry.lo.word &&
        first.progress.machine.lo.known_mask == entry.lo.known_mask);
  CHECK(first.data == second.data && first.known == second.known);
}

void budgetsAndJournal() {
  Fixture full;
  for (unsigned i = 0; i < 5; ++i) {
    full.put16(kOld + i * 2, static_cast<std::uint16_t>(100 + i));
    full.put16(kNew + i * 2, static_cast<std::uint16_t>(99 + i));
  }
  CHECK(full.run(5) == NBA97_TEXT_COMPLETE);
  CHECK(full.progress.access_events == 21);
  CHECK(full.journal[0].pc == 0x80076820u &&
        full.journal[0].kind == NBA97_GAME_MATCH_BUFFER_COMPRESS_READ);
  CHECK(full.journal[1].pc == 0x80076824u);
  Fixture negative;
  negative.put16(kOld, 0xffffu);
  negative.put16(kNew, 0);
  CHECK(negative.run() == NBA97_TEXT_COMPLETE);
  CHECK(negative.journal[2].pc == 0x8007685cu &&
        negative.journal[2].value == 0xffu && negative.journal[2].width == 1);
  for (std::size_t budget = 0; budget < full.progress.operations; ++budget) {
    Fixture cut;
    for (unsigned i = 0; i < 5; ++i) {
      cut.put16(kOld + i * 2, static_cast<std::uint16_t>(100 + i));
      cut.put16(kNew + i * 2, static_cast<std::uint16_t>(99 + i));
    }
    cut.context.operation_budget = budget;
    CHECK(cut.run(5) == NBA97_TEXT_LIMIT);
    CHECK(cut.progress.operations == budget);
    CHECK(cut.progress.stopped_pc == full.journal[budget].pc);
  }
}

void failurePrefixes() {
  Fixture malformed;
  malformed.put16(kOld, 0x1111);
  malformed.put16(kNew, 0x2222);
  malformed.known[malformed.at(kNew) + 1] = 2;
  const auto old_v0 = malformed.context.machine.registers.gpr[2];
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT);
  CHECK(malformed.progress.stopped_pc == 0x80076824u);
  CHECK(malformed.progress.machine.registers.gpr[3].word == 0x1111u);
  CHECK(malformed.progress.machine.registers.gpr[2].word != 0x2222u);
  CHECK(malformed.progress.machine.registers.gpr[2].word != old_v0.word);

  Fixture unknownDifference;
  unknownDifference.put16(kOld, 0);
  unknownDifference.put16(kNew, 0);
  unknownDifference.known[unknownDifference.at(kNew)] = 0;
  CHECK(unknownDifference.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknownDifference.progress.stopped_pc == 0x80076838u);
  CHECK(unknownDifference.progress.machine.registers.gpr[10].word == 0);
  CHECK(unknownDifference.progress.machine.registers.gpr[10].known_mask != 15);

  Fixture unknownCount;
  unknownCount.context.machine.registers.gpr[7] = {1, 14};
  unknownCount.put16(kOld, 5);
  unknownCount.put16(kNew, 5);
  CHECK(nba97_game_match_buffer_compress(&unknownCount.context,
                                         &unknownCount.progress) ==
        NBA97_TEXT_UNKNOWN);
  CHECK(unknownCount.progress.stopped_pc == 0x80076888u);
  CHECK(unknownCount.progress.machine.registers.gpr[2].word == 3);

  Fixture jr;
  jr.put16(kOld, 1);
  jr.put16(kNew, 1);
  jr.context.machine.registers.gpr[31] = {0x80001235u, 15};
  CHECK(nba97_game_match_buffer_compress(&jr.context, &jr.progress) ==
        NBA97_TEXT_ALIGNMENT_TRAP);
  CHECK(jr.progress.stopped_pc == 0x800768e8u);
  CHECK(jr.progress.machine.registers.gpr[2].word == kOut + 3);
  Fixture unknownJr;
  unknownJr.put16(kOld, 1);
  unknownJr.put16(kNew, 1);
  unknownJr.context.machine.registers.gpr[31] = {0x80001234u, 14};
  CHECK(nba97_game_match_buffer_compress(
            &unknownJr.context, &unknownJr.progress) == NBA97_TEXT_UNKNOWN);
  CHECK(unknownJr.progress.machine.registers.gpr[2].word == kOut + 3);
}

void addressAndStoreFailures() {
  Fixture alignment;
  alignment.context.machine.registers.gpr[4] = {kOld + 1, 15};
  CHECK(nba97_game_match_buffer_compress(&alignment.context,
                                         &alignment.progress) ==
        NBA97_TEXT_ALIGNMENT_TRAP);
  CHECK(alignment.progress.stopped_pc == 0x80076820u);

  Fixture mapped;
  mapped.context.machine.registers.gpr[5] = {0x90000000u, 15};
  CHECK(nba97_game_match_buffer_compress(&mapped.context, &mapped.progress) ==
        NBA97_TEXT_RESOURCE);
  CHECK(mapped.progress.stopped_pc == 0x80076824u);

  std::uint8_t old_data[2]{0, 2}, old_known[2]{0, 1};
  std::uint8_t new_data[2]{0, 0}, new_known[2]{1, 1};
  std::uint8_t output[8]{};
  std::uint8_t toggle[2]{0, 0}, toggle_known[2]{1, 1};
  Nba97GameTextRegion regions[] = {{kOld, old_data, old_known, 2},
                                   {kNew, new_data, new_known, 2},
                                   {kOut, output, nullptr, sizeof output},
                                   {kToggle, toggle, toggle_known, 2}};
  Fixture base;
  Nba97GameMatchBufferCompressContext context{};
  context.memory = {regions, std::size(regions)};
  context.operation_budget = 100;
  context.machine = base.context.machine;
  Nba97GameMatchBufferCompressProgress progress{};
  const int nullStoreResult =
      nba97_game_match_buffer_compress(&context, &progress);
  CHECK(nullStoreResult == NBA97_TEXT_ARGUMENT);
  CHECK(progress.stopped_pc == 0x8007686cu);
  CHECK(std::all_of(std::begin(output), std::end(output),
                    [](std::uint8_t value) { return value == 0; }));

  Fixture overlap;
  overlap.context.machine.registers.gpr[6] = {kOld, 15};
  overlap.put16(kOld, 0x0101);
  overlap.put16(kNew, 0x0100);
  CHECK(nba97_game_match_buffer_compress(&overlap.context, &overlap.progress) ==
        NBA97_TEXT_COMPLETE);
  CHECK(overlap.progress.stores >= 4);

  Fixture toggleAlias;
  toggleAlias.context.machine.registers.gpr[6] = {kToggle - 2, 15};
  toggleAlias.put16(kOld, 9);
  toggleAlias.put16(kNew, 9);
  toggleAlias.put16(kToggle, 0xffffu);
  CHECK(nba97_game_match_buffer_compress(&toggleAlias.context,
                                         &toggleAlias.progress) ==
        NBA97_TEXT_COMPLETE);
  CHECK(toggleAlias.get16(kToggle) == 0xff02u);

  Fixture a1Alignment;
  a1Alignment.context.machine.registers.gpr[5] = {kNew + 1, 15};
  CHECK(nba97_game_match_buffer_compress(&a1Alignment.context,
                                         &a1Alignment.progress) ==
        NBA97_TEXT_ALIGNMENT_TRAP);
  CHECK(a1Alignment.progress.stopped_pc == 0x80076824u);

  Fixture outputResource;
  outputResource.context.machine.registers.gpr[6] = {0x90000000u, 15};
  outputResource.put16(kOld, 1);
  outputResource.put16(kNew, 0);
  CHECK(nba97_game_match_buffer_compress(&outputResource.context,
                                         &outputResource.progress) ==
        NBA97_TEXT_RESOURCE);
  CHECK(outputResource.progress.stopped_pc == 0x8007685cu);

  Fixture wrap;
  wrap.context.machine.registers.gpr[4] = {0xfffffffeu, 15};
  CHECK(nba97_game_match_buffer_compress(&wrap.context, &wrap.progress) ==
        NBA97_TEXT_RESOURCE);
  CHECK(wrap.progress.stopped_address == 0xfffffffeu);
}

void invalidInputs() {
  Fixture f;
  CHECK(nba97_game_match_buffer_compress(nullptr, &f.progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_match_buffer_compress(&f.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
  f.context.machine.registers.gpr[0].known_mask = 14;
  CHECK(nba97_game_match_buffer_compress(&f.context, &f.progress) ==
        NBA97_TEXT_ARGUMENT);

  std::uint8_t byte = 0;
  Nba97GameTextRegion malformed{0, &byte, nullptr,
                                std::numeric_limits<std::size_t>::max()};
  Fixture valid;
  valid.context.memory = {&malformed, 1};
  CHECK(nba97_game_match_buffer_compress(&valid.context, &valid.progress) ==
        NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  encodeCases();
  countsAndMasks();
  machineKnownnessAndDeterminism();
  budgetsAndJournal();
  failurePrefixes();
  addressAndStoreFailures();
  invalidInputs();
  if (failures)
    return EXIT_FAILURE;
  std::cout << "game_match_buffer_compress_tests: PASS (" << checks
            << " checks)\n";
  return EXIT_SUCCESS;
}
