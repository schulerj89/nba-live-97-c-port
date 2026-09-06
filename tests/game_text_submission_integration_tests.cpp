#include "game_text_submission_adapter.h"

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("text-submission integration failed at " +
                             std::to_string(line));
}
#define check(value) checkAt((value), __LINE__)

struct Fixture {
  static constexpr U Font = 0x80110000u;
  static constexpr U Records = 0x80120000u;
  static constexpr U Glyphs = 0x80130000u;
  static constexpr U Characters = 0x80140000u;
  static constexpr U Packets = 0x80150000u;
  static constexpr U Heads = 0x80160000u;
  static constexpr U ClearTable = 0x80170000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{0x80000000u, bytes.data(), known.data(),
                             bytes.size()};
  Nba97GameCountdownUiUpdateMachine machine{};
  Nba97GameCountdownUiUpdateProgress parentProgress{};
  Nba97GameTextSubmissionBinding binding{};
  unsigned textChildren = 0;
  unsigned clearChildren = 0;
  unsigned parentChildren = 0;
  unsigned malformedTextMachine = 0;
  unsigned refuseClearInvocation = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x10100000u + i, 15};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[29] = {0x801ff000u, 15};
    machine.registers.gpr[31] = {0x80032b20u, 15};
    machine.hi = {0x11112222u, 7};
    machine.lo = {0x33334444u, 11};
    put(0x800fdba4u, 120, 4);
    put(0x800fe8ccu, 0, 2);
    put(0x800fdb58u, 120, 4);
    put(0x80021d92u, 1, 1);
    put(0x800fea2eu, 0xffffu, 2);
    for (unsigned i = 0; i < 22; ++i)
      put(0x800249e4u + i, 0x20u + i, 1);
    put(0x800249fcu, 0, 1);
    put(0x800b2048u, Font, 4);
    put(Font + 8u, Glyphs, 4);
    put(Font + 0x0cu, Characters, 4);
    put(Font + 0x10u, Records, 4);
    put(Font + 0x14u, Heads, 4);
    put(Font + 0x22u, 1, 2);
    put(Font + 0x26u, 0, 2);
    put(Font + 0x28u, 0x33, 2);
    put(Font + 0x2au, 0, 2);
    for (U offset = 0x2cu; offset <= 0x3eu; offset += 2)
      put(Font + offset, 0xffffu, 2);
    put(Font + 0x40u, 0, 2);
    put(Font + 0x42u, 4, 1);
    put(Font + 0x4au, 1, 1);
    put(Font + 0x52u, 10, 1);
    put(Records + 0x12u, 0xffffu, 2);
    put(Heads + 201u * 2u, 0xffffu, 2);
    put(0x800c55c2u, 0, 1);
    put(0x800c55b8u, ClearTable, 4);
    put(ClearTable + 0x2cu, 0x8009a97cu, 4);
    binding.operation_budget = 1024;
    binding.io = text;
    binding.user = this;
    binding.clear_operation_budget = 64;
    binding.clear_io = clear;
    binding.clear_user = this;
  }

  std::size_t at(U address, unsigned width = 1) const {
    if (address < 0x80000000u || std::uint64_t(address) + width > 0x80200000u)
      throw std::out_of_range("unmapped");
    return address - 0x80000000u;
  }
  void put(U address, U value, unsigned width) {
    const auto offset = at(address, width);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = std::uint8_t(value >> (8u * i));
      known[offset + i] = 1;
    }
  }
  U get(U address, unsigned width) const {
    U value = 0;
    const auto offset = at(address, width);
    for (unsigned i = 0; i < width; ++i)
      value |= U(bytes[offset + i]) << (8u * i);
    return value;
  }

  static int text(void *opaque, const Nba97GameTextMemory *,
                  const Nba97GameTextSubmissionEvent *event,
                  Nba97GameTextSubmissionMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.textChildren;
    if (f.malformedTextMachine && f.textChildren == 1) {
      machine->registers.gpr[15] = {0xabcdef01u, 15};
      if (f.malformedTextMachine == 1)
        machine->registers.gpr[0].word = 1;
      else if (f.malformedTextMachine == 2)
        machine->registers.gpr[10].known_mask = 16;
      else if (f.malformedTextMachine == 3)
        machine->hi.known_mask = 16;
      else
        machine->lo.known_mask = 16;
      return 1;
    }
    if (event->pc == 0x80030e14u) {
      const U sp = machine->registers.gpr[29].word;
      f.put(sp + 0x10u, 8, 2);
      f.put(sp + 0x12u, 4, 2);
      f.put(machine->registers.gpr[6].word, 0, 2);
    } else if (event->pc == 0x80030e20u) {
      machine->registers.gpr[2] = {Packets, 15};
    } else {
      return 0;
    }
    return 1;
  }

  static int clear(void *opaque, const Nba97GameTextMemory *,
                   const Nba97GameClearOrderingTableEvent *event,
                   Nba97GameClearOrderingTableMachine *) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.clearChildren;
    check(event->kind == NBA97_GAME_CLEAR_ORDERING_TABLE_BACKEND &&
          event->pc == 0x800999bcu && event->delay_slot_pc == 0x800999c0u &&
          event->target == 0x8009a97cu && event->argument_count == 2);
    return f.clearChildren != f.refuseClearInvocation;
  }

  static int parent(void *opaque, const Nba97GameTextMemory *,
                    const Nba97GameCountdownUiUpdateEvent *event,
                    Nba97GameCountdownUiUpdateMachine *) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.parentChildren;
    return event->pc == 0x80032ae4u && event->entry == 0x80094540u;
  }

  int run() {
    Nba97GameCountdownUiUpdateContext parentContext{};
    parentContext.memory = {&region, 1};
    parentContext.operation_budget = 256;
    parentContext.machine = machine;
    parentContext.io = parent;
    parentContext.user = this;
    return nba97_game_countdown_ui_update_with_text_submission(
        &parentContext, &binding, &parentProgress);
  }
};

void naturalAndReuse() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.parentProgress.completed &&
        f.binding.result == NBA97_TEXT_COMPLETE && f.binding.invocations == 1 &&
        f.binding.completions == 1 && f.binding.clear_invocations == 2 &&
        f.binding.clear_completions == 2);
  check(f.binding.event.kind == NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_80030D18 &&
        f.binding.event.pc == 0x800329e8u &&
        f.binding.event.delay_slot_pc == 0x800329ecu &&
        f.binding.event.entry == 0x80030d18u &&
        f.binding.event.argument_count == 5 && f.binding.event.invocation == 1);
  check(f.binding.progress.machine.registers.gpr[31].word == 0x800329f0u &&
        f.binding.progress.return_v0.word == Fixture::Records &&
        f.parentProgress.machine.registers.gpr[31].word == 0x80032b20u &&
        f.textChildren == 2 && f.clearChildren == 2 && f.parentChildren == 1);
  check(f.get(Fixture::Records, 4) == 0x000c567cu &&
        f.get(Fixture::Records + 4u, 4) == 0x000c567cu);

  f.put(Fixture::Records + 0x12u, 0xffffu, 2);
  f.put(Fixture::Font + 0x40u, 0, 2);
  f.put(Fixture::Heads + 201u * 2u, 0xffffu, 2);
  f.put(0x800fea2eu, 0xffffu, 2);
  check(f.run() == NBA97_TEXT_COMPLETE);
  check(f.binding.invocations == 2 && f.binding.completions == 2 &&
        f.binding.clear_invocations == 4);
}

void directGuardsAndPrefix() {
  Fixture f;
  Nba97GameCountdownUiUpdateEvent event{
      0x800329e8u,
      0x800329ecu,
      0x80030d18u,
      1,
      1,
      NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_80030D18,
      5};
  auto machine = f.machine;
  machine.registers.gpr[4] = {201, 15};
  machine.registers.gpr[5] = {0x800249fcu, 15};
  machine.registers.gpr[6] = {0x1ecu, 15};
  machine.registers.gpr[7] = {0x14u, 15};
  machine.registers.gpr[31] = {0x800329f0u, 15};
  f.put(machine.registers.gpr[29].word + 0x10u, 2, 4);
  Nba97GameTextMemory memory{&f.region, 1};
  const auto originalMachine = machine;
  auto rejected = [&](Nba97GameCountdownUiUpdateEvent invalidEvent,
                      Nba97GameCountdownUiUpdateMachine invalidMachine) {
    const auto before = invalidMachine;
    check(nba97_game_text_submission_from_countdown_ui_update(
              &f.binding, &memory, &invalidEvent, &invalidMachine) == 0 &&
          f.binding.result == NBA97_TEXT_ARGUMENT &&
          f.binding.invocations == 0);
    for (unsigned i = 0; i < 32; ++i)
      check(invalidMachine.registers.gpr[i].word ==
                before.registers.gpr[i].word &&
            invalidMachine.registers.gpr[i].known_mask ==
                before.registers.gpr[i].known_mask);
    check(invalidMachine.hi.word == before.hi.word &&
          invalidMachine.hi.known_mask == before.hi.known_mask &&
          invalidMachine.lo.word == before.lo.word &&
          invalidMachine.lo.known_mask == before.lo.known_mask);
  };
  auto invalid = event;
  invalid.kind = NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_8003066C;
  rejected(invalid, originalMachine);
  invalid = event;
  invalid.pc = 0;
  rejected(invalid, originalMachine);
  invalid = event;
  invalid.delay_slot_pc = 0;
  rejected(invalid, originalMachine);
  invalid = event;
  invalid.entry = 0;
  rejected(invalid, originalMachine);
  invalid = event;
  invalid.invocation = 2;
  rejected(invalid, originalMachine);
  invalid = event;
  invalid.argument_count = 4;
  rejected(invalid, originalMachine);
  auto invalidMachine = originalMachine;
  invalidMachine.registers.gpr[31].word = 0x800329f4u;
  rejected(event, invalidMachine);
  invalidMachine = originalMachine;
  invalidMachine.registers.gpr[31].known_mask = 14;
  rejected(event, invalidMachine);
  f.put(originalMachine.registers.gpr[29].word + 0x10u, 3, 4);
  rejected(event, originalMachine);
  f.put(originalMachine.registers.gpr[29].word + 0x10u, 2, 4);
  f.known[f.at(originalMachine.registers.gpr[29].word + 0x11u)] = 0;
  rejected(event, originalMachine);

  Fixture limited;
  limited.binding.operation_budget = 1;
  check(limited.run() == NBA97_TEXT_LIMIT &&
        limited.parentProgress.stopped_pc == 0x800329e8u &&
        limited.binding.progress.stopped_pc == 0x80030d24u &&
        limited.parentChildren == 0);
}

void nestedMalformedAndClearRefusals() {
  for (unsigned malformed = 1; malformed <= 4; ++malformed) {
    Fixture f;
    f.malformedTextMachine = malformed;
    check(f.run() == NBA97_TEXT_ARGUMENT && f.binding.invocations == 1 &&
          f.binding.completions == 0 && f.textChildren == 1 &&
          f.clearChildren == 0 && f.parentChildren == 0 &&
          f.parentProgress.stopped_pc == 0x800329e8u &&
          f.binding.progress.stopped_pc == 0x80030e14u);
    const auto &child = f.binding.progress.machine;
    const auto &parent = f.parentProgress.machine;
    for (unsigned i = 0; i < 32; ++i)
      check(parent.registers.gpr[i].word == child.registers.gpr[i].word &&
            parent.registers.gpr[i].known_mask ==
                child.registers.gpr[i].known_mask);
    check(parent.hi.word == child.hi.word &&
          parent.hi.known_mask == child.hi.known_mask &&
          parent.lo.word == child.lo.word &&
          parent.lo.known_mask == child.lo.known_mask &&
          child.registers.gpr[15].word == 0xabcdef01u);
    if (malformed == 1)
      check(child.registers.gpr[0].word == 1);
    else if (malformed == 2)
      check(child.registers.gpr[10].known_mask == 16);
    else if (malformed == 3)
      check(child.hi.known_mask == 16);
    else
      check(child.lo.known_mask == 16);
  }

  for (unsigned refused = 1; refused <= 2; ++refused) {
    Fixture f;
    f.refuseClearInvocation = refused;
    check(f.run() == NBA97_TEXT_IO_REFUSED && f.binding.invocations == 1 &&
          f.binding.completions == 0 && f.clearChildren == refused &&
          f.binding.clear_invocations == refused &&
          f.binding.clear_completions == refused - 1 &&
          f.parentProgress.stopped_pc == 0x800329e8u &&
          f.binding.progress.stopped_pc ==
              (refused == 1 ? 0x800310a8u : 0x800310b4u) &&
          f.binding.progress.stopped_entry == 0x80099960u &&
          f.binding.clear_result[refused - 1] == NBA97_TEXT_IO_REFUSED);
    const auto &child = f.binding.progress.machine;
    const auto &parent = f.parentProgress.machine;
    for (unsigned i = 0; i < 32; ++i)
      check(parent.registers.gpr[i].word == child.registers.gpr[i].word &&
            parent.registers.gpr[i].known_mask ==
                child.registers.gpr[i].known_mask);
    check(parent.hi.word == child.hi.word &&
          parent.hi.known_mask == child.hi.known_mask &&
          parent.lo.word == child.lo.word &&
          parent.lo.known_mask == child.lo.known_mask);
  }
}
} // namespace

int main() {
  try {
    naturalAndReuse();
    directGuardsAndPrefix();
    nestedMalformedAndClearRefusals();
    std::printf("game_text_submission_integration_tests: PASS (%u checks)\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
