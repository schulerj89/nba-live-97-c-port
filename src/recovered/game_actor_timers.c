#include "game_actor_timers.h"

#include <string.h>

#define ENTITY_POINTERS UINT32_C(0x80020bec)
#define DELTA UINT32_C(0x800fdb6c)
#define CLOCK UINT32_C(0x800fdb58)
#define CACHE_60 UINT32_C(0x800fdb74)
#define CONTROLLER_POINTERS UINT32_C(0x800fdc50)
#define TEAM_POINTERS UINT32_C(0x800fdc70)

enum {
  REG_T0 = NBA97_MATCH_INITIALIZE_T0,
  REG_T1 = NBA97_MATCH_INITIALIZE_T0 + 1,
  REG_T2 = NBA97_MATCH_INITIALIZE_T0 + 2,
  REG_T3 = NBA97_MATCH_INITIALIZE_T0 + 3
};

typedef struct Run {
  Nba97GameActorTimersContext *context;
  Nba97GameActorTimersProgress *out;
  Nba97GameActorTimersMachine machine;
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

static void set_known(Nba97GameActorTimersWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameActorTimersMachine *machine) {
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

static int initialize(Nba97GameActorTimersContext *context,
                      Nba97GameActorTimersProgress *out, Run *run) {
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

static Nba97GameActorTimersWord add_words(Nba97GameActorTimersWord left,
                                          Nba97GameActorTimersWord right) {
  Nba97GameActorTimersWord result;
  unsigned carry_mask = 1u;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0u;
  if (left.known_mask == 0x0fu && right.known_mask == 0x0fu) {
    result.known_mask = 0x0fu;
    return result;
  }
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

static Nba97GameActorTimersWord subtract_words(Nba97GameActorTimersWord left,
                                               Nba97GameActorTimersWord right) {
  Nba97GameActorTimersWord result;
  unsigned borrow_mask = 1u;
  unsigned byte;
  result.word = left.word - right.word;
  result.known_mask = 0u;
  if (left.known_mask == 0x0fu && right.known_mask == 0x0fu) {
    result.known_mask = 0x0fu;
    return result;
  }
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

static Nba97GameActorTimersWord add_constant(Nba97GameActorTimersWord value,
                                             uint32_t constant) {
  Nba97GameActorTimersWord immediate;
  set_known(&immediate, constant);
  return add_words(value, immediate);
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
                    unsigned width, const Nba97GameActorTimersWord *value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameActorTimersAccess *event = &run->context->access_journal[index];
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

static int address_from(Run *run, Nba97GameActorTimersWord base,
                        uint32_t offset, uint32_t pc, uint32_t *address) {
  Nba97GameActorTimersWord effective = add_constant(base, offset);
  if (effective.known_mask != 0x0fu) {
    stop(run, pc, effective.word);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = effective.word;
  return NBA97_TEXT_COMPLETE;
}

static int read_value(Run *run, uint32_t address, unsigned width, uint32_t pc,
                      Nba97GameActorTimersWord *destination) {
  uint8_t *data;
  uint8_t *known;
  Nba97GameActorTimersWord loaded;
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
  journal(run, NBA97_GAME_ACTOR_TIMERS_READ, pc, address, width, &loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, uint32_t address, unsigned width, uint32_t pc,
                       Nba97GameActorTimersWord value) {
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
  journal(run, NBA97_GAME_ACTOR_TIMERS_STORE, pc, address, width, &value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static Nba97GameActorTimersWord load_lhu(Nba97GameActorTimersWord raw) {
  raw.word &= 0xffffu;
  raw.known_mask = (uint8_t)((raw.known_mask & 3u) | 0x0cu);
  return raw;
}

static Nba97GameActorTimersWord load_lh(Nba97GameActorTimersWord raw) {
  uint32_t half = raw.word & 0xffffu;
  raw.word = (half & 0x8000u) != 0u ? half | UINT32_C(0xffff0000) : half;
  raw.known_mask = (uint8_t)(raw.known_mask & 3u);
  if ((raw.known_mask & 2u) != 0u)
    raw.known_mask = (uint8_t)(raw.known_mask | 0x0cu);
  return raw;
}

static Nba97GameActorTimersWord shift_left(Nba97GameActorTimersWord value,
                                           unsigned shift) {
  Nba97GameActorTimersWord result;
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

static Nba97GameActorTimersWord
shift_right_arithmetic(Nba97GameActorTimersWord value, unsigned shift) {
  Nba97GameActorTimersWord result;
  unsigned byte;
  result.word = value.word >> shift;
  if ((value.word & UINT32_C(0x80000000)) != 0u)
    result.word |= ~(UINT32_MAX >> shift);
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned low_bit = byte * 8u + shift;
    unsigned high_bit = low_bit + 7u;
    unsigned first_source = low_bit / 8u;
    unsigned last_source = high_bit < 32u ? high_bit / 8u : 3u;
    unsigned source;
    int known = 1;
    for (source = first_source; source <= last_source; ++source)
      if ((value.known_mask & (1u << source)) == 0u)
        known = 0;
    if (high_bit >= 32u && (value.known_mask & 8u) == 0u)
      known = 0;
    if (known != 0)
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
  }
  return result;
}

static int decide_zero(Run *run, Nba97GameActorTimersWord value, uint32_t pc,
                       int *is_zero) {
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

static int decide_equal(Run *run, Nba97GameActorTimersWord left,
                        Nba97GameActorTimersWord right, uint32_t pc,
                        int *equal) {
  unsigned byte;
  for (byte = 0u; byte != 4u; ++byte) {
    uint8_t bit = (uint8_t)(1u << byte);
    if ((left.known_mask & right.known_mask & bit) != 0u &&
        ((left.word >> (8u * byte)) & 255u) !=
            ((right.word >> (8u * byte)) & 255u)) {
      *equal = 0;
      return NBA97_TEXT_COMPLETE;
    }
  }
  if (left.known_mask == 0x0fu && right.known_mask == 0x0fu) {
    *equal = left.word == right.word;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0u);
  return NBA97_TEXT_UNKNOWN;
}

static int decide_nonnegative(Run *run, Nba97GameActorTimersWord value,
                              uint32_t pc, int *nonnegative) {
  if ((value.known_mask & 8u) == 0u) {
    stop(run, pc, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *nonnegative = (value.word & UINT32_C(0x80000000)) == 0u;
  return NBA97_TEXT_COMPLETE;
}

static int64_t signed_word(uint32_t word) {
  return word < UINT32_C(0x80000000) ? (int64_t)word
                                     : (int64_t)word - INT64_C(0x100000000);
}

typedef struct ProductKnownness {
  uint32_t first_hi;
  uint32_t first_lo;
  uint8_t hi_mask;
  uint8_t lo_mask;
  uint8_t first;
} ProductKnownness;

static void observe_product(ProductKnownness *knownness, uint32_t word,
                            uint32_t multiplier) {
  uint64_t product = (uint64_t)(signed_word(word) * signed_word(multiplier));
  uint32_t lo = (uint32_t)product;
  uint32_t hi = (uint32_t)(product >> 32u);
  unsigned byte;
  if (knownness->first != 0u) {
    knownness->first = 0u;
    knownness->first_hi = hi;
    knownness->first_lo = lo;
    knownness->hi_mask = 0x0fu;
    knownness->lo_mask = 0x0fu;
    return;
  }
  for (byte = 0u; byte != 4u; ++byte) {
    uint32_t mask = UINT32_C(255) << (8u * byte);
    if ((hi & mask) != (knownness->first_hi & mask))
      knownness->hi_mask =
          (uint8_t)(knownness->hi_mask & ~(uint8_t)(1u << byte));
    if ((lo & mask) != (knownness->first_lo & mask))
      knownness->lo_mask =
          (uint8_t)(knownness->lo_mask & ~(uint8_t)(1u << byte));
  }
}

static void enumerate_products(ProductKnownness *knownness,
                               Nba97GameActorTimersWord source,
                               uint32_t multiplier, unsigned byte,
                               uint32_t candidate) {
  unsigned value;
  if (byte == 4u) {
    observe_product(knownness, candidate, multiplier);
    return;
  }
  if ((source.known_mask & (1u << byte)) != 0u) {
    uint32_t mask = UINT32_C(255) << (8u * byte);
    enumerate_products(knownness, source, multiplier, byte + 1u,
                       (candidate & ~mask) | (source.word & mask));
    return;
  }
  for (value = 0u; value != 256u; ++value) {
    uint32_t mask = UINT32_C(255) << (8u * byte);
    enumerate_products(knownness, source, multiplier, byte + 1u,
                       (candidate & ~mask) | (value << (8u * byte)));
  }
}

static Nba97GameActorTimersMultiplyTrace *
begin_multiply(Run *run, uint32_t pc, Nba97GameActorTimersWord multiplicand,
               uint32_t multiplier_word) {
  Nba97GameActorTimersMultiplyTrace *trace =
      &run->out->multiply[run->out->multiply_count++];
  Nba97GameActorTimersWord multiplier;
  uint64_t product =
      (uint64_t)(signed_word(multiplicand.word) * signed_word(multiplier_word));
  unsigned unknown_bytes = 0u;
  unsigned byte;
  set_known(&multiplier, multiplier_word);
  memset(trace, 0, sizeof(*trace));
  trace->pc = pc;
  trace->multiplicand = multiplicand;
  trace->multiplier = multiplier;
  run->machine.lo.word = (uint32_t)product;
  run->machine.hi.word = (uint32_t)(product >> 32u);
  run->machine.lo.known_mask = 0u;
  run->machine.hi.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte)
    if ((multiplicand.known_mask & (1u << byte)) == 0u)
      ++unknown_bytes;
  if (unknown_bytes == 0u) {
    run->machine.lo.known_mask = 0x0fu;
    run->machine.hi.known_mask = 0x0fu;
  } else if (unknown_bytes <= 2u) {
    ProductKnownness knownness;
    memset(&knownness, 0, sizeof(knownness));
    knownness.first = 1u;
    enumerate_products(&knownness, multiplicand, multiplier_word, 0u, 0u);
    run->machine.lo.known_mask = knownness.lo_mask;
    run->machine.hi.known_mask = knownness.hi_mask;
  } else {
    uint8_t prefix = 0u;
    for (byte = 0u; byte != 4u; ++byte) {
      if ((multiplicand.known_mask & (1u << byte)) == 0u)
        break;
      prefix = (uint8_t)(prefix | (uint8_t)(1u << byte));
    }
    run->machine.lo.known_mask = prefix;
  }
  trace->hi = run->machine.hi;
  trace->lo = run->machine.lo;
  publish(run);
  return trace;
}

static int decrement_timer(Run *run, Nba97GameActorTimersWord actor,
                           uint32_t field_offset, uint32_t load_pc,
                           uint32_t zero_branch_pc, uint32_t delta_pc,
                           uint32_t store_pc, uint32_t branch_pc,
                           uint32_t clamp_pc, int *was_nonzero) {
  Nba97GameActorTimersWord raw;
  Nba97GameActorTimersWord branch_value;
  Nba97GameActorTimersWord zero;
  uint32_t address;
  int is_zero;
  int nonnegative;
  set_known(&zero, 0u);
  TRY(address_from(run, actor, field_offset, load_pc, &address));
  TRY(read_value(run, address, 2u, load_pc, &R(NBA97_MATCH_INITIALIZE_V0)));
  R(NBA97_MATCH_INITIALIZE_V0) = load_lh(R(NBA97_MATCH_INITIALIZE_V0));
  branch_value = R(NBA97_MATCH_INITIALIZE_V0);
  R(NBA97_MATCH_INITIALIZE_V1) = branch_value;
  TRY(decide_zero(run, branch_value, zero_branch_pc, &is_zero));
  *was_nonzero = !is_zero;
  if (is_zero)
    return NBA97_TEXT_COMPLETE;
  TRY(read_value(run, DELTA, 2u, delta_pc, &raw));
  R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(raw);
  R(NBA97_MATCH_INITIALIZE_V0) = subtract_words(R(NBA97_MATCH_INITIALIZE_V1),
                                                R(NBA97_MATCH_INITIALIZE_V0));
  TRY(address_from(run, actor, field_offset, store_pc, &address));
  TRY(write_value(run, address, 2u, store_pc, R(NBA97_MATCH_INITIALIZE_V0)));
  R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_V0), 16u);
  TRY(decide_nonnegative(run, R(NBA97_MATCH_INITIALIZE_V0), branch_pc,
                         &nonnegative));
  if (!nonnegative)
    TRY(write_value(run, address, 2u, clamp_pc, zero));
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_actor_timers(Nba97GameActorTimersContext *context,
                            Nba97GameActorTimersProgress *out) {
  Run storage;
  Run *run = &storage;
  Nba97GameActorTimersWord raw;
  Nba97GameActorTimersWord branch_value;
  Nba97GameActorTimersWord zero;
  Nba97GameActorTimersMultiplyTrace *trace;
  uint32_t address;
  unsigned index;
  int is_zero;
  int equal;
  int nonnegative;
  int timer_nonzero;
  TRY(initialize(context, out, run));
  set_known(&zero, 0u);

  /* 0x8006830C..0x80068328: allocate the private frame without spilling ra,
   * establish the three loop constants, and load the first actor pointer. */
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe8));
  out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  set_known(&R(NBA97_MATCH_INITIALIZE_A3), 0u);
  set_known(&R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x80100000));
  R(NBA97_MATCH_INITIALIZE_A2) =
      add_constant(R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0xffffdb6c));
  set_known(&R(REG_T0), 1u);
  set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80020000));
  R(NBA97_MATCH_INITIALIZE_A1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_A1), 0x0becu);

  for (index = 0u; index != 11u; ++index) {
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A1), 0u,
                     UINT32_C(0x80068328), &address));
    TRY(read_value(run, address, 4u, UINT32_C(0x80068328),
                   &R(NBA97_MATCH_INITIALIZE_A0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), index < 10u ? 1u : 0u);
    if (index < 10u) {
      /* 0x80068338..0x80068368: clear +D8/+F2 after loading +E6,
       * subtract a fresh delta, store the wrap, then clamp negatives. */
      TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0xe6u,
                       UINT32_C(0x80068338), &address));
      TRY(read_value(run, address, 2u, UINT32_C(0x80068338),
                     &R(NBA97_MATCH_INITIALIZE_V0)));
      R(NBA97_MATCH_INITIALIZE_V0) = load_lh(R(NBA97_MATCH_INITIALIZE_V0));
      branch_value = R(NBA97_MATCH_INITIALIZE_V0);
      TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0xd8u,
                       UINT32_C(0x8006833c), &address));
      TRY(write_value(run, address, 1u, UINT32_C(0x8006833c), zero));
      TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0xf2u,
                       UINT32_C(0x80068340), &address));
      TRY(write_value(run, address, 2u, UINT32_C(0x80068340), zero));
      R(NBA97_MATCH_INITIALIZE_V1) = branch_value;
      TRY(decide_zero(run, branch_value, UINT32_C(0x80068344), &is_zero));
      if (!is_zero) {
        TRY(read_value(run, DELTA, 2u, UINT32_C(0x8006834c), &raw));
        R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(raw);
        R(NBA97_MATCH_INITIALIZE_V0) = subtract_words(
            R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0));
        TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0xe6u,
                         UINT32_C(0x80068358), &address));
        TRY(write_value(run, address, 2u, UINT32_C(0x80068358),
                        R(NBA97_MATCH_INITIALIZE_V0)));
        R(NBA97_MATCH_INITIALIZE_V0) =
            shift_left(R(NBA97_MATCH_INITIALIZE_V0), 16u);
        TRY(decide_nonnegative(run, R(NBA97_MATCH_INITIALIZE_V0),
                               UINT32_C(0x80068360), &nonnegative));
        if (!nonnegative)
          TRY(write_value(run, address, 2u, UINT32_C(0x80068368), zero));
      }

      /* 0x8006836C..0x8006839C: +E4 independently rereads delta. Its
       * original nonzero value causes +DD=1 even after a zero clamp. */
      TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0xe4u,
                       UINT32_C(0x8006836c), &address));
      TRY(read_value(run, address, 2u, UINT32_C(0x8006836c),
                     &R(NBA97_MATCH_INITIALIZE_V0)));
      R(NBA97_MATCH_INITIALIZE_V0) = load_lh(R(NBA97_MATCH_INITIALIZE_V0));
      branch_value = R(NBA97_MATCH_INITIALIZE_V0);
      R(NBA97_MATCH_INITIALIZE_V1) = branch_value;
      TRY(decide_zero(run, branch_value, UINT32_C(0x80068374), &is_zero));
      if (!is_zero) {
        TRY(read_value(run, DELTA, 2u, UINT32_C(0x8006837c), &raw));
        R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(raw);
        R(NBA97_MATCH_INITIALIZE_V0) = subtract_words(
            R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0));
        TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0xe4u,
                         UINT32_C(0x80068388), &address));
        TRY(write_value(run, address, 2u, UINT32_C(0x80068388),
                        R(NBA97_MATCH_INITIALIZE_V0)));
        R(NBA97_MATCH_INITIALIZE_V0) =
            shift_left(R(NBA97_MATCH_INITIALIZE_V0), 16u);
        TRY(decide_nonnegative(run, R(NBA97_MATCH_INITIALIZE_V0),
                               UINT32_C(0x80068390), &nonnegative));
        if (!nonnegative)
          TRY(write_value(run, address, 2u, UINT32_C(0x80068398), zero));
        TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0xddu,
                         UINT32_C(0x8006839c), &address));
        TRY(write_value(run, address, 1u, UINT32_C(0x8006839c), R(REG_T0)));
      }
    }

    /* 0x800683A0..0x800683CC: all eleven actors receive the +B4 timer
     * path, including its store-before-clamp behavior. */
    TRY(decrement_timer(
        run, R(NBA97_MATCH_INITIALIZE_A0), 0xb4u, UINT32_C(0x800683a0),
        UINT32_C(0x800683a8), UINT32_C(0x800683b0), UINT32_C(0x800683bc),
        UINT32_C(0x800683c4), UINT32_C(0x800683cc), &timer_nonzero));
    (void)timer_nonzero;
    ++out->entity_iterations;
    R(NBA97_MATCH_INITIALIZE_A3) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A3), 1u);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), index + 1u < 11u ? 1u : 0u);
    R(NBA97_MATCH_INITIALIZE_A1) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A1), 4u);
  }

  /* 0x800683E0..0x80068420: compute signed clock/60 through the original
   * MULT/MFHI pipeline, compare the signed cached half, then clear a3 in the
   * clock-zero branch delay even when the branch decision is unknown. */
  set_known(&R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x80100000));
  R(NBA97_MATCH_INITIALIZE_A2) =
      add_constant(R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0xffffdb58));
  TRY(read_value(run, CLOCK, 4u, UINT32_C(0x800683e8),
                 &R(NBA97_MATCH_INITIALIZE_A1)));
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x88880000));
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x88888889));
  trace = begin_multiply(run, UINT32_C(0x800683f4),
                         R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x88888889));
  set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80100000));
  TRY(read_value(run, CACHE_60, 2u, UINT32_C(0x800683fc), &raw));
  R(NBA97_MATCH_INITIALIZE_A0) = load_lh(raw);
  R(NBA97_MATCH_INITIALIZE_V1) =
      shift_right_arithmetic(R(NBA97_MATCH_INITIALIZE_A1), 31u);
  R(REG_T3) = run->machine.hi;
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_words(R(REG_T3), R(NBA97_MATCH_INITIALIZE_A1));
  R(NBA97_MATCH_INITIALIZE_V0) =
      shift_right_arithmetic(R(NBA97_MATCH_INITIALIZE_V0), 5u);
  R(NBA97_MATCH_INITIALIZE_V0) = subtract_words(R(NBA97_MATCH_INITIALIZE_V0),
                                                R(NBA97_MATCH_INITIALIZE_V1));
  trace->quotient = R(NBA97_MATCH_INITIALIZE_V0);
  out->clock_quotient_60 = R(NBA97_MATCH_INITIALIZE_V0);
  TRY(decide_equal(run, R(NBA97_MATCH_INITIALIZE_V0),
                   R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80068414), &equal));
  if (!equal) {
    branch_value = R(NBA97_MATCH_INITIALIZE_A1);
    set_known(&R(NBA97_MATCH_INITIALIZE_A3), 0u);
    TRY(decide_zero(run, branch_value, UINT32_C(0x8006841c), &is_zero));
    if (!is_zero) {
      set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
      TRY(write_value(run, CACHE_60, 2u, UINT32_C(0x80068428),
                      R(NBA97_MATCH_INITIALIZE_V0)));
      set_known(&R(NBA97_MATCH_INITIALIZE_A1), 1u);
      R(NBA97_MATCH_INITIALIZE_A0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_A2), 0x118u);
      for (index = 0u; index != 10u; ++index) {
        TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0u,
                         UINT32_C(0x80068434), &address));
        TRY(read_value(run, address, 4u, UINT32_C(0x80068434),
                       &R(NBA97_MATCH_INITIALIZE_V1)));
        TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_V1), 0x1au,
                         UINT32_C(0x8006843c), &address));
        TRY(read_value(run, address, 2u, UINT32_C(0x8006843c), &raw));
        R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(raw);
        R(NBA97_MATCH_INITIALIZE_V0) =
            add_constant(R(NBA97_MATCH_INITIALIZE_V0), 1u);
        TRY(write_value(run, address, 2u, UINT32_C(0x80068448),
                        R(NBA97_MATCH_INITIALIZE_V0)));
        TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0u,
                         UINT32_C(0x8006844c), &address));
        TRY(read_value(run, address, 4u, UINT32_C(0x8006844c),
                       &R(NBA97_MATCH_INITIALIZE_V0)));
        R(NBA97_MATCH_INITIALIZE_A3) =
            add_constant(R(NBA97_MATCH_INITIALIZE_A3), 1u);
        TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_V0), 0x1cu,
                         UINT32_C(0x80068454), &address));
        TRY(write_value(run, address, 1u, UINT32_C(0x80068454),
                        R(NBA97_MATCH_INITIALIZE_A1)));
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), index + 1u < 10u ? 1u : 0u);
        R(NBA97_MATCH_INITIALIZE_A0) =
            add_constant(R(NBA97_MATCH_INITIALIZE_A0), 4u);
        ++out->team_counter_updates;
      }
    }
  }

  /* 0x80068464..0x80068494: each of ten actors reloads its signed controller
   * index. BLTZ consumes the old value while SLL index*4 always runs in its
   * delay slot, including unknown and negative-index paths. */
  set_known(&R(NBA97_MATCH_INITIALIZE_A3), 0u);
  set_known(&R(REG_T1), UINT32_C(0x80100000));
  R(REG_T1) = add_constant(R(REG_T1), UINT32_C(0xffffdc50));
  set_known(&R(REG_T2), UINT32_C(0x91a20000));
  set_known(&R(REG_T2), UINT32_C(0x91a2b3c5));
  set_known(&R(REG_T0), UINT32_C(0x80020000));
  R(REG_T0) = add_constant(R(REG_T0), 0x0becu);
  for (index = 0u; index != 10u; ++index) {
    TRY(address_from(run, R(REG_T0), 0u, UINT32_C(0x80068480), &address));
    TRY(read_value(run, address, 4u, UINT32_C(0x80068480),
                   &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_V0), 4u,
                     UINT32_C(0x80068488), &address));
    TRY(read_value(run, address, 2u, UINT32_C(0x80068488), &raw));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(raw);
    branch_value = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_V0), 2u);
    TRY(decide_nonnegative(run, branch_value, UINT32_C(0x80068490),
                           &nonnegative));
    if (nonnegative) {
      TRY(read_value(run, CLOCK, 4u, UINT32_C(0x80068498),
                     &R(NBA97_MATCH_INITIALIZE_A2)));
      trace =
          begin_multiply(run, UINT32_C(0x800684a0),
                         R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x91a2b3c5));
      R(NBA97_MATCH_INITIALIZE_V0) =
          add_words(R(NBA97_MATCH_INITIALIZE_V0), R(REG_T1));
      TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_V0), 0u,
                       UINT32_C(0x800684a8), &address));
      TRY(read_value(run, address, 4u, UINT32_C(0x800684a8),
                     &R(NBA97_MATCH_INITIALIZE_A1)));
      TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A1), 0x22u,
                       UINT32_C(0x800684b0), &address));
      TRY(read_value(run, address, 2u, UINT32_C(0x800684b0), &raw));
      R(NBA97_MATCH_INITIALIZE_A0) = load_lhu(raw);
      R(NBA97_MATCH_INITIALIZE_V0) =
          shift_right_arithmetic(R(NBA97_MATCH_INITIALIZE_A2), 31u);
      R(REG_T3) = run->machine.hi;
      R(NBA97_MATCH_INITIALIZE_V1) =
          add_words(R(REG_T3), R(NBA97_MATCH_INITIALIZE_A2));
      R(NBA97_MATCH_INITIALIZE_V1) =
          shift_right_arithmetic(R(NBA97_MATCH_INITIALIZE_V1), 11u);
      R(NBA97_MATCH_INITIALIZE_V1) = subtract_words(
          R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0));
      trace->quotient = R(NBA97_MATCH_INITIALIZE_V1);
      out->last_clock_quotient_3600 = R(NBA97_MATCH_INITIALIZE_V1);
      TRY(decide_equal(run, R(NBA97_MATCH_INITIALIZE_V1),
                       R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x800684c8),
                       &equal));
      if (!equal) {
        TRY(decide_zero(run, R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x800684d0),
                        &is_zero));
        if (!is_zero) {
          TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A1), 0x1eu,
                           UINT32_C(0x800684d8), &address));
          TRY(read_value(run, address, 2u, UINT32_C(0x800684d8), &raw));
          R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(raw);
          TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A1), 0x22u,
                           UINT32_C(0x800684dc), &address));
          TRY(write_value(run, address, 2u, UINT32_C(0x800684dc),
                          R(NBA97_MATCH_INITIALIZE_V1)));
          R(NBA97_MATCH_INITIALIZE_V0) =
              add_constant(R(NBA97_MATCH_INITIALIZE_V0), 1u);
          TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A1), 0x1eu,
                           UINT32_C(0x800684e4), &address));
          TRY(write_value(run, address, 2u, UINT32_C(0x800684e4),
                          R(NBA97_MATCH_INITIALIZE_V0)));
          ++out->participation_updates;
        }
      }
    }
    ++out->participation_iterations;
    R(NBA97_MATCH_INITIALIZE_A3) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A3), 1u);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), index + 1u < 10u ? 1u : 0u);
    R(REG_T0) = add_constant(R(REG_T0), 4u);
  }

  /* 0x800684F8..0x80068500: restore sp, then consume live ra only at JR.
   * The final NOP completes before unknown/alignment reporting. */
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x18u);
  out->return_address = R(NBA97_MATCH_INITIALIZE_RA);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x800684fc), R(NBA97_MATCH_INITIALIZE_RA).word);
    return NBA97_TEXT_UNKNOWN;
  }
  if ((R(NBA97_MATCH_INITIALIZE_RA).word & 3u) != 0u) {
    stop(run, UINT32_C(0x800684fc), R(NBA97_MATCH_INITIALIZE_RA).word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1u;
  stop(run, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
