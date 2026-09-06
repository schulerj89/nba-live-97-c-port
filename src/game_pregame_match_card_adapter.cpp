#include "game_pregame_match_card_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct ParentRun {
  Nba97GamePeriodPresentationFinishIo fallback;
  void *fallbackUser;
  Nba97GamePregameMatchCardBinding *binding;
};

bool machineValid(const Nba97GamePregameMatchCardMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 15 || machine.hi.known_mask > 15 ||
      machine.lo.known_mask > 15)
    return false;
  for (unsigned i = 0; i < 32; ++i)
    if (machine.registers.gpr[i].known_mask > 15)
      return false;
  return true;
}

bool memoryValid(const Nba97GameTextMemory &memory) {
  if (!memory.region && memory.count)
    return false;
  for (std::size_t i = 0; i < memory.count; ++i) {
    const auto &a = memory.region[i];
    if (!a.data || !a.size || a.size > UINT64_C(0x100000000) ||
        std::uint64_t(a.base) + a.size > UINT64_C(0x100000000))
      return false;
    for (std::size_t j = 0; j < i; ++j) {
      const auto &b = memory.region[j];
      if (std::uint64_t(a.base) < std::uint64_t(b.base) + b.size &&
          std::uint64_t(b.base) < std::uint64_t(a.base) + a.size)
        return false;
    }
  }
  return true;
}

bool cardTarget(const Nba97GamePeriodPresentationFinishEvent *event,
                const Nba97GamePeriodPresentationFinishMachine *machine) {
  return (event &&
          (event->kind ==
               NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80044550 ||
           event->pc == 0x8002ddf8u || event->delay_slot_pc == 0x8002ddfcu ||
           event->entry == 0x80044550u)) ||
         (machine && machine->registers.gpr[31].known_mask == 15 &&
          machine->registers.gpr[31].word == 0x8002de00u);
}

int dispatchParent(void *opaque, const Nba97GameTextMemory *memory,
                   const Nba97GamePeriodPresentationFinishEvent *event,
                   Nba97GamePeriodPresentationFinishMachine *machine) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (cardTarget(event, machine))
    return nba97_game_pregame_match_card_from_period_presentation_finish(
        run.binding, memory, event, machine);
  if (!run.fallback)
    return 0;
  return run.fallback(run.fallbackUser, memory, event, machine);
}
} // namespace

int nba97_game_pregame_match_card_from_period_presentation_finish(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GamePeriodPresentationFinishEvent *event,
    Nba97GamePeriodPresentationFinishMachine *machine) {
  auto *binding = static_cast<Nba97GamePregameMatchCardBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) ||
      event->kind != NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80044550 ||
      event->pc != 0x8002ddf8u || event->delay_slot_pc != 0x8002ddfcu ||
      event->entry != 0x80044550u || event->invocation != 1 ||
      event->argument_count != 0 ||
      machine->registers.gpr[31].known_mask != 15 ||
      machine->registers.gpr[31].word != 0x8002de00u ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ++binding->invocations;
  binding->event = *event;
  Nba97GamePregameMatchCardContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result = nba97_game_pregame_match_card(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

int nba97_game_period_presentation_finish_with_pregame_match_card(
    const Nba97GamePeriodPresentationFinishContext *parent,
    Nba97GamePregameMatchCardBinding *binding,
    Nba97GamePeriodPresentationFinishProgress *progress) {
  if (!parent || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  std::memset(&binding->event, 0, sizeof binding->event);
  std::memset(&binding->progress, 0, sizeof binding->progress);
  binding->result = NBA97_TEXT_COMPLETE;
  ParentRun run{parent->io, parent->user, binding};
  Nba97GamePeriodPresentationFinishContext context = *parent;
  context.io = dispatchParent;
  context.user = &run;
  const int result = nba97_game_period_presentation_finish(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED && progress->stopped_pc == 0x8002ddf8u)
    return binding->result;
  return result;
}
