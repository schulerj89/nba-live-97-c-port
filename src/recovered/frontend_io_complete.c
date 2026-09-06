#include "frontend_io_complete.h"

#include <string.h>

typedef Nba97FrontendIoCompleteWord Word;

typedef struct Run {
  Nba97FrontendIoCompleteContext *context;
  Nba97FrontendIoCompleteProgress *out;
  Nba97FrontendIoCompleteMachine machine;
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

static int machine_valid(const Nba97FrontendIoCompleteMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_FRONTEND_IO_COMPLETE_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int initialize(Nba97FrontendIoCompleteContext *context,
                      Nba97FrontendIoCompleteProgress *out, Run *run) {
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

static int load_word(Run *run, uint32_t pc, uint32_t address, Word *loaded) {
  uint8_t *data = 0;
  uint8_t *known = 0;
  size_t i;
  unsigned byte;
  stop(run, pc, address, 0);
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
  loaded->word = 0;
  loaded->known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    loaded->word |= (uint32_t)data[byte] << (byte * 8u);
    if (!known || known[byte])
      loaded->known_mask =
          (uint8_t)(loaded->known_mask | (uint8_t)(1u << byte));
  }
  ++run->out->reads;
  {
    size_t index = run->out->access_events++;
    if (index < run->context->access_journal_capacity) {
      Nba97FrontendIoCompleteAccess *event =
          &run->context->access_journal[index];
      event->pc = pc;
      event->address = address;
      event->value = loaded->word;
      event->operation = run->out->operations;
      event->width = 4;
      event->known_mask = loaded->known_mask;
      event->kind = NBA97_FRONTEND_IO_COMPLETE_READ;
    }
  }
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int nonzero_decision(Run *run, Word value, uint32_t pc,
                            int *is_nonzero) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((value.known_mask & (1u << byte)) &&
        ((value.word >> (byte * 8u)) & 0xffu)) {
      *is_nonzero = 1;
      return NBA97_TEXT_COMPLETE;
    }
  if (value.known_mask == 0x0fu) {
    *is_nonzero = 0;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

static int finish(Run *run) {
  STEP(0x800392f0);
  STEP(0x800392f4);
  if (REG(NBA97_FRONTEND_IO_COMPLETE_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x800392f0), 0,
         REG(NBA97_FRONTEND_IO_COMPLETE_RA).word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (REG(NBA97_FRONTEND_IO_COMPLETE_RA).word & 3u) {
    stop(run, UINT32_C(0x800392f0), 0,
         REG(NBA97_FRONTEND_IO_COMPLETE_RA).word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  run->out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}

int nba97_frontend_io_complete(Nba97FrontendIoCompleteContext *context,
                               Nba97FrontendIoCompleteProgress *out) {
  Run storage;
  Run *run = &storage;
  Word branch_value;
  int decision;
  unsigned slot;
  TRY(initialize(context, out, run));

  /* 0x800392A0..0x800392B8: the active decision uses the loaded v0, but its
   * delay slot clears a0 even when knownness prevents the branch decision. */
  STEP(0x800392a0);
  set_known(&REG(NBA97_FRONTEND_IO_COMPLETE_V0), UINT32_C(0x80100000));
  STEP(0x800392a4);
  TRY(load_word(run, UINT32_C(0x800392a4), UINT32_C(0x800f84c4),
                &REG(NBA97_FRONTEND_IO_COMPLETE_V0)));
  out->active_word = REG(NBA97_FRONTEND_IO_COMPLETE_V0);
  STEP(0x800392a8);
  branch_value = REG(NBA97_FRONTEND_IO_COMPLETE_V0);
  STEP(0x800392ac);
  STEP(0x800392b0);
  set_known(&REG(NBA97_FRONTEND_IO_COMPLETE_A0), 0);
  TRY(nonzero_decision(run, branch_value, UINT32_C(0x800392ac), &decision));
  if (!decision) {
    STEP(0x800392b4);
    STEP(0x800392b8);
    set_known(&REG(NBA97_FRONTEND_IO_COMPLETE_V0), 1);
    return finish(run);
  }

  STEP(0x800392c4);
  set_known(&REG(NBA97_FRONTEND_IO_COMPLETE_V1), 0);
  for (slot = 0; slot < 8; ++slot) {
    uint32_t address = UINT32_C(0x800ef840) + slot * UINT32_C(0x24);
    /* 0x800392C8..0x800392DC: a0 increments in the status BNE delay even
     * when the loaded status is nonzero or its branch is unknowable. */
    STEP(0x800392c8);
    set_known(&REG(NBA97_FRONTEND_IO_COMPLETE_AT), UINT32_C(0x800f0000));
    STEP(0x800392cc);
    set_known(&REG(NBA97_FRONTEND_IO_COMPLETE_AT), address + UINT32_C(0x7c0));
    STEP(0x800392d0);
    TRY(load_word(run, UINT32_C(0x800392d0), address,
                  &REG(NBA97_FRONTEND_IO_COMPLETE_V0)));
    out->last_status = REG(NBA97_FRONTEND_IO_COMPLETE_V0);
    ++out->status_reads;
    ++out->slots_examined;
    STEP(0x800392d4);
    branch_value = REG(NBA97_FRONTEND_IO_COMPLETE_V0);
    STEP(0x800392d8);
    STEP(0x800392dc);
    set_known(&REG(NBA97_FRONTEND_IO_COMPLETE_A0), slot + 1u);
    TRY(nonzero_decision(run, branch_value, UINT32_C(0x800392d8),
                         &decision));
    if (decision) {
      STEP(0x800392bc);
      STEP(0x800392c0);
      set_known(&REG(NBA97_FRONTEND_IO_COMPLETE_V0), 0);
      return finish(run);
    }

    /* 0x800392E0..0x800392E8: source a0 is exact here; the loop delay
     * advances v1 after the eighth slot even though the loop falls through. */
    STEP(0x800392e0);
    set_known(&REG(NBA97_FRONTEND_IO_COMPLETE_V0), slot + 1u < 8u);
    STEP(0x800392e4);
    STEP(0x800392e8);
    set_known(&REG(NBA97_FRONTEND_IO_COMPLETE_V1),
              (slot + 1u) * UINT32_C(0x24));
  }
  STEP(0x800392ec);
  set_known(&REG(NBA97_FRONTEND_IO_COMPLETE_V0), 1);
  return finish(run);
}
