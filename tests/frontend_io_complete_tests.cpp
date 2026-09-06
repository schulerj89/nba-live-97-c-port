#include "recovered/frontend_io_complete.h"

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
    throw std::runtime_error("frontend-io-complete failed at " +
                             std::to_string(line));
}
#define CHECK(value) checkAt((value), __LINE__)

constexpr U Base = 0x80000000u;
constexpr U Size = 0x100000u;
constexpr U Active = 0x800f84c4u;
constexpr U Status = 0x800ef840u;
constexpr U Ra = 0x800394f8u;
constexpr std::array<U, 22> Pcs{{
    0x800392a0u, 0x800392a4u, 0x800392a8u, 0x800392acu,
    0x800392b0u, 0x800392b4u, 0x800392b8u, 0x800392bcu,
    0x800392c0u, 0x800392c4u, 0x800392c8u, 0x800392ccu,
    0x800392d0u, 0x800392d4u, 0x800392d8u, 0x800392dcu,
    0x800392e0u, 0x800392e4u, 0x800392e8u, 0x800392ecu,
    0x800392f0u, 0x800392f4u}};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97FrontendIoCompleteContext context{};
  Nba97FrontendIoCompleteProgress progress{};
  std::array<Nba97FrontendIoCompleteAccess, 16> access{};
  std::array<U, 128> instructions{};

  Fixture(U active = 1) {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {
          0x41000000u + i * 0x101u, std::uint8_t((i % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[29] = {0x801f0000u, 7};
    context.machine.registers.gpr[31] = {Ra, 15};
    context.machine.hi = {0x12345678u, 6};
    context.machine.lo = {0x90abcdefu, 9};
    put(Active, active);
    for (unsigned i = 0; i < 8; ++i) put(Status + i * 0x24u, 0);
    context.memory = {&region, 1};
    context.operation_budget = 9;
    context.access_journal = access.data();
    context.access_journal_capacity = access.size();
    context.instruction_journal = instructions.data();
    context.instruction_journal_capacity = instructions.size();
  }

  bool extent(U address, U width = 4) const {
    return address >= Base && width <= Size && address - Base <= Size - width;
  }
  void put(U address, U value, std::uint8_t mask = 15) {
    if (!extent(address)) throw std::runtime_error("fixture write outside RAM");
    for (unsigned i = 0; i < 4; ++i) {
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
      if (region.known)
        known[address - Base + i] = std::uint8_t((mask >> i) & 1u);
    }
  }
  int run() { return nba97_frontend_io_complete(&context, &progress); }
};

void inactiveAndAllZero() {
  Fixture inactive(0);
  const auto before = inactive.context.machine;
  CHECK(inactive.run() == NBA97_TEXT_COMPLETE && inactive.progress.completed &&
        inactive.progress.operations == 1 && inactive.progress.accesses == 1 &&
        inactive.progress.reads == 1 && inactive.progress.status_reads == 0 &&
        inactive.progress.instruction_count == 9 &&
        inactive.progress.machine.registers.gpr[2].word == 1 &&
        inactive.progress.machine.registers.gpr[4].word == 0);
  CHECK(inactive.progress.machine.registers.gpr[1].word ==
            before.registers.gpr[1].word &&
        inactive.progress.machine.registers.gpr[3].word ==
            before.registers.gpr[3].word);
  for (unsigned reg = 5; reg < 32; ++reg)
    CHECK(inactive.progress.machine.registers.gpr[reg].word ==
              before.registers.gpr[reg].word &&
          inactive.progress.machine.registers.gpr[reg].known_mask ==
              before.registers.gpr[reg].known_mask);
  CHECK(inactive.progress.machine.hi.word == before.hi.word &&
        inactive.progress.machine.hi.known_mask == before.hi.known_mask &&
        inactive.progress.machine.lo.word == before.lo.word &&
        inactive.progress.machine.lo.known_mask == before.lo.known_mask);

  Fixture zero;
  CHECK(zero.run() == NBA97_TEXT_COMPLETE && zero.progress.completed &&
        zero.progress.operations == 9 && zero.progress.accesses == 9 &&
        zero.progress.reads == 9 && zero.progress.status_reads == 8 &&
        zero.progress.slots_examined == 8 &&
        zero.progress.instruction_count == 81 &&
        zero.progress.machine.registers.gpr[2].word == 1 &&
        zero.progress.machine.registers.gpr[3].word == 0x120u &&
        zero.progress.machine.registers.gpr[4].word == 8 &&
        zero.progress.machine.registers.gpr[1].word == 0x800f00fcu);
  CHECK(zero.access[0].address == Active);
  for (unsigned i = 0; i < 8; ++i)
    CHECK(zero.access[i + 1].address == Status + i * 0x24u &&
          zero.access[i + 1].operation == i + 2 &&
          zero.access[i + 1].kind == NBA97_FRONTEND_IO_COMPLETE_READ);
}

void everySlotAndEveryPc() {
  std::array<bool, 22> seen{};
  auto mark = [&](const Fixture &f) {
    for (std::size_t i = 0; i < f.progress.instruction_events; ++i)
      for (unsigned pc = 0; pc < Pcs.size(); ++pc)
        if (f.instructions[i] == Pcs[pc]) seen[pc] = true;
  };
  Fixture inactive(0);
  CHECK(inactive.run() == NBA97_TEXT_COMPLETE);
  mark(inactive);
  Fixture all_zero;
  CHECK(all_zero.run() == NBA97_TEXT_COMPLETE);
  mark(all_zero);
  for (unsigned slot = 0; slot < 8; ++slot) {
    Fixture f;
    f.put(Status + slot * 0x24u, slot == 7 ? 0x80000000u : 0x12340000u);
    CHECK(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
          f.progress.machine.registers.gpr[2].word == 0 &&
          f.progress.machine.registers.gpr[2].known_mask == 15 &&
          f.progress.operations == slot + 2 &&
          f.progress.status_reads == slot + 1 &&
          f.progress.slots_examined == slot + 1 &&
          f.progress.machine.registers.gpr[4].word == slot + 1 &&
          f.progress.machine.registers.gpr[3].word == slot * 0x24u &&
          f.progress.machine.registers.gpr[1].word ==
              0x800f0000u + slot * 0x24u &&
          f.progress.instruction_count == 16 + slot * 9);
    mark(f);
  }
  for (bool value : seen) CHECK(value);
}

void knownnessDecisions() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture active;
    active.put(Active, 1, std::uint8_t(mask));
    const int expected = (mask & 1u) ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN;
    CHECK(active.run() == expected);
    if (!(mask & 1u))
      CHECK(active.progress.stopped_pc == 0x800392acu &&
            active.progress.machine.registers.gpr[4].word == 0 &&
            active.progress.operations == 1);
  }
  Fixture partial_active;
  partial_active.put(Active, 0x80000000u, 8);
  CHECK(partial_active.run() == NBA97_TEXT_COMPLETE &&
        partial_active.progress.status_reads == 8);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture status;
    status.put(Status, 1, std::uint8_t(mask));
    const int expected = (mask & 1u) ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN;
    CHECK(status.run() == expected);
    if (mask & 1u)
      CHECK(status.progress.machine.registers.gpr[2].word == 0 &&
            status.progress.machine.registers.gpr[4].word == 1);
    else
      CHECK(status.progress.stopped_pc == 0x800392d8u &&
            status.progress.machine.registers.gpr[4].word == 1 &&
            status.progress.status_reads == 1);
  }
  Fixture partial_status;
  partial_status.put(Status, 0x80000000u, 8);
  CHECK(partial_status.run() == NBA97_TEXT_COMPLETE &&
        partial_status.progress.machine.registers.gpr[2].word == 0);
}

void budgetsMemoryAndReturnFaults() {
  for (unsigned budget = 0; budget < 9; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    CHECK(f.run() == NBA97_TEXT_LIMIT && f.progress.operations == budget &&
          f.progress.accesses == budget && !f.progress.completed);
    if (budget == 0)
      CHECK(f.progress.stopped_pc == 0x800392a4u &&
            f.progress.stopped_address == Active &&
            f.progress.machine.registers.gpr[2].word == 0x80100000u);
    else
      CHECK(f.progress.stopped_pc == 0x800392d0u &&
            f.progress.stopped_address == Status + (budget - 1u) * 0x24u);
  }

  Fixture malformed_active;
  malformed_active.known[Active - Base + 3] = 2;
  CHECK(malformed_active.run() == NBA97_TEXT_ARGUMENT &&
        malformed_active.progress.operations == 1 &&
        malformed_active.progress.reads == 0 &&
        malformed_active.progress.machine.registers.gpr[2].word ==
            0x80100000u);
  Fixture malformed_status;
  malformed_status.known[Status - Base + 3] = 2;
  CHECK(malformed_status.run() == NBA97_TEXT_ARGUMENT &&
        malformed_status.progress.operations == 2 &&
        malformed_status.progress.reads == 1 &&
        malformed_status.progress.status_reads == 0);

  Fixture no_plane;
  no_plane.region.known = nullptr;
  CHECK(no_plane.run() == NBA97_TEXT_COMPLETE &&
        no_plane.progress.active_word.known_mask == 15 &&
        no_plane.progress.last_status.known_mask == 15);

  Fixture short_region;
  short_region.region.base = Active;
  short_region.region.size = 3;
  CHECK(short_region.run() == NBA97_TEXT_RESOURCE &&
        short_region.progress.stopped_address == Active);
  Fixture wrapping;
  wrapping.region.base = UINT32_MAX;
  wrapping.region.size = 4;
  CHECK(wrapping.run() == NBA97_TEXT_ARGUMENT);
  Fixture overlap;
  std::array<std::uint8_t, 4> extra{};
  Nba97GameTextRegion regions[2]{{Base, overlap.bytes.data(),
                                  overlap.known.data(), overlap.bytes.size()},
                                 {Active, extra.data(), nullptr, extra.size()}};
  overlap.context.memory = {regions, 2};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f(0);
    f.context.machine.registers.gpr[31].known_mask = std::uint8_t(mask);
    const int expected = mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN;
    CHECK(f.run() == expected && f.progress.reads == 1 &&
          f.progress.machine.registers.gpr[2].word == 1);
    if (mask != 15)
      CHECK(f.progress.stopped_pc == 0x800392f0u &&
            f.progress.stopped_target == Ra);
  }
  Fixture misaligned(0);
  misaligned.context.machine.registers.gpr[31] = {Ra + 1u, 15};
  CHECK(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.reads == 1 &&
        misaligned.progress.machine.registers.gpr[2].word == 1 &&
        misaligned.progress.stopped_pc == 0x800392f0u);
}

void repeatedAndOptionalJournals() {
  Fixture f;
  CHECK(f.run() == NBA97_TEXT_COMPLETE &&
        f.progress.machine.registers.gpr[2].word == 1);
  f.put(Status + 4u * 0x24u, 0xfeedfaceu);
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.progress.status_reads == 5 &&
        f.progress.machine.registers.gpr[2].word == 0);

  Fixture no_journal(0);
  no_journal.context.access_journal = nullptr;
  no_journal.context.access_journal_capacity = 0;
  no_journal.context.instruction_journal = nullptr;
  no_journal.context.instruction_journal_capacity = 0;
  CHECK(no_journal.run() == NBA97_TEXT_COMPLETE &&
        no_journal.progress.access_events == 1 &&
        no_journal.progress.instruction_events == 9);
  Fixture bad_machine;
  bad_machine.context.machine.registers.gpr[12].known_mask = 16;
  CHECK(bad_machine.run() == NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  try {
    inactiveAndAllZero();
    everySlotAndEveryPc();
    knownnessDecisions();
    budgetsMemoryAndReturnFaults();
    repeatedAndOptionalJournals();
    std::printf("frontend_io_complete_tests: PASS (%u checks)\n", checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
