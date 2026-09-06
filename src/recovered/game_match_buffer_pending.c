#include "game_match_buffer_pending.h"

#include <string.h>

#define PENDING_ADDRESS UINT32_C(0x800fe864)
#define STORE_PC UINT32_C(0x80076b30)
#define JR_PC UINT32_C(0x80076b34)

static int machine_valid(const Nba97GameMatchBufferPendingMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int memory_valid(const Nba97GameTextMemory *memory) {
  size_t i;
  size_t j;
  if (!memory->region && memory->count)
    return 0;
  for (i = 0; i < memory->count; ++i) {
    const Nba97GameTextRegion *a = &memory->region[i];
    uint64_t size = (uint64_t)a->size;
    if (!a->data || !a->size || size > UINT64_C(0x100000000) ||
        (uint64_t)a->base + size > UINT64_C(0x100000000))
      return 0;
    for (j = 0; j < i; ++j) {
      const Nba97GameTextRegion *b = &memory->region[j];
      if ((uint64_t)a->base < (uint64_t)b->base + b->size &&
          (uint64_t)b->base < (uint64_t)a->base + a->size)
        return 0;
    }
  }
  return 1;
}

static void set_known(Nba97GameMatchBufferPendingWord *value,
                      uint32_t word) {
  value->word = word;
  value->known_mask = 0x0fu;
}

static void publish(Nba97GameMatchBufferPendingProgress *out,
                    const Nba97GameMatchBufferPendingMachine *machine) {
  out->machine = *machine;
  out->returned_value =
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0];
}

static void stop(Nba97GameMatchBufferPendingProgress *out,
                 const Nba97GameMatchBufferPendingMachine *machine,
                 uint32_t pc, uint32_t address) {
  out->stopped_pc = pc;
  out->stopped_address = address;
  publish(out, machine);
}

static int store_pending(Nba97GameMatchBufferPendingContext *context,
                         Nba97GameMatchBufferPendingProgress *out,
                         Nba97GameMatchBufferPendingMachine *machine) {
  size_t i;
  uint8_t *data = 0;
  uint8_t *known = 0;
  stop(out, machine, STORE_PC, PENDING_ADDRESS);
  if (out->operations >= context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++out->operations;
  ++out->accesses;
  for (i = 0; i < context->memory.count; ++i) {
    Nba97GameTextRegion *region = &context->memory.region[i];
    uint64_t offset = (uint64_t)PENDING_ADDRESS - region->base;
    if (PENDING_ADDRESS < region->base || offset >= region->size)
      continue;
    data = region->data + (size_t)offset;
    known = region->known ? region->known + (size_t)offset : 0;
    break;
  }
  if (!data)
    return NBA97_TEXT_RESOURCE;
  if (known && *known > 1)
    return NBA97_TEXT_ARGUMENT;
  *data = 1;
  if (known)
    *known = 1;
  ++out->stores;
  {
    size_t index = out->access_events++;
    if (index < context->access_journal_capacity) {
      Nba97GameMatchBufferPendingAccess *event =
          &context->access_journal[index];
      event->pc = STORE_PC;
      event->address = PENDING_ADDRESS;
      event->value = 1;
      event->operation = out->operations;
      event->width = 1;
      event->known_mask = 1;
      event->kind = NBA97_GAME_MATCH_BUFFER_PENDING_STORE;
    }
  }
  publish(out, machine);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_match_buffer_pending(
    Nba97GameMatchBufferPendingContext *context,
    Nba97GameMatchBufferPendingProgress *out) {
  Nba97GameMatchBufferPendingMachine machine;
  Nba97GameMatchBufferPendingWord ra;
  int result;
  if (!out)
    return NBA97_TEXT_ARGUMENT;
  memset(out, 0, sizeof *out);
  if (!context || !machine_valid(&context->machine) ||
      !memory_valid(&context->memory) ||
      (!context->access_journal && context->access_journal_capacity))
    return NBA97_TEXT_ARGUMENT;
  machine = context->machine;
  publish(out, &machine);

  /* GAMEONLY 0x80076B28..0x80076B30: both fixed register writes precede the
   * sole mapped store, so their effects survive any store failure. */
  set_known(&machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0], 1);
  set_known(&machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT],
            UINT32_C(0x80100000));
  result = store_pending(context, out, &machine);
  if (result != NBA97_TEXT_COMPLETE)
    return result;

  /* GAMEONLY 0x80076B34..0x80076B38: JR consumes the unchanged live ra after
   * the store and its NOP delay; unknownness or misalignment keeps the store. */
  ra = machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA];
  out->return_address = ra;
  if (ra.known_mask != 0x0fu) {
    stop(out, &machine, JR_PC, ra.word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (ra.word & 3u) {
    stop(out, &machine, JR_PC, ra.word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(out, &machine, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
