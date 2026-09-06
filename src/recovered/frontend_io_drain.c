#include "frontend_io_drain.h"

#include <string.h>

typedef Nba97FrontendIoDrainWord Word;

typedef struct Run {
  Nba97FrontendIoDrainContext *context;
  Nba97FrontendIoDrainProgress *out;
  Nba97FrontendIoDrainMachine machine;
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

static int machine_valid(const Nba97FrontendIoDrainMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_FRONTEND_IO_DRAIN_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int initialize(Nba97FrontendIoDrainContext *context,
                      Nba97FrontendIoDrainProgress *out, Run *run) {
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

static int64_t signed_bits(uint32_t value) {
  return value <= UINT32_C(0x7fffffff)
             ? (int64_t)value
             : (int64_t)value - INT64_C(0x100000000);
}

static Word signed_less_than_constant(Word input, int32_t constant) {
  Word result;
  uint32_t minimum = 0;
  uint32_t maximum = 0;
  unsigned byte;
  result.word = signed_bits(input.word) < constant ? 1u : 0u;
  for (byte = 0; byte < 3; ++byte) {
    uint32_t shift = byte * 8u;
    if (input.known_mask & (1u << byte)) {
      uint32_t value = input.word & (UINT32_C(0xff) << shift);
      minimum |= value;
      maximum |= value;
    } else {
      maximum |= UINT32_C(0xff) << shift;
    }
  }
  if (input.known_mask & 8u) {
    minimum |= input.word & UINT32_C(0xff000000);
    maximum |= input.word & UINT32_C(0xff000000);
  } else {
    minimum |= UINT32_C(0x80000000);
    maximum |= UINT32_C(0x7f000000);
  }
  if (signed_bits(maximum) < constant ||
      signed_bits(minimum) >= constant)
    result.known_mask = 0x0fu;
  else
    result.known_mask = 0x0eu;
  return result;
}

static void set_known(Word *word, uint32_t value) {
  word->word = value;
  word->known_mask = 0x0fu;
}

static int decide_zero(Word value, int *is_zero) {
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
  return NBA97_TEXT_UNKNOWN;
}

static int decide_equal(Word left, Word right, int *is_equal) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte) {
    uint8_t bit = (uint8_t)(1u << byte);
    if ((left.known_mask & bit) && (right.known_mask & bit) &&
        ((left.word >> (byte * 8u)) & 0xffu) !=
            ((right.word >> (byte * 8u)) & 0xffu)) {
      *is_equal = 0;
      return NBA97_TEXT_COMPLETE;
    }
  }
  if ((left.known_mask & right.known_mask) == 0x0fu) {
    *is_equal = 1;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_UNKNOWN;
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
    Nba97FrontendIoDrainAccess *event = &run->context->access_journal[index];
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
  journal(run, NBA97_FRONTEND_IO_DRAIN_STORE, pc, guest, &REG(source));
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
  journal(run, NBA97_FRONTEND_IO_DRAIN_READ, pc, guest, &loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int invoke(Run *run, uint8_t site, uint32_t pc, uint32_t delay_pc,
                  uint32_t target, uint8_t argument_count) {
  Nba97FrontendIoDrainEvent event;
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

int nba97_frontend_io_drain(Nba97FrontendIoDrainContext *context,
                            Nba97FrontendIoDrainProgress *out) {
  Run storage;
  Run *run = &storage;
  int branch;
  int decision_result;
  TRY(initialize(context, out, run));

  /* 0x800393F0..0x80039404: establish the 32-byte frame, saving s1 before
   * clearing it, then s0 before clearing it, and finally entry ra. */
  STEP(0x800393f0);
  REG(NBA97_FRONTEND_IO_DRAIN_SP) = add_constant(
      REG(NBA97_FRONTEND_IO_DRAIN_SP), UINT32_C(0xffffffe0));
  out->frame_stack_pointer = REG(NBA97_FRONTEND_IO_DRAIN_SP).word;
  publish(run);
  STEP(0x800393f4);
  TRY(store_word(run, NBA97_FRONTEND_IO_DRAIN_S1,
                 NBA97_FRONTEND_IO_DRAIN_SP, 20,
                 UINT32_C(0x800393f4)));
  out->saved_s1 = REG(NBA97_FRONTEND_IO_DRAIN_S1);
  STEP(0x800393f8);
  set_known(&REG(NBA97_FRONTEND_IO_DRAIN_S1), 0);
  STEP(0x800393fc);
  TRY(store_word(run, NBA97_FRONTEND_IO_DRAIN_S0,
                 NBA97_FRONTEND_IO_DRAIN_SP, 16,
                 UINT32_C(0x800393fc)));
  out->saved_s0 = REG(NBA97_FRONTEND_IO_DRAIN_S0);
  STEP(0x80039400);
  set_known(&REG(NBA97_FRONTEND_IO_DRAIN_S0), 0);
  STEP(0x80039404);
  TRY(store_word(run, NBA97_FRONTEND_IO_DRAIN_RA,
                 NBA97_FRONTEND_IO_DRAIN_SP, 24,
                 UINT32_C(0x80039404)));
  out->saved_return_address = REG(NBA97_FRONTEND_IO_DRAIN_RA);

  for (;;) {
    /* 0x80039408..0x80039434: load the status through live s0. Branch 39418
     * compares v1 with old v0=3 before SLTI overwrites v0 in the delay; branch
     * 39420 likewise latches that SLTI before its delay writes v0=1. */
    STEP(0x80039408);
    set_known(&REG(NBA97_FRONTEND_IO_DRAIN_AT), UINT32_C(0x800f0000));
    STEP(0x8003940c);
    REG(NBA97_FRONTEND_IO_DRAIN_AT) = add_constant(
        REG(NBA97_FRONTEND_IO_DRAIN_S0), UINT32_C(0x800f0000));
    publish(run);
    out->last_slot_offset = REG(NBA97_FRONTEND_IO_DRAIN_S0);
    STEP(0x80039410);
    TRY(load_word(run, NBA97_FRONTEND_IO_DRAIN_V1,
                  NBA97_FRONTEND_IO_DRAIN_AT, UINT32_C(0xfffff840),
                  UINT32_C(0x80039410)));
    out->last_status = REG(NBA97_FRONTEND_IO_DRAIN_V1);
    STEP(0x80039414);
    set_known(&REG(NBA97_FRONTEND_IO_DRAIN_V0), 3);
    STEP(0x80039418);
    decision_result = decide_equal(REG(NBA97_FRONTEND_IO_DRAIN_V1),
                                   REG(NBA97_FRONTEND_IO_DRAIN_V0), &branch);
    STEP(0x8003941c);
    REG(NBA97_FRONTEND_IO_DRAIN_V0) = signed_less_than_constant(
        REG(NBA97_FRONTEND_IO_DRAIN_V1), 4);
    publish(run);
    if (decision_result != NBA97_TEXT_COMPLETE) {
      stop(run, UINT32_C(0x80039418), 0, 0);
      return decision_result;
    }
    if (branch) {
      /* 0x8003944C..0x8003947C: status 3 loads the pre-callback pointer.
       * The callback may change s0/s1, so both clears and the increment use
       * callback-live registers rather than the address used by the load. */
      STEP(0x8003944c);
      set_known(&REG(NBA97_FRONTEND_IO_DRAIN_AT), UINT32_C(0x800f0000));
      STEP(0x80039450);
      REG(NBA97_FRONTEND_IO_DRAIN_AT) = add_constant(
          REG(NBA97_FRONTEND_IO_DRAIN_S0), UINT32_C(0x800f0000));
      publish(run);
      STEP(0x80039454);
      TRY(load_word(run, NBA97_FRONTEND_IO_DRAIN_A0,
                    NBA97_FRONTEND_IO_DRAIN_AT, UINT32_C(0xfffff844),
                    UINT32_C(0x80039454)));
      STEP(0x80039458);
      set_known(&REG(NBA97_FRONTEND_IO_DRAIN_RA), UINT32_C(0x80039460));
      STEP(0x8003945c);
      TRY(invoke(run, NBA97_FRONTEND_IO_DRAIN_SITE_80039458,
                 UINT32_C(0x80039458), UINT32_C(0x8003945c),
                 UINT32_C(0x80077638), 1));
      STEP(0x80039460);
      set_known(&REG(NBA97_FRONTEND_IO_DRAIN_AT), UINT32_C(0x800f0000));
      STEP(0x80039464);
      REG(NBA97_FRONTEND_IO_DRAIN_AT) = add_constant(
          REG(NBA97_FRONTEND_IO_DRAIN_S0), UINT32_C(0x800f0000));
      publish(run);
      STEP(0x80039468);
      TRY(store_word(run, NBA97_FRONTEND_IO_DRAIN_ZERO,
                     NBA97_FRONTEND_IO_DRAIN_AT, UINT32_C(0xfffff844),
                     UINT32_C(0x80039468)));
      STEP(0x8003946c);
      set_known(&REG(NBA97_FRONTEND_IO_DRAIN_AT), UINT32_C(0x800f0000));
      STEP(0x80039470);
      REG(NBA97_FRONTEND_IO_DRAIN_AT) = add_constant(
          REG(NBA97_FRONTEND_IO_DRAIN_S0), UINT32_C(0x800f0000));
      publish(run);
      STEP(0x80039474);
      TRY(store_word(run, NBA97_FRONTEND_IO_DRAIN_ZERO,
                     NBA97_FRONTEND_IO_DRAIN_AT, UINT32_C(0xfffff840),
                     UINT32_C(0x80039474)));
      STEP(0x80039478);
      STEP(0x8003947c);
      REG(NBA97_FRONTEND_IO_DRAIN_S1) = add_constant(
          REG(NBA97_FRONTEND_IO_DRAIN_S1), 1);
      publish(run);
    } else {
      STEP(0x80039420);
      decision_result = decide_zero(REG(NBA97_FRONTEND_IO_DRAIN_V0), &branch);
      STEP(0x80039424);
      set_known(&REG(NBA97_FRONTEND_IO_DRAIN_V0), 1);
      publish(run);
      if (decision_result != NBA97_TEXT_COMPLETE) {
        stop(run, UINT32_C(0x80039420), 0, 0);
        return decision_result;
      }
      if (!branch) {
        STEP(0x80039428);
        decision_result = decide_equal(REG(NBA97_FRONTEND_IO_DRAIN_V1),
                                       REG(NBA97_FRONTEND_IO_DRAIN_V0),
                                       &branch);
        STEP(0x8003942c);
        if (decision_result != NBA97_TEXT_COMPLETE) {
          stop(run, UINT32_C(0x80039428), 0, 0);
          return decision_result;
        }
        if (!branch) {
          STEP(0x80039430);
          STEP(0x80039434);
          REG(NBA97_FRONTEND_IO_DRAIN_S1) = add_constant(
              REG(NBA97_FRONTEND_IO_DRAIN_S1), 1);
          publish(run);
        } else {
          STEP(0x8003946c);
          set_known(&REG(NBA97_FRONTEND_IO_DRAIN_AT), UINT32_C(0x800f0000));
          STEP(0x80039470);
          REG(NBA97_FRONTEND_IO_DRAIN_AT) = add_constant(
              REG(NBA97_FRONTEND_IO_DRAIN_S0), UINT32_C(0x800f0000));
          publish(run);
          STEP(0x80039474);
          TRY(store_word(run, NBA97_FRONTEND_IO_DRAIN_ZERO,
                         NBA97_FRONTEND_IO_DRAIN_AT, UINT32_C(0xfffff840),
                         UINT32_C(0x80039474)));
          STEP(0x80039478);
          STEP(0x8003947c);
          REG(NBA97_FRONTEND_IO_DRAIN_S1) = add_constant(
              REG(NBA97_FRONTEND_IO_DRAIN_S1), 1);
          publish(run);
        }
      } else {
        STEP(0x80039438);
        REG(NBA97_FRONTEND_IO_DRAIN_V0) = signed_less_than_constant(
            REG(NBA97_FRONTEND_IO_DRAIN_V1), 6);
        publish(run);
        STEP(0x8003943c);
        decision_result = decide_zero(REG(NBA97_FRONTEND_IO_DRAIN_V0), &branch);
        STEP(0x80039440);
        if (decision_result != NBA97_TEXT_COMPLETE) {
          stop(run, UINT32_C(0x8003943c), 0, 0);
          return decision_result;
        }
        if (!branch) {
          STEP(0x80039444);
          STEP(0x80039448);
          STEP(0x80039480);
          set_known(&REG(NBA97_FRONTEND_IO_DRAIN_AT), UINT32_C(0x800f0000));
          STEP(0x80039484);
          REG(NBA97_FRONTEND_IO_DRAIN_AT) = add_constant(
              REG(NBA97_FRONTEND_IO_DRAIN_S0), UINT32_C(0x800f0000));
          publish(run);
          STEP(0x80039488);
          TRY(store_word(run, NBA97_FRONTEND_IO_DRAIN_ZERO,
                         NBA97_FRONTEND_IO_DRAIN_AT, UINT32_C(0xfffff830),
                         UINT32_C(0x80039488)));
        }
        STEP(0x8003948c);
        REG(NBA97_FRONTEND_IO_DRAIN_S1) = add_constant(
            REG(NBA97_FRONTEND_IO_DRAIN_S1), 1);
        publish(run);
      }
    }
    ++out->slot_iterations;

    /* 0x80039490..0x80039498: signed s1<8 controls the back edge, but the
     * branch delay advances callback-live s0 by 36 on both outcomes. */
    STEP(0x80039490);
    REG(NBA97_FRONTEND_IO_DRAIN_V0) = signed_less_than_constant(
        REG(NBA97_FRONTEND_IO_DRAIN_S1), 8);
    publish(run);
    STEP(0x80039494);
    decision_result = decide_zero(REG(NBA97_FRONTEND_IO_DRAIN_V0), &branch);
    STEP(0x80039498);
    REG(NBA97_FRONTEND_IO_DRAIN_S0) = add_constant(
        REG(NBA97_FRONTEND_IO_DRAIN_S0), 36);
    publish(run);
    if (decision_result != NBA97_TEXT_COMPLETE) {
      stop(run, UINT32_C(0x80039494), 0, 0);
      return decision_result;
    }
    if (!branch)
      continue;
    break;
  }

  /* 0x8003949C..0x800394B8: poll until v0 is nonzero. Every zero result
   * invokes the pump and jumps back through the explicit NOP delay. */
  for (;;) {
    STEP(0x8003949c);
    set_known(&REG(NBA97_FRONTEND_IO_DRAIN_RA), UINT32_C(0x800394a4));
    STEP(0x800394a0);
    ++out->poll_attempts;
    TRY(invoke(run, NBA97_FRONTEND_IO_DRAIN_SITE_8003949C,
               UINT32_C(0x8003949c), UINT32_C(0x800394a0),
               UINT32_C(0x800392a0), 0));
    STEP(0x800394a4);
    decision_result = decide_zero(REG(NBA97_FRONTEND_IO_DRAIN_V0), &branch);
    STEP(0x800394a8);
    if (decision_result != NBA97_TEXT_COMPLETE) {
      stop(run, UINT32_C(0x800394a4), 0, 0);
      return decision_result;
    }
    if (!branch)
      break;
    ++out->zero_poll_results;
    STEP(0x800394ac);
    set_known(&REG(NBA97_FRONTEND_IO_DRAIN_RA), UINT32_C(0x800394b4));
    STEP(0x800394b0);
    TRY(invoke(run, NBA97_FRONTEND_IO_DRAIN_SITE_800394AC,
               UINT32_C(0x800394ac), UINT32_C(0x800394b0),
               UINT32_C(0x80038e84), 0));
    STEP(0x800394b4);
    STEP(0x800394b8);
  }

  /* 0x800394BC..0x800394D0: restore ra, s1, and s0 in source order through
   * callback-live sp, release the 32-byte frame, then return through ra. */
  STEP(0x800394bc);
  TRY(load_word(run, NBA97_FRONTEND_IO_DRAIN_RA,
                NBA97_FRONTEND_IO_DRAIN_SP, 24,
                UINT32_C(0x800394bc)));
  out->restored_return_address = REG(NBA97_FRONTEND_IO_DRAIN_RA);
  STEP(0x800394c0);
  TRY(load_word(run, NBA97_FRONTEND_IO_DRAIN_S1,
                NBA97_FRONTEND_IO_DRAIN_SP, 20,
                UINT32_C(0x800394c0)));
  out->restored_s1 = REG(NBA97_FRONTEND_IO_DRAIN_S1);
  STEP(0x800394c4);
  TRY(load_word(run, NBA97_FRONTEND_IO_DRAIN_S0,
                NBA97_FRONTEND_IO_DRAIN_SP, 16,
                UINT32_C(0x800394c4)));
  out->restored_s0 = REG(NBA97_FRONTEND_IO_DRAIN_S0);
  STEP(0x800394c8);
  REG(NBA97_FRONTEND_IO_DRAIN_SP) = add_constant(
      REG(NBA97_FRONTEND_IO_DRAIN_SP), 32);
  publish(run);
  STEP(0x800394cc);
  STEP(0x800394d0);
  if (REG(NBA97_FRONTEND_IO_DRAIN_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x800394cc), 0,
         REG(NBA97_FRONTEND_IO_DRAIN_RA).word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (REG(NBA97_FRONTEND_IO_DRAIN_RA).word & 3u) {
    stop(run, UINT32_C(0x800394cc), 0,
         REG(NBA97_FRONTEND_IO_DRAIN_RA).word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
