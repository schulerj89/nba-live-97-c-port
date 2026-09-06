#include "game_actor_contact_gate_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "actor contact composition check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

struct Fixture {
  static constexpr std::uint32_t Ram = 0x80000000u;
  static constexpr std::uint32_t Ball = 0x80018000u;
  static constexpr std::uint32_t EntrySp = 0x800ff000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000);
  std::vector<std::uint8_t> known =
      std::vector<std::uint8_t>(0x110000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97GameContactDispatchContext context{};
  Nba97GameContactDispatchProgress progress{};
  Nba97GameActorContactGateBinding binding{};
  std::vector<Nba97GameActorContactGateEvent> calls;
  unsigned refuse_call = 0;

  Fixture() {
    context.memory = {&region, 1};
    context.operation_budget = 2000;
    context.io = parent_dispatch;
    context.user = this;
    for (auto &word : context.machine.registers.gpr)
      word = {0, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        0x80068e10u, 15};
    context.machine.hi = {0x12345678u, 15};
    context.machine.lo = {0x9abcdef0u, 15};

    binding.operation_budget = 32;
    binding.io = child;
    binding.user = this;

    put(0x800fdc48u, Ball, 4);
    put(Ball + 0xb4u, 1, 2);
    for (unsigned i = 1; i <= 11; ++i) {
      std::uint32_t actor = 0x80020000u + i * 0x100u;
      put(0x800fdcbcu + i * 4u, i == 11 ? Ball : actor, 4);
      put(actor + 8, i * 0x100u, 4);
    }
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    auto offset = address - Ram;
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
      known[offset + i] = 1;
    }
  }

  static int child(void *user, const Nba97GameTextMemory *,
                   const Nba97GameActorContactGateEvent *event,
                   Nba97GameActorContactGateMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(user);
    fixture.calls.push_back(*event);
    if (fixture.refuse_call == fixture.calls.size())
      return 0;
    const std::uint32_t values[4] = {0, 1, 2, 0xffffffffu};
    machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {
        values[(fixture.calls.size() - 1) % 4], 15};
    return 1;
  }

  static int parent_dispatch(void *user, const Nba97GameTextMemory *memory,
      const Nba97GameContactDispatchEvent *event,
      Nba97GameContactDispatchMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(user);
    if (event->entry != 0x8005faa8u)
      return 0;
    return nba97_game_actor_contact_gate_from_contact_dispatch(
        &fixture.binding, memory, event, machine);
  }

  int run() { return nba97_game_contact_dispatch(&context, &progress); }
};

void natural_aj_composition() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
  check(f.binding.invocations == 45 && f.calls.size() == 45 &&
        f.binding.child_result == NBA97_TEXT_COMPLETE &&
        f.binding.progress.completed);
  check(f.binding.event.pc == 0x8006104cu &&
        f.binding.event.delay_slot_pc == 0x80061050u &&
        f.binding.event.entry == 0x8005faa8u &&
        f.binding.event.argument_count == 2 &&
        f.binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80061054u);
  check(f.calls.front().pc == 0x8005faccu &&
        f.calls.front().delay_slot_pc == 0x8005fad0u &&
        f.calls.front().entry == 0x8005f948u &&
        f.calls.front().argument_count == 3 &&
        f.binding.progress.returned_value.word == 1 &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Fixture::EntrySp &&
        f.progress.machine.hi.word == 0x12345678u &&
        f.progress.machine.lo.word == 0x9abcdef0u);
}

void natural_rejection_and_failure_prefixes() {
  Fixture rejected;
  for (unsigned i = 1; i <= 10; ++i)
    rejected.put(0x80020000u + i * 0x100u + 8, i * 0x2000u, 4);
  check(rejected.run() == NBA97_TEXT_COMPLETE && rejected.progress.completed &&
        rejected.binding.invocations == 9 && rejected.calls.empty() &&
        rejected.binding.progress.returned_value.word == 0);

  Fixture limited;
  limited.binding.operation_budget = 0;
  check(limited.run() == NBA97_TEXT_IO_REFUSED &&
        limited.progress.stopped_pc == 0x8006104cu &&
        limited.progress.stopped_entry == 0x8005faa8u &&
        limited.binding.child_result == NBA97_TEXT_LIMIT &&
        limited.binding.progress.stopped_pc == 0x8005faacu &&
        limited.binding.progress.operations == 0);

  Fixture refused;
  refused.refuse_call = 1;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.progress.stopped_pc == 0x8006104cu &&
        refused.binding.child_result == NBA97_TEXT_IO_REFUSED &&
        refused.binding.progress.stopped_pc == 0x8005faccu &&
        refused.calls.size() == 1);
}

void adapter_validation() {
  Fixture f;
  Nba97GameContactDispatchEvent event{};
  event.pc = 0x8006104cu;
  event.delay_slot_pc = 0x80061050u;
  event.entry = 0x8005faa8u;
  event.kind = NBA97_GAME_CONTACT_DISPATCH_CHILD_8005FAA8;
  event.argument_count = 2;
  auto machine = f.context.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80061054u, 15};
  check(!nba97_game_actor_contact_gate_from_contact_dispatch(
      nullptr, &f.context.memory, &event, &machine));
  event.pc = 0x80061050u;
  auto before = machine;
  check(!nba97_game_actor_contact_gate_from_contact_dispatch(
            &f.binding, &f.context.memory, &event, &machine) &&
        f.binding.child_result == NBA97_TEXT_ARGUMENT &&
        machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            before.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word);
  event.pc = 0x8006104cu;
  f.binding.access_journal = nullptr;
  f.binding.access_journal_capacity = 1;
  check(!nba97_game_actor_contact_gate_from_contact_dispatch(
            &f.binding, &f.context.memory, &event, &machine) &&
        f.binding.child_result == NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  natural_aj_composition();
  natural_rejection_and_failure_prefixes();
  adapter_validation();
  std::printf("game actor contact composition: %u checks passed\n", checks);
}
