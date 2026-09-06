#include "recovered/frontend_clock_read.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
using U = std::uint32_t;
unsigned checks;
void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("frontend-clock-read failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr U Address = 0x800d9ab8u;
constexpr U Ra = 0x8002efecu;
constexpr std::array<U, 4> Pcs{{0x8008da5cu, 0x8008da60u,
                                0x8008da64u, 0x8008da68u}};

struct Fixture {
  std::array<std::uint8_t, 4> bytes{};
  std::array<std::uint8_t, 4> known{{1, 1, 1, 1}};
  Nba97GameTextRegion region{Address, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendClockReadContext context{};
  Nba97FrontendClockReadProgress progress{};
  std::array<Nba97FrontendClockReadAccess, 2> access{};
  std::array<U, 8> instructions{};

  Fixture(U value = 0x89abcdefu, std::uint8_t mask = 15) {
    for (unsigned i = 0; i < 4; ++i) {
      bytes[i] = std::uint8_t(value >> (i * 8u));
      known[i] = std::uint8_t((mask >> i) & 1u);
    }
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {
          0x51000000u + i * 0x101u, std::uint8_t(i % 16u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[31] = {Ra, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x90abcdefu, 10};
    context.memory = {&region, 1};
    context.operation_budget = 1;
    context.access_journal = access.data();
    context.access_journal_capacity = access.size();
    context.instruction_journal = instructions.data();
    context.instruction_journal_capacity = instructions.size();
  }

  int run() { return nba97_frontend_clock_read(&context, &progress); }
};

void exactExecutionAndKnownness() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f(0x89abcdefu, std::uint8_t(mask));
    const auto before = f.context.machine;
    CHECK(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
          f.progress.operations == 1 && f.progress.accesses == 1 &&
          f.progress.reads == 1 && f.progress.access_events == 1 &&
          f.progress.instruction_count == 4 &&
          f.progress.instruction_events == 4 &&
          f.progress.loaded_clock.word == 0x89abcdefu &&
          f.progress.loaded_clock.known_mask == mask &&
          f.progress.machine.registers.gpr[2].word == 0x89abcdefu &&
          f.progress.machine.registers.gpr[2].known_mask == mask);
    for (unsigned i = 0; i < Pcs.size(); ++i)
      CHECK(f.instructions[i] == Pcs[i]);
    CHECK(f.access[0].pc == 0x8008da60u &&
          f.access[0].address == Address &&
          f.access[0].value == 0x89abcdefu &&
          f.access[0].operation == 1 && f.access[0].width == 4 &&
          f.access[0].known_mask == mask &&
          f.access[0].kind == NBA97_FRONTEND_CLOCK_READ_READ);
    for (unsigned reg = 0; reg < 32; ++reg)
      if (reg != 2)
        CHECK(f.progress.machine.registers.gpr[reg].word ==
                  before.registers.gpr[reg].word &&
              f.progress.machine.registers.gpr[reg].known_mask ==
                  before.registers.gpr[reg].known_mask);
    CHECK(f.progress.machine.hi.word == before.hi.word &&
          f.progress.machine.hi.known_mask == before.hi.known_mask &&
          f.progress.machine.lo.word == before.lo.word &&
          f.progress.machine.lo.known_mask == before.lo.known_mask);
  }

  Fixture no_plane(0x10203040u, 0);
  no_plane.region.known = nullptr;
  CHECK(no_plane.run() == NBA97_TEXT_COMPLETE &&
        no_plane.progress.loaded_clock.word == 0x10203040u &&
        no_plane.progress.loaded_clock.known_mask == 15);
}

void orderingFailuresAndPrefixes() {
  Fixture limit;
  limit.context.operation_budget = 0;
  CHECK(limit.run() == NBA97_TEXT_LIMIT && !limit.progress.completed &&
        limit.progress.operations == 0 && limit.progress.accesses == 0 &&
        limit.progress.instruction_count == 2 &&
        limit.instructions[0] == 0x8008da5cu &&
        limit.instructions[1] == 0x8008da60u &&
        limit.progress.stopped_pc == 0x8008da60u &&
        limit.progress.stopped_address == Address &&
        limit.progress.machine.registers.gpr[2].word == 0x800e0000u &&
        limit.progress.machine.registers.gpr[2].known_mask == 15);

  Fixture malformed;
  malformed.known[2] = 2;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.operations == 1 &&
        malformed.progress.accesses == 1 && malformed.progress.reads == 0 &&
        malformed.progress.instruction_count == 2 &&
        malformed.progress.machine.registers.gpr[2].word == 0x800e0000u);

  Fixture unmapped;
  unmapped.region.size = 3;
  CHECK(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.operations == 1 && unmapped.progress.accesses == 1 &&
        unmapped.progress.reads == 0 &&
        unmapped.progress.stopped_address == Address);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f(0xa1b2c3d4u, 15);
    f.context.machine.registers.gpr[31].known_mask = std::uint8_t(mask);
    const int expected = mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN;
    CHECK(f.run() == expected && f.progress.operations == 1 &&
          f.progress.reads == 1 && f.progress.instruction_count == 4 &&
          f.progress.machine.registers.gpr[2].word == 0xa1b2c3d4u &&
          f.progress.machine.registers.gpr[2].known_mask == 15);
    if (mask != 15)
      CHECK(f.progress.stopped_pc == 0x8008da64u &&
            f.progress.stopped_target == Ra);
  }
  Fixture misaligned(0x76543210u);
  misaligned.context.machine.registers.gpr[31] = {Ra + 1u, 15};
  CHECK(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.reads == 1 &&
        misaligned.progress.machine.registers.gpr[2].word == 0x76543210u &&
        misaligned.progress.stopped_pc == 0x8008da64u &&
        misaligned.progress.stopped_target == Ra + 1u);
}

void liveReadsAndArguments() {
  Fixture f(1);
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.progress.loaded_clock.word == 1);
  f.bytes = {{2, 0, 0, 0}};
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.progress.loaded_clock.word == 2);

  Fixture no_journal;
  no_journal.context.access_journal = nullptr;
  no_journal.context.access_journal_capacity = 0;
  no_journal.context.instruction_journal = nullptr;
  no_journal.context.instruction_journal_capacity = 0;
  CHECK(no_journal.run() == NBA97_TEXT_COMPLETE &&
        no_journal.progress.access_events == 1 &&
        no_journal.progress.instruction_events == 4);

  Fixture overlap;
  std::array<std::uint8_t, 1> extra{{0}};
  Nba97GameTextRegion regions[2]{{Address, overlap.bytes.data(),
                                  overlap.known.data(), 4},
                                 {Address + 3u, extra.data(), nullptr, 1}};
  overlap.context.memory = {regions, 2};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);

  Fixture invalid;
  invalid.context.machine.registers.gpr[7].known_mask = 16;
  CHECK(invalid.run() == NBA97_TEXT_ARGUMENT);
  Fixture wrapping;
  wrapping.region.base = UINT32_MAX;
  wrapping.region.size = 4;
  CHECK(wrapping.run() == NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  try {
    exactExecutionAndKnownness();
    orderingFailuresAndPrefixes();
    liveReadsAndArguments();
    std::printf("frontend_clock_read_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
