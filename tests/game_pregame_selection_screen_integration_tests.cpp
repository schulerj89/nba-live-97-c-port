#include "game_pregame_selection_screen_adapter.h"

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
                 "pregame selection integration check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Sp = 0x800ff800u;

bool sameMachine(const Nba97GamePeriodPresentationFinishMachine &a,
                 const Nba97GamePeriodPresentationFinishMachine &b) {
  for (unsigned index = 0u; index != 32u; ++index)
    if (a.registers.gpr[index].word != b.registers.gpr[index].word ||
        a.registers.gpr[index].known_mask != b.registers.gpr[index].known_mask)
      return false;
  return a.hi.word == b.hi.word && a.hi.known_mask == b.hi.known_mask &&
         a.lo.word == b.lo.word && a.lo.known_mask == b.lo.known_mask;
}

struct Composition {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000u, 0u);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x110000u, 1u);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97GamePeriodPresentationFinishContext parent{};
  Nba97GamePeriodPresentationFinishProgress parentProgress{};
  Nba97GamePregameSelectionScreenPresentationBinding child{};
  std::vector<Nba97GamePeriodPresentationFinishEvent> parentCalls;
  std::vector<Nba97GamePregameSelectionScreenEvent> childCalls;
  std::vector<std::uint32_t> inputs{0x80u};
  std::vector<std::uint32_t> times{100u};
  size_t inputIndex{};
  size_t timeIndex{};
  unsigned refuseChild{};
  unsigned invalidHiChild{};
  unsigned invalidGprChild{};
  unsigned invalidZeroChild{};
  unsigned mutateChild{};
  Nba97GamePeriodPresentationFinishEvent natural{};
  Nba97GamePeriodPresentationFinishMachine naturalMachine{};

  explicit Composition(std::uint8_t gate = 0u, std::uint16_t demo = 0u) {
    for (unsigned index = 0u; index != 32u; ++index)
      parent.machine.registers.gpr[index] = {0x51000000u + index, 15u};
    parent.machine.registers.gpr[0] = {0u, 15u};
    parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Sp, 15u};
    parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x8006742cu,
                                                               15u};
    parent.machine.hi = {0x11223344u, 15u};
    parent.machine.lo = {0x55667788u, 15u};
    parent.memory = {&region, 1u};
    parent.operation_budget = 100u;
    parent.io = route;
    parent.user = this;
    nba97_game_pregame_selection_screen_presentation_binding_init(
        &child, 1000u, childIo, this, nullptr, 0u, fallback, this);
    put(0x8001ede8u, 0x81234560u, 4u);
    put(0x8001edecu, demo, 2u);
    put(0x800fdb78u, gate, 1u);
    put(0x800fdb9cu, 0x1234u, 2u);
    put(0x800eb680u, 0xaau, 1u);
    put(0x80109afcu, 0u, 4u);
    put(0x80109ae4u, 0u, 4u);
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
  static int fallback(void *user, const Nba97GameTextMemory *,
                      const Nba97GamePeriodPresentationFinishEvent *event,
                      Nba97GamePeriodPresentationFinishMachine *) {
    auto &fixture = *static_cast<Composition *>(user);
    fixture.parentCalls.push_back(*event);
    return 1;
  }
  static int route(void *user, const Nba97GameTextMemory *memory,
                   const Nba97GamePeriodPresentationFinishEvent *event,
                   Nba97GamePeriodPresentationFinishMachine *machine) {
    auto &fixture = *static_cast<Composition *>(user);
    if (event->entry == 0x80046c2cu) {
      fixture.natural = *event;
      fixture.naturalMachine = *machine;
    }
    return nba97_game_pregame_selection_screen_from_presentation_finish(
        &fixture.child, memory, event, machine);
  }
  static int childIo(void *user, const Nba97GameTextMemory *,
                     const Nba97GamePregameSelectionScreenEvent *event,
                     Nba97GamePregameSelectionScreenMachine *machine) {
    auto &fixture = *static_cast<Composition *>(user);
    fixture.childCalls.push_back(*event);
    unsigned ordinal = static_cast<unsigned>(fixture.childCalls.size());
    if (ordinal == fixture.mutateChild)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0] = {0x13572468u, 9u};
    if (event->entry == 0x800363dcu) {
      fixture.put(machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word,
                  0xfffeu, 2u);
      fixture.put(machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word, 14u,
                  2u);
    } else if (event->entry == 0x80036478u) {
      auto value = fixture.inputIndex < fixture.inputs.size()
                       ? fixture.inputs[fixture.inputIndex++]
                       : fixture.inputs.back();
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {value, 15u};
    } else if (event->entry == 0x800a5810u) {
      auto value = fixture.timeIndex < fixture.times.size()
                       ? fixture.times[fixture.timeIndex++]
                       : fixture.times.back();
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {value, 15u};
    }
    if (ordinal == fixture.invalidHiChild)
      machine->hi.known_mask = 16u;
    if (ordinal == fixture.invalidGprChild)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_T9].known_mask = 16u;
    if (ordinal == fixture.invalidZeroChild)
      machine->registers.gpr[0] = {1u, 15u};
    return ordinal == fixture.refuseChild ? 0 : 1;
  }
  int run() {
    return nba97_game_period_presentation_finish(&parent, &parentProgress);
  }
};

void actual_parent_gate_and_input_paths() {
  Composition skipped(1u);
  check(skipped.run() == NBA97_TEXT_COMPLETE &&
        skipped.parentProgress.completed && skipped.child.invocations == 0u &&
        skipped.childCalls.empty() && skipped.parentCalls.size() == 1u &&
        skipped.parentCalls[0].entry == 0x80044550u);

  Composition normal;
  check(normal.run() == NBA97_TEXT_COMPLETE && normal.parentProgress.completed);
  check(normal.natural.kind ==
            NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80046C2C &&
        normal.natural.pc == 0x8002de14u &&
        normal.natural.delay_slot_pc == 0x8002de18u &&
        normal.natural.entry == 0x80046c2cu &&
        normal.natural.invocation == 1u && normal.natural.argument_count == 0u);
  check(normal.naturalMachine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x8002de1cu &&
        normal.child.invocations == 1u && normal.child.completions == 1u &&
        normal.child.result == NBA97_TEXT_COMPLETE &&
        normal.child.progress.completed);
  check(normal.parentCalls.size() == 1u &&
        normal.parentCalls[0].entry == 0x80044550u &&
        normal.child.fallback_invocations == 1u);
  check(normal.parentProgress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == Sp &&
        normal.parentProgress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                .word == 0x8006742cu &&
        normal.get(0x80109afcu, 4u) == 0u);
}

void timeout_demo_failure_and_reuse() {
  Composition timeout;
  timeout.inputs = {0u, 0x80u};
  timeout.times = {100u, 460u, 500u};
  check(timeout.run() == NBA97_TEXT_COMPLETE &&
        timeout.child.progress.redraws == 2u &&
        timeout.child.progress.polls == 2u);

  Composition demo(0u, 1u);
  demo.inputs = {0x40u};
  check(demo.run() == NBA97_TEXT_COMPLETE && demo.child.progress.demo_skip &&
        demo.get(0x800fdb78u, 1u) == 1u && demo.get(0x8001edecu, 2u) == 99u);

  Composition refused;
  refused.refuseChild = 3u;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.child.result == NBA97_TEXT_IO_REFUSED &&
        refused.child.progress.callbacks_completed == 2u &&
        refused.parentProgress.stopped_pc == 0x8002de14u);

  Composition reused;
  check(reused.run() == NBA97_TEXT_COMPLETE);
  reused.put(0x800fdb78u, 0u, 1u);
  check(reused.run() == NBA97_TEXT_COMPLETE && reused.child.invocations == 2u &&
        reused.child.completions == 2u);
}

void same_machine_prefix_transport() {
  Composition valid;
  valid.mutateChild = 1u;
  check(valid.run() == NBA97_TEXT_COMPLETE &&
        valid.parentProgress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0]
                .word == 0x13572468u &&
        valid.parentProgress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0]
                .known_mask == 9u);

  Composition badHi;
  badHi.mutateChild = 1u;
  badHi.invalidHiChild = 1u;
  check(badHi.run() == NBA97_TEXT_IO_REFUSED &&
        badHi.child.result == NBA97_TEXT_ARGUMENT &&
        badHi.parentProgress.machine.hi.known_mask == 16u &&
        badHi.parentProgress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0]
                .word == 0x13572468u);

  Composition badGpr;
  badGpr.invalidGprChild = 1u;
  check(badGpr.run() == NBA97_TEXT_IO_REFUSED &&
        badGpr.child.result == NBA97_TEXT_ARGUMENT &&
        badGpr.parentProgress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T9]
                .known_mask == 16u);

  Composition badZero;
  badZero.invalidZeroChild = 1u;
  check(badZero.run() == NBA97_TEXT_IO_REFUSED &&
        badZero.child.result == NBA97_TEXT_ARGUMENT &&
        badZero.parentProgress.machine.registers.gpr[0].word == 1u &&
        badZero.parentProgress.machine.registers.gpr[0].known_mask == 15u);
}

void exact_claim_guards() {
  Composition fixture;
  Nba97GamePeriodPresentationFinishEvent event{};
  event.kind = NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80046C2C;
  event.pc = 0x8002de14u;
  event.delay_slot_pc = 0x8002de18u;
  event.entry = 0x80046c2cu;
  event.invocation = 1u;
  auto machine = fixture.parent.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x8002de1cu, 15u};
  const auto original = machine;
  for (unsigned field = 0u; field != 6u; ++field) {
    auto malformed = event;
    machine = original;
    if (field == 0u)
      malformed.kind = NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80044550;
    else if (field == 1u)
      malformed.pc = 0x8002ddf8u;
    else if (field == 2u)
      malformed.delay_slot_pc = 0x8002ddfcu;
    else if (field == 3u)
      malformed.entry = 0x80044550u;
    else if (field == 4u)
      malformed.invocation = 2u;
    else
      malformed.argument_count = 1u;
    check(!nba97_game_pregame_selection_screen_from_presentation_finish(
              &fixture.child, &fixture.parent.memory, &malformed, &machine) &&
          fixture.child.invocations == 0u &&
          fixture.child.fallback_invocations == 0u &&
          sameMachine(machine, original));
  }
  for (unsigned identifier = 0u; identifier != 5u; ++identifier) {
    Nba97GamePeriodPresentationFinishEvent claim{};
    claim.kind = NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80044550;
    claim.pc = 0x8002ddf8u;
    claim.delay_slot_pc = 0x8002ddfcu;
    claim.entry = 0x80044550u;
    claim.invocation = 1u;
    machine = original;
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x8002de00u, 15u};
    if (identifier == 0u)
      claim.kind = NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80046C2C;
    else if (identifier == 1u)
      claim.entry = 0x80046c2cu;
    else if (identifier == 2u)
      claim.pc = 0x8002de14u;
    else if (identifier == 3u)
      claim.delay_slot_pc = 0x8002de18u;
    else
      machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x8002de1cu, 7u};
    const auto before = machine;
    check(!nba97_game_pregame_selection_screen_from_presentation_finish(
              &fixture.child, &fixture.parent.memory, &claim, &machine) &&
          fixture.child.invocations == 0u &&
          fixture.child.fallback_invocations == 0u &&
          sameMachine(machine, before));
  }
  machine = original;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = 0x8002de20u;
  check(!nba97_game_pregame_selection_screen_from_presentation_finish(
            &fixture.child, &fixture.parent.memory, &event, &machine) &&
        fixture.child.invocations == 0u);

  machine = original;
  machine.hi.known_mask = 16u;
  check(!nba97_game_pregame_selection_screen_from_presentation_finish(
            &fixture.child, &fixture.parent.memory, &event, &machine) &&
        fixture.child.invocations == 0u);

  Nba97GamePeriodPresentationFinishEvent unrelated{};
  unrelated.kind = NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80044550;
  unrelated.pc = 0x8002ddf8u;
  unrelated.delay_slot_pc = 0x8002ddfcu;
  unrelated.entry = 0x80044550u;
  unrelated.invocation = 1u;
  machine = original;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x8002de00u, 15u};
  check(nba97_game_pregame_selection_screen_from_presentation_finish(
            &fixture.child, &fixture.parent.memory, &unrelated, &machine) &&
        fixture.child.fallback_invocations == 1u &&
        fixture.parentCalls.size() == 1u);
}
} // namespace

int main() {
  actual_parent_gate_and_input_paths();
  timeout_demo_failure_and_reuse();
  same_machine_prefix_transport();
  exact_claim_guards();
  std::printf("game pregame selection integration: %u checks passed\n", checks);
}
