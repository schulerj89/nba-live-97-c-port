#include "game_period_audio_noop_adapter.h"

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
    std::fprintf(stderr, "period audio composition check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Presentation = 0x800eb680u;
constexpr std::uint32_t Stack = 0x800ff800u;
constexpr std::uint32_t ForwardedV0 = 0xcafebabeu;

bool same(Nba97GameFirstPeriodStartupWord a,
          Nba97GameFirstPeriodStartupWord b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

struct Fixture {
  std::vector<std::uint8_t> bytes =
      std::vector<std::uint8_t>(0x120000u, 0);
  std::vector<std::uint8_t> known =
      std::vector<std::uint8_t>(0x120000u, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97GameFirstPeriodStartupContext context{};
  Nba97GameFirstPeriodStartupProgress progress{};
  Nba97GamePeriodAudioNoopBinding binding{};
  std::vector<Nba97GameFirstPeriodStartupEvent> parent_calls;
  std::vector<Nba97GameFirstPeriodStartupEvent> fallback_calls;
  bool forward_v0_from_presentation = false;
  bool audio_memory_unchanged = true;

  explicit Fixture(std::uint8_t presentation = 0) {
    context.memory = {&region, 1};
    context.operation_budget = 32;
    context.io = bridge;
    context.user = this;
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
      context.registers.gpr[i] = {0x33000000u + i, 15};
    context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 15};
    context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 15};
    context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80067498u, 15};
    binding.fallback = fallback;
    binding.fallback_user = this;
    put(Presentation, presentation, 1);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    const auto offset = static_cast<std::size_t>(address - Ram);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
      known[offset + i] = 1;
    }
  }

  static int bridge(void *user, const Nba97GameTextMemory *memory,
                    const Nba97GameFirstPeriodStartupEvent *event,
                    Nba97GameFirstPeriodStartupRegisters *registers) {
    auto &f = *static_cast<Fixture *>(user);
    f.parent_calls.push_back(*event);
    if (event->kind == NBA97_GAME_FIRST_PERIOD_STARTUP_2A254) {
      const auto before_bytes = f.bytes;
      const auto before_known = f.known;
      const int result =
          nba97_game_period_audio_noop_from_first_period_startup(
              &f.binding, memory, event, registers);
      f.audio_memory_unchanged =
          f.audio_memory_unchanged && f.bytes == before_bytes &&
          f.known == before_known;
      return result;
    }
    return nba97_game_period_audio_noop_from_first_period_startup(
        &f.binding, memory, event, registers);
  }

  static int fallback(void *user, const Nba97GameTextMemory *,
                      const Nba97GameFirstPeriodStartupEvent *event,
                      Nba97GameFirstPeriodStartupRegisters *registers) {
    auto &f = *static_cast<Fixture *>(user);
    f.fallback_calls.push_back(*event);
    if (f.forward_v0_from_presentation && event->entry == 0x8002ddccu)
      registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {ForwardedV0, 6};
    return 1;
  }

  int run() { return nba97_game_first_period_startup(&context, &progress); }
};

void actual_parent_both_presentation_paths() {
  Fixture skipped(0);
  check(skipped.run() == NBA97_TEXT_COMPLETE && skipped.progress.completed &&
        !skipped.progress.optional_presentation_executed &&
        skipped.parent_calls.size() == 5 && skipped.fallback_calls.size() == 4 &&
        skipped.binding.invocations == 1 && skipped.binding.completions == 1 &&
        skipped.audio_memory_unchanged);
  check(skipped.binding.event.pc == 0x80067434u &&
        skipped.binding.event.delay_slot_pc == 0x80067438u &&
        skipped.binding.event.entry == 0x8002a254u &&
        skipped.binding.event.operation == 5 &&
        skipped.binding.event.kind == NBA97_GAME_FIRST_PERIOD_STARTUP_2A254 &&
        skipped.binding.event.argument_count == 1 &&
        skipped.binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            1 &&
        skipped.binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                .known_mask == 15 &&
        skipped.binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8006743cu &&
        skipped.binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            0 &&
        skipped.binding.progress.machine.hi.known_mask == 0 &&
        skipped.binding.progress.machine.lo.known_mask == 0);
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    check(same(skipped.binding.entry_registers.gpr[i],
               skipped.binding.progress.machine.registers.gpr[i]));

  Fixture presented(1);
  presented.forward_v0_from_presentation = true;
  check(presented.run() == NBA97_TEXT_COMPLETE && presented.progress.completed &&
        presented.progress.optional_presentation_executed &&
        presented.parent_calls.size() == 7 &&
        presented.fallback_calls.size() == 6 &&
        presented.binding.invocations == 1 &&
        presented.binding.completions == 1 && presented.audio_memory_unchanged);
  const std::array<std::uint32_t, 7> entries{
      0x800295d0u, 0x8002a244u, 0x8002dd84u, 0x8002ddccu,
      0x8002a254u, 0x80065db0u, 0x8007ef4cu};
  for (unsigned i = 0; i < entries.size(); ++i)
    check(presented.parent_calls[i].entry == entries[i]);
  check(presented.binding.event.operation == 8 &&
        presented.binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            ForwardedV0 &&
        presented.binding.entry_registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 6 &&
        presented.binding.progress.machine
                .registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == ForwardedV0 &&
        presented.binding.progress.machine
                .registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 6);
  for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    check(same(presented.binding.entry_registers.gpr[i],
               presented.binding.progress.machine.registers.gpr[i]));
}

void reusable_binding() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE);
  check(f.run() == NBA97_TEXT_COMPLETE && f.binding.invocations == 2 &&
        f.binding.completions == 2 && f.parent_calls.size() == 10 &&
        f.fallback_calls.size() == 8 && f.audio_memory_unchanged);
}

void malformed_assigned_calls_never_fallback() {
  enum Field {
    WrongKind,
    WrongEntry,
    WrongPc,
    WrongDelay,
    WrongReturn,
    WrongArguments,
    WrongA0,
    KindOnly,
    EntryOnly,
    PcOnly,
    DelayOnly,
    ReturnOnly
  };
  const std::array<Field, 12> fields{
      WrongKind, WrongEntry, WrongPc, WrongDelay, WrongReturn, WrongArguments,
      WrongA0, KindOnly, EntryOnly, PcOnly, DelayOnly, ReturnOnly};
  for (Field field : fields) {
    Fixture f;
    Nba97GameFirstPeriodStartupEvent event{};
    event.pc = 0x80067434u;
    event.delay_slot_pc = 0x80067438u;
    event.entry = 0x8002a254u;
    event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_2A254;
    event.argument_count = 1;
    auto registers = f.context.registers;
    registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x8006743cu, 15};
    registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {1, 15};
    switch (field) {
    case WrongKind:
      event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_65DB0;
      break;
    case WrongEntry:
      event.entry = 0x80060008u;
      break;
    case WrongPc:
      event.pc = 0x80060000u;
      break;
    case WrongDelay:
      event.delay_slot_pc = 0x80060004u;
      break;
    case WrongReturn:
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = 0x8006000cu;
      break;
    case WrongArguments:
      event.argument_count = 0;
      break;
    case WrongA0:
      registers.gpr[NBA97_MATCH_INITIALIZE_A0].word = 0;
      break;
    case KindOnly:
      event.pc = 0x80060000u;
      event.delay_slot_pc = 0x80060004u;
      event.entry = 0x80060008u;
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = 0x8006000cu;
      break;
    case EntryOnly:
      event.pc = 0x80060000u;
      event.delay_slot_pc = 0x80060004u;
      event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_65DB0;
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = 0x8006000cu;
      break;
    case PcOnly:
      event.delay_slot_pc = 0x80060004u;
      event.entry = 0x80060008u;
      event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_65DB0;
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = 0x8006000cu;
      break;
    case DelayOnly:
      event.pc = 0x80060000u;
      event.entry = 0x80060008u;
      event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_65DB0;
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = 0x8006000cu;
      break;
    case ReturnOnly:
      event.pc = 0x80060000u;
      event.delay_slot_pc = 0x80060004u;
      event.entry = 0x80060008u;
      event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_65DB0;
      break;
    }
    const auto before = registers;
    check(!nba97_game_period_audio_noop_from_first_period_startup(
              &f.binding, &f.context.memory, &event, &registers) &&
          f.binding.result == NBA97_TEXT_ARGUMENT &&
          f.binding.invocations == 0 && f.fallback_calls.empty());
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
      check(same(registers.gpr[i], before.gpr[i]));
  }

  Fixture unrelated;
  Nba97GameFirstPeriodStartupEvent event{};
  event.pc = 0x80067448u;
  event.delay_slot_pc = 0x8006744cu;
  event.entry = 0x80065db0u;
  event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_65DB0;
  auto registers = unrelated.context.registers;
  registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80067450u, 15};
  check(nba97_game_period_audio_noop_from_first_period_startup(
            &unrelated.binding, &unrelated.context.memory, &event,
            &registers) == 1 &&
        unrelated.fallback_calls.size() == 1 &&
        unrelated.binding.invocations == 0);
}
} // namespace

int main() {
  actual_parent_both_presentation_paths();
  reusable_binding();
  malformed_assigned_calls_never_fallback();
  std::printf("game period audio no-op composition: %u checks passed\n",
              checks);
}
