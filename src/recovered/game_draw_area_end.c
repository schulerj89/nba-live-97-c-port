#include "game_draw_area_end.h"

#include <string.h>

typedef Nba97GameDrawAreaEndWord Word;

typedef struct Run {
  Nba97GameDrawAreaEndContext *context;
  Nba97GameDrawAreaEndProgress *progress;
  Nba97GameDrawAreaEndMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])

enum Decision { DECISION_FALSE = 0, DECISION_TRUE = 1, DECISION_UNKNOWN = 2 };

static int valid_word(Word value) { return value.known_mask <= 15u; }

static int valid_machine(const Nba97GameDrawAreaEndMachine *machine) {
  unsigned index;
  if (machine->registers.gpr[0].word != 0u ||
      machine->registers.gpr[0].known_mask != 15u || !valid_word(machine->hi) ||
      !valid_word(machine->lo))
    return 0;
  for (index = 0u; index != 32u; ++index)
    if (!valid_word(machine->registers.gpr[index]))
      return 0;
  return 1;
}

static int valid_memory(const Nba97GameTextMemory *memory) {
  size_t index, earlier;
  if (memory->count != 0u && memory->region == NULL)
    return 0;
  for (index = 0u; index != memory->count; ++index) {
    const Nba97GameTextRegion *region = &memory->region[index];
    if (region->data == NULL || region->size == 0u ||
        region->size > UINT64_C(0x100000000) ||
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

static Word known_word(uint32_t value) {
  Word result;
  result.word = value;
  result.known_mask = 15u;
  return result;
}

static Word sign_extend_half(Word value) {
  Word result;
  uint32_t half = value.word & UINT32_C(0xffff);
  result.word =
      (half & UINT32_C(0x8000)) != 0u ? half | UINT32_C(0xffff0000) : half;
  result.known_mask = (uint8_t)(value.known_mask & 3u);
  if ((value.known_mask & 2u) != 0u)
    result.known_mask = (uint8_t)(result.known_mask | 12u);
  return result;
}

static Word shift_left(Word value, unsigned amount) {
  Word result;
  unsigned destination, source;
  result.word = amount >= 32u ? 0u : value.word << amount;
  result.known_mask = 0u;
  for (destination = 0u; destination != 4u; ++destination) {
    uint32_t destination_bits = UINT32_C(0xff) << (8u * destination);
    int known = 1;
    for (source = 0u; source != 4u; ++source) {
      uint64_t influence = (uint64_t)UINT32_C(0xff) << (8u * source + amount);
      if ((influence & destination_bits) != 0u &&
          (value.known_mask & (uint8_t)(1u << source)) == 0u)
        known = 0;
    }
    if (known)
      result.known_mask =
          (uint8_t)(result.known_mask | (uint8_t)(1u << destination));
  }
  return result;
}

static Word add_constant(Word value, uint32_t constant) {
  Word result;
  unsigned byte;
  unsigned carry_set = 1u;
  result.word = value.word + constant;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned next_carry_set = 0u;
    unsigned first = 0u;
    int have_first = 0;
    int invariant = 1;
    unsigned carry;
    unsigned low = (value.known_mask & (uint8_t)(1u << byte)) != 0u
                       ? (value.word >> (8u * byte)) & 255u
                       : 0u;
    unsigned high =
        (value.known_mask & (uint8_t)(1u << byte)) != 0u ? low : 255u;
    unsigned addend = (constant >> (8u * byte)) & 255u;
    for (carry = 0u; carry != 2u; ++carry) {
      unsigned input;
      if ((carry_set & (1u << carry)) == 0u)
        continue;
      for (input = low; input <= high; ++input) {
        unsigned sum = input + addend + carry;
        unsigned output = sum & 255u;
        next_carry_set |= 1u << (sum >> 8u);
        if (!have_first) {
          first = output;
          have_first = 1;
        } else if (output != first) {
          invariant = 0;
        }
      }
    }
    if (invariant)
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
    carry_set = next_carry_set;
  }
  return result;
}

static Word and_constant(Word value, uint32_t constant) {
  Word result;
  unsigned byte;
  result.word = value.word & constant;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned mask_byte = (constant >> (8u * byte)) & 255u;
    if (mask_byte == 0u || (value.known_mask & (uint8_t)(1u << byte)) != 0u)
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
  }
  return result;
}

static Word or_words(Word left, Word right) {
  Word result;
  unsigned byte;
  result.word = left.word | right.word;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    uint8_t bit = (uint8_t)(1u << byte);
    unsigned lbyte = (left.word >> (8u * byte)) & 255u;
    unsigned rbyte = (right.word >> (8u * byte)) & 255u;
    int lknown = (left.known_mask & bit) != 0u;
    int rknown = (right.known_mask & bit) != 0u;
    if ((lknown && rknown) || (lknown && lbyte == 255u) ||
        (rknown && rbyte == 255u) || (lknown && lbyte == 0u && rknown) ||
        (rknown && rbyte == 0u && lknown))
      result.known_mask = (uint8_t)(result.known_mask | bit);
  }
  return result;
}

static void unsigned_bounds(Word value, uint32_t *minimum, uint32_t *maximum) {
  unsigned byte;
  *minimum = 0u;
  *maximum = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    uint32_t shift = 8u * byte;
    if ((value.known_mask & (uint8_t)(1u << byte)) != 0u) {
      uint32_t fixed = value.word & (UINT32_C(0xff) << shift);
      *minimum |= fixed;
      *maximum |= fixed;
    } else {
      *maximum |= UINT32_C(0xff) << shift;
    }
  }
}

static void biased_signed_bounds(Word value, uint32_t *minimum,
                                 uint32_t *maximum) {
  Word biased = value;
  biased.word ^= UINT32_C(0x80000000);
  unsigned_bounds(biased, minimum, maximum);
}

static enum Decision negative_decision(Word value) {
  if ((value.known_mask & 8u) == 0u)
    return DECISION_UNKNOWN;
  return (value.word & UINT32_C(0x80000000)) != 0u ? DECISION_TRUE
                                                   : DECISION_FALSE;
}

static Word signed_less(Word left, Word right) {
  Word result;
  uint32_t lmin, lmax, rmin, rmax;
  result.word =
      ((left.word ^ UINT32_C(0x80000000)) < (right.word ^ UINT32_C(0x80000000)))
          ? 1u
          : 0u;
  result.known_mask = 14u;
  biased_signed_bounds(left, &lmin, &lmax);
  biased_signed_bounds(right, &rmin, &rmax);
  if (lmax < rmin || lmin >= rmax)
    result.known_mask = 15u;
  return result;
}

static Word unsigned_less_constant(Word value, uint32_t constant) {
  Word result;
  uint32_t minimum, maximum;
  result.word = value.word < constant ? 1u : 0u;
  result.known_mask = 14u;
  unsigned_bounds(value, &minimum, &maximum);
  if (maximum < constant || minimum >= constant)
    result.known_mask = 15u;
  return result;
}

static enum Decision zero_decision(Word value) {
  unsigned byte;
  for (byte = 0u; byte != 4u; ++byte) {
    uint8_t bit = (uint8_t)(1u << byte);
    if ((value.known_mask & bit) != 0u &&
        ((value.word >> (8u * byte)) & 255u) != 0u)
      return DECISION_FALSE;
  }
  if (value.known_mask == 15u)
    return DECISION_TRUE;
  return DECISION_UNKNOWN;
}

static void publish(Run *run) {
  run->progress->machine = run->machine;
  run->progress->return_v0 = R(NBA97_MATCH_INITIALIZE_V0);
}

static void stop(Run *run, uint32_t pc, uint32_t address) {
  run->progress->stopped_pc = pc;
  run->progress->stopped_address = address;
  publish(run);
}

static void journal_read(Run *run, uint32_t pc, uint32_t address,
                         unsigned width, Word value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameDrawAreaEndAccess *event = &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word;
    event->operation = run->progress->operations;
    event->width = (uint8_t)width;
    event->known_mask = value.known_mask;
    event->kind = NBA97_GAME_MATCH_CLOCKS_READ;
  }
}

static int read_fixed(Run *run, uint32_t pc, uint32_t address, unsigned width,
                      Word *destination) {
  Word loaded = known_word(0u);
  size_t index;
  unsigned byte;
  loaded.known_mask = width == 1u ? 14u : 12u;
  stop(run, pc, address);
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  ++run->progress->accesses;
  for (index = 0u; index != run->context->memory.count; ++index) {
    Nba97GameTextRegion *region = &run->context->memory.region[index];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        width > region->size - (size_t)offset)
      continue;
    for (byte = 0u; byte != width; ++byte)
      if (region->known != NULL && region->known[(size_t)offset + byte] > 1u)
        return NBA97_TEXT_ARGUMENT;
    for (byte = 0u; byte != width; ++byte) {
      uint8_t known =
          region->known == NULL ? 1u : region->known[(size_t)offset + byte];
      loaded.word |= (uint32_t)region->data[(size_t)offset + byte]
                     << (8u * byte);
      if (known)
        loaded.known_mask =
            (uint8_t)(loaded.known_mask | (uint8_t)(1u << byte));
    }
    *destination = loaded;
    ++run->progress->reads;
    journal_read(run, pc, address, width, loaded);
    publish(run);
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

int nba97_game_draw_area_end(Nba97GameDrawAreaEndContext *context,
                             Nba97GameDrawAreaEndProgress *progress) {
  Run storage;
  Run *run = &storage;
  enum Decision condition;
  int status;
  if (progress == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof(*progress));
  if (context == NULL)
    return NBA97_TEXT_ARGUMENT;
  run->context = context;
  run->progress = progress;
  run->machine = context->machine;
  publish(run);
  if (!valid_machine(&context->machine) || !valid_memory(&context->memory) ||
      (context->access_journal_capacity != 0u &&
       context->access_journal == NULL))
    return NBA97_TEXT_ARGUMENT;

  /* 0x8009A710..0x8009A750: sign-extend x. A negative x clears v0 in the
   * BLTZ delay and skips the x-limit read; otherwise clamp to limit - 1. */
  R(NBA97_MATCH_INITIALIZE_A0) = sign_extend_half(R(NBA97_MATCH_INITIALIZE_A0));
  condition = negative_decision(R(NBA97_MATCH_INITIALIZE_A0));
  R(NBA97_MATCH_INITIALIZE_V0) = known_word(0u);
  if (condition == DECISION_UNKNOWN) {
    stop(run, UINT32_C(0x8009a718), 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (condition == DECISION_FALSE) {
    R(NBA97_MATCH_INITIALIZE_V0) = known_word(UINT32_C(0x800c0000));
    R(NBA97_MATCH_INITIALIZE_V0) = known_word(UINT32_C(0x800c55c4));
    status = read_fixed(run, UINT32_C(0x8009a728), UINT32_C(0x800c55c4), 2u,
                        &R(NBA97_MATCH_INITIALIZE_V0));
    if (status != NBA97_TEXT_COMPLETE)
      return status;
    R(NBA97_MATCH_INITIALIZE_V0) =
        sign_extend_half(R(NBA97_MATCH_INITIALIZE_V0));
    R(NBA97_MATCH_INITIALIZE_A2) =
        add_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xffffffff));
    R(NBA97_MATCH_INITIALIZE_V0) =
        signed_less(R(NBA97_MATCH_INITIALIZE_A2), R(NBA97_MATCH_INITIALIZE_A0));
    condition = zero_decision(R(NBA97_MATCH_INITIALIZE_V0));
    R(NBA97_MATCH_INITIALIZE_V0) = R(NBA97_MATCH_INITIALIZE_A0);
    if (condition == DECISION_UNKNOWN) {
      stop(run, UINT32_C(0x8009a740), 0u);
      return NBA97_TEXT_UNKNOWN;
    }
    if (condition == DECISION_FALSE) {
      R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_A2);
      R(NBA97_MATCH_INITIALIZE_V0) = R(NBA97_MATCH_INITIALIZE_A0);
    }
  }
  R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_V0);

  /* 0x8009A754..0x8009A790: retain the explicit shifted v0, sign-extend y
   * into v1, clear a1 in the BLTZ delay, and conditionally clamp y. */
  R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_A1), 16u);
  R(NBA97_MATCH_INITIALIZE_V1) = sign_extend_half(R(NBA97_MATCH_INITIALIZE_A1));
  condition = negative_decision(R(NBA97_MATCH_INITIALIZE_V1));
  R(NBA97_MATCH_INITIALIZE_A1) = known_word(0u);
  if (condition == DECISION_UNKNOWN) {
    stop(run, UINT32_C(0x8009a75c), 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (condition == DECISION_FALSE) {
    R(NBA97_MATCH_INITIALIZE_V0) = known_word(UINT32_C(0x800c0000));
    R(NBA97_MATCH_INITIALIZE_V0) = known_word(UINT32_C(0x800c55c6));
    status = read_fixed(run, UINT32_C(0x8009a76c), UINT32_C(0x800c55c6), 2u,
                        &R(NBA97_MATCH_INITIALIZE_V0));
    if (status != NBA97_TEXT_COMPLETE)
      return status;
    R(NBA97_MATCH_INITIALIZE_V0) =
        sign_extend_half(R(NBA97_MATCH_INITIALIZE_V0));
    R(NBA97_MATCH_INITIALIZE_A1) =
        add_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xffffffff));
    R(NBA97_MATCH_INITIALIZE_V0) =
        signed_less(R(NBA97_MATCH_INITIALIZE_A1), R(NBA97_MATCH_INITIALIZE_V1));
    condition = zero_decision(R(NBA97_MATCH_INITIALIZE_V0));
    if (condition == DECISION_UNKNOWN) {
      stop(run, UINT32_C(0x8009a784), 0u);
      return NBA97_TEXT_UNKNOWN;
    }
    if (condition == DECISION_FALSE)
      R(NBA97_MATCH_INITIALIZE_V1) = R(NBA97_MATCH_INITIALIZE_A1);
    R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_V1);
  }

  /* 0x8009A794..0x8009A7C8: type bytes one and two select 12-bit fields;
   * every other type uses 10-bit fields. The BNE delay always masks y. */
  R(NBA97_MATCH_INITIALIZE_V0) = known_word(UINT32_C(0x800c0000));
  R(NBA97_MATCH_INITIALIZE_V0) = known_word(UINT32_C(0x800c55c0));
  status = read_fixed(run, UINT32_C(0x8009a79c), UINT32_C(0x800c55c0), 1u,
                      &R(NBA97_MATCH_INITIALIZE_V0));
  if (status != NBA97_TEXT_COMPLETE)
    return status;
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xffffffff));
  R(NBA97_MATCH_INITIALIZE_V0) =
      unsigned_less_constant(R(NBA97_MATCH_INITIALIZE_V0), 2u);
  condition = zero_decision(R(NBA97_MATCH_INITIALIZE_V0));
  R(NBA97_MATCH_INITIALIZE_V1) =
      and_constant(R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x00000fff));
  if (condition == DECISION_UNKNOWN) {
    stop(run, UINT32_C(0x8009a7ac), 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (condition == DECISION_FALSE) {
    R(NBA97_MATCH_INITIALIZE_V1) =
        shift_left(R(NBA97_MATCH_INITIALIZE_V1), 12u);
    R(NBA97_MATCH_INITIALIZE_V0) =
        and_constant(R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x00000fff));
  } else {
    R(NBA97_MATCH_INITIALIZE_V1) =
        and_constant(R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x000003ff));
    R(NBA97_MATCH_INITIALIZE_V1) =
        shift_left(R(NBA97_MATCH_INITIALIZE_V1), 10u);
    R(NBA97_MATCH_INITIALIZE_V0) =
        and_constant(R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x000003ff));
  }

  /* 0x8009A7CC..0x8009A7D8: form E4, then execute the final OR in JR's
   *
   * delay slot before requiring a known live return address. */
  R(NBA97_MATCH_INITIALIZE_A0) = known_word(UINT32_C(0xe4000000));
  R(NBA97_MATCH_INITIALIZE_V0) =
      or_words(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_A0));
  R(NBA97_MATCH_INITIALIZE_V0) =
      or_words(R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0));
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15u) {
    stop(run, UINT32_C(0x8009a7d4), 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  progress->completed = 1u;
  progress->stopped_pc = 0u;
  progress->stopped_address = 0u;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

#undef R
