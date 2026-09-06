#include "game_countdown_ui_update.h"

#include <limits.h>
#include <string.h>

typedef Nba97GameCountdownUiUpdateWord Word;

typedef struct Run {
  Nba97GameCountdownUiUpdateContext *context;
  Nba97GameCountdownUiUpdateProgress *out;
  Nba97GameCountdownUiUpdateMachine machine;
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

static int machine_valid(const Nba97GameCountdownUiUpdateMachine *machine) {
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

static int initialize(Nba97GameCountdownUiUpdateContext *context,
                      Nba97GameCountdownUiUpdateProgress *out, Run *run) {
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
    Nba97GameCountdownUiUpdateAccess *event =
        &run->context->access_journal[index];
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
  journal(run, NBA97_GAME_COUNTDOWN_UI_UPDATE_READ, pc, address, width, loaded);
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
  journal(run, NBA97_GAME_COUNTDOWN_UI_UPDATE_STORE, pc, address, width, value);
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

static int read_piece(Run *run, uint32_t pc, uint32_t actual, unsigned width,
                      uint32_t logical, Word *value) {
  uint8_t *data;
  uint8_t *known_bytes;
  Word staged = *value;
  Word observed = {0, 0};
  unsigned i;
  TRY(locate(run, actual, width, 1, pc, &data, &known_bytes));
  for (i = 0; i < width; ++i) {
    uint32_t offset = (actual + i) - logical;
    if (offset >= 4)
      return NBA97_TEXT_ARGUMENT;
    staged.word &= ~(UINT32_C(255) << (offset * 8u));
    staged.word |= (uint32_t)data[i] << (offset * 8u);
    staged.known_mask =
        (uint8_t)(staged.known_mask & (uint8_t) ~(1u << offset));
    observed.word |= (uint32_t)data[i] << (i * 8u);
    if (!known_bytes || known_bytes[i]) {
      staged.known_mask = (uint8_t)(staged.known_mask | (1u << offset));
      observed.known_mask = (uint8_t)(observed.known_mask | (1u << i));
    }
  }
  *value = staged;
  ++run->out->reads;
  journal(run, NBA97_GAME_COUNTDOWN_UI_UPDATE_READ, pc, actual, width,
          observed);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_piece(Run *run, uint32_t pc, uint32_t actual, unsigned width,
                       uint32_t logical, Word value) {
  uint8_t *data;
  uint8_t *known_bytes;
  Word observed = {0, 0};
  unsigned i;
  TRY(locate(run, actual, width, 1, pc, &data, &known_bytes));
  for (i = 0; i < width; ++i) {
    uint32_t offset = (actual + i) - logical;
    if (offset >= 4)
      return NBA97_TEXT_ARGUMENT;
    if (!known_bytes && !(value.known_mask & (1u << offset)))
      return NBA97_TEXT_ARGUMENT;
    observed.word |= ((value.word >> (offset * 8u)) & 255u) << (i * 8u);
    if (value.known_mask & (1u << offset))
      observed.known_mask = (uint8_t)(observed.known_mask | (1u << i));
  }
  for (i = 0; i < width; ++i) {
    uint32_t offset = (actual + i) - logical;
    data[i] = (uint8_t)(value.word >> (offset * 8u));
    if (known_bytes)
      known_bytes[i] = (uint8_t)((value.known_mask >> offset) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_COUNTDOWN_UI_UPDATE_STORE, pc, actual, width,
          observed);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int32_t signed_word(uint32_t value);
static void multiply_signed(Run *run, Word left, Word right) {
  uint64_t bits = (uint64_t)((int64_t)signed_word(left.word) *
                             (int64_t)signed_word(right.word));
  unsigned byte;
  run->machine.lo.word = (uint32_t)bits;
  run->machine.hi.word = (uint32_t)(bits >> 32u);
  run->machine.lo.known_mask = 0;
  run->machine.hi.known_mask = 0;
  if (left.known_mask == 15 && right.known_mask == 15) {
    run->machine.lo.known_mask = 15;
    run->machine.hi.known_mask = 15;
  } else {
    for (byte = 0; byte < 4; ++byte) {
      uint8_t prefix = (uint8_t)((1u << (byte + 1u)) - 1u);
      if ((left.known_mask & prefix) == prefix &&
          (right.known_mask & prefix) == prefix)
        run->machine.lo.known_mask =
            (uint8_t)(run->machine.lo.known_mask | (1u << byte));
    }
  }
  publish(run);
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

static int invoke(Run *run, uint8_t kind, uint32_t pc, uint32_t delay_pc,
                  uint32_t entry, uint8_t argument_count) {
  Nba97GameCountdownUiUpdateEvent event;
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

int nba97_game_countdown_ui_update(Nba97GameCountdownUiUpdateContext *context,
                                   Nba97GameCountdownUiUpdateProgress *out) {
  Run storage;
  Run *run = &storage;
  Word predicate;
  Word loop_condition;
  uint32_t logical;
  int decision;
  TRY(initialize(context, out, run));

  /* 0x8003287C..0x80032894: capture the countdown before allocating the live
   * frame, then save ra/s2/s1/s0 in source order. */
  STEP(0x8003287c);
  known(&A2, 0x80100000);
  STEP(0x80032880);
  TRY(load(run, 6, 6, UINT32_C(0xffffdba4), 4, 4, 0x80032880));
  STEP(0x80032884);
  SP = add(SP, immediate(UINT32_C(0xffffffc0)));
  out->frame_stack_pointer = SP.word;
  STEP(0x80032888);
  out->saved_return_address = RA;
  TRY(store(run, 31, 29, 0x3c, 4, 4, 0x80032888));
  STEP(0x8003288c);
  TRY(store(run, 18, 29, 0x38, 4, 4, 0x8003288c));
  STEP(0x80032890);
  TRY(store(run, 17, 29, 0x34, 4, 4, 0x80032890));
  STEP(0x80032894);
  TRY(store(run, 16, 29, 0x30, 4, 4, 0x80032894));

  /* 0x80032898..0x800328F4: copy five runtime words through the original
   * redundant LWL/LWR and SWL/SWR accesses, then copy the signed tail half. */
  STEP(0x80032898);
  known(&A1, 0x80020000);
  STEP(0x8003289c);
  A1 = add(A1, immediate(0x49e4));
#define READ_PAIR(reg, source_offset, lwl_pc, lwr_pc)                          \
  do {                                                                         \
    logical = A1.word + (source_offset);                                       \
    STEP(lwl_pc);                                                              \
    TRY(read_piece(run, lwl_pc, (logical + 3u) & ~UINT32_C(3),                 \
                   ((logical + 3u) & 3u) + 1u, logical, &(reg)));              \
    STEP(lwr_pc);                                                              \
    TRY(read_piece(run, lwr_pc, logical, 4u - (logical & 3u), logical,         \
                   &(reg)));                                                   \
  } while (0)
#define WRITE_PAIR(reg, local_offset, swl_pc, swr_pc)                          \
  do {                                                                         \
    STEP(swl_pc);                                                              \
    TRY(address(run, SP, local_offset, swl_pc, &logical));                     \
    TRY(write_piece(run, swl_pc, (logical + 3u) & ~UINT32_C(3),                \
                    ((logical + 3u) & 3u) + 1u, logical, (reg)));              \
    STEP(swr_pc);                                                              \
    TRY(write_piece(run, swr_pc, logical, 4u - (logical & 3u), logical,        \
                    (reg)));                                                   \
  } while (0)
  READ_PAIR(V0, 0x00, 0x800328a0, 0x800328a4);
  READ_PAIR(V1, 0x04, 0x800328a8, 0x800328ac);
  READ_PAIR(A0, 0x08, 0x800328b0, 0x800328b4);
  WRITE_PAIR(V0, 0x18, 0x800328b8, 0x800328bc);
  WRITE_PAIR(V1, 0x1c, 0x800328c0, 0x800328c4);
  WRITE_PAIR(A0, 0x20, 0x800328c8, 0x800328cc);
  logical = A1.word + 0x0cu;
  STEP(0x800328d0);
  TRY(read_piece(run, 0x800328d0, (logical + 3u) & ~UINT32_C(3),
                 ((logical + 3u) & 3u) + 1u, logical, &V0));
  STEP(0x800328d4);
  TRY(read_piece(run, 0x800328d4, logical, 4u - (logical & 3u), logical, &V0));
  logical = A1.word + 0x10u;
  STEP(0x800328d8);
  TRY(read_piece(run, 0x800328d8, (logical + 3u) & ~UINT32_C(3),
                 ((logical + 3u) & 3u) + 1u, logical, &V1));
  STEP(0x800328dc);
  TRY(read_piece(run, 0x800328dc, logical, 4u - (logical & 3u), logical, &V1));
  STEP(0x800328e0);
  TRY(load(run, 4, 5, 0x14, 2, 2, 0x800328e0));
  A0 = sign_extend_half(A0);
  STEP(0x800328e4);
  TRY(address(run, SP, 0x24, 0x800328e4, &logical));
  TRY(write_piece(run, 0x800328e4, (logical + 3u) & ~UINT32_C(3),
                  ((logical + 3u) & 3u) + 1u, logical, V0));
  STEP(0x800328e8);
  TRY(write_piece(run, 0x800328e8, logical, 4u - (logical & 3u), logical, V0));
  STEP(0x800328ec);
  TRY(address(run, SP, 0x28, 0x800328ec, &logical));
  TRY(write_piece(run, 0x800328ec, (logical + 3u) & ~UINT32_C(3),
                  ((logical + 3u) & 3u) + 1u, logical, V1));
  STEP(0x800328f0);
  TRY(write_piece(run, 0x800328f0, logical, 4u - (logical & 3u), logical, V1));
  STEP(0x800328f4);
  TRY(store(run, 4, 29, 0x2c, 2, 2, 0x800328f4));
#undef WRITE_PAIR
#undef READ_PAIR

  /* 0x800328F8..0x80032940: evaluate every signed/global gate. The first
   * branch delay establishes s1, and the feature branch delay writes V0's
   * magic high half on both outcomes. */
  STEP(0x800328f8);
  V0 = signed_less(A2, immediate(601));
  STEP(0x800328fc);
  STEP(0x80032900);
  S1 = add(SP, immediate(0x18));
  TRY(zero_decision(run, V0, 0x800328fc, &decision));
  if (decision)
    goto failed_gate;
  STEP(0x80032904);
  known(&V0, 0x80100000);
  STEP(0x80032908);
  TRY(load(run, 2, 2, UINT32_C(0xffffe8cc), 2, 2, 0x80032908));
  V0 = sign_extend_half(V0);
  STEP(0x8003290c);
  STEP(0x80032910);
  STEP(0x80032914);
  TRY(zero_decision(run, V0, 0x80032910, &decision));
  if (!decision)
    goto failed_gate;
  STEP(0x80032918);
  known(&V0, 0x80100000);
  STEP(0x8003291c);
  TRY(load(run, 2, 2, UINT32_C(0xffffdb58), 4, 4, 0x8003291c));
  STEP(0x80032920);
  STEP(0x80032924);
  V0 = signed_less(V0, A2);
  STEP(0x80032928);
  STEP(0x8003292c);
  TRY(zero_decision(run, V0, 0x80032928, &decision));
  if (!decision)
    goto failed_gate;
  STEP(0x80032930);
  known(&V0, 0x80020000);
  STEP(0x80032934);
  TRY(load(run, 2, 2, 0x1d92, 1, 1, 0x80032934));
  STEP(0x80032938);
  STEP(0x8003293c);
  predicate = V0;
  STEP(0x80032940);
  known(&V0, 0x88880000);
  TRY(zero_decision(run, predicate, 0x8003293c, &decision));
  if (!decision)
    goto active_gate;

failed_gate:
  /* 0x80032944..0x8003296C: compare the live cache with -1, optionally issue
   * command C9, then store -1 through callback-live s0 in the jump delay. */
  STEP(0x80032944);
  known(&S0, 0x80100000);
  STEP(0x80032948);
  S0 = add(S0, immediate(UINT32_C(0xffffea2e)));
  STEP(0x8003294c);
  TRY(load(run, 3, 16, 0, 2, 2, 0x8003294c));
  V1 = sign_extend_half(V1);
  STEP(0x80032950);
  known(&V0, UINT32_MAX);
  STEP(0x80032954);
  STEP(0x80032958);
  known(&V0, UINT32_MAX);
  TRY(equal_decision(run, V1, V0, 0x80032954, &decision));
  if (!decision) {
    STEP(0x8003295c);
    known(&RA, 0x80032964);
    STEP(0x80032960);
    known(&A0, 0xc9);
    TRY(invoke(run, NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_8003066C, 0x8003295c,
               0x80032960, 0x8003066c, 1));
    STEP(0x80032964);
    known(&V0, UINT32_MAX);
  }
  STEP(0x80032968);
  STEP(0x8003296c);
  TRY(store(run, 2, 16, 0, 2, 2, 0x8003296c));
  goto epilogue;

active_gate:
  out->active_gate = 1;
  /* 0x80032970..0x800329AC: perform the source signed magic division by 60,
   * leaving MULT HI/LO and mfhi t2 live, then compare signed-low quotient. */
  STEP(0x80032970);
  V0 = or_constant(V0, 0x8889);
  STEP(0x80032974);
  V1 = add(A2, immediate(59));
  STEP(0x80032978);
  multiply_signed(run, V1, V0);
  STEP(0x8003297c);
  known(&A0, 0x80100000);
  STEP(0x80032980);
  A0 = add(A0, immediate(UINT32_C(0xffffea2e)));
  STEP(0x80032984);
  T2 = run->machine.hi;
  STEP(0x80032988);
  V0 = add(T2, V1);
  STEP(0x8003298c);
  V0 = shift_right_arithmetic(V0, 5);
  STEP(0x80032990);
  V1 = shift_right_arithmetic(V1, 31);
  STEP(0x80032994);
  V0 = subtract(V0, V1);
  STEP(0x80032998);
  TRY(load(run, 3, 4, 0, 2, 2, 0x80032998));
  V1 = sign_extend_half(V1);
  STEP(0x8003299c);
  S2 = V0;
  STEP(0x800329a0);
  V0 = shift_left(V0, 16);
  STEP(0x800329a4);
  S0 = shift_right_arithmetic(V0, 16);
  STEP(0x800329a8);
  predicate = V1;
  STEP(0x800329ac);
  known(&V1, 0x300);
  TRY(equal_decision(run, S0, predicate, 0x800329a8, &decision));
  if (decision)
    goto epilogue;

  /* 0x800329B0..0x800329F0: update the live font mode and optionally issue the
   * five-argument text call; both paths establish a1=13 before indexing. */
  STEP(0x800329b0);
  known(&V0, 0x800b0000);
  STEP(0x800329b4);
  TRY(load(run, 2, 2, 0x2048, 4, 4, 0x800329b4));
  STEP(0x800329b8);
  STEP(0x800329bc);
  TRY(store(run, 3, 2, 0x26, 2, 2, 0x800329bc));
  STEP(0x800329c0);
  TRY(load(run, 2, 4, 0, 2, 2, 0x800329c0));
  V0 = sign_extend_half(V0);
  STEP(0x800329c4);
  STEP(0x800329c8);
  STEP(0x800329cc);
  known(&A1, 13);
  predicate = signed_less(V0, immediate(0));
  TRY(zero_decision(run, predicate, 0x800329c8, &decision));
  if (!decision) {
    STEP(0x800329d0);
    known(&V0, 2);
    STEP(0x800329d4);
    TRY(store(run, 2, 29, 0x10, 4, 4, 0x800329d4));
    STEP(0x800329d8);
    known(&A0, 0xc9);
    STEP(0x800329dc);
    known(&A1, 0x80020000);
    STEP(0x800329e0);
    A1 = add(A1, immediate(0x49fc));
    STEP(0x800329e4);
    known(&A2, 0x1ec);
    STEP(0x800329e8);
    known(&RA, 0x800329f0);
    STEP(0x800329ec);
    known(&A3, 0x14);
    TRY(invoke(run, NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_80030D18, 0x800329e8,
               0x800329ec, 0x80030d18, 5));
    STEP(0x800329f0);
    known(&A1, 13);
  }

  /* 0x800329F4..0x80032A84: index the copied runtime table, then build the
   * record with SB before LBU and the exact ordered stores before header SW. */
  STEP(0x800329f4);
  V0 = shift_left(S0, 1);
  STEP(0x800329f8);
  V0 = add(V0, S1);
  STEP(0x800329fc);
  TRY(load(run, 4, 2, 0, 2, 2, 0x800329fc));
  STEP(0x80032a00);
  known(&V1, 0x80100000);
  STEP(0x80032a04);
  V1 = add(V1, immediate(UINT32_C(0xffffb5c0)));
  STEP(0x80032a08);
  A3 = add(V1, immediate(0x10));
  STEP(0x80032a0c);
  known(&V0, 0x23);
  STEP(0x80032a10);
  TRY(store(run, 2, 3, 0, 1, 1, 0x80032a10));
  STEP(0x80032a14);
  TRY(load(run, 6, 3, 0, 1, 1, 0x80032a14));
  STEP(0x80032a18);
  known(&T1, 0x1f);
  STEP(0x80032a1c);
  known(&T0, 8);
  STEP(0x80032a20);
  known(&V0, 0x10);
  STEP(0x80032a24);
  known(&AT, 0x80100000);
  STEP(0x80032a28);
  TRY(store(run, 2, 1, UINT32_C(0xffffb5c4), 2, 2, 0x80032a28));
  STEP(0x80032a2c);
  known(&V0, 1);
  STEP(0x80032a30);
  known(&AT, 0x80100000);
  STEP(0x80032a34);
  TRY(store(run, 2, 1, UINT32_C(0xffffb5c6), 2, 2, 0x80032a34));
  STEP(0x80032a38);
  known(&V0, 0x421);
  STEP(0x80032a3c);
  known(&AT, 0x80100000);
  STEP(0x80032a40);
  TRY(store(run, 2, 1, UINT32_C(0xffffb5d0), 2, 2, 0x80032a40));
  STEP(0x80032a44);
  known(&V0, 0x1a98);
  STEP(0x80032a48);
  known(&AT, 0x80100000);
  STEP(0x80032a4c);
  TRY(store(run, 2, 1, UINT32_C(0xffffb5ec), 2, 2, 0x80032a4c));
  STEP(0x80032a50);
  known(&V0, 0x4210);
  STEP(0x80032a54);
  known(&AT, 0x80100000);
  STEP(0x80032a58);
  TRY(store(run, 0, 1, UINT32_C(0xffffb5ce), 2, 2, 0x80032a58));
  STEP(0x80032a5c);
  known(&AT, 0x80100000);
  STEP(0x80032a60);
  TRY(store(run, 0, 1, UINT32_C(0xffffb5cc), 2, 2, 0x80032a60));
  STEP(0x80032a64);
  known(&AT, 0x80100000);
  STEP(0x80032a68);
  TRY(store(run, 0, 1, UINT32_C(0xffffb5ca), 2, 2, 0x80032a68));
  STEP(0x80032a6c);
  known(&AT, 0x80100000);
  STEP(0x80032a70);
  TRY(store(run, 0, 1, UINT32_C(0xffffb5c8), 2, 2, 0x80032a70));
  STEP(0x80032a74);
  known(&AT, 0x80100000);
  STEP(0x80032a78);
  TRY(store(run, 2, 1, UINT32_C(0xffffb5ee), 2, 2, 0x80032a78));
  STEP(0x80032a7c);
  A0 = shift_left(A0, 16);
  STEP(0x80032a80);
  A0 = shift_right_arithmetic(A0, 18);
  STEP(0x80032a84);
  TRY(store(run, 6, 3, 0, 4, 4, 0x80032a84));

  /* 0x80032A88..0x80032AC8: write thirteen descending palette halfwords from
   * signed table bits. The branch delay computes the next low-bit predicate. */
  STEP(0x80032a88);
  V0 = and_constant(A0, 1);
  predicate = V0;
palette_loop:
  ++out->palette_iterations;
  STEP(0x80032a8c);
  STEP(0x80032a90);
  V0 = shift_left(A1, 16);
  TRY(zero_decision(run, predicate, 0x80032a8c, &decision));
  if (!decision) {
    STEP(0x80032a94);
    V0 = shift_right_arithmetic(V0, 15);
    STEP(0x80032a98);
    V0 = add(V0, A3);
    STEP(0x80032a9c);
    STEP(0x80032aa0);
    TRY(store(run, 9, 2, 0, 2, 2, 0x80032aa0));
  } else {
    STEP(0x80032aa4);
    V0 = shift_right_arithmetic(V0, 15);
    STEP(0x80032aa8);
    V0 = add(V0, A3);
    STEP(0x80032aac);
    TRY(store(run, 8, 2, 0, 2, 2, 0x80032aac));
  }
  STEP(0x80032ab0);
  V0 = shift_left(A0, 16);
  STEP(0x80032ab4);
  A0 = shift_right_arithmetic(V0, 17);
  STEP(0x80032ab8);
  V0 = add(A1, immediate(UINT32_MAX));
  STEP(0x80032abc);
  A1 = V0;
  STEP(0x80032ac0);
  V0 = shift_left(V0, 16);
  STEP(0x80032ac4);
  loop_condition = signed_less(immediate(0), V0);
  STEP(0x80032ac8);
  V0 = and_constant(A0, 1);
  predicate = V0;
  TRY(zero_decision(run, loop_condition, 0x80032ac4, &decision));
  if (!decision)
    goto palette_loop;

  /* 0x80032ACC..0x80032AF0: submit the record. The JAL delay stores F0 through
   * live sp before the typed child; its callback-live s2 is then cached. */
  STEP(0x80032acc);
  known(&A0, 0x80100000);
  STEP(0x80032ad0);
  A0 = add(A0, immediate(UINT32_C(0xffffb5c0)));
  STEP(0x80032ad4);
  known(&A1, 0);
  STEP(0x80032ad8);
  known(&A2, 0);
  STEP(0x80032adc);
  known(&A3, 0x340);
  STEP(0x80032ae0);
  known(&V0, 0xf0);
  STEP(0x80032ae4);
  known(&RA, 0x80032aec);
  STEP(0x80032ae8);
  TRY(store(run, 2, 29, 0x10, 4, 4, 0x80032ae8));
  TRY(invoke(run, NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_80094540, 0x80032ae4,
             0x80032ae8, 0x80094540, 5));
  STEP(0x80032aec);
  known(&AT, 0x80100000);
  STEP(0x80032af0);
  TRY(store(run, 18, 1, UINT32_C(0xffffea2e), 2, 2, 0x80032af0));
  out->record_uploaded = 1;

epilogue:
  /* 0x80032AF4..0x80032B0C: restore through callback-live sp, then execute the
   * JR NOP delay before validating the restored target. */
  STEP(0x80032af4);
  TRY(load(run, 31, 29, 0x3c, 4, 4, 0x80032af4));
  out->restored_return_address = RA;
  STEP(0x80032af8);
  TRY(load(run, 18, 29, 0x38, 4, 4, 0x80032af8));
  STEP(0x80032afc);
  TRY(load(run, 17, 29, 0x34, 4, 4, 0x80032afc));
  STEP(0x80032b00);
  TRY(load(run, 16, 29, 0x30, 4, 4, 0x80032b00));
  STEP(0x80032b04);
  SP = add(SP, immediate(0x40));
  publish(run);
  STEP(0x80032b08);
  STEP(0x80032b0c);
  if (RA.known_mask != 15) {
    stop(run, 0x80032b08, RA.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, 0x80032b08, RA.word, 0);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
