#include "game_text_submission.h"

#include <limits.h>
#include <string.h>

typedef Nba97GameTextSubmissionWord Word;

typedef struct Run {
  Nba97GameTextSubmissionContext *context;
  Nba97GameTextSubmissionProgress *out;
  Nba97GameTextSubmissionMachine machine;
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
#define S0 R(16)
#define S1 R(17)
#define S2 R(18)
#define S3 R(19)
#define S4 R(20)
#define S5 R(21)
#define S6 R(22)
#define S7 R(23)
#define S8 R(30)
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
  run->out->stopped_entry = entry;
  publish(run);
}

static int machine_valid(const Nba97GameTextSubmissionMachine *machine) {
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

static int initialize(Nba97GameTextSubmissionContext *context,
                      Nba97GameTextSubmissionProgress *out, Run *run) {
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
    Nba97GameTextSubmissionAccess *event = &run->context->access_journal[index];
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
  journal(run, NBA97_GAME_TEXT_SUBMISSION_READ, pc, address, width, loaded);
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
  journal(run, NBA97_GAME_TEXT_SUBMISSION_STORE, pc, address, width, value);
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
static int negative_decision(Run *run, Word value, uint32_t pc, int *negative);

static int invoke(Run *run, uint8_t kind, uint32_t pc, uint32_t delay_pc,
                  uint32_t entry, uint8_t argument_count) {
  Nba97GameTextSubmissionEvent event;
  int accepted;
  stop(run, pc, 0, entry);
  TRY(spend(run));
  ++run->out->call_attempts[kind];
  memset(&event, 0, sizeof event);
  event.pc = pc;
  event.delay_slot_pc = delay_pc;
  event.entry = entry;
  event.operation = run->out->operations;
  event.invocation = run->out->call_attempts[kind];
  event.kind = kind;
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
  ++run->out->call_count[kind];
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_text_submission(Nba97GameTextSubmissionContext *context,
                               Nba97GameTextSubmissionProgress *out) {
  Run state;
  Run *run = &state;
  int decision;
  Word predicate;
  Word comparison;

  TRY(initialize(context, out, run));

  /* 0x80030D18..0x80030D50: allocate the live frame, read the original fifth
   * argument before later spills, and save every callee-saved word in order. */
  STEP(0x80030d18);
  SP = add(SP, immediate(UINT32_C(0xffffff80)));
  out->frame_stack_pointer = SP.word;
  STEP(0x80030d1c);
  TRY(store(run, 20, 29, 0x68, 4, 4, 0x80030d1c));
  STEP(0x80030d20);
  known(&S4, 0x800b0000);
  STEP(0x80030d24);
  TRY(load(run, 20, 20, 0x2048, 4, 4, 0x80030d24));
  STEP(0x80030d28);
  TRY(load(run, 8, 29, 0x90, 1, 1, 0x80030d28));
  STEP(0x80030d2c);
  TRY(store(run, 31, 29, 0x7c, 4, 4, 0x80030d2c));
  STEP(0x80030d30);
  TRY(store(run, 30, 29, 0x78, 4, 4, 0x80030d30));
  STEP(0x80030d34);
  TRY(store(run, 23, 29, 0x74, 4, 4, 0x80030d34));
  STEP(0x80030d38);
  TRY(store(run, 22, 29, 0x70, 4, 4, 0x80030d38));
  STEP(0x80030d3c);
  TRY(store(run, 21, 29, 0x6c, 4, 4, 0x80030d3c));
  STEP(0x80030d40);
  TRY(store(run, 19, 29, 0x64, 4, 4, 0x80030d40));
  STEP(0x80030d44);
  TRY(store(run, 18, 29, 0x60, 4, 4, 0x80030d44));
  STEP(0x80030d48);
  TRY(store(run, 17, 29, 0x5c, 4, 4, 0x80030d48));
  STEP(0x80030d4c);
  TRY(store(run, 16, 29, 0x58, 4, 4, 0x80030d4c));
  STEP(0x80030d50);
  TRY(store(run, 5, 29, 0x18, 4, 4, 0x80030d50));

  /* 0x80030D54..0x80030E04: scan from the retained cursor, then wrap and scan
   * from zero. An exhausted pool can reuse the original nonfree record.
   */
  STEP(0x80030d54);
  TRY(load(run, 2, 20, 0x40, 2, 2, 0x80030d54));
  V0 = sign_extend_half(V0);
  STEP(0x80030d58);
  S5 = A2;
  STEP(0x80030d5c);
  TRY(store(run, 7, 29, 0x20, 2, 2, 0x80030d5c));
  STEP(0x80030d60);
  TRY(load(run, 5, 20, 0x10, 4, 4, 0x80030d60));
  STEP(0x80030d64);
  TRY(load(run, 6, 20, 0x22, 2, 2, 0x80030d64));
  A2 = sign_extend_half(A2);
  STEP(0x80030d68);
  S2 = A0;
  STEP(0x80030d6c);
  TRY(store(run, 8, 29, 0x28, 1, 1, 0x80030d6c));
  STEP(0x80030d70);
  S1 = V0;
  STEP(0x80030d74);
  V1 = shift_left(V0, 6);
  STEP(0x80030d78);
  V0 = signed_less(V0, A2);
  predicate = V0;
  STEP(0x80030d7c);
  STEP(0x80030d80);
  S3 = add(A1, V1);
  TRY(zero_decision(run, predicate, 0x80030d7c, &decision));
  if (decision)
    goto allocation_wrap;

  STEP(0x80030d84);
  V1 = A2;
allocation_scan:
  ++out->allocation_iterations;
  STEP(0x80030d88);
  TRY(load(run, 2, 19, 0x12, 2, 2, 0x80030d88));
  V0 = sign_extend_half(V0);
  STEP(0x80030d8c);
  predicate = V0;
  STEP(0x80030d90);
  STEP(0x80030d94);
  V0 = add(S1, immediate(1));
  TRY(negative_decision(run, predicate, 0x80030d90, &decision));
  if (decision)
    goto allocation_candidate;
  STEP(0x80030d98);
  S1 = V0;
  STEP(0x80030d9c);
  V0 = shift_left(V0, 16);
  STEP(0x80030da0);
  V0 = shift_right_arithmetic(V0, 16);
  STEP(0x80030da4);
  V0 = signed_less(V0, V1);
  predicate = V0;
  STEP(0x80030da8);
  STEP(0x80030dac);
  S3 = add(S3, immediate(0x40));
  TRY(zero_decision(run, predicate, 0x80030da8, &decision));
  if (!decision)
    goto allocation_scan;
allocation_candidate:
  STEP(0x80030db0);
  TRY(load(run, 3, 20, 0x22, 2, 2, 0x80030db0));
  V1 = sign_extend_half(V1);
  STEP(0x80030db4);
  V0 = shift_left(S1, 16);
  STEP(0x80030db8);
  V0 = shift_right_arithmetic(V0, 16);
  STEP(0x80030dbc);
  V0 = signed_less(V0, V1);
  predicate = V0;
  STEP(0x80030dc0);
  STEP(0x80030dc4);
  TRY(zero_decision(run, predicate, 0x80030dc0, &decision));
  if (!decision)
    goto allocation_ready;

allocation_wrap:
  STEP(0x80030dc8);
  TRY(load(run, 2, 20, 0x40, 2, 2, 0x80030dc8));
  V0 = sign_extend_half(V0);
  predicate = V0;
  STEP(0x80030dcc);
  TRY(load(run, 19, 20, 0x10, 4, 4, 0x80030dcc));
  STEP(0x80030dd0);
  STEP(0x80030dd4);
  known(&S1, 0);
  comparison = signed_less(immediate(0), predicate);
  TRY(zero_decision(run, comparison, 0x80030dd0, &decision));
  if (decision)
    goto allocation_ready;
  STEP(0x80030dd8);
  V1 = predicate;
allocation_wrap_scan:
  ++out->allocation_iterations;
  STEP(0x80030ddc);
  TRY(load(run, 2, 19, 0x12, 2, 2, 0x80030ddc));
  V0 = sign_extend_half(V0);
  STEP(0x80030de0);
  predicate = V0;
  STEP(0x80030de4);
  STEP(0x80030de8);
  V0 = add(S1, immediate(1));
  TRY(negative_decision(run, predicate, 0x80030de4, &decision));
  if (decision)
    goto allocation_ready;
  STEP(0x80030dec);
  S1 = V0;
  STEP(0x80030df0);
  V0 = shift_left(V0, 16);
  STEP(0x80030df4);
  V0 = shift_right_arithmetic(V0, 16);
  STEP(0x80030df8);
  V0 = signed_less(V0, V1);
  predicate = V0;
  STEP(0x80030dfc);
  STEP(0x80030e00);
  S3 = add(S3, immediate(0x40));
  TRY(zero_decision(run, predicate, 0x80030dfc, &decision));
  if (!decision)
    goto allocation_wrap_scan;

allocation_ready:
  /* 0x80030E04..0x80030E38: measure and allocate the record. A null allocator
   * result is stored into record+8 before the zero return path. */
  STEP(0x80030e04);
  TRY(load(run, 4, 29, 0x18, 4, 4, 0x80030e04));
  STEP(0x80030e08);
  A1 = add(SP, immediate(0x10));
  STEP(0x80030e0c);
  A2 = add(S3, immediate(0x0c));
  STEP(0x80030e10);
  A3 = add(SP, immediate(0x12));
  STEP(0x80030e14);
  known(&RA, 0x80030e1c);
  STEP(0x80030e18);
  TRY(store(run, 17, 20, 0x40, 2, 2, 0x80030e18));
  TRY(invoke(run, NBA97_GAME_TEXT_SUBMISSION_CHILD_8002EB50, 0x80030e14,
             0x80030e18, 0x8002eb50, 4));
  STEP(0x80030e1c);
  TRY(load(run, 4, 19, 0x0c, 2, 2, 0x80030e1c));
  A0 = sign_extend_half(A0);
  STEP(0x80030e20);
  known(&RA, 0x80030e28);
  STEP(0x80030e24);
  TRY(invoke(run, NBA97_GAME_TEXT_SUBMISSION_CHILD_8002EF88, 0x80030e20,
             0x80030e24, 0x8002ef88, 1));
  STEP(0x80030e28);
  S0 = V0;
  predicate = S0;
  STEP(0x80030e2c);
  STEP(0x80030e30);
  TRY(store(run, 16, 19, 8, 4, 4, 0x80030e30));
  TRY(zero_decision(run, predicate, 0x80030e2c, &decision));
  if (decision) {
    STEP(0x80030e34);
    STEP(0x80030e38);
    known(&V0, 0);
    goto epilogue;
  }

  /* 0x80030E3C..0x80030F20: initialize the record and publish both bounded
   * signed slot caches exactly where the source does so. */
  STEP(0x80030e3c);
  TRY(store(run, 21, 19, 0x0e, 2, 2, 0x80030e3c));
  STEP(0x80030e40);
  TRY(load(run, 8, 29, 0x20, 2, 2, 0x80030e40));
  STEP(0x80030e44);
  STEP(0x80030e48);
  TRY(store(run, 8, 19, 0x10, 2, 2, 0x80030e48));
  STEP(0x80030e4c);
  TRY(load(run, 2, 20, 0x28, 2, 2, 0x80030e4c));
  STEP(0x80030e50);
  TRY(store(run, 0, 19, 0x3b, 1, 1, 0x80030e50));
  STEP(0x80030e54);
  TRY(store(run, 0, 19, 0x2b, 1, 1, 0x80030e54));
  STEP(0x80030e58);
  TRY(store(run, 0, 19, 0x20, 2, 2, 0x80030e58));
  STEP(0x80030e5c);
  TRY(store(run, 0, 19, 0x1e, 2, 2, 0x80030e5c));
  STEP(0x80030e60);
  TRY(store(run, 2, 19, 0x12, 2, 2, 0x80030e60));
  STEP(0x80030e64);
  V0 = shift_left(S2, 16);
  STEP(0x80030e68);
  V1 = shift_right_arithmetic(V0, 16);
  predicate = V1;
  STEP(0x80030e6c);
  STEP(0x80030e70);
  V0 = signed_less(V1, immediate(100));
  TRY(negative_decision(run, predicate, 0x80030e6c, &decision));
  if (decision)
    goto first_cache_done;
  predicate = V0;
  STEP(0x80030e74);
  STEP(0x80030e78);
  V0 = signed_less(V1, immediate(200));
  TRY(zero_decision(run, predicate, 0x80030e74, &decision));
  if (!decision) {
    STEP(0x80030e7c);
    known(&V0, 0x800b0000);
    STEP(0x80030e80);
    TRY(load(run, 2, 2, 0x2048, 4, 4, 0x80030e80));
    STEP(0x80030e84);
    STEP(0x80030e88);
    TRY(store(run, 18, 2, 0x2c, 2, 2, 0x80030e88));
    goto first_cache_done;
  }
  predicate = V0;
  STEP(0x80030e8c);
  STEP(0x80030e90);
  V0 = shift_left(S2, 16);
  TRY(zero_decision(run, predicate, 0x80030e8c, &decision));
  if (decision)
    goto slot_linkage;
  STEP(0x80030e94);
  known(&V0, 0x800b0000);
  STEP(0x80030e98);
  TRY(load(run, 2, 2, 0x2048, 4, 4, 0x80030e98));
  STEP(0x80030e9c);
  STEP(0x80030ea0);
  TRY(store(run, 18, 2, 0x2e, 2, 2, 0x80030ea0));
first_cache_done:
  STEP(0x80030ea4);
  V0 = shift_left(S2, 16);
slot_linkage:
  STEP(0x80030ea8);
  A0 = shift_right_arithmetic(V0, 16);
  predicate = A0;
  STEP(0x80030eac);
  STEP(0x80030eb0);
  TRY(store(run, 18, 19, 0x14, 2, 2, 0x80030eb0));
  TRY(negative_decision(run, predicate, 0x80030eac, &decision));
  if (decision)
    goto list_mode;
  STEP(0x80030eb4);
  known(&V0, UINT32_MAX);
  STEP(0x80030eb8);
  TRY(store(run, 2, 19, 0x16, 2, 2, 0x80030eb8));
  STEP(0x80030ebc);
  TRY(load(run, 2, 20, 0x14, 4, 4, 0x80030ebc));
  STEP(0x80030ec0);
  A1 = shift_left(A0, 1);
  STEP(0x80030ec4);
  V0 = add(A1, V0);
  STEP(0x80030ec8);
  TRY(load(run, 2, 2, 0, 2, 2, 0x80030ec8));
  STEP(0x80030ecc);
  STEP(0x80030ed0);
  TRY(store(run, 2, 19, 0x18, 2, 2, 0x80030ed0));
  STEP(0x80030ed4);
  V0 = shift_left(V0, 16);
  predicate = V0;
  STEP(0x80030ed8);
  V0 = shift_right_arithmetic(V0, 16);
  predicate = V0;
  STEP(0x80030edc);
  STEP(0x80030ee0);
  V0 = shift_left(V0, 6);
  TRY(negative_decision(run, predicate, 0x80030edc, &decision));
  if (!decision) {
    STEP(0x80030ee4);
    TRY(load(run, 3, 20, 0x10, 4, 4, 0x80030ee4));
    STEP(0x80030ee8);
    STEP(0x80030eec);
    V0 = add(V0, V1);
    STEP(0x80030ef0);
    TRY(store(run, 17, 2, 0x16, 2, 2, 0x80030ef0));
  }
  STEP(0x80030ef4);
  TRY(load(run, 2, 20, 0x14, 4, 4, 0x80030ef4));
  STEP(0x80030ef8);
  STEP(0x80030efc);
  V0 = add(A1, V0);
  STEP(0x80030f00);
  TRY(store(run, 17, 2, 0, 2, 2, 0x80030f00));
  STEP(0x80030f04);
  V0 = signed_less(A0, immediate(100));
  predicate = V0;
  STEP(0x80030f08);
  STEP(0x80030f0c);
  V0 = signed_less(A0, immediate(200));
  TRY(zero_decision(run, predicate, 0x80030f08, &decision));
  if (!decision) {
    STEP(0x80030f10);
    STEP(0x80030f14);
    TRY(store(run, 18, 20, 0x2c, 2, 2, 0x80030f14));
    goto list_mode;
  }
  predicate = V0;
  STEP(0x80030f18);
  STEP(0x80030f1c);
  TRY(zero_decision(run, predicate, 0x80030f18, &decision));
  if (!decision) {
    STEP(0x80030f20);
    TRY(store(run, 18, 20, 0x2e, 2, 2, 0x80030f20));
  }

list_mode:
  /* 0x80030F24..0x80031070: append the record to one of four retained lists;
   * mode values outside 1..3 use the source default list. */
  STEP(0x80030f24);
  TRY(load(run, 3, 20, 0x2a, 2, 2, 0x80030f24));
  V1 = sign_extend_half(V1);
  STEP(0x80030f28);
  known(&V0, 2);
  predicate = V1;
  STEP(0x80030f2c);
  STEP(0x80030f30);
  V0 = signed_less(V1, immediate(3));
  TRY(equal_decision(run, predicate, immediate(2), 0x80030f2c, &decision));
  if (decision)
    goto mode_two;
  predicate = V0;
  STEP(0x80030f34);
  STEP(0x80030f38);
  known(&V0, 1);
  TRY(zero_decision(run, predicate, 0x80030f34, &decision));
  if (decision)
    goto mode_at_least_three;
  predicate = V1;
  STEP(0x80030f3c);
  STEP(0x80030f40);
  TRY(equal_decision(run, predicate, V0, 0x80030f3c, &decision));
  if (decision)
    goto mode_one;
  STEP(0x80030f44);
  STEP(0x80030f48);
  goto mode_default;

mode_at_least_three:
  STEP(0x80030f4c);
  known(&V0, 3);
  predicate = V1;
  STEP(0x80030f50);
  STEP(0x80030f54);
  TRY(equal_decision(run, predicate, V0, 0x80030f50, &decision));
  if (!decision)
    goto mode_default;
  STEP(0x80030f58);
  TRY(load(run, 2, 20, 0x3c, 2, 2, 0x80030f58));
  V0 = sign_extend_half(V0);
  predicate = V0;
  STEP(0x80030f5c);
  STEP(0x80030f60);
  STEP(0x80030f64);
  known(&V1, UINT32_MAX);
  TRY(negative_decision(run, predicate, 0x80030f60, &decision));
  if (!decision)
    goto mode_three_nonempty;
  STEP(0x80030f68);
  known(&V0, UINT32_MAX);
  STEP(0x80030f6c);
  TRY(store(run, 17, 20, 0x3e, 2, 2, 0x80030f6c));
  STEP(0x80030f70);
  STEP(0x80030f74);
  TRY(store(run, 17, 20, 0x3c, 2, 2, 0x80030f74));
  goto list_empty_record;
mode_three_nonempty:
  STEP(0x80030f78);
  TRY(load(run, 2, 20, 0x3e, 2, 2, 0x80030f78));
  STEP(0x80030f7c);
  TRY(store(run, 3, 19, 0x1c, 2, 2, 0x80030f7c));
  STEP(0x80030f80);
  TRY(store(run, 2, 19, 0x1a, 2, 2, 0x80030f80));
  STEP(0x80030f84);
  TRY(load(run, 3, 20, 0x10, 4, 4, 0x80030f84));
  STEP(0x80030f88);
  TRY(store(run, 2, 29, 0x10, 2, 2, 0x80030f88));
  STEP(0x80030f8c);
  V0 = shift_left(V0, 16);
  STEP(0x80030f90);
  V0 = shift_right_arithmetic(V0, 10);
  STEP(0x80030f94);
  STEP(0x80030f98);
  TRY(store(run, 17, 20, 0x3e, 2, 2, 0x80030f98));
  goto append_previous;

mode_two:
  STEP(0x80030f9c);
  TRY(load(run, 2, 20, 0x38, 2, 2, 0x80030f9c));
  V0 = sign_extend_half(V0);
  predicate = V0;
  STEP(0x80030fa0);
  STEP(0x80030fa4);
  STEP(0x80030fa8);
  known(&V1, UINT32_MAX);
  TRY(negative_decision(run, predicate, 0x80030fa4, &decision));
  if (!decision)
    goto mode_two_nonempty;
  STEP(0x80030fac);
  known(&V0, UINT32_MAX);
  STEP(0x80030fb0);
  TRY(store(run, 17, 20, 0x3a, 2, 2, 0x80030fb0));
  STEP(0x80030fb4);
  STEP(0x80030fb8);
  TRY(store(run, 17, 20, 0x38, 2, 2, 0x80030fb8));
  goto list_empty_record;
mode_two_nonempty:
  STEP(0x80030fbc);
  TRY(load(run, 2, 20, 0x3a, 2, 2, 0x80030fbc));
  STEP(0x80030fc0);
  TRY(store(run, 3, 19, 0x1c, 2, 2, 0x80030fc0));
  STEP(0x80030fc4);
  TRY(store(run, 2, 19, 0x1a, 2, 2, 0x80030fc4));
  STEP(0x80030fc8);
  TRY(load(run, 3, 20, 0x10, 4, 4, 0x80030fc8));
  STEP(0x80030fcc);
  TRY(store(run, 2, 29, 0x10, 2, 2, 0x80030fcc));
  STEP(0x80030fd0);
  V0 = shift_left(V0, 16);
  STEP(0x80030fd4);
  V0 = shift_right_arithmetic(V0, 10);
  STEP(0x80030fd8);
  STEP(0x80030fdc);
  TRY(store(run, 17, 20, 0x3a, 2, 2, 0x80030fdc));
  goto append_previous;

mode_one:
  STEP(0x80030fe0);
  TRY(load(run, 2, 20, 0x34, 2, 2, 0x80030fe0));
  V0 = sign_extend_half(V0);
  predicate = V0;
  STEP(0x80030fe4);
  STEP(0x80030fe8);
  STEP(0x80030fec);
  known(&V1, UINT32_MAX);
  TRY(negative_decision(run, predicate, 0x80030fe8, &decision));
  if (!decision)
    goto mode_one_nonempty;
  STEP(0x80030ff0);
  known(&V0, UINT32_MAX);
  STEP(0x80030ff4);
  TRY(store(run, 17, 20, 0x36, 2, 2, 0x80030ff4));
  STEP(0x80030ff8);
  STEP(0x80030ffc);
  TRY(store(run, 17, 20, 0x34, 2, 2, 0x80030ffc));
  goto list_empty_record;
mode_one_nonempty:
  STEP(0x80031000);
  TRY(load(run, 2, 20, 0x36, 2, 2, 0x80031000));
  STEP(0x80031004);
  TRY(store(run, 3, 19, 0x1c, 2, 2, 0x80031004));
  STEP(0x80031008);
  TRY(store(run, 2, 19, 0x1a, 2, 2, 0x80031008));
  STEP(0x8003100c);
  TRY(load(run, 3, 20, 0x10, 4, 4, 0x8003100c));
  STEP(0x80031010);
  TRY(store(run, 2, 29, 0x10, 2, 2, 0x80031010));
  STEP(0x80031014);
  V0 = shift_left(V0, 16);
  STEP(0x80031018);
  V0 = shift_right_arithmetic(V0, 10);
  STEP(0x8003101c);
  STEP(0x80031020);
  TRY(store(run, 17, 20, 0x36, 2, 2, 0x80031020));
  goto append_previous;

mode_default:
  STEP(0x80031024);
  TRY(load(run, 2, 20, 0x30, 2, 2, 0x80031024));
  V0 = sign_extend_half(V0);
  predicate = V0;
  STEP(0x80031028);
  STEP(0x8003102c);
  STEP(0x80031030);
  known(&V1, UINT32_MAX);
  TRY(negative_decision(run, predicate, 0x8003102c, &decision));
  if (!decision)
    goto mode_default_nonempty;
  STEP(0x80031034);
  known(&V0, UINT32_MAX);
  STEP(0x80031038);
  TRY(store(run, 17, 20, 0x32, 2, 2, 0x80031038));
  STEP(0x8003103c);
  TRY(store(run, 17, 20, 0x30, 2, 2, 0x8003103c));
list_empty_record:
  STEP(0x80031040);
  TRY(store(run, 2, 19, 0x1a, 2, 2, 0x80031040));
  STEP(0x80031044);
  STEP(0x80031048);
  TRY(store(run, 2, 19, 0x1c, 2, 2, 0x80031048));
  goto prepare_text;
mode_default_nonempty:
  STEP(0x8003104c);
  TRY(load(run, 2, 20, 0x32, 2, 2, 0x8003104c));
  STEP(0x80031050);
  TRY(store(run, 3, 19, 0x1c, 2, 2, 0x80031050));
  STEP(0x80031054);
  TRY(store(run, 2, 19, 0x1a, 2, 2, 0x80031054));
  STEP(0x80031058);
  TRY(load(run, 3, 20, 0x10, 4, 4, 0x80031058));
  STEP(0x8003105c);
  TRY(store(run, 2, 29, 0x10, 2, 2, 0x8003105c));
  STEP(0x80031060);
  V0 = shift_left(V0, 16);
  STEP(0x80031064);
  V0 = shift_right_arithmetic(V0, 10);
  STEP(0x80031068);
  TRY(store(run, 17, 20, 0x32, 2, 2, 0x80031068));
append_previous:
  STEP(0x8003106c);
  V0 = add(V0, V1);
  STEP(0x80031070);
  TRY(store(run, 17, 2, 0x1c, 2, 2, 0x80031070));

prepare_text:
  /* 0x80031074..0x800310B8: derive runtime table pointers and clear the two
   * ordering-table packet heads through the typed existing service boundary. */
  STEP(0x80031074);
  TRY(load(run, 2, 20, 0x26, 2, 2, 0x80031074));
  STEP(0x80031078);
  TRY(load(run, 6, 20, 0x0c, 4, 4, 0x80031078));
  STEP(0x8003107c);
  V0 = shift_left(V0, 16);
  STEP(0x80031080);
  V1 = shift_right_arithmetic(V0, 15);
  STEP(0x80031084);
  V0 = shift_right_arithmetic(V0, 24);
  STEP(0x80031088);
  V0 = add(S4, V0);
  STEP(0x8003108c);
  TRY(load(run, 8, 2, 0x42, 1, 1, 0x8003108c));
  STEP(0x80031090);
  known(&S7, 0x80800000);
  STEP(0x80031094);
  TRY(store(run, 8, 29, 0x30, 1, 1, 0x80031094));
  STEP(0x80031098);
  TRY(load(run, 2, 2, 0x4a, 1, 1, 0x80031098));
  STEP(0x8003109c);
  A0 = S3;
  STEP(0x800310a0);
  known(&A1, 1);
  STEP(0x800310a4);
  S8 = add(A2, V1);
  STEP(0x800310a8);
  known(&RA, 0x800310b0);
  STEP(0x800310ac);
  TRY(store(run, 2, 29, 0x38, 2, 2, 0x800310ac));
  TRY(invoke(run, NBA97_GAME_TEXT_SUBMISSION_CHILD_80099960, 0x800310a8,
             0x800310ac, 0x80099960, 2));
  STEP(0x800310b0);
  A0 = add(S3, immediate(4));
  STEP(0x800310b4);
  known(&RA, 0x800310bc);
  STEP(0x800310b8);
  known(&A1, 1);
  TRY(invoke(run, NBA97_GAME_TEXT_SUBMISSION_CHILD_80099960, 0x800310b4,
             0x800310b8, 0x80099960, 2));

  /* 0x800310BC..0x80031144: establish color, mode-dependent horizontal
   * origin, and the first live string pointer. */
  STEP(0x800310bc);
  TRY(load(run, 3, 29, 0x28, 1, 1, 0x800310bc));
  STEP(0x800310c0);
  S7 = or_constant(S7, 0x8000);
  STEP(0x800310c4);
  known(&V0, 2);
  predicate = V1;
  STEP(0x800310c8);
  STEP(0x800310cc);
  S6 = S5;
  TRY(equal_decision(run, predicate, V0, 0x800310c8, &decision));
  if (decision)
    goto position_mode_two;
  STEP(0x800310d0);
  V0 = signed_less(V1, immediate(3));
  predicate = V0;
  STEP(0x800310d4);
  STEP(0x800310d8);
  known(&V0, 1);
  TRY(zero_decision(run, predicate, 0x800310d4, &decision));
  if (decision)
    goto position_mode_high;
  predicate = V1;
  STEP(0x800310dc);
  STEP(0x800310e0);
  TRY(equal_decision(run, predicate, V0, 0x800310dc, &decision));
  if (decision)
    goto position_mode_one;
  STEP(0x800310e4);
  STEP(0x800310e8);
  goto string_start;
position_mode_high:
  STEP(0x800310ec);
  known(&V0, 3);
  predicate = V1;
  STEP(0x800310f0);
  STEP(0x800310f4);
  known(&V0, 4);
  TRY(equal_decision(run, predicate, immediate(3), 0x800310f0, &decision));
  if (decision)
    goto position_delimiter_dot;
  predicate = V1;
  STEP(0x800310f8);
  STEP(0x800310fc);
  known(&A1, 0x2f);
  TRY(equal_decision(run, predicate, V0, 0x800310f8, &decision));
  if (decision)
    goto position_delimiter_call;
  STEP(0x80031100);
  STEP(0x80031104);
  goto string_start;
position_mode_one:
  STEP(0x80031108);
  TRY(load(run, 2, 29, 0x12, 2, 2, 0x80031108));
  STEP(0x8003110c);
  STEP(0x80031110);
  V0 = shift_left(V0, 16);
  STEP(0x80031114);
  STEP(0x80031118);
  V0 = shift_right_arithmetic(V0, 17);
  goto position_subtract;
position_mode_two:
  STEP(0x8003111c);
  TRY(load(run, 2, 29, 0x12, 2, 2, 0x8003111c));
  STEP(0x80031120);
  STEP(0x80031124);
  S5 = subtract(S5, V0);
  goto string_start;
position_delimiter_dot:
  STEP(0x80031128);
  TRY(load(run, 4, 29, 0x18, 4, 4, 0x80031128));
  STEP(0x8003112c);
  STEP(0x80031130);
  known(&A1, 0x2e);
  goto position_call;
position_delimiter_call:
  STEP(0x80031134);
  TRY(load(run, 4, 29, 0x18, 4, 4, 0x80031134));
position_call:
  STEP(0x80031138);
  known(&RA, 0x80031140);
  STEP(0x8003113c);
  TRY(invoke(run, NBA97_GAME_TEXT_SUBMISSION_CHILD_8002ECD4, 0x80031138,
             0x8003113c, 0x8002ecd4, 2));
position_subtract:
  STEP(0x80031140);
  S5 = subtract(S5, V0);

string_start:
  STEP(0x80031144);
  TRY(load(run, 18, 29, 0x18, 4, 4, 0x80031144));
  STEP(0x80031148);
  STEP(0x8003114c);
  TRY(load(run, 2, 18, 0, 1, 1, 0x8003114c));
  predicate = V0;
  STEP(0x80031150);
  STEP(0x80031154);
  STEP(0x80031158);
  V0 = shift_left(V0, 1);
  TRY(zero_decision(run, predicate, 0x80031154, &decision));
  if (decision)
    goto link_packets;

  /* 0x8003115C..0x80031320: map the current byte or execute its control
   * behavior. Newline dispatch uses the runtime five-entry JR table. */
  STEP(0x8003115c);
  TRY(load(run, 8, 29, 0x28, 1, 1, 0x8003115c));
  STEP(0x80031160);
  STEP(0x80031164);
  TRY(store(run, 8, 29, 0x48, 4, 4, 0x80031164));
  STEP(0x80031168);
  T0 = shift_left(T0, 2);
  STEP(0x8003116c);
  TRY(store(run, 8, 29, 0x50, 4, 4, 0x8003116c));
character_loop:
  ++out->glyph_iterations;
  STEP(0x80031170);
  V0 = add(V0, S8);
  STEP(0x80031174);
  TRY(load(run, 2, 2, 0, 2, 2, 0x80031174));
  STEP(0x80031178);
  STEP(0x8003117c);
  TRY(store(run, 2, 29, 0x10, 2, 2, 0x8003117c));
  STEP(0x80031180);
  V0 = shift_left(V0, 16);
  predicate = V0;
  STEP(0x80031184);
  STEP(0x80031188);
  known(&V0, 0x20);
  TRY(negative_decision(run, predicate, 0x80031184, &decision));
  if (!decision)
    goto glyph_record;
  STEP(0x8003118c);
  TRY(load(run, 4, 18, 0, 1, 1, 0x8003118c));
  STEP(0x80031190);
  STEP(0x80031194);
  V1 = and_constant(A0, 0xff);
  predicate = V1;
  STEP(0x80031198);
  STEP(0x8003119c);
  known(&V0, 0x0a);
  TRY(equal_decision(run, predicate, immediate(0x20), 0x80031198, &decision));
  if (decision)
    goto space_control;
  predicate = V1;
  STEP(0x800311a0);
  STEP(0x800311a4);
  known(&V0, 0x1f);
  TRY(equal_decision(run, predicate, immediate(0x0a), 0x800311a0, &decision));
  if (!decision)
    goto non_newline_control;

  STEP(0x800311a8);
  A0 = add(S2, immediate(1));
  STEP(0x800311ac);
  A1 = add(SP, immediate(0x10));
  STEP(0x800311b0);
  A2 = A1;
  STEP(0x800311b4);
  known(&RA, 0x800311bc);
  STEP(0x800311b8);
  A3 = add(SP, immediate(0x12));
  TRY(invoke(run, NBA97_GAME_TEXT_SUBMISSION_CHILD_8002EB50, 0x800311b4,
             0x800311b8, 0x8002eb50, 4));
  STEP(0x800311bc);
  TRY(load(run, 8, 29, 0x48, 4, 4, 0x800311bc));
  STEP(0x800311c0);
  STEP(0x800311c4);
  V0 = unsigned_less(T0, immediate(5));
  predicate = V0;
  STEP(0x800311c8);
  STEP(0x800311cc);
  TRY(zero_decision(run, predicate, 0x800311c8, &decision));
  if (decision)
    goto newline_advance_y;
  STEP(0x800311d0);
  TRY(load(run, 8, 29, 0x50, 4, 4, 0x800311d0));
  STEP(0x800311d4);
  known(&AT, 0x80020000);
  STEP(0x800311d8);
  AT = add(AT, T0);
  STEP(0x800311dc);
  TRY(load(run, 2, 1, 0x4940, 4, 4, 0x800311dc));
  STEP(0x800311e0);
  STEP(0x800311e4);
  STEP(0x800311e8);
  if (V0.known_mask != 15) {
    stop(run, 0x800311e4, V0.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (V0.word & 3u) {
    stop(run, 0x800311e4, V0.word, 0);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  if (V0.word == 0x800311ecu)
    goto newline_mode_zero;
  if (V0.word == 0x800311f4u)
    goto newline_mode_one;
  if (V0.word == 0x80031208u)
    goto newline_mode_two;
  if (V0.word == 0x80031214u)
    goto newline_mode_three;
  if (V0.word == 0x80031220u)
    goto newline_mode_four;
  stop(run, 0x800311e4, V0.word, V0.word);
  return NBA97_TEXT_IO_REFUSED;
newline_mode_zero:
  STEP(0x800311ec);
  STEP(0x800311f0);
  S5 = S6;
  goto newline_advance_y;
newline_mode_one:
  STEP(0x800311f4);
  TRY(load(run, 2, 29, 0x12, 2, 2, 0x800311f4));
  STEP(0x800311f8);
  STEP(0x800311fc);
  V0 = shift_left(V0, 16);
  STEP(0x80031200);
  STEP(0x80031204);
  V0 = shift_right_arithmetic(V0, 17);
  goto newline_subtract;
newline_mode_two:
  STEP(0x80031208);
  TRY(load(run, 2, 29, 0x12, 2, 2, 0x80031208));
  STEP(0x8003120c);
  STEP(0x80031210);
  S5 = subtract(S6, V0);
  goto newline_advance_y;
newline_mode_three:
  STEP(0x80031214);
  A0 = add(S2, immediate(1));
  STEP(0x80031218);
  STEP(0x8003121c);
  known(&A1, 0x2e);
  goto newline_delimiter;
newline_mode_four:
  STEP(0x80031220);
  A0 = add(S2, immediate(1));
  STEP(0x80031224);
  known(&A1, 0x2f);
newline_delimiter:
  STEP(0x80031228);
  known(&RA, 0x80031230);
  STEP(0x8003122c);
  TRY(invoke(run, NBA97_GAME_TEXT_SUBMISSION_CHILD_8002ECD4, 0x80031228,
             0x8003122c, 0x8002ecd4, 2));
newline_subtract:
  STEP(0x80031230);
  S5 = subtract(S6, V0);
newline_advance_y:
  STEP(0x80031234);
  TRY(load(run, 2, 20, 0x52, 1, 1, 0x80031234));
  STEP(0x80031238);
  TRY(load(run, 8, 29, 0x20, 2, 2, 0x80031238));
  STEP(0x8003123c);
  STEP(0x80031240);
  T0 = add(T0, V0);
  goto store_vertical;

non_newline_control:
  predicate = V1;
  STEP(0x80031244);
  STEP(0x80031248);
  known(&V0, 0x1e);
  TRY(equal_decision(run, predicate, immediate(0x1f), 0x80031244, &decision));
  if (decision) {
    STEP(0x8003124c);
    S2 = add(S2, immediate(1));
    STEP(0x80031250);
    TRY(load(run, 2, 18, 0, 1, 1, 0x80031250));
    STEP(0x80031254);
    STEP(0x80031258);
    S5 = add(S5, V0);
    goto advance_character;
  }
  predicate = V1;
  STEP(0x8003125c);
  STEP(0x80031260);
  known(&V0, 0x1d);
  TRY(equal_decision(run, predicate, immediate(0x1e), 0x8003125c, &decision));
  if (decision) {
    STEP(0x80031264);
    S2 = add(S2, immediate(1));
    STEP(0x80031268);
    TRY(load(run, 2, 18, 0, 1, 1, 0x80031268));
    STEP(0x8003126c);
    STEP(0x80031270);
    V0 = and_constant(V0, 3);
    STEP(0x80031274);
    V0 = shift_left(V0, 2);
    STEP(0x80031278);
    known(&AT, 0x800b0000);
    STEP(0x8003127c);
    AT = add(AT, V0);
    STEP(0x80031280);
    TRY(load(run, 23, 1, 0x204c, 4, 4, 0x80031280));
    STEP(0x80031284);
    STEP(0x80031288);
    S2 = add(S2, immediate(1));
    goto load_next_character;
  }
  predicate = V1;
  STEP(0x8003128c);
  STEP(0x80031290);
  known(&V0, 0x1c);
  TRY(equal_decision(run, predicate, immediate(0x1d), 0x8003128c, &decision));
  if (decision) {
    STEP(0x80031294);
    S2 = add(S2, immediate(1));
    STEP(0x80031298);
    TRY(load(run, 2, 18, 0, 1, 1, 0x80031298));
    STEP(0x8003129c);
    TRY(load(run, 3, 20, 0x0c, 4, 4, 0x8003129c));
    STEP(0x800312a0);
    V0 = add(V0, immediate(UINT32_MAX));
    STEP(0x800312a4);
    V0 = shift_left(V0, 9);
    STEP(0x800312a8);
    STEP(0x800312ac);
    S8 = add(V1, V0);
    goto advance_character;
  }
  predicate = V1;
  STEP(0x800312b0);
  STEP(0x800312b4);
  V0 = add(A0, immediate(UINT32_C(0xffffff9f)));
  TRY(equal_decision(run, predicate, immediate(0x1c), 0x800312b0, &decision));
  if (decision) {
    STEP(0x800312b8);
    S2 = add(S2, immediate(1));
    STEP(0x800312bc);
    TRY(load(run, 2, 18, 0, 1, 1, 0x800312bc));
    STEP(0x800312c0);
    TRY(load(run, 8, 29, 0x20, 2, 2, 0x800312c0));
    STEP(0x800312c4);
    V0 = shift_left(V0, 24);
    STEP(0x800312c8);
    V0 = shift_right_arithmetic(V0, 24);
    STEP(0x800312cc);
    T0 = add(T0, V0);
  store_vertical:
    STEP(0x800312d0);
    STEP(0x800312d4);
    TRY(store(run, 8, 29, 0x20, 2, 2, 0x800312d4));
    goto advance_character;
  }

  STEP(0x800312d8);
  V0 = unsigned_less(V0, immediate(0x1a));
  predicate = V0;
  STEP(0x800312dc);
  STEP(0x800312e0);
  V0 = shift_left(V1, 1);
  TRY(zero_decision(run, predicate, 0x800312dc, &decision));
  if (decision)
    goto fallback_glyph;
  STEP(0x800312e4);
  V0 = add(V0, S8);
  STEP(0x800312e8);
  TRY(load(run, 2, 2, UINT32_C(0xffffffc0), 2, 2, 0x800312e8));
  STEP(0x800312ec);
  STEP(0x800312f0);
  TRY(store(run, 2, 29, 0x10, 2, 2, 0x800312f0));
  STEP(0x800312f4);
  V0 = shift_left(V0, 16);
  predicate = V0;
  STEP(0x800312f8);
  STEP(0x800312fc);
  TRY(negative_decision(run, predicate, 0x800312f8, &decision));
  if (!decision)
    goto glyph_record;
fallback_glyph:
  STEP(0x80031300);
  TRY(load(run, 2, 30, 0x7e, 2, 2, 0x80031300));
  STEP(0x80031304);
  STEP(0x80031308);
  TRY(store(run, 2, 29, 0x10, 2, 2, 0x80031308));
  STEP(0x8003130c);
  V0 = shift_left(V0, 16);
  predicate = V0;
  STEP(0x80031310);
  STEP(0x80031314);
  TRY(negative_decision(run, predicate, 0x80031310, &decision));
  if (!decision)
    goto glyph_record;
space_control:
  STEP(0x80031318);
  TRY(load(run, 8, 29, 0x30, 1, 1, 0x80031318));
  STEP(0x8003131c);
  STEP(0x80031320);
  S5 = add(T0, S5);
  goto advance_character;

glyph_record:
  /* 0x80031324..0x80031488: materialize one glyph into the paired 40-byte
   * packets, then copy the first packet through the typed memory-copy call. */
  STEP(0x80031324);
  TRY(load(run, 4, 29, 0x10, 2, 2, 0x80031324));
  A0 = sign_extend_half(A0);
  predicate = A0;
  STEP(0x80031328);
  STEP(0x8003132c);
  STEP(0x80031330);
  V0 = shift_left(A0, 2);
  TRY(negative_decision(run, predicate, 0x8003132c, &decision));
  if (decision)
    goto advance_character;
  STEP(0x80031334);
  TRY(load(run, 3, 20, 8, 4, 4, 0x80031334));
  STEP(0x80031338);
  V0 = add(V0, A0);
  STEP(0x8003133c);
  V0 = shift_left(V0, 2);
  STEP(0x80031340);
  S1 = add(V1, V0);
  STEP(0x80031344);
  TRY(load(run, 2, 17, 0, 4, 4, 0x80031344));
  STEP(0x80031348);
  STEP(0x8003134c);
  TRY(store(run, 2, 16, 0, 4, 4, 0x8003134c));
  STEP(0x80031350);
  TRY(load(run, 2, 17, 4, 2, 2, 0x80031350));
  STEP(0x80031354);
  STEP(0x80031358);
  TRY(store(run, 2, 16, 0x0e, 2, 2, 0x80031358));
  STEP(0x8003135c);
  TRY(load(run, 2, 17, 6, 2, 2, 0x8003135c));
  STEP(0x80031360);
  STEP(0x80031364);
  TRY(store(run, 2, 16, 0x16, 2, 2, 0x80031364));
  STEP(0x80031368);
  TRY(load(run, 2, 17, 0x0b, 1, 1, 0x80031368));
  STEP(0x8003136c);
  STEP(0x80031370);
  TRY(store(run, 2, 16, 7, 1, 1, 0x80031370));
  STEP(0x80031374);
  TRY(load(run, 2, 17, 0x0c, 1, 1, 0x80031374));
  STEP(0x80031378);
  STEP(0x8003137c);
  TRY(store(run, 2, 16, 0x0c, 1, 1, 0x8003137c));
  STEP(0x80031380);
  TRY(load(run, 2, 17, 0x0d, 1, 1, 0x80031380));
  STEP(0x80031384);
  STEP(0x80031388);
  TRY(store(run, 2, 16, 0x14, 1, 1, 0x80031388));
  STEP(0x8003138c);
  TRY(load(run, 2, 17, 0x0e, 1, 1, 0x8003138c));
  STEP(0x80031390);
  STEP(0x80031394);
  TRY(store(run, 2, 16, 0x1c, 1, 1, 0x80031394));
  STEP(0x80031398);
  TRY(load(run, 2, 17, 0x0f, 1, 1, 0x80031398));
  STEP(0x8003139c);
  STEP(0x800313a0);
  TRY(store(run, 2, 16, 0x24, 1, 1, 0x800313a0));
  STEP(0x800313a4);
  TRY(load(run, 2, 17, 0x10, 1, 1, 0x800313a4));
  STEP(0x800313a8);
  STEP(0x800313ac);
  TRY(store(run, 2, 16, 0x0d, 1, 1, 0x800313ac));
  STEP(0x800313b0);
  TRY(load(run, 2, 17, 0x11, 1, 1, 0x800313b0));
  STEP(0x800313b4);
  STEP(0x800313b8);
  TRY(store(run, 2, 16, 0x15, 1, 1, 0x800313b8));
  STEP(0x800313bc);
  TRY(load(run, 2, 17, 0x12, 1, 1, 0x800313bc));
  STEP(0x800313c0);
  STEP(0x800313c4);
  TRY(store(run, 2, 16, 0x1d, 1, 1, 0x800313c4));
  STEP(0x800313c8);
  TRY(load(run, 2, 17, 0x13, 1, 1, 0x800313c8));
  STEP(0x800313cc);
  known(&V1, 0x80800000);
  STEP(0x800313d0);
  V1 = or_constant(V1, 0x8000);
  predicate = S7;
  STEP(0x800313d4);
  STEP(0x800313d8);
  TRY(store(run, 2, 16, 0x25, 1, 1, 0x800313d8));
  TRY(equal_decision(run, predicate, V1, 0x800313d4, &decision));
  if (!decision)
    goto custom_color;
  STEP(0x800313dc);
  known(&V0, 0x80);
  STEP(0x800313e0);
  TRY(store(run, 2, 16, 6, 1, 1, 0x800313e0));
  STEP(0x800313e4);
  TRY(store(run, 2, 16, 5, 1, 1, 0x800313e4));
  STEP(0x800313e8);
  STEP(0x800313ec);
  TRY(store(run, 2, 16, 4, 1, 1, 0x800313ec));
  goto color_done;
custom_color:
  STEP(0x800313f0);
  V0 = shift_right_arithmetic(S7, 24);
  STEP(0x800313f4);
  TRY(store(run, 2, 16, 4, 1, 1, 0x800313f4));
  STEP(0x800313f8);
  V0 = shift_right_arithmetic(S7, 16);
  STEP(0x800313fc);
  TRY(store(run, 2, 16, 5, 1, 1, 0x800313fc));
  STEP(0x80031400);
  V0 = shift_right_arithmetic(S7, 8);
  STEP(0x80031404);
  TRY(store(run, 2, 16, 6, 1, 1, 0x80031404));
color_done:
  STEP(0x80031408);
  TRY(store(run, 0, 16, 0x1e, 2, 2, 0x80031408));
  STEP(0x8003140c);
  TRY(store(run, 21, 16, 0x18, 2, 2, 0x8003140c));
  STEP(0x80031410);
  TRY(store(run, 21, 16, 8, 2, 2, 0x80031410));
  STEP(0x80031414);
  TRY(load(run, 2, 17, 9, 1, 1, 0x80031414));
  STEP(0x80031418);
  STEP(0x8003141c);
  V0 = add(S5, V0);
  STEP(0x80031420);
  TRY(store(run, 2, 16, 0x20, 2, 2, 0x80031420));
  STEP(0x80031424);
  TRY(store(run, 2, 16, 0x10, 2, 2, 0x80031424));
  STEP(0x80031428);
  TRY(load(run, 2, 17, 8, 1, 1, 0x80031428));
  STEP(0x8003142c);
  A0 = S0;
  STEP(0x80031430);
  TRY(load(run, 8, 29, 0x20, 2, 2, 0x80031430));
  STEP(0x80031434);
  known(&A2, 0x28);
  STEP(0x80031438);
  V0 = shift_left(V0, 24);
  STEP(0x8003143c);
  V0 = shift_right_arithmetic(V0, 24);
  STEP(0x80031440);
  V0 = add(T0, V0);
  STEP(0x80031444);
  TRY(store(run, 2, 4, 0x12, 2, 2, 0x80031444));
  STEP(0x80031448);
  TRY(store(run, 2, 4, 0x0a, 2, 2, 0x80031448));
  STEP(0x8003144c);
  TRY(load(run, 2, 17, 8, 1, 1, 0x8003144c));
  STEP(0x80031450);
  S0 = add(A0, immediate(0x28));
  STEP(0x80031454);
  A1 = S0;
  STEP(0x80031458);
  TRY(load(run, 3, 17, 0x0a, 1, 1, 0x80031458));
  STEP(0x8003145c);
  V0 = shift_left(V0, 24);
  STEP(0x80031460);
  V0 = shift_right_arithmetic(V0, 24);
  STEP(0x80031464);
  V0 = add(T0, V0);
  STEP(0x80031468);
  V1 = add(V1, V0);
  STEP(0x8003146c);
  TRY(store(run, 3, 4, 0x22, 2, 2, 0x8003146c));
  STEP(0x80031470);
  known(&RA, 0x80031478);
  STEP(0x80031474);
  TRY(store(run, 3, 4, 0x1a, 2, 2, 0x80031474));
  TRY(invoke(run, NBA97_GAME_TEXT_SUBMISSION_CHILD_800AA468, 0x80031470,
             0x80031474, 0x800aa468, 3));
  STEP(0x80031478);
  TRY(load(run, 2, 17, 9, 1, 1, 0x80031478));
  STEP(0x8003147c);
  TRY(load(run, 8, 29, 0x38, 2, 2, 0x8003147c));
  STEP(0x80031480);
  S0 = add(S0, immediate(0x28));
  STEP(0x80031484);
  V0 = subtract(V0, T0);
  STEP(0x80031488);
  S5 = add(S5, V0);

advance_character:
  STEP(0x8003148c);
  S2 = add(S2, immediate(1));
load_next_character:
  STEP(0x80031490);
  TRY(load(run, 2, 18, 0, 1, 1, 0x80031490));
  predicate = V0;
  STEP(0x80031494);
  STEP(0x80031498);
  STEP(0x8003149c);
  V0 = shift_left(V0, 1);
  TRY(zero_decision(run, predicate, 0x80031498, &decision));
  if (!decision)
    goto character_loop;

link_packets:
  /* 0x800314A0..0x800314EC: walk the generated packet pairs backward and
   * submit both heads, preserving callback-live s0 and sp on every iteration.
   */
  STEP(0x800314a0);
  TRY(load(run, 2, 19, 0x0c, 2, 2, 0x800314a0));
  STEP(0x800314a4);
  predicate = V0;
  STEP(0x800314a8);
  STEP(0x800314ac);
  TRY(store(run, 2, 29, 0x10, 2, 2, 0x800314ac));
  TRY(zero_decision(run, predicate, 0x800314a8, &decision));
  if (decision)
    goto success;
  STEP(0x800314b0);
  S0 = add(S0, immediate(UINT32_C(0xffffffb0)));
packet_link_loop:
  ++out->packet_link_iterations;
  STEP(0x800314b4);
  A0 = S3;
  STEP(0x800314b8);
  known(&RA, 0x800314c0);
  STEP(0x800314bc);
  A1 = S0;
  TRY(invoke(run, NBA97_GAME_TEXT_SUBMISSION_CHILD_80056914, 0x800314b8,
             0x800314bc, 0x80056914, 2));
  STEP(0x800314c0);
  A0 = add(S3, immediate(4));
  STEP(0x800314c4);
  known(&RA, 0x800314cc);
  STEP(0x800314c8);
  A1 = add(S0, immediate(0x28));
  TRY(invoke(run, NBA97_GAME_TEXT_SUBMISSION_CHILD_80056914, 0x800314c4,
             0x800314c8, 0x80056914, 2));
  STEP(0x800314cc);
  TRY(load(run, 2, 29, 0x10, 2, 2, 0x800314cc));
  STEP(0x800314d0);
  STEP(0x800314d4);
  V0 = add(V0, immediate(UINT32_MAX));
  STEP(0x800314d8);
  TRY(store(run, 2, 29, 0x10, 2, 2, 0x800314d8));
  STEP(0x800314dc);
  V0 = shift_left(V0, 16);
  predicate = V0;
  STEP(0x800314e0);
  STEP(0x800314e4);
  S0 = add(S0, immediate(UINT32_C(0xffffffb0)));
  TRY(zero_decision(run, predicate, 0x800314e0, &decision));
  if (!decision)
    goto packet_link_loop;
  STEP(0x800314e8);
  S0 = add(S0, immediate(0x50));

success:
  STEP(0x800314ec);
  V0 = S3;

epilogue:
  /* 0x800314F0..0x80031520: restore every saved word through callback-live sp,
   * advance that live frame, and execute the JR NOP before target validation.
   */
  out->return_v0 = V0;
  STEP(0x800314f0);
  TRY(load(run, 31, 29, 0x7c, 4, 4, 0x800314f0));
  STEP(0x800314f4);
  TRY(load(run, 30, 29, 0x78, 4, 4, 0x800314f4));
  STEP(0x800314f8);
  TRY(load(run, 23, 29, 0x74, 4, 4, 0x800314f8));
  STEP(0x800314fc);
  TRY(load(run, 22, 29, 0x70, 4, 4, 0x800314fc));
  STEP(0x80031500);
  TRY(load(run, 21, 29, 0x6c, 4, 4, 0x80031500));
  STEP(0x80031504);
  TRY(load(run, 20, 29, 0x68, 4, 4, 0x80031504));
  STEP(0x80031508);
  TRY(load(run, 19, 29, 0x64, 4, 4, 0x80031508));
  STEP(0x8003150c);
  TRY(load(run, 18, 29, 0x60, 4, 4, 0x8003150c));
  STEP(0x80031510);
  TRY(load(run, 17, 29, 0x5c, 4, 4, 0x80031510));
  STEP(0x80031514);
  TRY(load(run, 16, 29, 0x58, 4, 4, 0x80031514));
  STEP(0x80031518);
  SP = add(SP, immediate(0x80));
  publish(run);
  STEP(0x8003151c);
  STEP(0x80031520);
  if (RA.known_mask != 15) {
    stop(run, 0x8003151c, RA.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, 0x8003151c, RA.word, 0);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
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

static int negative_decision(Run *run, Word value, uint32_t pc, int *negative) {
  if (value.known_mask & 8u) {
    *negative = !!(value.word & UINT32_C(0x80000000));
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}
