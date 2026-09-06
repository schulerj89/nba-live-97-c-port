#include "recovered/game_text_chain_clear.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("text-chain check failed at " +
                             std::to_string(line));
}
#define check(value) checkAt((value), __LINE__)

bool wordEq(const Nba97GameTextChainClearWord &a,
            const Nba97GameTextChainClearWord &b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

struct Fixture {
  static constexpr U Base = 0x80000000u;
  static constexpr U Size = 0x200000u;
  static constexpr U Font = 0x80110000u;
  static constexpr U Table = 0x80120000u;
  static constexpr U Links = 0x80130000u;
  static constexpr U Ra = 0x81234568u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97GameTextChainClearMachine entry{};
  Nba97GameTextChainClearProgress progress{};
  std::array<Nba97GameTextChainClearAccess, 128> journal{};
  std::size_t budget = 128;

  explicit Fixture(U index = 0xc9u) {
    for (unsigned i = 0; i < 32; ++i)
      entry.registers.gpr[i] = {0x11110000u + i * 0x101u,
                                std::uint8_t((i * 7u) & 15u)};
    entry.registers.gpr[0] = {0, 15};
    entry.registers.gpr[4] = {index, 15};
    entry.registers.gpr[31] = {Ra, 15};
    entry.hi = {0x12345678u, 5};
    entry.lo = {0x89abcdefu, 10};
    put(0x800b2048u, Font, 4);
    put(Font + 0x10u, Links, 4);
    put(Font + 0x14u, Table, 4);
    setHead(std::uint16_t(index), 3);
    setLink(3, 5);
    setLink(5, 0xffffu);
    put(Links + 3u * 64u + 0x12u, 0x7777u, 2);
    put(Links + 5u * 64u + 0x12u, 0x8888u, 2);
  }

  std::size_t at(U address, unsigned width = 1) const {
    if (address < Base || std::uint64_t(address) + width > Base + Size)
      throw std::out_of_range("unmapped");
    return address - Base;
  }
  void put(U address, U value, unsigned width, std::uint8_t mask = 15) {
    const auto offset = at(address, width);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = std::uint8_t(value >> (8u * i));
      known[offset + i] = std::uint8_t((mask >> i) & 1u);
    }
  }
  U get(U address, unsigned width) const {
    const auto offset = at(address, width);
    U value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= U(bytes[offset + i]) << (8u * i);
    return value;
  }
  void setHead(std::uint16_t index, std::uint16_t head,
               std::uint8_t mask = 15) {
    put(Table + U(index) * 2u, head, 2, mask);
  }
  void setLink(std::uint16_t link, std::uint16_t next, std::uint8_t mask = 15) {
    put(Links + U(link) * 64u + 0x18u, next, 2, mask);
  }
  int run() {
    Nba97GameTextChainClearContext context{};
    context.memory = {&region, 1};
    context.operation_budget = budget;
    context.machine = entry;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    return nba97_game_text_chain_clear(&context, &progress);
  }
};

void normalAndSignedEdges() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.operations == 11 && f.progress.chain_iterations == 2 &&
        f.progress.instruction_count == 39);
  check(f.get(Fixture::Links + 3u * 64u + 0x12u, 2) == 0 &&
        f.get(Fixture::Links + 5u * 64u + 0x12u, 2) == 0 &&
        f.get(Fixture::Table + 0xc9u * 2u, 2) == 0xffffu);
  const std::array<U, 10> pcs{
      0x80030674u, 0x80030688u, 0x80030694u, 0x800306a8u, 0x800306b4u,
      0x800306c4u, 0x800306a8u, 0x800306b4u, 0x800306c4u, 0x800306c8u};
  for (std::size_t i = 0; i < pcs.size(); ++i)
    check(f.journal[i].pc == pcs[i]);
  check(f.journal[5].kind == NBA97_GAME_TEXT_CHAIN_CLEAR_STORE &&
        f.journal[5].value == 0 && f.journal[8].value == 0 &&
        f.journal[9].kind == NBA97_GAME_TEXT_CHAIN_CLEAR_READ &&
        f.journal[10].pc == 0x800306dcu && f.journal[10].value == 0xffffu);
  check(f.progress.machine.hi.word == f.entry.hi.word &&
        f.progress.machine.hi.known_mask == f.entry.hi.known_mask &&
        f.progress.machine.lo.word == f.entry.lo.word &&
        f.progress.machine.lo.known_mask == f.entry.lo.known_mask);
  for (unsigned i = 0; i < 32; ++i)
    if (i < 2 || i > 6)
      check(wordEq(f.progress.machine.registers.gpr[i],
                   f.entry.registers.gpr[i]));

  for (U value : {0x8000u, 0xffffu, 0xabcd8000u, 0x1234ffffu}) {
    Fixture negative(value);
    check(negative.run() == NBA97_TEXT_COMPLETE &&
          negative.progress.operations == 1 && negative.progress.stores == 0 &&
          negative.progress.machine.registers.gpr[6].word == value);
  }
  Fixture zero(0);
  zero.setHead(0, 0xffffu);
  check(zero.run() == NBA97_TEXT_COMPLETE && zero.progress.operations == 5 &&
        zero.get(Fixture::Table, 2) == 0xffffu);
  Fixture maximum(0x7fffu);
  maximum.setHead(0x7fffu, 0x8000u);
  check(maximum.run() == NBA97_TEXT_COMPLETE &&
        maximum.progress.operations == 5);
  Fixture highIgnored(0xdead00c9u);
  check(highIgnored.run() == NBA97_TEXT_COMPLETE &&
        highIgnored.get(Fixture::Table + 0xc9u * 2u, 2) == 0xffffu);
}

void predicatesCyclesBudgetsAndAliases() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture negative(0xffffu);
    negative.entry.registers.gpr[4].known_mask = std::uint8_t(mask);
    const int negativeResult = negative.run();
    check(negativeResult ==
          ((mask & 2u) ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    Fixture positive;
    positive.entry.registers.gpr[4].known_mask = std::uint8_t(mask);
    const int positiveResult = positive.run();
    check(positiveResult == ((mask & 2u) == 0 ? NBA97_TEXT_UNKNOWN
                             : (mask & 1u)    ? NBA97_TEXT_COMPLETE
                                              : NBA97_TEXT_UNKNOWN));
    if ((mask & 2u) && !(mask & 1u))
      check(positive.progress.stopped_pc == 0x80030694u);
  }

  Fixture unknownIndex;
  unknownIndex.entry.registers.gpr[4].known_mask = 13;
  check(unknownIndex.run() == NBA97_TEXT_UNKNOWN &&
        unknownIndex.progress.stopped_pc == 0x80030680u &&
        unknownIndex.progress.operations == 1);

  Fixture unknownHead;
  unknownHead.setHead(0xc9u, 3, 1);
  check(unknownHead.run() == NBA97_TEXT_UNKNOWN &&
        unknownHead.progress.stopped_pc == 0x800306a0u &&
        unknownHead.progress.stores == 0);

  Fixture unknownLink;
  unknownLink.setLink(3, 5, 1);
  check(unknownLink.run() == NBA97_TEXT_UNKNOWN &&
        unknownLink.progress.stopped_pc == 0x800306c0u &&
        unknownLink.get(Fixture::Links + 3u * 64u + 0x12u, 2) == 0 &&
        unknownLink.known[unknownLink.at(Fixture::Links + 3u * 64u + 0x12u)] ==
            1);

  Fixture knownNegativeLink;
  knownNegativeLink.setLink(3, 0xffffu, 2);
  check(knownNegativeLink.run() == NBA97_TEXT_COMPLETE &&
        knownNegativeLink.get(Fixture::Links + 3u * 64u + 0x12u, 2) == 0 &&
        knownNegativeLink.progress.chain_iterations == 1);
  Fixture unknownNextAddress;
  unknownNextAddress.setLink(3, 5, 2);
  check(unknownNextAddress.run() == NBA97_TEXT_UNKNOWN &&
        unknownNextAddress.progress.stopped_pc == 0x800306b4u &&
        unknownNextAddress.get(Fixture::Links + 3u * 64u + 0x12u, 2) == 0);

  Fixture cycle;
  cycle.setLink(3, 3);
  cycle.budget = 17;
  check(cycle.run() == NBA97_TEXT_LIMIT && cycle.progress.operations == 17 &&
        cycle.progress.chain_iterations == 5 &&
        cycle.get(Fixture::Links + 3u * 64u + 0x12u, 2) == 0);

  Fixture complete;
  check(complete.run() == NBA97_TEXT_COMPLETE);
  for (std::size_t budget = 0; budget < complete.progress.operations;
       ++budget) {
    Fixture limited;
    limited.budget = budget;
    check(limited.run() == NBA97_TEXT_LIMIT &&
          limited.progress.operations == budget && !limited.progress.completed);
  }

  Fixture alias;
  alias.put(Fixture::Font + 0x10u, Fixture::Font + 2u, 4);
  alias.put(Fixture::Font + 0x14u, Fixture::Table + 0x100u, 4);
  alias.put(Fixture::Table + 0x100u + 0xc9u * 2u, 0, 2);
  alias.put(Fixture::Font + 0x1au, 0xffffu, 2);
  check(alias.run() == NBA97_TEXT_COMPLETE &&
        alias.get(Fixture::Font + 0x14u, 4) == Fixture::Table &&
        alias.get(Fixture::Table + 0xc9u * 2u, 2) == 0xffffu);
}

void mappedWrap() {
  std::array<std::uint8_t, 4> global{{0xf0, 0xff, 0xff, 0xff}};
  std::array<std::uint8_t, 4> globalKnown{{1, 1, 1, 1}};
  std::array<std::uint8_t, 16> top{};
  std::array<std::uint8_t, 16> topKnown{};
  std::array<std::uint8_t, 0x2000> low{};
  std::array<std::uint8_t, 0x2000> lowKnown{};
  topKnown.fill(1);
  lowKnown.fill(1);
  low[4] = 0x00;
  low[5] = 0x10;
  low[0x1000] = 0xff;
  low[0x1001] = 0xff;
  Nba97GameTextRegion regions[3]{
      {0x800b2048u, global.data(), globalKnown.data(), global.size()},
      {0xfffffff0u, top.data(), topKnown.data(), top.size()},
      {0, low.data(), lowKnown.data(), low.size()}};
  Nba97GameTextChainClearContext context{};
  context.memory = {regions, 3};
  context.operation_budget = 8;
  for (auto &reg : context.machine.registers.gpr)
    reg = {0, 15};
  context.machine.registers.gpr[31] = {Fixture::Ra, 15};
  Nba97GameTextChainClearProgress progress{};
  check(nba97_game_text_chain_clear(&context, &progress) ==
            NBA97_TEXT_COMPLETE &&
        progress.completed && low[0x1000] == 0xff && low[0x1001] == 0xff &&
        progress.operations == 5);
}

void trapsKnownnessValidationAndRepeatability() {
  Fixture malformed;
  malformed.known[malformed.at(Fixture::Links + 3u * 64u + 0x19u)] = 2;
  check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x800306b4u &&
        malformed.progress.machine.registers.gpr[2].word == 3u * 64u);

  Fixture unalignedFont;
  unalignedFont.put(0x800b2048u, Fixture::Font + 1u, 4);
  check(unalignedFont.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unalignedFont.progress.stopped_pc == 0x80030688u);
  Fixture unalignedTable;
  unalignedTable.put(Fixture::Font + 0x14u, Fixture::Table + 1u, 4);
  check(unalignedTable.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unalignedTable.progress.stopped_pc == 0x80030694u);
  Fixture unmapped;
  unmapped.region.size = 0x1000;
  check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.stopped_pc == 0x80030674u);
  Fixture nullKnown;
  nullKnown.region.known = nullptr;
  check(nullKnown.run() == NBA97_TEXT_COMPLETE &&
        nullKnown.get(Fixture::Links + 3u * 64u + 0x12u, 2) == 0 &&
        nullKnown.get(Fixture::Table + 0xc9u * 2u, 2) == 0xffffu);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture ra(0xffffu);
    ra.entry.registers.gpr[31].known_mask = std::uint8_t(mask);
    const int result = ra.run();
    check(result == (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    if (mask != 15)
      check(ra.progress.stopped_pc == 0x800306e0u &&
            ra.progress.instruction_count == 9);
  }
  Fixture badRa(0xffffu);
  badRa.entry.registers.gpr[31] = {Fixture::Ra | 1u, 15};
  check(badRa.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        badRa.progress.instruction_count == 9);

  Nba97GameTextChainClearProgress progress{};
  check(nba97_game_text_chain_clear(nullptr, &progress) == NBA97_TEXT_ARGUMENT);
  Fixture invalid;
  invalid.entry.registers.gpr[0].word = 1;
  check(invalid.run() == NBA97_TEXT_ARGUMENT);
  Nba97GameTextChainClearContext context{};
  Fixture backing;
  context.memory = {&backing.region, 1};
  context.machine = backing.entry;
  context.access_journal_capacity = 1;
  check(nba97_game_text_chain_clear(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);
  Nba97GameTextRegion overlap[2]{
      {Fixture::Base, backing.bytes.data(), backing.known.data(),
       backing.bytes.size()},
      {Fixture::Base + 1u, backing.bytes.data(), backing.known.data(), 1}};
  context = {};
  context.memory = {overlap, 2};
  context.machine = backing.entry;
  check(nba97_game_text_chain_clear(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);
  Nba97GameTextRegion oversized{0, backing.bytes.data(), backing.known.data(),
                                SIZE_MAX};
  context = {};
  context.memory = {&oversized, 1};
  context.machine = backing.entry;
  check(nba97_game_text_chain_clear(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);

  Fixture a;
  Fixture b;
  check(a.run() == NBA97_TEXT_COMPLETE && b.run() == NBA97_TEXT_COMPLETE &&
        a.bytes == b.bytes && a.known == b.known);
  for (unsigned i = 0; i < 32; ++i)
    check(wordEq(a.progress.machine.registers.gpr[i],
                 b.progress.machine.registers.gpr[i]));
}
} // namespace

int main() {
  try {
    normalAndSignedEdges();
    predicatesCyclesBudgetsAndAliases();
    mappedWrap();
    trapsKnownnessValidationAndRepeatability();
    std::printf("game_text_chain_clear_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
