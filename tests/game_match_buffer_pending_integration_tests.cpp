#include "game_match_buffer_pending_adapter.h"

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
    std::fprintf(stderr, "match-buffer pending composition check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Pending = 0x800fe864u;
constexpr std::uint32_t Stack = 0x800ff800u;

bool same(Nba97GamePeriodStartupWord a, Nba97GamePeriodStartupWord b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

struct Fixture {
  std::vector<std::uint8_t> bytes =
      std::vector<std::uint8_t>(0x100000u, 0);
  std::vector<std::uint8_t> known =
      std::vector<std::uint8_t>(0x100000u, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameMatchBufferPendingAccess, 2> journal{};
  Nba97GamePeriodStartupContext context{};
  Nba97GamePeriodStartupProgress progress{};
  Nba97GameMatchBufferPendingPeriodBinding binding{};
  std::vector<Nba97GamePeriodStartupEvent> all_calls;
  std::vector<Nba97GamePeriodStartupEvent> fallback_calls;
  bool poison_before_second = false;

  Fixture() {
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
      context.registers.gpr[i] = {0x33000000u + i, 15};
    context.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0, 15};
    context.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 15};
    context.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80068c54u, 15};
    context.memory = {&region, 1};
    context.operation_budget = 100;
    context.io = bridge;
    context.user = this;
    binding.operation_budget = 1;
    binding.access_journal = journal.data();
    binding.access_journal_capacity = journal.size();
    binding.fallback = fallback;
    binding.fallback_user = this;
    put(0x800fdb68u, 0, 2);
    put(0x80020c14u, 0x800fed00u, 4);
    put(0x8001edecu, 0, 2);
    put(Pending, 0x5a, 1);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    const auto offset = static_cast<std::size_t>(address - Ram);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
      known[offset + i] = 1;
    }
  }

  static int bridge(void *user, const Nba97GameTextMemory *memory,
                    const Nba97GamePeriodStartupEvent *event,
                    Nba97GamePeriodStartupRegisters *registers) {
    auto &f = *static_cast<Fixture *>(user);
    f.all_calls.push_back(*event);
    return nba97_game_match_buffer_pending_from_period_startup(
        &f.binding, memory, event, registers);
  }

  static int fallback(void *user, const Nba97GameTextMemory *,
                      const Nba97GamePeriodStartupEvent *event,
                      Nba97GamePeriodStartupRegisters *) {
    auto &f = *static_cast<Fixture *>(user);
    f.fallback_calls.push_back(*event);
    if (f.poison_before_second && event->pc == 0x800674f8u)
      f.known[Pending - Ram] = 2;
    return 1;
  }

  int run() { return nba97_game_period_startup(&context, &progress); }
};

void actual_parent_two_calls() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.operations == 23);
  check(f.all_calls.size() == 13 &&
        f.all_calls[7].pc == 0x800674f0u &&
        f.all_calls[7].entry == 0x80076b28u &&
        f.all_calls[8].pc == 0x800674f8u &&
        f.all_calls[8].entry == 0x80076b3cu &&
        f.all_calls[9].pc == 0x80067500u &&
        f.all_calls[9].entry == 0x80076b28u &&
        f.all_calls[10].pc == 0x80067508u &&
        f.all_calls[10].entry == 0x80076b3cu);
  check(f.binding.invocations == 2 && f.binding.completions == 2 &&
        f.binding.first_invocations == 1 &&
        f.binding.second_invocations == 1 && f.fallback_calls.size() == 11);
  check(f.binding.first_event.pc == 0x800674f0u &&
        f.binding.first_event.delay_slot_pc == 0x800674f4u &&
        f.binding.first_event.entry == 0x80076b28u &&
        f.binding.first_event.operation == 15 &&
        f.binding.first_event.kind == NBA97_GAME_PERIOD_STARTUP_76B28 &&
        f.binding.first_event.argument_count == 0 &&
        f.binding.second_event.pc == 0x80067500u &&
        f.binding.second_event.delay_slot_pc == 0x80067504u &&
        f.binding.second_event.entry == 0x80076b28u &&
        f.binding.second_event.operation == 17 &&
        f.binding.second_event.kind == NBA97_GAME_PERIOD_STARTUP_76B28 &&
        f.binding.second_event.argument_count == 0);
  check(f.bytes[Pending - Ram] == 1 && f.known[Pending - Ram] == 1 &&
        f.binding.result == NBA97_TEXT_COMPLETE &&
        f.binding.progress.completed && f.binding.progress.stores == 1 &&
        f.binding.progress.machine.hi.word == 0 &&
        f.binding.progress.machine.hi.known_mask == 0 &&
        f.binding.progress.machine.lo.word == 0 &&
        f.binding.progress.machine.lo.known_mask == 0 &&
        f.binding.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                .word == 0x80067508u);
}

void first_and_second_failure_prefixes() {
  Fixture first;
  first.binding.operation_budget = 0;
  check(first.run() == NBA97_TEXT_IO_REFUSED && !first.progress.completed &&
        first.progress.stopped_pc == 0x800674f0u &&
        first.progress.stopped_entry == 0x80076b28u &&
        first.binding.invocations == 1 && first.binding.completions == 0 &&
        first.binding.first_invocations == 1 &&
        first.binding.second_invocations == 0 &&
        first.binding.result == NBA97_TEXT_LIMIT &&
        first.binding.progress.stopped_pc == 0x80076b30u &&
        first.bytes[Pending - Ram] == 0x5a);

  Fixture second;
  second.poison_before_second = true;
  check(second.run() == NBA97_TEXT_IO_REFUSED && !second.progress.completed &&
        second.progress.stopped_pc == 0x80067500u &&
        second.progress.stopped_entry == 0x80076b28u &&
        second.binding.invocations == 2 && second.binding.completions == 1 &&
        second.binding.first_invocations == 1 &&
        second.binding.second_invocations == 1 &&
        second.binding.result == NBA97_TEXT_ARGUMENT &&
        second.binding.progress.stopped_pc == 0x80076b30u &&
        second.binding.progress.operations == 1 &&
        second.binding.progress.stores == 0 &&
        second.bytes[Pending - Ram] == 1 && second.known[Pending - Ram] == 2);
}

void reusable_binding() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE);
  f.put(Pending, 0x88, 1);
  check(f.run() == NBA97_TEXT_COMPLETE && f.binding.invocations == 4 &&
        f.binding.completions == 4 && f.binding.first_invocations == 2 &&
        f.binding.second_invocations == 2 && f.fallback_calls.size() == 22 &&
        f.all_calls.size() == 26 && f.bytes[Pending - Ram] == 1 &&
        f.binding.progress.completed);
}

void malformed_assigned_calls_do_not_fallback() {
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
    Nba97GamePeriodStartupEvent event{};
    event.pc = 0x800674f0u;
    event.delay_slot_pc = 0x800674f4u;
    event.entry = 0x80076b28u;
    event.kind = NBA97_GAME_PERIOD_STARTUP_76B28;
    event.argument_count = 0;
    auto registers = f.context.registers;
    registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x800674f8u, 15};
    switch (field) {
    case WrongKind:
      event.kind = NBA97_GAME_PERIOD_STARTUP_A584C;
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
      event.kind = NBA97_GAME_PERIOD_STARTUP_A584C;
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = 0x8006000cu;
      break;
    case PcOnly:
      event.delay_slot_pc = 0x80060004u;
      event.entry = 0x80060008u;
      event.kind = NBA97_GAME_PERIOD_STARTUP_A584C;
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = 0x8006000cu;
      break;
    case DelayOnly:
      event.pc = 0x80060000u;
      event.entry = 0x80060008u;
      event.kind = NBA97_GAME_PERIOD_STARTUP_A584C;
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = 0x8006000cu;
      break;
    case ReturnOnly:
      event.pc = 0x80060000u;
      event.delay_slot_pc = 0x80060004u;
      event.entry = 0x80060008u;
      event.kind = NBA97_GAME_PERIOD_STARTUP_A584C;
      break;
    }
    const auto before = registers;
    check(!nba97_game_match_buffer_pending_from_period_startup(
              &f.binding, &f.context.memory, &event, &registers) &&
          f.binding.result == NBA97_TEXT_ARGUMENT &&
          f.binding.invocations == 0 && f.fallback_calls.empty());
    for (unsigned i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
      check(same(registers.gpr[i], before.gpr[i]));
  }

  Fixture unrelated;
  Nba97GamePeriodStartupEvent event{};
  event.pc = 0x80067510u;
  event.delay_slot_pc = 0x80067514u;
  event.entry = 0x800a584cu;
  event.kind = NBA97_GAME_PERIOD_STARTUP_A584C;
  auto registers = unrelated.context.registers;
  check(nba97_game_match_buffer_pending_from_period_startup(
            &unrelated.binding, &unrelated.context.memory, &event,
            &registers) == 1 &&
        unrelated.fallback_calls.size() == 1 &&
        unrelated.binding.invocations == 0);
}
} // namespace

int main() {
  actual_parent_two_calls();
  first_and_second_failure_prefixes();
  reusable_binding();
  malformed_assigned_calls_do_not_fallback();
  std::printf("game match-buffer pending composition: %u checks passed\n",
              checks);
}
