#include "game_camera_override_end_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "camera override composition check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

struct Fixture {
  static constexpr std::uint32_t Ram = 0x80000000u;
  static constexpr std::uint32_t Flag = 0x800bc1f0u;
  static constexpr std::uint32_t EntrySp = 0x800ff000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000);
  std::vector<std::uint8_t> known =
      std::vector<std::uint8_t>(0x110000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97GameSelectionInput state{};
  Nba97GameCameraOverrideEndSelectionBinding binding{};
  bool refuse = false;
  unsigned calls = 0;

  Fixture() {
    for (unsigned i = 0; i < 8; ++i) {
      state.controller_table[i] = static_cast<std::uint8_t>(i);
      state.controller[i].team_base = -1;
      state.controller[i].selected = {
          static_cast<std::uint16_t>(0xdead + i), 1};
    }
    for (unsigned i = 0; i < 11; ++i) {
      state.entity_table[i] = static_cast<std::uint8_t>(i);
      state.entity[i].claim = -1;
    }
    state.controller[0].team_base = 0;
    state.ball = 10;
    state.incoming_s6 = {7, 1};
    state.tail_entity = 10;
    state.tail_state = 2;

    binding.memory = {&region, 1};
    binding.operation_budget = 16;
    binding.entry_machine_ready = 1;
    for (auto &word : binding.entry_machine.registers.gpr)
      word = {0, 15};
    binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {
        0x12345678u, 7};
    binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
        EntrySp, 15};
    binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        0x80065578u, 15};
    binding.entry_machine.hi = {0x11223344u, 5};
    binding.entry_machine.lo = {0x55667788u, 10};
    binding.io = child;
    binding.user = this;
    put(Flag, 1, 1);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    auto offset = address - Ram;
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
      known[offset + i] = 1;
    }
  }

  std::uint32_t get(std::uint32_t address, unsigned width) const {
    auto offset = address - Ram;
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[offset + i]) << (i * 8u);
    return value;
  }

  static int child(void *user, const Nba97GameTextMemory *,
                   const Nba97GameCameraOverrideEndEvent *event,
                   Nba97GameCameraOverrideEndMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(user);
    ++fixture.calls;
    check(event->pc == 0x8007a380u && event->entry == 0x8007a114u);
    check(fixture.state.controller[0].selected.word == 4 &&
          fixture.state.entity[4].claim == 0 &&
          fixture.state.tail_state == 2);
    fixture.state.controller[0].selected = {99, 1};
    fixture.state.entity[4].claim = 7;
    machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0xcafebabeu, 6};
    machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0] = {0xabcdef01u, 9};
    if (fixture.refuse)
      return 0;
    return 1;
  }

  int run() {
    return nba97_game_camera_override_end_from_selection(&binding, &state);
  }
};

void actual_selection_order_and_leaf() {
  Fixture f;
  check(f.run() == NBA97_SELECTION_OK &&
        f.binding.selection_result == NBA97_SELECTION_OK &&
        f.binding.tail_result == NBA97_TEXT_COMPLETE &&
        f.binding.invocations == 1 && f.calls == 1);
  check(f.binding.selection_effects.write_count == 1 &&
        f.binding.selection_effects.call_7a36c == 1 &&
        f.binding.selection_effects.tail_state_written == 1 &&
        f.binding.selection_effects.tail_state == 1);
  check(f.state.controller[0].selected.word == 99 &&
        f.state.entity[4].claim == 7 && f.state.tail_state == 1 &&
        f.get(Fixture::Flag, 1) == 0);
  check(f.binding.progress.completed &&
        f.binding.progress.returned_value.word == 0xcafebabeu &&
        f.binding.progress.returned_value.known_mask == 6 &&
        f.binding.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0]
                .word == 0xabcdef01u &&
        f.binding.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0]
                .known_mask == 9 &&
        f.binding.progress.restored_return_address.word == 0x80065578u &&
        f.binding.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == Fixture::EntrySp);
}

void zero_flag_and_no_call_selection_path() {
  Fixture zero;
  zero.put(Fixture::Flag, 0, 1);
  check(zero.run() == NBA97_SELECTION_OK && zero.calls == 0 &&
        zero.binding.invocations == 1 && zero.binding.progress.completed &&
        zero.state.controller[0].selected.word == 4 &&
        zero.state.entity[4].claim == 0 && zero.state.tail_state == 1);

  Fixture no_leaf;
  no_leaf.state.tail_state = 1;
  no_leaf.binding.entry_machine_ready = 0;
  check(no_leaf.run() == NBA97_SELECTION_OK && no_leaf.calls == 0 &&
        no_leaf.binding.invocations == 0 &&
        no_leaf.state.controller[0].selected.word == 4 &&
        no_leaf.state.entity[4].claim == 0 && no_leaf.state.tail_state == 1);
}

void refusal_limit_and_bridge_validation() {
  Fixture refused;
  refused.refuse = true;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.binding.tail_result == NBA97_TEXT_IO_REFUSED &&
        refused.binding.progress.stopped_pc == 0x8007a380u &&
        refused.state.controller[0].selected.word == 99 &&
        refused.state.entity[4].claim == 7 && refused.state.tail_state == 2 &&
        refused.get(Fixture::Flag, 1) == 1);

  Fixture limited;
  limited.binding.operation_budget = 0;
  check(limited.run() == NBA97_TEXT_LIMIT &&
        limited.binding.tail_result == NBA97_TEXT_LIMIT &&
        limited.binding.progress.stopped_pc == 0x8007a370u &&
        limited.state.controller[0].selected.word == 4 &&
        limited.state.entity[4].claim == 0 && limited.state.tail_state == 2 &&
        limited.calls == 0);

  Fixture bridge;
  bridge.binding.entry_machine_ready = 0;
  check(bridge.run() == NBA97_TEXT_ARGUMENT &&
        bridge.binding.tail_result == NBA97_TEXT_ARGUMENT &&
        bridge.binding.invocations == 0 && bridge.calls == 0 &&
        bridge.state.controller[0].selected.word == 4 &&
        bridge.state.entity[4].claim == 0 && bridge.state.tail_state == 2);
  Fixture wrong_ra;
  wrong_ra.binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .word = 0x8006557cu;
  check(wrong_ra.run() == NBA97_TEXT_ARGUMENT &&
        wrong_ra.binding.invocations == 0 && wrong_ra.state.tail_state == 2);
}

void selection_failure_and_arguments() {
  Fixture invalid;
  invalid.state.entity_table[10] = NBA97_SELECTION_UNKNOWN_REF;
  auto before = invalid.state;
  check(invalid.run() == NBA97_SELECTION_UNRESOLVED &&
        invalid.binding.selection_result == NBA97_SELECTION_UNRESOLVED &&
        invalid.binding.invocations == 0 && invalid.calls == 0 &&
        std::memcmp(&invalid.state, &before, sizeof before) == 0);
  Fixture f;
  check(nba97_game_camera_override_end_from_selection(nullptr, &f.state) ==
        NBA97_SELECTION_INVALID);
  check(nba97_game_camera_override_end_from_selection(&f.binding, nullptr) ==
        NBA97_SELECTION_INVALID);
}
} // namespace

int main() {
  actual_selection_order_and_leaf();
  zero_flag_and_no_call_selection_path();
  refusal_limit_and_bridge_validation();
  selection_failure_and_arguments();
  std::printf("game camera override composition: %u checks passed\n", checks);
}
