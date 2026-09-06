#include "game_camera_state_lookup.h"

#include <string.h>

typedef Nba97GameCameraStateLookupWord Word;

typedef struct Run {
  Nba97GameCameraStateLookupContext *context;
  Nba97GameCameraStateLookupProgress *out;
  Nba97GameCameraStateLookupMachine machine;
} Run;

typedef struct RefinedPath {
  Word biased;
  Word index;
  Word offset;
  Word base_register;
  Word address;
} RefinedPath;

#define R(index_) (run->machine.registers.gpr[(index_)])
#define AT R(1)
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

static int valid_machine(const Nba97GameCameraStateLookupMachine *machine) {
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

static int initialize(Nba97GameCameraStateLookupContext *context,
                      Nba97GameCameraStateLookupProgress *out, Run *run) {
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
    Nba97GameCameraStateLookupAccess *event =
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

static Word shift_left_four(Word value) {
  Word result;
  result.word = value.word << 4u;
  result.known_mask = 0u;
  if (value.known_mask & 1u)
    result.known_mask |= 1u;
  if ((value.known_mask & 3u) == 3u)
    result.known_mask |= 2u;
  if ((value.known_mask & 6u) == 6u)
    result.known_mask |= 4u;
  if ((value.known_mask & 12u) == 12u)
    result.known_mask |= 8u;
  return result;
}

static Word mask_index_bits(Word source, Word shifted) {
  Word result;
  result.word = shifted.word & UINT32_C(0xfffff000);
  result.known_mask = 1u;
  if (source.known_mask & 2u)
    result.known_mask |= 2u;
  if ((source.known_mask & 6u) == 6u)
    result.known_mask |= 4u;
  if ((source.known_mask & 12u) == 12u)
    result.known_mask |= 8u;
  return result;
}

static uint32_t arithmetic_right_twelve(uint32_t value) {
  uint32_t result = value >> 12u;
  if (value & UINT32_C(0x80000000))
    result |= UINT32_C(0xfff00000);
  return result;
}

static uint8_t invariant_mask(uint32_t first, uint32_t value, uint8_t current) {
  unsigned byte;
  for (byte = 0u; byte != 4u; ++byte)
    if (((first ^ value) >> (byte * 8u)) & 255u)
      current = (uint8_t)(current & (uint8_t) ~(1u << byte));
  return current;
}

/* Once BLTZ is decidable, byte3 is known. Enumerating source bytes1 and2
 * proves every invariant byte through the bias, SRA, SLL, and address add. */
static RefinedPath refine_path(Word source, int negative) {
  RefinedPath result;
  uint32_t first_biased = 0u;
  uint32_t first_index = 0u;
  uint32_t first_offset = 0u;
  uint32_t first_base = 0u;
  uint32_t first_address = 0u;
  uint8_t biased_mask = 15u;
  uint8_t index_mask = 15u;
  uint8_t offset_mask = 15u;
  uint8_t base_mask = 15u;
  uint8_t address_mask = 15u;
  unsigned byte1_start =
      (source.known_mask & 2u) ? (source.word >> 8u) & 255u : 0u;
  unsigned byte1_end = (source.known_mask & 2u) ? byte1_start : 255u;
  unsigned byte2_start =
      (source.known_mask & 4u) ? (source.word >> 16u) & 255u : 0u;
  unsigned byte2_end = (source.known_mask & 4u) ? byte2_start : 255u;
  unsigned byte1;
  int first = 1;
  for (byte1 = byte1_start; byte1 <= byte1_end; ++byte1) {
    unsigned byte2;
    for (byte2 = byte2_start; byte2 <= byte2_end; ++byte2) {
      uint32_t candidate = source.word & UINT32_C(0xff0000ff);
      uint32_t masked;
      uint32_t index;
      uint32_t offset;
      uint32_t base;
      uint32_t address;
      candidate |= byte1 << 8u;
      candidate |= byte2 << 16u;
      masked = (candidate << 4u) & UINT32_C(0xfffff000);
      if (negative)
        masked += UINT32_C(0x00008000);
      index = arithmetic_right_twelve(masked);
      offset = index << 2u;
      base = UINT32_C(0x800c0000) + offset;
      address = base + (negative ? UINT32_C(0xffffc224) : UINT32_C(0xffffc204));
      if (first) {
        first_biased = masked;
        first_index = index;
        first_offset = offset;
        first_base = base;
        first_address = address;
        first = 0;
      } else {
        biased_mask = invariant_mask(first_biased, masked, biased_mask);
        index_mask = invariant_mask(first_index, index, index_mask);
        offset_mask = invariant_mask(first_offset, offset, offset_mask);
        base_mask = invariant_mask(first_base, base, base_mask);
        address_mask = invariant_mask(first_address, address, address_mask);
      }
    }
  }
  result.biased.word = ((source.word << 4u) & UINT32_C(0xfffff000)) +
                       (negative ? UINT32_C(0x00008000) : 0u);
  result.index.word = arithmetic_right_twelve(result.biased.word);
  result.offset.word = result.index.word << 2u;
  result.base_register.word = UINT32_C(0x800c0000) + result.offset.word;
  result.address.word =
      result.base_register.word +
      (negative ? UINT32_C(0xffffc224) : UINT32_C(0xffffc204));
  result.biased.known_mask = biased_mask;
  result.index.known_mask = index_mask;
  result.offset.known_mask = offset_mask;
  result.base_register.known_mask = base_mask;
  result.address.known_mask = address_mask;
  return result;
}

int nba97_game_camera_state_lookup(Nba97GameCameraStateLookupContext *context,
                                   Nba97GameCameraStateLookupProgress *out) {
  Run storage;
  Run *run = &storage;
  Word source;
  Word shifted;
  RefinedPath path;
  int negative;
  TRY(initialize(context, out, run));

  /* 0x8007A410..0x8007A428: load the source while the delay initializes v1,
   * discard bits outside 8..27, then always set v0=0x8000 in BLTZ's delay. */
  STEP(0x8007a410);
  known(&V0, UINT32_C(0x80100000));
  STEP(0x8007a414);
  TRY(read_word(run, UINT32_C(0x800fc9ac), UINT32_C(0x8007a414), &V0));
  source = V0;
  out->source_value = source;
  STEP(0x8007a418);
  known(&V1, UINT32_C(0xfffff000));
  STEP(0x8007a41c);
  shifted = shift_left_four(V0);
  V0 = shifted;
  STEP(0x8007a420);
  V1 = mask_index_bits(source, shifted);
  out->masked_value = V1;
  STEP(0x8007a424);
  STEP(0x8007a428);
  known(&V0, UINT32_C(0x00008000));
  /* BLTZ consumes only source bit27, which is byte3 bit3 after SLL. */
  if (!(source.known_mask & 8u)) {
    stop(run, UINT32_C(0x8007a424), 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  negative = (V1.word & UINT32_C(0x80000000)) != 0u;
  out->negative_table = (uint8_t)negative;
  path = refine_path(source, negative);

  if (!negative) {
    /* 0x8007A42C..0x8007A444: positive indices use the BC204 table; the jump
     * NOP executes only after its unchecked mapped table load succeeds. */
    STEP(0x8007a42c);
    V1 = path.index;
    out->signed_index = V1;
    STEP(0x8007a430);
    V0 = path.offset;
    STEP(0x8007a434);
    known(&AT, UINT32_C(0x800c0000));
    STEP(0x8007a438);
    AT = path.base_register;
    out->lookup_address = path.address;
    STEP(0x8007a43c);
    if (path.address.known_mask != 15u) {
      stop(run, UINT32_C(0x8007a43c), path.address.word);
      return NBA97_TEXT_UNKNOWN;
    }
    TRY(read_word(run, path.address.word, UINT32_C(0x8007a43c), &V0));
    STEP(0x8007a440);
    STEP(0x8007a444);
  } else {
    /* 0x8007A448..0x8007A45C: negative indices add the delay's 0x8000 with
     * wrap before SRA and use the separately biased BC224 table. */
    STEP(0x8007a448);
    V1 = path.biased;
    STEP(0x8007a44c);
    V1 = path.index;
    out->signed_index = V1;
    STEP(0x8007a450);
    V0 = path.offset;
    STEP(0x8007a454);
    known(&AT, UINT32_C(0x800c0000));
    STEP(0x8007a458);
    AT = path.base_register;
    out->lookup_address = path.address;
    STEP(0x8007a45c);
    if (path.address.known_mask != 15u) {
      stop(run, UINT32_C(0x8007a45c), path.address.word);
      return NBA97_TEXT_UNKNOWN;
    }
    TRY(read_word(run, path.address.word, UINT32_C(0x8007a45c), &V0));
  }

  /* 0x8007A460..0x8007A464: return the raw table word after JR's NOP. */
  out->returned_value = V0;
  STEP(0x8007a460);
  STEP(0x8007a464);
  if (RA.known_mask != 15u) {
    stop(run, UINT32_C(0x8007a460), RA.word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, UINT32_C(0x8007a460), RA.word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1u;
  stop(run, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
