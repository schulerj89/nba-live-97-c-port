#include "game_image_record_upload_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct ParentRun {
  Nba97GameCountdownUiUpdateIo fallback;
  void *user;
  Nba97GameImageRecordUploadBinding *binding;
};

bool machineValid(const Nba97GameImageRecordUploadMachine &machine) {
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

bool readKnownWord(const Nba97GameTextMemory &memory, std::uint32_t address,
                   std::uint32_t &value) {
  if (address & 3u)
    return false;
  for (std::size_t i = 0; i < memory.count; ++i) {
    const auto &region = memory.region[i];
    const std::uint64_t offset = std::uint64_t(address) - region.base;
    if (address < region.base || offset > region.size ||
        4u > region.size - std::size_t(offset))
      continue;
    value = 0;
    for (unsigned byte = 0; byte < 4; ++byte) {
      if (region.known && region.known[std::size_t(offset) + byte] != 1)
        return false;
      value |= std::uint32_t(region.data[std::size_t(offset) + byte])
               << (byte * 8u);
    }
    return true;
  }
  return false;
}

bool assigned(const Nba97GameCountdownUiUpdateEvent *event,
              const Nba97GameCountdownUiUpdateMachine *machine) {
  return (event &&
          (event->kind == NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_80094540 ||
           event->pc == 0x80032ae4u || event->delay_slot_pc == 0x80032ae8u ||
           event->entry == 0x80094540u)) ||
         (machine && machine->registers.gpr[31].known_mask == 15 &&
          machine->registers.gpr[31].word == 0x80032aecu);
}

int dispatch(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97GameCountdownUiUpdateEvent *event,
             Nba97GameCountdownUiUpdateMachine *machine) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (assigned(event, machine))
    return nba97_game_image_record_upload_from_countdown_ui_update(
        run.binding, memory, event, machine);
  if (!run.fallback)
    return 0;
  return run.fallback(run.user, memory, event, machine);
}
} // namespace

int nba97_game_image_record_upload_from_countdown_ui_update(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameCountdownUiUpdateEvent *event,
    Nba97GameCountdownUiUpdateMachine *machine) {
  auto *binding = static_cast<Nba97GameImageRecordUploadBinding *>(opaque);
  std::uint32_t fifth = 0;
  const bool identifiers = event && machine &&
      event->kind == NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_80094540 &&
      event->pc == 0x80032ae4u && event->delay_slot_pc == 0x80032ae8u &&
      event->entry == 0x80094540u && event->invocation == 1 &&
      event->argument_count == 5 &&
      machine->registers.gpr[31].known_mask == 15 &&
      machine->registers.gpr[31].word == 0x80032aecu &&
      machine->registers.gpr[4].known_mask == 15 &&
      machine->registers.gpr[4].word == 0x800fb5c0u &&
      machine->registers.gpr[5].known_mask == 15 &&
      machine->registers.gpr[5].word == 0 &&
      machine->registers.gpr[6].known_mask == 15 &&
      machine->registers.gpr[6].word == 0 &&
      machine->registers.gpr[7].known_mask == 15 &&
      machine->registers.gpr[7].word == 0x340u &&
      machine->registers.gpr[29].known_mask == 15;
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) || !identifiers ||
      !readKnownWord(*memory, machine->registers.gpr[29].word + 0x10u, fifth) ||
      fifth != 0xf0u ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  ++binding->invocations;
  binding->event = *event;
  Nba97GameImageRecordUploadContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result = nba97_game_image_record_upload(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

int nba97_game_countdown_ui_update_with_image_record_upload(
    const Nba97GameCountdownUiUpdateContext *parent,
    Nba97GameImageRecordUploadBinding *binding,
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
  if (result == NBA97_TEXT_IO_REFUSED && progress->stopped_pc == 0x80032ae4u)
    return binding->result;
  return result;
}
