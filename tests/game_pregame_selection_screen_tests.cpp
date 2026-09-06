#include "recovered/game_pregame_selection_screen.h"

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
    std::fprintf(stderr, "pregame selection check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Sp = 0x800ff000u;

bool sameWord(Nba97GamePregameSelectionScreenWord a,
              Nba97GamePregameSelectionScreenWord b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

bool sameMachine(const Nba97GamePregameSelectionScreenMachine &a,
                 const Nba97GamePregameSelectionScreenMachine &b) {
  for (unsigned index = 0u; index != 32u; ++index)
    if (!sameWord(a.registers.gpr[index], b.registers.gpr[index]))
      return false;
  return sameWord(a.hi, b.hi) && sameWord(a.lo, b.lo);
}

struct Call {
  Nba97GamePregameSelectionScreenEvent event{};
  Nba97GamePregameSelectionScreenMachine machine{};
};

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000u, 0u);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x110000u, 1u);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97GamePregameSelectionScreenContext context{};
  Nba97GamePregameSelectionScreenProgress progress{};
  std::array<Nba97GamePregameSelectionScreenAccess, 256> journal{};
  std::vector<Call> calls;
  std::vector<std::uint32_t> inputs{0x80u};
  std::vector<std::uint32_t> times{100u};
  size_t inputIndex{};
  size_t timeIndex{};
  unsigned refuseOrdinal{};
  std::uint32_t refuseEntry{};
  unsigned badMachineOrdinal{};
  bool overrideSelections{};
  std::uint32_t initialS1{};
  std::uint32_t initialS2{12u};
  std::uint8_t initialS2Mask{15u};
  bool mutateMenuRestore{};
  bool relocateFirst{};
  std::uint32_t relocatedSp{0x800fe000u};
  std::uint32_t alternateController{0x80022000u};
  std::uint16_t local10{0xfffeu};
  std::uint16_t local12{14u};
  std::uint16_t menuObserved{};

  explicit Fixture(std::uint16_t demo = 0u) {
    for (unsigned index = 0u; index != 32u; ++index)
      context.machine.registers.gpr[index] = {
          0x21000000u + index * 0x010203u,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Sp, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x8002de1cu,
                                                                15u};
    context.machine.hi = {0x12345678u, 5u};
    context.machine.lo = {0x9abcdef0u, 10u};
    context.memory = {&region, 1u};
    context.operation_budget = 1000u;
    context.io = io;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    put(0x8001edecu, demo, 2u);
    put(0x800fdb78u, 0u, 1u);
    put(0x800fdb9cu, 0x1234u, 2u);
    put(alternateController, 0x9999u, 2u);
  }

  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }
  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    auto at = offset(address);
    for (unsigned byte = 0u; byte != width; ++byte) {
      bytes[at + byte] = static_cast<std::uint8_t>(value >> (8u * byte));
      known[at + byte] = 1u;
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    auto at = offset(address);
    std::uint32_t value = 0u;
    for (unsigned byte = 0u; byte != width; ++byte)
      value |= std::uint32_t(bytes[at + byte]) << (8u * byte);
    return value;
  }
  void writeHalf(std::uint32_t address, std::uint16_t value) {
    put(address, value, 2u);
  }
  static int io(void *user, const Nba97GameTextMemory *,
                const Nba97GamePregameSelectionScreenEvent *event,
                Nba97GamePregameSelectionScreenMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(user);
    fixture.calls.push_back({*event, *machine});
    const unsigned ordinal = static_cast<unsigned>(fixture.calls.size());
    if (fixture.relocateFirst && event->pc == 0x80046c54u) {
      const std::uint32_t oldSp =
          machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word;
      for (unsigned byte = 0x18u; byte != 0x40u; ++byte) {
        fixture.bytes[fixture.offset(fixture.relocatedSp + byte)] =
            fixture.bytes[fixture.offset(oldSp + byte)];
        fixture.known[fixture.offset(fixture.relocatedSp + byte)] =
            fixture.known[fixture.offset(oldSp + byte)];
      }
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {fixture.relocatedSp,
                                                           15u};
      machine->hi = {0xabcdef01u, 3u};
      machine->lo = {0x10293847u, 12u};
    }
    if (event->entry == 0x800363dcu) {
      fixture.writeHalf(machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word,
                        fixture.local10);
      fixture.writeHalf(machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word,
                        fixture.local12);
      if (fixture.overrideSelections) {
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1] = {
            fixture.initialS1, 15u};
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2] = {
            fixture.initialS2, fixture.initialS2Mask};
      }
    } else if (event->entry == 0x80036478u) {
      const auto value = fixture.inputIndex < fixture.inputs.size()
                             ? fixture.inputs[fixture.inputIndex++]
                             : fixture.inputs.back();
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {value, 15u};
    } else if (event->entry == 0x800a5810u) {
      const auto value = fixture.timeIndex < fixture.times.size()
                             ? fixture.times[fixture.timeIndex++]
                             : fixture.times.back();
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {value, 15u};
    } else if (event->entry == 0x80036be4u) {
      fixture.menuObserved =
          static_cast<std::uint16_t>(fixture.get(0x800fdb9cu, 2u));
      if (fixture.mutateMenuRestore) {
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {0x5678u, 15u};
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 6] = {
            fixture.alternateController, 15u};
      }
    }
    if (ordinal == fixture.badMachineOrdinal)
      machine->hi.known_mask = 16u;
    return ordinal == fixture.refuseOrdinal ||
                   event->entry == fixture.refuseEntry
               ? 0
               : 1;
  }
  int run() { return nba97_game_pregame_selection_screen(&context, &progress); }
  const Call *findPc(std::uint32_t pc, unsigned occurrence = 1u) const {
    for (const auto &call : calls)
      if (call.event.pc == pc && --occurrence == 0u)
        return &call;
    return nullptr;
  }
};

void normal_call_order_and_machine() {
  Fixture fixture;
  auto incoming = fixture.context.machine;
  check(fixture.run() == NBA97_TEXT_COMPLETE && fixture.progress.completed);
  const std::array<std::uint32_t, 14> expected = {
      0x80081358u, 0x800363dcu, 0x8003081cu, 0x80046738u, 0x80049018u,
      0x800a5810u, 0x80083eecu, 0x80036478u, 0x80049018u, 0x80029258u,
      0x80036600u, 0x8003081cu, 0x80049018u, 0x8008048cu};
  check(fixture.calls.size() == expected.size());
  for (size_t index = 0u; index != expected.size(); ++index) {
    check(fixture.calls[index].event.entry == expected[index] &&
          fixture.calls[index].event.delay_slot_pc ==
              fixture.calls[index].event.pc + 4u &&
          fixture.calls[index]
                  .machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                  .word == fixture.calls[index].event.pc + 8u);
  }
  check(fixture.findPc(0x80046c70u)
                ->machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                .word == Sp - 0x30u &&
        fixture.findPc(0x80046c70u)
                ->machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
                .word == Sp - 0x2eu);
  check(fixture.findPc(0x80046cd8u)
                ->machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                .word == 0xfffffffeu &&
        fixture.findPc(0x80046cd8u)
                ->machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
                .word == Sp - 0x2eu);
  check(fixture.findPc(0x80046f08u)
                ->machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                .word == 97u &&
        fixture.findPc(0x80046f14u)
                ->machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                .word == 14u);
  check(fixture.progress.redraws == 1u && fixture.progress.polls == 1u &&
        fixture.progress.frame_stack_pointer == Sp - 0x40u);
  check(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
          Sp &&
      fixture.progress.restored_return_address.word == 0x8002de1cu);
  for (unsigned saved = 0u; saved != 8u; ++saved)
    check(sameWord(fixture.progress.restored_s[saved],
                   incoming.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + saved]));
  check(sameWord(fixture.progress.restored_s[8],
                 incoming.registers.gpr[NBA97_MATCH_INITIALIZE_FP]));
  check(!sameWord(incoming.registers.gpr[NBA97_MATCH_INITIALIZE_T8],
                  incoming.registers.gpr[NBA97_MATCH_INITIALIZE_FP]) &&
        sameWord(
            fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T8],
            incoming.registers.gpr[NBA97_MATCH_INITIALIZE_T8]));
  check(sameWord(fixture.progress.machine.hi, incoming.hi) &&
        sameWord(fixture.progress.machine.lo, incoming.lo));
  const std::array<unsigned, 13> untouched = {0u,  6u,  7u,  8u,  9u,  10u, 11u,
                                              12u, 13u, 14u, 15u, 28u, 30u};
  for (unsigned index : untouched)
    check(sameWord(fixture.progress.machine.registers.gpr[index],
                   incoming.registers.gpr[index]));
}

void direct_masks_and_limits() {
  struct Case {
    std::uint32_t mask;
    std::uint32_t s1;
    std::uint32_t s2;
    std::uint32_t expectedS1AtSecondRedraw;
    std::uint32_t expectedS2AtSecondRedraw;
    bool redraw;
  };
  const std::array<Case, 10> cases = {{{4u, 0u, 12u, 1u, 13u, true},
                                       {8u, 0u, 12u, 0u, 12u, false},
                                       {0x200u, 0u, 12u, 0u, 12u, false},
                                       {0x200u, 0u, 13u, 0u, 12u, true},
                                       {0x400u, 0u, 12u, 0u, 13u, true},
                                       {0x400u, 0u, 16u, 0u, 16u, false},
                                       {0x1000u, 0u, 12u, 0u, 12u, false},
                                       {0x1000u, 1u, 13u, 0u, 13u, true},
                                       {0x2000u, 0u, 12u, 1u, 12u, true},
                                       {0x2000u, 4u, 16u, 4u, 16u, false}}};
  for (const auto &test : cases) {
    Fixture fixture;
    fixture.inputs = {test.mask, 0x80u};
    fixture.overrideSelections = true;
    fixture.initialS1 = test.s1;
    fixture.initialS2 = test.s2;
    check(fixture.run() == NBA97_TEXT_COMPLETE && fixture.progress.completed);
    check(fixture.progress.redraws == (test.redraw ? 2u : 1u));
    if (test.redraw) {
      const auto *second = fixture.findPc(0x80046c7cu, 2u);
      check(second != nullptr &&
            second->machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 1].word ==
                test.expectedS1AtSecondRedraw &&
            second->machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + 2].word ==
                test.expectedS2AtSecondRedraw);
    }
  }

  Fixture increment;
  increment.inputs = {0x400u, 0x80u};
  check(increment.run() == NBA97_TEXT_COMPLETE &&
        increment.findPc(0x80046e80u) != nullptr &&
        increment.findPc(0x80046e80u)
                ->machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 0u &&
        increment.findPc(0x80046e80u)
                ->machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 15u);

  Fixture unknownBound;
  unknownBound.inputs = {0x400u};
  unknownBound.overrideSelections = true;
  unknownBound.initialS2 = 16u;
  unknownBound.initialS2Mask = 0u;
  check(unknownBound.run() == NBA97_TEXT_UNKNOWN &&
        unknownBound.progress.stopped_pc == 0x80046e78u &&
        unknownBound.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 0u &&
        unknownBound.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 15u);

  Fixture unrelated;
  unrelated.inputs = {0x40u, 0x80u};
  check(unrelated.run() == NBA97_TEXT_COMPLETE &&
        unrelated.progress.input_latched && unrelated.progress.polls == 2u &&
        unrelated.progress.redraws == 1u);
}

void menu_alias_and_demo_skip() {
  Fixture menu;
  menu.inputs = {0x20u, 0x80u};
  menu.mutateMenuRestore = true;
  check(menu.run() == NBA97_TEXT_COMPLETE && menu.menuObserved == 14u);
  check(menu.get(0x800fdb9cu, 2u) == 14u &&
        menu.get(menu.alternateController, 2u) == 0x5678u);
  const auto *opened = menu.findPc(0x80046d84u);
  check(opened != nullptr &&
        opened->machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            0x800b2fd4u &&
        opened->machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word == 1u);

  Fixture demo(1u);
  demo.inputs = {0x40u};
  check(demo.run() == NBA97_TEXT_COMPLETE && demo.progress.demo_skip &&
        demo.get(0x800fdb78u, 1u) == 1u && demo.get(0x8001edecu, 2u) == 99u &&
        demo.progress.polls == 1u && demo.findPc(0x80046ed8u) == nullptr);
}

void callback_live_frame_relocation() {
  Fixture fixture;
  fixture.relocateFirst = true;
  auto incoming = fixture.context.machine;
  check(
      fixture.run() == NBA97_TEXT_COMPLETE && fixture.progress.completed &&
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
          fixture.relocatedSp + 0x40u &&
      fixture.findPc(0x80046c70u)
              ->machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
              .word == fixture.relocatedSp + 0x10u);
  for (unsigned saved = 0u; saved != 8u; ++saved)
    check(sameWord(fixture.progress.restored_s[saved],
                   incoming.registers.gpr[NBA97_MATCH_INITIALIZE_S0 + saved]));
  check(sameWord(fixture.progress.restored_s[8],
                 incoming.registers.gpr[NBA97_MATCH_INITIALIZE_FP]) &&
        fixture.progress.machine.hi.word == 0xabcdef01u &&
        fixture.progress.machine.hi.known_mask == 3u &&
        fixture.progress.machine.lo.word == 0x10293847u &&
        fixture.progress.machine.lo.known_mask == 12u);
}

void timers_and_wrapping() {
  Fixture threshold;
  threshold.inputs = {0u, 0x80u};
  threshold.times = {100u, 460u, 500u};
  check(threshold.run() == NBA97_TEXT_COMPLETE &&
        threshold.progress.redraws == 2u &&
        threshold.findPc(0x80046e0cu) != nullptr);

  Fixture end;
  end.inputs = {0u};
  end.times = {100u, 460u};
  end.overrideSelections = true;
  end.initialS1 = 4u;
  end.initialS2 = 16u;
  check(end.run() == NBA97_TEXT_COMPLETE && end.progress.redraws == 1u &&
        end.progress.polls == 1u);

  const std::array<std::array<std::uint32_t, 2>, 3> clocks = {
      {{{100u, 100u}}, {{100u, 99u}}, {{0xfffffff0u, 0x10u}}}};
  for (const auto &clock : clocks) {
    Fixture fixture;
    fixture.inputs = {0u, 0x80u};
    fixture.times = {clock[0], clock[1], clock[1]};
    check(fixture.run() == NBA97_TEXT_COMPLETE &&
          fixture.progress.redraws == 1u && fixture.progress.polls == 2u);
  }
}

void failures_budgets_unknowns_and_return() {
  Fixture complete;
  check(complete.run() == NBA97_TEXT_COMPLETE);
  Fixture repeatComplete;
  check(
      repeatComplete.run() == NBA97_TEXT_COMPLETE &&
      complete.bytes == repeatComplete.bytes &&
      complete.known == repeatComplete.known &&
      sameMachine(complete.progress.machine, repeatComplete.progress.machine));
  const size_t operations = complete.progress.operations;
  for (size_t budget = 0u; budget != operations; ++budget) {
    Fixture first;
    Fixture repeat;
    first.context.operation_budget = budget;
    repeat.context.operation_budget = budget;
    check(first.run() == NBA97_TEXT_LIMIT && repeat.run() == NBA97_TEXT_LIMIT);
    check(first.progress.operations == budget &&
          first.progress.stopped_pc == repeat.progress.stopped_pc &&
          first.progress.stopped_address == repeat.progress.stopped_address &&
          first.progress.stopped_entry == repeat.progress.stopped_entry &&
          first.bytes == repeat.bytes && first.known == repeat.known &&
          sameMachine(first.progress.machine, repeat.progress.machine));
  }
  for (unsigned ordinal = 1u; ordinal <= complete.calls.size(); ++ordinal) {
    Fixture refused;
    refused.refuseOrdinal = ordinal;
    check(refused.run() == NBA97_TEXT_IO_REFUSED &&
          refused.calls.size() == ordinal &&
          refused.progress.callbacks_completed == ordinal - 1u);
  }
  const std::array<std::uint32_t, 13> children = {
      0x80081358u, 0x800363dcu, 0x8003081cu, 0x80035678u, 0x80046738u,
      0x80049018u, 0x800a5810u, 0x80083eecu, 0x80036478u, 0x80029258u,
      0x80036be4u, 0x80036600u, 0x8008048cu};
  for (std::uint32_t child : children) {
    Fixture refused(child == 0x80035678u ? 1u : 0u);
    if (child == 0x80036be4u)
      refused.inputs = {0x20u};
    refused.refuseEntry = child;
    check(refused.run() == NBA97_TEXT_IO_REFUSED &&
          refused.progress.stopped_entry == child);
  }
  Fixture bad;
  bad.badMachineOrdinal = 1u;
  check(bad.run() == NBA97_TEXT_ARGUMENT &&
        bad.progress.machine.hi.known_mask == 16u);

  Fixture pollRunaway;
  pollRunaway.inputs = {0u};
  pollRunaway.times = {100u};
  pollRunaway.context.operation_budget = 80u;
  check(pollRunaway.run() == NBA97_TEXT_LIMIT &&
        pollRunaway.progress.operations == 80u &&
        pollRunaway.progress.polls > 2u);
  Fixture redrawRunaway;
  redrawRunaway.inputs = {4u};
  redrawRunaway.context.operation_budget = 80u;
  check(redrawRunaway.run() == NBA97_TEXT_LIMIT &&
        redrawRunaway.progress.operations == 80u &&
        redrawRunaway.progress.redraws > 2u);

  Fixture unknownDemo;
  unknownDemo.known[unknownDemo.offset(0x8001edecu)] = 0u;
  unknownDemo.known[unknownDemo.offset(0x8001edecu) + 1u] = 0u;
  check(unknownDemo.run() == NBA97_TEXT_UNKNOWN &&
        unknownDemo.progress.stopped_pc == 0x80046c94u &&
        unknownDemo.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                .word == 0u);

  Fixture malformed;
  malformed.known[malformed.offset(Sp - 0x30u) + 1u] = 2u;
  /* 363DC overwrites the local before it is read, so put malformed knownness
   * on the fixed demo halfword's later byte to prove atomic LHU publication. */
  malformed.known[malformed.offset(0x8001edecu) + 1u] = 2u;
  check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x80046c8cu &&
        malformed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 0u);

  for (unsigned mask = 0u; mask != 15u; ++mask) {
    Fixture unknownRa;
    unknownRa.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = static_cast<std::uint8_t>(mask);
    check(unknownRa.run() == NBA97_TEXT_UNKNOWN &&
          unknownRa.progress.stopped_pc == 0x80046f60u &&
          unknownRa.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                  .word == Sp);
  }
  Fixture misaligned;
  misaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word =
      0x8002de1eu;
  check(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.stopped_pc == 0x80046f60u &&
        misaligned.progress.stopped_address == 0x8002de1eu);

  Fixture misalignedStack;
  misalignedStack.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
      .word = Sp + 2u;
  check(misalignedStack.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misalignedStack.progress.stopped_pc == 0x80046c30u);

  Fixture unmapped;
  unmapped.region.size = 0x100u;
  check(
      unmapped.run() == NBA97_TEXT_RESOURCE &&
      unmapped.progress.stopped_pc == 0x80046c30u &&
      unmapped.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
          Sp - 0x40u);

  std::array<std::uint8_t, 64> stackBytes{};
  Nba97GameTextRegion stackRegion{Sp - 0x40u, stackBytes.data(), nullptr,
                                  stackBytes.size()};
  Nba97GamePregameSelectionScreenContext partial{};
  Nba97GamePregameSelectionScreenProgress partialProgress{};
  for (unsigned index = 0u; index != 32u; ++index)
    partial.machine.registers.gpr[index] = {index, 15u};
  partial.machine.registers.gpr[0] = {0u, 15u};
  partial.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Sp, 15u};
  partial.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x8002de1cu, 7u};
  partial.memory = {&stackRegion, 1u};
  partial.operation_budget = 10u;
  auto originalBytes = stackBytes;
  check(nba97_game_pregame_selection_screen(&partial, &partialProgress) ==
            NBA97_TEXT_ARGUMENT &&
        partialProgress.stopped_pc == 0x80046c30u &&
        stackBytes == originalBytes);
}
} // namespace

int main() {
  normal_call_order_and_machine();
  direct_masks_and_limits();
  menu_alias_and_demo_skip();
  callback_live_frame_relocation();
  timers_and_wrapping();
  failures_budgets_unknowns_and_return();
  std::printf("game pregame selection screen: %u checks passed\n", checks);
}
