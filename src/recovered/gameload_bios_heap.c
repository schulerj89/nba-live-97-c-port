#include "gameload_bios_heap.h"

#include <string.h>

typedef struct Run {
  Nba97GameloadBiosHeapContext *context;
  Nba97GameloadBiosHeapProgress *progress;
  Nba97GameloadBiosHeapMachine machine;
} Run;

static int valid_word(Nba97GameloadBiosHeapWord value) {
  return value.known_mask <= 15u;
}

static int valid_machine(const Nba97GameloadBiosHeapMachine *machine) {
  unsigned index;
  if (machine->registers.gpr[0].word != 0u ||
      machine->registers.gpr[0].known_mask != 15u ||
      !valid_word(machine->hi) || !valid_word(machine->lo))
    return 0;
  for (index = 0u; index != NBA97_GAMELOAD_BIOS_HEAP_REGISTER_COUNT; ++index)
    if (!valid_word(machine->registers.gpr[index]))
      return 0;
  return 1;
}

static int valid_memory(const Nba97GameTextMemory *memory) {
  size_t index;
  size_t earlier;
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

static void step(Run *run, uint32_t pc) {
  size_t index = run->progress->instruction_events++;
  if (index < run->context->instruction_journal_capacity)
    run->context->instruction_journal[index] = pc;
  ++run->progress->instruction_count;
}

static int initialize(Nba97GameloadBiosHeapContext *context,
                      Nba97GameloadBiosHeapProgress *progress, Run *run) {
  if (progress == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof(*progress));
  if (context == NULL)
    return NBA97_TEXT_ARGUMENT;
  run->context = context;
  run->progress = progress;
  run->machine = context->machine;
  publish(run);
  if (!valid_machine(&context->machine) || !valid_memory(&context->memory) ||
      (context->instruction_journal == NULL &&
       context->instruction_journal_capacity != 0u))
    return NBA97_TEXT_ARGUMENT;
  return NBA97_TEXT_COMPLETE;
}

static int invoke_bios(Run *run) {
  Nba97GameloadBiosHeapEvent event;
  int accepted;
  stop(run, UINT32_C(0x801e1594), UINT32_C(0x000000a0), 0x39u);
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  memset(&event, 0, sizeof(event));
  event.pc = UINT32_C(0x801e1594);
  event.delay_slot_pc = UINT32_C(0x801e1598);
  event.entry = UINT32_C(0x000000a0);
  event.operation = run->progress->operations;
  event.invocation = 1u;
  event.site = NBA97_GAMELOAD_BIOS_HEAP_SITE_A0_SERVICE_39;
  event.service = 0x39u;
  event.argument_count = 2u;
  run->progress->event = event;
  publish(run);
  if (run->context->io == NULL)
    return NBA97_TEXT_IO_REFUSED;
  accepted = run->context->io(run->context->user, &run->context->memory,
                              &event, &run->machine);
  publish(run);
  if (accepted != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!valid_machine(&run->machine))
    return NBA97_TEXT_ARGUMENT;
  ++run->progress->callbacks_completed;
  return NBA97_TEXT_COMPLETE;
}

int nba97_gameload_bios_heap(Nba97GameloadBiosHeapContext *context,
                             Nba97GameloadBiosHeapProgress *progress) {
  Run run;
  int status = initialize(context, progress, &run);
  if (status != NBA97_TEXT_COMPLETE)
    return status;

  /* 0x801E1590 loads the BIOS A0 vector before JR reads t2. */
  step(&run, UINT32_C(0x801e1590));
  run.machine.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_T2].word =
      UINT32_C(0x000000a0);
  run.machine.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_T2].known_mask = 15u;

  /* 0x801E1594..0x801E1598: JR does not link; its delay slot selects InitHeap
   * before the bounded BIOS callback observes the full machine. */
  step(&run, UINT32_C(0x801e1594));
  step(&run, UINT32_C(0x801e1598));
  run.machine.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_T1].word = 0x39u;
  run.machine.registers.gpr[NBA97_GAMELOAD_BIOS_HEAP_T1].known_mask = 15u;

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
