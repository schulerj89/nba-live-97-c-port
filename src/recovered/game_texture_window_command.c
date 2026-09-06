#include "game_texture_window_command.h"

#include <string.h>

typedef struct Run {
  Nba97GameTextureWindowCommandContext *context;
  Nba97GameTextureWindowCommandProgress *progress;
  Nba97GameTextureWindowCommandMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int status_ = (expression);                                                \
    if (status_ != NBA97_TEXT_COMPLETE)                                        \
      return status_;                                                          \
  } while (0)

static void publish(Run *run) { run->progress->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address) {
  run->progress->stopped_pc = pc;
  run->progress->stopped_address = address;
  publish(run);
}

static void set_known(Nba97GameTextureWindowCommandWord *value,
                      uint32_t word) {
  value->word = word;
  value->known_mask = 15u;
}

static Nba97GameTextureWindowCommandWord known_word(uint32_t word) {
  Nba97GameTextureWindowCommandWord value;
  set_known(&value, word);
  return value;
}

static int valid_machine(const Nba97GameTextureWindowCommandMachine *machine) {
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

static int initialize(Nba97GameTextureWindowCommandContext *context,
                      Nba97GameTextureWindowCommandProgress *progress,
                      Run *run) {
  if (progress == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof(*progress));
  if (context == NULL || !valid_memory(&context->memory) ||
      !valid_machine(&context->machine) ||
      (context->access_journal_capacity != 0u &&
       context->access_journal == NULL))
    return NBA97_TEXT_ARGUMENT;
  run->context = context;
  run->progress = progress;
  run->machine = context->machine;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static Nba97GameTextureWindowCommandWord
add_words(Nba97GameTextureWindowCommandWord left,
          Nba97GameTextureWindowCommandWord right) {
  Nba97GameTextureWindowCommandWord result;
  unsigned carry_mask = 1u;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned next_carry_mask = 0u;
    unsigned first_output = 0u;
    unsigned first = 1u;
    unsigned invariant = 1u;
    unsigned left_start = (left.known_mask & (1u << byte))
                              ? (left.word >> (8u * byte)) & 255u
                              : 0u;
    unsigned left_end =
        (left.known_mask & (1u << byte)) ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte))
                               ? (right.word >> (8u * byte)) & 255u
                               : 0u;
    unsigned right_end =
        (right.known_mask & (1u << byte)) ? right_start : 255u;
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
          if (first) {
            first_output = output;
            first = 0u;
          } else if (first_output != output) {
            invariant = 0u;
          }
        }
      }
    }
    if (invariant)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
    carry_mask = next_carry_mask;
  }
  return result;
}

static Nba97GameTextureWindowCommandWord
subtract_words(Nba97GameTextureWindowCommandWord left,
               Nba97GameTextureWindowCommandWord right) {
  Nba97GameTextureWindowCommandWord result;
  unsigned borrow_mask = 1u;
  unsigned byte;
  result.word = left.word - right.word;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned next_borrow_mask = 0u;
    unsigned first_output = 0u;
    unsigned first = 1u;
    unsigned invariant = 1u;
    unsigned left_start = (left.known_mask & (1u << byte))
                              ? (left.word >> (8u * byte)) & 255u
                              : 0u;
    unsigned left_end =
        (left.known_mask & (1u << byte)) ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte))
                               ? (right.word >> (8u * byte)) & 255u
                               : 0u;
    unsigned right_end =
        (right.known_mask & (1u << byte)) ? right_start : 255u;
    unsigned borrow;
    for (borrow = 0u; borrow != 2u; ++borrow) {
      unsigned a;
      if ((borrow_mask & (1u << borrow)) == 0u)
        continue;
      for (a = left_start; a <= left_end; ++a) {
        unsigned b;
        for (b = right_start; b <= right_end; ++b) {
          unsigned subtrahend = b + borrow;
          unsigned output = (a - subtrahend) & 255u;
          next_borrow_mask |= 1u << (a < subtrahend);
          if (first) {
            first_output = output;
            first = 0u;
          } else if (first_output != output) {
            invariant = 0u;
          }
        }
      }
    }
    if (invariant)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
    borrow_mask = next_borrow_mask;
  }
  return result;
}

static Nba97GameTextureWindowCommandWord
add_constant(Nba97GameTextureWindowCommandWord value, uint32_t constant) {
  return add_words(value, known_word(constant));
}

static Nba97GameTextureWindowCommandWord
and_constant(Nba97GameTextureWindowCommandWord value, uint32_t constant) {
  Nba97GameTextureWindowCommandWord result;
  unsigned byte;
  result.word = value.word & constant;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned bit = 1u << byte;
    unsigned constant_byte = (constant >> (8u * byte)) & 255u;
    if ((value.known_mask & bit) != 0u || constant_byte == 0u)
      result.known_mask = (uint8_t)(result.known_mask | bit);
  }
  return result;
}

static Nba97GameTextureWindowCommandWord
or_words(Nba97GameTextureWindowCommandWord left,
         Nba97GameTextureWindowCommandWord right) {
  Nba97GameTextureWindowCommandWord result;
  unsigned byte;
  result.word = left.word | right.word;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned bit = 1u << byte;
    unsigned l = (left.word >> (8u * byte)) & 255u;
    unsigned r = (right.word >> (8u * byte)) & 255u;
    if (((left.known_mask & right.known_mask & bit) != 0u) ||
        ((left.known_mask & bit) != 0u && l == 255u) ||
        ((right.known_mask & bit) != 0u && r == 255u))
      result.known_mask = (uint8_t)(result.known_mask | bit);
  }
  return result;
}

static Nba97GameTextureWindowCommandWord
shift_left(Nba97GameTextureWindowCommandWord value, unsigned shift) {
  Nba97GameTextureWindowCommandWord result;
  unsigned byte, bit;
  result.word = value.word << shift;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned known = 1u;
    for (bit = byte * 8u; bit != byte * 8u + 8u; ++bit)
      if (bit >= shift &&
          (value.known_mask & (1u << ((bit - shift) / 8u))) == 0u)
        known = 0u;
    if (known)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static Nba97GameTextureWindowCommandWord
shift_right_logical(Nba97GameTextureWindowCommandWord value, unsigned shift) {
  Nba97GameTextureWindowCommandWord result;
  unsigned byte, bit;
  result.word = value.word >> shift;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned known = 1u;
    for (bit = byte * 8u; bit != byte * 8u + 8u; ++bit)
      if (bit + shift < 32u &&
          (value.known_mask & (1u << ((bit + shift) / 8u))) == 0u)
        known = 0u;
    if (known)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static Nba97GameTextureWindowCommandWord
shift_right_arithmetic(Nba97GameTextureWindowCommandWord value,
                       unsigned shift) {
  Nba97GameTextureWindowCommandWord result;
  unsigned byte, bit;
  result.word = value.word >> shift;
  if (shift != 0u && (value.word & UINT32_C(0x80000000)) != 0u)
    result.word |= UINT32_MAX << (32u - shift);
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned known = 1u;
    for (bit = byte * 8u; bit != byte * 8u + 8u; ++bit) {
      unsigned source_bit = bit + shift;
      unsigned source_byte = source_bit < 32u ? source_bit / 8u : 3u;
      if ((value.known_mask & (1u << source_byte)) == 0u)
        known = 0u;
    }
    if (known)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static int spend(Run *run) {
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  return NBA97_TEXT_COMPLETE;
}

static uint8_t width_mask(unsigned width) {
  return (uint8_t)((1u << width) - 1u);
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width,
                    const Nba97GameTextureWindowCommandWord *value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameTextureWindowCommandAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word;
    event->operation = run->progress->operations;
    event->width = (uint8_t)width;
    event->known_mask = (uint8_t)(value->known_mask & width_mask(width));
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t pc, uint32_t address, unsigned width,
                  uint8_t **data, uint8_t **known) {
  size_t index, byte;
  stop(run, pc, address);
  TRY(spend(run));
  ++run->progress->accesses;
  if ((address & (width - 1u)) != 0u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (index = 0u; index != run->context->memory.count; ++index) {
    Nba97GameTextRegion *region = &run->context->memory.region[index];
    uint64_t offset = (uint64_t)address - region->base;
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

static int address(Run *run, Nba97GameTextureWindowCommandWord base,
                   uint32_t offset, uint32_t pc, uint32_t *result) {
  Nba97GameTextureWindowCommandWord sum = add_constant(base, offset);
  if (sum.known_mask != 15u) {
    stop(run, pc, sum.word);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = sum.word;
  return NBA97_TEXT_COMPLETE;
}

static int read_at(Run *run, Nba97GameTextureWindowCommandWord base,
                   uint32_t offset, unsigned width, uint32_t pc,
                   Nba97GameTextureWindowCommandWord *destination) {
  Nba97GameTextureWindowCommandWord loaded = {0u, 0u};
  uint32_t guest_address;
  uint8_t *data, *known;
  unsigned byte;
  TRY(address(run, base, offset, pc, &guest_address));
  TRY(locate(run, pc, guest_address, width, &data, &known));
  for (byte = 0u; byte != width; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (8u * byte);
    if (known == NULL || known[byte] != 0u)
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << byte));
  }
  *destination = loaded;
  ++run->progress->reads;
  journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, guest_address, width, &loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_at(Run *run, Nba97GameTextureWindowCommandWord base,
                    uint32_t offset, uint32_t pc,
                    const Nba97GameTextureWindowCommandWord *source) {
  uint32_t guest_address;
  uint8_t *data, *known;
  unsigned byte;
  TRY(address(run, base, offset, pc, &guest_address));
  TRY(locate(run, pc, guest_address, 4u, &data, &known));
  if (known == NULL && source->known_mask != 15u)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0u; byte != 4u; ++byte) {
    data[byte] = (uint8_t)(source->word >> (8u * byte));
    if (known != NULL)
      known[byte] = (uint8_t)((source->known_mask >> byte) & 1u);
  }
  ++run->progress->stores;
  journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, guest_address, 4u, source);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static Nba97GameTextureWindowCommandWord
zero_extend(Nba97GameTextureWindowCommandWord value, unsigned width) {
  value.known_mask =
      (uint8_t)(value.known_mask | (uint8_t)(15u ^ width_mask(width)));
  return value;
}

static Nba97GameTextureWindowCommandWord
sign_extend_half(Nba97GameTextureWindowCommandWord value) {
  value.word = (value.word & UINT32_C(0x0000ffff)) |
               ((value.word & UINT32_C(0x00008000)) != 0u
                    ? UINT32_C(0xffff0000)
                    : 0u);
  if ((value.known_mask & 2u) != 0u)
    value.known_mask = (uint8_t)(value.known_mask | 12u);
  return value;
}

static int load_byte(Run *run, Nba97GameTextureWindowCommandWord base,
                     uint32_t offset, uint32_t pc,
                     Nba97GameTextureWindowCommandWord *destination) {
  Nba97GameTextureWindowCommandWord loaded;
  TRY(read_at(run, base, offset, 1u, pc, &loaded));
  *destination = zero_extend(loaded, 1u);
  return NBA97_TEXT_COMPLETE;
}

static int load_half(Run *run, Nba97GameTextureWindowCommandWord base,
                     uint32_t offset, uint32_t pc,
                     Nba97GameTextureWindowCommandWord *destination) {
  Nba97GameTextureWindowCommandWord loaded;
  TRY(read_at(run, base, offset, 2u, pc, &loaded));
  *destination = sign_extend_half(loaded);
  return NBA97_TEXT_COMPLETE;
}

static int branch_zero(Run *run, Nba97GameTextureWindowCommandWord value,
                       uint32_t pc, int *is_zero) {
  unsigned byte;
  for (byte = 0u; byte != 4u; ++byte)
    if ((value.known_mask & (1u << byte)) != 0u &&
        ((value.word >> (8u * byte)) & 255u) != 0u) {
      *is_zero = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (value.known_mask == 15u) {
    *is_zero = value.word == 0u;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0u);
  return NBA97_TEXT_UNKNOWN;
}

int nba97_game_texture_window_command(
    Nba97GameTextureWindowCommandContext *context,
    Nba97GameTextureWindowCommandProgress *progress) {
  Run storage;
  Run *run = &storage;
  Nba97GameTextureWindowCommandWord input_pointer;
  int zero;

  TRY(initialize(context, progress, run));

  /* 0x8009A824..0x8009A830: the entry branch always allocates its wrapping
   * scratch frame. Null a0 clears v0 and skips every mapped access. */
  input_pointer = R(NBA97_MATCH_INITIALIZE_A0);
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xfffffff0));
  progress->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  TRY(branch_zero(run, input_pointer, UINT32_C(0x8009a824), &zero));
  if (zero) {
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0u);
  } else {
    /* 0x8009A834..0x8009A858: read x, publish its shifted word, then read and
     * negate signed width before the second non-address-order stack store. */
    TRY(load_byte(run, input_pointer, 0u, UINT32_C(0x8009a834),
                  &R(NBA97_MATCH_INITIALIZE_A1)));
    R(NBA97_MATCH_INITIALIZE_A1) =
        shift_right_logical(R(NBA97_MATCH_INITIALIZE_A1), 3u);
    TRY(write_at(run, R(NBA97_MATCH_INITIALIZE_SP), 0u,
                 UINT32_C(0x8009a840), &R(NBA97_MATCH_INITIALIZE_A1)));
    TRY(load_half(run, input_pointer, 4u, UINT32_C(0x8009a844),
                  &R(NBA97_MATCH_INITIALIZE_A2)));
    R(NBA97_MATCH_INITIALIZE_A2) =
        subtract_words(R(NBA97_MATCH_INITIALIZE_ZERO),
                       R(NBA97_MATCH_INITIALIZE_A2));
    R(NBA97_MATCH_INITIALIZE_A2) =
        and_constant(R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0xff));
    R(NBA97_MATCH_INITIALIZE_A2) =
        shift_right_arithmetic(R(NBA97_MATCH_INITIALIZE_A2), 3u);
    TRY(write_at(run, R(NBA97_MATCH_INITIALIZE_SP), 8u,
                 UINT32_C(0x8009a858), &R(NBA97_MATCH_INITIALIZE_A2)));

    /* 0x8009A85C..0x8009A86C: the y load precedes the x-field shift in its
     * load-delay slot; its shifted value is stored before becoming E2 bits. */
    TRY(load_byte(run, input_pointer, 2u, UINT32_C(0x8009a85c),
                  &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_A1) =
        shift_left(R(NBA97_MATCH_INITIALIZE_A1), 10u);
    R(NBA97_MATCH_INITIALIZE_V0) =
        shift_right_logical(R(NBA97_MATCH_INITIALIZE_V0), 3u);
    TRY(write_at(run, R(NBA97_MATCH_INITIALIZE_SP), 4u,
                 UINT32_C(0x8009a868), &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_V0) =
        shift_left(R(NBA97_MATCH_INITIALIZE_V0), 15u);

    /* 0x8009A870..0x8009A898: the height load precedes LUI a0 in its load
     * delay, then the routine merges E2/x/y/width/height and stores height. */
    TRY(load_half(run, input_pointer, 6u, UINT32_C(0x8009a870),
                  &R(NBA97_MATCH_INITIALIZE_V1)));
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0xe2000000));
    R(NBA97_MATCH_INITIALIZE_A1) =
        or_words(R(NBA97_MATCH_INITIALIZE_A1),
                 R(NBA97_MATCH_INITIALIZE_A0));
    R(NBA97_MATCH_INITIALIZE_V0) =
        or_words(R(NBA97_MATCH_INITIALIZE_V0),
                 R(NBA97_MATCH_INITIALIZE_A1));
    R(NBA97_MATCH_INITIALIZE_V1) =
        subtract_words(R(NBA97_MATCH_INITIALIZE_ZERO),
                       R(NBA97_MATCH_INITIALIZE_V1));
    R(NBA97_MATCH_INITIALIZE_V1) =
        and_constant(R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xff));
    R(NBA97_MATCH_INITIALIZE_V1) =
        shift_right_arithmetic(R(NBA97_MATCH_INITIALIZE_V1), 3u);
    R(NBA97_MATCH_INITIALIZE_A0) =
        shift_left(R(NBA97_MATCH_INITIALIZE_V1), 5u);
    R(NBA97_MATCH_INITIALIZE_V0) =
        or_words(R(NBA97_MATCH_INITIALIZE_V0),
                 R(NBA97_MATCH_INITIALIZE_A0));
    R(NBA97_MATCH_INITIALIZE_V0) =
        or_words(R(NBA97_MATCH_INITIALIZE_V0),
                 R(NBA97_MATCH_INITIALIZE_A2));
    TRY(write_at(run, R(NBA97_MATCH_INITIALIZE_SP), 12u,
                 UINT32_C(0x8009a898), &R(NBA97_MATCH_INITIALIZE_V1)));
  }

  /* 0x8009A89C..0x8009A8A4: both paths restore temporary sp before JR.
   * Unknown ra therefore refuses only after the restoration is published. */
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x10u);
  progress->return_v0 = R(NBA97_MATCH_INITIALIZE_V0);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15u) {
    stop(run, UINT32_C(0x8009a8a0), 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  progress->completed = 1u;
  stop(run, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
