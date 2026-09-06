#include "game_gte_reference_transform.h"

#include <string.h>

typedef struct Run {
  Nba97GameGteReferenceTransformContext *context;
  Nba97GameGteReferenceTransformProgress *progress;
  Nba97GameGteReferenceTransformMachine machine;
  Nba97GameGteReferenceTransformState state;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int status_ = (expression);                                                \
    if (status_ != NBA97_TEXT_COMPLETE)                                        \
      return status_;                                                          \
  } while (0)

static void publish(Run *run) {
  run->progress->machine = run->machine;
  run->progress->state = run->state;
}

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t command) {
  run->progress->stopped_pc = pc;
  run->progress->stopped_address = address;
  run->progress->stopped_command = command;
  publish(run);
}

static int valid_machine(const Nba97GameGteReferenceTransformMachine *machine) {
  unsigned index;
  if (machine->registers.gpr[0].word != 0u ||
      machine->registers.gpr[0].known_mask != 15u ||
      machine->hi.known_mask > 15u || machine->lo.known_mask > 15u)
    return 0;
  for (index = 0u; index != 32u; ++index)
    if (machine->registers.gpr[index].known_mask > 15u)
      return 0;
  return 1;
}

static int valid_state(const Nba97GameGteReferenceTransformState *state) {
  unsigned index;
  for (index = 0u; index != 32u; ++index)
    if (state->control[index].known_mask > 15u ||
        state->data[index].known_mask > 15u)
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

static int initialize(Nba97GameGteReferenceTransformContext *context,
                      Nba97GameGteReferenceTransformProgress *progress,
                      Run *run) {
  if (progress == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof(*progress));
  if (context == NULL || !valid_memory(&context->memory) ||
      !valid_machine(&context->machine) || !valid_state(&context->state) ||
      (context->access_journal_capacity != 0u &&
       context->access_journal == NULL))
    return NBA97_TEXT_ARGUMENT;
  run->context = context;
  run->progress = progress;
  run->machine = context->machine;
  run->state = context->state;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int spend(Run *run) {
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    const Nba97GameGteReferenceTransformWord *value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameGteReferenceTransformAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word;
    event->operation = run->progress->operations;
    event->width = 4u;
    event->known_mask = value->known_mask;
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t pc, uint32_t address, uint8_t **data,
                  uint8_t **known_bytes) {
  size_t index, byte;
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
    *data = region->data + (size_t)offset;
    *known_bytes =
        region->known != NULL ? region->known + (size_t)offset : NULL;
    if (*known_bytes != NULL)
      for (byte = 0u; byte != 4u; ++byte)
        if ((*known_bytes)[byte] > 1u)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int read_word(Run *run, uint32_t pc, uint32_t address,
                     Nba97GameGteReferenceTransformWord *destination) {
  Nba97GameGteReferenceTransformWord loaded = {0u, 0u};
  uint8_t *data, *known_bytes;
  unsigned byte;
  TRY(locate(run, pc, address, &data, &known_bytes));
  for (byte = 0u; byte != 4u; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (byte * 8u);
    if (known_bytes == NULL || known_bytes[byte] != 0u)
      loaded.known_mask = (uint8_t)(loaded.known_mask | (uint8_t)(1u << byte));
  }
  *destination = loaded;
  ++run->progress->reads;
  journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, address, &loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_word(Run *run, uint32_t pc, uint32_t address,
                      const Nba97GameGteReferenceTransformWord *value) {
  uint8_t *data, *known_bytes;
  unsigned byte;
  TRY(locate(run, pc, address, &data, &known_bytes));
  if (known_bytes == NULL && value->known_mask != 15u)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0u; byte != 4u; ++byte) {
    data[byte] = (uint8_t)(value->word >> (byte * 8u));
    if (known_bytes != NULL)
      known_bytes[byte] = (uint8_t)((value->known_mask >> byte) & 1u);
  }
  ++run->progress->stores;
  journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int address(Run *run, unsigned register_index, uint32_t offset,
                   uint32_t pc, uint32_t *result) {
  Nba97GameGteReferenceTransformWord base = R(register_index);
  if (base.known_mask != 15u) {
    stop(run, pc, base.word + offset, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = base.word + offset;
  return NBA97_TEXT_COMPLETE;
}

static int read_at(Run *run, unsigned register_index, uint32_t offset,
                   uint32_t pc,
                   Nba97GameGteReferenceTransformWord *destination) {
  uint32_t guest_address;
  TRY(address(run, register_index, offset, pc, &guest_address));
  return read_word(run, pc, guest_address, destination);
}

static int write_at(Run *run, unsigned register_index, uint32_t offset,
                    uint32_t pc,
                    const Nba97GameGteReferenceTransformWord *value) {
  uint32_t guest_address;
  TRY(address(run, register_index, offset, pc, &guest_address));
  return write_word(run, pc, guest_address, value);
}

static Nba97GameGteReferenceTransformWord
sign_extend_low_half(Nba97GameGteReferenceTransformWord value) {
  uint32_t half = value.word & UINT32_C(0x0000ffff);
  value.word =
      (half & UINT32_C(0x8000)) != 0u ? half | UINT32_C(0xffff0000) : half;
  value.known_mask = (uint8_t)((value.known_mask & 3u) |
                               ((value.known_mask & 2u) != 0u ? 12u : 0u));
  return value;
}

static int valid_result(int result) {
  return result == NBA97_TEXT_COMPLETE || result == NBA97_TEXT_ARGUMENT ||
         result == NBA97_TEXT_RESOURCE || result == NBA97_TEXT_UNKNOWN ||
         result == NBA97_TEXT_ALIGNMENT_TRAP || result == NBA97_TEXT_LIMIT ||
         result == NBA97_TEXT_IO_REFUSED;
}

static int transform(Run *run) {
  Nba97GameGteReferenceTransformHardwareEvent event;
  int result;
  stop(run, UINT32_C(0x8005665c), 0u, UINT32_C(0x00480012));
  TRY(spend(run));
  event.pc = UINT32_C(0x8005665c);
  event.command = UINT32_C(0x00480012);
  event.operation = run->progress->operations;
  event.invocation = run->progress->hardware_calls;
  ++run->progress->hardware_calls;
  if (run->context->hardware == NULL)
    return NBA97_TEXT_IO_REFUSED;
  result =
      run->context->hardware(run->context->hardware_user, &event, &run->state);
  publish(run);
  if (!valid_result(result))
    return NBA97_TEXT_ARGUMENT;
  if (result != NBA97_TEXT_COMPLETE)
    return result;
  if (!valid_state(&run->state))
    return NBA97_TEXT_ARGUMENT;
  ++run->progress->hardware_completed;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_gte_reference_transform(
    Nba97GameGteReferenceTransformContext *context,
    Nba97GameGteReferenceTransformProgress *progress) {
  Run state;
  Run *run = &state;
  Nba97GameGteReferenceTransformWord loaded;

  TRY(initialize(context, progress, run));

  /* 0x80056650..0x80056654: load V0XY, then full V0Z word. */
  TRY(read_at(run, NBA97_MATCH_INITIALIZE_A0, 0u, UINT32_C(0x80056650),
              &run->state.data[0]));
  TRY(read_at(run, NBA97_MATCH_INITIALIZE_A0, 4u, UINT32_C(0x80056654),
              &loaded));
  run->state.data[1] = sign_extend_low_half(loaded);
  publish(run);

  /* 0x80056658..0x8005665C: NOP, then RT * V0 + TR, sf=12, lm=0. */
  TRY(transform(run));

  /* 0x80056660..0x80056668: publish raw MAC1..3 in source order. */
  TRY(write_at(run, NBA97_MATCH_INITIALIZE_A1, 0u, UINT32_C(0x80056660),
               &run->state.data[25]));
  TRY(write_at(run, NBA97_MATCH_INITIALIZE_A1, 4u, UINT32_C(0x80056664),
               &run->state.data[26]));
  TRY(write_at(run, NBA97_MATCH_INITIALIZE_A1, 8u, UINT32_C(0x80056668),
               &run->state.data[27]));

  /* 0x8005666C: CFC2 v0, FLAG. */
  R(NBA97_MATCH_INITIALIZE_V0) = run->state.control[31];
  publish(run);

  /* 0x80056670/0x80056674: the FLAG store is the JR delay slot. */
  TRY(write_at(run, NBA97_MATCH_INITIALIZE_A2, 0u, UINT32_C(0x80056674),
               &R(NBA97_MATCH_INITIALIZE_V0)));
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15u) {
    stop(run, UINT32_C(0x80056670), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }

  progress->completed = 1u;
  stop(run, 0u, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
