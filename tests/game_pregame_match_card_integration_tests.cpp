#include "game_pregame_match_card_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr,
                 "pregame match card integration check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

struct Fixture {
  static constexpr std::uint32_t Ram = 0x80000000u;
  static constexpr std::uint32_t Sp = 0x801ff800u;
  static constexpr std::uint32_t Font = 0x80030000u;
  static constexpr std::uint32_t TeamA = 0x80040000u;
  static constexpr std::uint32_t TeamB = 0x80041000u;

  std::vector<std::uint8_t> data = std::vector<std::uint8_t>(0x200000, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(data.size(), 1);
  Nba97GameTextRegion region{Ram, data.data(), known.data(), data.size()};
  std::vector<Nba97GamePeriodPresentationFinishAccess> parentJournal =
      std::vector<Nba97GamePeriodPresentationFinishAccess>(32);
  std::vector<Nba97GamePregameMatchCardAccess> childJournal =
      std::vector<Nba97GamePregameMatchCardAccess>(512);
  Nba97GamePeriodPresentationFinishContext parent{};
  Nba97GamePeriodPresentationFinishProgress progress{};
  Nba97GamePregameMatchCardBinding binding{};
  std::vector<Nba97GamePregameMatchCardEvent> childCalls;
  unsigned fallbackCalls = 0;
  unsigned refuseChildKind = 0;
  std::uint32_t input = 0x180;
  std::uint32_t firstTime = 100;
  std::uint32_t secondTime = 110;
  std::uint32_t ready = 1;

  explicit Fixture(bool demo = false) {
    parent.memory = {&region, 1};
    parent.operation_budget = 32;
    parent.io = fallback;
    parent.user = this;
    parent.access_journal = parentJournal.data();
    parent.access_journal_capacity = parentJournal.size();
    for (unsigned i = 0; i < 32; ++i)
      parent.machine.registers.gpr[i] = {0x61000000u + i * 0x101u, 15};
    parent.machine.registers.gpr[0] = {0, 15};
    parent.machine.registers.gpr[29] = {Sp, 15};
    parent.machine.registers.gpr[31] = {0x81234568u, 15};
    parent.machine.hi = {0x11112222u, 5};
    parent.machine.lo = {0x33334444u, 10};

    binding.operation_budget = 1000;
    binding.io = child;
    binding.user = this;
    binding.access_journal = childJournal.data();
    binding.access_journal_capacity = childJournal.size();

    put(0x8001ede8u, 0x76543210u, 4);
    put(0x800eb680u, 1, 1);
    put(0x800fdb78u, 0, 1);
    put(0x80109afcu, 0, 4);
    put(0x80109ae4u, 0, 4);
    put(0x800b2048u, Font, 4);
    put(0x8001ef24u, TeamA, 4);
    put(0x8001ee60u, TeamB, 4);
    put(TeamA + 0x40u, 0x80060000u, 4);
    put(TeamB + 0x40u, 0x80061000u, 4);
    put(TeamB + 0x4cu, 0x80062000u, 4);
    put(0x8001ec94u, 0, 4);
    put(0x8001edecu, demo ? 1 : 0, 2);
  }

  std::size_t at(std::uint32_t address) const { return address - Ram; }
  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i) {
      data[at(address) + i] = static_cast<std::uint8_t>(value >> (i * 8u));
      known[at(address) + i] = 1;
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(data[at(address) + i]) << (i * 8u);
    return value;
  }
  static int child(void *opaque, const Nba97GameTextMemory *,
                   const Nba97GamePregameMatchCardEvent *event,
                   Nba97GamePregameMatchCardMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.childCalls.push_back(*event);
    if (f.refuseChildKind == event->kind)
      return 0;
    auto &v0 = machine->registers.gpr[2];
    switch (event->kind) {
    case NBA97_GAME_PREGAME_MATCH_CARD_CALL_800A5810:
      v0 = {event->invocation == 1 ? f.firstTime : f.secondTime, 15};
      break;
    case NBA97_GAME_PREGAME_MATCH_CARD_CALL_800363DC:
      f.put(machine->registers.gpr[4].word, 7, 2);
      f.put(machine->registers.gpr[5].word, 9, 2);
      break;
    case NBA97_GAME_PREGAME_MATCH_CARD_CALL_80036478:
      v0 = {f.input, 15};
      break;
    case NBA97_GAME_PREGAME_MATCH_CARD_CALL_80088D0C:
      v0 = {f.ready, 15};
      break;
    case NBA97_GAME_PREGAME_MATCH_CARD_CALL_8008048C:
      v0 = {0xcafebabeu, 6};
      break;
    default:
      break;
    }
    return 1;
  }
  static int fallback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GamePeriodPresentationFinishEvent *event,
                      Nba97GamePeriodPresentationFinishMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.fallbackCalls;
    if (!event ||
        event->kind != NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80046C2C ||
        event->pc != 0x8002de14u || event->entry != 0x80046c2cu)
      return 0;
    machine->registers.gpr[2] = {0x12345678u, 9};
    return 1;
  }
  int run() {
    return nba97_game_period_presentation_finish_with_pregame_match_card(
        &parent, &binding, &progress);
  }
};

void actualParentSkipAndDemoPaths() {
  Fixture normal;
  check(normal.run() == NBA97_TEXT_COMPLETE && normal.progress.completed);
  check(normal.binding.invocations == 1 && normal.binding.completions == 1 &&
        normal.binding.progress.completed);
  check(normal.binding.event.pc == 0x8002ddf8u &&
        normal.binding.event.delay_slot_pc == 0x8002ddfcu &&
        normal.binding.event.entry == 0x80044550u &&
        normal.binding.event.argument_count == 0);
  check(normal.fallbackCalls == 1 && normal.progress.optional_child_called);
  check(normal.get(0x800eb680u, 1) == 0 && normal.get(0x80109afcu, 4) == 0 &&
        normal.get(0x80109ae4u, 4) == 0x76543210u);
  check(normal.progress.machine.registers.gpr[29].word == Fixture::Sp &&
        normal.progress.machine.registers.gpr[31].word == 0x81234568u);

  Fixture demo(true);
  check(demo.run() == NBA97_TEXT_COMPLETE && demo.progress.completed);
  check(demo.get(0x800fdb78u, 1) == 1 && demo.get(0x8001edecu, 2) == 99);
  check(demo.fallbackCalls == 0 && !demo.progress.optional_child_called);
  check(demo.binding.progress.exited_for_input &&
        demo.binding.progress
                .call_count[NBA97_GAME_PREGAME_MATCH_CARD_CALL_80035678] == 1);
}

void timerControlAndFailurePrefixes() {
  Fixture timeout;
  timeout.input = 1;
  timeout.firstTime = 0;
  timeout.secondTime = 3600;
  timeout.ready = 0;
  check(timeout.run() == NBA97_TEXT_COMPLETE);
  check(timeout.binding.progress.exited_for_timeout &&
        timeout.binding.progress.polling_iterations == 2);

  Fixture refused;
  refused.refuseChildKind = NBA97_GAME_PREGAME_MATCH_CARD_CALL_80036478;
  check(refused.run() == NBA97_TEXT_IO_REFUSED);
  check(refused.binding.result == NBA97_TEXT_IO_REFUSED &&
        refused.binding.progress.stopped_pc == 0x800448a4u &&
        refused.progress.stopped_pc == 0x8002ddf8u);

  Fixture limited;
  limited.binding.operation_budget = 0;
  check(limited.run() == NBA97_TEXT_LIMIT &&
        limited.binding.progress.stopped_pc == 0x80044554u &&
        limited.progress.stopped_pc == 0x8002ddf8u);
}

void exactGuardsAndReuse() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE);
  f.put(0x800eb680u, 1, 1);
  f.put(0x800fdb78u, 0, 1);
  check(f.run() == NBA97_TEXT_COMPLETE && f.binding.invocations == 2 &&
        f.binding.completions == 2);

  Nba97GamePeriodPresentationFinishEvent event{};
  event.pc = 0x8002ddf8u;
  event.delay_slot_pc = 0x8002ddfcu;
  event.entry = 0x80044550u;
  event.invocation = 1;
  event.kind = NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80044550;
  auto machine = f.parent.machine;
  machine.registers.gpr[31] = {0x8002de00u, 15};
  for (unsigned field = 0; field < 7; ++field) {
    auto bad = event;
    auto badMachine = machine;
    switch (field) {
    case 0:
      bad.pc ^= 4;
      break;
    case 1:
      bad.delay_slot_pc ^= 4;
      break;
    case 2:
      bad.entry ^= 4;
      break;
    case 3:
      bad.kind = NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80046C2C;
      break;
    case 4:
      bad.argument_count = 1;
      break;
    case 5:
      bad.invocation = 2;
      break;
    default:
      badMachine.registers.gpr[31].word ^= 4;
      break;
    }
    const auto before = badMachine;
    check(nba97_game_pregame_match_card_from_period_presentation_finish(
              &f.binding, &f.parent.memory, &bad, &badMachine) == 0);
    for (unsigned i = 0; i < 32; ++i)
      check(badMachine.registers.gpr[i].word == before.registers.gpr[i].word &&
            badMachine.registers.gpr[i].known_mask ==
                before.registers.gpr[i].known_mask);
  }
}
} // namespace

int main() {
  actualParentSkipAndDemoPaths();
  timerControlAndFailurePrefixes();
  exactGuardsAndReuse();
  std::printf("game_pregame_match_card_integration_tests: PASS (%u checks)\n",
              checks);
  return EXIT_SUCCESS;
}
