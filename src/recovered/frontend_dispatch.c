#include "frontend_dispatch.h"

#include <limits.h>
#include <string.h>

typedef Nba97FrontendDispatchWord Word;

typedef struct Run {
  Nba97FrontendDispatchContext *context;
  Nba97FrontendDispatchProgress *out;
  Nba97FrontendDispatchMachine machine;
} Run;

#define R(i) (run->machine.registers.gpr[(i)])
#define ZERO R(0)
#define AT R(1)
#define V0 R(2)
#define V1 R(3)
#define A0 R(4)
#define A1 R(5)
#define A2 R(6)
#define A3 R(7)
#define T0 R(8)
#define T1 R(9)
#define T2 R(10)
#define T3 R(11)
#define T4 R(12)
#define T5 R(13)
#define T6 R(14)
#define T7 R(15)
#define S0 R(16)
#define S1 R(17)
#define S2 R(18)
#define S3 R(19)
#define S4 R(20)
#define S5 R(21)
#define S6 R(22)
#define S7 R(23)
#define T8 R(24)
#define T9 R(25)
#define K0 R(26)
#define K1 R(27)
#define GP R(28)
#define S8 R(30)
#define FP R(30)
#define SP R(29)
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

static void known(Word *word, uint32_t value) {
  word->word = value;
  word->known_mask = 15;
}

static Word immediate(uint32_t value) {
  Word result;
  known(&result, value);
  return result;
}

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t entry) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  run->out->stopped_target = entry;
  publish(run);
}

static int machine_valid(const Nba97FrontendDispatchMachine *machine) {
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

static int initialize(Nba97FrontendDispatchContext *context,
                      Nba97FrontendDispatchProgress *out, Run *run) {
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

/* Enumerating byte carries retains every invariant result byte. */
static Word add(Word left, Word right) {
  Word result;
  unsigned carry_mask = 1;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned left_start = (left.known_mask & (1u << byte))
                              ? ((left.word >> (byte * 8u)) & 255u)
                              : 0;
    unsigned left_end = (left.known_mask & (1u << byte)) ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte))
                               ? ((right.word >> (byte * 8u)) & 255u)
                               : 0;
    unsigned right_end = (right.known_mask & (1u << byte)) ? right_start : 255u;
    unsigned carry;
    for (carry = 0; carry < 2; ++carry) {
      unsigned a;
      if (!(carry_mask & (1u << carry)))
        continue;
      for (a = left_start; a <= left_end; ++a) {
        unsigned b;
        for (b = right_start; b <= right_end; ++b) {
          unsigned sum = a + b + carry;
          unsigned output = sum & 255u;
          next_mask |= 1u << (sum >> 8u);
          if (first) {
            first_output = output;
            first = 0;
          } else if (output != first_output) {
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

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width, Word value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97FrontendDispatchAccess *event = &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value =
        value.word &
        (width == 4 ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - 1u);
    event->operation = run->out->operations;
    event->width = (uint8_t)width;
    event->known_mask =
        (uint8_t)(value.known_mask & (uint8_t)((1u << width) - 1u));
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t address, unsigned width,
                  unsigned alignment, uint32_t pc, uint8_t **data,
                  uint8_t **known_bytes) {
  size_t i;
  size_t j;
  stop(run, pc, address, 0);
  TRY(spend(run));
  ++run->out->accesses;
  if (address & (alignment - 1u))
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0; i < run->context->memory.count; ++i) {
    Nba97GameTextRegion *region = &run->context->memory.region[i];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        width > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known_bytes = region->known ? region->known + (size_t)offset : 0;
    if (*known_bytes)
      for (j = 0; j < width; ++j)
        if ((*known_bytes)[j] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int read_value(Run *run, uint32_t address, unsigned width,
                      unsigned alignment, uint32_t pc, Word *value) {
  uint8_t *data;
  uint8_t *known_bytes;
  Word loaded = {0, 0};
  unsigned i;
  TRY(locate(run, address, width, alignment, pc, &data, &known_bytes));
  for (i = 0; i < width; ++i) {
    loaded.word |= (uint32_t)data[i] << (i * 8u);
    if (!known_bytes || known_bytes[i])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
  }
  if (width == 1) {
    loaded.word &= 255u;
    loaded.known_mask = (uint8_t)(loaded.known_mask | 14u);
  } else if (width == 2) {
    loaded.word &= 65535u;
    loaded.known_mask = (uint8_t)(loaded.known_mask | 12u);
  }
  *value = loaded;
  ++run->out->reads;
  journal(run, NBA97_FRONTEND_DISPATCH_READ, pc, address, width, loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, uint32_t address, unsigned width,
                       unsigned alignment, uint32_t pc, Word value) {
  uint8_t *data;
  uint8_t *known_bytes;
  uint8_t required = (uint8_t)((1u << width) - 1u);
  unsigned i;
  TRY(locate(run, address, width, alignment, pc, &data, &known_bytes));
  if (!known_bytes && (value.known_mask & required) != required)
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < width; ++i) {
    data[i] = (uint8_t)(value.word >> (i * 8u));
    if (known_bytes)
      known_bytes[i] = (uint8_t)((value.known_mask >> i) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_FRONTEND_DISPATCH_STORE, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int address(Run *run, Word base, uint32_t offset, uint32_t pc,
                   uint32_t *effective) {
  Word value = add(base, immediate(offset));
  if (value.known_mask != 15) {
    stop(run, pc, value.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *effective = value.word;
  return NBA97_TEXT_COMPLETE;
}

static int load(Run *run, unsigned destination, unsigned base, uint32_t offset,
                unsigned width, unsigned alignment, uint32_t pc) {
  uint32_t effective;
  Word value;
  TRY(address(run, R(base), offset, pc, &effective));
  TRY(read_value(run, effective, width, alignment, pc, &value));
  R(destination) = value;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int store(Run *run, unsigned source, unsigned base, uint32_t offset,
                 unsigned width, unsigned alignment, uint32_t pc) {
  uint32_t effective;
  TRY(address(run, R(base), offset, pc, &effective));
  return write_value(run, effective, width, alignment, pc, R(source));
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
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

static Word sign_extend_half(Word value) {
  value.word = (value.word & 0x8000u) ? value.word | UINT32_C(0xffff0000)
                                      : value.word & UINT32_C(0x0000ffff);
  value.known_mask =
      (uint8_t)((value.known_mask & 3u) | ((value.known_mask & 2u) ? 12u : 0u));
  return value;
}

static Word sign_extend_byte(Word value) {
  value.word = (value.word & UINT32_C(0x80)) ? value.word | UINT32_C(0xffffff00)
                                             : value.word & UINT32_C(0xff);
  value.known_mask =
      (uint8_t)((value.known_mask & 1u) | ((value.known_mask & 1u) ? 14u : 0u));
  return value;
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

static Word subtract(Word left, Word right) {
  Word result;
  unsigned borrow_mask = 1;
  unsigned byte;
  result.word = left.word - right.word;
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
    unsigned borrow;
    for (borrow = 0; borrow < 2; ++borrow) {
      unsigned a;
      if (!(borrow_mask & (1u << borrow)))
        continue;
      for (a = ls; a <= le; ++a) {
        unsigned b;
        for (b = rs; b <= re; ++b) {
          unsigned output = (a - b - borrow) & 255u;
          next_mask |= 1u << (a < b + borrow);
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
    borrow_mask = next_mask;
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

static int32_t signed_word(uint32_t value) {
  return value <= (uint32_t)INT32_MAX ? (int32_t)value
                                      : -1 - (int32_t)(UINT32_MAX - value);
}

static void signed_bounds(Word value, int64_t *minimum, int64_t *maximum) {
  uint32_t low = 0;
  uint32_t high = 0;
  unsigned byte;
  for (byte = 0; byte < 3; ++byte) {
    uint32_t part = (value.word >> (byte * 8u)) & 255u;
    low |= ((value.known_mask & (1u << byte)) ? part : 0u) << (byte * 8u);
    high |= ((value.known_mask & (1u << byte)) ? part : 255u) << (byte * 8u);
  }
  if (value.known_mask & 8u) {
    low |= value.word & UINT32_C(0xff000000);
    high |= value.word & UINT32_C(0xff000000);
  } else {
    low |= UINT32_C(0x80000000);
    high |= UINT32_C(0x7f000000);
  }
  *minimum = (low & UINT32_C(0x80000000)) ? (int64_t)low - INT64_C(0x100000000)
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
  known(&result, signed_word(left.word) < signed_word(right.word));
  signed_bounds(left, &lmin, &lmax);
  signed_bounds(right, &rmin, &rmax);
  if (lmax < rmin || lmin >= rmax)
    return result;
  result.known_mask = 14;
  return result;
}

static int equal_decision(Run *run, Word left, Word right, uint32_t pc,
                          int *equal) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((left.known_mask & right.known_mask & (1u << byte)) &&
        (((left.word ^ right.word) >> (byte * 8u)) & 255u)) {
      *equal = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (left.known_mask == 15 && right.known_mask == 15) {
    *equal = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

static Word unsigned_less(Word left, Word right);

static int invoke(Run *run, uint8_t site, uint32_t pc, uint32_t delay_pc,
                  uint32_t target, uint8_t argument_count) {
  Nba97FrontendDispatchEvent event;
  int accepted;
  stop(run, pc, 0, target);
  TRY(spend(run));
  ++run->out->call_attempts[site];
  memset(&event, 0, sizeof event);
  event.pc = pc;
  event.delay_slot_pc = delay_pc;
  event.target = target;
  event.operation = run->out->operations;
  event.invocation = run->out->call_attempts[site];
  event.site = site;
  event.argument_count = argument_count;
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

int nba97_frontend_dispatch(Nba97FrontendDispatchContext *context,
                            Nba97FrontendDispatchProgress *out) {
  Run state;
  Run *run = &state;
  Word predicate;
  Word comparison;
  int decision;

  TRY(initialize(context, out, run));

  /* 0x8003F7C8..0x8003F8B8: allocate the live frame and initialize the
   * frontend context from the retained home/away and launch-mode globals. */
  STEP(0x8003f7c8);
  SP = add(SP, immediate(UINT32_C(0xffffff78)));
  out->frame_stack_pointer = SP.word;
  STEP(0x8003f7cc);
  TRY(store(run, 20, 29, 0x70, 4, 4, 0x8003f7cc));
  STEP(0x8003f7d0);
  known(&S4, 0x80010000u);
  STEP(0x8003f7d4);
  TRY(load(run, 20, 20, 0x70c0, 4, 4, 0x8003f7d4));
  STEP(0x8003f7d8);
  known(&V1, 0x80020000u);
  STEP(0x8003f7dc);
  V1 = add(V1, immediate(0x1d74u));
  STEP(0x8003f7e0);
  TRY(store(run, 31, 29, 0x84, 4, 4, 0x8003f7e0));
  STEP(0x8003f7e4);
  TRY(store(run, 30, 29, 0x80, 4, 4, 0x8003f7e4));
  STEP(0x8003f7e8);
  TRY(store(run, 23, 29, 0x7c, 4, 4, 0x8003f7e8));
  STEP(0x8003f7ec);
  TRY(store(run, 22, 29, 0x78, 4, 4, 0x8003f7ec));
  STEP(0x8003f7f0);
  TRY(store(run, 21, 29, 0x74, 4, 4, 0x8003f7f0));
  STEP(0x8003f7f4);
  TRY(store(run, 19, 29, 0x6c, 4, 4, 0x8003f7f4));
  STEP(0x8003f7f8);
  TRY(store(run, 18, 29, 0x68, 4, 4, 0x8003f7f8));
  STEP(0x8003f7fc);
  TRY(store(run, 17, 29, 0x64, 4, 4, 0x8003f7fc));
  STEP(0x8003f800);
  TRY(store(run, 16, 29, 0x60, 4, 4, 0x8003f800));
  STEP(0x8003f804);
  TRY(load(run, 2, 3, 0, 4, 4, 0x8003f804));
  STEP(0x8003f808);
  STEP(0x8003f80c);
  TRY(store(run, 2, 20, 0x70e, 2, 2, 0x8003f80c));
  STEP(0x8003f810);
  known(&V0, 0x80020000u);
  STEP(0x8003f814);
  TRY(load(run, 2, 2, 0x1d78, 4, 4, 0x8003f814));
  STEP(0x8003f818);
  TRY(store(run, 0, 29, 0x58, 2, 2, 0x8003f818));
  STEP(0x8003f81c);
  TRY(store(run, 2, 20, 0x710, 2, 2, 0x8003f81c));
  STEP(0x8003f820);
  V0 = and_constant(V0, 0xffffu);
  STEP(0x8003f824);
  V0 = unsigned_less(V0, immediate(0x1du));
  predicate = V0;
  STEP(0x8003f828);
  STEP(0x8003f82c);
  known(&V0, 3);
  TRY(zero_decision(run, predicate, 0x8003f828, &decision));
  if (decision) {
    STEP(0x8003f830);
    TRY(store(run, 2, 20, 0x710, 2, 2, 0x8003f830));
  }
  STEP(0x8003f834);
  TRY(load(run, 2, 20, 0x70e, 2, 2, 0x8003f834));
  STEP(0x8003f838);
  STEP(0x8003f83c);
  V0 = unsigned_less(V0, immediate(0x1du));
  predicate = V0;
  STEP(0x8003f840);
  STEP(0x8003f844);
  known(&V0, UINT32_MAX);
  TRY(zero_decision(run, predicate, 0x8003f840, &decision));
  if (decision) {
    STEP(0x8003f848);
    known(&V0, 0x18);
    STEP(0x8003f84c);
    TRY(store(run, 2, 20, 0x70e, 2, 2, 0x8003f84c));
    STEP(0x8003f850);
    known(&V0, UINT32_MAX);
  }
  STEP(0x8003f854);
  TRY(store(run, 2, 20, 0x712, 2, 2, 0x8003f854));
  STEP(0x8003f858);
  TRY(store(run, 2, 20, 0x714, 2, 2, 0x8003f858));
  STEP(0x8003f85c);
  known(&V0, UINT32_MAX);
  STEP(0x8003f860);
  TRY(store(run, 2, 20, 0x71e, 1, 1, 0x8003f860));
  STEP(0x8003f864);
  known(&V0, 1);
  STEP(0x8003f868);
  TRY(store(run, 2, 20, 0x878, 2, 2, 0x8003f868));
  STEP(0x8003f86c);
  known(&V0, 1);
  STEP(0x8003f870);
  TRY(store(run, 0, 20, 0x71c, 1, 1, 0x8003f870));
  STEP(0x8003f874);
  TRY(store(run, 0, 20, 0x72e, 2, 2, 0x8003f874));
  STEP(0x8003f878);
  TRY(store(run, 0, 20, 0x876, 2, 2, 0x8003f878));
  STEP(0x8003f87c);
  known(&AT, 0x800f0000u);
  STEP(0x8003f880);
  TRY(store(run, 2, 1, UINT32_C(0xfffffce8), 4, 4, 0x8003f880));
  STEP(0x8003f884);
  known(&V0, UINT32_MAX);
  STEP(0x8003f888);
  TRY(store(run, 2, 20, 4, 4, 4, 0x8003f888));
  STEP(0x8003f88c);
  TRY(store(run, 2, 20, 0, 4, 4, 0x8003f88c));
  STEP(0x8003f890);
  TRY(store(run, 2, 20, 0x0c, 4, 4, 0x8003f890));
  STEP(0x8003f894);
  TRY(store(run, 2, 20, 8, 4, 4, 0x8003f894));
  STEP(0x8003f898);
  known(&V0, 0x50);
  STEP(0x8003f89c);
  TRY(store(run, 2, 20, 0x4b0, 1, 1, 0x8003f89c));
  STEP(0x8003f8a0);
  known(&V0, 0x61);
  STEP(0x8003f8a4);
  TRY(store(run, 2, 20, 0x4b1, 1, 1, 0x8003f8a4));
  STEP(0x8003f8a8);
  known(&V0, 0x6c);
  STEP(0x8003f8ac);
  TRY(store(run, 2, 20, 0x4b2, 1, 1, 0x8003f8ac));
  STEP(0x8003f8b0);
  known(&V0, 0x30);
  STEP(0x8003f8b4);
  TRY(store(run, 2, 20, 0x4b3, 1, 1, 0x8003f8b4));
  STEP(0x8003f8b8);
  TRY(store(run, 0, 20, 0x4b4, 1, 1, 0x8003f8b8));

  /* 0x8003F8BC..0x8003F9D4: invoke frontend initialization services and
   * construct the source state stack at live sp+0x18. */
  STEP(0x8003f8bc);
  known(&A0, 0x80020000u);
  STEP(0x8003f8c0);
  TRY(load(run, 4, 4, 0x1d78, 2, 2, 0x8003f8c0));
  A0 = sign_extend_half(A0);
  STEP(0x8003f8c4);
  TRY(load(run, 5, 3, 0, 2, 2, 0x8003f8c4));
  A1 = sign_extend_half(A1);
  STEP(0x8003f8c8);
  known(&RA, 0x8003f8d0u);
  STEP(0x8003f8cc);
  TRY(store(run, 0, 29, 0x48, 1, 1, 0x8003f8cc));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003F8C8, 0x8003f8c8, 0x8003f8cc,
             0x8003f7b0, 2));
  STEP(0x8003f8d0);
  known(&A0, 0x80020000u);
  STEP(0x8003f8d4);
  A0 = add(A0, immediate(0x4f74));
  STEP(0x8003f8d8);
  known(&A1, 0x0fa0);
  STEP(0x8003f8dc);
  known(&RA, 0x8003f8e4u);
  STEP(0x8003f8e0);
  known(&A2, 0);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003F8DC, 0x8003f8dc, 0x8003f8e0,
             0x800770d4, 3));
  STEP(0x8003f8e4);
  known(&AT, 0x800f0000u);
  STEP(0x8003f8e8);
  TRY(store(run, 2, 1, UINT32_C(0xfffff754), 4, 4, 0x8003f8e8));
  STEP(0x8003f8ec);
  known(&AT, 0x800f0000u);
  STEP(0x8003f8f0);
  TRY(store(run, 2, 1, UINT32_C(0xffffd260), 4, 4, 0x8003f8f0));
  STEP(0x8003f8f4);
  known(&RA, 0x8003f8fcu);
  STEP(0x8003f8f8);
  known(&S0, 0);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003F8F4, 0x8003f8f4, 0x8003f8f8,
             0x80030cdc, 0));
  STEP(0x8003f8fc);
  known(&RA, 0x8003f904u);
  STEP(0x8003f900);
  known(&S3, 0);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003F8FC, 0x8003f8fc, 0x8003f900,
             0x80030308, 0));
  STEP(0x8003f904);
  known(&AT, 0x800e0000u);
  STEP(0x8003f908);
  TRY(store(run, 0, 1, UINT32_C(0xffffed2c), 4, 4, 0x8003f908));
  STEP(0x8003f90c);
  known(&AT, 0x800e0000u);
  STEP(0x8003f910);
  TRY(store(run, 0, 1, UINT32_C(0xffffed30), 4, 4, 0x8003f910));
  STEP(0x8003f914);
  known(&AT, 0x800e0000u);
  STEP(0x8003f918);
  TRY(store(run, 0, 1, UINT32_C(0xffffed28), 4, 4, 0x8003f918));
  STEP(0x8003f91c);
  known(&AT, 0x800e0000u);
  STEP(0x8003f920);
  TRY(store(run, 0, 1, UINT32_C(0xffffed24), 4, 4, 0x8003f920));
  STEP(0x8003f924);
  known(&AT, 0x800e0000u);
  STEP(0x8003f928);
  TRY(store(run, 0, 1, UINT32_C(0xffffed20), 4, 4, 0x8003f928));
  STEP(0x8003f92c);
  known(&RA, 0x8003f934u);
  STEP(0x8003f930);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003F92C, 0x8003f92c, 0x8003f930,
             0x8003d2a4, 0));
  STEP(0x8003f934);
  known(&V1, 0x80010000u);
  STEP(0x8003f938);
  TRY(load(run, 3, 3, 0x5098, 4, 4, 0x8003f938));
  STEP(0x8003f93c);
  known(&T0, UINT32_MAX);
  STEP(0x8003f940);
  V0 = signed_less(V1, immediate(4));
  predicate = V0;
  STEP(0x8003f944);
  STEP(0x8003f948);
  TRY(store(run, 8, 29, 0x50, 2, 2, 0x8003f948));
  TRY(zero_decision(run, predicate, 0x8003f944, &decision));
  if (decision) {
    STEP(0x8003f94c);
    V0 = add(V1, immediate(UINT32_C(0xfffffffd)));
    STEP(0x8003f950);
    STEP(0x8003f954);
    TRY(store(run, 2, 20, 0x78, 2, 2, 0x8003f954));
  } else {
    STEP(0x8003f958);
    TRY(store(run, 3, 20, 0x78, 2, 2, 0x8003f958));
  }
  STEP(0x8003f95c);
  S1 = add(SP, immediate(0x18));
  STEP(0x8003f960);
  V0 = add(S1, S0);
  STEP(0x8003f964);
  TRY(store(run, 0, 2, 0, 1, 1, 0x8003f964));
  STEP(0x8003f968);
  known(&V0, 0x80010000u);
  STEP(0x8003f96c);
  TRY(load(run, 2, 2, 0x5098, 4, 4, 0x8003f96c));
  STEP(0x8003f970);
  predicate = V0;
  STEP(0x8003f974);
  STEP(0x8003f978);
  known(&V0, 0x1d);
  TRY(zero_decision(run, predicate, 0x8003f974, &decision));
  if (!decision) {
    STEP(0x8003f97c);
    known(&RA, 0x8003f984u);
    STEP(0x8003f980);
    S0 = add(S0, immediate(1));
    TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003F97C, 0x8003f97c,
               0x8003f980, 0x800459c8, 0));
    STEP(0x8003f984);
    V1 = add(S1, S0);
    STEP(0x8003f988);
    STEP(0x8003f98c);
    TRY(store(run, 2, 3, 0, 1, 1, 0x8003f98c));
  } else {
    STEP(0x8003f990);
    TRY(store(run, 0, 20, 0x72a, 2, 2, 0x8003f990));
    STEP(0x8003f994);
    TRY(store(run, 2, 20, 0x728, 2, 2, 0x8003f994));
  }
  STEP(0x8003f998);
  known(&A0, 0x80020000u);
  STEP(0x8003f99c);
  A0 = add(A0, immediate(UINT32_C(0xffffedec)));
  STEP(0x8003f9a0);
  TRY(load(run, 3, 4, 0, 2, 2, 0x8003f9a0));
  STEP(0x8003f9a4);
  known(&V0, 1);
  predicate = V1;
  STEP(0x8003f9a8);
  STEP(0x8003f9ac);
  S2 = add(SP, immediate(0x18));
  TRY(equal_decision(run, predicate, V0, 0x8003f9a8, &decision));
  if (!decision) {
    STEP(0x8003f9b0);
    TRY(store(run, 0, 4, 0, 2, 2, 0x8003f9b0));
  }
  STEP(0x8003f9b4);
  known(&S8, 1);
  STEP(0x8003f9b8);
  known(&V0, 0x80090000u);
  STEP(0x8003f9bc);
  TRY(load(run, 2, 2, 0x352c, 4, 4, 0x8003f9bc));
  STEP(0x8003f9c0);
  known(&S5, 2);
  STEP(0x8003f9c4);
  known(&S7, 6);
  STEP(0x8003f9c8);
  known(&S6, UINT32_MAX);
  STEP(0x8003f9cc);
  known(&AT, 0x80010000u);
  STEP(0x8003f9d0);
  TRY(store(run, 0, 1, 0x5098, 4, 4, 0x8003f9d0));
  STEP(0x8003f9d4);
  TRY(store(run, 0, 2, 0x2a, 2, 2, 0x8003f9d4));

dispatch_loop:
  ++out->dispatch_iterations;
  /* 0x8003F9D8..0x8003FA38: publish the current state and branch through the
   * runtime 43-entry table. */
  STEP(0x8003f9d8);
  TRY(load(run, 2, 20, 0x720, 2, 2, 0x8003f9d8));
  STEP(0x8003f9dc);
  V1 = add(S2, S0);
  STEP(0x8003f9e0);
  TRY(store(run, 2, 20, 0x722, 2, 2, 0x8003f9e0));
  STEP(0x8003f9e4);
  TRY(load(run, 2, 3, 0, 1, 1, 0x8003f9e4));
  STEP(0x8003f9e8);
  known(&AT, 0x800f0000u);
  STEP(0x8003f9ec);
  TRY(store(run, 2, 1, 0x13fc, 1, 1, 0x8003f9ec));
  STEP(0x8003f9f0);
  V0 = and_constant(V0, 0xff);
  predicate = V0;
  STEP(0x8003f9f4);
  STEP(0x8003f9f8);
  TRY(store(run, 2, 20, 0x720, 2, 2, 0x8003f9f8));
  TRY(zero_decision(run, predicate, 0x8003f9f4, &decision));
  if (decision) {
    STEP(0x8003f9fc);
    known(&V0, 0x30);
    STEP(0x8003fa00);
    TRY(store(run, 2, 20, 0x4b3, 1, 1, 0x8003fa00));
  }
  STEP(0x8003fa04);
  TRY(load(run, 4, 3, 0, 1, 1, 0x8003fa04));
  STEP(0x8003fa08);
  known(&RA, 0x8003fa10u);
  STEP(0x8003fa0c);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FA08, 0x8003fa08, 0x8003fa0c,
             0x80031a88, 1));
  STEP(0x8003fa10);
  TRY(load(run, 3, 20, 0x720, 2, 2, 0x8003fa10));
  V1 = sign_extend_half(V1);
  STEP(0x8003fa14);
  STEP(0x8003fa18);
  V0 = unsigned_less(V1, immediate(0x2b));
  predicate = V0;
  STEP(0x8003fa1c);
  STEP(0x8003fa20);
  V0 = shift_left(V1, 2);
  TRY(zero_decision(run, predicate, 0x8003fa1c, &decision));
  if (decision)
    goto loop_test;
  STEP(0x8003fa24);
  known(&AT, 0x80020000u);
  STEP(0x8003fa28);
  AT = add(AT, V0);
  STEP(0x8003fa2c);
  TRY(load(run, 2, 1, 0x4f80, 4, 4, 0x8003fa2c));
  STEP(0x8003fa30);
  STEP(0x8003fa34);
  STEP(0x8003fa38);
  if (V0.known_mask != 15) {
    stop(run, 0x8003fa34, V0.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (V0.word & 3u) {
    stop(run, 0x8003fa34, V0.word, 0);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  switch (V0.word) {
  case 0x8003fa3c:
    goto state_00;
  case 0x8003fb6c:
    goto state_01;
  case 0x8003fc34:
    goto state_02;
  case 0x8003fc78:
    goto state_03;
  case 0x8003fca8:
    goto state_04;
  case 0x8003fcf4:
    goto state_05;
  case 0x8003fd74:
    goto state_06;
  case 0x8003fe58:
    goto state_07;
  case 0x8003fe98:
    goto state_08;
  case 0x8003ff10:
    goto state_09;
  case 0x8004005c:
    goto state_0a;
  case 0x8004006c:
    goto state_0b;
  case 0x800400f0:
    goto state_0c;
  case 0x80040120:
    goto state_0d;
  case 0x80040154:
    goto state_0e;
  case 0x80040184:
    goto state_0f;
  case 0x80040194:
    goto state_10;
  case 0x800401c0:
    goto state_11;
  case 0x800401fc:
    goto state_12;
  case 0x8004028c:
    goto state_13;
  case 0x800402d8:
    goto state_14;
  case 0x800402e8:
    goto state_15;
  case 0x80040350:
    goto state_16;
  case 0x80040360:
    goto state_17;
  case 0x80040370:
    goto state_18;
  case 0x80040380:
    goto state_19;
  case 0x80040390:
    goto state_1a;
  case 0x80040410:
    goto state_1b;
  case 0x80040474:
    goto state_1c;
  case 0x800405d8:
    goto state_1d;
  case 0x80040558:
    goto state_1e;
  case 0x80040658:
    goto state_1f;
  case 0x800406bc:
    goto state_20;
  case 0x800406fc:
    goto state_21;
  case 0x8004070c:
    goto state_22;
  case 0x8004071c:
    goto state_23;
  case 0x8004072c:
    goto state_24;
  case 0x8004073c:
    goto state_25;
  case 0x8004076c:
    goto state_26;
  case 0x8004077c:
    goto state_27;
  case 0x8004009c:
    goto state_28;
  case 0x800400ac:
    goto state_29;
  case 0x800407d4:
    goto state_2a;
  default:
    stop(run, 0x8003fa34, V0.word, V0.word);
    return NBA97_TEXT_IO_REFUSED;
  }

state_00:
  /* 0x8003FA3C..0x8003FB68: title-menu result routing. */
  STEP(0x8003fa3c);
  known(&RA, 0x8003fa44u);
  STEP(0x8003fa40);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FA3C, 0x8003fa3c, 0x8003fa40,
             0x8003f43c, 0));
  STEP(0x8003fa44);
  known(&V1, 0x80020000u);
  STEP(0x8003fa48);
  TRY(load(run, 3, 3, UINT32_C(0xffffedec), 2, 2, 0x8003fa48));
  STEP(0x8003fa4c);
  predicate = V1;
  STEP(0x8003fa50);
  STEP(0x8003fa54);
  S3 = V0;
  TRY(zero_decision(run, predicate, 0x8003fa50, &decision));
  if (!decision) {
    STEP(0x8003fa58);
    V0 = shift_left(S3, 16);
    STEP(0x8003fa5c);
    V0 = shift_right_arithmetic(V0, 16);
    STEP(0x8003fa60);
    V0 = signed_less(V0, immediate(2));
    predicate = V0;
    STEP(0x8003fa64);
    STEP(0x8003fa68);
    V0 = shift_left(S3, 16);
    TRY(zero_decision(run, predicate, 0x8003fa64, &decision));
    if (!decision) {
      STEP(0x8003fa6c);
      known(&S3, 6);
      STEP(0x8003fa70);
      STEP(0x8003fa74);
      known(&S0, UINT32_MAX);
      goto loop_test;
    }
  } else {
    STEP(0x8003fa78);
    V0 = shift_left(S3, 16);
  }
  STEP(0x8003fa7c);
  V1 = shift_right_arithmetic(V0, 16);
  predicate = V1;
  STEP(0x8003fa80);
  STEP(0x8003fa84);
  V0 = signed_less(V1, S5);
  TRY(equal_decision(run, predicate, S8, 0x8003fa80, &decision));
  if (!decision)
    goto state_00_other_result;
  STEP(0x8003fa88);
  known(&V0, 0x80020000u);
  STEP(0x8003fa8c);
  TRY(load(run, 2, 2, 0x1d70, 1, 1, 0x8003fa8c));
  STEP(0x8003fa90);
  predicate = V0;
  STEP(0x8003fa94);
  STEP(0x8003fa98);
  TRY(zero_decision(run, predicate, 0x8003fa94, &decision));
  if (decision) {
    STEP(0x8003fa9c);
    S0 = add(S0, immediate(1));
    STEP(0x8003faa0);
    V1 = add(S2, S0);
    STEP(0x8003faa4);
    known(&V0, 3);
    STEP(0x8003faa8);
    STEP(0x8003faac);
    TRY(store(run, 2, 3, 0, 1, 1, 0x8003faac));
    goto loop_test;
  }
  predicate = V0;
  STEP(0x8003fab0);
  STEP(0x8003fab4);
  TRY(equal_decision(run, predicate, S8, 0x8003fab0, &decision));
  if (decision) {
    STEP(0x8003fab8);
    known(&V0, 0x31);
    STEP(0x8003fabc);
    TRY(store(run, 2, 20, 0x4b3, 1, 1, 0x8003fabc));
    STEP(0x8003fac0);
    known(&V0, 0x80020000u);
    STEP(0x8003fac4);
    TRY(load(run, 2, 2, 0x1d9f, 1, 1, 0x8003fac4));
    STEP(0x8003fac8);
    predicate = V0;
    STEP(0x8003facc);
    STEP(0x8003fad0);
    TRY(zero_decision(run, predicate, 0x8003facc, &decision));
    if (decision)
      goto push_state_04_increment;
    STEP(0x8003fad4);
    STEP(0x8003fad8);
    S0 = add(S0, immediate(1));
    goto replace_state_07;
  }
  STEP(0x8003fadc);
  STEP(0x8003fae0);
  known(&V0, 0x32);
  TRY(equal_decision(run, predicate, S5, 0x8003fadc, &decision));
  if (!decision)
    goto loop_test;
  STEP(0x8003fae4);
  TRY(store(run, 2, 20, 0x4b3, 1, 1, 0x8003fae4));
  STEP(0x8003fae8);
  TRY(store(run, 30, 20, 0x71d, 1, 1, 0x8003fae8));
  STEP(0x8003faec);
  known(&V0, 0x80020000u);
  STEP(0x8003faf0);
  TRY(load(run, 2, 2, 0x1d98, 1, 1, 0x8003faf0));
  STEP(0x8003faf4);
  predicate = V0;
  STEP(0x8003faf8);
  STEP(0x8003fafc);
  TRY(zero_decision(run, predicate, 0x8003faf8, &decision));
  if (decision)
    goto state_1b_team_flag;
  STEP(0x8003fb00);
  STEP(0x8003fb04);
  S0 = add(S0, immediate(1));
  goto replace_state_1b;

state_00_other_result:
  STEP(0x8003fb08);
  STEP(0x8003fb0c);
  TRY(zero_decision(run, V0, 0x8003fb08, &decision));
  if (!decision)
    goto loop_test;
  STEP(0x8003fb10);
  STEP(0x8003fb14);
  known(&V0, 7);
  TRY(equal_decision(run, V1, S7, 0x8003fb10, &decision));
  if (decision)
    goto push_state_01;
  STEP(0x8003fb18);
  STEP(0x8003fb1c);
  known(&T0, 8);
  TRY(equal_decision(run, V1, V0, 0x8003fb18, &decision));
  if (decision)
    goto push_state_02;
  STEP(0x8003fb20);
  STEP(0x8003fb24);
  known(&V0, 9);
  TRY(equal_decision(run, V1, T0, 0x8003fb20, &decision));
  if (decision)
    goto push_state_09;
  STEP(0x8003fb28);
  comparison = V0;
  STEP(0x8003fb2c);
  known(&V0, 10);
  TRY(equal_decision(run, V1, comparison, 0x8003fb28, &decision));
  if (decision)
    goto push_state_13;
  STEP(0x8003fb30);
  STEP(0x8003fb34);
  known(&V0, 11);
  TRY(equal_decision(run, V1, immediate(10), 0x8003fb30, &decision));
  if (decision) {
    STEP(0x8003fb38);
    S0 = add(S0, immediate(1));
    STEP(0x8003fb3c);
    V1 = add(S2, S0);
    STEP(0x8003fb40);
    known(&V0, 11);
    STEP(0x8003fb44);
    known(&AT, 0x80010000u);
    STEP(0x8003fb48);
    TRY(store(run, 0, 1, 0x501c, 4, 4, 0x8003fb48));
    STEP(0x8003fb4c);
    STEP(0x8003fb50);
    TRY(store(run, 2, 3, 0, 1, 1, 0x8003fb50));
    goto loop_test;
  }
  STEP(0x8003fb54);
  STEP(0x8003fb58);
  known(&V0, 0x2a);
  TRY(equal_decision(run, V1, immediate(11), 0x8003fb54, &decision));
  if (!decision)
    goto loop_test;
  STEP(0x8003fb5c);
  S0 = add(S0, immediate(1));
  STEP(0x8003fb60);
  V1 = add(S2, S0);
  STEP(0x8003fb64);
  STEP(0x8003fb68);
  TRY(store(run, 2, 3, 0, 1, 1, 0x8003fb68));
  goto loop_test;

state_01:
  /* 0x8003FB6C..0x8003FC30: preserve and conditionally restore fourteen
   * custom-rule bytes around the typed setup service. */
  STEP(0x8003fb6c);
  known(&S1, 0);
  STEP(0x8003fb70);
  A1 = add(SP, immediate(0x38));
backup_rules_loop_body:
  ++out->backup_iterations;
  STEP(0x8003fb74);
  V0 = shift_left(S1, 16);
  STEP(0x8003fb78);
  V0 = shift_right_arithmetic(V0, 16);
  STEP(0x8003fb7c);
  known(&AT, 0x80020000u);
  STEP(0x8003fb80);
  AT = add(AT, V0);
  STEP(0x8003fb84);
  TRY(load(run, 4, 1, 0x1da3, 1, 1, 0x8003fb84));
  STEP(0x8003fb88);
  V1 = add(S1, immediate(1));
  STEP(0x8003fb8c);
  S1 = V1;
  STEP(0x8003fb90);
  V0 = add(A1, V0);
  STEP(0x8003fb94);
  V1 = shift_left(V1, 16);
  STEP(0x8003fb98);
  V1 = shift_right_arithmetic(V1, 16);
  STEP(0x8003fb9c);
  V1 = signed_less(V1, immediate(14));
  predicate = V1;
  STEP(0x8003fba0);
  STEP(0x8003fba4);
  TRY(store(run, 4, 2, 0, 1, 1, 0x8003fba4));
  TRY(zero_decision(run, predicate, 0x8003fba0, &decision));
  if (!decision)
    goto backup_rules_loop_body;
  STEP(0x8003fba8);
  known(&A0, 0x800a0000u);
  STEP(0x8003fbac);
  A0 = add(A0, immediate(UINT32_C(0xffff8194)));
  STEP(0x8003fbb0);
  known(&V0, 0x800a0000u);
  STEP(0x8003fbb4);
  TRY(load(run, 2, 2, UINT32_C(0xffff821c), 4, 4, 0x8003fbb4));
  STEP(0x8003fbb8);
  known(&A1, 1);
  STEP(0x8003fbbc);
  known(&A2, 0x80020000u);
  STEP(0x8003fbc0);
  A2 = add(A2, immediate(0x1d70));
  STEP(0x8003fbc4);
  known(&A3, 0x80040000u);
  STEP(0x8003fbc8);
  A3 = add(A3, immediate(UINT32_C(0xffffeb1c)));
  STEP(0x8003fbcc);
  TRY(store(run, 0, 2, 0x0e, 2, 2, 0x8003fbcc));
  STEP(0x8003fbd0);
  known(&V0, 0x80040000u);
  STEP(0x8003fbd4);
  V0 = add(V0, immediate(0x4e64));
  STEP(0x8003fbd8);
  known(&RA, 0x8003fbe0u);
  STEP(0x8003fbdc);
  TRY(store(run, 2, 29, 0x10, 4, 4, 0x8003fbdc));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FBD8, 0x8003fbd8, 0x8003fbdc,
             0x8003d930, 5));
  STEP(0x8003fbe0);
  V0 = shift_left(V0, 16);
  STEP(0x8003fbe4);
  V0 = shift_right_arithmetic(V0, 16);
  predicate = V0;
  STEP(0x8003fbe8);
  STEP(0x8003fbec);
  known(&S1, 0);
  TRY(equal_decision(run, predicate, S6, 0x8003fbe8, &decision));
  if (!decision)
    goto pop_state;
  ++out->restore_iterations;
  STEP(0x8003fbf0);
  A1 = add(SP, immediate(0x38));
  STEP(0x8003fbf4);
  V1 = shift_left(S1, 16);
restore_rules_loop_body:
  STEP(0x8003fbf8);
  V1 = shift_right_arithmetic(V1, 16);
  STEP(0x8003fbfc);
  V0 = add(A1, V1);
  STEP(0x8003fc00);
  TRY(load(run, 4, 2, 0, 1, 1, 0x8003fc00));
  STEP(0x8003fc04);
  V0 = add(S1, immediate(1));
  STEP(0x8003fc08);
  S1 = V0;
  STEP(0x8003fc0c);
  V0 = shift_left(V0, 16);
  STEP(0x8003fc10);
  V0 = shift_right_arithmetic(V0, 16);
  STEP(0x8003fc14);
  V0 = signed_less(V0, immediate(14));
  STEP(0x8003fc18);
  known(&AT, 0x80020000u);
  STEP(0x8003fc1c);
  AT = add(AT, V1);
  STEP(0x8003fc20);
  TRY(store(run, 4, 1, 0x1da3, 1, 1, 0x8003fc20));
  predicate = V0;
  STEP(0x8003fc24);
  STEP(0x8003fc28);
  V1 = shift_left(S1, 16);
  TRY(zero_decision(run, predicate, 0x8003fc24, &decision));
  if (!decision) {
    ++out->restore_iterations;
    goto restore_rules_loop_body;
  }
  STEP(0x8003fc2c);
  STEP(0x8003fc30);
  S0 = add(S0, immediate(UINT32_MAX));
  goto loop_test;

state_02:
  STEP(0x8003fc34);
  known(&A0, 0x800a0000u);
  STEP(0x8003fc38);
  A0 = add(A0, immediate(UINT32_C(0xffff8258)));
  STEP(0x8003fc3c);
  known(&V0, 0x800a0000u);
  STEP(0x8003fc40);
  TRY(load(run, 2, 2, UINT32_C(0xffff82e0), 4, 4, 0x8003fc40));
  STEP(0x8003fc44);
  known(&A1, 2);
  STEP(0x8003fc48);
  known(&A2, 0x80020000u);
  STEP(0x8003fc4c);
  A2 = add(A2, immediate(0x1d70));
  STEP(0x8003fc50);
  known(&A3, 0);
  STEP(0x8003fc54);
  TRY(store(run, 0, 2, 0x0e, 2, 2, 0x8003fc54));
  STEP(0x8003fc58);
  known(&RA, 0x8003fc60u);
  STEP(0x8003fc5c);
  TRY(store(run, 0, 29, 0x10, 4, 4, 0x8003fc5c));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FC58, 0x8003fc58, 0x8003fc5c,
             0x8003d930, 5));
  STEP(0x8003fc60);
  known(&A0, 0x80020000u);
  STEP(0x8003fc64);
  TRY(load(run, 4, 4, 0x1d7c, 1, 1, 0x8003fc64));
  STEP(0x8003fc68);
  known(&RA, 0x8003fc70u);
  STEP(0x8003fc6c);
  S0 = add(S0, immediate(UINT32_MAX));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FC68, 0x8003fc68, 0x8003fc6c,
             0x8002f0e8, 1));
  STEP(0x8003fc70);
  STEP(0x8003fc74);
  goto loop_test;

state_03:
  STEP(0x8003fc78);
  known(&RA, 0x8003fc80u);
  STEP(0x8003fc7c);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FC78, 0x8003fc78, 0x8003fc7c,
             0x8004fcd8, 0));
  STEP(0x8003fc80);
  S3 = V0;
  STEP(0x8003fc84);
  V0 = shift_left(S3, 16);
  STEP(0x8003fc88);
  V0 = shift_right_arithmetic(V0, 16);
  predicate = V0;
  STEP(0x8003fc8c);
  STEP(0x8003fc90);
  TRY(equal_decision(run, predicate, S6, 0x8003fc8c, &decision));
  if (decision) {
    STEP(0x8003fc94);
    S0 = add(S0, immediate(UINT32_MAX));
  }
  STEP(0x8003fc98);
  STEP(0x8003fc9c);
  TRY(equal_decision(run, V0, S8, 0x8003fc98, &decision));
  if (!decision)
    goto loop_test;
  STEP(0x8003fca0);
  STEP(0x8003fca4);
  S0 = add(S0, immediate(1));
  goto store_state_05_at_current;

state_04:
  STEP(0x8003fca8);
  known(&RA, 0x8003fcb0u);
  STEP(0x8003fcac);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FCA8, 0x8003fca8, 0x8003fcac,
             0x8004f5f4, 0));
  STEP(0x8003fcb0);
  S3 = V0;
  STEP(0x8003fcb4);
  V0 = shift_left(S3, 16);
  STEP(0x8003fcb8);
  V0 = shift_right_arithmetic(V0, 16);
  predicate = V0;
  STEP(0x8003fcbc);
  STEP(0x8003fcc0);
  known(&T0, 8);
  TRY(equal_decision(run, predicate, S6, 0x8003fcbc, &decision));
  if (decision)
    goto pop_state;
  STEP(0x8003fcc4);
  STEP(0x8003fcc8);
  TRY(equal_decision(run, predicate, T0, 0x8003fcc4, &decision));
  if (decision)
    goto replace_state_07;
  STEP(0x8003fcdc);
  known(&RA, 0x8003fce4u);
  STEP(0x8003fce0);
  known(&A0, 1);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FCDC, 0x8003fcdc, 0x8003fce0,
             0x80041144, 1));
  STEP(0x8003fce4);
  S0 = add(S0, immediate(1));
  STEP(0x8003fce8);
  V0 = add(S2, S0);
  STEP(0x8003fcec);
  STEP(0x8003fcf0);
  TRY(store(run, 23, 2, 0, 1, 1, 0x8003fcf0));
  goto loop_test;

state_05:
  STEP(0x8003fcf4);
  known(&RA, 0x8003fcfcu);
  STEP(0x8003fcf8);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FCF4, 0x8003fcf4, 0x8003fcf8,
             0x80037010, 0));
  STEP(0x8003fcfc);
  S3 = V0;
  STEP(0x8003fd00);
  V0 = shift_left(V0, 16);
  STEP(0x8003fd04);
  V0 = shift_right_arithmetic(V0, 16);
  predicate = V0;
  STEP(0x8003fd08);
  STEP(0x8003fd0c);
  TRY(equal_decision(run, predicate, S6, 0x8003fd08, &decision));
  if (decision) {
    STEP(0x8003fd10);
    known(&RA, 0x8003fd18u);
    STEP(0x8003fd14);
    S0 = add(S0, immediate(UINT32_MAX));
    TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FD10, 0x8003fd10,
               0x8003fd14, 0x8003b194, 0));
    STEP(0x8003fd18);
    STEP(0x8003fd1c);
    goto loop_test;
  }
  STEP(0x8003fd20);
  STEP(0x8003fd24);
  TRY(equal_decision(run, predicate, S7, 0x8003fd20, &decision));
  if (!decision)
    goto loop_test;
  STEP(0x8003fd28);
  TRY(load(run, 2, 20, 0x78, 1, 1, 0x8003fd28));
  STEP(0x8003fd2c);
  known(&AT, 0x80020000u);
  STEP(0x8003fd30);
  TRY(store(run, 2, 1, 0x1d70, 1, 1, 0x8003fd30));
  STEP(0x8003fd34);
  known(&AT, 0x80010000u);
  STEP(0x8003fd38);
  TRY(store(run, 2, 1, 0x5098, 4, 4, 0x8003fd38));
  STEP(0x8003fd3c);
  known(&RA, 0x8003fd44u);
  STEP(0x8003fd40);
  known(&A0, 0);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FD3C, 0x8003fd3c, 0x8003fd40,
             0x80061674, 1));
  STEP(0x8003fd44);
  known(&RA, 0x8003fd4cu);
  STEP(0x8003fd48);
  known(&S0, UINT32_MAX);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FD44, 0x8003fd44, 0x8003fd48,
             0x80046d24, 0));
  STEP(0x8003fd4c);
  known(&RA, 0x8003fd54u);
  STEP(0x8003fd50);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FD4C, 0x8003fd4c, 0x8003fd50,
             0x8003e7a8, 0));
  STEP(0x8003fd54);
  TRY(load(run, 2, 20, 0x78, 2, 2, 0x8003fd54));
  STEP(0x8003fd58);
  predicate = V0;
  STEP(0x8003fd5c);
  STEP(0x8003fd60);
  TRY(equal_decision(run, predicate, S8, 0x8003fd5c, &decision));
  if (decision)
    goto loop_test;
  STEP(0x8003fd64);
  known(&AT, 0x80020000u);
  STEP(0x8003fd68);
  TRY(store(run, 0, 1, UINT32_C(0xffffec94), 4, 4, 0x8003fd68));
  STEP(0x8003fd6c);
  STEP(0x8003fd70);
  goto loop_test;

state_06:
  STEP(0x8003fd74);
  known(&RA, 0x8003fd7cu);
  STEP(0x8003fd78);
  known(&A0, 0);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FD74, 0x8003fd74, 0x8003fd78,
             0x8003f778, 1));
  STEP(0x8003fd7c);
  known(&RA, 0x8003fd84u);
  STEP(0x8003fd80);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FD7C, 0x8003fd7c, 0x8003fd80,
             0x80044944, 0));
  STEP(0x8003fd84);
  S3 = V0;
  STEP(0x8003fd88);
  V0 = shift_left(S3, 16);
  STEP(0x8003fd8c);
  V1 = shift_right_arithmetic(V0, 16);
  predicate = V1;
  STEP(0x8003fd90);
  STEP(0x8003fd94);
  TRY(equal_decision(run, predicate, S6, 0x8003fd90, &decision));
  if (decision)
    goto pop_state;
  STEP(0x8003fd98);
  STEP(0x8003fd9c);
  known(&V0, 4);
  TRY(equal_decision(run, predicate, S5, 0x8003fd98, &decision));
  if (decision)
    goto push_state_05_at_current;
  STEP(0x8003fda0);
  STEP(0x8003fda4);
  known(&V0, 5);
  TRY(equal_decision(run, predicate, immediate(4), 0x8003fda0, &decision));
  if (decision) {
    STEP(0x8003fda8);
    S0 = add(S0, immediate(1));
    STEP(0x8003fdac);
    V1 = add(S2, S0);
    STEP(0x8003fdb0);
    known(&V0, 0x12);
    STEP(0x8003fdb4);
    STEP(0x8003fdb8);
    TRY(store(run, 2, 3, 0, 1, 1, 0x8003fdb8));
    goto loop_test;
  }
  STEP(0x8003fdbc);
  STEP(0x8003fdc0);
  TRY(equal_decision(run, predicate, V0, 0x8003fdbc, &decision));
  if (decision)
    goto push_state_09;
  STEP(0x8003fdc4);
  STEP(0x8003fdc8);
  known(&V0, 7);
  TRY(equal_decision(run, predicate, S7, 0x8003fdc4, &decision));
  if (decision) {
    STEP(0x8003fdcc);
    S0 = add(S0, immediate(1));
    STEP(0x8003fdd0);
    V0 = add(S2, S0);
    STEP(0x8003fdd4);
    known(&T0, 8);
    STEP(0x8003fdd8);
    STEP(0x8003fddc);
    TRY(store(run, 8, 2, 0, 1, 1, 0x8003fddc));
    goto loop_test;
  }
  STEP(0x8003fde0);
  STEP(0x8003fde4);
  known(&T0, 8);
  TRY(equal_decision(run, predicate, V0, 0x8003fde0, &decision));
  if (decision) {
    STEP(0x8003fde8);
    S0 = add(S0, immediate(1));
    STEP(0x8003fdec);
    V1 = add(S2, S0);
    STEP(0x8003fdf0);
    known(&V0, 0x28);
    STEP(0x8003fdf4);
    known(&AT, 0x80010000u);
    STEP(0x8003fdf8);
    TRY(store(run, 30, 1, 0x501c, 4, 4, 0x8003fdf8));
    STEP(0x8003fdfc);
    STEP(0x8003fe00);
    TRY(store(run, 2, 3, 0, 1, 1, 0x8003fe00));
    goto loop_test;
  }
  STEP(0x8003fe04);
  STEP(0x8003fe08);
  known(&V0, 9);
  TRY(equal_decision(run, predicate, T0, 0x8003fe04, &decision));
  if (decision)
    goto state_08_result_08;
  STEP(0x8003fe0c);
  comparison = V0;
  STEP(0x8003fe10);
  known(&V0, 10);
  TRY(equal_decision(run, predicate, comparison, 0x8003fe0c, &decision));
  if (decision) {
    STEP(0x8003fe14);
    known(&RA, 0x8003fe1cu);
    STEP(0x8003fe18);
    known(&S0, 0);
    TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FE14, 0x8003fe14,
               0x8003fe18, 0x800435a4, 0));
    STEP(0x8003fe1c);
    STEP(0x8003fe20);
    TRY(store(run, 0, 29, 0x18, 1, 1, 0x8003fe20));
    goto loop_test;
  }
  STEP(0x8003fe24);
  STEP(0x8003fe28);
  known(&V0, 9);
  TRY(equal_decision(run, predicate, immediate(10), 0x8003fe24, &decision));
  if (!decision)
    goto loop_test;
  STEP(0x8003fe2c);
  S0 = add(S0, immediate(1));
  STEP(0x8003fe30);
  V1 = add(S2, S0);
  STEP(0x8003fe34);
  TRY(store(run, 2, 3, 0, 1, 1, 0x8003fe34));
  STEP(0x8003fe38);
  known(&T0, 0x80020000u);
  STEP(0x8003fe3c);
  TRY(load(run, 8, 8, 0x1d74, 2, 2, 0x8003fe3c));
  STEP(0x8003fe40);
  STEP(0x8003fe44);
  TRY(store(run, 8, 29, 0x50, 2, 2, 0x8003fe44));
  STEP(0x8003fe48);
  known(&T0, 0x80020000u);
  STEP(0x8003fe4c);
  TRY(load(run, 8, 8, 0x1d78, 2, 2, 0x8003fe4c));
  STEP(0x8003fe50);
  STEP(0x8003fe54);
  TRY(store(run, 8, 29, 0x58, 2, 2, 0x8003fe54));
  goto loop_test;

state_07:
  STEP(0x8003fe58);
  known(&RA, 0x8003fe60u);
  STEP(0x8003fe5c);
  S1 = S3;
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FE58, 0x8003fe58, 0x8003fe5c,
             0x800417d4, 0));
  STEP(0x8003fe60);
  S3 = V0;
  STEP(0x8003fe64);
  V0 = shift_left(S1, 16);
  STEP(0x8003fe68);
  V0 = shift_right_arithmetic(V0, 16);
  STEP(0x8003fe6c);
  known(&T0, 8);
  predicate = V0;
  STEP(0x8003fe70);
  STEP(0x8003fe74);
  V0 = shift_left(S3, 16);
  TRY(equal_decision(run, predicate, T0, 0x8003fe70, &decision));
  if (!decision) {
    STEP(0x8003fe78);
    V0 = shift_right_arithmetic(V0, 16);
    predicate = V0;
    STEP(0x8003fe7c);
    STEP(0x8003fe80);
    TRY(equal_decision(run, predicate, S6, 0x8003fe7c, &decision));
    if (decision)
      goto pop_state;
  }
push_state_04_increment:
  STEP(0x8003fe84);
  S0 = add(S0, immediate(1));
  STEP(0x8003fe88);
  V1 = add(S2, S0);
  STEP(0x8003fe8c);
  known(&V0, 4);
  STEP(0x8003fe90);
  STEP(0x8003fe94);
  TRY(store(run, 2, 3, 0, 1, 1, 0x8003fe94));
  goto loop_test;

state_08:
  STEP(0x8003fe98);
  known(&RA, 0x8003fea0u);
  STEP(0x8003fe9c);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FE98, 0x8003fe98, 0x8003fe9c,
             0x80041df4, 0));
  STEP(0x8003fea0);
  S3 = V0;
  STEP(0x8003fea4);
  V0 = shift_left(S3, 16);
  STEP(0x8003fea8);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x8003feac);
  V0 = signed_less(V1, immediate(2));
  predicate = V0;
  STEP(0x8003feb0);
  STEP(0x8003feb4);
  known(&V0, 5);
  TRY(zero_decision(run, predicate, 0x8003feb0, &decision));
  if (!decision)
    goto pop_state;
  STEP(0x8003feb8);
  STEP(0x8003febc);
  TRY(equal_decision(run, V1, V0, 0x8003feb8, &decision));
  if (decision)
    goto push_state_01;
  STEP(0x8003fec0);
  STEP(0x8003fec4);
  known(&V0, 7);
  TRY(equal_decision(run, V1, S7, 0x8003fec0, &decision));
  if (decision)
    goto push_state_02;
  STEP(0x8003fec8);
  STEP(0x8003fecc);
  known(&T0, 8);
  TRY(equal_decision(run, V1, V0, 0x8003fec8, &decision));
  if (decision)
    goto push_state_0a;
  STEP(0x8003fed0);
  STEP(0x8003fed4);
  known(&V0, 9);
  TRY(equal_decision(run, V1, T0, 0x8003fed0, &decision));
  if (decision) {
  state_08_result_08:
    STEP(0x8003fed8);
    known(&RA, 0x8003fee0u);
    STEP(0x8003fedc);
    known(&A0, 1);
    TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FED8, 0x8003fed8,
               0x8003fedc, 0x8003f778, 1));
    STEP(0x8003fee0);
    known(&RA, 0x8003fee8u);
    STEP(0x8003fee4);
    known(&S0, 1);
    TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FEE0, 0x8003fee0,
               0x8003fee4, 0x80046354, 0));
    STEP(0x8003fee8);
    known(&V0, 0x1c);
    STEP(0x8003feec);
    TRY(store(run, 0, 29, 0x18, 1, 1, 0x8003feec));
    STEP(0x8003fef0);
    STEP(0x8003fef4);
    TRY(store(run, 2, 29, 0x19, 1, 1, 0x8003fef4));
    goto loop_test;
  }
  STEP(0x8003fef8);
  STEP(0x8003fefc);
  TRY(equal_decision(run, V1, V0, 0x8003fef8, &decision));
  if (!decision)
    goto loop_test;
  STEP(0x8003ff00);
  known(&RA, 0x8003ff08u);
  STEP(0x8003ff04);
  known(&S0, 0);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FF00, 0x8003ff00, 0x8003ff04,
             0x800435a4, 0));
  STEP(0x8003ff08);
  STEP(0x8003ff0c);
  TRY(store(run, 0, 29, 0x18, 1, 1, 0x8003ff0c));
  goto loop_test;

state_09:
  STEP(0x8003ff10);
  known(&RA, 0x8003ff18u);
  STEP(0x8003ff14);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FF10, 0x8003ff10, 0x8003ff14,
             0x80057ce4, 0));
  STEP(0x8003ff18);
  S3 = V0;
  STEP(0x8003ff1c);
  V0 = shift_left(S3, 16);
  STEP(0x8003ff20);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x8003ff24);
  V0 = signed_less(V1, immediate(2));
  predicate = V0;
  STEP(0x8003ff28);
  STEP(0x8003ff2c);
  TRY(zero_decision(run, predicate, 0x8003ff28, &decision));
  if (decision)
    goto state_09_positive;
  STEP(0x8003ff30);
  TRY(load(run, 2, 29, 0x48, 1, 1, 0x8003ff30));
  STEP(0x8003ff34);
  predicate = V0;
  STEP(0x8003ff38);
  STEP(0x8003ff3c);
  V1 = add(S2, S0);
  TRY(zero_decision(run, predicate, 0x8003ff38, &decision));
  if (!decision) {
    STEP(0x8003ff40);
    known(&V0, 0x1c);
    STEP(0x8003ff44);
    TRY(store(run, 0, 29, 0x48, 1, 1, 0x8003ff44));
    STEP(0x8003ff48);
    TRY(store(run, 2, 3, 0, 1, 1, 0x8003ff48));
    STEP(0x8003ff4c);
    STEP(0x8003ff50);
    TRY(store(run, 21, 20, 0x78, 2, 2, 0x8003ff50));
    goto loop_test;
  }
  STEP(0x8003ff54);
  TRY(load(run, 8, 29, 0x50, 2, 2, 0x8003ff54));
  STEP(0x8003ff58);
  STEP(0x8003ff5c);
  V0 = shift_left(T0, 16);
  STEP(0x8003ff60);
  A1 = shift_right_arithmetic(V0, 16);
  predicate = A1;
  STEP(0x8003ff64);
  STEP(0x8003ff68);
  known(&V0, 5);
  TRY(equal_decision(run, predicate, S6, 0x8003ff64, &decision));
  if (decision)
    goto pop_state;
  STEP(0x8003ff6c);
  TRY(load(run, 8, 29, 0x58, 2, 2, 0x8003ff6c));
  STEP(0x8003ff70);
  TRY(store(run, 2, 3, 0, 1, 1, 0x8003ff70));
  STEP(0x8003ff74);
  known(&AT, 0x80020000u);
  STEP(0x8003ff78);
  TRY(store(run, 5, 1, 0x1d74, 4, 4, 0x8003ff78));
  STEP(0x8003ff7c);
  A0 = shift_left(T0, 16);
  STEP(0x8003ff80);
  A0 = shift_right_arithmetic(A0, 16);
  STEP(0x8003ff84);
  known(&AT, 0x80020000u);
  STEP(0x8003ff88);
  TRY(store(run, 4, 1, 0x1d78, 4, 4, 0x8003ff88));
  STEP(0x8003ff8c);
  known(&RA, 0x8003ff94u);
  STEP(0x8003ff90);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8003FF8C, 0x8003ff8c, 0x8003ff90,
             0x8003f7b0, 2));
  STEP(0x8003ff94);
  known(&T0, UINT32_MAX);
  STEP(0x8003ff98);
  STEP(0x8003ff9c);
  TRY(store(run, 8, 29, 0x50, 2, 2, 0x8003ff9c));
  goto loop_test;

state_09_positive:
  STEP(0x8003ffa0);
  STEP(0x8003ffa4);
  known(&V0, 3);
  TRY(equal_decision(run, V1, S5, 0x8003ffa0, &decision));
  if (decision) {
    STEP(0x8003ffa8);
    S0 = add(S0, immediate(1));
    STEP(0x8003ffac);
    V1 = add(S2, S0);
    STEP(0x8003ffb0);
    known(&V0, 0x0d);
    STEP(0x8003ffb4);
    STEP(0x8003ffb8);
    TRY(store(run, 2, 3, 0, 1, 1, 0x8003ffb8));
    goto loop_test;
  }
  STEP(0x8003ffbc);
  STEP(0x8003ffc0);
  known(&V0, 4);
  TRY(equal_decision(run, V1, immediate(3), 0x8003ffbc, &decision));
  if (decision) {
    STEP(0x8003ffc4);
    S0 = add(S0, immediate(1));
    STEP(0x8003ffc8);
    V1 = add(S2, S0);
    STEP(0x8003ffcc);
    known(&V0, 0x0e);
    STEP(0x8003ffd0);
    STEP(0x8003ffd4);
    TRY(store(run, 2, 3, 0, 1, 1, 0x8003ffd4));
    goto loop_test;
  }
  STEP(0x8003ffd8);
  STEP(0x8003ffdc);
  known(&V0, 0x11);
  TRY(equal_decision(run, V1, immediate(4), 0x8003ffd8, &decision));
  if (decision) {
    STEP(0x8003ffe0);
    S0 = add(S0, immediate(1));
    STEP(0x8003ffe4);
    V1 = add(S2, S0);
    STEP(0x8003ffe8);
    STEP(0x8003ffec);
    TRY(store(run, 2, 3, 0, 1, 1, 0x8003ffec));
    goto loop_test;
  }
  STEP(0x8003fff0);
  STEP(0x8003fff4);
  known(&V0, 7);
  TRY(equal_decision(run, V1, S7, 0x8003fff0, &decision));
  if (decision) {
    STEP(0x8003fff8);
    S0 = add(S0, immediate(1));
    STEP(0x8003fffc);
    V1 = add(S2, S0);
    STEP(0x80040000);
    known(&V0, 0x10);
    STEP(0x80040004);
    STEP(0x80040008);
    TRY(store(run, 2, 3, 0, 1, 1, 0x80040008));
    goto loop_test;
  }
  STEP(0x8004000c);
  STEP(0x80040010);
  known(&T0, 8);
  TRY(equal_decision(run, V1, V0, 0x8004000c, &decision));
  if (decision) {
    STEP(0x80040014);
    S0 = add(S0, immediate(1));
    STEP(0x80040018);
    V1 = add(S2, S0);
    STEP(0x8004001c);
    known(&V0, 0x0c);
    STEP(0x80040020);
    STEP(0x80040024);
    TRY(store(run, 2, 3, 0, 1, 1, 0x80040024));
    goto loop_test;
  }
  STEP(0x80040028);
  STEP(0x8004002c);
  known(&V0, 9);
  TRY(equal_decision(run, V1, T0, 0x80040028, &decision));
  if (decision) {
    STEP(0x80040030);
    S0 = add(S0, immediate(1));
    STEP(0x80040034);
    V1 = add(S2, S0);
    STEP(0x80040038);
    known(&V0, 0x1f);
    STEP(0x8004003c);
    STEP(0x80040040);
    TRY(store(run, 2, 3, 0, 1, 1, 0x80040040));
    goto loop_test;
  }
  STEP(0x80040044);
  comparison = V0;
  STEP(0x80040048);
  known(&V0, 0x0f);
  TRY(equal_decision(run, V1, comparison, 0x80040044, &decision));
  if (!decision)
    goto loop_test;
  STEP(0x8004004c);
  S0 = add(S0, immediate(1));
  STEP(0x80040050);
  V1 = add(S2, S0);
  STEP(0x80040054);
  STEP(0x80040058);
  TRY(store(run, 2, 3, 0, 1, 1, 0x80040058));
  goto loop_test;

state_0a:
  STEP(0x8004005c);
  known(&RA, 0x80040064u);
  STEP(0x80040060);
  S0 = add(S0, immediate(UINT32_MAX));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8004005C, 0x8004005c, 0x80040060,
             0x80042288, 0));
  STEP(0x80040064);
  STEP(0x80040068);
  goto loop_test;

state_0b:
  STEP(0x8004006c);
  known(&RA, 0x80040074u);
  STEP(0x80040070);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8004006C, 0x8004006c, 0x80040070,
             0x80053f4c, 0));
  STEP(0x80040074);
  S3 = V0;
state_28_common_result:
  STEP(0x80040078);
  V0 = shift_left(S3, 16);
  STEP(0x8004007c);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x80040080);
  V0 = signed_less(V1, immediate(2));
  predicate = V0;
  STEP(0x80040084);
  STEP(0x80040088);
  TRY(zero_decision(run, predicate, 0x80040084, &decision));
  if (!decision)
    goto pop_state;
  STEP(0x8004008c);
  STEP(0x80040090);
  TRY(store(run, 0, 29, 0x18, 1, 1, 0x80040090));
  TRY(equal_decision(run, V1, S5, 0x8004008c, &decision));
  if (decision)
    goto reset_stack_push_06;
  STEP(0x80040094);
  STEP(0x80040098);
  known(&S0, 1);
  goto reset_stack_push_1c;

state_28:
  STEP(0x8004009c);
  known(&RA, 0x800400a4u);
  STEP(0x800400a0);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8004009C, 0x8004009c, 0x800400a0,
             0x8005428c, 0));
  STEP(0x800400a4);
  STEP(0x800400a8);
  S3 = V0;
  goto state_28_common_result;

state_29:
  STEP(0x800400ac);
  known(&RA, 0x800400b4u);
  STEP(0x800400b0);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800400AC, 0x800400ac, 0x800400b0,
             0x8005460c, 0));
  STEP(0x800400b4);
  S3 = V0;
  STEP(0x800400b8);
  V0 = shift_left(S3, 16);
  STEP(0x800400bc);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x800400c0);
  V0 = signed_less(V1, immediate(2));
  predicate = V0;
  STEP(0x800400c4);
  STEP(0x800400c8);
  TRY(zero_decision(run, predicate, 0x800400c4, &decision));
  if (!decision)
    goto pop_state;
  STEP(0x800400cc);
  STEP(0x800400d0);
  TRY(store(run, 0, 29, 0x18, 1, 1, 0x800400d0));
  TRY(equal_decision(run, V1, S5, 0x800400cc, &decision));
  if (!decision) {
    STEP(0x800400e0);
    known(&S0, 1);
    goto reset_stack_push_1c;
  }
reset_stack_push_06:
  STEP(0x800400d4);
  known(&S0, 1);
  STEP(0x800400d8);
  STEP(0x800400dc);
  TRY(store(run, 23, 29, 0x19, 1, 1, 0x800400dc));
  goto loop_test;
reset_stack_push_1c:
  STEP(0x800400e4);
  known(&V0, 0x1c);
  STEP(0x800400e8);
  STEP(0x800400ec);
  TRY(store(run, 2, 29, 0x19, 1, 1, 0x800400ec));
  goto loop_test;

state_0c:
  STEP(0x800400f0);
  TRY(load(run, 4, 20, 0x70e, 2, 2, 0x800400f0));
  A0 = sign_extend_half(A0);
  STEP(0x800400f4);
  known(&RA, 0x800400fcu);
  STEP(0x800400f8);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800400F4, 0x800400f4, 0x800400f8,
             0x80056aec, 1));
  STEP(0x800400fc);
  S3 = V0;
  STEP(0x80040100);
  V0 = shift_left(S3, 16);
  STEP(0x80040104);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x80040108);
  STEP(0x8004010c);
  known(&V0, 3);
  TRY(equal_decision(run, V1, S5, 0x80040108, &decision));
  if (decision)
    goto push_state_24;
  STEP(0x80040110);
  STEP(0x80040114);
  TRY(equal_decision(run, V1, V0, 0x80040110, &decision));
  if (decision)
    goto push_state_23;
  STEP(0x80040118);
  STEP(0x8004011c);
  S0 = add(S0, immediate(UINT32_MAX));
  goto loop_test;

state_0d:
  STEP(0x80040120);
  TRY(load(run, 4, 20, 0x70e, 2, 2, 0x80040120));
  A0 = sign_extend_half(A0);
  STEP(0x80040124);
  TRY(load(run, 5, 20, 0x710, 2, 2, 0x80040124));
  A1 = sign_extend_half(A1);
  STEP(0x80040128);
  known(&RA, 0x80040130u);
  STEP(0x8004012c);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040128, 0x80040128, 0x8004012c,
             0x80056cd0, 2));
  STEP(0x80040130);
  S3 = V0;
  STEP(0x80040134);
  V0 = shift_left(S3, 16);
  STEP(0x80040138);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x8004013c);
  STEP(0x80040140);
  known(&V0, 3);
  TRY(equal_decision(run, V1, S5, 0x8004013c, &decision));
  if (decision)
    goto push_state_24;
  STEP(0x80040144);
  STEP(0x80040148);
  TRY(equal_decision(run, V1, V0, 0x80040144, &decision));
  if (decision)
    goto push_state_23;
  STEP(0x8004014c);
  STEP(0x80040150);
  S0 = add(S0, immediate(UINT32_MAX));
  goto loop_test;

state_0e:
  STEP(0x80040154);
  TRY(load(run, 4, 20, 0x70e, 2, 2, 0x80040154));
  A0 = sign_extend_half(A0);
  STEP(0x80040158);
  known(&RA, 0x80040160u);
  STEP(0x8004015c);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040158, 0x80040158, 0x8004015c,
             0x80056f9c, 1));
  STEP(0x80040160);
  S3 = V0;
  STEP(0x80040164);
  V0 = shift_left(S3, 16);
  STEP(0x80040168);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x8004016c);
  STEP(0x80040170);
  known(&V0, 3);
  TRY(equal_decision(run, V1, S5, 0x8004016c, &decision));
  if (decision)
    goto push_state_24;
  STEP(0x80040174);
  STEP(0x80040178);
  TRY(equal_decision(run, V1, V0, 0x80040174, &decision));
  if (decision)
    goto push_state_23;
  STEP(0x8004017c);
  STEP(0x80040180);
  S0 = add(S0, immediate(UINT32_MAX));
  goto loop_test;

state_0f:
  STEP(0x80040184);
  known(&RA, 0x8004018cu);
  STEP(0x80040188);
  S0 = add(S0, immediate(UINT32_MAX));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040184, 0x80040184, 0x80040188,
             0x80057508, 0));
  STEP(0x8004018c);
  STEP(0x80040190);
  goto loop_test;

state_10:
  STEP(0x80040194);
  known(&RA, 0x8004019cu);
  STEP(0x80040198);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040194, 0x80040194, 0x80040198,
             0x800592c4, 0));
  STEP(0x8004019c);
  S3 = V0;
  STEP(0x800401a0);
  V0 = shift_left(S3, 16);
  STEP(0x800401a4);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x800401a8);
  STEP(0x800401ac);
  known(&V0, 3);
  TRY(equal_decision(run, V1, S5, 0x800401a8, &decision));
  if (decision)
    goto push_state_24;
  STEP(0x800401b0);
  STEP(0x800401b4);
  TRY(equal_decision(run, V1, V0, 0x800401b0, &decision));
  if (decision)
    goto push_state_23;
  STEP(0x800401b8);
  STEP(0x800401bc);
  S0 = add(S0, immediate(UINT32_MAX));
  goto loop_test;

state_11:
  STEP(0x800401c0);
  TRY(load(run, 4, 20, 0x70e, 2, 2, 0x800401c0));
  A0 = sign_extend_half(A0);
  STEP(0x800401c4);
  known(&RA, 0x800401ccu);
  STEP(0x800401c8);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800401C4, 0x800401c4, 0x800401c8,
             0x8005721c, 1));
  STEP(0x800401cc);
  S3 = V0;
  STEP(0x800401d0);
  V0 = shift_left(S3, 16);
  STEP(0x800401d4);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x800401d8);
  STEP(0x800401dc);
  known(&V0, 3);
  TRY(equal_decision(run, V1, S5, 0x800401d8, &decision));
  if (decision)
    goto push_state_24;
  STEP(0x800401e0);
  STEP(0x800401e4);
  TRY(equal_decision(run, V1, V0, 0x800401e0, &decision));
  if (!decision)
    goto pop_state;
push_state_23:
  STEP(0x800401e8);
  S0 = add(S0, immediate(1));
  STEP(0x800401ec);
  V1 = add(S2, S0);
  STEP(0x800401f0);
  known(&V0, 0x23);
  STEP(0x800401f4);
  STEP(0x800401f8);
  TRY(store(run, 2, 3, 0, 1, 1, 0x800401f8));
  goto loop_test;

state_12:
  STEP(0x800401fc);
  known(&RA, 0x80040204u);
  STEP(0x80040200);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800401FC, 0x800401fc, 0x80040200,
             0x80041a38, 0));
  STEP(0x80040204);
  S3 = V0;
  STEP(0x80040208);
  V0 = shift_left(S3, 16);
  STEP(0x8004020c);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x80040210);
  V0 = signed_less(V1, immediate(2));
  predicate = V0;
  STEP(0x80040214);
  STEP(0x80040218);
  TRY(zero_decision(run, predicate, 0x80040214, &decision));
  if (!decision)
    goto pop_state;
  STEP(0x8004021c);
  STEP(0x80040220);
  known(&V0, 3);
  TRY(equal_decision(run, V1, S5, 0x8004021c, &decision));
  if (decision)
    goto push_state_17;
  STEP(0x80040224);
  comparison = V0;
  STEP(0x80040228);
  known(&V0, 4);
  TRY(equal_decision(run, V1, comparison, 0x80040224, &decision));
  if (decision)
    goto push_state_14;
  STEP(0x8004022c);
  comparison = V0;
  STEP(0x80040230);
  known(&V0, 5);
  TRY(equal_decision(run, V1, comparison, 0x8004022c, &decision));
  if (decision)
    goto push_state_13;
  STEP(0x80040234);
  STEP(0x80040238);
  TRY(equal_decision(run, V1, V0, 0x80040234, &decision));
  if (decision)
    goto push_state_15;
  STEP(0x8004023c);
  STEP(0x80040240);
  known(&V0, 7);
  TRY(equal_decision(run, V1, S7, 0x8004023c, &decision));
  if (decision) {
    STEP(0x80040244);
    S0 = add(S0, immediate(1));
    STEP(0x80040248);
    V1 = add(S2, S0);
    STEP(0x8004024c);
    known(&V0, 0x18);
    STEP(0x80040250);
    STEP(0x80040254);
    TRY(store(run, 2, 3, 0, 1, 1, 0x80040254));
    goto loop_test;
  }
  STEP(0x80040258);
  STEP(0x8004025c);
  known(&T0, 8);
  TRY(equal_decision(run, V1, V0, 0x80040258, &decision));
  if (decision) {
    STEP(0x80040260);
    S0 = add(S0, immediate(1));
    STEP(0x80040264);
    V1 = add(S2, S0);
    STEP(0x80040268);
    known(&V0, 0x16);
    STEP(0x8004026c);
    STEP(0x80040270);
    TRY(store(run, 2, 3, 0, 1, 1, 0x80040270));
    goto loop_test;
  }
  STEP(0x80040274);
  STEP(0x80040278);
  known(&V0, 0x19);
  TRY(equal_decision(run, V1, T0, 0x80040274, &decision));
  if (!decision)
    goto loop_test;
  STEP(0x8004027c);
  S0 = add(S0, immediate(1));
  STEP(0x80040280);
  V1 = add(S2, S0);
  STEP(0x80040284);
  STEP(0x80040288);
  TRY(store(run, 2, 3, 0, 1, 1, 0x80040288));
  goto loop_test;

state_13:
  STEP(0x8004028c);
  known(&RA, 0x80040294u);
  STEP(0x80040290);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8004028C, 0x8004028c, 0x80040290,
             0x8005cf78, 0));
  STEP(0x80040294);
  S3 = V0;
  STEP(0x80040298);
  V0 = shift_left(S3, 16);
  STEP(0x8004029c);
  V0 = shift_right_arithmetic(V0, 16);
  predicate = V0;
  STEP(0x800402a0);
  STEP(0x800402a4);
  known(&V0, 0x27);
  TRY(equal_decision(run, predicate, S5, 0x800402a0, &decision));
  if (decision) {
    STEP(0x800402a8);
    TRY(load(run, 3, 20, 0x722, 2, 2, 0x800402a8));
    V1 = sign_extend_half(V1);
    STEP(0x800402ac);
    STEP(0x800402b0);
    STEP(0x800402b4);
    TRY(equal_decision(run, V1, V0, 0x800402b0, &decision));
    if (decision)
      goto clear_current_state;
    STEP(0x800402b8);
    STEP(0x800402bc);
    S0 = add(S0, immediate(1));
    goto push_state_27_at_current;
  }
  STEP(0x800402c0);
  TRY(load(run, 3, 20, 0x722, 2, 2, 0x800402c0));
  V1 = sign_extend_half(V1);
  STEP(0x800402c4);
  STEP(0x800402c8);
  STEP(0x800402cc);
  TRY(equal_decision(run, V1, V0, 0x800402c8, &decision));
  if (!decision)
    goto pop_state;
clear_current_state:
  STEP(0x800402d0);
  STEP(0x800402d4);
  TRY(store(run, 0, 20, 0x720, 2, 2, 0x800402d4));
  goto pop_state;

state_14:
  STEP(0x800402d8);
  known(&RA, 0x800402e0u);
  STEP(0x800402dc);
  S0 = add(S0, immediate(UINT32_MAX));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800402D8, 0x800402d8, 0x800402dc,
             0x80059220, 0));
  STEP(0x800402e0);
  STEP(0x800402e4);
  goto loop_test;

state_15:
  STEP(0x800402e8);
  known(&RA, 0x800402f0u);
  STEP(0x800402ec);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800402E8, 0x800402e8, 0x800402ec,
             0x8005bf34, 0));
  STEP(0x800402f0);
  S3 = V0;
  STEP(0x800402f4);
  V0 = shift_left(S3, 16);
  STEP(0x800402f8);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x800402fc);
  STEP(0x80040300);
  known(&V0, 3);
  TRY(equal_decision(run, V1, S5, 0x800402fc, &decision));
  if (decision) {
    STEP(0x80040304);
    S0 = add(S0, immediate(1));
    STEP(0x80040308);
    V1 = add(S2, S0);
    STEP(0x8004030c);
    known(&V0, 0x26);
    STEP(0x80040310);
    STEP(0x80040314);
    TRY(store(run, 2, 3, 0, 1, 1, 0x80040314));
    goto loop_test;
  }
  STEP(0x80040318);
  comparison = V0;
  STEP(0x8004031c);
  known(&V0, 4);
  TRY(equal_decision(run, V1, comparison, 0x80040318, &decision));
  if (decision) {
    STEP(0x80040320);
    S0 = add(S0, immediate(1));
    STEP(0x80040324);
    V1 = add(S2, S0);
    STEP(0x80040328);
    known(&V0, 0x25);
    STEP(0x8004032c);
    STEP(0x80040330);
    TRY(store(run, 2, 3, 0, 1, 1, 0x80040330));
    goto loop_test;
  }
  STEP(0x80040334);
  STEP(0x80040338);
  TRY(equal_decision(run, V1, V0, 0x80040334, &decision));
  if (!decision)
    goto pop_state;
  STEP(0x8004033c);
  S0 = add(S0, immediate(1));
push_state_27_at_current:
  STEP(0x80040340);
  V1 = add(S2, S0);
  STEP(0x80040344);
  known(&V0, 0x27);
  STEP(0x80040348);
  STEP(0x8004034c);
  TRY(store(run, 2, 3, 0, 1, 1, 0x8004034c));
  goto loop_test;

state_16:
  STEP(0x80040350);
  known(&RA, 0x80040358u);
  STEP(0x80040354);
  S0 = add(S0, immediate(UINT32_MAX));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040350, 0x80040350, 0x80040354,
             0x800431d4, 0));
  STEP(0x80040358);
  STEP(0x8004035c);
  goto loop_test;

state_17:
  STEP(0x80040360);
  known(&RA, 0x80040368u);
  STEP(0x80040364);
  S0 = add(S0, immediate(UINT32_MAX));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040360, 0x80040360, 0x80040364,
             0x80058a18, 0));
  STEP(0x80040368);
  STEP(0x8004036c);
  goto loop_test;

state_18:
  STEP(0x80040370);
  known(&RA, 0x80040378u);
  STEP(0x80040374);
  S0 = add(S0, immediate(UINT32_MAX));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040370, 0x80040370, 0x80040374,
             0x8005b500, 0));
  STEP(0x80040378);
  STEP(0x8004037c);
  goto loop_test;

state_19:
  STEP(0x80040380);
  known(&RA, 0x80040388u);
  STEP(0x80040384);
  S0 = add(S0, immediate(UINT32_MAX));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040380, 0x80040380, 0x80040384,
             0x8005bc8c, 0));
  STEP(0x80040388);
  STEP(0x8004038c);
  goto loop_test;

state_1a:
  STEP(0x80040390);
  known(&RA, 0x80040398u);
  STEP(0x80040394);
  known(&A0, 0);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040390, 0x80040390, 0x80040394,
             0x8003f778, 1));
  STEP(0x80040398);
  known(&RA, 0x800403a0u);
  STEP(0x8004039c);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040398, 0x80040398, 0x8004039c,
             0x800482f0, 0));
  STEP(0x800403a0);
  S3 = V0;
  STEP(0x800403a4);
  V0 = shift_left(S3, 16);
  STEP(0x800403a8);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x800403ac);
  STEP(0x800403b0);
  known(&V0, 0x0c);
  TRY(equal_decision(run, V1, S6, 0x800403ac, &decision));
  if (decision) {
    STEP(0x800403b4);
    STEP(0x800403b8);
    TRY(store(run, 0, 20, 0x78, 2, 2, 0x800403b8));
    goto pop_state;
  }
  STEP(0x800403bc);
  STEP(0x800403c0);
  TRY(equal_decision(run, V1, V0, 0x800403bc, &decision));
  if (decision)
    goto replace_state_1b;
  STEP(0x800403d4);
  known(&RA, 0x800403dcu);
  STEP(0x800403d8);
  known(&A0, 1);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800403D4, 0x800403d4, 0x800403d8,
             0x8004875c, 1));
  STEP(0x800403dc);
  known(&RA, 0x800403e4u);
  STEP(0x800403e0);
  S0 = add(S0, immediate(1));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800403DC, 0x800403dc, 0x800403e0,
             0x800487e0, 0));
  STEP(0x800403e4);
  V1 = add(S2, S0);
  STEP(0x800403e8);
  TRY(store(run, 2, 3, 0, 1, 1, 0x800403e8));
  STEP(0x800403ec);
  V0 = and_constant(V0, 0xff);
  STEP(0x800403f0);
  known(&V1, 0x1c);
  STEP(0x800403f4);
  STEP(0x800403f8);
  known(&T0, 1);
  TRY(equal_decision(run, V0, V1, 0x800403f4, &decision));
  if (decision) {
    STEP(0x800403fc);
    STEP(0x80040400);
    TRY(store(run, 0, 29, 0x48, 1, 1, 0x80040400));
    goto loop_test;
  }
  STEP(0x80040404);
  TRY(store(run, 8, 29, 0x48, 1, 1, 0x80040404));
  STEP(0x80040408);
  STEP(0x8004040c);
  TRY(store(run, 30, 20, 0x78, 2, 2, 0x8004040c));
  goto loop_test;

replace_state_1b:
  STEP(0x800403c4);
  V1 = add(S2, S0);
  STEP(0x800403c8);
  known(&V0, 0x1b);
  STEP(0x800403cc);
  STEP(0x800403d0);
  TRY(store(run, 2, 3, 0, 1, 1, 0x800403d0));
  goto loop_test;

state_1b:
  STEP(0x80040410);
  known(&RA, 0x80040418u);
  STEP(0x80040414);
  S1 = S3;
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040410, 0x80040410, 0x80040414,
             0x80047618, 0));
  STEP(0x80040418);
  S3 = V0;
  STEP(0x8004041c);
  V0 = shift_left(S1, 16);
  STEP(0x80040420);
  V0 = shift_right_arithmetic(V0, 16);
  STEP(0x80040424);
  known(&V1, 0x0c);
  predicate = V0;
  STEP(0x80040428);
  STEP(0x8004042c);
  V0 = shift_left(S3, 16);
  TRY(equal_decision(run, predicate, V1, 0x80040428, &decision));
  if (decision)
    goto state_1b_push_1a;
  {
    STEP(0x80040430);
    V0 = shift_right_arithmetic(V0, 16);
    predicate = V0;
    STEP(0x80040434);
    STEP(0x80040438);
    TRY(equal_decision(run, predicate, S6, 0x80040434, &decision));
    if (decision) {
      STEP(0x8004043c);
      STEP(0x80040440);
      TRY(store(run, 0, 20, 0x78, 2, 2, 0x80040440));
      goto pop_state;
    }
  }
state_1b_team_flag:
  STEP(0x80040444);
  TRY(load(run, 4, 20, 0x14, 4, 4, 0x80040444));
  STEP(0x80040448);
  STEP(0x8004044c);
  TRY(load(run, 3, 4, 0x2f60, 1, 1, 0x8004044c));
  STEP(0x80040450);
  known(&V0, 0x63);
  STEP(0x80040454);
  STEP(0x80040458);
  TRY(equal_decision(run, V1, V0, 0x80040454, &decision));
  if (!decision) {
    STEP(0x8004045c);
    TRY(store(run, 30, 4, 0x2f60, 1, 1, 0x8004045c));
  }
  STEP(0x80040460);
  S0 = add(S0, immediate(1));
state_1b_push_1a:
  STEP(0x80040464);
  V1 = add(S2, S0);
  STEP(0x80040468);
  known(&V0, 0x1a);
  STEP(0x8004046c);
  STEP(0x80040470);
  TRY(store(run, 2, 3, 0, 1, 1, 0x80040470));
  goto loop_test;

state_1c:
  STEP(0x80040474);
  known(&RA, 0x8004047cu);
  STEP(0x80040478);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040474, 0x80040474, 0x80040478,
             0x80049c40, 0));
  STEP(0x8004047c);
  S3 = V0;
  STEP(0x80040480);
  V0 = shift_left(S3, 16);
  STEP(0x80040484);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x80040488);
  STEP(0x8004048c);
  TRY(equal_decision(run, V1, S6, 0x80040488, &decision));
  if (decision) {
    STEP(0x80040490);
    STEP(0x80040494);
    known(&S3, 1);
    goto pop_state;
  }
  STEP(0x80040498);
  STEP(0x8004049c);
  known(&V0, 4);
  TRY(equal_decision(run, V1, S5, 0x80040498, &decision));
  if (decision)
    goto push_state_05_at_current;
  STEP(0x800404b4);
  STEP(0x800404b8);
  known(&V0, 5);
  TRY(equal_decision(run, V1, immediate(4), 0x800404b4, &decision));
  if (decision) {
    STEP(0x800404bc);
    TRY(load(run, 2, 20, 0x14, 4, 4, 0x800404bc));
    STEP(0x800404c0);
    STEP(0x800404c4);
    TRY(load(run, 2, 2, 0x2fc3, 1, 1, 0x800404c4));
    V0 = sign_extend_byte(V0);
    STEP(0x800404c8);
    predicate = V0;
    STEP(0x800404cc);
    STEP(0x800404d0);
    known(&V0, 0x1e);
    TRY(equal_decision(run, predicate, S6, 0x800404cc, &decision));
    if (decision)
      goto push_state_13;
    STEP(0x800404d4);
    S0 = add(S0, immediate(1));
    STEP(0x800404d8);
    V1 = add(S2, S0);
    STEP(0x800404dc);
    STEP(0x800404e0);
    TRY(store(run, 2, 3, 0, 1, 1, 0x800404e0));
    goto loop_test;
  }
  STEP(0x800404e4);
  STEP(0x800404e8);
  TRY(equal_decision(run, V1, V0, 0x800404e4, &decision));
  if (decision)
    goto push_state_09;
  STEP(0x80040500);
  STEP(0x80040504);
  known(&V0, 7);
  TRY(equal_decision(run, V1, S7, 0x80040500, &decision));
  if (decision)
    goto push_state_1d;
  STEP(0x8004051c);
  STEP(0x80040520);
  known(&T0, 8);
  TRY(equal_decision(run, V1, V0, 0x8004051c, &decision));
  if (decision) {
    STEP(0x80040524);
    S0 = add(S0, immediate(1));
    STEP(0x80040528);
    V1 = add(S2, S0);
    STEP(0x8004052c);
    known(&V0, 0x29);
    STEP(0x80040530);
    known(&AT, 0x80010000u);
    STEP(0x80040534);
    TRY(store(run, 21, 1, 0x501c, 4, 4, 0x80040534));
    STEP(0x80040538);
    STEP(0x8004053c);
    TRY(store(run, 2, 3, 0, 1, 1, 0x8004053c));
    goto loop_test;
  }
  STEP(0x80040540);
  STEP(0x80040544);
  TRY(equal_decision(run, V1, T0, 0x80040540, &decision));
  if (!decision)
    goto loop_test;
  STEP(0x80040548);
  known(&RA, 0x80040550u);
  STEP(0x8004054c);
  known(&S0, 0);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040548, 0x80040548, 0x8004054c,
             0x800435a4, 0));
  STEP(0x80040550);
  STEP(0x80040554);
  TRY(store(run, 0, 29, 0x18, 1, 1, 0x80040554));
  goto loop_test;

state_1d:
  STEP(0x800405d8);
  known(&RA, 0x800405e0u);
  STEP(0x800405dc);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800405D8, 0x800405d8, 0x800405dc,
             0x80047194, 0));
  STEP(0x800405e0);
  S3 = V0;
  STEP(0x800405e4);
  V0 = shift_left(S3, 16);
  STEP(0x800405e8);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x800405ec);
  V0 = signed_less(V1, immediate(2));
  predicate = V0;
  STEP(0x800405f0);
  STEP(0x800405f4);
  known(&V0, 5);
  TRY(zero_decision(run, predicate, 0x800405f0, &decision));
  if (!decision)
    goto pop_state;
  STEP(0x800405f8);
  STEP(0x800405fc);
  TRY(equal_decision(run, V1, V0, 0x800405f8, &decision));
  if (decision)
    goto push_state_01;
  STEP(0x80040610);
  STEP(0x80040614);
  known(&V0, 7);
  TRY(equal_decision(run, V1, S7, 0x80040610, &decision));
  if (decision)
    goto push_state_02;
  STEP(0x80040628);
  STEP(0x8004062c);
  known(&T0, 8);
  TRY(equal_decision(run, V1, V0, 0x80040628, &decision));
  if (decision)
    goto push_state_0a;
  STEP(0x80040644);
  STEP(0x80040648);
  TRY(equal_decision(run, V1, T0, 0x80040644, &decision));
  if (!decision)
    goto loop_test;
  STEP(0x8004064c);
  known(&S0, 0);
  STEP(0x80040650);
  STEP(0x80040654);
  TRY(store(run, 0, 29, 0x18, 1, 1, 0x80040654));
  goto loop_test;

state_1e:
  STEP(0x80040558);
  known(&RA, 0x80040560u);
  STEP(0x8004055c);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040558, 0x80040558, 0x8004055c,
             0x80046f80, 0));
  STEP(0x80040560);
  S3 = V0;
  STEP(0x80040564);
  V0 = shift_left(S3, 16);
  STEP(0x80040568);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x8004056c);
  V0 = signed_less(V1, immediate(2));
  predicate = V0;
  STEP(0x80040570);
  STEP(0x80040574);
  TRY(zero_decision(run, predicate, 0x80040570, &decision));
  if (!decision) {
    STEP(0x80040578);
    S0 = add(S0, immediate(UINT32_MAX));
  }
  STEP(0x8004057c);
  STEP(0x80040580);
  known(&V0, 3);
  TRY(equal_decision(run, V1, S5, 0x8004057c, &decision));
  if (decision)
    goto push_state_17;
  STEP(0x80040598);
  comparison = V0;
  STEP(0x8004059c);
  known(&V0, 4);
  TRY(equal_decision(run, V1, comparison, 0x80040598, &decision));
  if (decision)
    goto push_state_14;
  STEP(0x800405b4);
  comparison = V0;
  STEP(0x800405b8);
  known(&V0, 5);
  TRY(equal_decision(run, V1, comparison, 0x800405b4, &decision));
  if (decision)
    goto push_state_13;
  STEP(0x800405bc);
  STEP(0x800405c0);
  TRY(equal_decision(run, V1, V0, 0x800405bc, &decision));
  if (!decision)
    goto loop_test;
push_state_15:
  STEP(0x800405c4);
  S0 = add(S0, immediate(1));
  STEP(0x800405c8);
  V1 = add(S2, S0);
  STEP(0x800405cc);
  known(&V0, 0x15);
  STEP(0x800405d0);
  STEP(0x800405d4);
  TRY(store(run, 2, 3, 0, 1, 1, 0x800405d4));
  goto loop_test;

state_1f:
  STEP(0x80040658);
  known(&RA, 0x80040660u);
  STEP(0x8004065c);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040658, 0x80040658, 0x8004065c,
             0x8004dae8, 0));
  STEP(0x80040660);
  S3 = V0;
  STEP(0x80040664);
  V0 = shift_left(S3, 16);
  STEP(0x80040668);
  V1 = shift_right_arithmetic(V0, 16);
  STEP(0x8004066c);
  STEP(0x80040670);
  known(&V0, 3);
  TRY(equal_decision(run, V1, S5, 0x8004066c, &decision));
  if (decision) {
    STEP(0x80040674);
    S0 = add(S0, immediate(1));
    STEP(0x80040678);
    V1 = add(S2, S0);
    STEP(0x8004067c);
    known(&V0, 0x20);
    STEP(0x80040680);
    STEP(0x80040684);
    TRY(store(run, 2, 3, 0, 1, 1, 0x80040684));
    goto loop_test;
  }
  STEP(0x80040688);
  comparison = V0;
  STEP(0x8004068c);
  known(&V0, 4);
  TRY(equal_decision(run, V1, comparison, 0x80040688, &decision));
  if (decision) {
    STEP(0x80040690);
    S0 = add(S0, immediate(1));
    STEP(0x80040694);
    V1 = add(S2, S0);
    STEP(0x80040698);
    known(&V0, 0x22);
    STEP(0x8004069c);
    STEP(0x800406a0);
    TRY(store(run, 2, 3, 0, 1, 1, 0x800406a0));
    goto loop_test;
  }
  STEP(0x800406a4);
  comparison = V0;
  STEP(0x800406a8);
  known(&V0, 0x21);
  TRY(equal_decision(run, V1, comparison, 0x800406a4, &decision));
  if (!decision)
    goto pop_state;
  STEP(0x800406ac);
  S0 = add(S0, immediate(1));
  STEP(0x800406b0);
  V1 = add(S2, S0);
  STEP(0x800406b4);
  STEP(0x800406b8);
  TRY(store(run, 2, 3, 0, 1, 1, 0x800406b8));
  goto loop_test;

state_20:
  STEP(0x800406bc);
  known(&RA, 0x800406c4u);
  STEP(0x800406c0);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800406BC, 0x800406bc, 0x800406c0,
             0x8004e46c, 0));
  STEP(0x800406c4);
  S3 = V0;
  STEP(0x800406c8);
  V0 = shift_left(S3, 16);
  STEP(0x800406cc);
  V0 = shift_right_arithmetic(V0, 16);
  STEP(0x800406d0);
  V0 = signed_less(V0, immediate(2));
  predicate = V0;
  STEP(0x800406d4);
  STEP(0x800406d8);
  A0 = add(S3, immediate(UINT32_C(0xfffffffe)));
  TRY(zero_decision(run, predicate, 0x800406d4, &decision));
  if (!decision)
    goto pop_state;
  STEP(0x800406dc);
  A0 = shift_left(A0, 16);
  STEP(0x800406e0);
  A0 = shift_right_arithmetic(A0, 16);
  STEP(0x800406e4);
  known(&V0, 0x2b);
  STEP(0x800406e8);
  known(&RA, 0x800406f0u);
  STEP(0x800406ec);
  TRY(store(run, 2, 20, 0x720, 2, 2, 0x800406ec));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800406E8, 0x800406e8, 0x800406ec,
             0x8004d514, 1));
  STEP(0x800406f0);
  known(&V0, 0x20);
  STEP(0x800406f4);
  STEP(0x800406f8);
  TRY(store(run, 2, 20, 0x720, 2, 2, 0x800406f8));
  goto loop_test;

state_21:
  STEP(0x800406fc);
  known(&RA, 0x80040704u);
  STEP(0x80040700);
  S0 = add(S0, immediate(UINT32_MAX));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800406FC, 0x800406fc, 0x80040700,
             0x8004e768, 0));
  STEP(0x80040704);
  STEP(0x80040708);
  goto loop_test;

state_22:
  STEP(0x8004070c);
  known(&RA, 0x80040714u);
  STEP(0x80040710);
  known(&A0, UINT32_MAX);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8004070C, 0x8004070c, 0x80040710,
             0x8004d514, 1));
  STEP(0x80040714);
  STEP(0x80040718);
  S0 = add(S0, immediate(UINT32_MAX));
  goto loop_test;

state_23:
  STEP(0x8004071c);
  known(&RA, 0x80040724u);
  STEP(0x80040720);
  S0 = add(S0, immediate(UINT32_MAX));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8004071C, 0x8004071c, 0x80040720,
             0x8005a880, 0));
  STEP(0x80040724);
  STEP(0x80040728);
  goto loop_test;

state_24:
  STEP(0x8004072c);
  known(&RA, 0x80040734u);
  STEP(0x80040730);
  S0 = add(S0, immediate(UINT32_MAX));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8004072C, 0x8004072c, 0x80040730,
             0x8005a538, 0));
  STEP(0x80040734);
  STEP(0x80040738);
  goto loop_test;

state_25:
  STEP(0x8004073c);
  known(&RA, 0x80040744u);
  STEP(0x80040740);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8004073C, 0x8004073c, 0x80040740,
             0x8005cb2c, 0));
  STEP(0x80040744);
  S3 = V0;
  STEP(0x80040748);
  V0 = shift_left(S3, 16);
  STEP(0x8004074c);
  V0 = shift_right_arithmetic(V0, 16);
  STEP(0x80040750);
  STEP(0x80040754);
  TRY(equal_decision(run, V0, S5, 0x80040750, &decision));
  if (!decision)
    goto pop_state;
push_state_24:
  STEP(0x80040758);
  S0 = add(S0, immediate(1));
  STEP(0x8004075c);
  V1 = add(S2, S0);
  STEP(0x80040760);
  known(&V0, 0x24);
  STEP(0x80040764);
  STEP(0x80040768);
  TRY(store(run, 2, 3, 0, 1, 1, 0x80040768));
  goto loop_test;

state_26:
  STEP(0x8004076c);
  known(&RA, 0x80040774u);
  STEP(0x80040770);
  S0 = add(S0, immediate(UINT32_MAX));
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8004076C, 0x8004076c, 0x80040770,
             0x8005c4e0, 0));
  STEP(0x80040774);
  STEP(0x80040778);
  S3 = V0;
  goto loop_test;

state_27:
  STEP(0x8004077c);
  known(&RA, 0x80040784u);
  STEP(0x80040780);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_8004077C, 0x8004077c, 0x80040780,
             0x8005d46c, 0));
  STEP(0x80040784);
  S3 = V0;
  STEP(0x80040788);
  V0 = shift_left(S3, 16);
  STEP(0x8004078c);
  V0 = shift_right_arithmetic(V0, 16);
  predicate = V0;
  STEP(0x80040790);
  STEP(0x80040794);
  known(&V0, 0x13);
  TRY(equal_decision(run, predicate, S5, 0x80040790, &decision));
  if (decision) {
    STEP(0x80040798);
    TRY(load(run, 3, 20, 0x722, 2, 2, 0x80040798));
    V1 = sign_extend_half(V1);
    STEP(0x8004079c);
    STEP(0x800407a0);
    STEP(0x800407a4);
    TRY(equal_decision(run, V1, V0, 0x800407a0, &decision));
    if (decision)
      goto clear_current_state_27;
  push_state_13:
    STEP(0x800407a8);
    S0 = add(S0, immediate(1));
    STEP(0x800407ac);
    V1 = add(S2, S0);
    STEP(0x800407b0);
    known(&V0, 0x13);
    STEP(0x800407b4);
    STEP(0x800407b8);
    TRY(store(run, 2, 3, 0, 1, 1, 0x800407b8));
    goto loop_test;
  }
  STEP(0x800407bc);
  TRY(load(run, 3, 20, 0x722, 2, 2, 0x800407bc));
  V1 = sign_extend_half(V1);
  STEP(0x800407c0);
  STEP(0x800407c4);
  STEP(0x800407c8);
  TRY(equal_decision(run, V1, V0, 0x800407c4, &decision));
  if (!decision)
    goto pop_state;
clear_current_state_27:
  STEP(0x800407cc);
  STEP(0x800407d0);
  TRY(store(run, 0, 20, 0x720, 2, 2, 0x800407d0));
  goto pop_state;

state_2a:
  STEP(0x800407d4);
  known(&RA, 0x800407dcu);
  STEP(0x800407d8);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800407D4, 0x800407d4, 0x800407d8,
             0x8005d7d4, 0));
  goto pop_state;

replace_state_07:
  STEP(0x8003fccc);
  V1 = add(S2, S0);
  STEP(0x8003fcd0);
  known(&V0, 7);
  STEP(0x8003fcd4);
  STEP(0x8003fcd8);
  TRY(store(run, 2, 3, 0, 1, 1, 0x8003fcd8));
  goto loop_test;

push_state_05_at_current:
  STEP(0x800404a0);
  S0 = add(S0, immediate(1));
store_state_05_at_current:
  STEP(0x800404a4);
  V1 = add(S2, S0);
  STEP(0x800404a8);
  known(&V0, 5);
  STEP(0x800404ac);
  STEP(0x800404b0);
  TRY(store(run, 2, 3, 0, 1, 1, 0x800404b0));
  goto loop_test;

push_state_09:
  STEP(0x800404ec);
  S0 = add(S0, immediate(1));
  STEP(0x800404f0);
  V1 = add(S2, S0);
  STEP(0x800404f4);
  known(&V0, 9);
  STEP(0x800404f8);
  STEP(0x800404fc);
  TRY(store(run, 2, 3, 0, 1, 1, 0x800404fc));
  goto loop_test;

push_state_1d:
  STEP(0x80040508);
  S0 = add(S0, immediate(1));
  STEP(0x8004050c);
  V1 = add(S2, S0);
  STEP(0x80040510);
  known(&V0, 0x1d);
  STEP(0x80040514);
  STEP(0x80040518);
  TRY(store(run, 2, 3, 0, 1, 1, 0x80040518));
  goto loop_test;

push_state_17:
  STEP(0x80040584);
  S0 = add(S0, immediate(1));
  STEP(0x80040588);
  V1 = add(S2, S0);
  STEP(0x8004058c);
  known(&V0, 0x17);
  STEP(0x80040590);
  STEP(0x80040594);
  TRY(store(run, 2, 3, 0, 1, 1, 0x80040594));
  goto loop_test;

push_state_14:
  STEP(0x800405a0);
  S0 = add(S0, immediate(1));
  STEP(0x800405a4);
  V1 = add(S2, S0);
  STEP(0x800405a8);
  known(&V0, 0x14);
  STEP(0x800405ac);
  STEP(0x800405b0);
  TRY(store(run, 2, 3, 0, 1, 1, 0x800405b0));
  goto loop_test;

push_state_01:
  STEP(0x80040600);
  S0 = add(S0, immediate(1));
  STEP(0x80040604);
  V0 = add(S2, S0);
  STEP(0x80040608);
  STEP(0x8004060c);
  TRY(store(run, 30, 2, 0, 1, 1, 0x8004060c));
  goto loop_test;

push_state_02:
  STEP(0x80040618);
  S0 = add(S0, immediate(1));
  STEP(0x8004061c);
  V0 = add(S2, S0);
  STEP(0x80040620);
  STEP(0x80040624);
  TRY(store(run, 21, 2, 0, 1, 1, 0x80040624));
  goto loop_test;

push_state_0a:
  STEP(0x80040630);
  S0 = add(S0, immediate(1));
  STEP(0x80040634);
  V1 = add(S2, S0);
  STEP(0x80040638);
  known(&V0, 0x0a);
  STEP(0x8004063c);
  STEP(0x80040640);
  TRY(store(run, 2, 3, 0, 1, 1, 0x80040640));
  goto loop_test;

pop_state:
  STEP(0x800407dc);
  S0 = add(S0, immediate(UINT32_MAX));
loop_test:
  STEP(0x800407e0);
  predicate = S0;
  STEP(0x800407e4);
  TRY(equal_decision(run, predicate, S6, 0x800407e0, &decision));
  if (!decision)
    goto dispatch_loop;

  /* 0x800407E8..0x800408A8: unwind frontend services, update the two
   * selected-team bytes, and establish the roster-copy bases. */
  STEP(0x800407e8);
  known(&RA, 0x800407f0u);
  STEP(0x800407ec);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800407E8, 0x800407e8, 0x800407ec,
             0x80028b8c, 0));
  STEP(0x800407f0);
  known(&RA, 0x800407f8u);
  STEP(0x800407f4);
  known(&A0, 0);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800407F0, 0x800407f0, 0x800407f4,
             0x800804e8, 1));
  STEP(0x800407f8);
  known(&RA, 0x80040800u);
  STEP(0x800407fc);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800407F8, 0x800407f8, 0x800407fc,
             0x80028b8c, 0));
  STEP(0x80040800);
  known(&V0, 0x800f0000u);
  STEP(0x80040804);
  TRY(load(run, 2, 2, UINT32_C(0xfffff754), 4, 4, 0x80040804));
  STEP(0x80040808);
  predicate = V0;
  STEP(0x8004080c);
  STEP(0x80040810);
  TRY(zero_decision(run, predicate, 0x8004080c, &decision));
  if (!decision) {
    STEP(0x80040814);
    known(&AT, 0x800f0000u);
    STEP(0x80040818);
    TRY(store(run, 0, 1, UINT32_C(0xfffff754), 4, 4, 0x80040818));
  }
  STEP(0x8004081c);
  known(&V0, 0x80020000u);
  STEP(0x80040820);
  TRY(load(run, 2, 2, UINT32_C(0xffffedec), 2, 2, 0x80040820));
  STEP(0x80040824);
  predicate = V0;
  STEP(0x80040828);
  STEP(0x8004082c);
  TRY(zero_decision(run, predicate, 0x80040828, &decision));
  if (!decision) {
    STEP(0x80040830);
    known(&RA, 0x80040838u);
    STEP(0x80040834);
    TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040830, 0x80040830,
               0x80040834, 0x800357b0, 0));
  }
  STEP(0x80040838);
  TRY(load(run, 2, 20, 0x78, 2, 2, 0x80040838));
  STEP(0x8004083c);
  predicate = V0;
  STEP(0x80040840);
  STEP(0x80040844);
  known(&V0, 0xff);
  TRY(zero_decision(run, predicate, 0x80040840, &decision));
  if (decision) {
    STEP(0x80040880);
    known(&AT, 0x80020000u);
    STEP(0x80040884);
    TRY(store(run, 2, 1, 0x1ed6, 1, 1, 0x80040884));
    STEP(0x80040888);
    known(&AT, 0x80020000u);
    STEP(0x8004088c);
    TRY(store(run, 2, 1, 0x1ed5, 1, 1, 0x8004088c));
    STEP(0x80040890);
    known(&S1, 0);
  } else {
    STEP(0x80040848);
    known(&A0, 0x80020000u);
    STEP(0x8004084c);
    TRY(load(run, 4, 4, 0x1d74, 4, 4, 0x8004084c));
    STEP(0x80040850);
    known(&RA, 0x80040858u);
    STEP(0x80040854);
    known(&S1, 0);
    TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040850, 0x80040850,
               0x80040854, 0x8005851c, 1));
    STEP(0x80040858);
    known(&A0, 0x80020000u);
    STEP(0x8004085c);
    TRY(load(run, 4, 4, 0x1d78, 4, 4, 0x8004085c));
    STEP(0x80040860);
    known(&AT, 0x80020000u);
    STEP(0x80040864);
    TRY(store(run, 2, 1, 0x1ed5, 1, 1, 0x80040864));
    STEP(0x80040868);
    known(&RA, 0x80040870u);
    STEP(0x8004086c);
    TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040868, 0x80040868,
               0x8004086c, 0x8005851c, 1));
    STEP(0x80040870);
    known(&AT, 0x80020000u);
    STEP(0x80040874);
    TRY(store(run, 2, 1, 0x1ed6, 1, 1, 0x80040874));
    STEP(0x80040878);
    STEP(0x8004087c);
  }
  STEP(0x80040894);
  known(&S3, 0x80020000u);
  STEP(0x80040898);
  S3 = add(S3, immediate(0x1d74));
  STEP(0x8004089c);
  known(&S2, 0x80020000u);
  STEP(0x800408a0);
  S2 = add(S2, immediate(0x3ab0));
  STEP(0x800408a4);
  known(&S0, 0x80020000u);
  STEP(0x800408a8);
  S0 = add(S0, immediate(0x208c));

roster_loop:
  ++out->roster_iterations;
  STEP(0x800408ac);
  TRY(load(run, 3, 19, 0, 4, 4, 0x800408ac));
  STEP(0x800408b0);
  STEP(0x800408b4);
  V0 = signed_less(V1, immediate(0x1d));
  predicate = V0;
  STEP(0x800408b8);
  STEP(0x800408bc);
  V0 = shift_left(V1, 1);
  TRY(zero_decision(run, predicate, 0x800408b8, &decision));
  if (!decision) {
    STEP(0x800408c0);
    V0 = add(V0, V1);
    STEP(0x800408c4);
    V0 = shift_left(V0, 2);
    STEP(0x800408c8);
    V0 = add(V0, V1);
    STEP(0x800408cc);
    V0 = shift_left(V0, 3);
    STEP(0x800408d0);
    A2 = shift_left(S1, 16);
    STEP(0x800408d4);
    A2 = shift_right_arithmetic(A2, 16);
    STEP(0x800408d8);
    V1 = shift_left(A2, 2);
    STEP(0x800408dc);
    V0 = add(V0, S2);
    STEP(0x800408e0);
    V1 = add(V1, V0);
    STEP(0x800408e4);
    TRY(load(run, 4, 3, 0, 4, 4, 0x800408e4));
    STEP(0x800408e8);
    A1 = shift_left(A2, 3);
    STEP(0x800408ec);
    A1 = subtract(A1, A2);
    STEP(0x800408f0);
    A1 = shift_left(A1, 3);
    STEP(0x800408f4);
    A1 = subtract(A1, A2);
    STEP(0x800408f8);
    A1 = shift_left(A1, 1);
    STEP(0x800408fc);
    A1 = add(A1, S0);
    STEP(0x80040900);
    known(&RA, 0x80040908u);
    STEP(0x80040904);
    known(&A2, 0x6e);
    TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040900, 0x80040900,
               0x80040904, 0x800909a8, 3));
  }
  STEP(0x80040908);
  TRY(load(run, 3, 19, 4, 4, 4, 0x80040908));
  STEP(0x8004090c);
  STEP(0x80040910);
  V0 = signed_less(V1, immediate(0x1d));
  predicate = V0;
  STEP(0x80040914);
  STEP(0x80040918);
  V0 = add(S1, immediate(1));
  TRY(zero_decision(run, predicate, 0x80040914, &decision));
  if (!decision) {
    STEP(0x8004091c);
    V0 = shift_left(V1, 1);
    STEP(0x80040920);
    V0 = add(V0, V1);
    STEP(0x80040924);
    V0 = shift_left(V0, 2);
    STEP(0x80040928);
    V0 = add(V0, V1);
    STEP(0x8004092c);
    V0 = shift_left(V0, 3);
    STEP(0x80040930);
    A2 = shift_left(S1, 16);
    STEP(0x80040934);
    A2 = shift_right_arithmetic(A2, 16);
    STEP(0x80040938);
    V1 = shift_left(A2, 2);
    STEP(0x8004093c);
    V0 = add(V0, S2);
    STEP(0x80040940);
    V1 = add(V1, V0);
    STEP(0x80040944);
    TRY(load(run, 4, 3, 0, 4, 4, 0x80040944));
    STEP(0x80040948);
    A2 = add(A2, immediate(0x0c));
    STEP(0x8004094c);
    A1 = shift_left(A2, 3);
    STEP(0x80040950);
    A1 = subtract(A1, A2);
    STEP(0x80040954);
    A1 = shift_left(A1, 3);
    STEP(0x80040958);
    A1 = subtract(A1, A2);
    STEP(0x8004095c);
    A1 = shift_left(A1, 1);
    STEP(0x80040960);
    A1 = add(A1, S0);
    STEP(0x80040964);
    known(&RA, 0x8004096cu);
    STEP(0x80040968);
    known(&A2, 0x6e);
    TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_80040964, 0x80040964,
               0x80040968, 0x800909a8, 3));
    STEP(0x8004096c);
    V0 = add(S1, immediate(1));
  }
  STEP(0x80040970);
  S1 = V0;
  STEP(0x80040974);
  V0 = shift_left(V0, 16);
  STEP(0x80040978);
  V0 = shift_right_arithmetic(V0, 16);
  STEP(0x8004097c);
  V0 = signed_less(V0, immediate(12));
  predicate = V0;
  STEP(0x80040980);
  STEP(0x80040984);
  TRY(zero_decision(run, predicate, 0x80040980, &decision));
  if (!decision)
    goto roster_loop;

  STEP(0x80040988);
  known(&A0, 0x80020000u);
  STEP(0x8004098c);
  TRY(load(run, 4, 4, 0x1d74, 4, 4, 0x8004098c));
  STEP(0x80040990);
  STEP(0x80040994);
  V0 = signed_less(A0, immediate(0x1d));
  predicate = V0;
  STEP(0x80040998);
  STEP(0x8004099c);
  A0 = shift_left(A0, 16);
  TRY(zero_decision(run, predicate, 0x80040998, &decision));
  if (decision) {
    STEP(0x800409a0);
    known(&A1, 0x80020000u);
    STEP(0x800409a4);
    A1 = add(A1, immediate(0x208c));
    STEP(0x800409a8);
    known(&RA, 0x800409b0u);
    STEP(0x800409ac);
    A0 = shift_right_arithmetic(A0, 16);
    TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800409A8, 0x800409a8,
               0x800409ac, 0x8004e9d8, 2));
  }
  STEP(0x800409b0);
  known(&A0, 0x80020000u);
  STEP(0x800409b4);
  TRY(load(run, 4, 4, 0x1d78, 4, 4, 0x800409b4));
  STEP(0x800409b8);
  STEP(0x800409bc);
  V0 = signed_less(A0, immediate(0x1d));
  predicate = V0;
  STEP(0x800409c0);
  STEP(0x800409c4);
  A0 = shift_left(A0, 16);
  TRY(zero_decision(run, predicate, 0x800409c0, &decision));
  if (decision) {
    STEP(0x800409c8);
    known(&A1, 0x80020000u);
    STEP(0x800409cc);
    A1 = add(A1, immediate(0x25b4));
    STEP(0x800409d0);
    known(&RA, 0x800409d8u);
    STEP(0x800409d4);
    A0 = shift_right_arithmetic(A0, 16);
    TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800409D0, 0x800409d0,
               0x800409d4, 0x8004e9d8, 2));
  }
  STEP(0x800409d8);
  known(&RA, 0x800409e0u);
  STEP(0x800409dc);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800409D8, 0x800409d8, 0x800409dc,
             0x80029dd0, 0));
  STEP(0x800409e0);
  known(&RA, 0x800409e8u);
  STEP(0x800409e4);
  TRY(invoke(run, NBA97_FRONTEND_DISPATCH_SITE_800409E0, 0x800409e0, 0x800409e4,
             0x8002fc30, 0));

  /* 0x800409E8..0x80040A18: restore through callback-live sp, execute
   * the JR delay slot, and validate the resulting return target. */
  STEP(0x800409e8);
  TRY(load(run, 31, 29, 0x84, 4, 4, 0x800409e8));
  STEP(0x800409ec);
  TRY(load(run, 30, 29, 0x80, 4, 4, 0x800409ec));
  STEP(0x800409f0);
  TRY(load(run, 23, 29, 0x7c, 4, 4, 0x800409f0));
  STEP(0x800409f4);
  TRY(load(run, 22, 29, 0x78, 4, 4, 0x800409f4));
  STEP(0x800409f8);
  TRY(load(run, 21, 29, 0x74, 4, 4, 0x800409f8));
  STEP(0x800409fc);
  TRY(load(run, 20, 29, 0x70, 4, 4, 0x800409fc));
  STEP(0x80040a00);
  TRY(load(run, 19, 29, 0x6c, 4, 4, 0x80040a00));
  STEP(0x80040a04);
  TRY(load(run, 18, 29, 0x68, 4, 4, 0x80040a04));
  STEP(0x80040a08);
  TRY(load(run, 17, 29, 0x64, 4, 4, 0x80040a08));
  STEP(0x80040a0c);
  TRY(load(run, 16, 29, 0x60, 4, 4, 0x80040a0c));
  STEP(0x80040a10);
  SP = add(SP, immediate(0x88));
  STEP(0x80040a14);
  STEP(0x80040a18);
  if (RA.known_mask != 15) {
    stop(run, 0x80040a14, RA.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, 0x80040a14, RA.word, 0);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static Word unsigned_less(Word left, Word right) {
  Word result;
  uint64_t lmin = 0;
  uint64_t lmax = 0;
  uint64_t rmin = 0;
  uint64_t rmax = 0;
  unsigned byte;
  known(&result, left.word < right.word);
  for (byte = 0; byte < 4; ++byte) {
    uint32_t lpart = (left.word >> (byte * 8u)) & 255u;
    uint32_t rpart = (right.word >> (byte * 8u)) & 255u;
    lmin |= (uint64_t)((left.known_mask & (1u << byte)) ? lpart : 0u)
            << (byte * 8u);
    lmax |= (uint64_t)((left.known_mask & (1u << byte)) ? lpart : 255u)
            << (byte * 8u);
    rmin |= (uint64_t)((right.known_mask & (1u << byte)) ? rpart : 0u)
            << (byte * 8u);
    rmax |= (uint64_t)((right.known_mask & (1u << byte)) ? rpart : 255u)
            << (byte * 8u);
  }
  if (lmax < rmin || lmin >= rmax)
    return result;
  result.known_mask = 14;
  return result;
}
