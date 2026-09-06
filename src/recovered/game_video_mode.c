#include "game_video_mode.h"

#include <string.h>

typedef struct Run {
  Nba97GameVideoModeContext *context;
  Nba97GameVideoModeProgress *progress;
  Nba97GameVideoModeMachine machine;
} Run;

#define VIDEO_ADDRESS UINT32_C(0x800c54ac)

static int valid_word(Nba97GameVideoModeWord value) {
  return value.known_mask <= 15u;
}

static int valid_machine(const Nba97GameVideoModeMachine *machine) {
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

static void publish(Run *run) {
  run->progress->machine = run->machine;
  run->progress->return_v0 =
      run->machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0];
}

static void stop(Run *run, uint32_t pc, uint32_t address) {
  run->progress->stopped_pc = pc;
  run->progress->stopped_address = address;
  publish(run);
}

static void journal_read(Run *run, Nba97GameVideoModeWord value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameVideoModeAccess *event = &run->context->access_journal[index];
    event->pc = UINT32_C(0x800985d0);
    event->address = VIDEO_ADDRESS;
    event->value = value.word;
    event->operation = run->progress->operations;
    event->width = 4u;
    event->known_mask = value.known_mask;
    event->kind = NBA97_GAME_MATCH_CLOCKS_READ;
  }
}

static int read_video_word(Run *run) {
  Nba97GameVideoModeWord loaded = {0u, 0u};
  size_t index;
  unsigned byte;
  stop(run, UINT32_C(0x800985d0), VIDEO_ADDRESS);
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  ++run->progress->accesses;
  for (index = 0u; index != run->context->memory.count; ++index) {
    Nba97GameTextRegion *region = &run->context->memory.region[index];
    uint64_t offset = (uint64_t)VIDEO_ADDRESS - region->base;
    if (VIDEO_ADDRESS < region->base || offset > region->size ||
        4u > region->size - (size_t)offset)
      continue;
    for (byte = 0u; byte != 4u; ++byte)
      if (region->known != NULL && region->known[(size_t)offset + byte] > 1u)
        return NBA97_TEXT_ARGUMENT;
    for (byte = 0u; byte != 4u; ++byte) {
      uint8_t known =
          region->known == NULL ? 1u : region->known[(size_t)offset + byte];
      loaded.word |= (uint32_t)region->data[(size_t)offset + byte]
                     << (8u * byte);
      if (known)
        loaded.known_mask =
            (uint8_t)(loaded.known_mask | (uint8_t)(1u << byte));
    }
    run->machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0] = loaded;
    ++run->progress->reads;
    journal_read(run, loaded);
    publish(run);
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

int nba97_game_video_mode(Nba97GameVideoModeContext *context,
                          Nba97GameVideoModeProgress *progress) {
  Run run;
  int status;
  if (progress == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof(*progress));
  if (context == NULL)
    return NBA97_TEXT_ARGUMENT;
  run.context = context;
  run.progress = progress;
  run.machine = context->machine;
  publish(&run);
  if (!valid_machine(&context->machine) || !valid_memory(&context->memory) ||
      (context->access_journal_capacity != 0u &&
       context->access_journal == NULL))
    return NBA97_TEXT_ARGUMENT;

  /* 0x800985CC..0x800985D0: LUI creates an observable known V0 prefix before
   * the bounded mapped load replaces it atomically. */
  run.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word =
      UINT32_C(0x800c0000);
  run.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask = 15u;
  status = read_video_word(&run);
  if (status != NBA97_TEXT_COMPLETE) {
    publish(&run);
    return status;
  }

  /* 0x800985D4..0x800985D8: the NOP delay changes no state; only then does
   * native continuation require a known live ra. */
  if (run.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15u) {
    stop(&run, UINT32_C(0x800985d4), 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  publish(&run);
  progress->completed = 1u;
  progress->stopped_pc = 0u;
  progress->stopped_address = 0u;
  return NBA97_TEXT_COMPLETE;
}
