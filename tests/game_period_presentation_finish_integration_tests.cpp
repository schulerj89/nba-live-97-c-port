#include "game_period_presentation_finish_adapter.h"

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
    std::fprintf(stderr,
                 "period-presentation composition check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Presentation = 0x800eb680u;
constexpr std::uint32_t Gate = 0x800fdb78u;
constexpr std::uint32_t Active = 0x80109afcu;
constexpr std::uint32_t Published = 0x80109ae4u;
constexpr std::uint32_t Stack = 0x800ff800u;

bool same(Nba97GameFirstPeriodStartupWord a,
          Nba97GameFirstPeriodStartupWord b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

struct FinishCall {
  Nba97GamePeriodPresentationFinishEvent event{};
  Nba97GamePeriodPresentationFinishMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes =
      std::vector<std::uint8_t>(0x120000u, 0);
  std::vector<std::uint8_t> known =
      std::vector<std::uint8_t>(0x120000u, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GamePeriodPresentationFinishAccess, 16> journal{};
  Nba97GameFirstPeriodStartupContext context{};
  Nba97GameFirstPeriodStartupProgress progress{};
  Nba97GamePeriodPresentationFinishBinding binding{};
  std::vector<Nba97GameFirstPeriodStartupEvent> parent_calls;
  std::vector<Nba97GameFirstPeriodStartupEvent> fallback_calls;
  std::vector<FinishCall> finish_calls;
  unsigned refuse_finish = 0;
  unsigned invalid_hi = 0;
  unsigned invalid_lo = 0;
  unsigned invalid_gpr = 0;
  bool mutate_valid_gpr = false;

  explicit Fixture(std::uint8_t presentation = 1,
                   std::uint8_t gate = 0) {
    context.memory = {&region, 1};
    context.operation_budget = 32;
    context.io = bridge;
    context.user = this;
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
      context.registers.gpr[i] = {0x33000000u + i, 15};
    context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 15};
    context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 15};
    context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80067498u, 15};
    binding.operation_budget = 16;
    binding.io = finishChild;
    binding.user = this;
    binding.fallback = fallback;
    binding.fallback_user = this;
    binding.access_journal = journal.data();
    binding.access_journal_capacity = journal.size();
    put(Presentation, presentation, 1);
    put(Gate, gate, 1);
    put(0x8001ede8u, 0x81234560u, 4);
    put(Active, 0x55667788u, 4);
    put(Published, 0x11223344u, 4);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    const auto offset = static_cast<std::size_t>(address - Ram);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
      known[offset + i] = 1;
    }
  }

  std::uint32_t get(std::uint32_t address, unsigned width) const {
    const auto offset = static_cast<std::size_t>(address - Ram);
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[offset + i]) << (i * 8u);
    return value;
  }

  static int bridge(void *user, const Nba97GameTextMemory *memory,
                    const Nba97GameFirstPeriodStartupEvent *event,
                    Nba97GameFirstPeriodStartupRegisters *registers) {
    auto &f = *static_cast<Fixture *>(user);
    f.parent_calls.push_back(*event);
    return nba97_game_period_presentation_finish_from_first_period_startup(
        &f.binding, memory, event, registers);
  }

  static int fallback(void *user, const Nba97GameTextMemory *,
                      const Nba97GameFirstPeriodStartupEvent *event,
                      Nba97GameFirstPeriodStartupRegisters *) {
    auto &f = *static_cast<Fixture *>(user);
    f.fallback_calls.push_back(*event);
    return 1;
  }

  static int finishChild(
      void *user, const Nba97GameTextMemory *,
      const Nba97GamePeriodPresentationFinishEvent *event,
      Nba97GamePeriodPresentationFinishMachine *machine) {
    auto &f = *static_cast<Fixture *>(user);
    f.finish_calls.push_back({*event, *machine});
    const unsigned call = static_cast<unsigned>(f.finish_calls.size());
    if (f.mutate_valid_gpr)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 3] = {
          0x13572468u, 9};
    if (f.invalid_hi == call)
      machine->hi.known_mask = 16;
    if (f.invalid_lo == call)
      machine->lo.known_mask = 16;
    if (f.invalid_gpr == call)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_T9].known_mask = 16;
    return f.refuse_finish == call ? 0 : 1;
  }

  int run() { return nba97_game_first_period_startup(&context, &progress); }
};

void optional_zero_skips_and_nonzero_composes() {
  Fixture skipped(0);
  check(skipped.run() == NBA97_TEXT_COMPLETE && skipped.progress.completed &&
        !skipped.progress.optional_presentation_executed &&
        skipped.binding.invocations == 0 && skipped.binding.completions == 0 &&
        skipped.finish_calls.empty() && skipped.parent_calls.size() == 5 &&
        skipped.fallback_calls.size() == 5 &&
        skipped.get(Presentation, 1) == 0);

  Fixture f(0xff, 0);
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.optional_presentation_executed &&
        f.progress.presentation_flag.word == 0xff &&
        f.progress.operations == 12);
  const std::array<std::uint32_t, 7> entries{
      0x800295d0u, 0x8002a244u, 0x8002dd84u, 0x8002ddccu,
      0x8002a254u, 0x80065db0u, 0x8007ef4cu};
  check(f.parent_calls.size() == entries.size());
  for (unsigned i = 0; i < entries.size(); ++i)
    check(f.parent_calls[i].entry == entries[i]);
  check(f.binding.invocations == 1 && f.binding.completions == 1 &&
        f.binding.result == NBA97_TEXT_COMPLETE &&
        f.binding.event.pc == 0x80067424u &&
        f.binding.event.delay_slot_pc == 0x80067428u &&
        f.binding.event.entry == 0x8002ddccu &&
        f.binding.event.operation == 6 &&
        f.binding.event.kind == NBA97_GAME_FIRST_PERIOD_STARTUP_2DDCC &&
        f.binding.event.argument_count == 0 && f.fallback_calls.size() == 6);
  check(f.finish_calls.size() == 2 &&
        f.finish_calls[0].event.pc == 0x8002ddf8u &&
        f.finish_calls[0].event.delay_slot_pc == 0x8002ddfcu &&
        f.finish_calls[0].event.entry == 0x80044550u &&
        f.finish_calls[1].event.pc == 0x8002de14u &&
        f.finish_calls[1].event.delay_slot_pc == 0x8002de18u &&
        f.finish_calls[1].event.entry == 0x80046c2cu &&
        f.finish_calls[0].machine.hi.known_mask == 0 &&
        f.finish_calls[0].machine.lo.known_mask == 0);
  check(f.get(Presentation, 1) == 0 && f.get(Active, 4) == 0 &&
        f.get(Published, 4) == 0x81234560u &&
        f.binding.progress.completed &&
        f.binding.progress.machine.hi.known_mask == 0 &&
        f.binding.progress.machine.lo.known_mask == 0);
}

void typed_child_failures_and_valid_gpr_prefix() {
  for (unsigned call = 1; call <= 2; ++call) {
    Fixture f;
    f.refuse_finish = call;
    check(f.run() == NBA97_TEXT_IO_REFUSED && !f.progress.completed &&
          f.progress.stopped_pc == 0x80067424u &&
          f.progress.stopped_entry == 0x8002ddccu &&
          f.binding.result == NBA97_TEXT_IO_REFUSED &&
          f.binding.progress.stopped_pc ==
              (call == 1 ? 0x8002ddf8u : 0x8002de14u) &&
          f.binding.progress.callbacks_completed == call - 1 &&
          f.get(Presentation, 1) == 0 && f.get(Active, 4) == 1);
  }

  for (unsigned special = 0; special < 2; ++special) {
    Fixture f;
    f.mutate_valid_gpr = true;
    if (special == 0)
      f.invalid_hi = 1;
    else
      f.invalid_lo = 1;
    check(f.run() == NBA97_TEXT_IO_REFUSED &&
          f.binding.result == NBA97_TEXT_ARGUMENT &&
          f.binding.progress.stopped_pc == 0x8002ddf8u &&
          f.binding.progress.machine
                  .registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 3]
                  .word == 0x13572468u &&
          f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 3].word ==
              0x13572468u &&
          f.progress.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 3].known_mask ==
              9);
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
      check(f.progress.registers.gpr[i].known_mask <= 15);
  }

  Fixture bad_gpr;
  bad_gpr.invalid_gpr = 1;
  const auto original_t9 =
      bad_gpr.context.registers.gpr[NBA97_MATCH_INITIALIZE_T9];
  check(bad_gpr.run() == NBA97_TEXT_IO_REFUSED &&
        bad_gpr.binding.result == NBA97_TEXT_ARGUMENT &&
        same(bad_gpr.progress.registers.gpr[NBA97_MATCH_INITIALIZE_T9],
             original_t9));
}

void reusable_binding_and_actual_clear_effect() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.get(Presentation, 1) == 0);
  f.put(Presentation, 1, 1);
  check(f.run() == NBA97_TEXT_COMPLETE && f.binding.invocations == 2 &&
        f.binding.completions == 2 && f.finish_calls.size() == 4 &&
        f.parent_calls.size() == 14 && f.fallback_calls.size() == 12 &&
        f.get(Presentation, 1) == 0);
}

void malformed_assigned_calls_never_fallback() {
  enum Field {
    WrongKind,
    WrongEntry,
    WrongPc,
    WrongDelay,
    WrongReturn,
    WrongArguments,
    KindOnly,
    EntryOnly,
    PcOnly,
    DelayOnly,
    ReturnOnly
  };
  const std::array<Field, 11> fields{
      WrongKind, WrongEntry, WrongPc, WrongDelay, WrongReturn, WrongArguments,
      KindOnly, EntryOnly, PcOnly, DelayOnly, ReturnOnly};
  for (Field field : fields) {
    Fixture f;
    Nba97GameFirstPeriodStartupEvent event{};
    event.pc = 0x80067424u;
    event.delay_slot_pc = 0x80067428u;
    event.entry = 0x8002ddccu;
    event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_2DDCC;
    event.argument_count = 0;
    auto registers = f.context.registers;
    registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x8006742cu, 15};
    switch (field) {
    case WrongKind:
      event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_2A254;
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
      event.argument_count = 1;
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
      event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_2A254;
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = 0x8006000cu;
      break;
    case PcOnly:
      event.delay_slot_pc = 0x80060004u;
      event.entry = 0x80060008u;
      event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_2A254;
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = 0x8006000cu;
      break;
    case DelayOnly:
      event.pc = 0x80060000u;
      event.entry = 0x80060008u;
      event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_2A254;
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = 0x8006000cu;
      break;
    case ReturnOnly:
      event.pc = 0x80060000u;
      event.delay_slot_pc = 0x80060004u;
      event.entry = 0x80060008u;
      event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_2A254;
      break;
    }
    const auto before = registers;
    check(!nba97_game_period_presentation_finish_from_first_period_startup(
              &f.binding, &f.context.memory, &event, &registers) &&
          f.binding.result == NBA97_TEXT_ARGUMENT &&
          f.binding.invocations == 0 && f.fallback_calls.empty());
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
      check(same(registers.gpr[i], before.gpr[i]));
  }

  Fixture unrelated;
  Nba97GameFirstPeriodStartupEvent event{};
  event.pc = 0x80067434u;
  event.delay_slot_pc = 0x80067438u;
  event.entry = 0x8002a254u;
  event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_2A254;
  event.argument_count = 1;
  auto registers = unrelated.context.registers;
  registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x8006743cu, 15};
  check(nba97_game_period_presentation_finish_from_first_period_startup(
            &unrelated.binding, &unrelated.context.memory, &event,
            &registers) == 1 &&
        unrelated.fallback_calls.size() == 1 &&
        unrelated.binding.invocations == 0);
}
} // namespace

int main() {
  optional_zero_skips_and_nonzero_composes();
  typed_child_failures_and_valid_gpr_prefix();
  reusable_binding_and_actual_clear_effect();
  malformed_assigned_calls_never_fallback();
  std::printf("game period-presentation composition: %u checks passed\n",
              checks);
}
