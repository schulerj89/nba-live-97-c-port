#include "recovered/game_period_audio_noop.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "period audio no-op check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

bool same(Nba97GamePeriodAudioNoopWord a, Nba97GamePeriodAudioNoopWord b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

struct Fixture {
  Nba97GamePeriodAudioNoopContext context{};
  Nba97GamePeriodAudioNoopProgress progress{};

  Fixture() {
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i) {
      context.machine.registers.gpr[i].word =
          0x21000000u + i * 0x01010101u;
      context.machine.registers.gpr[i].known_mask =
          static_cast<std::uint8_t>((i * 7u) & 15u);
    }
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        0x81234568u, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
  }

  int run() { return nba97_game_period_audio_noop(&context, &progress); }
};

void arbitrary_machine_is_exactly_preserved() {
  Fixture f;
  const auto entry = f.context.machine;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.operations == 0 && f.progress.accesses == 0 &&
        f.progress.reads == 0 && f.progress.stores == 0 &&
        f.progress.stopped_pc == 0 && f.progress.stopped_address == 0 &&
        same(f.progress.return_address,
             entry.registers.gpr[NBA97_MATCH_INITIALIZE_RA]));
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    check(same(f.progress.machine.registers.gpr[i],
               entry.registers.gpr[i]));
  check(same(f.progress.machine.hi, entry.hi) &&
        same(f.progress.machine.lo, entry.lo));
}

void ignored_values_never_fabricate_v0() {
  const std::array<std::uint32_t, 5> values{
      0, 1, 0x7fffffffu, 0x80000000u, 0xffffffffu};
  for (std::uint32_t a0 : values) {
    for (std::uint32_t v0 : values) {
      Fixture f;
      f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {a0, 3};
      f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {v0, 12};
      f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
          v0 ^ a0, 6};
      const auto entry = f.context.machine;
      check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
            same(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
                 entry.registers.gpr[NBA97_MATCH_INITIALIZE_A0]) &&
            same(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0],
                 entry.registers.gpr[NBA97_MATCH_INITIALIZE_V0]) &&
            same(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP],
                 entry.registers.gpr[NBA97_MATCH_INITIALIZE_SP]));
    }
  }
}

void return_target_outcomes_after_nop() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f;
    f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        0x81234568u, static_cast<std::uint8_t>(mask)};
    const auto entry = f.context.machine;
    const int result = f.run();
    check((mask == 15 && result == NBA97_TEXT_COMPLETE &&
           f.progress.completed) ||
          (mask != 15 && result == NBA97_TEXT_UNKNOWN &&
           !f.progress.completed && f.progress.stopped_pc == 0x8002a254u &&
           f.progress.stopped_address == 0x81234568u));
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
      check(same(f.progress.machine.registers.gpr[i],
                 entry.registers.gpr[i]));
  }

  for (unsigned low = 1; low <= 3; ++low) {
    Fixture f;
    f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        0x81234560u + low, 15};
    check(f.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
          !f.progress.completed && f.progress.stopped_pc == 0x8002a254u &&
          f.progress.stopped_address == 0x81234560u + low &&
          f.progress.operations == 0 && f.progress.accesses == 0);
  }

  Fixture zero;
  zero.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0, 15};
  check(zero.run() == NBA97_TEXT_COMPLETE && zero.progress.completed &&
        zero.progress.return_address.word == 0);
}

void invalid_metadata_and_null_arguments() {
  Nba97GamePeriodAudioNoopProgress progress{};
  Fixture f;
  check(nba97_game_period_audio_noop(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_period_audio_noop(&f.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);

  Fixture bad_zero_word;
  bad_zero_word.context.machine.registers.gpr[0].word = 1;
  check(bad_zero_word.run() == NBA97_TEXT_ARGUMENT &&
        !bad_zero_word.progress.completed);
  Fixture bad_zero_mask;
  bad_zero_mask.context.machine.registers.gpr[0].known_mask = 14;
  check(bad_zero_mask.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_gpr;
  bad_gpr.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T9]
      .known_mask = 16;
  check(bad_gpr.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_hi;
  bad_hi.context.machine.hi.known_mask = 16;
  check(bad_hi.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_lo;
  bad_lo.context.machine.lo.known_mask = 16;
  check(bad_lo.run() == NBA97_TEXT_ARGUMENT);
}

void deterministic_full_machine() {
  Fixture a;
  Fixture b;
  check(a.run() == NBA97_TEXT_COMPLETE && b.run() == NBA97_TEXT_COMPLETE);
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    check(same(a.progress.machine.registers.gpr[i],
               b.progress.machine.registers.gpr[i]));
  check(same(a.progress.machine.hi, b.progress.machine.hi) &&
        same(a.progress.machine.lo, b.progress.machine.lo) &&
        same(a.progress.return_address, b.progress.return_address) &&
        a.progress.operations == b.progress.operations &&
        a.progress.completed == b.progress.completed);
}
} // namespace

int main() {
  arbitrary_machine_is_exactly_preserved();
  ignored_values_never_fabricate_v0();
  return_target_outcomes_after_nop();
  invalid_metadata_and_null_arguments();
  deterministic_full_machine();
  std::printf("game period audio no-op: %u checks passed\n", checks);
}
