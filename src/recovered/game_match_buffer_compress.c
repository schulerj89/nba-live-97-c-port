#include "game_match_buffer_compress.h"

#include <string.h>

typedef Nba97GameMatchBufferCompressWord Word;

typedef struct Run {
  Nba97GameMatchBufferCompressContext *context;
  Nba97GameMatchBufferCompressProgress *out;
  Nba97GameMatchBufferCompressMachine machine;
} Run;

#define R(i) (run->machine.registers.gpr[(i)])
#define ZERO R(0)
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

static void stop(Run *run, uint32_t pc, uint32_t address) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  publish(run);
}

static int machine_valid(const Nba97GameMatchBufferCompressMachine *machine) {
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

static int initialize(Nba97GameMatchBufferCompressContext *context,
                      Nba97GameMatchBufferCompressProgress *out, Run *run) {
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

/* Enumerating byte carries and borrows retains each invariant result byte. */
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
    unsigned left_start = (left.known_mask & (1u << byte))
                              ? ((left.word >> (byte * 8u)) & 255u)
                              : 0;
    unsigned left_end = (left.known_mask & (1u << byte)) ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte))
                               ? ((right.word >> (byte * 8u)) & 255u)
                               : 0;
    unsigned right_end = (right.known_mask & (1u << byte)) ? right_start : 255u;
    unsigned borrow;
    for (borrow = 0; borrow < 2; ++borrow) {
      unsigned a;
      if (!(borrow_mask & (1u << borrow)))
        continue;
      for (a = left_start; a <= left_end; ++a) {
        unsigned b;
        for (b = right_start; b <= right_end; ++b) {
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

static Word shift(Word value, unsigned amount, int right) {
  Word result;
  uint32_t known_bits = 0;
  uint32_t shifted_known;
  unsigned byte;
  result.word = right ? value.word >> amount : value.word << amount;
  for (byte = 0; byte < 4; ++byte)
    if (value.known_mask & (1u << byte))
      known_bits |= UINT32_C(255) << (byte * 8u);
  if (right)
    shifted_known = (known_bits >> amount) | (UINT32_MAX << (32u - amount));
  else
    shifted_known = (known_bits << amount) | ((UINT32_C(1) << amount) - 1u);
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte)
    if (((shifted_known >> (byte * 8u)) & 255u) == 255u)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  return result;
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
    uint32_t mask = (constant >> (byte * 8u)) & 255u;
    if ((value.known_mask & (1u << byte)) || mask == 255u)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static Word xor_constant(Word value, uint32_t constant) {
  value.word ^= constant;
  return value;
}

/* The SLTIU at 0x80076848 classifies the preceding modular halfword
 * expression. Retaining that expression proves out-of-range high bytes even
 * when the low byte leaves the ADDIU carry unknown. */
static Word signed_byte_difference(Word difference) {
  Word result;
  unsigned high = (difference.word >> 8) & 255u;
  known(&result, (uint16_t)(difference.word + 0x80u) < 0x100u);
  if (!(difference.known_mask & 2u)) {
    result.known_mask = 14;
    return result;
  }
  if (high != 0 && high != 255)
    return result;
  if (!(difference.known_mask & 1u)) {
    result.known_mask = 14;
    return result;
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
    Nba97GameMatchBufferCompressAccess *event =
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
  journal(run, NBA97_GAME_MATCH_BUFFER_COMPRESS_READ, pc, address, width,
          loaded);
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
  journal(run, NBA97_GAME_MATCH_BUFFER_COMPRESS_STORE, pc, address, width,
          value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int address(Run *run, Word base, uint32_t offset, uint32_t pc,
                   uint32_t *effective) {
  Word value = add(base, immediate(offset));
  if (value.known_mask != 15) {
    stop(run, pc, value.word);
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
  stop(run, pc, 0);
  return NBA97_TEXT_UNKNOWN;
}

int nba97_game_match_buffer_compress(
    Nba97GameMatchBufferCompressContext *context,
    Nba97GameMatchBufferCompressProgress *out) {
  Run storage;
  Run *run = &storage;
  Word branch_value;
  int condition;
  TRY(initialize(context, out, run));

  /* 0x800767FC..0x8007681C: reserve the low-16-bit flag area, retain its
   * first slot and record start, then initialize the group state. */
  STEP(0x800767fc);
  V0 = add(A3, immediate(7));
  STEP(0x80076800);
  T1 = shift(V0, 2, 1);
  STEP(0x80076804);
  T3 = add(A2, immediate(1));
  STEP(0x80076808);
  T4 = A2;
  STEP(0x8007680c);
  V0 = and_constant(T1, 0xffff);
  STEP(0x80076810);
  A2 = add(A2, V0);
  STEP(0x80076814);
  T1 = A3;

new_group:
  STEP(0x80076818);
  known(&A3, 0);
  STEP(0x8007681c);
  known(&T0, 4);

element:
  ++out->element_iterations;
  /* 0x80076820..0x8007683C: read a0 then a1 halfwords, advance both source
   * pointers, and test the modular low-halfword difference after its delay. */
  STEP(0x80076820);
  TRY(load(run, 3, 4, 0, 2, 2, 0x80076820));
  STEP(0x80076824);
  TRY(load(run, 2, 5, 0, 2, 2, 0x80076824));
  STEP(0x80076828);
  A1 = add(A1, immediate(2));
  STEP(0x8007682c);
  A0 = add(A0, immediate(2));
  STEP(0x80076830);
  V0 = subtract(V1, V0);
  STEP(0x80076834);
  V1 = shift(V0, 16, 0);
  STEP(0x80076838);
  branch_value = V1;
  STEP(0x8007683c);
  T2 = V0;
  TRY(zero_decision(run, branch_value, 0x80076838, &condition));
  if (condition) {
    /* Equal values consume code 0 and no data bytes. */
    STEP(0x80076860);
    STEP(0x80076864);
    A3 = shift(A3, 2, 0);
  } else {
    /* 0x80076840..0x80076850: classify the low-16-bit delta as signed byte
     * or full halfword. The flag shift is the BEQ delay slot. */
    STEP(0x80076840);
    V0 = add(V0, immediate(0x80));
    STEP(0x80076844);
    V0 = and_constant(V0, 0xffff);
    STEP(0x80076848);
    branch_value = signed_byte_difference(T2);
    V0 = branch_value;
    STEP(0x8007684c);
    STEP(0x80076850);
    V0 = shift(A3, 2, 0);
    TRY(zero_decision(run, branch_value, 0x8007684c, &condition));
    if (!condition) {
      /* Code 1 writes only the low difference byte in the jump delay slot. */
      STEP(0x80076854);
      A3 = or_constant(V0, 1);
      STEP(0x80076858);
      STEP(0x8007685c);
      TRY(store(run, 10, 6, 0, 1, 1, 0x8007685c));
    } else {
      /* Code 3 writes low then high modular difference bytes. */
      STEP(0x80076868);
      A3 = or_constant(V0, 3);
      STEP(0x8007686c);
      TRY(store(run, 10, 6, 0, 1, 1, 0x8007686c));
      STEP(0x80076870);
      A2 = add(A2, immediate(1));
      STEP(0x80076874);
      V0 = shift(V1, 24, 1);
      STEP(0x80076878);
      TRY(store(run, 2, 6, 0, 1, 1, 0x80076878));
    }
    STEP(0x8007687c);
    A2 = add(A2, immediate(1));
  }

  /* 0x80076880..0x800768A8: low-16 count zero ends immediately. Otherwise
   * either continue this group or store its flags before resetting to four. */
  STEP(0x80076880);
  T1 = add(T1, immediate(UINT32_MAX));
  STEP(0x80076884);
  V0 = and_constant(T1, 0xffff);
  STEP(0x80076888);
  branch_value = V0;
  STEP(0x8007688c);
  V0 = add(T0, immediate(UINT32_MAX));
  TRY(zero_decision(run, branch_value, 0x80076888, &condition));
  if (!condition) {
    STEP(0x80076890);
    T0 = V0;
    STEP(0x80076894);
    V0 = and_constant(V0, 0xffff);
    STEP(0x80076898);
    STEP(0x8007689c);
    TRY(zero_decision(run, V0, 0x80076898, &condition));
    if (!condition)
      goto element;
    STEP(0x800768a0);
    TRY(store(run, 7, 11, 0, 1, 1, 0x800768a0));
    ++out->completed_flag_groups;
    STEP(0x800768a4);
    STEP(0x800768a8);
    T3 = add(T3, immediate(1));
    goto new_group;
  }

  /* 0x800768AC..0x800768D4: align the partial group's flags, publish the
   * total low byte at both ends, and retain every output alias in order. */
  STEP(0x800768ac);
  T0 = shift(V0, 1, 0);
  STEP(0x800768b0);
  V1 = and_constant(A3, 0xffff);
  STEP(0x800768b4);
  V0 = and_constant(T0, 0xffff);
  STEP(0x800768b8);
  if (V0.known_mask == 15)
    V1 = shift(V1, V0.word & 31u, 0);
  else {
    V1.word <<= V0.word & 31u;
    V1.known_mask = 0;
  }
  STEP(0x800768bc);
  V0 = subtract(A2, T4);
  STEP(0x800768c0);
  V0 = add(V0, immediate(1));
  STEP(0x800768c4);
  TRY(store(run, 3, 11, 0, 1, 1, 0x800768c4));
  STEP(0x800768c8);
  known(&V1, UINT32_C(0x80100000));
  STEP(0x800768cc);
  V1 = add(V1, immediate(UINT32_C(0xffff9ffc)));
  STEP(0x800768d0);
  TRY(store(run, 2, 12, 0, 1, 1, 0x800768d0));
  STEP(0x800768d4);
  TRY(store(run, 2, 6, 0, 1, 1, 0x800768d4));

  /* 0x800768D8..0x800768EC: toggle the full retained halfword, then assign
   * the returned byte pointer in JR's delay slot before validating ra. */
  STEP(0x800768d8);
  TRY(load(run, 2, 3, 0, 2, 2, 0x800768d8));
  STEP(0x800768dc);
  STEP(0x800768e0);
  V0 = xor_constant(V0, 1);
  STEP(0x800768e4);
  TRY(store(run, 2, 3, 0, 2, 2, 0x800768e4));
  STEP(0x800768e8);
  STEP(0x800768ec);
  V0 = add(A2, immediate(1));
  if (RA.known_mask != 15) {
    stop(run, 0x800768e8, RA.word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, 0x800768e8, RA.word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
