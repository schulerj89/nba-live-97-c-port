#include "frontend_clock_read.h"

#include <string.h>

typedef Nba97FrontendClockReadWord Word;

typedef struct Run {
  Nba97FrontendClockReadContext *context;
  Nba97FrontendClockReadProgress *out;
  Nba97FrontendClockReadMachine machine;
} Run;

#define REG(index) (run->machine.registers.gpr[(index)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int result_ = (expression);                                                \
    if (result_ != NBA97_TEXT_COMPLETE)                                        \
      return result_;                                                          \
  } while (0)
#define STEP(pc_)                                                              \
  do {                                                                         \
    size_t index_ = run->out->instruction_events++;                            \
    if (index_ < run->context->instruction_journal_capacity)                   \
      run->context->instruction_journal[index_] = UINT32_C(pc_);               \
    ++run->out->instruction_count;                                             \
  } while (0)

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t target) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  run->out->stopped_target = target;
  publish(run);
}

static int machine_valid(const Nba97FrontendClockReadMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_FRONTEND_CLOCK_READ_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int initialize(Nba97FrontendClockReadContext *context,
                      Nba97FrontendClockReadProgress *out, Run *run) {
  size_t i;
  size_t j;
  if (!out)
    return NBA97_TEXT_ARGUMENT;
  memset(out, 0, sizeof *out);
  if (!context || (!context->memory.region && context->memory.count) ||
      (!context->access_journal && context->access_journal_capacity) ||
      (!context->instruction_journal && context->instruction_journal_capacity) ||
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

static void set_known(Word *word, uint32_t value) {
  word->word = value;
  word->known_mask = 0x0fu;
}

static int load_clock(Run *run) {
  uint8_t *data = 0;
  uint8_t *known = 0;
  Word loaded = {0, 0};
  size_t i;
  unsigned byte;
  const uint32_t address = UINT32_C(0x800d9ab8);
  stop(run, UINT32_C(0x8008da60), address, 0);
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  ++run->out->accesses;
  for (i = 0; i < run->context->memory.count; ++i) {
    Nba97GameTextRegion *region = &run->context->memory.region[i];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        4u > region->size - (size_t)offset)
      continue;
    data = region->data + (size_t)offset;
    known = region->known ? region->known + (size_t)offset : 0;
    if (known)
      for (byte = 0; byte < 4; ++byte)
        if (known[byte] > 1)
          return NBA97_TEXT_ARGUMENT;
    break;
  }
  if (!data)
    return NBA97_TEXT_RESOURCE;
  for (byte = 0; byte < 4; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (byte * 8u);
    if (!known || known[byte])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << byte));
  }
  REG(NBA97_FRONTEND_CLOCK_READ_V0) = loaded;
  run->out->loaded_clock = loaded;
  ++run->out->reads;
  {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
      Nba97FrontendClockReadAccess *event = &run->context->access_journal[index];
      event->pc = UINT32_C(0x8008da60);
      event->address = address;
      event->value = loaded.word;
      event->operation = run->out->operations;
      event->width = 4;
      event->known_mask = loaded.known_mask;
      event->kind = NBA97_FRONTEND_CLOCK_READ_READ;
    }
  }
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

int nba97_frontend_clock_read(Nba97FrontendClockReadContext *context,
                              Nba97FrontendClockReadProgress *out) {
  Run storage;
  Run *run = &storage;
  TRY(initialize(context, out, run));

  STEP(0x8008da5c);
  set_known(&REG(NBA97_FRONTEND_CLOCK_READ_V0), UINT32_C(0x800e0000));
  STEP(0x8008da60);
  TRY(load_clock(run));
  STEP(0x8008da64);
  STEP(0x8008da68);
  if (REG(NBA97_FRONTEND_CLOCK_READ_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x8008da64), 0,
         REG(NBA97_FRONTEND_CLOCK_READ_RA).word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (REG(NBA97_FRONTEND_CLOCK_READ_RA).word & 3u) {
    stop(run, UINT32_C(0x8008da64), 0,
         REG(NBA97_FRONTEND_CLOCK_READ_RA).word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
