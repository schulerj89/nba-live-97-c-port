#include "game_rectangle_normalize_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct ParentRun {
  Nba97GameRectangleUploadSubmitIo fallback;
  void *user;
  Nba97GameRectangleNormalizeBinding *binding;
};

bool machineValid(const Nba97GameRectangleNormalizeMachine &machine) {
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

bool assigned(const Nba97GameRectangleUploadSubmitEvent *event,
              const Nba97GameRectangleUploadSubmitMachine *machine) {
  return (event &&
          (event->kind == NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_80094440 ||
           event->pc == 0x80094508u || event->delay_slot_pc == 0x8009450cu ||
           event->entry == 0x80094440u)) ||
         (machine && machine->registers.gpr[31].known_mask == 15 &&
          machine->registers.gpr[31].word == 0x80094510u);
}

int dispatch(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97GameRectangleUploadSubmitEvent *event,
             Nba97GameRectangleUploadSubmitMachine *machine) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (assigned(event, machine))
    return nba97_game_rectangle_normalize_from_rectangle_upload_submit(
        run.binding, memory, event, machine);
  if (!run.fallback)
    return 0;
  return run.fallback(run.user, memory, event, machine);
}
} // namespace

int nba97_game_rectangle_normalize_from_rectangle_upload_submit(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameRectangleUploadSubmitEvent *event,
    Nba97GameRectangleUploadSubmitMachine *machine) {
  auto *binding = static_cast<Nba97GameRectangleNormalizeBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) ||
      event->kind != NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_80094440 ||
      event->pc != 0x80094508u || event->delay_slot_pc != 0x8009450cu ||
      event->entry != 0x80094440u || event->invocation != 1 ||
      event->argument_count != 1 ||
      machine->registers.gpr[31].known_mask != 15 ||
      machine->registers.gpr[31].word != 0x80094510u ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->event = *event;
  Nba97GameRectangleNormalizeContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result = nba97_game_rectangle_normalize(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

int nba97_game_rectangle_upload_submit_with_rectangle_normalize(
    const Nba97GameRectangleUploadSubmitContext *parent,
    Nba97GameRectangleNormalizeBinding *binding,
    Nba97GameRectangleUploadSubmitProgress *progress) {
  if (!parent || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  std::memset(&binding->event, 0, sizeof binding->event);
  std::memset(&binding->progress, 0, sizeof binding->progress);
  binding->result = NBA97_TEXT_COMPLETE;
  ParentRun run{parent->io, parent->user, binding};
  Nba97GameRectangleUploadSubmitContext context = *parent;
  context.io = dispatch;
  context.user = &run;
  const int result = nba97_game_rectangle_upload_submit(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED && progress->stopped_pc == 0x80094508u)
    return binding->result;
  return result;
}
