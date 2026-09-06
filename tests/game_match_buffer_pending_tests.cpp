#include "recovered/game_match_buffer_pending.h"

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
    std::fprintf(stderr, "match-buffer pending check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Base = 0x800fe860u;
constexpr std::uint32_t Pending = 0x800fe864u;
constexpr std::uint32_t Return = 0x81234568u;

bool same(Nba97GameMatchBufferPendingWord a,
          Nba97GameMatchBufferPendingWord b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(16, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(16, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameMatchBufferPendingAccess, 2> journal{};
  Nba97GameMatchBufferPendingContext context{};
  Nba97GameMatchBufferPendingProgress progress{};

  explicit Fixture(std::uint8_t prior = 0xa5) {
    bytes[Pending - Base] = prior;
    context.memory = {&region, 1};
    context.operation_budget = 1;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
      context.machine.registers.gpr[i].word =
          0x21000000u + i * 0x01010101u;
      context.machine.registers.gpr[i].known_mask =
          static_cast<std::uint8_t>((i * 7u) & 15u);
    }
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {
        0xdeadbeefu, 0};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT] = {
        0xaaaaaaaau, 3};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {Return, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
  }

  int run() {
    return nba97_game_match_buffer_pending(&context, &progress);
  }
};

void all_prior_bytes_and_full_machine() {
  for (unsigned prior = 0; prior < 256; ++prior) {
    Fixture f(static_cast<std::uint8_t>(prior));
    const auto entry = f.context.machine;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
    check(f.bytes[Pending - Base] == 1 && f.known[Pending - Base] == 1 &&
          f.progress.operations == 1 && f.progress.accesses == 1 &&
          f.progress.stores == 1 && f.progress.access_events == 1);
    check(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
              1 &&
          f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                  .known_mask == 15 &&
          f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT].word ==
              0x80100000u &&
          f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT]
                  .known_mask == 15 &&
          same(f.progress.returned_value,
               f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]) &&
          same(f.progress.return_address,
               entry.registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
      if (i == NBA97_MATCH_INITIALIZE_V0 || i == NBA97_MATCH_INITIALIZE_AT)
        continue;
      check(same(f.progress.machine.registers.gpr[i],
                 entry.registers.gpr[i]));
    }
    check(same(f.progress.machine.hi, entry.hi) &&
          same(f.progress.machine.lo, entry.lo));
  }

  Fixture journal;
  check(journal.run() == NBA97_TEXT_COMPLETE &&
        journal.journal[0].pc == 0x80076b30u &&
        journal.journal[0].address == Pending &&
        journal.journal[0].value == 1 && journal.journal[0].operation == 1 &&
        journal.journal[0].width == 1 &&
        journal.journal[0].known_mask == 1 &&
        journal.journal[0].kind == NBA97_GAME_MATCH_BUFFER_PENDING_STORE);
}

void operation_and_return_prefixes() {
  Fixture limited(0x6a);
  limited.context.operation_budget = 0;
  check(limited.run() == NBA97_TEXT_LIMIT && !limited.progress.completed &&
        limited.progress.stopped_pc == 0x80076b30u &&
        limited.progress.stopped_address == Pending &&
        limited.progress.operations == 0 && limited.progress.accesses == 0 &&
        limited.progress.stores == 0 && limited.bytes[Pending - Base] == 0x6a &&
        limited.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 1 &&
        limited.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT]
                .word == 0x80100000u);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f(0x37);
    f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask =
        static_cast<std::uint8_t>(mask);
    const int result = f.run();
    check((mask == 15 && result == NBA97_TEXT_COMPLETE &&
           f.progress.completed) ||
          (mask != 15 && result == NBA97_TEXT_UNKNOWN &&
           !f.progress.completed && f.progress.stopped_pc == 0x80076b34u));
    check(f.bytes[Pending - Base] == 1 && f.progress.stores == 1 &&
          f.progress.stopped_address == (mask == 15 ? 0 : Return) &&
          f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                  .known_mask == mask);
  }

  Fixture unaligned;
  unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
      0x81234567u, 15};
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x80076b34u &&
        unaligned.progress.stopped_address == 0x81234567u &&
        unaligned.progress.stores == 1 &&
        unaligned.bytes[Pending - Base] == 1);
}

void memory_and_argument_failures() {
  Fixture unmapped(0x44);
  unmapped.region.base = 0x800fe870u;
  check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.stopped_pc == 0x80076b30u &&
        unmapped.progress.operations == 1 && unmapped.progress.accesses == 1 &&
        unmapped.progress.stores == 0 && unmapped.bytes[Pending - Base] == 0x44);

  Fixture malformed(0x55);
  malformed.known[Pending - Base] = 2;
  check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x80076b30u &&
        malformed.progress.operations == 1 && malformed.progress.accesses == 1 &&
        malformed.progress.stores == 0 &&
        malformed.bytes[Pending - Base] == 0x55 &&
        malformed.known[Pending - Base] == 2);

  Fixture raw(0x66);
  raw.region.known = nullptr;
  check(raw.run() == NBA97_TEXT_COMPLETE && raw.progress.completed &&
        raw.bytes[Pending - Base] == 1);

  Nba97GameMatchBufferPendingProgress progress{};
  Fixture argument;
  check(nba97_game_match_buffer_pending(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_match_buffer_pending(&argument.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);

  argument.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T9]
      .known_mask = 16;
  check(argument.run() == NBA97_TEXT_ARGUMENT &&
        argument.bytes[Pending - Base] == 0xa5 &&
        argument.progress.operations == 0);

  Fixture bad_hi;
  bad_hi.context.machine.hi.known_mask = 16;
  check(bad_hi.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_lo;
  bad_lo.context.machine.lo.known_mask = 16;
  check(bad_lo.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_zero;
  bad_zero.context.machine.registers.gpr[0] = {1, 15};
  check(bad_zero.run() == NBA97_TEXT_ARGUMENT);
  Fixture null_regions;
  null_regions.context.memory.region = nullptr;
  check(null_regions.run() == NBA97_TEXT_ARGUMENT);
  Fixture null_data;
  null_data.region.data = nullptr;
  check(null_data.run() == NBA97_TEXT_ARGUMENT);
  Fixture zero_size;
  zero_size.region.size = 0;
  check(zero_size.run() == NBA97_TEXT_ARGUMENT);
  Fixture overflow;
  overflow.region.base = 0xfffffff0u;
  overflow.region.size = 32;
  check(overflow.run() == NBA97_TEXT_ARGUMENT);
  Fixture overlap;
  Nba97GameTextRegion regions[2]{overlap.region, overlap.region};
  overlap.context.memory = {regions, 2};
  check(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture missing_journal;
  missing_journal.context.access_journal = nullptr;
  check(missing_journal.run() == NBA97_TEXT_ARGUMENT);
}

void deterministic_machine_and_memory() {
  Fixture a(0x29);
  Fixture b(0x29);
  check(a.run() == NBA97_TEXT_COMPLETE && b.run() == NBA97_TEXT_COMPLETE);
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    check(same(a.progress.machine.registers.gpr[i],
               b.progress.machine.registers.gpr[i]));
  check(same(a.progress.machine.hi, b.progress.machine.hi) &&
        same(a.progress.machine.lo, b.progress.machine.lo) &&
        a.bytes == b.bytes && a.known == b.known);
}
} // namespace

int main() {
  all_prior_bytes_and_full_machine();
  operation_and_return_prefixes();
  memory_and_argument_failures();
  deterministic_machine_and_memory();
  std::printf("game match-buffer pending: %u checks passed\n", checks);
}
