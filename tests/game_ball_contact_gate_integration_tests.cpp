#include "game_ball_contact_gate_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "ball contact composition check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

struct Fixture {
  static constexpr std::uint32_t Ball = 0x80001000u;
  static constexpr std::uint32_t Actor = 0x80002000u;
  static constexpr std::uint32_t EntrySp = 0x801ff100u;
  static constexpr std::uint32_t Return = 0x80061078u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000);
  std::vector<std::uint8_t> known =
      std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{0x80000000u, bytes.data(), known.data(),
                             bytes.size()};
  Nba97GameBallContactGateContext context{};
  Nba97GameBallContactGateProgress progress{};
  Nba97GameBallContactGateBinding binding{};
  std::vector<std::uint32_t> unresolved_calls;
  bool refuse_unresolved = false;

  Fixture() {
    context.memory = {&region, 1};
    context.operation_budget = 32;
    for (auto &word : context.machine.registers.gpr)
      word = {0, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {Actor, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {Ball, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {Return, 15};
    context.machine.hi = {0x12345678u, 15};
    context.machine.lo = {0x9abcdef0u, 15};

    binding.child_operation_budget = 10000;
    binding.io = unresolved;
    binding.user = this;
    binding.contact_binding.child_operation_budget = 1000;

    put32(Ball, 10);
    put32(Ball + 8, 0);
    put32(Actor + 8, 0);
    put16(0x800fdbccu, 0xffff);
    put32(0x800fdb58u, 1);
    put16(0x800fe8c4u, 0);
    put16(0x800fe8ccu, 0);
    put16(0x800fdb90u, 0);
    put16(0x800fdb94u, 0);
    put16(0x800fdbd4u, 0);
    put16(0x800fdbd2u, 0);
    put16(0x800fdbd0u, 0xffff);
    put32(0x800fdc40u, 0x8001edf4u);
    put32(0x800fdc48u, Actor);
    put8(Actor + 0xd9, 0);
    put16(Actor + 4, 0xffff);
    put32(Actor + 0x20, 0x80003000u);
    put8(0x8000300du, 0);
  }

  void put8(std::uint32_t address, std::uint8_t value) {
    bytes[address - region.base] = value;
  }
  void put16(std::uint32_t address, std::uint16_t value) {
    put8(address, static_cast<std::uint8_t>(value));
    put8(address + 1, static_cast<std::uint8_t>(value >> 8));
  }
  void put32(std::uint32_t address, std::uint32_t value) {
    put16(address, static_cast<std::uint16_t>(value));
    put16(address + 2, static_cast<std::uint16_t>(value >> 16));
  }
  std::uint16_t get16(std::uint32_t address) const {
    auto offset = address - region.base;
    return static_cast<std::uint16_t>(bytes[offset] |
        (static_cast<std::uint16_t>(bytes[offset + 1]) << 8));
  }

  static int unresolved(void *user, const Nba97GameTextMemory *,
                        const Nba97GameBallActorContactEvent *event,
                        Nba97GameBallActorContactMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(user);
    fixture.unresolved_calls.push_back(event->pc);
    if (fixture.refuse_unresolved)
      return 0;
    if (event->entry == 0x8007066cu || event->entry == 0x8005d140u ||
        event->entry == 0x8002ab70u || event->entry == 0x800601b8u ||
        event->entry == 0x80060240u || event->entry == 0x80060008u) {
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {
          event->entry == 0x800601b8u || event->entry == 0x80060240u ||
                  event->entry == 0x80060008u
              ? 1u
              : 0u,
          15};
    }
    return 1;
  }

  int run() {
    return nba97_game_ball_contact_gate_run(&context, &progress, &binding);
  }
};

void actual_contact_owner_composition() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
  check(f.binding.invocations == 1 &&
        f.binding.event.pc == 0x80060ed4u &&
        f.binding.event.delay_slot_pc == 0x80060ed8u &&
        f.binding.event.entry == 0x800602ccu &&
        f.binding.event.argument_count == 3 &&
        f.binding.child_result == NBA97_TEXT_COMPLETE &&
        f.binding.contact_progress.completed);
  check(f.binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            Fixture::Ball &&
        f.binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            Fixture::Actor &&
        f.binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
            0 &&
        f.binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Fixture::EntrySp - 0x18 &&
        f.binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80060edcu);
  check(f.binding.contact_progress.restored_return_address.word ==
            0x80060edcu &&
        f.binding.contact_progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == Fixture::EntrySp - 0x18 &&
        f.progress.restored_return_address.word == Fixture::Return &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Fixture::EntrySp &&
        f.progress.returned_value.word == 1 &&
        f.progress.returned_value.known_mask == 15);
  check(f.binding.contact_binding.rule_delay_count == 1 &&
        f.binding.contact_binding.rule_delay_event[0].pc == 0x80060788u &&
        f.binding.contact_binding.actor_resume_count == 0 &&
        f.binding.contact_binding.unresolved_count ==
            f.unresolved_calls.size() &&
        f.get16(Fixture::Ball + 0x18) == 0);
}

void gate_skip_and_child_failures() {
  Fixture outside;
  outside.put32(Fixture::Ball + 8, 33u * 256u);
  check(outside.run() == NBA97_TEXT_COMPLETE && outside.progress.completed &&
        outside.progress.returned_value.word == 0 &&
        outside.binding.invocations == 0 &&
        outside.unresolved_calls.empty());

  Fixture refused;
  refused.refuse_unresolved = true;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.progress.stopped_pc == 0x80060ed4u &&
        refused.progress.stopped_entry == 0x800602ccu &&
        refused.binding.invocations == 1 &&
        refused.binding.child_result == NBA97_TEXT_IO_REFUSED &&
        refused.progress.returned_value.word ==
            refused.binding.contact_progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_V0]
                .word &&
        refused.progress.returned_value.known_mask ==
            refused.binding.contact_progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask);

  Fixture limited;
  limited.binding.child_operation_budget = 0;
  check(limited.run() == NBA97_TEXT_IO_REFUSED &&
        limited.progress.stopped_pc == 0x80060ed4u &&
        limited.binding.child_result == NBA97_TEXT_LIMIT &&
        limited.binding.contact_progress.operations == 0 &&
        limited.progress.returned_value.word ==
            limited.binding.contact_progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_V0]
                .word &&
        limited.progress.returned_value.known_mask ==
            limited.binding.contact_progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask);
}

void adapter_validation_and_restoration() {
  Fixture f;
  auto original_io = f.context.io;
  auto original_user = f.context.user;
  check(f.run() == NBA97_TEXT_COMPLETE && f.context.io == original_io &&
        f.context.user == original_user);
  check(nba97_game_ball_contact_gate_run(nullptr, &f.progress, &f.binding) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_ball_contact_gate_run(&f.context, nullptr, &f.binding) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_ball_contact_gate_run(&f.context, &f.progress, nullptr) ==
        NBA97_TEXT_ARGUMENT);
  Fixture journal;
  journal.binding.access_journal = nullptr;
  journal.binding.access_journal_capacity = 1;
  check(journal.run() == NBA97_TEXT_IO_REFUSED &&
        journal.binding.child_result == NBA97_TEXT_ARGUMENT &&
        journal.progress.stopped_pc == 0x80060ed4u);
}
} // namespace

int main() {
  actual_contact_owner_composition();
  gate_skip_and_child_failures();
  adapter_validation_and_restoration();
  std::printf("game ball contact composition: %u checks passed\n", checks);
}
