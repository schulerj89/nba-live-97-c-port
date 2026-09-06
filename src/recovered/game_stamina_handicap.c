#include "game_stamina_handicap.h"

#include <string.h>

#define FEATURE_HANDICAP UINT32_C(0x80021d81)
#define FEATURE_STAMINA UINT32_C(0x80021d93)
#define CLOCK UINT32_C(0x800fdb58)
#define HANDICAP UINT32_C(0x800fdb98)
#define PHASE UINT32_C(0x800fe8cc)
#define DELTA UINT32_C(0x800fdb7e)
#define SCORE_HOME UINT32_C(0x8001ee22)
#define SCORE_AWAY UINT32_C(0x8001eee6)
#define SCORE_TABLE UINT32_C(0x8001f80c)
#define ACTOR_TABLE UINT32_C(0x80020bec)

enum {
  REG_T0 = NBA97_MATCH_INITIALIZE_T0,
  REG_T1 = NBA97_MATCH_INITIALIZE_T0 + 1,
  REG_T2 = NBA97_MATCH_INITIALIZE_T0 + 2
};

typedef struct Run {
  Nba97GameStaminaHandicapContext *context;
  Nba97GameStaminaHandicapProgress *out;
  Nba97GameStaminaHandicapMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int nba97_status_ = (expression);                                          \
    if (nba97_status_ != NBA97_TEXT_COMPLETE)                                  \
      return nba97_status_;                                                    \
  } while (0)

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  publish(run);
}

static void set_known(Nba97GameStaminaHandicapWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameStaminaHandicapMachine *machine) {
  unsigned index;
  if (machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0u ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (index = 0u; index != NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++index)
    if (machine->registers.gpr[index].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int memory_valid(const Nba97GameTextMemory *memory) {
  size_t index;
  size_t earlier;
  if (memory->count != 0u && memory->region == NULL)
    return 0;
  for (index = 0u; index != memory->count; ++index) {
    const Nba97GameTextRegion *region = &memory->region[index];
    if (region->data == NULL || region->size == 0u ||
        (uint64_t)region->size > UINT64_C(0x100000000) ||
        (uint64_t)region->base + region->size > UINT64_C(0x100000000))
      return 0;
    for (earlier = 0u; earlier != index; ++earlier) {
      const Nba97GameTextRegion *other = &memory->region[earlier];
      if ((uint64_t)region->base < (uint64_t)other->base + other->size &&
          (uint64_t)other->base < (uint64_t)region->base + region->size)
        return 0;
    }
  }
  return 1;
}

static int initialize(Nba97GameStaminaHandicapContext *context,
                      Nba97GameStaminaHandicapProgress *out, Run *run) {
  if (out == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(out, 0, sizeof(*out));
  if (context == NULL || !memory_valid(&context->memory) ||
      !machine_valid(&context->machine) ||
      (context->access_journal_capacity != 0u &&
       context->access_journal == NULL))
    return NBA97_TEXT_ARGUMENT;
  run->context = context;
  run->out = out;
  run->machine = context->machine;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static uint32_t width_mask(unsigned width) {
  return width == 4u ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - 1u;
}

static uint8_t known_width_mask(unsigned width) {
  return (uint8_t)((1u << width) - 1u);
}

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width, const Nba97GameStaminaHandicapWord *value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameStaminaHandicapAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word & width_mask(width);
    event->operation = run->out->operations;
    event->width = (uint8_t)width;
    event->known_mask = (uint8_t)(value->known_mask & known_width_mask(width));
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t address, unsigned width, uint32_t pc,
                  uint8_t **data, uint8_t **known) {
  size_t index;
  stop(run, pc, address);
  TRY(spend(run));
  ++run->out->accesses;
  if ((address & (width - 1u)) != 0u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (index = 0u; index != run->context->memory.count; ++index) {
    Nba97GameTextRegion *region = &run->context->memory.region[index];
    uint64_t offset = (uint64_t)address - region->base;
    unsigned byte;
    if (address < region->base || offset > region->size ||
        width > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known = region->known != NULL ? region->known + (size_t)offset : NULL;
    if (*known != NULL)
      for (byte = 0u; byte != width; ++byte)
        if ((*known)[byte] > 1u)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int read_value(Run *run, uint32_t address, unsigned width, uint32_t pc,
                      Nba97GameStaminaHandicapWord *destination) {
  uint8_t *data;
  uint8_t *known;
  Nba97GameStaminaHandicapWord loaded;
  unsigned byte;
  TRY(locate(run, address, width, pc, &data, &known));
  loaded.word = 0u;
  loaded.known_mask = 0u;
  for (byte = 0u; byte != width; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (8u * byte);
    if (known == NULL || known[byte] != 0u)
      loaded.known_mask = (uint8_t)(loaded.known_mask | (uint8_t)(1u << byte));
  }
  *destination = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_STAMINA_HANDICAP_READ, pc, address, width, &loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, uint32_t address, unsigned width, uint32_t pc,
                       Nba97GameStaminaHandicapWord value) {
  uint8_t *data;
  uint8_t *known;
  unsigned byte;
  value.word &= width_mask(width);
  value.known_mask = (uint8_t)(value.known_mask & known_width_mask(width));
  TRY(locate(run, address, width, pc, &data, &known));
  if (known == NULL && value.known_mask != known_width_mask(width))
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0u; byte != width; ++byte) {
    data[byte] = (uint8_t)(value.word >> (8u * byte));
    if (known != NULL)
      known[byte] = (uint8_t)((value.known_mask >> byte) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_STAMINA_HANDICAP_STORE, pc, address, width, &value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static Nba97GameStaminaHandicapWord
add_words(Nba97GameStaminaHandicapWord left,
          Nba97GameStaminaHandicapWord right) {
  Nba97GameStaminaHandicapWord result;
  unsigned carry_mask = 1u;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned next_carry_mask = 0u;
    unsigned first_output = 0u;
    unsigned first = 1u;
    unsigned invariant = 1u;
    unsigned left_start = (left.known_mask & (1u << byte)) != 0u
                              ? (left.word >> (8u * byte)) & 255u
                              : 0u;
    unsigned left_end =
        (left.known_mask & (1u << byte)) != 0u ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte)) != 0u
                               ? (right.word >> (8u * byte)) & 255u
                               : 0u;
    unsigned right_end =
        (right.known_mask & (1u << byte)) != 0u ? right_start : 255u;
    unsigned carry;
    for (carry = 0u; carry != 2u; ++carry) {
      unsigned a;
      if ((carry_mask & (1u << carry)) == 0u)
        continue;
      for (a = left_start; a <= left_end; ++a) {
        unsigned b;
        for (b = right_start; b <= right_end; ++b) {
          unsigned sum = a + b + carry;
          unsigned output = sum & 255u;
          next_carry_mask |= 1u << (sum >> 8u);
          if (first != 0u) {
            first_output = output;
            first = 0u;
          } else if (first_output != output) {
            invariant = 0u;
          }
        }
      }
    }
    if (invariant != 0u)
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
    carry_mask = next_carry_mask;
  }
  return result;
}

static Nba97GameStaminaHandicapWord
subtract_words(Nba97GameStaminaHandicapWord left,
               Nba97GameStaminaHandicapWord right) {
  Nba97GameStaminaHandicapWord result;
  unsigned borrow_mask = 1u;
  unsigned byte;
  result.word = left.word - right.word;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned next_borrow_mask = 0u;
    unsigned first_output = 0u;
    unsigned first = 1u;
    unsigned invariant = 1u;
    unsigned left_start = (left.known_mask & (1u << byte)) != 0u
                              ? (left.word >> (8u * byte)) & 255u
                              : 0u;
    unsigned left_end =
        (left.known_mask & (1u << byte)) != 0u ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte)) != 0u
                               ? (right.word >> (8u * byte)) & 255u
                               : 0u;
    unsigned right_end =
        (right.known_mask & (1u << byte)) != 0u ? right_start : 255u;
    unsigned borrow;
    for (borrow = 0u; borrow != 2u; ++borrow) {
      unsigned a;
      if ((borrow_mask & (1u << borrow)) == 0u)
        continue;
      for (a = left_start; a <= left_end; ++a) {
        unsigned b;
        for (b = right_start; b <= right_end; ++b) {
          int difference = (int)a - (int)b - (int)borrow;
          unsigned output = (unsigned)difference & 255u;
          next_borrow_mask |= 1u << (difference < 0);
          if (first != 0u) {
            first_output = output;
            first = 0u;
          } else if (first_output != output) {
            invariant = 0u;
          }
        }
      }
    }
    if (invariant != 0u)
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
    borrow_mask = next_borrow_mask;
  }
  return result;
}

static Nba97GameStaminaHandicapWord
add_constant(Nba97GameStaminaHandicapWord value, uint32_t constant) {
  Nba97GameStaminaHandicapWord immediate;
  set_known(&immediate, constant);
  return add_words(value, immediate);
}

static Nba97GameStaminaHandicapWord load_lbu(Nba97GameStaminaHandicapWord raw) {
  raw.word &= 0xffu;
  raw.known_mask = (uint8_t)((raw.known_mask & 1u) | 0x0eu);
  return raw;
}

static Nba97GameStaminaHandicapWord load_lhu(Nba97GameStaminaHandicapWord raw) {
  raw.word &= 0xffffu;
  raw.known_mask = (uint8_t)((raw.known_mask & 3u) | 0x0cu);
  return raw;
}

static Nba97GameStaminaHandicapWord load_lh(Nba97GameStaminaHandicapWord raw) {
  uint32_t half = raw.word & 0xffffu;
  raw.word = (half & 0x8000u) != 0u ? half | UINT32_C(0xffff0000) : half;
  raw.known_mask = (uint8_t)(raw.known_mask & 3u);
  if ((raw.known_mask & 2u) != 0u)
    raw.known_mask = (uint8_t)(raw.known_mask | 0x0cu);
  return raw;
}

static Nba97GameStaminaHandicapWord
shift_left(Nba97GameStaminaHandicapWord value, unsigned shift) {
  Nba97GameStaminaHandicapWord result;
  unsigned byte;
  result.word = value.word << shift;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned low_bit = byte * 8u;
    unsigned high_bit = low_bit + 7u;
    unsigned first_source = low_bit < shift ? 0u : (low_bit - shift) / 8u;
    unsigned last_source = high_bit < shift ? 0u : (high_bit - shift) / 8u;
    unsigned source;
    int known = 1;
    if (high_bit < shift) {
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
      continue;
    }
    for (source = first_source; source <= last_source; ++source)
      if ((value.known_mask & (1u << source)) == 0u)
        known = 0;
    if (known != 0)
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
  }
  return result;
}

static int64_t signed_word(uint32_t word) {
  return word < UINT32_C(0x80000000) ? (int64_t)word
                                     : (int64_t)word - INT64_C(0x100000000);
}

static void unsigned_bounds(Nba97GameStaminaHandicapWord value, uint32_t bias,
                            uint32_t *low, uint32_t *high) {
  unsigned byte;
  *low = 0u;
  *high = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    uint32_t bits = UINT32_C(255) << (byte * 8u);
    if ((value.known_mask & (1u << byte)) != 0u) {
      uint32_t fixed = value.word & bits;
      if (byte == 3u)
        fixed ^= bias;
      *low |= fixed;
      *high |= fixed;
    } else {
      *high |= bits;
    }
  }
}

static Nba97GameStaminaHandicapWord
predicate_signed_less_constant(Nba97GameStaminaHandicapWord value,
                               int32_t constant) {
  Nba97GameStaminaHandicapWord result;
  uint32_t low;
  uint32_t high;
  uint32_t biased_constant = (uint32_t)constant ^ UINT32_C(0x80000000);
  result.word = signed_word(value.word) < (int64_t)constant ? 1u : 0u;
  result.known_mask = 0x0eu;
  unsigned_bounds(value, UINT32_C(0x80000000), &low, &high);
  if (high < biased_constant || low >= biased_constant)
    result.known_mask = 0x0fu;
  return result;
}

static Nba97GameStaminaHandicapWord
predicate_unsigned_less_constant(Nba97GameStaminaHandicapWord value,
                                 uint32_t constant) {
  Nba97GameStaminaHandicapWord result;
  uint32_t low;
  uint32_t high;
  result.word = value.word < constant ? 1u : 0u;
  result.known_mask = 0x0eu;
  unsigned_bounds(value, 0u, &low, &high);
  if (high < constant || low >= constant)
    result.known_mask = 0x0fu;
  return result;
}

static Nba97GameStaminaHandicapWord
predicate_signed_greater_constant(Nba97GameStaminaHandicapWord value,
                                  int32_t constant) {
  Nba97GameStaminaHandicapWord result;
  uint32_t low;
  uint32_t high;
  uint32_t biased_constant = (uint32_t)constant ^ UINT32_C(0x80000000);
  result.word = signed_word(value.word) > (int64_t)constant ? 1u : 0u;
  result.known_mask = 0x0eu;
  unsigned_bounds(value, UINT32_C(0x80000000), &low, &high);
  if (low > biased_constant || high <= biased_constant)
    result.known_mask = 0x0fu;
  return result;
}

static int decide_zero(Run *run, Nba97GameStaminaHandicapWord value,
                       uint32_t pc, int *is_zero) {
  unsigned byte;
  for (byte = 0u; byte != 4u; ++byte)
    if ((value.known_mask & (1u << byte)) != 0u &&
        ((value.word >> (8u * byte)) & 255u) != 0u) {
      *is_zero = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (value.known_mask == 0x0fu) {
    *is_zero = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0u);
  return NBA97_TEXT_UNKNOWN;
}

static int decide_boolean(Run *run, Nba97GameStaminaHandicapWord value,
                          uint32_t pc, int *truth) {
  if ((value.known_mask & 1u) == 0u) {
    stop(run, pc, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *truth = (value.word & 1u) != 0u;
  return NBA97_TEXT_COMPLETE;
}

static int decide_nonnegative(Run *run, Nba97GameStaminaHandicapWord value,
                              uint32_t pc, int *nonnegative) {
  if ((value.known_mask & 8u) == 0u) {
    stop(run, pc, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *nonnegative = (value.word & UINT32_C(0x80000000)) == 0u;
  return NBA97_TEXT_COMPLETE;
}

static int address_from(Run *run, Nba97GameStaminaHandicapWord base,
                        uint32_t offset, uint32_t pc, uint32_t *address) {
  Nba97GameStaminaHandicapWord effective = add_constant(base, offset);
  if (effective.known_mask != 0x0fu) {
    stop(run, pc, effective.word);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = effective.word;
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_stamina_handicap(Nba97GameStaminaHandicapContext *context,
                                Nba97GameStaminaHandicapProgress *out) {
  Run storage;
  Run *run = &storage;
  Nba97GameStaminaHandicapWord raw;
  Nba97GameStaminaHandicapWord branch;
  Nba97GameStaminaHandicapWord zero;
  uint32_t address;
  unsigned index;
  int is_zero;
  int truth;
  int nonnegative;
  TRY(initialize(context, out, run));
  set_known(&zero, 0u);

  /* 0x80068504..0x8006851C: the source stores -1 to the handicap word in the
   * first BEQ delay slot, before a possibly unknown feature decision. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80020000));
  TRY(read_value(run, FEATURE_HANDICAP, 1u, UINT32_C(0x80068508), &raw));
  R(NBA97_MATCH_INITIALIZE_V1) = load_lbu(raw);
  set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80100000));
  R(NBA97_MATCH_INITIALIZE_A0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0xffffdb98));
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_MAX);
  branch = R(NBA97_MATCH_INITIALIZE_V1);
  TRY(write_value(run, HANDICAP, 2u, UINT32_C(0x8006851c),
                  R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(decide_zero(run, branch, UINT32_C(0x80068518), &is_zero));
  if (!is_zero) {
    /* 0x80068520..0x80068530: signed clock eligibility is materialized as a
     * SLTI word, preserving its known upper bytes on an unknown decision. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, CLOCK, 4u, UINT32_C(0x80068524),
                   &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_V0) = predicate_signed_less_constant(
        R(NBA97_MATCH_INITIALIZE_V0), INT32_C(7201));
    branch = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(decide_boolean(run, branch, UINT32_C(0x80068530), &truth));
    if (truth) {
      /* 0x80068538..0x8006856C: compare the unsigned score halfwords through
       * wrapped SUBU. The second SLTI runs in the first branch delay, and ORI
       * v0,5 runs in the second branch delay regardless of its outcome. */
      set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
      TRY(read_value(run, SCORE_HOME, 2u, UINT32_C(0x8006853c), &raw));
      R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(raw);
      set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80020000));
      TRY(read_value(run, SCORE_AWAY, 2u, UINT32_C(0x80068544), &raw));
      R(NBA97_MATCH_INITIALIZE_V1) = load_lhu(raw);
      R(NBA97_MATCH_INITIALIZE_A1) = subtract_words(
          R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
      R(NBA97_MATCH_INITIALIZE_V0) = predicate_signed_less_constant(
          R(NBA97_MATCH_INITIALIZE_A1), INT32_C(-2));
      branch = R(NBA97_MATCH_INITIALIZE_V0);
      R(NBA97_MATCH_INITIALIZE_V0) = predicate_signed_less_constant(
          R(NBA97_MATCH_INITIALIZE_A1), INT32_C(3));
      TRY(decide_boolean(run, branch, UINT32_C(0x80068554), &truth));
      if (truth) {
        TRY(write_value(run, HANDICAP, 2u, UINT32_C(0x80068560), zero));
      } else {
        branch = R(NBA97_MATCH_INITIALIZE_V0);
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), 5u);
        TRY(decide_boolean(run, branch, UINT32_C(0x80068564), &truth));
        if (!truth)
          TRY(write_value(run, HANDICAP, 2u, UINT32_C(0x8006856c),
                          R(NBA97_MATCH_INITIALIZE_V0)));
      }
    }
  }

  /* 0x80068570..0x80068584: phase is tested as a signed LH. The delay slot
   * assigns the distinct stamina delta address 0x800FDB7E even on refusal. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
  R(NBA97_MATCH_INITIALIZE_V1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xffffe8cc));
  TRY(read_value(run, PHASE, 2u, UINT32_C(0x80068578), &raw));
  R(NBA97_MATCH_INITIALIZE_V0) = load_lh(raw);
  branch = R(NBA97_MATCH_INITIALIZE_V0);
  R(NBA97_MATCH_INITIALIZE_A2) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xfffff2b2));
  TRY(decide_zero(run, branch, UINT32_C(0x80068580), &is_zero));
  if (!is_zero)
    goto epilogue;

  /* 0x80068588..0x800685D8: update 24 signed score entries. The source's
   * unsigned threshold admits only sign-extended values 0..0x7FFE. */
  set_known(&R(NBA97_MATCH_INITIALIZE_A3), 0u);
  set_known(&R(NBA97_MATCH_INITIALIZE_A0), 0x7fffu);
  set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80020000));
  R(NBA97_MATCH_INITIALIZE_V1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xfffff80c));
  for (index = 0u; index != 24u; ++index) {
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_V1), 0u,
                     UINT32_C(0x80068598), &address));
    TRY(read_value(run, address, 2u, UINT32_C(0x80068598), &raw));
    R(NBA97_MATCH_INITIALIZE_A1) = load_lh(raw);
    R(NBA97_MATCH_INITIALIZE_V0) =
        predicate_unsigned_less_constant(R(NBA97_MATCH_INITIALIZE_A1), 0x7fffu);
    branch = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(decide_boolean(run, branch, UINT32_C(0x800685a4), &truth));
    if (truth) {
      TRY(read_value(run, DELTA, 2u, UINT32_C(0x800685ac), &raw));
      R(NBA97_MATCH_INITIALIZE_V0) = load_lh(raw);
      R(NBA97_MATCH_INITIALIZE_A1) =
          add_words(R(NBA97_MATCH_INITIALIZE_A1), R(NBA97_MATCH_INITIALIZE_V0));
      R(NBA97_MATCH_INITIALIZE_V0) = predicate_signed_greater_constant(
          R(NBA97_MATCH_INITIALIZE_A1), INT32_C(32767));
      branch = R(NBA97_MATCH_INITIALIZE_V0);
      TRY(decide_boolean(run, branch, UINT32_C(0x800685bc), &truth));
      if (truth)
        set_known(&R(NBA97_MATCH_INITIALIZE_A1), 0x7fffu);
      TRY(write_value(run, address, 2u, UINT32_C(0x800685c8),
                      R(NBA97_MATCH_INITIALIZE_A1)));
      ++out->score_updates;
    }
    ++out->score_iterations;
    R(NBA97_MATCH_INITIALIZE_A3) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A3), 1u);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), index + 1u < 24u ? 1u : 0u);
    R(NBA97_MATCH_INITIALIZE_V1) =
        add_constant(R(NBA97_MATCH_INITIALIZE_V1), 0x22u);
  }

  /* 0x800685DC..0x80068614: establish the feature, delta, and actor tables;
   * load each live actor and linked record, then reject negative stamina. */
  set_known(&R(NBA97_MATCH_INITIALIZE_A3), 0u);
  set_known(&R(REG_T2), UINT32_C(0x80020000));
  R(REG_T2) = add_constant(R(REG_T2), 0x1d93u);
  set_known(&R(REG_T1), UINT32_C(0x80100000));
  R(REG_T1) = add_constant(R(REG_T1), UINT32_C(0xffffdb7e));
  set_known(&R(REG_T0), UINT32_C(0x80020000));
  R(REG_T0) = add_constant(R(REG_T0), 0x0becu);
  for (index = 0u; index != 10u; ++index) {
    TRY(address_from(run, R(REG_T0), 0u, UINT32_C(0x800685f8), &address));
    TRY(read_value(run, address, 4u, UINT32_C(0x800685f8),
                   &R(NBA97_MATCH_INITIALIZE_A0)));
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0x1cu,
                     UINT32_C(0x80068600), &address));
    TRY(read_value(run, address, 4u, UINT32_C(0x80068600),
                   &R(NBA97_MATCH_INITIALIZE_A2)));
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A2), 0x20u,
                     UINT32_C(0x80068608), &address));
    TRY(read_value(run, address, 2u, UINT32_C(0x80068608), &raw));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(raw);
    branch = R(NBA97_MATCH_INITIALIZE_V0);
    TRY(decide_nonnegative(run, branch, UINT32_C(0x80068610), &nonnegative));
    if (nonnegative) {
      TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0xa0u,
                       UINT32_C(0x80068618), &address));
      TRY(read_value(run, address, 2u, UINT32_C(0x80068618), &raw));
      R(NBA97_MATCH_INITIALIZE_V0) = load_lh(raw);
      branch = R(NBA97_MATCH_INITIALIZE_V0);
      TRY(decide_zero(run, branch, UINT32_C(0x80068620), &is_zero));
      if (!is_zero) {
        TRY(read_value(run, FEATURE_STAMINA, 1u, UINT32_C(0x80068628), &raw));
        R(NBA97_MATCH_INITIALIZE_V1) = load_lbu(raw);
        branch = R(NBA97_MATCH_INITIALIZE_V1);
        TRY(decide_zero(run, branch, UINT32_C(0x80068630), &is_zero));
        if (is_zero) {
          TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0xddu,
                           UINT32_C(0x80068638), &address));
          TRY(read_value(run, address, 1u, UINT32_C(0x80068638), &raw));
          R(NBA97_MATCH_INITIALIZE_V0) = load_lbu(raw);
          branch = R(NBA97_MATCH_INITIALIZE_V0);
          TRY(decide_zero(run, branch, UINT32_C(0x80068640), &is_zero));
          if (is_zero)
            goto clear_actor_flag;
        }

        TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0xddu,
                         UINT32_C(0x80068648), &address));
        TRY(read_value(run, address, 1u, UINT32_C(0x80068648), &raw));
        R(NBA97_MATCH_INITIALIZE_V0) = load_lbu(raw);
        branch = R(NBA97_MATCH_INITIALIZE_V0);
        TRY(decide_zero(run, branch, UINT32_C(0x80068650), &is_zero));
        if (!is_zero) {
          TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0x44u,
                           UINT32_C(0x80068658), &address));
          TRY(read_value(run, address, 2u, UINT32_C(0x80068658), &raw));
          R(NBA97_MATCH_INITIALIZE_V1) = load_lh(raw);
          R(NBA97_MATCH_INITIALIZE_V0) =
              shift_left(R(NBA97_MATCH_INITIALIZE_V1), 1u);
          R(NBA97_MATCH_INITIALIZE_A1) = add_words(
              R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
        } else {
          branch = R(NBA97_MATCH_INITIALIZE_V1);
          TRY(decide_zero(run, branch, UINT32_C(0x8006866c), &is_zero));
          if (!is_zero) {
            TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0x44u,
                             UINT32_C(0x80068674), &address));
            TRY(read_value(run, address, 2u, UINT32_C(0x80068674), &raw));
            R(NBA97_MATCH_INITIALIZE_A1) = load_lh(raw);
          }
        }

        /* 0x80068678..0x80068698: reread the distinct FDB7E delta and linked
         * stamina, store the wrapped low half, then clamp a negative half. */
        TRY(read_value(run, DELTA, 2u, UINT32_C(0x80068678), &raw));
        R(NBA97_MATCH_INITIALIZE_V1) = load_lhu(raw);
        TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A2), 0x20u,
                         UINT32_C(0x8006867c), &address));
        TRY(read_value(run, address, 2u, UINT32_C(0x8006867c), &raw));
        R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(raw);
        R(NBA97_MATCH_INITIALIZE_V1) = add_words(R(NBA97_MATCH_INITIALIZE_V1),
                                                 R(NBA97_MATCH_INITIALIZE_A1));
        R(NBA97_MATCH_INITIALIZE_V0) = subtract_words(
            R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
        TRY(write_value(run, address, 2u, UINT32_C(0x80068688),
                        R(NBA97_MATCH_INITIALIZE_V0)));
        R(NBA97_MATCH_INITIALIZE_V0) =
            shift_left(R(NBA97_MATCH_INITIALIZE_V0), 16u);
        TRY(decide_nonnegative(run, R(NBA97_MATCH_INITIALIZE_V0),
                               UINT32_C(0x80068690), &nonnegative));
        if (!nonnegative)
          TRY(write_value(run, address, 2u, UINT32_C(0x80068698), zero));
        ++out->stamina_updates;
      }
    }

  clear_actor_flag:
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0xddu,
                     UINT32_C(0x8006869c), &address));
    TRY(write_value(run, address, 1u, UINT32_C(0x8006869c), zero));
    ++out->actor_iterations;
    R(NBA97_MATCH_INITIALIZE_A3) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A3), 1u);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), index + 1u < 10u ? 1u : 0u);
    R(REG_T0) = add_constant(R(REG_T0), 4u);
  }

epilogue:
  /* 0x800686B0..0x800686B4: JR consumes live ra; the NOP delay has completed
   * before unknown or alignment reporting. No stack or HI/LO state changed. */
  out->return_address = R(NBA97_MATCH_INITIALIZE_RA);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x800686b0), R(NBA97_MATCH_INITIALIZE_RA).word);
    return NBA97_TEXT_UNKNOWN;
  }
  if ((R(NBA97_MATCH_INITIALIZE_RA).word & 3u) != 0u) {
    stop(run, UINT32_C(0x800686b0), R(NBA97_MATCH_INITIALIZE_RA).word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1u;
  stop(run, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
