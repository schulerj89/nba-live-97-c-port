#include "frontend_resource_lookup.h"

#include <string.h>

typedef Nba97FrontendResourceLookupWord Word;

typedef struct Run {
  Nba97FrontendResourceLookupContext *context;
  Nba97FrontendResourceLookupProgress *out;
  Nba97FrontendResourceLookupMachine machine;
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

static int machine_valid(const Nba97FrontendResourceLookupMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_FRONTEND_RESOURCE_LOOKUP_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int initialize(Nba97FrontendResourceLookupContext *context,
                      Nba97FrontendResourceLookupProgress *out, Run *run) {
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
    Nba97FrontendResourceLookupAccess *event =
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

static int store_word(Run *run, unsigned source, unsigned base, uint32_t offset,
                      uint32_t pc) {
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
  journal(run, NBA97_FRONTEND_RESOURCE_LOOKUP_STORE, pc, guest, &REG(source));
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
  journal(run, NBA97_FRONTEND_RESOURCE_LOOKUP_READ, pc, guest, &loaded);
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
  Nba97FrontendResourceLookupEvent event;
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
  accepted = run->context->io(run->context->user, &run->context->memory, &event,
                              &run->machine);
  publish(run);
  if (accepted != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!machine_valid(&run->machine))
    return NBA97_TEXT_ARGUMENT;
  ++run->out->callbacks_completed;
  ++run->out->call_count[site];
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

static Word shift_left(Word value, unsigned amount) {
  Word result;
  uint32_t known_bits = 0;
  uint32_t shifted_known;
  unsigned byte;
  result.word = value.word << amount;
  for (byte = 0; byte < 4; ++byte)
    if (value.known_mask & (1u << byte))
      known_bits |= UINT32_C(255) << (byte * 8u);
  shifted_known =
      (known_bits << amount) | (amount ? (UINT32_C(1) << amount) - 1u : 0u);
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte)
    if (((shifted_known >> (byte * 8u)) & 255u) == 255u)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  return result;
}

static Word shift_right_arithmetic(Word value, unsigned amount) {
  Word result;
  unsigned byte;
  result.word = value.word >> amount;
  if (value.word & UINT32_C(0x80000000))
    result.word |= ~(UINT32_MAX >> amount);
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned low_bit = byte * 8u + amount;
    unsigned high_bit = low_bit + 7u;
    unsigned first_source = low_bit / 8u;
    unsigned last_source = high_bit < 32u ? high_bit / 8u : 3u;
    unsigned source;
    int all_known = 1;
    for (source = first_source; source <= last_source; ++source)
      if (!(value.known_mask & (1u << source)))
        all_known = 0;
    if (high_bit >= 32u && !(value.known_mask & 8u))
      all_known = 0;
    if (all_known)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static Word add_words(Word left, Word right) {
  Word result;
  unsigned byte;
  unsigned carry_mask = 1u;
  result.word = left.word + right.word;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned ls = (left.known_mask & (1u << byte))
                      ? ((left.word >> (byte * 8u)) & 255u)
                      : 0;
    unsigned le = (left.known_mask & (1u << byte)) ? ls : 255u;
    unsigned rs = (right.known_mask & (1u << byte))
                      ? ((right.word >> (byte * 8u)) & 255u)
                      : 0;
    unsigned re = (right.known_mask & (1u << byte)) ? rs : 255u;
    unsigned carry;
    for (carry = 0; carry < 2; ++carry) {
      unsigned a;
      if (!(carry_mask & (1u << carry)))
        continue;
      for (a = ls; a <= le; ++a) {
        unsigned b;
        for (b = rs; b <= re; ++b) {
          unsigned sum = a + b + carry;
          unsigned output = sum & 255u;
          next_mask |= 1u << (sum >> 8u);
          if (first) {
            first_output = output;
            first = 0;
          } else if (first_output != output) {
            invariant = 0;
          }
        }
      }
    }
    if (invariant)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
    carry_mask = next_mask;
  }
  return result;
}

int nba97_frontend_resource_lookup(Nba97FrontendResourceLookupContext *context,
                                   Nba97FrontendResourceLookupProgress *out) {
  Run storage;
  Run *run = &storage;
  int decision;
  TRY(initialize(context, out, run));
  out->input_filename = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A0);

  STEP(0x8008a2c8);
  REG(NBA97_FRONTEND_RESOURCE_LOOKUP_SP) = add_constant(
      REG(NBA97_FRONTEND_RESOURCE_LOOKUP_SP), UINT32_C(0xffffffd8));
  out->frame_stack_pointer = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_SP).word;
  publish(run);
  STEP(0x8008a2cc);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_S2,
                 NBA97_FRONTEND_RESOURCE_LOOKUP_SP, 24, UINT32_C(0x8008a2cc)));
  out->saved_s2 = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S2);
  STEP(0x8008a2d0);
  REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S2) =
      REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A0);
  STEP(0x8008a2d4);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_RA,
                 NBA97_FRONTEND_RESOURCE_LOOKUP_SP, 32, UINT32_C(0x8008a2d4)));
  out->saved_return_address = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_RA);
  STEP(0x8008a2d8);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_S3,
                 NBA97_FRONTEND_RESOURCE_LOOKUP_SP, 28, UINT32_C(0x8008a2d8)));
  out->saved_s3 = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S3);
  STEP(0x8008a2dc);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_S1,
                 NBA97_FRONTEND_RESOURCE_LOOKUP_SP, 20, UINT32_C(0x8008a2dc)));
  out->saved_s1 = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S1);
  STEP(0x8008a2e0);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_LOOKUP_RA), UINT32_C(0x8008a2e8));
  STEP(0x8008a2e4);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_S0,
                 NBA97_FRONTEND_RESOURCE_LOOKUP_SP, 16, UINT32_C(0x8008a2e4)));
  out->saved_s0 = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S0);
  TRY(invoke(run, NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A2E0,
             UINT32_C(0x8008a2e0), UINT32_C(0x8008a2e4), UINT32_C(0x8008a0a8),
             1));

  STEP(0x8008a2e8);
  REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S1) =
      REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0);
  out->initial_lookup = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S1);

  /* 0x8008A2EC..0x8008A308: the delay slot latches -9, and the flag-clear
   * store executes before the branch consumes the independently latched old
   * bit 0x10 decision. */
  STEP(0x8008a2ec);
  STEP(0x8008a2f0);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V1), UINT32_C(0xfffffff7));
  TRY(zero_decision(run, REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S1),
                    UINT32_C(0x8008a2ec), &decision));
  if (!decision) {
    STEP(0x8008a2f4);
    TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_V0,
                  NBA97_FRONTEND_RESOURCE_LOOKUP_S1, 24, UINT32_C(0x8008a2f4)));
    out->initial_flags = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0);
    STEP(0x8008a2f8);
    STEP(0x8008a2fc);
    REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A2) = and_constant(
        REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0), UINT32_C(0xfffffff7));
    out->cleared_flags = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A2);
    STEP(0x8008a300);
    REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0) =
        and_constant(out->initial_flags, UINT32_C(0x10));
    STEP(0x8008a304);
    STEP(0x8008a308);
    TRY(store_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_A2,
                   NBA97_FRONTEND_RESOURCE_LOOKUP_S1, 24,
                   UINT32_C(0x8008a308)));
    TRY(zero_decision(run, REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0),
                      UINT32_C(0x8008a304), &decision));
    if (decision) {
      STEP(0x8008a30c);
      TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_A1,
                    NBA97_FRONTEND_RESOURCE_LOOKUP_S1, 20,
                    UINT32_C(0x8008a30c)));
      STEP(0x8008a310);
      REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A0) =
          REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S2);
      STEP(0x8008a314);
      set_known(&REG(NBA97_FRONTEND_RESOURCE_LOOKUP_RA), UINT32_C(0x8008a31c));
      STEP(0x8008a318);
      set_known(&REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A3), 0);
      TRY(invoke(run, NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A314,
                 UINT32_C(0x8008a314), UINT32_C(0x8008a318),
                 UINT32_C(0x800771f0), 4));
      STEP(0x8008a31c);
      REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S0) =
          REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0);
      out->allocation_result = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S0);
      STEP(0x8008a320);
      STEP(0x8008a324);
      TRY(zero_decision(run, REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S0),
                        UINT32_C(0x8008a320), &decision));
      if (!decision) {
        STEP(0x8008a330);
        TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_A0,
                      NBA97_FRONTEND_RESOURCE_LOOKUP_S1, 0,
                      UINT32_C(0x8008a330)));
        out->descriptor_source = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A0);
        STEP(0x8008a334);
        TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_A1,
                      NBA97_FRONTEND_RESOURCE_LOOKUP_S0, 0,
                      UINT32_C(0x8008a334)));
        out->descriptor_destination = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A1);
        STEP(0x8008a338);
        TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_A2,
                      NBA97_FRONTEND_RESOURCE_LOOKUP_S1, 20,
                      UINT32_C(0x8008a338)));
        out->descriptor_length = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A2);
        STEP(0x8008a33c);
        set_known(&REG(NBA97_FRONTEND_RESOURCE_LOOKUP_RA),
                  UINT32_C(0x8008a344));
        STEP(0x8008a340);
        TRY(invoke(run, NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A33C,
                   UINT32_C(0x8008a33c), UINT32_C(0x8008a340),
                   UINT32_C(0x800909a8), 3));
        out->copied = 1;
        STEP(0x8008a344);
        set_known(&REG(NBA97_FRONTEND_RESOURCE_LOOKUP_RA),
                  UINT32_C(0x8008a34c));
        STEP(0x8008a348);
        REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A0) =
            REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S1);
        TRY(invoke(run, NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A344,
                   UINT32_C(0x8008a344), UINT32_C(0x8008a348),
                   UINT32_C(0x80077638), 1));
        out->freed = 1;
        STEP(0x8008a34c);
        STEP(0x8008a350);
        REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0) =
            REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S0);
      } else {
        STEP(0x8008a328);
        STEP(0x8008a32c);
        REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0) =
            REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S1);
      }
      goto epilogue;
    }
    STEP(0x8008a328);
    STEP(0x8008a32c);
    REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0) =
        REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S1);
    goto epilogue;
  }

  STEP(0x8008a354);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0), UINT32_C(0x800c0000));
  STEP(0x8008a358);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_V0,
                NBA97_FRONTEND_RESOURCE_LOOKUP_V0, UINT32_C(0x7374),
                UINT32_C(0x8008a358)));
  out->chain_root = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0);
  STEP(0x8008a35c);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S3), UINT32_C(0x80100000));
  STEP(0x8008a360);
  REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S3) = add_constant(
      REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S3), UINT32_C(0xffff8d88));
  STEP(0x8008a364);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_S0,
                NBA97_FRONTEND_RESOURCE_LOOKUP_V0, 16, UINT32_C(0x8008a364)));
  out->chain_key = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S0);

  /* 0x8008A368 runs once for the initial key. The source back edge targets
   * 0x8008A36C, while 0x8008A37C refreshes a0 in the callback-result branch
   * delay slot for every repeated chain lookup. */
  STEP(0x8008a368);
  REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A0) =
      REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S2);
chain_lookup:
  ++out->chain_attempts;
  STEP(0x8008a36c);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_LOOKUP_RA), UINT32_C(0x8008a374));
  STEP(0x8008a370);
  REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A1) =
      REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S0);
  TRY(invoke(run, NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A36C,
             UINT32_C(0x8008a36c), UINT32_C(0x8008a370), UINT32_C(0x80089ffc),
             2));
  STEP(0x8008a374);
  REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S1) =
      REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0);
  out->chain_result = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S1);
  STEP(0x8008a378);
  STEP(0x8008a37c);
  REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A0) =
      REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S2);
  TRY(zero_decision(run, REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S1),
                    UINT32_C(0x8008a378), &decision));
  if (decision) {
    STEP(0x8008a380);
    REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V1) = and_constant(
        REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S0), UINT32_C(0x00000f00));
    STEP(0x8008a384);
    REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V1) =
        shift_right_arithmetic(REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V1), 8);
    STEP(0x8008a388);
    REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0) =
        shift_left(REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V1), 1);
    STEP(0x8008a38c);
    REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0) =
        add_words(REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0),
                  REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V1));
    STEP(0x8008a390);
    REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0) =
        shift_left(REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0), 3);
    STEP(0x8008a394);
    REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0) =
        add_words(REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0),
                  REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S3));
    STEP(0x8008a398);
    TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_S0,
                  NBA97_FRONTEND_RESOURCE_LOOKUP_V0, 16, UINT32_C(0x8008a398)));
    out->chain_key = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S0);
    STEP(0x8008a39c);
    STEP(0x8008a3a0);
    STEP(0x8008a3a4);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0), 0);
    TRY(zero_decision(run, REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S0),
                      UINT32_C(0x8008a3a0), &decision));
    if (!decision)
      goto chain_lookup;
    STEP(0x8008a3a8);
    STEP(0x8008a3ac);
    goto epilogue;
  }

  out->secondary_path = 1;
  STEP(0x8008a3b0);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_A2,
                NBA97_FRONTEND_RESOURCE_LOOKUP_S1, 24, UINT32_C(0x8008a3b0)));
  out->initial_flags = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A2);
  STEP(0x8008a3b4);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_A1,
                NBA97_FRONTEND_RESOURCE_LOOKUP_S1, 20, UINT32_C(0x8008a3b4)));
  STEP(0x8008a3b8);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A3), 0);
  STEP(0x8008a3bc);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0), UINT32_C(0xfffffff7));
  STEP(0x8008a3c0);
  REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A2) = and_constant(
      REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A2), UINT32_C(0xfffffff7));
  out->cleared_flags = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A2);
  STEP(0x8008a3c4);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_LOOKUP_RA), UINT32_C(0x8008a3cc));
  STEP(0x8008a3c8);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_A2,
                 NBA97_FRONTEND_RESOURCE_LOOKUP_S1, 24, UINT32_C(0x8008a3c8)));
  TRY(invoke(run, NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A3C4,
             UINT32_C(0x8008a3c4), UINT32_C(0x8008a3c8), UINT32_C(0x800771f0),
             4));
  STEP(0x8008a3cc);
  REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S0) =
      REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0);
  out->allocation_result = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S0);

  /* 0x8008A3D0..0x8008A3D4: preserve the source's unguarded allocation
   * dereference. A zero s0 reads guest address zero through retained memory. */
  STEP(0x8008a3d0);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_A0,
                NBA97_FRONTEND_RESOURCE_LOOKUP_S1, 0, UINT32_C(0x8008a3d0)));
  out->descriptor_source = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A0);
  STEP(0x8008a3d4);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_A1,
                NBA97_FRONTEND_RESOURCE_LOOKUP_S0, 0, UINT32_C(0x8008a3d4)));
  out->descriptor_destination = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A1);
  STEP(0x8008a3d8);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_A2,
                NBA97_FRONTEND_RESOURCE_LOOKUP_S1, 20, UINT32_C(0x8008a3d8)));
  out->descriptor_length = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_A2);
  STEP(0x8008a3dc);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_LOOKUP_RA), UINT32_C(0x8008a3e4));
  STEP(0x8008a3e0);
  TRY(invoke(run, NBA97_FRONTEND_RESOURCE_LOOKUP_SITE_8008A3DC,
             UINT32_C(0x8008a3dc), UINT32_C(0x8008a3e0), UINT32_C(0x800909a8),
             3));
  out->copied = 1;
  STEP(0x8008a3e4);
  REG(NBA97_FRONTEND_RESOURCE_LOOKUP_V0) =
      REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S0);

epilogue:
  /* 0x8008A3E8..0x8008A404: every restore uses the callback-live sp. The
   * restored ra is validated only when JR consumes it after its NOP delay. */
  STEP(0x8008a3e8);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_RA,
                NBA97_FRONTEND_RESOURCE_LOOKUP_SP, 32, UINT32_C(0x8008a3e8)));
  out->restored_return_address = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_RA);
  STEP(0x8008a3ec);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_S3,
                NBA97_FRONTEND_RESOURCE_LOOKUP_SP, 28, UINT32_C(0x8008a3ec)));
  out->restored_s3 = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S3);
  STEP(0x8008a3f0);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_S2,
                NBA97_FRONTEND_RESOURCE_LOOKUP_SP, 24, UINT32_C(0x8008a3f0)));
  out->restored_s2 = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S2);
  STEP(0x8008a3f4);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_S1,
                NBA97_FRONTEND_RESOURCE_LOOKUP_SP, 20, UINT32_C(0x8008a3f4)));
  out->restored_s1 = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S1);
  STEP(0x8008a3f8);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOOKUP_S0,
                NBA97_FRONTEND_RESOURCE_LOOKUP_SP, 16, UINT32_C(0x8008a3f8)));
  out->restored_s0 = REG(NBA97_FRONTEND_RESOURCE_LOOKUP_S0);
  STEP(0x8008a3fc);
  REG(NBA97_FRONTEND_RESOURCE_LOOKUP_SP) =
      add_constant(REG(NBA97_FRONTEND_RESOURCE_LOOKUP_SP), 40);
  publish(run);
  STEP(0x8008a400);
  STEP(0x8008a404);
  if (REG(NBA97_FRONTEND_RESOURCE_LOOKUP_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x8008a400), 0,
         REG(NBA97_FRONTEND_RESOURCE_LOOKUP_RA).word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (REG(NBA97_FRONTEND_RESOURCE_LOOKUP_RA).word & 3u) {
    stop(run, UINT32_C(0x8008a400), 0,
         REG(NBA97_FRONTEND_RESOURCE_LOOKUP_RA).word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
