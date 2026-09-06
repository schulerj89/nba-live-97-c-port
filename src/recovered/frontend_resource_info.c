#include "frontend_resource_info.h"

#include <string.h>

typedef Nba97FrontendResourceInfoWord Word;

typedef struct Run {
  Nba97FrontendResourceInfoContext *context;
  Nba97FrontendResourceInfoProgress *out;
  Nba97FrontendResourceInfoMachine machine;
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

static int machine_valid(const Nba97FrontendResourceInfoMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_FRONTEND_RESOURCE_INFO_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int initialize(Nba97FrontendResourceInfoContext *context,
                      Nba97FrontendResourceInfoProgress *out, Run *run) {
  size_t i;
  size_t j;
  if (!out)
    return NBA97_TEXT_ARGUMENT;
  memset(out, 0, sizeof *out);
  if (!context || (!context->memory.region && context->memory.count) ||
      (!context->access_journal && context->access_journal_capacity) ||
      (!context->instruction_journal &&
       context->instruction_journal_capacity) ||
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
    Nba97FrontendResourceInfoAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word;
    event->operation = run->out->operations;
    event->width = 4;
    event->known_mask = value->known_mask;
    event->kind = kind;
  }
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

static int locate(Run *run, uint32_t guest, uint32_t pc, uint8_t **data,
                  uint8_t **known) {
  size_t i;
  unsigned byte;
  stop(run, pc, guest, 0);
  TRY(spend(run));
  ++run->out->accesses;
  if (guest & 3u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0; i < run->context->memory.count; ++i) {
    Nba97GameTextRegion *region = &run->context->memory.region[i];
    uint64_t offset = (uint64_t)guest - region->base;
    if (guest < region->base || offset > region->size ||
        4u > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known = region->known ? region->known + (size_t)offset : 0;
    if (*known)
      for (byte = 0; byte < 4; ++byte)
        if ((*known)[byte] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int store_word(Run *run, unsigned source, unsigned base,
                      uint32_t offset, uint32_t pc) {
  uint32_t guest;
  uint8_t *data;
  uint8_t *known;
  unsigned byte;
  TRY(address(run, base, offset, pc, &guest));
  TRY(locate(run, guest, pc, &data, &known));
  if (!known && REG(source).known_mask != 0x0fu)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0; byte < 4; ++byte) {
    data[byte] = (uint8_t)(REG(source).word >> (byte * 8u));
    if (known)
      known[byte] = (uint8_t)((REG(source).known_mask >> byte) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_FRONTEND_RESOURCE_INFO_STORE, pc, guest, &REG(source));
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int load_word(Run *run, unsigned destination, unsigned base,
                     uint32_t offset, uint32_t pc) {
  uint32_t guest;
  uint8_t *data;
  uint8_t *known;
  Word loaded = {0, 0};
  unsigned byte;
  TRY(address(run, base, offset, pc, &guest));
  TRY(locate(run, guest, pc, &data, &known));
  for (byte = 0; byte < 4; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (byte * 8u);
    if (!known || known[byte])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << byte));
  }
  REG(destination) = loaded;
  ++run->out->reads;
  journal(run, NBA97_FRONTEND_RESOURCE_INFO_READ, pc, guest, &loaded);
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

static int positive_decision(Run *run, Word value, uint32_t pc,
                             int *is_positive) {
  unsigned byte;
  if (!(value.known_mask & 8u)) {
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (value.word & UINT32_C(0x80000000)) {
    *is_positive = 0;
    return NBA97_TEXT_COMPLETE;
  }
  for (byte = 0; byte < 4; ++byte)
    if ((value.known_mask & (1u << byte)) &&
        ((value.word >> (byte * 8u)) & 0xffu)) {
      *is_positive = 1;
      return NBA97_TEXT_COMPLETE;
    }
  if (value.known_mask == 0x0fu) {
    *is_positive = 0;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

static int invoke(Run *run, uint8_t site, uint32_t pc, uint32_t delay_pc,
                  uint32_t target, uint8_t argument_count) {
  Nba97FrontendResourceInfoEvent event;
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

static int restore_and_return(Run *run) {
  /* 0x8008A6BC..0x8008A6E8: all restores use the callback-live stack. */
  STEP(0x8008a6bc);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_INFO_RA,
                NBA97_FRONTEND_RESOURCE_INFO_SP, 344,
                UINT32_C(0x8008a6bc)));
  run->out->restored_return_address = REG(NBA97_FRONTEND_RESOURCE_INFO_RA);
  STEP(0x8008a6c0);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_INFO_S7,
                NBA97_FRONTEND_RESOURCE_INFO_SP, 340,
                UINT32_C(0x8008a6c0)));
  run->out->restored_s7 = REG(NBA97_FRONTEND_RESOURCE_INFO_S7);
  STEP(0x8008a6c4);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_INFO_S6,
                NBA97_FRONTEND_RESOURCE_INFO_SP, 336,
                UINT32_C(0x8008a6c4)));
  run->out->restored_s6 = REG(NBA97_FRONTEND_RESOURCE_INFO_S6);
  STEP(0x8008a6c8);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_INFO_S5,
                NBA97_FRONTEND_RESOURCE_INFO_SP, 332,
                UINT32_C(0x8008a6c8)));
  run->out->restored_s5 = REG(NBA97_FRONTEND_RESOURCE_INFO_S5);
  STEP(0x8008a6cc);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_INFO_S4,
                NBA97_FRONTEND_RESOURCE_INFO_SP, 328,
                UINT32_C(0x8008a6cc)));
  run->out->restored_s4 = REG(NBA97_FRONTEND_RESOURCE_INFO_S4);
  STEP(0x8008a6d0);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_INFO_S3,
                NBA97_FRONTEND_RESOURCE_INFO_SP, 324,
                UINT32_C(0x8008a6d0)));
  run->out->restored_s3 = REG(NBA97_FRONTEND_RESOURCE_INFO_S3);
  STEP(0x8008a6d4);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_INFO_S2,
                NBA97_FRONTEND_RESOURCE_INFO_SP, 320,
                UINT32_C(0x8008a6d4)));
  run->out->restored_s2 = REG(NBA97_FRONTEND_RESOURCE_INFO_S2);
  STEP(0x8008a6d8);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_INFO_S1,
                NBA97_FRONTEND_RESOURCE_INFO_SP, 316,
                UINT32_C(0x8008a6d8)));
  run->out->restored_s1 = REG(NBA97_FRONTEND_RESOURCE_INFO_S1);
  STEP(0x8008a6dc);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_INFO_S0,
                NBA97_FRONTEND_RESOURCE_INFO_SP, 312,
                UINT32_C(0x8008a6dc)));
  run->out->restored_s0 = REG(NBA97_FRONTEND_RESOURCE_INFO_S0);
  STEP(0x8008a6e0);
  REG(NBA97_FRONTEND_RESOURCE_INFO_SP) =
      add_constant(REG(NBA97_FRONTEND_RESOURCE_INFO_SP), 352);
  publish(run);
  STEP(0x8008a6e4);
  STEP(0x8008a6e8);
  if (REG(NBA97_FRONTEND_RESOURCE_INFO_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x8008a6e4), 0,
         REG(NBA97_FRONTEND_RESOURCE_INFO_RA).word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (REG(NBA97_FRONTEND_RESOURCE_INFO_RA).word & 3u) {
    stop(run, UINT32_C(0x8008a6e4), 0,
         REG(NBA97_FRONTEND_RESOURCE_INFO_RA).word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  run->out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}

int nba97_frontend_resource_info(Nba97FrontendResourceInfoContext *context,
                                 Nba97FrontendResourceInfoProgress *out) {
  Run storage;
  Run *run = &storage;
  int decision;
  int positive;
  TRY(initialize(context, out, run));
  out->input_filename = REG(NBA97_FRONTEND_RESOURCE_INFO_A0);
  out->input_handle_pointer = REG(NBA97_FRONTEND_RESOURCE_INFO_A1);
  out->input_other_pointer = REG(NBA97_FRONTEND_RESOURCE_INFO_A2);
  out->input_size_pointer = REG(NBA97_FRONTEND_RESOURCE_INFO_A3);

  /* 0x8008A594..0x8008A5EC: allocate the 352-byte frame, preserve s0-s7,
   * read the fifth argument through frame+368, and save s3 in the first JAL
   * delay slot after ra has become 0x8008A5F0. */
  STEP(0x8008a594);
  REG(NBA97_FRONTEND_RESOURCE_INFO_SP) = add_constant(
      REG(NBA97_FRONTEND_RESOURCE_INFO_SP), UINT32_C(0xfffffea0));
  out->frame_stack_pointer = REG(NBA97_FRONTEND_RESOURCE_INFO_SP).word;
  publish(run);
  STEP(0x8008a598);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_S5,
                 NBA97_FRONTEND_RESOURCE_INFO_SP, 332,
                 UINT32_C(0x8008a598)));
  out->saved_s5 = REG(NBA97_FRONTEND_RESOURCE_INFO_S5);
  STEP(0x8008a59c);
  REG(NBA97_FRONTEND_RESOURCE_INFO_S5) = REG(NBA97_FRONTEND_RESOURCE_INFO_A0);
  STEP(0x8008a5a0);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_S0,
                 NBA97_FRONTEND_RESOURCE_INFO_SP, 312,
                 UINT32_C(0x8008a5a0)));
  out->saved_s0 = REG(NBA97_FRONTEND_RESOURCE_INFO_S0);
  STEP(0x8008a5a4);
  REG(NBA97_FRONTEND_RESOURCE_INFO_S0) = REG(NBA97_FRONTEND_RESOURCE_INFO_A1);
  STEP(0x8008a5a8);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_S6,
                 NBA97_FRONTEND_RESOURCE_INFO_SP, 336,
                 UINT32_C(0x8008a5a8)));
  out->saved_s6 = REG(NBA97_FRONTEND_RESOURCE_INFO_S6);
  STEP(0x8008a5ac);
  REG(NBA97_FRONTEND_RESOURCE_INFO_S6) = REG(NBA97_FRONTEND_RESOURCE_INFO_A2);
  STEP(0x8008a5b0);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_S4,
                 NBA97_FRONTEND_RESOURCE_INFO_SP, 328,
                 UINT32_C(0x8008a5b0)));
  out->saved_s4 = REG(NBA97_FRONTEND_RESOURCE_INFO_S4);
  STEP(0x8008a5b4);
  REG(NBA97_FRONTEND_RESOURCE_INFO_S4) = REG(NBA97_FRONTEND_RESOURCE_INFO_A3);
  STEP(0x8008a5b8);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_S1,
                 NBA97_FRONTEND_RESOURCE_INFO_SP, 316,
                 UINT32_C(0x8008a5b8)));
  out->saved_s1 = REG(NBA97_FRONTEND_RESOURCE_INFO_S1);
  STEP(0x8008a5bc);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_S1), 0);
  STEP(0x8008a5c0);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_S2,
                 NBA97_FRONTEND_RESOURCE_INFO_SP, 320,
                 UINT32_C(0x8008a5c0)));
  out->saved_s2 = REG(NBA97_FRONTEND_RESOURCE_INFO_S2);
  STEP(0x8008a5c4);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_S2), 10);
  STEP(0x8008a5c8);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_S7,
                 NBA97_FRONTEND_RESOURCE_INFO_SP, 340,
                 UINT32_C(0x8008a5c8)));
  out->saved_s7 = REG(NBA97_FRONTEND_RESOURCE_INFO_S7);
  STEP(0x8008a5cc);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_INFO_S7,
                NBA97_FRONTEND_RESOURCE_INFO_SP, 368,
                UINT32_C(0x8008a5cc)));
  out->input_fifth_argument = REG(NBA97_FRONTEND_RESOURCE_INFO_S7);
  STEP(0x8008a5d0);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_A0), UINT32_C(0x800e0000));
  STEP(0x8008a5d4);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_A0), UINT32_C(0x800d96a8));
  STEP(0x8008a5d8);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_A1), UINT32_C(0x800e0000));
  STEP(0x8008a5dc);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_A1), UINT32_C(0x800d9a58));
  STEP(0x8008a5e0);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_A2), 6);
  STEP(0x8008a5e4);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_RA,
                 NBA97_FRONTEND_RESOURCE_INFO_SP, 344,
                 UINT32_C(0x8008a5e4)));
  out->saved_return_address = REG(NBA97_FRONTEND_RESOURCE_INFO_RA);
  STEP(0x8008a5e8);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_RA), UINT32_C(0x8008a5f0));
  STEP(0x8008a5ec);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_S3,
                 NBA97_FRONTEND_RESOURCE_INFO_SP, 324,
                 UINT32_C(0x8008a5ec)));
  out->saved_s3 = REG(NBA97_FRONTEND_RESOURCE_INFO_S3);
  TRY(invoke(run, NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A5E8,
             UINT32_C(0x8008a5e8), UINT32_C(0x8008a5ec),
             UINT32_C(0x80084910), 3));
  out->prefix_result = REG(NBA97_FRONTEND_RESOURCE_INFO_V0);

  /* 0x8008A5F0..0x8008A61C: the branch delay always forms frame+24. The
   * equal route forwards four registers and two stack arguments. */
  STEP(0x8008a5f0);
  STEP(0x8008a5f4);
  REG(NBA97_FRONTEND_RESOURCE_INFO_A0) =
      add_constant(REG(NBA97_FRONTEND_RESOURCE_INFO_SP), 24);
  TRY(zero_decision(run, REG(NBA97_FRONTEND_RESOURCE_INFO_V0),
                    UINT32_C(0x8008a5f0), &decision));
  if (decision) {
    out->delegated_path = 1;
    STEP(0x8008a5f8);
    REG(NBA97_FRONTEND_RESOURCE_INFO_A0) = REG(NBA97_FRONTEND_RESOURCE_INFO_S5);
    STEP(0x8008a5fc);
    REG(NBA97_FRONTEND_RESOURCE_INFO_A1) = REG(NBA97_FRONTEND_RESOURCE_INFO_S0);
    STEP(0x8008a600);
    REG(NBA97_FRONTEND_RESOURCE_INFO_A2) = REG(NBA97_FRONTEND_RESOURCE_INFO_S6);
    STEP(0x8008a604);
    REG(NBA97_FRONTEND_RESOURCE_INFO_A3) = REG(NBA97_FRONTEND_RESOURCE_INFO_S4);
    STEP(0x8008a608);
    REG(NBA97_FRONTEND_RESOURCE_INFO_V0) =
        add_constant(REG(NBA97_FRONTEND_RESOURCE_INFO_SP), 304);
    STEP(0x8008a60c);
    TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_V0,
                   NBA97_FRONTEND_RESOURCE_INFO_SP, 16,
                   UINT32_C(0x8008a60c)));
    STEP(0x8008a610);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_RA), UINT32_C(0x8008a618));
    STEP(0x8008a614);
    TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_S7,
                   NBA97_FRONTEND_RESOURCE_INFO_SP, 20,
                   UINT32_C(0x8008a614)));
    TRY(invoke(run, NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A610,
               UINT32_C(0x8008a610), UINT32_C(0x8008a614),
               UINT32_C(0x80074184), 6));
    STEP(0x8008a618);
    STEP(0x8008a61c);
    return restore_and_return(run);
  }

  /* 0x8008A620..0x8008A6B4: direct attempts retain callback mutations,
   * including s1/s3 on an open failure and s2 at the retry branch. */
  for (;;) {
    ++out->attempts_started;
    STEP(0x8008a620);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_A1), UINT32_C(0x800e0000));
    STEP(0x8008a624);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_A1), UINT32_C(0x800d9a60));
    STEP(0x8008a628);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_A2), UINT32_C(0x800e0000));
    STEP(0x8008a62c);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_A2), UINT32_C(0x800d96a8));
    STEP(0x8008a630);
    REG(NBA97_FRONTEND_RESOURCE_INFO_A3) = REG(NBA97_FRONTEND_RESOURCE_INFO_S5);
    STEP(0x8008a634);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_ZERO), 0);
    TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_ZERO,
                   NBA97_FRONTEND_RESOURCE_INFO_S0, 0,
                   UINT32_C(0x8008a634)));
    STEP(0x8008a638);
    TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_ZERO,
                   NBA97_FRONTEND_RESOURCE_INFO_S6, 0,
                   UINT32_C(0x8008a638)));
    STEP(0x8008a63c);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_RA), UINT32_C(0x8008a644));
    STEP(0x8008a640);
    TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_ZERO,
                   NBA97_FRONTEND_RESOURCE_INFO_S4, 0,
                   UINT32_C(0x8008a640)));
    TRY(invoke(run, NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A63C,
               UINT32_C(0x8008a63c), UINT32_C(0x8008a640),
               UINT32_C(0x80083b70), 4));
    STEP(0x8008a644);
    REG(NBA97_FRONTEND_RESOURCE_INFO_A0) =
        add_constant(REG(NBA97_FRONTEND_RESOURCE_INFO_SP), 24);
    STEP(0x8008a648);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_RA), UINT32_C(0x8008a650));
    STEP(0x8008a64c);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_A1), 1);
    TRY(invoke(run, NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A648,
               UINT32_C(0x8008a648), UINT32_C(0x8008a64c),
               UINT32_C(0x8007f588), 2));
    out->open_result = REG(NBA97_FRONTEND_RESOURCE_INFO_V0);
    STEP(0x8008a650);
    STEP(0x8008a654);
    TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_V0,
                   NBA97_FRONTEND_RESOURCE_INFO_S0, 0,
                   UINT32_C(0x8008a654)));
    TRY(positive_decision(run, REG(NBA97_FRONTEND_RESOURCE_INFO_V0),
                          UINT32_C(0x8008a650), &positive));
    if (positive) {
      STEP(0x8008a658);
      set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_RA), UINT32_C(0x8008a660));
      STEP(0x8008a65c);
      REG(NBA97_FRONTEND_RESOURCE_INFO_A0) = REG(NBA97_FRONTEND_RESOURCE_INFO_V0);
      TRY(invoke(run, NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A658,
                 UINT32_C(0x8008a658), UINT32_C(0x8008a65c),
                 UINT32_C(0x8008a408), 1));
      STEP(0x8008a660);
      TRY(load_word(run, NBA97_FRONTEND_RESOURCE_INFO_A0,
                    NBA97_FRONTEND_RESOURCE_INFO_S0, 0,
                    UINT32_C(0x8008a660)));
      STEP(0x8008a664);
      REG(NBA97_FRONTEND_RESOURCE_INFO_S1) = REG(NBA97_FRONTEND_RESOURCE_INFO_V0);
      out->info_result = REG(NBA97_FRONTEND_RESOURCE_INFO_S1);
      STEP(0x8008a668);
      set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_A1), 0);
      STEP(0x8008a66c);
      set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_RA), UINT32_C(0x8008a674));
      STEP(0x8008a670);
      set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_A2), 0);
      TRY(invoke(run, NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A66C,
                 UINT32_C(0x8008a66c), UINT32_C(0x8008a670),
                 UINT32_C(0x8007f318), 3));
      STEP(0x8008a674);
      REG(NBA97_FRONTEND_RESOURCE_INFO_S3) = REG(NBA97_FRONTEND_RESOURCE_INFO_V0);
      out->seek_result = REG(NBA97_FRONTEND_RESOURCE_INFO_S3);
    }
    STEP(0x8008a678);
    STEP(0x8008a67c);
    TRY(zero_decision(run, REG(NBA97_FRONTEND_RESOURCE_INFO_S1),
                      UINT32_C(0x8008a678), &decision));
    if (!decision) {
      STEP(0x8008a680);
      STEP(0x8008a684);
      TRY(zero_decision(run, REG(NBA97_FRONTEND_RESOURCE_INFO_S3),
                        UINT32_C(0x8008a680), &decision));
      if (decision) {
        STEP(0x8008a6ac);
        set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_S2), 0);
        out->direct_path_success = 1;
        STEP(0x8008a6b0);
        STEP(0x8008a6b4);
        REG(NBA97_FRONTEND_RESOURCE_INFO_A0) =
            add_constant(REG(NBA97_FRONTEND_RESOURCE_INFO_SP), 24);
        break;
      }
    }

    ++out->failed_attempts;
    STEP(0x8008a688);
    TRY(load_word(run, NBA97_FRONTEND_RESOURCE_INFO_A0,
                  NBA97_FRONTEND_RESOURCE_INFO_S0, 0,
                  UINT32_C(0x8008a688)));
    STEP(0x8008a68c);
    STEP(0x8008a690);
    STEP(0x8008a694);
    REG(NBA97_FRONTEND_RESOURCE_INFO_S2) =
        add_constant(REG(NBA97_FRONTEND_RESOURCE_INFO_S2),
                     UINT32_C(0xffffffff));
    TRY(positive_decision(run, REG(NBA97_FRONTEND_RESOURCE_INFO_A0),
                          UINT32_C(0x8008a690), &positive));
    if (positive) {
      STEP(0x8008a698);
      set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_RA), UINT32_C(0x8008a6a0));
      STEP(0x8008a69c);
      TRY(invoke(run, NBA97_FRONTEND_RESOURCE_INFO_SITE_8008A698,
                 UINT32_C(0x8008a698), UINT32_C(0x8008a69c),
                 UINT32_C(0x8008a7b0), 1));
    }
    STEP(0x8008a6a0);
    TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_ZERO,
                   NBA97_FRONTEND_RESOURCE_INFO_S0, 0,
                   UINT32_C(0x8008a6a0)));
    STEP(0x8008a6a4);
    STEP(0x8008a6a8);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_INFO_S1), 0);
    STEP(0x8008a6b0);
    STEP(0x8008a6b4);
    REG(NBA97_FRONTEND_RESOURCE_INFO_A0) =
        add_constant(REG(NBA97_FRONTEND_RESOURCE_INFO_SP), 24);
    TRY(zero_decision(run, REG(NBA97_FRONTEND_RESOURCE_INFO_S2),
                      UINT32_C(0x8008a6b0), &decision));
    if (decision)
      break;
  }

  STEP(0x8008a6b8);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_INFO_S1,
                 NBA97_FRONTEND_RESOURCE_INFO_S4, 0,
                 UINT32_C(0x8008a6b8)));
  out->published_size = REG(NBA97_FRONTEND_RESOURCE_INFO_S1);
  return restore_and_return(run);
}
