#include "recovered/frontend_memory_copy.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "frontend memory-copy check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr std::uint32_t Base = 0x80100000u;

struct Fixture {
  std::vector<std::uint8_t> bytes;
  std::vector<std::uint8_t> known;
  Nba97GameTextRegion region{};
  std::vector<Nba97FrontendMemoryCopyAccess> accesses;
  std::vector<std::uint32_t> instructions;
  Nba97FrontendMemoryCopyContext context{};
  Nba97FrontendMemoryCopyProgress progress{};

  Fixture(std::uint32_t source = Base + 0x100u,
          std::uint32_t destination = Base + 0x500u,
          std::uint32_t length = 64u, std::size_t budget = 100000)
      : bytes(0x1000), known(0x1000, 1), accesses(4096),
        instructions(16384),
        region{Base, bytes.data(), known.data(), bytes.size()} {
    for (std::size_t i = 0; i < bytes.size(); ++i)
      bytes[i] = static_cast<std::uint8_t>((i * 37u + (i >> 4u) + 0x5au) & 255u);
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x33000000u + i * 0x10101u,
                                          std::uint8_t((i % 4u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_MEMORY_COPY_A0] = {source, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_MEMORY_COPY_A1] = {destination,
                                                                    15};
    context.machine.registers.gpr[NBA97_FRONTEND_MEMORY_COPY_A2] = {length, 15};
    context.machine.registers.gpr[NBA97_FRONTEND_MEMORY_COPY_RA] = {0x80028b5cu,
                                                                    15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    context.memory = {&region, 1};
    context.operation_budget = budget;
    context.access_journal = accesses.data();
    context.access_journal_capacity = accesses.size();
    context.instruction_journal = instructions.data();
    context.instruction_journal_capacity = instructions.size();
  }

  int run() { return nba97_frontend_memory_copy(&context, &progress); }
  std::size_t offset(std::uint32_t address) const { return address - Base; }
};

void checkCopy(std::uint32_t source, std::uint32_t destination,
               std::uint32_t length) {
  Fixture f(source, destination, length);
  const auto original = f.bytes;
  auto expected = original;
  std::memmove(expected.data() + f.offset(destination),
               expected.data() + f.offset(source), length);
  const auto input_machine = f.context.machine;
  const bool backward =
      static_cast<std::int32_t>(source) < static_cast<std::int32_t>(destination) &&
      static_cast<std::int32_t>(destination) <
          static_cast<std::int32_t>(source + length);
  const auto expected_v0 = backward
                               ? ((source + length) | (destination + length)) & 3u
                               : (source | destination) & 3u;
  CHECK(f.run() == NBA97_TEXT_COMPLETE);
  CHECK(f.progress.completed && !f.progress.trapped && f.bytes == expected);
  CHECK(f.progress.source == source && f.progress.destination == destination &&
        f.progress.requested_length == length);
  CHECK(f.progress.return_v0 == expected_v0 &&
        f.progress.return_v0_known_mask == 15 &&
        f.progress.machine.registers.gpr[2].word == f.progress.return_v0);
  CHECK((f.progress.backward != 0) == backward);
  for (unsigned reg : {0u, 3u, 16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u,
                       24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u})
    CHECK(f.progress.machine.registers.gpr[reg].word ==
              input_machine.registers.gpr[reg].word &&
          f.progress.machine.registers.gpr[reg].known_mask ==
              input_machine.registers.gpr[reg].known_mask);
  CHECK(f.progress.machine.hi.word == input_machine.hi.word &&
        f.progress.machine.hi.known_mask == input_machine.hi.known_mask &&
        f.progress.machine.lo.word == input_machine.lo.word &&
        f.progress.machine.lo.known_mask == input_machine.lo.known_mask);
  CHECK(f.progress.accesses == f.progress.reads + f.progress.stores &&
        f.progress.operations == f.progress.accesses &&
        f.progress.access_events == f.progress.accesses);
  for (std::size_t i = 0; i < f.progress.access_events; ++i) {
    CHECK(f.accesses[i].operation == i + 1 && f.accesses[i].width >= 1 &&
          f.accesses[i].width <= 4 && f.accesses[i].transfer_mask != 0 &&
          f.accesses[i].known_mask <= 15);
  }
}

void exhaustiveLengthsAlignmentsAndAliases() {
  for (std::uint32_t length = 0; length <= 130; ++length)
    for (std::uint32_t source_align = 0; source_align < 4; ++source_align)
      for (std::uint32_t destination_align = 0; destination_align < 4;
           ++destination_align) {
        const auto source = Base + 0x100u + source_align;
        checkCopy(source, Base + 0x500u + destination_align, length);
        checkCopy(source, source, length);
        checkCopy(source, source + 5u + destination_align, length);
        checkCopy(Base + 0x500u + source_align,
                  Base + 0x100u + destination_align, length);
      }
}

void instructionCoverageAndPartialTraffic() {
  std::set<std::uint32_t> covered;
  std::set<std::uint32_t> exits;
  std::set<std::uint8_t> masks;
  for (const auto test : std::array<std::array<std::uint32_t, 3>, 5>{{
           {Base + 0x100u, Base + 0x500u, 95u},
           {Base + 0x101u, Base + 0x502u, 31u},
           {Base + 0x500u, Base + 0x100u, 4u},
           {Base + 0x101u, Base + 0x105u, 31u},
           {Base + 0x101u, Base + 0x106u, 31u},
       }}) {
    Fixture f(test[0], test[1], test[2]);
    CHECK(f.run() == NBA97_TEXT_COMPLETE);
    covered.insert(f.instructions.begin(),
                   f.instructions.begin() + f.progress.instruction_events);
    for (std::size_t i = 0; i < f.progress.access_events; ++i)
      if (f.accesses[i].pc == 0x80090af4u ||
          f.accesses[i].pc == 0x80090af8u ||
          f.accesses[i].pc == 0x80090b14u ||
          f.accesses[i].pc == 0x80090b18u ||
          (f.accesses[i].pc >= 0x80090c20u &&
           f.accesses[i].pc <= 0x80090c8cu))
        masks.insert(f.accesses[i].transfer_mask);
    for (const auto pc : {0x80090ae0u, 0x80090b94u, 0x80090cc0u})
      if (covered.count(pc))
        exits.insert(pc);
  }
  for (std::uint32_t source_align = 0; source_align < 4; ++source_align)
    for (std::uint32_t destination_align = 0; destination_align < 4;
         ++destination_align) {
      Fixture f(Base + 0x100u + source_align,
                Base + 0x500u + destination_align, 16u);
      CHECK(f.run() == NBA97_TEXT_COMPLETE);
      for (std::size_t i = 0; i < f.progress.access_events; ++i)
        masks.insert(f.accesses[i].transfer_mask);
    }
  for (std::uint32_t pc = 0x800909a8u; pc <= 0x80090cc4u; pc += 4u) {
    if (covered.count(pc) != 1)
      std::fprintf(stderr, "missing source PC %08x\n", pc);
    CHECK(covered.count(pc) == 1);
  }
  CHECK(covered.size() == 200 && exits.size() == 3);
  CHECK(masks.count(1) && masks.count(3) && masks.count(7) && masks.count(15) &&
        masks.count(8) && masks.count(12) && masks.count(14));
}

void budgetsFailuresKnownnessAndRepeatability() {
  Fixture baseline(Base + 0x101u, Base + 0x106u, 31u);
  CHECK(baseline.run() == NBA97_TEXT_COMPLETE);
  for (std::size_t budget = 0; budget < baseline.progress.operations; ++budget) {
    Fixture limited(Base + 0x101u, Base + 0x106u, 31u, budget);
    CHECK(limited.run() == NBA97_TEXT_LIMIT && !limited.progress.completed &&
          limited.progress.operations == budget &&
          limited.progress.accesses == budget);
    CHECK(limited.progress.stopped_pc >= 0x80090c20u &&
          limited.progress.stopped_pc <= 0x80090cb0u);
  }

  Fixture runaway(Base + 0x500u, Base + 0x100u, 0x80000000u, 3);
  const int runaway_result = runaway.run();
  if (runaway_result != NBA97_TEXT_LIMIT || runaway.progress.operations != 3 ||
      runaway.progress.stopped_pc != 0x800909d8u ||
      runaway.progress.working_count != 0x7fffffc0u)
    std::fprintf(stderr, "runaway result=%d ops=%zu pc=%08x count=%08x\n",
                 runaway_result, runaway.progress.operations,
                 runaway.progress.stopped_pc,
                 runaway.progress.working_count);
  CHECK(runaway_result == NBA97_TEXT_LIMIT && runaway.progress.operations == 3 &&
        runaway.progress.stopped_pc == 0x800909d8u &&
        runaway.progress.working_count == 0x7fffffc0u);

  Fixture refusal(Base + 0x100u, Base + 0x500u, 4u);
  std::array<std::uint8_t, 16> source{};
  std::array<std::uint8_t, 16> source_known{};
  std::array<std::uint8_t, 16> destination{};
  std::array<Nba97GameTextRegion, 2> split{{
      {Base + 0x100u, source.data(), source_known.data(), source.size()},
      {Base + 0x500u, destination.data(), nullptr, destination.size()},
  }};
  source_known.fill(1);
  source_known[2] = 0;
  refusal.context.memory = {split.data(), split.size()};
  CHECK(refusal.run() == NBA97_TEXT_UNKNOWN &&
        refusal.progress.stopped_pc == 0x80090aacu &&
        refusal.progress.reads == 1 && refusal.progress.stores == 0);

  Fixture propagation(Base + 0x101u, Base + 0x502u, 7u);
  propagation.known[propagation.offset(Base + 0x103u)] = 0;
  CHECK(propagation.run() == NBA97_TEXT_COMPLETE &&
        propagation.known[propagation.offset(Base + 0x504u)] == 0);

  Fixture first(Base + 0x101u, Base + 0x106u, 31u);
  Fixture second(Base + 0x101u, Base + 0x106u, 31u);
  CHECK(first.run() == second.run() && first.bytes == second.bytes &&
        first.known == second.known &&
        std::memcmp(&first.progress, &second.progress,
                    sizeof first.progress) == 0 &&
        std::equal(first.instructions.begin(),
                   first.instructions.begin() + first.progress.instruction_events,
                   second.instructions.begin()) &&
        std::memcmp(first.accesses.data(), second.accesses.data(),
                    first.progress.access_events * sizeof first.accesses[0]) == 0);
}

void trapsBoundariesAndValidation() {
  Fixture add_trap(0x7ffffff0u, 0x7ffffff8u, 0x20u);
  add_trap.context.memory = {nullptr, 0};
  CHECK(add_trap.run() == NBA97_FRONTEND_MEMORY_COPY_ARITHMETIC_TRAP &&
        add_trap.progress.trapped && add_trap.progress.stopped_pc == 0x80090b9cu &&
        add_trap.progress.machine.registers.gpr[2].word == 0x7ffffff8u);

  Fixture destination_trap(0x7fffff6cu, 0x7fffffc6u, 100u);
  destination_trap.context.memory = {nullptr, 0};
  CHECK(destination_trap.run() == NBA97_FRONTEND_MEMORY_COPY_ARITHMETIC_TRAP &&
        destination_trap.progress.stopped_pc == 0x80090bb0u &&
        destination_trap.progress.machine.registers.gpr[4].word == 0x7fffffd0u);

  std::array<std::uint8_t, 8> low{};
  std::array<std::uint8_t, 8> high{};
  std::array<Nba97GameTextRegion, 2> signed_regions{{
      {0x7ffffff0u, low.data(), nullptr, low.size()},
      {0x80000010u, high.data(), nullptr, high.size()},
  }};
  for (unsigned i = 0; i < 8; ++i)
    low[i] = std::uint8_t(i + 1);
  Fixture signed_boundary(0x7ffffff0u, 0x80000010u, 4u);
  signed_boundary.context.memory = {signed_regions.data(), signed_regions.size()};
  CHECK(signed_boundary.run() == NBA97_TEXT_COMPLETE &&
        !signed_boundary.progress.backward &&
        std::equal(low.begin(), low.begin() + 4, high.begin()));

  for (unsigned kind = 0; kind < 2; ++kind) {
    Fixture late(Base + 0x100u, Base + 0x500u, 0u);
    late.context.machine.registers.gpr[31] =
        kind ? Nba97FrontendMemoryCopyWord{0x80028b5du, 15}
             : Nba97FrontendMemoryCopyWord{0x80028b5cu, 14};
    CHECK(late.run() == (kind ? NBA97_TEXT_ALIGNMENT_TRAP : NBA97_TEXT_UNKNOWN) &&
          late.progress.stopped_pc == 0x80090ae0u &&
          late.progress.instruction_events >= 2 &&
          late.instructions[late.progress.instruction_events - 2] == 0x80090ae0u &&
          late.instructions[late.progress.instruction_events - 1] == 0x80090ae4u);
  }

  Fixture missing(Base + 0x100u, Base + 0x500u, 4u);
  missing.context.memory = {nullptr, 0};
  CHECK(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc == 0x80090aa4u);

  Fixture unaligned_lw(Base + 0x102u, Base + 0x502u, 4u);
  unaligned_lw.context.machine.registers.gpr[4].word = Base + 0x102u;
  unaligned_lw.context.machine.registers.gpr[5].word = Base + 0x500u;
  CHECK(unaligned_lw.run() == NBA97_TEXT_COMPLETE);

  Fixture malformed;
  malformed.region.known[0] = 2;
  malformed.context.machine.registers.gpr[4].word = Base;
  malformed.context.machine.registers.gpr[5].word = Base + 0x500u;
  malformed.context.machine.registers.gpr[6].word = 1;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x80090ac8u);
  malformed.region.known[0] = 1;
  malformed.context.access_journal = nullptr;
  malformed.context.access_journal_capacity = 1;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT && malformed.progress.operations == 0);

  Fixture overlapping_regions;
  std::array<Nba97GameTextRegion, 2> overlap{{
      overlapping_regions.region,
      {Base + 8u, overlapping_regions.bytes.data() + 8,
       overlapping_regions.known.data() + 8, 8},
  }};
  overlapping_regions.context.memory = {overlap.data(), overlap.size()};
  CHECK(overlapping_regions.run() == NBA97_TEXT_ARGUMENT);
  CHECK(nba97_frontend_memory_copy(nullptr, &overlapping_regions.progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_frontend_memory_copy(&overlapping_regions.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}

void partialKnownDecisionsAndMergePrefixes() {
  Fixture branch_unknown(Base + 0x100u, Base + 0x500u, 0u);
  branch_unknown.context.machine.registers.gpr[4].known_mask = 0;
  CHECK(branch_unknown.run() == NBA97_TEXT_UNKNOWN &&
        branch_unknown.progress.stopped_pc == 0x800909acu &&
        branch_unknown.progress.instruction_events == 3 &&
        branch_unknown.instructions[2] == 0x800909b0u);

  Fixture add_unknown(Base + 0x100u, Base + 0x500u, 8u);
  add_unknown.context.machine.registers.gpr[4].known_mask = 14;
  CHECK(add_unknown.run() == NBA97_TEXT_UNKNOWN &&
        add_unknown.progress.stopped_pc == 0x80090b9cu &&
        add_unknown.progress.machine.registers.gpr[2].word ==
            ((Base + 0x100u) | (Base + 0x500u)));

  Fixture alignment_unknown(Base + 0x500u, Base + 0x100u, 0u);
  alignment_unknown.context.machine.registers.gpr[4].known_mask = 14;
  CHECK(alignment_unknown.run() == NBA97_TEXT_UNKNOWN &&
        alignment_unknown.progress.stopped_pc == 0x800909b8u);

  Fixture failed_lwr(Base + 0x101u, Base + 0x502u, 16u);
  std::array<std::uint8_t, 1> lwl_byte{{0xaau}};
  std::array<std::uint8_t, 1> lwl_known{{1}};
  Nba97GameTextRegion lwl_region{Base + 0x104u, lwl_byte.data(),
                                 lwl_known.data(), lwl_byte.size()};
  failed_lwr.context.memory = {&lwl_region, 1};
  failed_lwr.context.machine.registers.gpr[8] = {0x11223344u, 3};
  CHECK(failed_lwr.run() == NBA97_TEXT_RESOURCE &&
        failed_lwr.progress.stopped_pc == 0x80090af8u &&
        failed_lwr.progress.reads == 1 && failed_lwr.progress.accesses == 2 &&
        failed_lwr.progress.access_events == 1 &&
        failed_lwr.accesses[0].transfer_mask == 8 &&
        failed_lwr.progress.machine.registers.gpr[8].word == 0xaa223344u &&
        failed_lwr.progress.machine.registers.gpr[8].known_mask == 11);

  Fixture failed_swr(Base + 0x101u, Base + 0x502u, 16u);
  std::array<std::uint8_t, 20> source{};
  std::array<std::uint8_t, 20> source_known{};
  std::array<std::uint8_t, 2> destination{{0xeeu, 0xeeu}};
  std::array<std::uint8_t, 2> destination_known{{1, 1}};
  for (unsigned i = 0; i < source.size(); ++i) {
    source[i] = std::uint8_t(0x40u + i);
    source_known[i] = 1;
  }
  std::array<Nba97GameTextRegion, 2> regions{{
      {Base + 0x100u, source.data(), source_known.data(), source.size()},
      {Base + 0x504u, destination.data(), destination_known.data(),
       destination.size()},
  }};
  failed_swr.context.memory = {regions.data(), regions.size()};
  const int failed_swr_result = failed_swr.run();
  if (destination[0] != source[3] || destination[1] != source[4])
    std::fprintf(stderr, "failed SWR result=%d pc=%08x reads=%zu stores=%zu accesses=%zu bytes=%02x,%02x expected=%02x,%02x\n",
                 failed_swr_result, failed_swr.progress.stopped_pc,
                 failed_swr.progress.reads, failed_swr.progress.stores,
                 failed_swr.progress.accesses, destination[0], destination[1],
                 source[3], source[4]);
  CHECK(failed_swr_result == NBA97_TEXT_RESOURCE &&
        failed_swr.progress.stopped_pc == 0x80090b18u &&
        failed_swr.progress.reads == 8 && failed_swr.progress.stores == 1 &&
        failed_swr.progress.accesses == 10 &&
        failed_swr.accesses[8].pc == 0x80090b14u &&
        failed_swr.accesses[8].transfer_mask == 12 &&
        destination[0] == source[3] && destination[1] == source[4]);
}
}  // namespace

int main() {
  exhaustiveLengthsAlignmentsAndAliases();
  instructionCoverageAndPartialTraffic();
  budgetsFailuresKnownnessAndRepeatability();
  trapsBoundariesAndValidation();
  partialKnownDecisionsAndMergePrefixes();
  std::printf("frontend_memory_copy_tests: PASS (%u checks)\n", checks);
  return 0;
}
