#include "frontend_memory_copy.h"

#include <limits.h>
#include <string.h>

typedef Nba97FrontendMemoryCopyWord Word;
typedef struct Run {
  Nba97FrontendMemoryCopyContext *context;
  Nba97FrontendMemoryCopyProgress *out;
  Nba97FrontendMemoryCopyMachine machine;
} Run;

#define R(i) (run->machine.registers.gpr[(i)])
#define AT R(NBA97_FRONTEND_MEMORY_COPY_AT)
#define V0 R(NBA97_FRONTEND_MEMORY_COPY_V0)
#define A0 R(NBA97_FRONTEND_MEMORY_COPY_A0)
#define A1 R(NBA97_FRONTEND_MEMORY_COPY_A1)
#define A2 R(NBA97_FRONTEND_MEMORY_COPY_A2)
#define A3 R(NBA97_FRONTEND_MEMORY_COPY_A3)
#define T0 R(NBA97_FRONTEND_MEMORY_COPY_T0)
#define RA R(NBA97_FRONTEND_MEMORY_COPY_RA)
#define TRY(expression)                                                        \
  do {                                                                         \
    int result_ = (expression);                                                \
    if (result_ != NBA97_TEXT_COMPLETE)                                        \
      return result_;                                                          \
  } while (0)

static void publish(Run *run) {
  run->out->machine = run->machine;
  run->out->working_source = A0.word;
  run->out->working_destination = A1.word;
  run->out->working_count = A2.word;
  run->out->return_v0 = V0.word;
  run->out->return_v0_known_mask = V0.known_mask;
}

static void stop(Run *run, uint32_t pc, uint32_t address) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  publish(run);
}

static void step(Run *run, uint32_t pc) {
  size_t index = run->out->instruction_events++;
  if (index < run->context->instruction_journal_capacity)
    run->context->instruction_journal[index] = pc;
  ++run->out->instruction_count;
}

#define STEP(pc) step(run, (uint32_t)(pc))

static void set_known(Word *word, uint32_t value) {
  word->word = value;
  word->known_mask = 0x0fu;
}

static Word immediate(uint32_t value) {
  Word result;
  set_known(&result, value);
  return result;
}

static Word add_words(Word left, Word right) {
  Word result;
  unsigned carry_mask = 1u;
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
                      : 0u;
    unsigned le = (left.known_mask & (1u << byte)) ? ls : 255u;
    unsigned rs = (right.known_mask & (1u << byte))
                      ? ((right.word >> (byte * 8u)) & 255u)
                      : 0u;
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

static Word or_words(Word left, Word right) {
  Word result;
  unsigned byte;
  result.word = left.word | right.word;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    uint32_t shift = byte * 8u;
    uint32_t l = (left.word >> shift) & 255u;
    uint32_t r = (right.word >> shift) & 255u;
    if (((left.known_mask & (1u << byte)) && l == 255u) ||
        ((right.known_mask & (1u << byte)) && r == 255u) ||
        ((left.known_mask & right.known_mask) & (1u << byte)))
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static Word and_immediate(Word input, uint32_t value) {
  Word result;
  unsigned byte;
  result.word = input.word & value;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    uint32_t part = (value >> (byte * 8u)) & 255u;
    if (part == 0 || (input.known_mask & (1u << byte)))
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
    high |= ((value.known_mask & (1u << byte)) ? part : 255u)
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

static Word signed_less_word(Word left, Word right) {
  Word result;
  int64_t lmin;
  int64_t lmax;
  int64_t rmin;
  int64_t rmax;
  set_known(&result, signed_word(left.word) < signed_word(right.word));
  signed_bounds(left, &lmin, &lmax);
  signed_bounds(right, &rmin, &rmax);
  if (lmax < rmin || lmin >= rmax)
    return result;
  result.known_mask = 0x0eu;
  return result;
}

static int zero_decision(Run *run, Word value, uint32_t pc, int *is_zero) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((value.known_mask & (1u << byte)) &&
        ((value.word >> (byte * 8u)) & 255u)) {
      *is_zero = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (value.known_mask == 0x0fu) {
    *is_zero = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0);
  return NBA97_TEXT_UNKNOWN;
}

static int negative_decision(Run *run, Word value, uint32_t pc,
                             int *is_negative) {
  if (!(value.known_mask & 8u)) {
    stop(run, pc, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *is_negative = (value.word & UINT32_C(0x80000000)) != 0;
  return NBA97_TEXT_COMPLETE;
}

static int signed_add_instruction(Run *run, Word left, Word right,
                                  uint32_t pc, Word *destination) {
  uint32_t result;
  if (left.known_mask != 0x0fu || right.known_mask != 0x0fu) {
    stop(run, pc, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  result = left.word + right.word;
  if (((left.word ^ result) & (right.word ^ result) &
       UINT32_C(0x80000000)) != 0) {
    run->out->trapped = 1;
    stop(run, pc, 0);
    return NBA97_FRONTEND_MEMORY_COPY_ARITHMETIC_TRAP;
  }
  set_known(destination, result);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static int locate(Run *run, uint32_t address, unsigned width,
                  unsigned alignment, uint32_t pc, uint8_t **data,
                  uint8_t **known_bytes) {
  size_t i;
  unsigned byte;
  stop(run, pc, address);
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
      for (byte = 0; byte < width; ++byte)
        if ((*known_bytes)[byte] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    uint32_t logical, unsigned width, uint8_t transfer,
                    Word value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97FrontendMemoryCopyAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->logical_address = logical;
    event->value = value.word;
    event->operation = run->out->operations;
    event->width = (uint8_t)width;
    event->known_mask = value.known_mask;
    event->transfer_mask = transfer;
    event->kind = kind;
  }
}

static int checked_logical(Run *run, Word base, uint32_t offset,
                           uint32_t pc, uint32_t *logical) {
  Word result = add_words(base, immediate(offset));
  if (result.known_mask != 0x0fu) {
    stop(run, pc, result.word);
    return NBA97_TEXT_UNKNOWN;
  }
  *logical = result.word;
  return NBA97_TEXT_COMPLETE;
}

static int read_piece(Run *run, uint32_t pc, uint32_t address,
                      uint32_t logical, unsigned width, Word *value) {
  uint8_t *data;
  uint8_t *known_bytes;
  uint8_t transfer = 0;
  unsigned byte;
  TRY(locate(run, address, width, 1, pc, &data, &known_bytes));
  for (byte = 0; byte < width; ++byte) {
    uint32_t lane = (address + byte) - logical;
    if (lane >= 4u)
      return NBA97_TEXT_ARGUMENT;
    value->word = (value->word & ~(UINT32_C(255) << (lane * 8u))) |
                  ((uint32_t)data[byte] << (lane * 8u));
    value->known_mask = (uint8_t)(
        (value->known_mask & ~(1u << lane)) |
        ((!known_bytes || known_bytes[byte]) ? (1u << lane) : 0u));
    transfer = (uint8_t)(transfer | (1u << lane));
  }
  ++run->out->reads;
  run->out->bytes_read += width;
  journal(run, NBA97_FRONTEND_MEMORY_COPY_READ, pc, address, logical, width,
          transfer, *value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_piece(Run *run, uint32_t pc, uint32_t address,
                       uint32_t logical, unsigned width, Word value) {
  uint8_t *data;
  uint8_t *known_bytes;
  uint8_t transfer = 0;
  unsigned byte;
  TRY(locate(run, address, width, 1, pc, &data, &known_bytes));
  for (byte = 0; byte < width; ++byte) {
    uint32_t lane = (address + byte) - logical;
    if (lane >= 4u)
      return NBA97_TEXT_ARGUMENT;
    transfer = (uint8_t)(transfer | (1u << lane));
    if (!known_bytes && !(value.known_mask & (1u << lane)))
      return NBA97_TEXT_UNKNOWN;
  }
  for (byte = 0; byte < width; ++byte) {
    uint32_t lane = (address + byte) - logical;
    data[byte] = (uint8_t)(value.word >> (lane * 8u));
    if (known_bytes)
      known_bytes[byte] = (uint8_t)((value.known_mask >> lane) & 1u);
  }
  ++run->out->stores;
  run->out->bytes_stored += width;
  journal(run, NBA97_FRONTEND_MEMORY_COPY_STORE, pc, address, logical, width,
          transfer, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int load_word(Run *run, uint32_t pc, Word base, uint32_t offset,
                     Word *destination) {
  uint32_t address;
  uint8_t *data;
  uint8_t *known_bytes;
  Word value = {0, 0};
  unsigned byte;
  TRY(checked_logical(run, base, offset, pc, &address));
  TRY(locate(run, address, 4, 4, pc, &data, &known_bytes));
  for (byte = 0; byte < 4; ++byte) {
    value.word |= (uint32_t)data[byte] << (byte * 8u);
    if (!known_bytes || known_bytes[byte])
      value.known_mask = (uint8_t)(value.known_mask | (1u << byte));
  }
  *destination = value;
  ++run->out->reads;
  run->out->bytes_read += 4;
  journal(run, NBA97_FRONTEND_MEMORY_COPY_READ, pc, address, address, 4,
          0x0f, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int store_word(Run *run, uint32_t pc, Word base, uint32_t offset,
                      Word value) {
  uint32_t address;
  TRY(checked_logical(run, base, offset, pc, &address));
  return write_piece(run, pc, address, address, 4, value);
}

static int load_lwl(Run *run, uint32_t pc, uint32_t logical, Word *value) {
  uint32_t effective = logical + 3u;
  uint32_t address = effective & ~UINT32_C(3);
  unsigned width = (effective & 3u) + 1u;
  return read_piece(run, pc, address, logical, width, value);
}

static int load_lwr(Run *run, uint32_t pc, uint32_t logical, Word *value) {
  unsigned width = 4u - (logical & 3u);
  return read_piece(run, pc, logical, logical, width, value);
}

static int store_swl(Run *run, uint32_t pc, uint32_t logical, Word value) {
  uint32_t effective = logical + 3u;
  uint32_t address = effective & ~UINT32_C(3);
  unsigned width = (effective & 3u) + 1u;
  return write_piece(run, pc, address, logical, width, value);
}

static int store_swr(Run *run, uint32_t pc, uint32_t logical, Word value) {
  unsigned width = 4u - (logical & 3u);
  return write_piece(run, pc, logical, logical, width, value);
}

static int load_byte(Run *run, uint32_t pc, Word base, uint32_t offset,
                     Word *destination) {
  uint32_t address;
  uint8_t *data;
  uint8_t *known_bytes;
  Word value;
  TRY(checked_logical(run, base, offset, pc, &address));
  TRY(locate(run, address, 1, 1, pc, &data, &known_bytes));
  value.word = (data[0] & 0x80u) ? UINT32_C(0xffffff00) | data[0] : data[0];
  value.known_mask = (!known_bytes || known_bytes[0]) ? 0x0fu : 0;
  *destination = value;
  ++run->out->reads;
  ++run->out->bytes_read;
  journal(run, NBA97_FRONTEND_MEMORY_COPY_READ, pc, address, address, 1, 1,
          value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int store_byte(Run *run, uint32_t pc, Word base, uint32_t offset,
                      Word value) {
  uint32_t address;
  TRY(checked_logical(run, base, offset, pc, &address));
  return write_piece(run, pc, address, address, 1, value);
}

static int return_through_ra(Run *run, uint32_t jr_pc, uint32_t delay_pc) {
  STEP(jr_pc);
  STEP(delay_pc);
  if (RA.known_mask != 0x0fu) {
    stop(run, jr_pc, RA.word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    run->out->trapped = 1;
    stop(run, jr_pc, RA.word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  run->out->completed = 1;
  stop(run, 0, 0);
  return NBA97_TEXT_COMPLETE;
}

static int forward_words_and_bytes(Run *run, int aligned) {
  int negative;
  uint32_t logical;

  STEP(aligned ? 0x80090a98 : 0x80090b44);
  A2 = add_words(A2, immediate(0x0cu));
  publish(run);
  STEP(aligned ? 0x80090a9c : 0x80090b48);
  STEP(aligned ? 0x80090aa0 : 0x80090b4c);
  TRY(negative_decision(run, A2, aligned ? UINT32_C(0x80090a9c)
                                         : UINT32_C(0x80090b48),
                        &negative));
  while (!negative) {
    if (aligned) {
      STEP(0x80090aa4);
      TRY(load_word(run, UINT32_C(0x80090aa4), A0, 0, &T0));
      STEP(0x80090aa8);
      A2 = add_words(A2, immediate(UINT32_C(0xfffffffc)));
      publish(run);
      STEP(0x80090aac);
      TRY(store_word(run, UINT32_C(0x80090aac), A1, 0, T0));
      STEP(0x80090ab0);
      A0 = add_words(A0, immediate(4));
      publish(run);
      STEP(0x80090ab4);
      STEP(0x80090ab8);
      A1 = add_words(A1, immediate(4));
      publish(run);
      TRY(negative_decision(run, A2, UINT32_C(0x80090ab4), &negative));
    } else {
      STEP(0x80090b50);
      TRY(checked_logical(run, A0, 0, UINT32_C(0x80090b50), &logical));
      TRY(load_lwl(run, UINT32_C(0x80090b50), logical, &T0));
      STEP(0x80090b54);
      TRY(load_lwr(run, UINT32_C(0x80090b54), logical, &T0));
      STEP(0x80090b58);
      A2 = add_words(A2, immediate(UINT32_C(0xfffffffc)));
      publish(run);
      STEP(0x80090b5c);
      TRY(checked_logical(run, A1, 0, UINT32_C(0x80090b5c), &logical));
      TRY(store_swl(run, UINT32_C(0x80090b5c), logical, T0));
      STEP(0x80090b60);
      TRY(store_swr(run, UINT32_C(0x80090b60), logical, T0));
      STEP(0x80090b64);
      A0 = add_words(A0, immediate(4));
      publish(run);
      STEP(0x80090b68);
      STEP(0x80090b6c);
      A1 = add_words(A1, immediate(4));
      publish(run);
      TRY(negative_decision(run, A2, UINT32_C(0x80090b68), &negative));
    }
  }

  STEP(aligned ? 0x80090abc : 0x80090b70);
  A2 = add_words(A2, immediate(3));
  publish(run);
  STEP(aligned ? 0x80090ac0 : 0x80090b74);
  STEP(aligned ? 0x80090ac4 : 0x80090b78);
  TRY(negative_decision(run, A2, aligned ? UINT32_C(0x80090ac0)
                                         : UINT32_C(0x80090b74),
                        &negative));
  while (!negative) {
    STEP(aligned ? 0x80090ac8 : 0x80090b7c);
    TRY(load_byte(run, aligned ? UINT32_C(0x80090ac8)
                               : UINT32_C(0x80090b7c),
                  A0, 0, &T0));
    STEP(aligned ? 0x80090acc : 0x80090b80);
    A2 = add_words(A2, immediate(UINT32_C(0xffffffff)));
    publish(run);
    STEP(aligned ? 0x80090ad0 : 0x80090b84);
    TRY(store_byte(run, aligned ? UINT32_C(0x80090ad0)
                                : UINT32_C(0x80090b84),
                   A1, 0, T0));
    STEP(aligned ? 0x80090ad4 : 0x80090b88);
    A0 = add_words(A0, immediate(1));
    publish(run);
    STEP(aligned ? 0x80090ad8 : 0x80090b8c);
    STEP(aligned ? 0x80090adc : 0x80090b90);
    A1 = add_words(A1, immediate(1));
    publish(run);
    TRY(negative_decision(run, A2, aligned ? UINT32_C(0x80090ad8)
                                           : UINT32_C(0x80090b8c),
                          &negative));
  }
  return return_through_ra(run, aligned ? UINT32_C(0x80090ae0)
                                         : UINT32_C(0x80090b94),
                           aligned ? UINT32_C(0x80090ae4)
                                   : UINT32_C(0x80090b98));
}

static int forward_aligned(Run *run) {
  Word values[8];
  int negative;
  unsigned half;
  unsigned i;

  STEP(0x800909c0);
  A2 = add_words(A2, immediate(UINT32_C(0xffffffc0)));
  publish(run);
  STEP(0x800909c4);
  STEP(0x800909c8);
  TRY(negative_decision(run, A2, UINT32_C(0x800909c4), &negative));
  while (!negative) {
    /* 0x800909CC..0x80090A48 snapshots each eight-word half before writes. */
    for (half = 0; half < 2; ++half) {
      uint32_t offset = half * 0x20u;
      uint32_t read_pc = UINT32_C(0x800909cc) + half * 0x40u;
      uint32_t write_pc = UINT32_C(0x800909ec) + half * 0x40u;
      for (i = 0; i < 8; ++i) {
        STEP(read_pc + i * 4u);
        TRY(load_word(run, read_pc + i * 4u, A0, offset + i * 4u,
                      &values[i]));
        R(8u + i) = values[i];
      }
      for (i = 0; i < 8; ++i) {
        STEP(write_pc + i * 4u);
        TRY(store_word(run, write_pc + i * 4u, A1, offset + i * 4u,
                       R(8u + i)));
      }
    }
    STEP(0x80090a4c);
    A2 = add_words(A2, immediate(UINT32_C(0xffffffc0)));
    publish(run);
    STEP(0x80090a50);
    A0 = add_words(A0, immediate(0x40));
    publish(run);
    STEP(0x80090a54);
    STEP(0x80090a58);
    A1 = add_words(A1, immediate(0x40));
    publish(run);
    TRY(negative_decision(run, A2, UINT32_C(0x80090a54), &negative));
  }

  STEP(0x80090a5c);
  A2 = add_words(A2, immediate(0x30));
  publish(run);
  STEP(0x80090a60);
  STEP(0x80090a64);
  TRY(negative_decision(run, A2, UINT32_C(0x80090a60), &negative));
  while (!negative) {
    for (i = 0; i < 4; ++i) {
      uint32_t pc = UINT32_C(0x80090a68) + i * 4u;
      STEP(pc);
      TRY(load_word(run, pc, A0, i * 4u, &R(8u + i)));
    }
    for (i = 0; i < 4; ++i) {
      uint32_t pc = UINT32_C(0x80090a78) + i * 4u;
      STEP(pc);
      TRY(store_word(run, pc, A1, i * 4u, R(8u + i)));
    }
    STEP(0x80090a88);
    A2 = add_words(A2, immediate(UINT32_C(0xfffffff0)));
    publish(run);
    STEP(0x80090a8c);
    A0 = add_words(A0, immediate(0x10));
    publish(run);
    STEP(0x80090a90);
    STEP(0x80090a94);
    A1 = add_words(A1, immediate(0x10));
    publish(run);
    TRY(negative_decision(run, A2, UINT32_C(0x80090a90), &negative));
  }
  return forward_words_and_bytes(run, 1);
}

static int forward_unaligned(Run *run) {
  int negative;
  unsigned i;
  uint32_t logical;

  STEP(0x80090ae8);
  A2 = add_words(A2, immediate(UINT32_C(0xfffffff0)));
  publish(run);
  STEP(0x80090aec);
  STEP(0x80090af0);
  TRY(negative_decision(run, A2, UINT32_C(0x80090aec), &negative));
  while (!negative) {
    for (i = 0; i < 4; ++i) {
      uint32_t lwl_pc = UINT32_C(0x80090af4) + i * 8u;
      STEP(lwl_pc);
      TRY(checked_logical(run, A0, i * 4u, lwl_pc, &logical));
      TRY(load_lwl(run, lwl_pc, logical, &R(8u + i)));
      STEP(lwl_pc + 4u);
      TRY(load_lwr(run, lwl_pc + 4u, logical, &R(8u + i)));
    }
    for (i = 0; i < 4; ++i) {
      uint32_t swl_pc = UINT32_C(0x80090b14) + i * 8u;
      STEP(swl_pc);
      TRY(checked_logical(run, A1, i * 4u, swl_pc, &logical));
      TRY(store_swl(run, swl_pc, logical, R(8u + i)));
      STEP(swl_pc + 4u);
      TRY(store_swr(run, swl_pc + 4u, logical, R(8u + i)));
    }
    STEP(0x80090b34);
    A2 = add_words(A2, immediate(UINT32_C(0xfffffff0)));
    publish(run);
    STEP(0x80090b38);
    A0 = add_words(A0, immediate(0x10));
    publish(run);
    STEP(0x80090b3c);
    STEP(0x80090b40);
    A1 = add_words(A1, immediate(0x10));
    publish(run);
    TRY(negative_decision(run, A2, UINT32_C(0x80090b3c), &negative));
  }
  return forward_words_and_bytes(run, 0);
}

static int backward_words_and_bytes(Run *run) {
  int negative;

  STEP(0x80090c9c);
  A2 = add_words(A2, immediate(3));
  publish(run);
  STEP(0x80090ca0);
  STEP(0x80090ca4);
  TRY(negative_decision(run, A2, UINT32_C(0x80090ca0), &negative));
  while (!negative) {
    STEP(0x80090ca8);
    TRY(load_byte(run, UINT32_C(0x80090ca8), A0, UINT32_C(0xffffffff),
                  &T0));
    STEP(0x80090cac);
    A2 = add_words(A2, immediate(UINT32_C(0xffffffff)));
    publish(run);
    STEP(0x80090cb0);
    TRY(store_byte(run, UINT32_C(0x80090cb0), A1,
                   UINT32_C(0xffffffff), T0));
    STEP(0x80090cb4);
    A0 = add_words(A0, immediate(UINT32_C(0xffffffff)));
    publish(run);
    STEP(0x80090cb8);
    STEP(0x80090cbc);
    A1 = add_words(A1, immediate(UINT32_C(0xffffffff)));
    publish(run);
    TRY(negative_decision(run, A2, UINT32_C(0x80090cb8), &negative));
  }
  return return_through_ra(run, UINT32_C(0x80090cc0),
                           UINT32_C(0x80090cc4));
}

static int backward_word_loop(Run *run) {
  int negative = 0;
  uint32_t logical;
  while (!negative) {
    STEP(0x80090c7c);
    TRY(checked_logical(run, A0, UINT32_C(0xfffffffc),
                        UINT32_C(0x80090c7c), &logical));
    TRY(load_lwl(run, UINT32_C(0x80090c7c), logical, &T0));
    STEP(0x80090c80);
    TRY(load_lwr(run, UINT32_C(0x80090c80), logical, &T0));
    STEP(0x80090c84);
    A2 = add_words(A2, immediate(UINT32_C(0xfffffffc)));
    publish(run);
    STEP(0x80090c88);
    TRY(checked_logical(run, A1, UINT32_C(0xfffffffc),
                        UINT32_C(0x80090c88), &logical));
    TRY(store_swl(run, UINT32_C(0x80090c88), logical, T0));
    STEP(0x80090c8c);
    TRY(store_swr(run, UINT32_C(0x80090c8c), logical, T0));
    STEP(0x80090c90);
    A0 = add_words(A0, immediate(UINT32_C(0xfffffffc)));
    publish(run);
    STEP(0x80090c94);
    STEP(0x80090c98);
    A1 = add_words(A1, immediate(UINT32_C(0xfffffffc)));
    publish(run);
    TRY(negative_decision(run, A2, UINT32_C(0x80090c94), &negative));
  }
  return backward_words_and_bytes(run);
}

static int backward_tail_words(Run *run, uint32_t add_pc,
                               uint32_t branch_pc, uint32_t delay_pc) {
  int negative;
  STEP(add_pc);
  A2 = add_words(A2, immediate(0x0cu));
  publish(run);
  STEP(branch_pc);
  STEP(delay_pc);
  TRY(negative_decision(run, A2, branch_pc, &negative));
  return negative ? backward_words_and_bytes(run) : backward_word_loop(run);
}

static int backward_aligned(Run *run) {
  int negative;
  unsigned i;
  STEP(0x80090bc4);
  A2 = add_words(A2, immediate(UINT32_C(0xfffffff0)));
  publish(run);
  STEP(0x80090bc8);
  STEP(0x80090bcc);
  TRY(negative_decision(run, A2, UINT32_C(0x80090bc8), &negative));
  while (!negative) {
    for (i = 0; i < 4; ++i) {
      uint32_t pc = UINT32_C(0x80090bd0) + i * 4u;
      STEP(pc);
      TRY(load_word(run, pc, A0, UINT32_C(0xfffffff0) + i * 4u,
                    &R(8u + i)));
    }
    for (i = 0; i < 4; ++i) {
      uint32_t pc = UINT32_C(0x80090be0) + i * 4u;
      STEP(pc);
      TRY(store_word(run, pc, A1, UINT32_C(0xfffffff0) + i * 4u,
                     R(8u + i)));
    }
    STEP(0x80090bf0);
    A0 = add_words(A0, immediate(UINT32_C(0xfffffff0)));
    publish(run);
    STEP(0x80090bf4);
    A2 = add_words(A2, immediate(UINT32_C(0xfffffff0)));
    publish(run);
    STEP(0x80090bf8);
    STEP(0x80090bfc);
    A1 = add_words(A1, immediate(UINT32_C(0xfffffff0)));
    publish(run);
    TRY(negative_decision(run, A2, UINT32_C(0x80090bf8), &negative));
  }
  STEP(0x80090c00);
  A2 = add_words(A2, immediate(0x0cu));
  publish(run);
  STEP(0x80090c04);
  STEP(0x80090c08);
  TRY(negative_decision(run, A2, UINT32_C(0x80090c04), &negative));
  if (!negative) {
    STEP(0x80090c0c);
    STEP(0x80090c10);
    return backward_word_loop(run);
  }
  return backward_words_and_bytes(run);
}

static int backward_unaligned(Run *run) {
  int negative;
  unsigned i;
  uint32_t logical;
  STEP(0x80090c14);
  A2 = add_words(A2, immediate(UINT32_C(0xfffffff0)));
  publish(run);
  STEP(0x80090c18);
  STEP(0x80090c1c);
  TRY(negative_decision(run, A2, UINT32_C(0x80090c18), &negative));
  while (!negative) {
    for (i = 0; i < 4; ++i) {
      uint32_t pc = UINT32_C(0x80090c20) + i * 8u;
      STEP(pc);
      TRY(checked_logical(run, A0, UINT32_C(0xfffffff0) + i * 4u, pc,
                          &logical));
      TRY(load_lwl(run, pc, logical, &R(8u + i)));
      STEP(pc + 4u);
      TRY(load_lwr(run, pc + 4u, logical, &R(8u + i)));
    }
    for (i = 0; i < 4; ++i) {
      uint32_t pc = UINT32_C(0x80090c40) + i * 8u;
      STEP(pc);
      TRY(checked_logical(run, A1, UINT32_C(0xfffffff0) + i * 4u, pc,
                          &logical));
      TRY(store_swl(run, pc, logical, R(8u + i)));
      STEP(pc + 4u);
      TRY(store_swr(run, pc + 4u, logical, R(8u + i)));
    }
    STEP(0x80090c60);
    A2 = add_words(A2, immediate(UINT32_C(0xfffffff0)));
    publish(run);
    STEP(0x80090c64);
    A0 = add_words(A0, immediate(UINT32_C(0xfffffff0)));
    publish(run);
    STEP(0x80090c68);
    STEP(0x80090c6c);
    A1 = add_words(A1, immediate(UINT32_C(0xfffffff0)));
    publish(run);
    TRY(negative_decision(run, A2, UINT32_C(0x80090c68), &negative));
  }
  return backward_tail_words(run, UINT32_C(0x80090c70),
                             UINT32_C(0x80090c74),
                             UINT32_C(0x80090c78));
}

static int initialize(Nba97FrontendMemoryCopyContext *context,
                      Nba97FrontendMemoryCopyProgress *out, Run *run) {
  size_t i;
  size_t j;
  unsigned reg;
  if (!out)
    return NBA97_TEXT_ARGUMENT;
  memset(out, 0, sizeof *out);
  if (!context || (!context->memory.region && context->memory.count) ||
      (!context->access_journal && context->access_journal_capacity) ||
      (!context->instruction_journal && context->instruction_journal_capacity))
    return NBA97_TEXT_ARGUMENT;
  if (context->machine.registers.gpr[0].word != 0 ||
      context->machine.registers.gpr[0].known_mask != 0x0fu ||
      context->machine.hi.known_mask > 0x0fu ||
      context->machine.lo.known_mask > 0x0fu)
    return NBA97_TEXT_ARGUMENT;
  for (reg = 0; reg < NBA97_FRONTEND_MEMORY_COPY_REGISTER_COUNT; ++reg)
    if (context->machine.registers.gpr[reg].known_mask > 0x0fu)
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
  out->source = A0.word;
  out->destination = A1.word;
  out->requested_length = A2.word;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

int nba97_frontend_memory_copy(Nba97FrontendMemoryCopyContext *context,
                               Nba97FrontendMemoryCopyProgress *out) {
  Run storage;
  Run *run = &storage;
  int is_zero;
  TRY(initialize(context, out, run));

  /* 0x800909A8..0x800909B0: the OR is the initial branch delay and runs on
   * both paths, including before a possible endpoint ADD trap. */
  STEP(0x800909a8);
  AT = signed_less_word(A0, A1);
  publish(run);
  STEP(0x800909ac);
  STEP(0x800909b0);
  V0 = or_words(A0, A1);
  publish(run);
  TRY(zero_decision(run, AT, UINT32_C(0x800909ac), &is_zero));
  if (!is_zero) {
    STEP(0x80090b9c);
    TRY(signed_add_instruction(run, A0, A2, UINT32_C(0x80090b9c), &A3));
    STEP(0x80090ba0);
    AT = signed_less_word(A1, A3);
    publish(run);
    STEP(0x80090ba4);
    STEP(0x80090ba8);
    TRY(zero_decision(run, AT, UINT32_C(0x80090ba4), &is_zero));
    if (!is_zero) {
      STEP(0x80090bac);
      TRY(signed_add_instruction(run, A0, A2, UINT32_C(0x80090bac), &A0));
      STEP(0x80090bb0);
      TRY(signed_add_instruction(run, A1, A2, UINT32_C(0x80090bb0), &A1));
      STEP(0x80090bb4);
      V0 = or_words(A0, A1);
      publish(run);
      STEP(0x80090bb8);
      V0 = and_immediate(V0, 3);
      publish(run);
      out->backward = 1;
      out->unaligned = V0.word != 0;
      STEP(0x80090bbc);
      STEP(0x80090bc0);
      TRY(zero_decision(run, V0, UINT32_C(0x80090bbc), &is_zero));
      return is_zero ? backward_aligned(run) : backward_unaligned(run);
    }
    /* The BEQ target is 0x800909B0, so the initial delay-slot OR executes a
     * second time when source<destination but the ranges do not overlap. */
    STEP(0x800909b0);
    V0 = or_words(A0, A1);
    publish(run);
  }

  STEP(0x800909b4);
  V0 = and_immediate(V0, 3);
  publish(run);
  out->unaligned = V0.word != 0;
  STEP(0x800909b8);
  STEP(0x800909bc);
  TRY(zero_decision(run, V0, UINT32_C(0x800909b8), &is_zero));
  return is_zero ? forward_aligned(run) : forward_unaligned(run);
}
