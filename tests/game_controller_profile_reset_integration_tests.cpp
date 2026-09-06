#include "game_controller_profile_reset_adapter.h"

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
                 "controller profile integration check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

struct Fixture {
  static constexpr std::uint32_t Ram = 0x80000000u;
  static constexpr std::uint32_t EntrySp = 0x800ff800u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97GameMatchStateResetContext context{};
  Nba97GameMatchStateResetProgress progress{};
  Nba97GameControllerProfileResetBinding binding{};
  unsigned fallbackCalls = 0;

  Fixture() {
    context.memory = {&region, 1};
    context.operation_budget = 1000;
    context.io = fallback;
    context.user = this;
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x31000000u + i, 15};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[4] = {0x12345678u, 15};
    context.machine.registers.gpr[29] = {EntrySp, 15};
    context.machine.registers.gpr[31] = {0x82345678u, 15};
    context.machine.hi = {0x11223344u, 3};
    context.machine.lo = {0x55667788u, 12};
    put(0x8001edecu, 98, 2);
    for (unsigned i = 0; i < 8; ++i) {
      put(0x80021ddeu + i, i, 1);
      put(0x80020c1cu + i * 108u + 0x6bu, 0, 1);
    }
    for (unsigned i = 0; i < 256; ++i)
      put(0x800bc94cu + i, 0x60u + i, 1);
    nba97_game_controller_profile_reset_binding_init(&binding, 2000, 32);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    const auto offset = static_cast<std::size_t>(address - Ram);
    for (unsigned i = 0; i < width; ++i)
      bytes[offset + i] = static_cast<std::uint8_t>(value >> (8u * i));
  }

  std::uint32_t get(std::uint32_t address, unsigned width = 1) const {
    const auto offset = static_cast<std::size_t>(address - Ram);
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[offset + i]) << (8u * i);
    return value;
  }

  static int fallback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameMatchStateResetEvent *event,
                      Nba97GameMatchStateResetMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    ++fixture.fallbackCalls;
    check(event != nullptr && machine != nullptr);
    check(event->entry != 0x80083490u);
    check(machine->registers.gpr[0].word == 0 &&
          machine->registers.gpr[0].known_mask == 15);
    return 1;
  }

  int run() {
    return nba97_game_match_state_reset_with_controller_profile_reset(
        &context, &binding, &progress);
  }
};

void naturalCallerCompletes() {
  Fixture fixture;
  check(fixture.run() == NBA97_TEXT_COMPLETE && fixture.progress.completed);
  check(fixture.binding.invocations == 1 && fixture.binding.completions == 1 &&
        fixture.binding.result == NBA97_TEXT_COMPLETE &&
        fixture.binding.zero_invocations == 8);
  check(fixture.binding.event.pc == 0x80065a38u &&
        fixture.binding.event.delay_slot_pc == 0x80065a3cu &&
        fixture.binding.event.entry == 0x80083490u &&
        fixture.binding.event.kind == NBA97_GAME_MATCH_STATE_RESET_80083490 &&
        fixture.binding.event.argument_count == 1 &&
        fixture.binding.event.invocation == 1);
  check(fixture.binding.progress.frame_stack_pointer ==
            Fixture::EntrySp - 0x20u - 0x28u &&
        fixture.binding.progress.completed);
  for (unsigned record = 0; record < 8; ++record) {
    const auto base = 0x8001ef7cu + record * 120u;
    for (unsigned byte = 0; byte < 0x24; ++byte)
      check(fixture.get(base + byte) == 0);
    for (unsigned byte = 0; byte < 59; ++byte)
      check(fixture.get(base + 0x3cu + byte) ==
            static_cast<std::uint8_t>(0x60u + byte));
  }
  check(fixture.get(0x8001edf2u, 2) == 0 &&
        fixture.get(0x800fdb9cu, 2) == 0xffffu &&
        fixture.progress.machine.registers.gpr[29].word == Fixture::EntrySp &&
        fixture.progress.restored_return_address.word == 0x82345678u &&
        fixture.progress.machine.hi.word == 0x11223344u &&
        fixture.progress.machine.hi.known_mask == 3 &&
        fixture.progress.machine.lo.word == 0x55667788u &&
        fixture.progress.machine.lo.known_mask == 12);
}

void ownerAndNestedBudgetPrefixes() {
  Fixture ownerLimit;
  ownerLimit.put(0x8001edf2u, 0x7676u, 2);
  ownerLimit.binding.operation_budget = 0;
  check(ownerLimit.run() == NBA97_TEXT_LIMIT &&
        ownerLimit.progress.stopped_pc == 0x80065a38u &&
        ownerLimit.binding.progress.stopped_pc == 0x80083494u &&
        ownerLimit.binding.progress.operations == 0 &&
        ownerLimit.binding.progress.frame_stack_pointer ==
            Fixture::EntrySp - 0x20u - 0x28u &&
        ownerLimit.binding.progress.machine.registers.gpr[31].word ==
            0x80065a40u &&
        ownerLimit.get(0x8001edf2u, 2) == 0x7676u);

  Fixture nestedLimit;
  nestedLimit.put(0x8001edf2u, 0x4545u, 2);
  const auto firstHeader = nestedLimit.get(0x8001ef7cu, 4);
  nestedLimit.binding.zero_operation_budget = 0;
  check(nestedLimit.run() == NBA97_TEXT_LIMIT &&
        nestedLimit.progress.stopped_pc == 0x80065a38u &&
        nestedLimit.binding.progress.stopped_pc == 0x800834d8u &&
        nestedLimit.binding.nested_result == NBA97_TEXT_LIMIT &&
        nestedLimit.binding.zero_invocations == 1 &&
        nestedLimit.binding.progress.machine.registers.gpr[1].word == 0 &&
        nestedLimit.binding.progress.machine.registers.gpr[1].known_mask ==
            15 &&
        nestedLimit.binding.progress.machine.registers.gpr[6].word == 0 &&
        nestedLimit.binding.progress.machine.registers.gpr[10].word == 0 &&
        nestedLimit.get(0x8001ef7cu, 4) == firstHeader &&
        nestedLimit.get(0x8001edf2u, 2) == 0x4545u);
}

void wrapperArgumentsAndFallback() {
  Fixture fixture;
  check(nba97_game_match_state_reset_with_controller_profile_reset(
            nullptr, &fixture.binding, &fixture.progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_match_state_reset_with_controller_profile_reset(
            &fixture.context, nullptr, &fixture.progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_match_state_reset_with_controller_profile_reset(
            &fixture.context, &fixture.binding, nullptr) ==
        NBA97_TEXT_ARGUMENT);

  fixture.context.io = nullptr;
  check(fixture.run() == NBA97_TEXT_IO_REFUSED &&
        fixture.progress.stopped_pc == 0x80065a0cu &&
        fixture.binding.invocations == 0);
}
} // namespace

int main() {
  naturalCallerCompletes();
  ownerAndNestedBudgetPrefixes();
  wrapperArgumentsAndFallback();
  std::printf("game controller profile integration: %u checks\n", checks);
}
