#include "game_match_buffer_compress_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct ParentRun {
  Nba97GameMatchBufferRecordIo fallback;
  void *fallback_user;
  Nba97GameMatchBufferCompressBinding *binding;
};

bool machineValid(const Nba97GameMatchBufferCompressMachine &machine) {
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

bool compressTarget(const Nba97GameMatchBufferRecordEvent *event,
                    const Nba97GameMatchBufferRecordMachine *machine) {
  return (event &&
          (event->kind == NBA97_GAME_MATCH_BUFFER_RECORD_COMPRESS_800767FC ||
           event->entry == 0x800767fcu || event->pc == 0x80076e58u ||
           event->delay_slot_pc == 0x80076e5cu)) ||
         (machine && machine->registers.gpr[31].known_mask == 15 &&
          machine->registers.gpr[31].word == 0x80076e60u);
}

int dispatchParent(void *opaque, const Nba97GameTextMemory *memory,
                   const Nba97GameMatchBufferRecordEvent *event,
                   Nba97GameMatchBufferRecordMachine *machine) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (compressTarget(event, machine))
    return nba97_game_match_buffer_compress_from_record(run.binding, memory,
                                                        event, machine);
  if (!run.fallback)
    return 0;
  return run.fallback(run.fallback_user, memory, event, machine);
}
} // namespace

int nba97_game_match_buffer_compress_from_record(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameMatchBufferRecordEvent *event,
    Nba97GameMatchBufferRecordMachine *machine) {
  auto *binding = static_cast<Nba97GameMatchBufferCompressBinding *>(opaque);
  if (!binding || !memory || !event || !machine || !memoryValid(*memory) ||
      !machineValid(*machine) ||
      event->kind != NBA97_GAME_MATCH_BUFFER_RECORD_COMPRESS_800767FC ||
      event->pc != 0x80076e58u || event->delay_slot_pc != 0x80076e5cu ||
      event->entry != 0x800767fcu || event->invocation != 1 ||
      event->argument_count != 4 ||
      machine->registers.gpr[31].known_mask != 15 ||
      machine->registers.gpr[31].word != 0x80076e60u ||
      machine->registers.gpr[7].known_mask != 15 ||
      machine->registers.gpr[7].word != 0x82u ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ++binding->invocations;
  binding->event = *event;
  Nba97GameMatchBufferCompressContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result =
      nba97_game_match_buffer_compress(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

int nba97_game_match_buffer_record_with_compress(
    const Nba97GameMatchBufferRecordContext *parent,
    Nba97GameMatchBufferCompressBinding *binding,
    Nba97GameMatchBufferRecordProgress *progress) {
  if (!parent || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  std::memset(&binding->event, 0, sizeof binding->event);
  std::memset(&binding->progress, 0, sizeof binding->progress);
  binding->result = NBA97_TEXT_COMPLETE;
  ParentRun run{parent->io, parent->user, binding};
  Nba97GameMatchBufferRecordContext context = *parent;
  context.io = dispatchParent;
  context.user = &run;
  const int result = nba97_game_match_buffer_record(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED && progress->stopped_pc == 0x80076e58u)
    return binding->result;
  return result;
}
