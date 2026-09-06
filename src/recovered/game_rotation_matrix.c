#include "game_rotation_matrix.h"

#include <string.h>

typedef Nba97GameRotationMatrixWord Word;
typedef struct Run {
  Nba97GameRotationMatrixContext *context;
  Nba97GameRotationMatrixProgress *out;
  Nba97GameRotationMatrixMachine machine;
} Run;

#define R(n) (run->machine.registers.gpr[(n)])
#define ZERO R(0)
#define AT R(1)
#define V0 R(2)
#define A0 R(4)
#define A1 R(5)
#define T0 R(8)
#define T1 R(9)
#define T2 R(10)
#define T3 R(11)
#define T4 R(12)
#define T5 R(13)
#define T6 R(14)
#define T7 R(15)
#define T8 R(24)
#define T9 R(25)
#define RA R(31)
#define TRY(expression)                                                        \
  do {                                                                         \
    int result_ = (expression);                                                \
    if (result_ != NBA97_TEXT_COMPLETE)                                        \
      return result_;                                                          \
  } while (0)
#define STEP(pc_)                                                              \
  do {                                                                         \
    (void)(pc_);                                                               \
    ++out->instruction_count;                                                  \
  } while (0)

static void set_known(Word *value, uint32_t word) {
  value->word = word;
  value->known_mask = 15;
}
static Word immediate(uint32_t word) {
  Word value;
  set_known(&value, word);
  return value;
}
static void publish(Run *run) {
  run->out->machine = run->machine;
  run->out->returned_value = V0;
}
static void stop(Run *run, uint32_t pc, uint32_t address) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  publish(run);
}
static int machine_valid(const Nba97GameRotationMatrixMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word ||
      machine->registers.gpr[0].known_mask != 15 ||
      machine->hi.known_mask > 15 || machine->lo.known_mask > 15)
    return 0;
  for (i = 0; i < 32; ++i)
    if (machine->registers.gpr[i].known_mask > 15)
      return 0;
  return 1;
}
static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}
static uint32_t width_mask(unsigned width) {
  return width == 4 ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - UINT32_C(1);
}
static uint8_t known_width(unsigned width) {
  return (uint8_t)((1u << width) - 1u);
}
static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width, Word value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameRotationMatrixAccess *event = &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word & width_mask(width);
    event->operation = run->out->operations;
    event->width = (uint8_t)width;
    event->known_mask = (uint8_t)(value.known_mask & known_width(width));
    event->kind = kind;
  }
}
static int locate(Run *run, uint32_t address, unsigned width, uint32_t pc,
                  uint8_t **data, uint8_t **known) {
  size_t i;
  size_t j;
  stop(run, pc, address);
  TRY(spend(run));
  ++run->out->accesses;
  if (address & (width - 1u))
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0; i < run->context->memory.count; ++i) {
    Nba97GameTextRegion *region = &run->context->memory.region[i];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        width > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known = region->known ? region->known + (size_t)offset : 0;
    if (*known)
      for (j = 0; j < width; ++j)
        if ((*known)[j] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}
static int read_memory(Run *run, uint32_t address, unsigned width, uint32_t pc,
                       Word *value) {
  uint8_t *data;
  uint8_t *known;
  Word loaded = {0, 0};
  unsigned i;
  TRY(locate(run, address, width, pc, &data, &known));
  for (i = 0; i < width; ++i) {
    loaded.word |= (uint32_t)data[i] << (i * 8u);
    if (!known || known[i])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
  }
  *value = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, address, width, loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}
static int write_memory(Run *run, uint32_t address, unsigned width, uint32_t pc,
                        Word value) {
  uint8_t *data;
  uint8_t *known;
  unsigned i;
  value.word &= width_mask(width);
  value.known_mask &= known_width(width);
  TRY(locate(run, address, width, pc, &data, &known));
  if (!known && value.known_mask != known_width(width))
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < width; ++i) {
    data[i] = (uint8_t)(value.word >> (i * 8u));
    if (known)
      known[i] = (uint8_t)((value.known_mask >> i) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

/* Enumerating byte carries and borrows retains every invariant result byte. */
static Word add_words(Word left, Word right) {
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
static Word subtract_words(Word left, Word right) {
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
          } else if (output != first_output) {
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
static int guest_address(Run *run, Word base, int32_t offset, uint32_t pc,
                         uint32_t *address) {
  Word value = add_words(base, immediate((uint32_t)offset));
  if (value.known_mask != 15) {
    stop(run, pc, value.word);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = value.word;
  return NBA97_TEXT_COMPLETE;
}
static Word extend(Word value, unsigned width, int sign) {
  Word result;
  value.word &= width_mask(width);
  value.known_mask &= known_width(width);
  result.word = value.word;
  if (sign && (value.word & (UINT32_C(1) << (width * 8u - 1u))))
    result.word |= ~width_mask(width);
  result.known_mask = value.known_mask;
  if (!sign)
    result.known_mask |= (uint8_t)(15u ^ known_width(width));
  else if (value.known_mask & (1u << (width - 1u)))
    result.known_mask |= (uint8_t)(15u ^ known_width(width));
  return result;
}
static int load(Run *run, unsigned target, unsigned base, int32_t offset,
                unsigned width, int sign, uint32_t pc) {
  uint32_t address;
  Word value;
  TRY(guest_address(run, R(base), offset, pc, &address));
  TRY(read_memory(run, address, width, pc, &value));
  R(target) = extend(value, width, sign);
  return NBA97_TEXT_COMPLETE;
}
static int store(Run *run, unsigned source, unsigned base, int32_t offset,
                 unsigned width, uint32_t pc) {
  uint32_t address;
  TRY(guest_address(run, R(base), offset, pc, &address));
  return write_memory(run, address, width, pc, R(source));
}
static Word shift_word(Word value, unsigned amount, int right, int arithmetic) {
  Word result;
  unsigned output;
  result.word = right ? value.word >> amount : value.word << amount;
  if (right && arithmetic && (value.word & UINT32_C(0x80000000)))
    result.word |= UINT32_MAX << (32u - amount);
  result.known_mask = 0;
  for (output = 0; output < 4; ++output) {
    unsigned low = output * 8u;
    unsigned high = low + 7u;
    unsigned source_low;
    unsigned source_high;
    unsigned source;
    int all = 1;
    if (!right) {
      if (high < amount) {
        result.known_mask |= (uint8_t)(1u << output);
        continue;
      }
      source_low = low < amount ? 0 : low - amount;
      source_high = high - amount;
    } else {
      source_low = low + amount;
      source_high = high + amount;
      if (source_low >= 32) {
        if (!arithmetic || (value.known_mask & 8u))
          result.known_mask |= (uint8_t)(1u << output);
        continue;
      }
      if (source_high >= 32) {
        if (arithmetic && !(value.known_mask & 8u))
          continue;
        source_high = 31;
      }
    }
    for (source = source_low / 8u; source <= source_high / 8u; ++source)
      if (!(value.known_mask & (1u << source)))
        all = 0;
    if (all)
      result.known_mask |= (uint8_t)(1u << output);
  }
  return result;
}
static Word and_constant(Word value, uint32_t mask) {
  Word result;
  unsigned byte;
  result.word = value.word & mask;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    uint32_t part = (mask >> (byte * 8u)) & 255u;
    if (!part || (value.known_mask & (1u << byte)))
      result.known_mask |= (uint8_t)(1u << byte);
  }
  return result;
}
static int nonnegative(Word value, int *answer) {
  if (!(value.known_mask & 8u))
    return 0;
  *answer = (value.word & UINT32_C(0x80000000)) == 0;
  return 1;
}
enum {
  HALF_DOMAIN_NONE = 0,
  HALF_DOMAIN_SIGNED = 1,
  HALF_DOMAIN_SIGNED_OR_NEGATED_EXTREME = 2
};

static uint32_t sign_extended_half(uint32_t candidate) {
  uint32_t word = candidate & UINT32_C(0xffff);
  if (word & UINT32_C(0x8000))
    word |= UINT32_C(0xffff0000);
  return word;
}
static int concrete_word_matches(Word value, uint32_t word) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((value.known_mask & (1u << byte)) &&
        (((value.word >> (byte * 8u)) & 255u) !=
         ((word >> (byte * 8u)) & 255u)))
      return 0;
  return 1;
}
static size_t half_domain_count(Word value, int domain) {
  uint32_t candidate;
  size_t count = 0;
  for (candidate = 0; candidate < UINT32_C(0x10000); ++candidate)
    if (concrete_word_matches(value, sign_extended_half(candidate)))
      ++count;
  if (domain == HALF_DOMAIN_SIGNED_OR_NEGATED_EXTREME &&
      concrete_word_matches(value, UINT32_C(0x00008000)))
    ++count;
  return count;
}
static size_t collect_half_domain(Word value, int domain, uint32_t *words,
                                  size_t capacity) {
  uint32_t candidate;
  size_t count = 0;
  for (candidate = 0; candidate < UINT32_C(0x10000); ++candidate) {
    uint32_t word = sign_extended_half(candidate);
    if (concrete_word_matches(value, word)) {
      if (count < capacity)
        words[count] = word;
      ++count;
    }
  }
  if (domain == HALF_DOMAIN_SIGNED_OR_NEGATED_EXTREME &&
      concrete_word_matches(value, UINT32_C(0x00008000))) {
    if (count < capacity)
      words[count] = UINT32_C(0x00008000);
    ++count;
  }
  return count;
}
static void compare_product_bytes(uint64_t product, Word *lo, Word *hi) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte) {
    if (((uint32_t)product >> (byte * 8u) & 255u) !=
        (lo->word >> (byte * 8u) & 255u))
      lo->known_mask = (uint8_t)(lo->known_mask & ~(1u << byte));
    if (((uint32_t)(product >> 32u) >> (byte * 8u) & 255u) !=
        (hi->word >> (byte * 8u) & 255u))
      hi->known_mask = (uint8_t)(hi->known_mask & ~(1u << byte));
  }
}
static int refine_half_product(Word left, int left_domain, Word right,
                               int right_domain, Word *lo, Word *hi) {
  uint32_t smaller[256];
  Word larger_value;
  int larger_domain;
  int smaller_is_left;
  size_t left_count = half_domain_count(left, left_domain);
  size_t right_count = half_domain_count(right, right_domain);
  size_t smaller_count;
  uint32_t candidate;
  size_t index;

  if (!left_count || !right_count || left_count > 65536u / right_count)
    return 0;
  smaller_is_left = left_count <= right_count;
  smaller_count = collect_half_domain(
      smaller_is_left ? left : right,
      smaller_is_left ? left_domain : right_domain, smaller, 256);
  if (smaller_count > 256)
    return 0;
  larger_value = smaller_is_left ? right : left;
  larger_domain = smaller_is_left ? right_domain : left_domain;
  for (candidate = 0; candidate < UINT32_C(0x10000); ++candidate) {
    uint32_t larger_word = sign_extended_half(candidate);
    if (!concrete_word_matches(larger_value, larger_word))
      continue;
    for (index = 0; index < smaller_count; ++index)
      compare_product_bytes((uint64_t)smaller[index] * larger_word, lo, hi);
  }
  if (larger_domain == HALF_DOMAIN_SIGNED_OR_NEGATED_EXTREME &&
      concrete_word_matches(larger_value, UINT32_C(0x00008000)))
    for (index = 0; index < smaller_count; ++index)
      compare_product_bytes((uint64_t)smaller[index] * UINT32_C(0x00008000), lo,
                            hi);
  return 1;
}
static void multiply_unsigned(Run *run, Word left, Word right, int left_domain,
                              int right_domain) {
  uint64_t product = (uint64_t)left.word * (uint64_t)right.word;
  unsigned byte;
  run->machine.lo.word = (uint32_t)product;
  run->machine.hi.word = (uint32_t)(product >> 32u);
  run->machine.lo.known_mask = 0;
  run->machine.hi.known_mask = 0;
  if ((left.known_mask == 15 && left.word == 0) ||
      (right.known_mask == 15 && right.word == 0) ||
      (left.known_mask == 15 && right.known_mask == 15)) {
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
    if (left_domain != HALF_DOMAIN_NONE && right_domain != HALF_DOMAIN_NONE) {
      uint8_t previous_lo_mask = run->machine.lo.known_mask;
      uint8_t previous_hi_mask = run->machine.hi.known_mask;
      run->machine.lo.known_mask = 15;
      run->machine.hi.known_mask = 15;
      if (!refine_half_product(left, left_domain, right, right_domain,
                               &run->machine.lo, &run->machine.hi)) {
        run->machine.lo.known_mask = previous_lo_mask;
        run->machine.hi.known_mask = previous_hi_mask;
      }
    }
  }
  ++run->out->multiply_count;
  publish(run);
}
static int unknown_branch(Run *run, uint32_t pc) {
  stop(run, pc, 0);
  return NBA97_TEXT_UNKNOWN;
}

int nba97_game_rotation_matrix(Nba97GameRotationMatrixContext *context,
                               Nba97GameRotationMatrixProgress *out) {
  Run storage;
  Run *run = &storage;
  size_t i;
  size_t j;
  int branch = 0;
  int decided = 0;
  if (!out)
    return NBA97_TEXT_ARGUMENT;
  memset(out, 0, sizeof *out);
  if (!context || (!context->memory.region && context->memory.count) ||
      (!context->access_journal && context->access_journal_capacity) ||
      !machine_valid(&context->machine))
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < context->memory.count; ++i) {
    Nba97GameTextRegion *a = &context->memory.region[i];
    if (!a->data || !a->size || a->size > UINT64_C(0x100000000) ||
        (uint64_t)a->base + a->size > UINT64_C(0x100000000))
      return NBA97_TEXT_ARGUMENT;
    for (j = 0; j < i; ++j) {
      Nba97GameTextRegion *b = &context->memory.region[j];
      if ((uint64_t)a->base < (uint64_t)b->base + b->size &&
          (uint64_t)b->base < (uint64_t)a->base + a->size)
        return NBA97_TEXT_ARGUMENT;
    }
  }
  memset(run, 0, sizeof *run);
  run->context = context;
  run->out = out;
  run->machine = context->machine;
  publish(run);

  /* 0x80056080..0x800560E0: load X and select signed sine/cosine. */
  STEP(0x80056080);
  TRY(load(run, 15, 4, 0, 2, 1, UINT32_C(0x80056080)));
  out->angle_x = T7;
  STEP(0x80056084);
  V0 = A1;
  STEP(0x80056088);
  decided = nonnegative(T7, &branch);
  STEP(0x8005608c);
  T9 = and_constant(T7, 0xfff);
  if (!decided)
    return unknown_branch(run, UINT32_C(0x80056088));
  if (branch)
    goto x_positive;
  STEP(0x80056090);
  T7 = subtract_words(ZERO, T7);
  STEP(0x80056094);
  STEP(0x80056098);
  T7 = and_constant(T7, 0xfff);
  STEP(0x8005609c);
  T8 = shift_word(T7, 2, 0, 0);
  STEP(0x800560a0);
  set_known(&AT, UINT32_C(0x800b0000));
  STEP(0x800560a4);
  AT = add_words(AT, T8);
  STEP(0x800560a8);
  TRY(load(run, 25, 1, 0x3254, 4, 0, UINT32_C(0x800560a8)));
  STEP(0x800560ac);
  STEP(0x800560b0);
  T8 = shift_word(T9, 16, 0, 0);
  STEP(0x800560b4);
  T8 = shift_word(T8, 16, 1, 1);
  STEP(0x800560b8);
  T3 = subtract_words(ZERO, T8);
  STEP(0x800560bc);
  STEP(0x800560c0);
  T0 = shift_word(T9, 16, 1, 1);
  goto y_angle;
x_positive:
  STEP(0x800560c4);
  T8 = shift_word(T9, 2, 0, 0);
  STEP(0x800560c8);
  set_known(&AT, UINT32_C(0x800b0000));
  STEP(0x800560cc);
  AT = add_words(AT, T8);
  STEP(0x800560d0);
  TRY(load(run, 25, 1, 0x3254, 4, 0, UINT32_C(0x800560d0)));
  STEP(0x800560d4);
  STEP(0x800560d8);
  T8 = shift_word(T9, 16, 0, 0);
  STEP(0x800560dc);
  T3 = shift_word(T8, 16, 1, 1);
  STEP(0x800560e0);
  T0 = shift_word(T9, 16, 1, 1);

y_angle:
  /* 0x800560E4..0x80056148: repeat the signed lookup for Y. */
  STEP(0x800560e4);
  TRY(load(run, 15, 4, 2, 2, 1, UINT32_C(0x800560e4)));
  out->angle_y = T7;
  STEP(0x800560e8);
  STEP(0x800560ec);
  decided = nonnegative(T7, &branch);
  STEP(0x800560f0);
  T9 = and_constant(T7, 0xfff);
  if (!decided)
    return unknown_branch(run, UINT32_C(0x800560ec));
  if (branch)
    goto y_positive;
  STEP(0x800560f4);
  T7 = subtract_words(ZERO, T7);
  STEP(0x800560f8);
  STEP(0x800560fc);
  T7 = and_constant(T7, 0xfff);
  STEP(0x80056100);
  T8 = shift_word(T7, 2, 0, 0);
  STEP(0x80056104);
  set_known(&AT, UINT32_C(0x800b0000));
  STEP(0x80056108);
  AT = add_words(AT, T8);
  STEP(0x8005610c);
  TRY(load(run, 25, 1, 0x3254, 4, 0, UINT32_C(0x8005610c)));
  STEP(0x80056110);
  STEP(0x80056114);
  T4 = shift_word(T9, 16, 0, 0);
  STEP(0x80056118);
  T4 = shift_word(T4, 16, 1, 1);
  STEP(0x8005611c);
  T6 = subtract_words(ZERO, T4);
  STEP(0x80056120);
  STEP(0x80056124);
  T1 = shift_word(T9, 16, 1, 1);
  goto initial_products;
y_positive:
  STEP(0x80056128);
  T8 = shift_word(T9, 2, 0, 0);
  STEP(0x8005612c);
  set_known(&AT, UINT32_C(0x800b0000));
  STEP(0x80056130);
  AT = add_words(AT, T8);
  STEP(0x80056134);
  TRY(load(run, 25, 1, 0x3254, 4, 0, UINT32_C(0x80056134)));
  STEP(0x80056138);
  STEP(0x8005613c);
  T6 = shift_word(T9, 16, 0, 0);
  STEP(0x80056140);
  T6 = shift_word(T6, 16, 1, 1);
  STEP(0x80056144);
  T4 = subtract_words(ZERO, T6);
  STEP(0x80056148);
  T1 = shift_word(T9, 16, 1, 1);

initial_products:
  /* 0x8005614C..0x800561BC: start products, cache Z, and perform the first
   * three stores before Z's table read. */
  STEP(0x8005614c);
  multiply_unsigned(run, T1, T3, HALF_DOMAIN_SIGNED,
                    HALF_DOMAIN_SIGNED_OR_NEGATED_EXTREME);
  STEP(0x80056150);
  TRY(load(run, 15, 4, 4, 2, 1, UINT32_C(0x80056150)));
  out->angle_z = T7;
  STEP(0x80056154);
  TRY(store(run, 14, 5, 4, 2, UINT32_C(0x80056154)));
  STEP(0x80056158);
  T8 = run->machine.lo;
  STEP(0x8005615c);
  T9 = subtract_words(ZERO, T8);
  STEP(0x80056160);
  T6 = shift_word(T9, 12, 1, 1);
  STEP(0x80056164);
  multiply_unsigned(run, T1, T0, HALF_DOMAIN_SIGNED, HALF_DOMAIN_SIGNED);
  STEP(0x80056168);
  TRY(store(run, 14, 5, 0x0a, 2, UINT32_C(0x80056168)));
  STEP(0x8005616c);
  decided = nonnegative(T7, &branch);
  STEP(0x80056170);
  T9 = and_constant(T7, 0xfff);
  if (!decided)
    return unknown_branch(run, UINT32_C(0x8005616c));
  if (branch)
    goto z_positive;
  STEP(0x80056174);
  T8 = run->machine.lo;
  STEP(0x80056178);
  T6 = shift_word(T8, 12, 1, 1);
  STEP(0x8005617c);
  TRY(store(run, 14, 5, 0x10, 2, UINT32_C(0x8005617c)));
  STEP(0x80056180);
  T7 = subtract_words(ZERO, T7);
  STEP(0x80056184);
  STEP(0x80056188);
  T7 = and_constant(T7, 0xfff);
  STEP(0x8005618c);
  T8 = shift_word(T7, 2, 0, 0);
  STEP(0x80056190);
  set_known(&AT, UINT32_C(0x800b0000));
  STEP(0x80056194);
  AT = add_words(AT, T8);
  STEP(0x80056198);
  TRY(load(run, 25, 1, 0x3254, 4, 0, UINT32_C(0x80056198)));
  STEP(0x8005619c);
  STEP(0x800561a0);
  T8 = shift_word(T9, 16, 0, 0);
  STEP(0x800561a4);
  T8 = shift_word(T8, 16, 1, 1);
  STEP(0x800561a8);
  T5 = subtract_words(ZERO, T8);
  STEP(0x800561ac);
  STEP(0x800561b0);
  T2 = shift_word(T9, 16, 1, 1);
  goto remaining_products;
z_positive:
  STEP(0x800561b4);
  T7 = run->machine.lo;
  STEP(0x800561b8);
  T6 = shift_word(T7, 12, 1, 1);
  STEP(0x800561bc);
  TRY(store(run, 14, 5, 0x10, 2, UINT32_C(0x800561bc)));
  STEP(0x800561c0);
  T8 = shift_word(T9, 2, 0, 0);
  STEP(0x800561c4);
  set_known(&AT, UINT32_C(0x800b0000));
  STEP(0x800561c8);
  AT = add_words(AT, T8);
  STEP(0x800561cc);
  TRY(load(run, 25, 1, 0x3254, 4, 0, UINT32_C(0x800561cc)));
  STEP(0x800561d0);
  STEP(0x800561d4);
  T8 = shift_word(T9, 16, 0, 0);
  STEP(0x800561d8);
  T5 = shift_word(T8, 16, 1, 1);
  STEP(0x800561dc);
  T2 = shift_word(T9, 16, 1, 1);

remaining_products:
  /* 0x800561E0..0x800562C0: preserve each unsigned product, wrapping
   * add/subtract, negation-before-shift, and source halfword store order. */
  STEP(0x800561e0);
  multiply_unsigned(run, T2, T1, HALF_DOMAIN_SIGNED, HALF_DOMAIN_SIGNED);
  STEP(0x800561e4);
  T7 = run->machine.lo;
  STEP(0x800561e8);
  T6 = shift_word(T7, 12, 1, 1);
  STEP(0x800561ec);
  TRY(store(run, 14, 5, 0, 2, UINT32_C(0x800561ec)));
  STEP(0x800561f0);
  multiply_unsigned(run, T5, T1, HALF_DOMAIN_SIGNED_OR_NEGATED_EXTREME,
                    HALF_DOMAIN_SIGNED);
  STEP(0x800561f4);
  T7 = run->machine.lo;
  STEP(0x800561f8);
  T6 = subtract_words(ZERO, T7);
  STEP(0x800561fc);
  T7 = shift_word(T6, 12, 1, 1);
  STEP(0x80056200);
  multiply_unsigned(run, T2, T4, HALF_DOMAIN_SIGNED,
                    HALF_DOMAIN_SIGNED_OR_NEGATED_EXTREME);
  STEP(0x80056204);
  TRY(store(run, 15, 5, 2, 2, UINT32_C(0x80056204)));
  STEP(0x80056208);
  STEP(0x8005620c);
  T7 = run->machine.lo;
  STEP(0x80056210);
  T8 = shift_word(T7, 12, 1, 1);
  STEP(0x80056214);
  STEP(0x80056218);
  multiply_unsigned(run, T8, T3, HALF_DOMAIN_NONE, HALF_DOMAIN_NONE);
  STEP(0x8005621c);
  T7 = run->machine.lo;
  STEP(0x80056220);
  T6 = shift_word(T7, 12, 1, 1);
  STEP(0x80056224);
  STEP(0x80056228);
  multiply_unsigned(run, T5, T0, HALF_DOMAIN_SIGNED_OR_NEGATED_EXTREME,
                    HALF_DOMAIN_SIGNED);
  STEP(0x8005622c);
  T7 = run->machine.lo;
  STEP(0x80056230);
  T9 = shift_word(T7, 12, 1, 1);
  STEP(0x80056234);
  T7 = subtract_words(T9, T6);
  STEP(0x80056238);
  multiply_unsigned(run, T8, T0, HALF_DOMAIN_NONE, HALF_DOMAIN_NONE);
  STEP(0x8005623c);
  TRY(store(run, 15, 5, 6, 2, UINT32_C(0x8005623c)));
  STEP(0x80056240);
  STEP(0x80056244);
  T6 = run->machine.lo;
  STEP(0x80056248);
  T7 = shift_word(T6, 12, 1, 1);
  STEP(0x8005624c);
  STEP(0x80056250);
  multiply_unsigned(run, T5, T3, HALF_DOMAIN_SIGNED_OR_NEGATED_EXTREME,
                    HALF_DOMAIN_SIGNED_OR_NEGATED_EXTREME);
  STEP(0x80056254);
  T6 = run->machine.lo;
  STEP(0x80056258);
  T9 = shift_word(T6, 12, 1, 1);
  STEP(0x8005625c);
  T6 = add_words(T9, T7);
  STEP(0x80056260);
  multiply_unsigned(run, T5, T4, HALF_DOMAIN_SIGNED_OR_NEGATED_EXTREME,
                    HALF_DOMAIN_SIGNED_OR_NEGATED_EXTREME);
  STEP(0x80056264);
  TRY(store(run, 14, 5, 0x0c, 2, UINT32_C(0x80056264)));
  STEP(0x80056268);
  STEP(0x8005626c);
  T7 = run->machine.lo;
  STEP(0x80056270);
  T8 = shift_word(T7, 12, 1, 1);
  STEP(0x80056274);
  STEP(0x80056278);
  multiply_unsigned(run, T8, T3, HALF_DOMAIN_NONE, HALF_DOMAIN_NONE);
  STEP(0x8005627c);
  T7 = run->machine.lo;
  STEP(0x80056280);
  T6 = shift_word(T7, 12, 1, 1);
  STEP(0x80056284);
  STEP(0x80056288);
  multiply_unsigned(run, T2, T0, HALF_DOMAIN_SIGNED, HALF_DOMAIN_SIGNED);
  STEP(0x8005628c);
  T7 = run->machine.lo;
  STEP(0x80056290);
  T9 = shift_word(T7, 12, 1, 1);
  STEP(0x80056294);
  T7 = add_words(T9, T6);
  STEP(0x80056298);
  multiply_unsigned(run, T8, T0, HALF_DOMAIN_NONE, HALF_DOMAIN_NONE);
  STEP(0x8005629c);
  TRY(store(run, 15, 5, 8, 2, UINT32_C(0x8005629c)));
  STEP(0x800562a0);
  STEP(0x800562a4);
  T6 = run->machine.lo;
  STEP(0x800562a8);
  T7 = shift_word(T6, 12, 1, 1);
  STEP(0x800562ac);
  STEP(0x800562b0);
  multiply_unsigned(run, T2, T3, HALF_DOMAIN_SIGNED,
                    HALF_DOMAIN_SIGNED_OR_NEGATED_EXTREME);
  STEP(0x800562b4);
  T6 = run->machine.lo;
  STEP(0x800562b8);
  T9 = shift_word(T6, 12, 1, 1);
  STEP(0x800562bc);
  T6 = subtract_words(T9, T7);
  STEP(0x800562c0);
  TRY(store(run, 14, 5, 0x0e, 2, UINT32_C(0x800562c0)));
  STEP(0x800562c4);
  STEP(0x800562c8);
  if (RA.known_mask != 15) {
    stop(run, UINT32_C(0x800562c4), RA.word);
    return NBA97_TEXT_UNKNOWN;
  }
  out->completed = 1;
  stop(run, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
