#include "game_rectangle_upload_submit_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct ParentRun {
  Nba97GameImageRecordUploadIo fallback;
  void *user;
  Nba97GameRectangleUploadSubmitBinding *binding;
};

bool machineValid(const Nba97GameRectangleUploadSubmitMachine &machine) {
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

bool assigned(const Nba97GameImageRecordUploadEvent *event,
              const Nba97GameImageRecordUploadMachine *machine) {
  return (event &&
          (event->kind == NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800944F4 ||
           event->pc == 0x8009464cu || event->delay_slot_pc == 0x80094650u ||
           event->entry == 0x800944f4u)) ||
         (machine && machine->registers.gpr[31].known_mask == 15 &&
          machine->registers.gpr[31].word == 0x80094654u);
}

int dispatch(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97GameImageRecordUploadEvent *event,
             Nba97GameImageRecordUploadMachine *machine) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (assigned(event, machine))
    return nba97_game_rectangle_upload_submit_from_image_record_upload(
        run.binding, memory, event, machine);
  if (!run.fallback)
    return 0;
  return run.fallback(run.user, memory, event, machine);
}
} // namespace

int nba97_game_rectangle_upload_submit_from_image_record_upload(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameImageRecordUploadEvent *event,
    Nba97GameImageRecordUploadMachine *machine) {
  auto *binding = static_cast<Nba97GameRectangleUploadSubmitBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) ||
      event->kind != NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800944F4 ||
      event->pc != 0x8009464cu || event->delay_slot_pc != 0x80094650u ||
      event->entry != 0x800944f4u || event->invocation == 0 ||
      event->argument_count != 2 ||
      machine->registers.gpr[31].known_mask != 15 ||
      machine->registers.gpr[31].word != 0x80094654u ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->event = *event;
  Nba97GameRectangleUploadSubmitContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result =
      nba97_game_rectangle_upload_submit(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

int nba97_game_image_record_upload_with_rectangle_upload_submit(
    const Nba97GameImageRecordUploadContext *parent,
    Nba97GameRectangleUploadSubmitBinding *binding,
    Nba97GameImageRecordUploadProgress *progress) {
  if (!parent || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  std::memset(&binding->event, 0, sizeof binding->event);
  std::memset(&binding->progress, 0, sizeof binding->progress);
  binding->result = NBA97_TEXT_COMPLETE;
  ParentRun run{parent->io, parent->user, binding};
  Nba97GameImageRecordUploadContext context = *parent;
  context.io = dispatch;
  context.user = &run;
  const int result = nba97_game_image_record_upload(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED && progress->stopped_pc == 0x8009464cu)
    return binding->result;
  return result;
}
