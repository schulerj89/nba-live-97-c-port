#include "game_bios_memory_copy.h"

#include <string.h>

typedef struct Run {
  Nba97GameBiosMemoryCopyContext *context;
  Nba97GameBiosMemoryCopyProgress *progress;
  Nba97GameBiosMemoryCopyMachine machine;
} Run;

static int valid_word(Nba97GameBiosMemoryCopyWord value) {
  return value.known_mask <= 15u;
}

static int valid_machine(const Nba97GameBiosMemoryCopyMachine *machine) {
  unsigned index;
  if (machine->registers.gpr[0].word != 0u ||
      machine->registers.gpr[0].known_mask != 15u || !valid_word(machine->hi) ||
      !valid_word(machine->lo))
    return 0;
  for (index = 0u; index != 32u; ++index)
    if (!valid_word(machine->registers.gpr[index]))
      return 0;
  return 1;
}

static int valid_memory(const Nba97GameTextMemory *memory) {
  size_t index, earlier;
  if (memory->count != 0u && memory->region == NULL)
    return 0;
  for (index = 0u; index != memory->count; ++index) {
    const Nba97GameTextRegion *region = &memory->region[index];
    if (region->data == NULL || region->size == 0u ||
        region->size > UINT64_C(0x100000000) ||
        (uint64_t)region->base + region->size > UINT64_C(0x100000000))
      return 0;
    for (earlier = 0u; earlier != index; ++earlier) {
      const Nba97GameTextRegion *other = &memory->region[earlier];
      if ((uint64_t)region->base < (uint64_t)other->base + other->size &&
          (uint64_t)other->base < (uint64_t)region->base + region->size)
        return 0;
    }
  }
  return 1;
}

static void publish(Run *run) { run->progress->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t entry, uint8_t service) {
  run->progress->stopped_pc = pc;
  run->progress->stopped_entry = entry;
  run->progress->stopped_service = service;
  publish(run);
}

static int initialize(Nba97GameBiosMemoryCopyContext *context,
                      Nba97GameBiosMemoryCopyProgress *progress, Run *run) {
  if (progress == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof(*progress));
  if (context == NULL)
    return NBA97_TEXT_ARGUMENT;
  run->context = context;
  run->progress = progress;
  run->machine = context->machine;
  publish(run);
  if (!valid_machine(&context->machine) || !valid_memory(&context->memory))
    return NBA97_TEXT_ARGUMENT;
  return NBA97_TEXT_COMPLETE;
}

static int invoke_bios(Run *run) {
  Nba97GameBiosMemoryCopyEvent event;
  int accepted;
  stop(run, UINT32_C(0x8009cb10), UINT32_C(0x000000a0), 0x2au);
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  memset(&event, 0, sizeof(event));
  event.pc = UINT32_C(0x8009cb10);
  event.delay_slot_pc = UINT32_C(0x8009cb14);
  event.entry = UINT32_C(0x000000a0);
  event.operation = run->progress->operations;
  event.invocation = 1u;
  event.service = 0x2au;
  event.argument_count = 3u;
  run->progress->event = event;
  publish(run);
  if (run->context->io == NULL)
    return NBA97_TEXT_IO_REFUSED;
  accepted = run->context->io(run->context->user, &run->context->memory, &event,
                              &run->machine);
  publish(run);
  if (accepted != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!valid_machine(&run->machine))
    return NBA97_TEXT_ARGUMENT;
  ++run->progress->callbacks_completed;
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_bios_memory_copy(Nba97GameBiosMemoryCopyContext *context,
                                Nba97GameBiosMemoryCopyProgress *progress) {
  Run run;
  int status = initialize(context, progress, &run);
  if (status != NBA97_TEXT_COMPLETE)
    return status;

  /* 0x8009CB0C..0x8009CB14: JR uses the newly loaded BIOS vector and its
   * delay slot selects memcpy. Neither instruction consumes the live args or
   * ra before the typed tail transfer. */
  run.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 2u].word =
      UINT32_C(0x000000a0);
  run.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 2u].known_mask = 15u;
  run.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 1u].word = 0x2au;
  run.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 1u].known_mask = 15u;
  status = invoke_bios(&run);
  publish(&run);
  if (status == NBA97_TEXT_COMPLETE) {
    progress->completed = 1u;
    progress->stopped_pc = 0u;
    progress->stopped_entry = 0u;
    progress->stopped_service = 0u;
  }
  return status;
}
