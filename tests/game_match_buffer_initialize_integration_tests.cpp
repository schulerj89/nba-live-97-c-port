#include "game_match_buffer_initialize_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "match buffer composition check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

struct Fixture {
  static constexpr std::uint32_t Base = 0x80000000u;
  static constexpr std::uint32_t Stack = 0x801ff000u;
  std::vector<std::uint8_t> bytes =
      std::vector<std::uint8_t>(0x200000, 0xa5);
  std::vector<std::uint8_t> known =
      std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameMatchStateResetAccess, 64> parent_journal{};
  std::array<Nba97GameMatchBufferInitializeAccess, 16> child_journal{};
  Nba97GameMatchStateResetContext context{};
  Nba97GameMatchStateResetProgress progress{};
  Nba97GameMatchBufferInitializeBinding binding{};
  std::vector<Nba97GameMatchStateResetEvent> fallback_calls;
  std::vector<Nba97GameMatchBufferInitializeEvent> child_calls;
  bool refuse_child = false;

  explicit Fixture(std::uint16_t mode = 7) {
    context.memory = {&region, 1};
    context.operation_budget = 100;
    context.io = nba97_game_match_buffer_initialize_from_match_state_reset;
    context.user = &binding;
    context.access_journal = parent_journal.data();
    context.access_journal_capacity = parent_journal.size();
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x11000000u + i, 15};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        0x81234568u, 15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x87654321u, 10};
    binding.operation_budget = 32;
    binding.zero_operation_budget = 1000;
    binding.io = child;
    binding.user = this;
    binding.fallback = fallback;
    binding.fallback_user = this;
    binding.access_journal = child_journal.data();
    binding.access_journal_capacity = child_journal.size();
    put(0x8001edecu, mode, 2);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i) {
      bytes[address - Base + i] =
          static_cast<std::uint8_t>(value >> (8u * i));
      known[address - Base + i] = 1;
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width = 4) const {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[address - Base + i]) << (8u * i);
    return value;
  }

  static int fallback(void *user, const Nba97GameTextMemory *,
                      const Nba97GameMatchStateResetEvent *event,
                      Nba97GameMatchStateResetMachine *) {
    auto &f = *static_cast<Fixture *>(user);
    f.fallback_calls.push_back(*event);
    return 1;
  }
  static int child(void *user, const Nba97GameTextMemory *,
                   const Nba97GameMatchBufferInitializeEvent *event,
                   Nba97GameMatchBufferInitializeMachine *machine) {
    auto &f = *static_cast<Fixture *>(user);
    f.child_calls.push_back(*event);
    machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0xcafebabeu, 6};
    machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 3] = {0x13572468u, 9};
    return f.refuse_child ? 0 : 1;
  }
  int run() { return nba97_game_match_state_reset(&context, &progress); }
};

void natural_non98_actual_buffer_and_zero() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        !f.progress.mode_98);
  check(f.binding.invocations == 1 && f.binding.completions == 1 &&
        f.binding.event.pc == 0x80065af8u &&
        f.binding.event.delay_slot_pc == 0x80065afcu &&
        f.binding.event.entry == 0x8006432cu &&
        f.binding.event.argument_count == 0);
  check(f.binding.result == NBA97_TEXT_COMPLETE &&
        f.binding.zero_invocations == 1 &&
        f.binding.zero_result == NBA97_TEXT_COMPLETE &&
        f.binding.zero_progress.completed &&
        f.binding.zero_progress.destination == 0x800f9ffcu &&
        f.binding.zero_progress.requested_length == 0x378u &&
        f.binding.child_80076AD0_invocations == 1 &&
        f.child_calls.size() == 1);
  check(f.child_calls[0].pc == 0x80064370u &&
        f.child_calls[0].delay_slot_pc == 0x80064374u &&
        f.child_calls[0].entry == 0x80076ad0u &&
        f.child_calls[0].argument_count == 0);
  check(f.get(0x800f9ffcu, 4) == 0 && f.get(0x800fa000u, 2) == 0x76 &&
        f.get(0x800fa004u, 4) == 0x800ccc00u &&
        f.get(0x800fa008u, 4) == 0x800d5734u &&
        f.get(0x800fa00cu, 4) == 0);
  check(f.binding.progress.returned_value.word == 0xcafebabeu &&
        f.binding.progress.returned_value.known_mask == 6 &&
        f.binding.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 3]
                .word == 0x13572468u &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Fixture::Stack &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x81234568u && f.progress.machine.hi.word == 0x12345678u &&
        f.progress.machine.hi.known_mask == 5 &&
        f.progress.machine.lo.word == 0x87654321u &&
        f.progress.machine.lo.known_mask == 10);
}

void mode98_falls_back() {
  Fixture f(98);
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.mode_98 && f.binding.invocations == 0 &&
        f.binding.completions == 0 && f.binding.zero_invocations == 0 &&
        f.child_calls.empty());
  check(!f.fallback_calls.empty() &&
        f.fallback_calls.back().pc == 0x80065ae8u &&
        f.fallback_calls.back().entry == 0x80076ad0u &&
        f.get(0x800fa000u, 4) == 0xa5a5a5a5u);
}

void nested_and_core_failures() {
  Fixture zero;
  zero.binding.zero_operation_budget = 0;
  check(zero.run() == NBA97_TEXT_IO_REFUSED &&
        zero.progress.stopped_pc == 0x80065af8u &&
        zero.binding.result == NBA97_TEXT_LIMIT &&
        zero.binding.zero_result == NBA97_TEXT_LIMIT &&
        zero.binding.zero_progress.operations == 0 &&
        zero.binding.progress.stopped_pc == 0x8006433cu);

  Fixture core;
  core.binding.operation_budget = 0;
  check(core.run() == NBA97_TEXT_IO_REFUSED &&
        core.binding.result == NBA97_TEXT_LIMIT &&
        core.binding.progress.stopped_pc == 0x80064338u &&
        core.binding.zero_invocations == 0);

  Fixture refused;
  refused.refuse_child = true;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.binding.result == NBA97_TEXT_IO_REFUSED &&
        refused.binding.nested_result == NBA97_TEXT_IO_REFUSED &&
        refused.binding.progress.stopped_pc == 0x80064370u &&
        refused.binding.child_80076AD0_invocations == 1 &&
        refused.get(0x800fa008u, 4) == 0x800d5734u);
}

void repeated_binding_and_guard() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE);
  f.put(0x800f9ffcu, 0xffffffffu);
  check(f.run() == NBA97_TEXT_COMPLETE && f.binding.invocations == 2 &&
        f.binding.completions == 2 && f.binding.zero_invocations == 1 &&
        f.binding.child_80076AD0_invocations == 1 &&
        f.binding.zero_progress.completed && f.get(0x800f9ffcu, 4) == 0);

  Nba97GameMatchStateResetEvent event{};
  event.pc = 0x80065af8u;
  event.delay_slot_pc = 0x80065afcu;
  event.entry = 0x8006432cu;
  event.kind = NBA97_GAME_MATCH_STATE_RESET_8006432C;
  event.invocation = 1;
  event.argument_count = 0;
  auto machine = f.context.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80065b00u, 15};
  event.pc = 0x80065af4u;
  auto before = machine;
  check(!nba97_game_match_buffer_initialize_from_match_state_reset(
            &f.binding, &f.context.memory, &event, &machine) &&
        f.binding.result == NBA97_TEXT_ARGUMENT &&
        machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            before.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word);
}

void malformed_assigned_boundaries_never_fall_back() {
  enum Field {
    Kind,
    Entry,
    Pc,
    Delay,
    ReturnAddress,
    Arguments,
    Invocation
  };
  const std::array<Field, 7> fields{
      Kind, Entry, Pc, Delay, ReturnAddress, Arguments, Invocation};
  for (Field field : fields) {
    Fixture f;
    Nba97GameMatchStateResetEvent event{};
    event.pc = 0x80065af8u;
    event.delay_slot_pc = 0x80065afcu;
    event.entry = 0x8006432cu;
    event.invocation = 1;
    event.kind = NBA97_GAME_MATCH_STATE_RESET_8006432C;
    event.argument_count = 0;
    auto machine = f.context.machine;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80065b00u, 15};
    switch (field) {
    case Kind:
      event.kind = NBA97_GAME_MATCH_STATE_RESET_80076AD0;
      break;
    case Entry:
      event.entry = 0x80064330u;
      break;
    case Pc:
      event.pc = 0x80065af4u;
      break;
    case Delay:
      event.delay_slot_pc = 0x80065af8u;
      break;
    case ReturnAddress:
      machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = 0x80065afcu;
      break;
    case Arguments:
      event.argument_count = 1;
      break;
    case Invocation:
      event.invocation = 2;
      break;
    }
    const auto before = machine;
    check(!nba97_game_match_buffer_initialize_from_match_state_reset(
              &f.binding, &f.context.memory, &event, &machine) &&
          f.binding.result == NBA97_TEXT_ARGUMENT &&
          f.binding.invocations == 0 && f.fallback_calls.empty() &&
          machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
              before.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word);
  }
}
} // namespace

int main() {
  natural_non98_actual_buffer_and_zero();
  mode98_falls_back();
  nested_and_core_failures();
  repeated_binding_and_guard();
  malformed_assigned_boundaries_never_fall_back();
  std::printf("game match buffer composition: %u checks passed\n", checks);
}
