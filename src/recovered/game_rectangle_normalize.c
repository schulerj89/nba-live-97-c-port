#include "game_rectangle_normalize.h"

#include <stdint.h>
#include <string.h>

typedef Nba97GameRectangleNormalizeWord Word;

typedef struct Run {
  Nba97GameRectangleNormalizeContext *context;
  Nba97GameRectangleNormalizeProgress *out;
  Nba97GameRectangleNormalizeMachine machine;
} Run;

#define R(i) (run->machine.registers.gpr[(i)])
#define V0 R(2)
#define A0 R(4)
#define RA R(31)
#define TRY(expression)                                                        \
  do {                                                                         \
    int result_ = (expression);                                                \
    if (result_ != NBA97_TEXT_COMPLETE)                                        \
      return result_;                                                          \
  } while (0)
#define STEP(pc)                                                               \
  do {                                                                         \
    (void)(pc);                                                                \
    ++run->out->instruction_count;                                             \
  } while (0)

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  publish(run);
}

static int machine_valid(const Nba97GameRectangleNormalizeMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 15 ||
      machine->hi.known_mask > 15 || machine->lo.known_mask > 15)
    return 0;
  for (i = 0; i < 32; ++i)
    if (machine->registers.gpr[i].known_mask > 15)
      return 0;
  return 1;
}

static int initialize(Nba97GameRectangleNormalizeContext *context,
                      Nba97GameRectangleNormalizeProgress *out, Run *run) {
  size_t i;
  size_t j;
  if (!out)
    return NBA97_TEXT_ARGUMENT;
  memset(out, 0, sizeof *out);
  if (!context || (!context->memory.region && context->memory.count) ||
      (!context->access_journal && context->access_journal_capacity) ||
      !machine_valid(&context->machine))
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < context->memory.count; ++i) {
    const Nba97GameTextRegion *a = &context->memory.region[i];
    if (!a->data || !a->size || a->size > UINT64_C(0x100000000) ||
        (uint64_t)a->base + a->size > UINT64_C(0x100000000))
      return NBA97_TEXT_ARGUMENT;
    for (j = 0; j < i; ++j) {
      const Nba97GameTextRegion *b = &context->memory.region[j];
      if ((uint64_t)a->base < (uint64_t)b->base + b->size &&
          (uint64_t)b->base < (uint64_t)a->base + a->size)
        return NBA97_TEXT_ARGUMENT;
    }
  }
  run->context = context;
  run->out = out;
  run->machine = context->machine;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    Word value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameRectangleNormalizeAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word & 0xffffu;
    event->operation = run->out->operations;
    event->width = 2;
    event->known_mask = (uint8_t)(value.known_mask & 3u);
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t address, uint32_t pc, uint8_t **data,
                  uint8_t **known_bytes) {
  size_t i;
  size_t j;
  stop(run, pc, address);
  TRY(spend(run));
  ++run->out->accesses;
  if (address & 1u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0; i < run->context->memory.count; ++i) {
    Nba97GameTextRegion *region = &run->context->memory.region[i];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        2u > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known_bytes = region->known ? region->known + (size_t)offset : 0;
    if (*known_bytes)
      for (j = 0; j < 2; ++j)
        if ((*known_bytes)[j] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int read_half(Run *run, uint32_t address, uint32_t pc, Word *value) {
  uint8_t *data;
  uint8_t *known_bytes;
  Word loaded = {0, 12};
  unsigned i;
  TRY(locate(run, address, pc, &data, &known_bytes));
  for (i = 0; i < 2; ++i) {
    loaded.word |= (uint32_t)data[i] << (i * 8u);
    if (!known_bytes || known_bytes[i])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
  }
  *value = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_RECTANGLE_NORMALIZE_READ, pc, address, loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_half(Run *run, uint32_t address, uint32_t pc, Word value) {
  uint8_t *data;
  uint8_t *known_bytes;
  unsigned i;
  TRY(locate(run, address, pc, &data, &known_bytes));
  if (!known_bytes && (value.known_mask & 3u) != 3u)
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < 2; ++i) {
    data[i] = (uint8_t)(value.word >> (i * 8u));
    if (known_bytes)
      known_bytes[i] = (uint8_t)((value.known_mask >> i) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_RECTANGLE_NORMALIZE_STORE, pc, address, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int effective_address(Run *run, uint32_t offset, uint32_t pc,
                             uint32_t *address) {
  if (A0.known_mask != 15) {
    stop(run, pc, A0.word + offset);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = A0.word + offset;
  return NBA97_TEXT_COMPLETE;
}

static Word and_constant(Word value, uint32_t constant) {
  Word result;
  unsigned byte;
  result.word = value.word & constant;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    uint32_t mask = (constant >> (byte * 8u)) & 255u;
    if ((value.known_mask & (1u << byte)) || mask == 0)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static Word or_constant(Word value, uint32_t constant) {
  Word result;
  unsigned byte;
  result.word = value.word | constant;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    uint32_t bits = (constant >> (byte * 8u)) & 255u;
    if ((value.known_mask & (1u << byte)) || bits == 255u)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static int zero_decision(Run *run, Word value, uint32_t pc, int *is_zero) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((value.known_mask & (1u << byte)) &&
        ((value.word >> (byte * 8u)) & 255u)) {
      *is_zero = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (value.known_mask == 15) {
    *is_zero = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0);
  return NBA97_TEXT_UNKNOWN;
}

int nba97_game_rectangle_normalize(Nba97GameRectangleNormalizeContext *context,
                                   Nba97GameRectangleNormalizeProgress *out) {
  Run storage;
  Run *run = &storage;
  uint32_t address;
  int decision;
  TRY(initialize(context, out, run));

  /* 0x80094440..0x80094450: read unsigned width, discard all but its low
   * bit, execute the BEQ NOP delay, and skip height entirely when even. */
  STEP(0x80094440);
  TRY(effective_address(run, 4, 0x80094440, &address));
  TRY(read_half(run, address, 0x80094440, &V0));
  STEP(0x80094444);
  STEP(0x80094448);
  V0 = and_constant(V0, 1);
  STEP(0x8009444c);
  STEP(0x80094450);
  TRY(zero_decision(run, V0, 0x8009444c, &decision));
  if (decision)
    goto epilogue;

  /* 0x80094454..0x80094460: odd width reads unsigned height, forces its low
   * bit, and stores the resulting halfword back in exact source order. */
  STEP(0x80094454);
  TRY(effective_address(run, 6, 0x80094454, &address));
  TRY(read_half(run, address, 0x80094454, &V0));
  STEP(0x80094458);
  STEP(0x8009445c);
  V0 = or_constant(V0, 1);
  STEP(0x80094460);
  TRY(effective_address(run, 6, 0x80094460, &address));
  TRY(write_half(run, address, 0x80094460, V0));

epilogue:
  /* 0x80094464/0x80094468: execute the JR NOP delay before consuming ra. */
  STEP(0x80094464);
  STEP(0x80094468);
  if (RA.known_mask != 15) {
    stop(run, 0x80094464, RA.word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, 0x80094464, RA.word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  stop(run, 0, 0);
  out->completed = 1;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}
