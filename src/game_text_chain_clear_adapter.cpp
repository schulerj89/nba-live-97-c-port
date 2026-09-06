#include "game_text_chain_clear_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct ParentRun {
  Nba97GameCountdownUiUpdateIo fallback;
  void *user;
  Nba97GameTextChainClearBinding *binding;
};

bool machineValid(const Nba97GameTextChainClearMachine &machine) {
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

bool assigned(const Nba97GameCountdownUiUpdateEvent *event,
              const Nba97GameCountdownUiUpdateMachine *machine) {
  return (event &&
          (event->kind == NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_8003066C ||
           event->pc == 0x8003295cu || event->delay_slot_pc == 0x80032960u ||
           event->entry == 0x8003066cu)) ||
         (machine && machine->registers.gpr[31].known_mask == 15 &&
          machine->registers.gpr[31].word == 0x80032964u);
}

int dispatch(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97GameCountdownUiUpdateEvent *event,
             Nba97GameCountdownUiUpdateMachine *machine) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (assigned(event, machine))
    return nba97_game_text_chain_clear_from_countdown_ui_update(
        run.binding, memory, event, machine);
  if (!run.fallback)
    return 0;
  return run.fallback(run.user, memory, event, machine);
}
} // namespace

int nba97_game_text_chain_clear_from_countdown_ui_update(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameCountdownUiUpdateEvent *event,
    Nba97GameCountdownUiUpdateMachine *machine) {
  auto *binding = static_cast<Nba97GameTextChainClearBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) ||
      event->kind != NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_8003066C ||
      event->pc != 0x8003295cu || event->delay_slot_pc != 0x80032960u ||
      event->entry != 0x8003066cu || event->invocation != 1 ||
      event->argument_count != 1 ||
      machine->registers.gpr[4].known_mask != 15 ||
      machine->registers.gpr[4].word != 0xc9u ||
      machine->registers.gpr[31].known_mask != 15 ||
      machine->registers.gpr[31].word != 0x80032964u ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->event = *event;
  Nba97GameTextChainClearContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result = nba97_game_text_chain_clear(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

int nba97_game_countdown_ui_update_with_text_chain_clear(
    const Nba97GameCountdownUiUpdateContext *parent,
    Nba97GameTextChainClearBinding *binding,
    Nba97GameCountdownUiUpdateProgress *progress) {
  if (!parent || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  std::memset(&binding->event, 0, sizeof binding->event);
  std::memset(&binding->progress, 0, sizeof binding->progress);
  binding->result = NBA97_TEXT_COMPLETE;
  ParentRun run{parent->io, parent->user, binding};
  Nba97GameCountdownUiUpdateContext context = *parent;
  context.io = dispatch;
  context.user = &run;
  const int result = nba97_game_countdown_ui_update(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED && progress->stopped_pc == 0x8003295cu)
    return binding->result;
  return result;
}
