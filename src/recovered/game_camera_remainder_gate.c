#include "game_camera_remainder_gate.h"

#include <string.h>

typedef Nba97GameCameraRemainderGateWord Word;

typedef struct Run {
  Nba97GameCameraRemainderGateContext *context;
  Nba97GameCameraRemainderGateProgress *out;
  Nba97GameCameraRemainderGateMachine machine;
} Run;

typedef struct RefinedArithmetic {
  Word adjusted;
  Word remainder;
  Word result;
} RefinedArithmetic;

#define R(index_) (run->machine.registers.gpr[(index_)])
#define V0 R(2)
#define V1 R(3)
#define RA R(31)
#define TRY(expression_)                                                       \
  do {                                                                         \
    int result_ = (expression_);                                               \
    if (result_ != NBA97_TEXT_COMPLETE)                                        \
      return result_;                                                          \
  } while (0)
#define STEP(pc_)                                                              \
  do {                                                                         \
    (void)(pc_);                                                               \
    ++run->out->instruction_count;                                             \
  } while (0)

static void known(Word *value, uint32_t word) {
  value->word = word;
  value->known_mask = 15u;
}

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  publish(run);
}

static int valid_machine(const Nba97GameCameraRemainderGateMachine *machine) {
  unsigned index;
  if (machine->registers.gpr[0].word != 0u ||
      machine->registers.gpr[0].known_mask != 15u ||
      machine->hi.known_mask > 15u || machine->lo.known_mask > 15u)
    return 0;
  for (index = 0u; index != 32u; ++index)
    if (machine->registers.gpr[index].known_mask > 15u)
      return 0;
  return 1;
}

static int initialize(Nba97GameCameraRemainderGateContext *context,
                      Nba97GameCameraRemainderGateProgress *out, Run *run) {
  size_t index;
  size_t earlier;
  if (out == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(out, 0, sizeof(*out));
  if (context == NULL ||
      (context->memory.count != 0u && context->memory.region == NULL) ||
      (context->access_journal_capacity != 0u &&
       context->access_journal == NULL) ||
      !valid_machine(&context->machine))
    return NBA97_TEXT_ARGUMENT;
  for (index = 0u; index != context->memory.count; ++index) {
    const Nba97GameTextRegion *region = &context->memory.region[index];
    if (region->data == NULL || region->size == 0u ||
        (uint64_t)region->size > UINT64_C(0x100000000) ||
        (uint64_t)region->base + (uint64_t)region->size > UINT64_C(0x100000000))
      return NBA97_TEXT_ARGUMENT;
    for (earlier = 0u; earlier != index; ++earlier) {
      const Nba97GameTextRegion *other = &context->memory.region[earlier];
      if ((uint64_t)region->base < (uint64_t)other->base + other->size &&
          (uint64_t)other->base < (uint64_t)region->base + region->size)
        return NBA97_TEXT_ARGUMENT;
    }
  }
  run->context = context;
  run->out = out;
  run->machine = context->machine;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint32_t pc, uint32_t address, Word value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameCameraRemainderGateAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word;
    event->operation = run->out->operations;
    event->width = 4u;
    event->known_mask = value.known_mask;
    event->kind = NBA97_GAME_MATCH_CLOCKS_READ;
  }
}

static int read_word(Run *run, uint32_t address, uint32_t pc, Word *result) {
  size_t index;
  size_t byte;
  uint8_t *data = NULL;
  uint8_t *known_bytes = NULL;
  Word loaded = {0u, 0u};
  stop(run, pc, address);
  TRY(spend(run));
  ++run->out->accesses;
  if (address & 3u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (index = 0u; index != run->context->memory.count; ++index) {
    Nba97GameTextRegion *region = &run->context->memory.region[index];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        4u > region->size - (size_t)offset)
      continue;
    data = region->data + (size_t)offset;
    known_bytes = region->known == NULL ? NULL : region->known + (size_t)offset;
    if (known_bytes != NULL)
      for (byte = 0u; byte != 4u; ++byte)
        if (known_bytes[byte] > 1u)
          return NBA97_TEXT_ARGUMENT;
    break;
  }
  if (data == NULL)
    return NBA97_TEXT_RESOURCE;
  for (byte = 0u; byte != 4u; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (byte * 8u);
    if (known_bytes == NULL || known_bytes[byte])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << byte));
  }
  ++run->out->reads;
  journal(run, pc, address, loaded);
  *result = loaded;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static uint32_t arithmetic_right_eleven(uint32_t value) {
  uint32_t result = value >> 11u;
  if (value & UINT32_C(0x80000000))
    result |= UINT32_C(0xffe00000);
  return result;
}

static uint32_t source_result(uint32_t source, int negative, uint32_t *adjusted,
                              uint32_t *remainder) {
  uint32_t value = negative ? source + UINT32_C(0x7ff) : source;
  uint32_t multiple = arithmetic_right_eleven(value) << 11u;
  uint32_t rem = source - multiple;
  *adjusted = value;
  *remainder = rem;
  return rem + UINT32_C(50) < UINT32_C(101) ? 1u : 0u;
}

static uint8_t invariant_mask(uint32_t first, uint32_t value, uint8_t current) {
  unsigned byte;
  for (byte = 0u; byte != 4u; ++byte)
    if (((first ^ value) >> (byte * 8u)) & 255u)
      current = (uint8_t)(current & (uint8_t) ~(1u << byte));
  return current;
}

static Word add_constant(Word input, uint32_t constant) {
  Word result;
  unsigned byte;
  unsigned carry_mask = 1u;
  result.word = input.word + constant;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned carry;
    unsigned first_output = 0u;
    unsigned next_carry_mask = 0u;
    int first = 1;
    int invariant = 1;
    for (carry = 0u; carry <= 1u; ++carry) {
      unsigned input_start;
      unsigned input_end;
      unsigned candidate;
      if (!(carry_mask & (1u << carry)))
        continue;
      input_start = (input.known_mask & (1u << byte))
                        ? (input.word >> (byte * 8u)) & 255u
                        : 0u;
      input_end = (input.known_mask & (1u << byte)) ? input_start : 255u;
      for (candidate = input_start; candidate <= input_end; ++candidate) {
        unsigned sum = candidate + ((constant >> (byte * 8u)) & 255u) + carry;
        unsigned output = sum & 255u;
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

static Word shift_right_eleven(Word input) {
  Word result;
  result.word = arithmetic_right_eleven(input.word);
  result.known_mask = 0u;
  if ((input.known_mask & 6u) == 6u)
    result.known_mask |= 1u;
  if ((input.known_mask & 12u) == 12u)
    result.known_mask |= 2u;
  if (input.known_mask & 8u)
    result.known_mask |= 12u;
  return result;
}

static Word shift_left_eleven(Word input) {
  Word result;
  result.word = input.word << 11u;
  result.known_mask = 1u;
  if (input.known_mask & 1u)
    result.known_mask |= 2u;
  if ((input.known_mask & 3u) == 3u)
    result.known_mask |= 4u;
  if ((input.known_mask & 6u) == 6u)
    result.known_mask |= 8u;
  return result;
}

/* The remainder and predicate consume only the source sign and low eleven
 * bits. Enumerating those bits proves all invariant result bytes. */
static RefinedArithmetic refine_arithmetic(Word source, int negative) {
  RefinedArithmetic result;
  unsigned byte0_start = (source.known_mask & 1u) ? source.word & 255u : 0u;
  unsigned byte0_end = (source.known_mask & 1u) ? byte0_start : 255u;
  unsigned low3_start =
      (source.known_mask & 2u) ? (source.word >> 8u) & 7u : 0u;
  unsigned low3_end = (source.known_mask & 2u) ? low3_start : 7u;
  uint32_t first_remainder = 0u;
  uint32_t first_result = 0u;
  uint8_t remainder_mask = 15u;
  int result_invariant = 1;
  unsigned byte0;
  int first = 1;
  uint32_t adjusted;
  uint32_t remainder;
  result.result.word =
      source_result(source.word, negative, &adjusted, &remainder);
  result.adjusted = negative ? add_constant(source, UINT32_C(0x7ff)) : source;
  result.remainder.word = remainder;
  result.result.known_mask = 14u;
  for (byte0 = byte0_start; byte0 <= byte0_end; ++byte0) {
    unsigned low3;
    for (low3 = low3_start; low3 <= low3_end; ++low3) {
      uint32_t candidate =
          (source.word & UINT32_C(0xfffff800)) | byte0 | (low3 << 8u);
      unsigned candidate_result =
          source_result(candidate, negative, &adjusted, &remainder);
      if (first) {
        first_remainder = remainder;
        first_result = candidate_result;
        first = 0;
      } else {
        remainder_mask =
            invariant_mask(first_remainder, remainder, remainder_mask);
        if (candidate_result != first_result)
          result_invariant = 0;
      }
    }
  }
  result.remainder.known_mask = remainder_mask;
  if (result_invariant)
    result.result.known_mask = 15u;
  return result;
}

int nba97_game_camera_remainder_gate(
    Nba97GameCameraRemainderGateContext *context,
    Nba97GameCameraRemainderGateProgress *out) {
  Run storage;
  Run *run = &storage;
  Word source;
  RefinedArithmetic arithmetic;
  int negative;
  TRY(initialize(context, out, run));

  /* 0x8007A468..0x8007A478: load the signed source and copy it to v0 in
   * BGEZ's delay before the source sign is required. */
  STEP(0x8007a468);
  known(&V1, UINT32_C(0x80100000));
  STEP(0x8007a46c);
  TRY(read_word(run, UINT32_C(0x800fc9ac), UINT32_C(0x8007a46c), &V1));
  source = V1;
  out->source_value = source;
  STEP(0x8007a470);
  STEP(0x8007a474);
  STEP(0x8007a478);
  V0 = V1;
  if (!(source.known_mask & 8u)) {
    stop(run, UINT32_C(0x8007a474), 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  negative = (source.word & UINT32_C(0x80000000)) != 0u;
  out->negative = (uint8_t)negative;
  arithmetic = refine_arithmetic(source, negative);

  /* 0x8007A47C: negative values add 0x7FF with wrap so the following SRA
   * implements signed division truncated toward zero. */
  if (negative) {
    STEP(0x8007a47c);
    V0 = arithmetic.adjusted;
  }
  out->adjusted_value = arithmetic.adjusted;

  /* 0x8007A480..0x8007A48C: reconstruct the signed multiple of 2048,
   * subtract it from the original source, and bias the remainder by 50. */
  STEP(0x8007a480);
  V0 = shift_right_eleven(V0);
  STEP(0x8007a484);
  V0 = shift_left_eleven(V0);
  STEP(0x8007a488);
  V0 = arithmetic.remainder;
  out->remainder_value = V0;
  STEP(0x8007a48c);
  V0 = add_constant(V0, UINT32_C(50));

  /* 0x8007A490..0x8007A494: JR's delay tests the biased remainder <101
   * before an unknown or unaligned return address can stop the routine. */
  STEP(0x8007a490);
  STEP(0x8007a494);
  V0 = arithmetic.result;
  out->returned_value = V0;
  if (RA.known_mask != 15u) {
    stop(run, UINT32_C(0x8007a490), RA.word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, UINT32_C(0x8007a490), RA.word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1u;
  stop(run, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
