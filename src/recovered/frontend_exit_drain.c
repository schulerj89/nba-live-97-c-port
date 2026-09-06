#include "frontend_exit_drain.h"

#include <string.h>

typedef Nba97FrontendExitDrainWord Word;

typedef struct Run {
  Nba97FrontendExitDrainContext *context;
  Nba97FrontendExitDrainProgress *out;
  Nba97FrontendExitDrainMachine machine;
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

static int machine_valid(const Nba97FrontendExitDrainMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_FRONTEND_EXIT_DRAIN_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int initialize(Nba97FrontendExitDrainContext *context,
                      Nba97FrontendExitDrainProgress *out, Run *run) {
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

static Word add_constant(Word input, uint32_t constant) {
  Word result;
  unsigned byte;
  unsigned carry_mask = 1u;
  result.word = input.word + constant;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_carry_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned carry;
    unsigned start = (input.known_mask & (1u << byte))
                         ? ((input.word >> (byte * 8u)) & 0xffu)
                         : 0u;
    unsigned end = (input.known_mask & (1u << byte)) ? start : 255u;
    unsigned addend = (constant >> (byte * 8u)) & 0xffu;
    for (carry = 0; carry < 2; ++carry) {
      unsigned source;
      if (!(carry_mask & (1u << carry)))
        continue;
      for (source = start; source <= end; ++source) {
        unsigned sum = source + addend + carry;
        unsigned output = sum & 0xffu;
        next_carry_mask |= 1u << (sum >> 8u);
        if (first) {
          first_output = output;
          first = 0;
        } else if (output != first_output) {
          invariant = 0;
        }
      }
    }
    if (invariant)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
    carry_mask = next_carry_mask;
  }
  return result;
}

static void set_known(Word *word, uint32_t value) {
  word->word = value;
  word->known_mask = 0x0fu;
}

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    const Word *value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97FrontendExitDrainAccess *event = &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word;
    event->operation = run->out->operations;
    event->width = 4;
    event->known_mask = value->known_mask;
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t address, uint32_t pc, uint8_t **data,
                  uint8_t **known_bytes) {
  size_t i;
  unsigned byte;
  stop(run, pc, address, 0);
  TRY(spend(run));
  ++run->out->accesses;
  if (address & 3u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0; i < run->context->memory.count; ++i) {
    Nba97GameTextRegion *region = &run->context->memory.region[i];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        4u > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known_bytes = region->known ? region->known + (size_t)offset : 0;
    if (*known_bytes)
      for (byte = 0; byte < 4; ++byte)
        if ((*known_bytes)[byte] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int address(Run *run, unsigned base, uint32_t offset, uint32_t pc,
                   uint32_t *result) {
  Word computed = add_constant(REG(base), offset);
  if (computed.known_mask != 0x0fu) {
    stop(run, pc, computed.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = computed.word;
  return NBA97_TEXT_COMPLETE;
}

static int store_word(Run *run, unsigned source, unsigned base,
                      uint32_t offset, uint32_t pc) {
  uint32_t guest;
  uint8_t *data;
  uint8_t *known_bytes;
  unsigned byte;
  TRY(address(run, base, offset, pc, &guest));
  TRY(locate(run, guest, pc, &data, &known_bytes));
  if (!known_bytes && REG(source).known_mask != 0x0fu)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0; byte < 4; ++byte) {
    data[byte] = (uint8_t)(REG(source).word >> (byte * 8u));
    if (known_bytes)
      known_bytes[byte] = (uint8_t)((REG(source).known_mask >> byte) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_FRONTEND_EXIT_DRAIN_STORE, pc, guest, &REG(source));
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int load_word(Run *run, unsigned destination, unsigned base,
                     uint32_t offset, uint32_t pc) {
  uint32_t guest;
  uint8_t *data;
  uint8_t *known_bytes;
  Word loaded = {0, 0};
  unsigned byte;
  TRY(address(run, base, offset, pc, &guest));
  TRY(locate(run, guest, pc, &data, &known_bytes));
  for (byte = 0; byte < 4; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (byte * 8u);
    if (!known_bytes || known_bytes[byte])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << byte));
  }
  REG(destination) = loaded;
  ++run->out->reads;
  journal(run, NBA97_FRONTEND_EXIT_DRAIN_READ, pc, guest, &loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int zero_decision(Run *run, Word value, uint32_t pc, int *is_zero) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((value.known_mask & (1u << byte)) &&
        ((value.word >> (byte * 8u)) & 0xffu)) {
      *is_zero = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (value.known_mask == 0x0fu) {
    *is_zero = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

static int invoke(Run *run, uint8_t site, uint32_t pc, uint32_t delay_pc,
                  uint32_t target, uint8_t argument_count) {
  Nba97FrontendExitDrainEvent event;
  int accepted;
  stop(run, pc, 0, target);
  TRY(spend(run));
  ++run->out->call_attempts[site];
  memset(&event, 0, sizeof event);
  event.pc = pc;
  event.delay_slot_pc = delay_pc;
  event.entry = target;
  event.operation = run->out->operations;
  event.invocation = run->out->call_attempts[site];
  event.site = site;
  event.argument_count = argument_count;
  event.target_program = NBA97_FRONTEND_MAIN_PROGRAM_FEONLY;
  publish(run);
  if (!run->context->io)
    return NBA97_TEXT_IO_REFUSED;
  accepted = run->context->io(run->context->user, &run->context->memory,
                              &event, &run->machine);
  publish(run);
  if (accepted != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!machine_valid(&run->machine))
    return NBA97_TEXT_ARGUMENT;
  ++run->out->callbacks_completed;
  ++run->out->call_count[site];
  return NBA97_TEXT_COMPLETE;
}

int nba97_frontend_exit_drain(Nba97FrontendExitDrainContext *context,
                              Nba97FrontendExitDrainProgress *out) {
  Run storage;
  Run *run = &storage;
  int decision;
  TRY(initialize(context, out, run));

  /* 0x800394D4..0x800394E4: the source reads the active flag before changing
   * sp. The taken branch still executes the saved-ra store in its delay. */
  STEP(0x800394d4);
  set_known(&REG(NBA97_FRONTEND_EXIT_DRAIN_V0), UINT32_C(0x80100000));
  STEP(0x800394d8);
  TRY(load_word(run, NBA97_FRONTEND_EXIT_DRAIN_V0,
                NBA97_FRONTEND_EXIT_DRAIN_V0, UINT32_C(0xffff84c4),
                UINT32_C(0x800394d8)));
  out->initial_active_flag = REG(NBA97_FRONTEND_EXIT_DRAIN_V0);
  STEP(0x800394dc);
  REG(NBA97_FRONTEND_EXIT_DRAIN_SP) = add_constant(
      REG(NBA97_FRONTEND_EXIT_DRAIN_SP), UINT32_C(0xffffffe8));
  out->frame_stack_pointer = REG(NBA97_FRONTEND_EXIT_DRAIN_SP).word;
  publish(run);
  STEP(0x800394e0);
  STEP(0x800394e4);
  TRY(store_word(run, NBA97_FRONTEND_EXIT_DRAIN_RA,
                 NBA97_FRONTEND_EXIT_DRAIN_SP, 16,
                 UINT32_C(0x800394e4)));
  out->saved_return_address = REG(NBA97_FRONTEND_EXIT_DRAIN_RA);
  TRY(zero_decision(run, out->initial_active_flag, UINT32_C(0x800394e0),
                    &decision));
  if (!decision) {
    /* 0x800394E8..0x8003950C: begin the drain, then poll until nonzero. Each
     * zero result runs 0x80038E84 and jumps back through a NOP delay slot. */
    STEP(0x800394e8);
    set_known(&REG(NBA97_FRONTEND_EXIT_DRAIN_RA), UINT32_C(0x800394f0));
    STEP(0x800394ec);
    TRY(invoke(run, NBA97_FRONTEND_EXIT_DRAIN_SITE_800394E8,
               UINT32_C(0x800394e8), UINT32_C(0x800394ec),
               UINT32_C(0x800393f0), 0));
    for (;;) {
      STEP(0x800394f0);
      set_known(&REG(NBA97_FRONTEND_EXIT_DRAIN_RA), UINT32_C(0x800394f8));
      STEP(0x800394f4);
      ++out->poll_attempts;
      TRY(invoke(run, NBA97_FRONTEND_EXIT_DRAIN_SITE_800394F0,
                 UINT32_C(0x800394f0), UINT32_C(0x800394f4),
                 UINT32_C(0x800392a0), 0));
      STEP(0x800394f8);
      STEP(0x800394fc);
      TRY(zero_decision(run, REG(NBA97_FRONTEND_EXIT_DRAIN_V0),
                        UINT32_C(0x800394f8), &decision));
      if (!decision)
        break;
      ++out->zero_poll_results;
      STEP(0x80039500);
      set_known(&REG(NBA97_FRONTEND_EXIT_DRAIN_RA), UINT32_C(0x80039508));
      STEP(0x80039504);
      TRY(invoke(run, NBA97_FRONTEND_EXIT_DRAIN_SITE_80039500,
                 UINT32_C(0x80039500), UINT32_C(0x80039504),
                 UINT32_C(0x80038e84), 0));
      STEP(0x80039508);
      STEP(0x8003950c);
    }

    /* 0x80039510..0x80039534: snapshot 0x8002149C before either flag clear.
     * a0 is cleared in the branch delay on both paths and a1 in the JAL delay. */
    STEP(0x80039510);
    set_known(&REG(NBA97_FRONTEND_EXIT_DRAIN_V0), UINT32_C(0x80020000));
    STEP(0x80039514);
    TRY(load_word(run, NBA97_FRONTEND_EXIT_DRAIN_V0,
                  NBA97_FRONTEND_EXIT_DRAIN_V0, UINT32_C(0x149c),
                  UINT32_C(0x80039514)));
    out->first_mode_flag = REG(NBA97_FRONTEND_EXIT_DRAIN_V0);
    STEP(0x80039518);
    set_known(&REG(NBA97_FRONTEND_EXIT_DRAIN_AT), UINT32_C(0x800f0000));
    STEP(0x8003951c);
    TRY(store_word(run, NBA97_FRONTEND_EXIT_DRAIN_ZERO,
                   NBA97_FRONTEND_EXIT_DRAIN_AT, UINT32_C(0x43b0),
                   UINT32_C(0x8003951c)));
    STEP(0x80039520);
    set_known(&REG(NBA97_FRONTEND_EXIT_DRAIN_AT), UINT32_C(0x80100000));
    STEP(0x80039524);
    TRY(store_word(run, NBA97_FRONTEND_EXIT_DRAIN_ZERO,
                   NBA97_FRONTEND_EXIT_DRAIN_AT, UINT32_C(0xffff84c4),
                   UINT32_C(0x80039524)));
    STEP(0x80039528);
    STEP(0x8003952c);
    set_known(&REG(NBA97_FRONTEND_EXIT_DRAIN_A0), 0);
    publish(run);
    TRY(zero_decision(run, out->first_mode_flag, UINT32_C(0x80039528),
                      &decision));
    if (!decision) {
      STEP(0x80039530);
      set_known(&REG(NBA97_FRONTEND_EXIT_DRAIN_RA), UINT32_C(0x80039538));
      STEP(0x80039534);
      set_known(&REG(NBA97_FRONTEND_EXIT_DRAIN_A1), 0);
      publish(run);
      TRY(invoke(run, NBA97_FRONTEND_EXIT_DRAIN_SITE_80039530,
                 UINT32_C(0x80039530), UINT32_C(0x80039534),
                 UINT32_C(0x80029b64), 2));
    }

    /* 0x80039538..0x80039560: 0x8008C274 always runs. Reload the mode global
     * after that callback; only its second value controls the final two calls. */
    STEP(0x80039538);
    set_known(&REG(NBA97_FRONTEND_EXIT_DRAIN_RA), UINT32_C(0x80039540));
    STEP(0x8003953c);
    TRY(invoke(run, NBA97_FRONTEND_EXIT_DRAIN_SITE_80039538,
               UINT32_C(0x80039538), UINT32_C(0x8003953c),
               UINT32_C(0x8008c274), 0));
    STEP(0x80039540);
    set_known(&REG(NBA97_FRONTEND_EXIT_DRAIN_A0), UINT32_C(0x80020000));
    STEP(0x80039544);
    TRY(load_word(run, NBA97_FRONTEND_EXIT_DRAIN_A0,
                  NBA97_FRONTEND_EXIT_DRAIN_A0, UINT32_C(0x149c),
                  UINT32_C(0x80039544)));
    out->second_mode_flag = REG(NBA97_FRONTEND_EXIT_DRAIN_A0);
    STEP(0x80039548);
    STEP(0x8003954c);
    STEP(0x80039550);
    TRY(zero_decision(run, out->second_mode_flag, UINT32_C(0x8003954c),
                      &decision));
    if (!decision) {
      STEP(0x80039554);
      set_known(&REG(NBA97_FRONTEND_EXIT_DRAIN_RA), UINT32_C(0x8003955c));
      STEP(0x80039558);
      TRY(invoke(run, NBA97_FRONTEND_EXIT_DRAIN_SITE_80039554,
                 UINT32_C(0x80039554), UINT32_C(0x80039558),
                 UINT32_C(0x8006cde4), 1));
      STEP(0x8003955c);
      set_known(&REG(NBA97_FRONTEND_EXIT_DRAIN_RA), UINT32_C(0x80039564));
      STEP(0x80039560);
      TRY(invoke(run, NBA97_FRONTEND_EXIT_DRAIN_SITE_8003955C,
                 UINT32_C(0x8003955c), UINT32_C(0x80039560),
                 UINT32_C(0x8006ae60), 0));
    }
  }

  /* 0x80039564..0x80039570: all paths restore ra through callback-live sp,
   * release the 24-byte frame with 32-bit wrap, then return through that word. */
  STEP(0x80039564);
  TRY(load_word(run, NBA97_FRONTEND_EXIT_DRAIN_RA,
                NBA97_FRONTEND_EXIT_DRAIN_SP, 16,
                UINT32_C(0x80039564)));
  out->restored_return_address = REG(NBA97_FRONTEND_EXIT_DRAIN_RA);
  STEP(0x80039568);
  REG(NBA97_FRONTEND_EXIT_DRAIN_SP) = add_constant(
      REG(NBA97_FRONTEND_EXIT_DRAIN_SP), 24);
  publish(run);
  STEP(0x8003956c);
  STEP(0x80039570);
  if (REG(NBA97_FRONTEND_EXIT_DRAIN_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x8003956c), 0,
         REG(NBA97_FRONTEND_EXIT_DRAIN_RA).word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (REG(NBA97_FRONTEND_EXIT_DRAIN_RA).word & 3u) {
    stop(run, UINT32_C(0x8003956c), 0,
         REG(NBA97_FRONTEND_EXIT_DRAIN_RA).word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
