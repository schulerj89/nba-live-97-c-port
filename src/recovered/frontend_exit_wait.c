#include "frontend_exit_wait.h"

#include <limits.h>
#include <string.h>

typedef Nba97FrontendExitWaitWord Word;

typedef struct Run {
  Nba97FrontendExitWaitContext *context;
  Nba97FrontendExitWaitProgress *out;
  Nba97FrontendExitWaitMachine machine;
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

static int machine_valid(const Nba97FrontendExitWaitMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_FRONTEND_EXIT_WAIT_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int initialize(Nba97FrontendExitWaitContext *context,
                      Nba97FrontendExitWaitProgress *out, Run *run) {
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

static int32_t signed_word(uint32_t value) {
  return value <= (uint32_t)INT32_MAX ? (int32_t)value
                                      : -1 - (int32_t)(UINT32_MAX - value);
}

static void signed_bounds(Word value, int64_t *minimum, int64_t *maximum) {
  uint32_t low = 0;
  uint32_t high = 0;
  unsigned byte;
  for (byte = 0; byte < 3; ++byte) {
    uint32_t part = (value.word >> (byte * 8u)) & 0xffu;
    low |= ((value.known_mask & (1u << byte)) ? part : 0u) << (byte * 8u);
    high |= ((value.known_mask & (1u << byte)) ? part : 0xffu)
            << (byte * 8u);
  }
  if (value.known_mask & 8u) {
    low |= value.word & UINT32_C(0xff000000);
    high |= value.word & UINT32_C(0xff000000);
  } else {
    low |= UINT32_C(0x80000000);
    high |= UINT32_C(0x7f000000);
  }
  *minimum = (low & UINT32_C(0x80000000))
                 ? (int64_t)low - INT64_C(0x100000000)
                 : (int64_t)low;
  *maximum = (high & UINT32_C(0x80000000))
                 ? (int64_t)high - INT64_C(0x100000000)
                 : (int64_t)high;
}

static Word signed_less(Word left, Word right) {
  Word result;
  int64_t lmin;
  int64_t lmax;
  int64_t rmin;
  int64_t rmax;
  result.word = signed_word(left.word) < signed_word(right.word);
  result.known_mask = 0x0fu;
  signed_bounds(left, &lmin, &lmax);
  signed_bounds(right, &rmin, &rmax);
  if (lmax < rmin || lmin >= rmax)
    return result;
  result.known_mask = 0x0eu;
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
    Nba97FrontendExitWaitAccess *event = &run->context->access_journal[index];
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
  journal(run, NBA97_FRONTEND_EXIT_WAIT_STORE, pc, guest, &REG(source));
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
  journal(run, NBA97_FRONTEND_EXIT_WAIT_READ, pc, guest, &loaded);
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

static int equal_decision(Run *run, Word left, Word right, uint32_t pc,
                          int *is_equal) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((left.known_mask & right.known_mask & (1u << byte)) &&
        (((left.word ^ right.word) >> (byte * 8u)) & 0xffu)) {
      *is_equal = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if ((left.known_mask & right.known_mask) == 0x0fu) {
    *is_equal = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

static int negative_decision(Run *run, Word value, uint32_t pc,
                             int *is_negative) {
  if (!(value.known_mask & 8u)) {
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *is_negative = (value.word & UINT32_C(0x80000000)) != 0;
  return NBA97_TEXT_COMPLETE;
}

static int invoke(Run *run, uint8_t site, uint32_t pc, uint32_t delay_pc,
                  uint32_t target, uint8_t argument_count) {
  Nba97FrontendExitWaitEvent event;
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

int nba97_frontend_exit_wait(Nba97FrontendExitWaitContext *context,
                             Nba97FrontendExitWaitProgress *out) {
  Run storage;
  Run *run = &storage;
  Word branch_value;
  int decision;
  TRY(initialize(context, out, run));

  /* 0x8002EFBC..0x8002EFD4: the handle load precedes the frame allocation.
   * The sentinel branch latches a0 before its delay saves s0. */
  STEP(0x8002efbc);
  set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_A0), UINT32_C(0x80010000));
  STEP(0x8002efc0);
  TRY(load_word(run, NBA97_FRONTEND_EXIT_WAIT_A0,
                NBA97_FRONTEND_EXIT_WAIT_A0, UINT32_C(0x7268),
                UINT32_C(0x8002efc0)));
  out->initial_handle = REG(NBA97_FRONTEND_EXIT_WAIT_A0);
  STEP(0x8002efc4);
  REG(NBA97_FRONTEND_EXIT_WAIT_SP) = add_constant(
      REG(NBA97_FRONTEND_EXIT_WAIT_SP), UINT32_C(0xffffffe8));
  out->frame_stack_pointer = REG(NBA97_FRONTEND_EXIT_WAIT_SP).word;
  STEP(0x8002efc8);
  set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_V0), UINT32_MAX);
  STEP(0x8002efcc);
  TRY(store_word(run, NBA97_FRONTEND_EXIT_WAIT_RA,
                 NBA97_FRONTEND_EXIT_WAIT_SP, 20,
                 UINT32_C(0x8002efcc)));
  out->saved_return_address = REG(NBA97_FRONTEND_EXIT_WAIT_RA);
  STEP(0x8002efd0);
  branch_value = REG(NBA97_FRONTEND_EXIT_WAIT_A0);
  STEP(0x8002efd4);
  TRY(store_word(run, NBA97_FRONTEND_EXIT_WAIT_S0,
                 NBA97_FRONTEND_EXIT_WAIT_SP, 16,
                 UINT32_C(0x8002efd4)));
  out->saved_s0 = REG(NBA97_FRONTEND_EXIT_WAIT_S0);
  TRY(equal_decision(run, branch_value, REG(NBA97_FRONTEND_EXIT_WAIT_V0),
                     UINT32_C(0x8002efd0), &decision));
  if (decision) {
    out->exit_path = NBA97_FRONTEND_EXIT_WAIT_EXIT_SENTINEL;
  } else {
    /* 0x8002EFD8..0x8002EFEC: stop the handle, read the initial clock, and
     * form the wrapping 32-bit deadline in live s0. */
    STEP(0x8002efd8);
    set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_A1), 100);
    STEP(0x8002efdc);
    set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_RA), UINT32_C(0x8002efe4));
    STEP(0x8002efe0);
    set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_A2), UINT32_MAX);
    TRY(invoke(run, NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFDC,
               UINT32_C(0x8002efdc), UINT32_C(0x8002efe0),
               UINT32_C(0x8007b2bc), 3));
    STEP(0x8002efe4);
    set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_RA), UINT32_C(0x8002efec));
    STEP(0x8002efe8);
    TRY(invoke(run, NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFE4,
               UINT32_C(0x8002efe4), UINT32_C(0x8002efe8),
               UINT32_C(0x8008da5c), 0));
    out->clock_result = REG(NBA97_FRONTEND_EXIT_WAIT_V0);
    STEP(0x8002efec);
    REG(NBA97_FRONTEND_EXIT_WAIT_S0) =
        add_constant(REG(NBA97_FRONTEND_EXIT_WAIT_V0), 360);
    out->deadline = REG(NBA97_FRONTEND_EXIT_WAIT_S0);

    for (;;) {
      ++out->loop_iterations;
      /* BLTZ 0x8002EFF8 latches the poll result before delay V0=-1. */
      STEP(0x8002eff0);
      set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_RA), UINT32_C(0x8002eff8));
      STEP(0x8002eff4);
      TRY(invoke(run, NBA97_FRONTEND_EXIT_WAIT_SITE_8002EFF0,
                 UINT32_C(0x8002eff0), UINT32_C(0x8002eff4),
                 UINT32_C(0x8006b6a0), 0));
      branch_value = REG(NBA97_FRONTEND_EXIT_WAIT_V0);
      out->first_poll_result = branch_value;
      STEP(0x8002eff8);
      STEP(0x8002effc);
      set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_V0), UINT32_MAX);
      TRY(negative_decision(run, branch_value, UINT32_C(0x8002eff8),
                            &decision));
      if (decision) {
        out->exit_path = NBA97_FRONTEND_EXIT_WAIT_EXIT_POLL_NEGATIVE;
        break;
      }

      /* BNE 0x8002F008 likewise observes the old second poll result while
       * its delay forces the value later stored to the handle global. */
      STEP(0x8002f000);
      set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_RA), UINT32_C(0x8002f008));
      STEP(0x8002f004);
      TRY(invoke(run, NBA97_FRONTEND_EXIT_WAIT_SITE_8002F000,
                 UINT32_C(0x8002f000), UINT32_C(0x8002f004),
                 UINT32_C(0x8006fcf0), 0));
      branch_value = REG(NBA97_FRONTEND_EXIT_WAIT_V0);
      out->second_poll_result = branch_value;
      STEP(0x8002f008);
      STEP(0x8002f00c);
      set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_V0), UINT32_MAX);
      TRY(zero_decision(run, branch_value, UINT32_C(0x8002f008),
                        &decision));
      if (!decision) {
        out->exit_path = NBA97_FRONTEND_EXIT_WAIT_EXIT_POLL_NONZERO;
        break;
      }

      STEP(0x8002f010);
      set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_RA), UINT32_C(0x8002f018));
      STEP(0x8002f014);
      TRY(invoke(run, NBA97_FRONTEND_EXIT_WAIT_SITE_8002F010,
                 UINT32_C(0x8002f010), UINT32_C(0x8002f014),
                 UINT32_C(0x80039260), 0));
      STEP(0x8002f018);
      set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_RA), UINT32_C(0x8002f020));
      STEP(0x8002f01c);
      TRY(invoke(run, NBA97_FRONTEND_EXIT_WAIT_SITE_8002F018,
                 UINT32_C(0x8002f018), UINT32_C(0x8002f01c),
                 UINT32_C(0x8008da5c), 0));
      out->clock_result = REG(NBA97_FRONTEND_EXIT_WAIT_V0);
      STEP(0x8002f020);
      REG(NBA97_FRONTEND_EXIT_WAIT_V0) =
          signed_less(REG(NBA97_FRONTEND_EXIT_WAIT_S0),
                      REG(NBA97_FRONTEND_EXIT_WAIT_V0));
      STEP(0x8002f024);
      branch_value = REG(NBA97_FRONTEND_EXIT_WAIT_V0);
      STEP(0x8002f028);
      TRY(zero_decision(run, branch_value, UINT32_C(0x8002f024),
                        &decision));
      if (!decision) {
        out->exit_path = NBA97_FRONTEND_EXIT_WAIT_EXIT_DEADLINE;
        break;
      }
    }

    if (out->exit_path == NBA97_FRONTEND_EXIT_WAIT_EXIT_DEADLINE) {
      STEP(0x8002f02c);
      set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_A0), UINT32_C(0x80010000));
      STEP(0x8002f030);
      TRY(load_word(run, NBA97_FRONTEND_EXIT_WAIT_A0,
                    NBA97_FRONTEND_EXIT_WAIT_A0, UINT32_C(0x7268),
                    UINT32_C(0x8002f030)));
      out->reloaded_handle = REG(NBA97_FRONTEND_EXIT_WAIT_A0);
      STEP(0x8002f034);
      set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_RA), UINT32_C(0x8002f03c));
      STEP(0x8002f038);
      TRY(invoke(run, NBA97_FRONTEND_EXIT_WAIT_SITE_8002F034,
                 UINT32_C(0x8002f034), UINT32_C(0x8002f038),
                 UINT32_C(0x80092c34), 1));
      STEP(0x8002f03c);
      set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_V0), UINT32_MAX);
    }

    /* 0x8002F040..0x8002F06C: all non-sentinel exits store the delay-forced
     * v0=-1, then clear the secondary global only after its callback. */
    STEP(0x8002f040);
    set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_AT), UINT32_C(0x80010000));
    STEP(0x8002f044);
    TRY(store_word(run, NBA97_FRONTEND_EXIT_WAIT_V0,
                   NBA97_FRONTEND_EXIT_WAIT_AT, UINT32_C(0x7268),
                   UINT32_C(0x8002f044)));
    STEP(0x8002f048);
    set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_RA), UINT32_C(0x8002f050));
    STEP(0x8002f04c);
    TRY(invoke(run, NBA97_FRONTEND_EXIT_WAIT_SITE_8002F048,
               UINT32_C(0x8002f048), UINT32_C(0x8002f04c),
               UINT32_C(0x80028c28), 0));
    STEP(0x8002f050);
    set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_RA), UINT32_C(0x8002f058));
    STEP(0x8002f054);
    TRY(invoke(run, NBA97_FRONTEND_EXIT_WAIT_SITE_8002F050,
               UINT32_C(0x8002f050), UINT32_C(0x8002f054),
               UINT32_C(0x8006faa0), 0));
    STEP(0x8002f058);
    set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_A0), UINT32_C(0x80020000));
    STEP(0x8002f05c);
    TRY(load_word(run, NBA97_FRONTEND_EXIT_WAIT_A0,
                  NBA97_FRONTEND_EXIT_WAIT_A0, UINT32_C(0x149c),
                  UINT32_C(0x8002f05c)));
    out->secondary_word = REG(NBA97_FRONTEND_EXIT_WAIT_A0);
    STEP(0x8002f060);
    set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_RA), UINT32_C(0x8002f068));
    STEP(0x8002f064);
    TRY(invoke(run, NBA97_FRONTEND_EXIT_WAIT_SITE_8002F060,
               UINT32_C(0x8002f060), UINT32_C(0x8002f064),
               UINT32_C(0x80028cf4), 1));
    STEP(0x8002f068);
    set_known(&REG(NBA97_FRONTEND_EXIT_WAIT_AT), UINT32_C(0x80020000));
    STEP(0x8002f06c);
    TRY(store_word(run, NBA97_FRONTEND_EXIT_WAIT_ZERO,
                   NBA97_FRONTEND_EXIT_WAIT_AT, UINT32_C(0x149c),
                   UINT32_C(0x8002f06c)));
  }

  /* 0x8002F070..0x8002F080: both paths restore through callback-live sp;
   * the source consumes restored ra after the JR NOP. */
  STEP(0x8002f070);
  TRY(load_word(run, NBA97_FRONTEND_EXIT_WAIT_RA,
                NBA97_FRONTEND_EXIT_WAIT_SP, 20,
                UINT32_C(0x8002f070)));
  out->restored_return_address = REG(NBA97_FRONTEND_EXIT_WAIT_RA);
  STEP(0x8002f074);
  TRY(load_word(run, NBA97_FRONTEND_EXIT_WAIT_S0,
                NBA97_FRONTEND_EXIT_WAIT_SP, 16,
                UINT32_C(0x8002f074)));
  out->restored_s0 = REG(NBA97_FRONTEND_EXIT_WAIT_S0);
  STEP(0x8002f078);
  REG(NBA97_FRONTEND_EXIT_WAIT_SP) = add_constant(
      REG(NBA97_FRONTEND_EXIT_WAIT_SP), 24);
  publish(run);
  STEP(0x8002f07c);
  STEP(0x8002f080);
  if (REG(NBA97_FRONTEND_EXIT_WAIT_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x8002f07c), 0,
         REG(NBA97_FRONTEND_EXIT_WAIT_RA).word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (REG(NBA97_FRONTEND_EXIT_WAIT_RA).word & 3u) {
    stop(run, UINT32_C(0x8002f07c), 0,
         REG(NBA97_FRONTEND_EXIT_WAIT_RA).word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
