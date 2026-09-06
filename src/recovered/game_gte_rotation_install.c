#include "game_gte_rotation_install.h"

#include <string.h>

typedef struct Run {
  Nba97GameGteRotationInstallContext *context;
  Nba97GameGteRotationInstallProgress *progress;
  Nba97GameGteRotationInstallMachine machine;
} Run;

#define TRY(expression)                                                        \
  do {                                                                         \
    int status_ = (expression);                                                \
    if (status_ != NBA97_TEXT_COMPLETE)                                        \
      return status_;                                                          \
  } while (0)

static int valid_word(Nba97GameGteRotationInstallWord value) {
  return value.known_mask <= 15u;
}

static int valid_machine(const Nba97GameGteRotationInstallMachine *machine) {
  unsigned index;
  if (machine->registers.gpr[0].word != 0u ||
      machine->registers.gpr[0].known_mask != 15u ||
      !valid_word(machine->hi) || !valid_word(machine->lo))
    return 0;
  for (index = 0u; index != 32u; ++index)
    if (!valid_word(machine->registers.gpr[index]))
      return 0;
  return 1;
}

static void publish(Run *run) {
  run->progress->machine = run->machine;
  memcpy(run->progress->control, run->context->control,
         sizeof(run->progress->control));
}

static void stop(Run *run, uint32_t pc, uint32_t address, uint8_t control) {
  run->progress->stopped_pc = pc;
  run->progress->stopped_address = address;
  run->progress->stopped_control = control;
  publish(run);
}

static int initialize(Nba97GameGteRotationInstallContext *context,
                      Nba97GameGteRotationInstallProgress *progress,
                      Run *run) {
  size_t index, earlier;
  if (progress == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof(*progress));
  if (context == NULL)
    return NBA97_TEXT_ARGUMENT;
  run->context = context;
  run->progress = progress;
  run->machine = context->machine;
  progress->machine = context->machine;
  if (context->control != NULL)
    memcpy(progress->control, context->control, sizeof(progress->control));
  if (context->control == NULL ||
      (context->memory.count != 0u && context->memory.region == NULL) ||
      (context->access_journal_capacity != 0u &&
       context->access_journal == NULL) ||
      (context->control_journal_capacity != 0u &&
       context->control_journal == NULL) ||
      !valid_machine(&context->machine))
    return NBA97_TEXT_ARGUMENT;
  for (index = 0u; index != 32u; ++index)
    if (!valid_word(context->control[index])) {
      publish(run);
      return NBA97_TEXT_ARGUMENT;
    }
  for (index = 0u; index != context->memory.count; ++index) {
    const Nba97GameTextRegion *region = &context->memory.region[index];
    if (region->data == NULL || region->size == 0u ||
        region->size > UINT64_C(0x100000000) ||
        (uint64_t)region->base + region->size > UINT64_C(0x100000000)) {
      publish(run);
      return NBA97_TEXT_ARGUMENT;
    }
    for (earlier = 0u; earlier != index; ++earlier) {
      const Nba97GameTextRegion *other = &context->memory.region[earlier];
      if ((uint64_t)region->base < (uint64_t)other->base + other->size &&
          (uint64_t)other->base < (uint64_t)region->base + region->size) {
        publish(run);
        return NBA97_TEXT_ARGUMENT;
      }
    }
  }
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int spend(Run *run) {
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal_access(Run *run, uint32_t pc, uint32_t address,
                           Nba97GameGteRotationInstallWord value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameGteRotationInstallAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word;
    event->operation = run->progress->operations;
    event->width = 4u;
    event->known_mask = value.known_mask;
    event->kind = NBA97_GAME_MATCH_CLOCKS_READ;
  }
}

static int read_word(Run *run, uint32_t address, uint32_t pc,
                     Nba97GameGteRotationInstallWord *value) {
  Nba97GameGteRotationInstallWord loaded = {0u, 0u};
  size_t index;
  unsigned byte;
  stop(run, pc, address, 0u);
  TRY(spend(run));
  ++run->progress->accesses;
  if ((address & 3u) != 0u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (index = 0u; index != run->context->memory.count; ++index) {
    Nba97GameTextRegion *region = &run->context->memory.region[index];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        4u > region->size - (size_t)offset)
      continue;
    for (byte = 0u; byte != 4u; ++byte)
      if (region->known != NULL &&
          region->known[(size_t)offset + byte] > 1u)
        return NBA97_TEXT_ARGUMENT;
    for (byte = 0u; byte != 4u; ++byte) {
      uint8_t known = region->known == NULL
                          ? 1u
                          : region->known[(size_t)offset + byte];
      loaded.word |=
          (uint32_t)region->data[(size_t)offset + byte] << (8u * byte);
      if (known)
        loaded.known_mask =
            (uint8_t)(loaded.known_mask | (uint8_t)(1u << byte));
    }
    *value = loaded;
    ++run->progress->reads;
    journal_access(run, pc, address, loaded);
    publish(run);
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static void journal_control(Run *run, uint32_t pc, uint8_t index,
                            Nba97GameGteRotationInstallWord value) {
  size_t event_index = run->progress->control_events++;
  if (event_index < run->context->control_journal_capacity) {
    Nba97GameGteRotationInstallControlWrite *event =
        &run->context->control_journal[event_index];
    event->pc = pc;
    event->operation = run->progress->operations;
    event->value = value;
    event->index = index;
  }
}

static int write_control(Run *run, uint32_t pc, uint8_t index,
                         Nba97GameGteRotationInstallWord value) {
  stop(run, pc, 0u, index);
  TRY(spend(run));
  run->context->control[index] = value;
  ++run->progress->control_writes;
  journal_control(run, pc, index, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static Nba97GameGteRotationInstallWord
rt33_value(Nba97GameGteRotationInstallWord raw) {
  Nba97GameGteRotationInstallWord result;
  uint32_t half = raw.word & UINT32_C(0xffff);
  result.word = (half & UINT32_C(0x8000))
                    ? half | UINT32_C(0xffff0000)
                    : half;
  result.known_mask =
      (uint8_t)((raw.known_mask & 3u) | ((raw.known_mask & 2u) ? 12u : 0u));
  return result;
}

static int install(Run *run) {
  uint32_t base;
  unsigned index;
  if (run->machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask != 15u) {
    stop(run, UINT32_C(0x80055f18),
         run->machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  base = run->machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word;

  /* 0x80055F18..0x80055F28: the source finishes all five matrix reads
   * before changing any GTE control. Address addition wraps at 32 bits. */
  for (index = 0u; index != 5u; ++index)
    TRY(read_word(run, base + index * 4u,
                  UINT32_C(0x80055f18) + index * 4u,
                  &run->machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 +
                                              index]));

  /* 0x80055F2C..0x80055F38: controls 0..3 preserve each raw word and its
   * per-byte knowledge exactly. */
  for (index = 0u; index != 4u; ++index)
    TRY(write_control(run, UINT32_C(0x80055f2c) + index * 4u,
                      (uint8_t)index,
                      run->machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 +
                                                  index]));

  /* 0x80055F3C..0x80055F40: RT33 is the JR delay slot and therefore writes
   * before a live unknown ra can refuse native continuation. */
  TRY(write_control(
      run, UINT32_C(0x80055f40), 4u,
      rt33_value(run->machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 4u])));
  if (run->machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15u) {
    stop(run, UINT32_C(0x80055f3c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_gte_rotation_install(
    Nba97GameGteRotationInstallContext *context,
    Nba97GameGteRotationInstallProgress *progress) {
  Run run;
  int status = initialize(context, progress, &run);
  if (status != NBA97_TEXT_COMPLETE)
    return status;
  status = install(&run);
  publish(&run);
  if (status == NBA97_TEXT_COMPLETE) {
    progress->completed = 1u;
    progress->stopped_pc = 0u;
    progress->stopped_address = 0u;
    progress->stopped_control = 0u;
  }
  return status;
}
