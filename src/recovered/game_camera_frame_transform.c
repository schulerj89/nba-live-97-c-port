#include "game_camera_frame_transform.h"

#include <limits.h>
#include <string.h>

typedef struct Run {
  Nba97GameCameraFrameTransformContext *context;
  Nba97GameCameraFrameTransformProgress *progress;
  Nba97GameCameraFrameTransformMachine machine;
} Run;

#define R(i) (run->machine.registers.gpr[(i)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int status_ = (expression);                                                \
    if (status_ != NBA97_TEXT_COMPLETE)                                        \
      return status_;                                                          \
  } while (0)

static void publish(Run *run) { run->progress->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t entry) {
  run->progress->stopped_pc = pc;
  run->progress->stopped_address = address;
  run->progress->stopped_entry = entry;
  publish(run);
}

static void known(Nba97GameCameraFrameTransformWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 15u;
}

static int valid_machine(const Nba97GameCameraFrameTransformMachine *machine) {
  unsigned index;
  if (machine->registers.gpr[0].word != 0u ||
      machine->registers.gpr[0].known_mask != 15u ||
      machine->hi.known_mask > 15u || machine->lo.known_mask > 15u)
    return 0;
  for (index = 0; index != 32u; ++index)
    if (machine->registers.gpr[index].known_mask > 15u)
      return 0;
  return 1;
}

static int initialize(Nba97GameCameraFrameTransformContext *context,
                      Nba97GameCameraFrameTransformProgress *progress,
                      Run *run) {
  size_t index, earlier;
  if (progress == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof(*progress));
  if (context == NULL ||
      (context->memory.count != 0u && context->memory.region == NULL) ||
      (context->access_journal_capacity != 0u &&
       context->access_journal == NULL) ||
      !valid_machine(&context->machine))
    return NBA97_TEXT_ARGUMENT;
  for (index = 0; index != context->memory.count; ++index) {
    const Nba97GameTextRegion *region = &context->memory.region[index];
    if (region->data == NULL || region->size == 0u ||
        region->size > UINT64_C(0x100000000) ||
        (uint64_t)region->base + region->size > UINT64_C(0x100000000))
      return NBA97_TEXT_ARGUMENT;
    for (earlier = 0; earlier != index; ++earlier) {
      const Nba97GameTextRegion *other = &context->memory.region[earlier];
      if ((uint64_t)region->base < (uint64_t)other->base + other->size &&
          (uint64_t)other->base < (uint64_t)region->base + region->size)
        return NBA97_TEXT_ARGUMENT;
    }
  }
  run->context = context;
  run->progress = progress;
  run->machine = context->machine;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static Nba97GameCameraFrameTransformWord
add_words(Nba97GameCameraFrameTransformWord left,
          Nba97GameCameraFrameTransformWord right) {
  Nba97GameCameraFrameTransformWord result;
  unsigned carry_mask = 1u;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0u;
  if (left.known_mask == 15u && right.known_mask == 15u) {
    result.known_mask = 15u;
    return result;
  }
  for (byte = 0; byte != 4u; ++byte) {
    unsigned next_carry_mask = 0u;
    unsigned first_output = 0u;
    unsigned first = 1u;
    unsigned invariant = 1u;
    unsigned left_start = (left.known_mask & (1u << byte))
                              ? (left.word >> (8u * byte)) & 255u
                              : 0u;
    unsigned left_end = (left.known_mask & (1u << byte)) ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte))
                               ? (right.word >> (8u * byte)) & 255u
                               : 0u;
    unsigned right_end = (right.known_mask & (1u << byte)) ? right_start : 255u;
    unsigned carry;
    for (carry = 0; carry != 2u; ++carry) {
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
          } else if (output != first_output) {
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

static Nba97GameCameraFrameTransformWord
subtract_words(Nba97GameCameraFrameTransformWord left,
               Nba97GameCameraFrameTransformWord right) {
  Nba97GameCameraFrameTransformWord result;
  unsigned borrow_mask = 1u;
  unsigned byte;
  result.word = left.word - right.word;
  result.known_mask = 0u;
  if (left.known_mask == 15u && right.known_mask == 15u) {
    result.known_mask = 15u;
    return result;
  }
  for (byte = 0; byte != 4u; ++byte) {
    unsigned next_borrow_mask = 0u;
    unsigned first_output = 0u;
    unsigned first = 1u;
    unsigned invariant = 1u;
    unsigned left_start = (left.known_mask & (1u << byte))
                              ? (left.word >> (8u * byte)) & 255u
                              : 0u;
    unsigned left_end = (left.known_mask & (1u << byte)) ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte))
                               ? (right.word >> (8u * byte)) & 255u
                               : 0u;
    unsigned right_end = (right.known_mask & (1u << byte)) ? right_start : 255u;
    unsigned borrow;
    for (borrow = 0; borrow != 2u; ++borrow) {
      unsigned a;
      if ((borrow_mask & (1u << borrow)) == 0u)
        continue;
      for (a = left_start; a <= left_end; ++a) {
        unsigned b;
        for (b = right_start; b <= right_end; ++b) {
          int difference = (int)a - (int)b - (int)borrow;
          unsigned output = (unsigned)difference & 255u;
          next_borrow_mask |= 1u << (difference < 0);
          if (first) {
            first_output = output;
            first = 0u;
          } else if (output != first_output) {
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

static Nba97GameCameraFrameTransformWord
add_constant(Nba97GameCameraFrameTransformWord value, uint32_t constant) {
  Nba97GameCameraFrameTransformWord right;
  known(&right, constant);
  return add_words(value, right);
}

static Nba97GameCameraFrameTransformWord
and_immediate(Nba97GameCameraFrameTransformWord value, uint16_t immediate) {
  Nba97GameCameraFrameTransformWord result;
  unsigned byte;
  result.word = value.word & immediate;
  result.known_mask = 12u;
  for (byte = 0; byte != 2u; ++byte) {
    unsigned mask = (immediate >> (8u * byte)) & 255u;
    if (mask == 0u || (value.known_mask & (1u << byte)) != 0u)
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
  }
  return result;
}

static Nba97GameCameraFrameTransformWord
shift_left(Nba97GameCameraFrameTransformWord value, unsigned amount) {
  Nba97GameCameraFrameTransformWord result;
  unsigned byte;
  result.word = value.word << amount;
  result.known_mask = 0u;
  for (byte = 0; byte != 4u; ++byte) {
    int low_bit = (int)(byte * 8u) - (int)amount;
    int high_bit = low_bit + 7;
    int source;
    int all_known = 1;
    if (high_bit < 0) {
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
      continue;
    }
    if (low_bit < 0)
      low_bit = 0;
    for (source = low_bit / 8; source <= high_bit / 8 && source < 4; ++source)
      if ((value.known_mask & (1u << source)) == 0u)
        all_known = 0;
    if (all_known)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static Nba97GameCameraFrameTransformWord
shift_right_arithmetic(Nba97GameCameraFrameTransformWord value,
                       unsigned amount) {
  Nba97GameCameraFrameTransformWord result;
  unsigned byte;
  result.word = value.word >> amount;
  if ((value.word & UINT32_C(0x80000000)) != 0u)
    result.word |= ~(UINT32_MAX >> amount);
  result.known_mask = 0u;
  for (byte = 0; byte != 4u; ++byte) {
    unsigned low_bit = byte * 8u + amount;
    unsigned high_bit = low_bit + 7u;
    unsigned first_source = low_bit / 8u;
    unsigned last_source = high_bit < 32u ? high_bit / 8u : 3u;
    unsigned source;
    int all_known = 1;
    for (source = first_source; source <= last_source; ++source)
      if ((value.known_mask & (1u << source)) == 0u)
        all_known = 0;
    if (high_bit >= 32u && (value.known_mask & 8u) == 0u)
      all_known = 0;
    if (all_known)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static int64_t signed_word(uint32_t word) {
  return word < UINT32_C(0x80000000) ? (int64_t)word
                                     : (int64_t)word - INT64_C(0x100000000);
}

static void
source_product_known_masks(Nba97GameCameraFrameTransformWord source_half,
                           Nba97GameCameraFrameTransformWord multiplier,
                           uint8_t *lo_mask, uint8_t *hi_mask) {
  uint8_t first_byte[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
  uint8_t invariant[8] = {1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u};
  unsigned candidate_half;
  int first = 1;
  *lo_mask = 0u;
  *hi_mask = 0u;
  if (multiplier.known_mask != 15u)
    return;
  if ((source_half.known_mask & 3u) == 3u) {
    *lo_mask = 15u;
    *hi_mask = 15u;
    return;
  }
  for (candidate_half = 0u; candidate_half != UINT32_C(0x10000);
       ++candidate_half) {
    int32_t signed_half = candidate_half < 0x8000u
                              ? (int32_t)candidate_half
                              : (int32_t)candidate_half - 0x10000;
    uint32_t candidate_word = (uint32_t)(signed_half * 16);
    unsigned byte;
    int matches = 1;
    for (byte = 0u; byte != 2u; ++byte)
      if ((source_half.known_mask & (1u << byte)) != 0u &&
          ((candidate_half >> (8u * byte)) & 255u) !=
              ((source_half.word >> (8u * byte)) & 255u))
        matches = 0;
    if (matches) {
      int64_t product =
          signed_word(candidate_word) * signed_word(multiplier.word);
      uint64_t bits = (uint64_t)product;
      for (byte = 0u; byte != 8u; ++byte) {
        uint8_t product_byte = (uint8_t)(bits >> (8u * byte));
        if (first)
          first_byte[byte] = product_byte;
        else if (first_byte[byte] != product_byte)
          invariant[byte] = 0u;
      }
      first = 0;
    }
  }
  if (first)
    return;
  for (candidate_half = 0u; candidate_half != 4u; ++candidate_half) {
    if (invariant[candidate_half] != 0u)
      *lo_mask = (uint8_t)(*lo_mask | (1u << candidate_half));
    if (invariant[candidate_half + 4u] != 0u)
      *hi_mask = (uint8_t)(*hi_mask | (1u << candidate_half));
  }
}

static void multiply(Run *run, size_t index, uint32_t pc,
                     Nba97GameCameraFrameTransformWord left,
                     Nba97GameCameraFrameTransformWord right,
                     Nba97GameCameraFrameTransformWord source_half) {
  Nba97GameCameraFrameTransformMultiply *trace =
      &run->progress->multiply[index];
  int64_t product = signed_word(left.word) * signed_word(right.word);
  uint64_t bits = (uint64_t)product;
  memset(trace, 0, sizeof(*trace));
  trace->pc = pc;
  trace->multiplicand = left;
  trace->multiplier = right;
  run->machine.lo.word = (uint32_t)bits;
  run->machine.hi.word = (uint32_t)(bits >> 32u);
  source_product_known_masks(source_half, right, &run->machine.lo.known_mask,
                             &run->machine.hi.known_mask);
  trace->hi = run->machine.hi;
  trace->lo = run->machine.lo;
  run->progress->multiply_count = index + 1u;
  publish(run);
}

static void move_from_hi(Run *run, size_t index, uint32_t pc,
                         unsigned destination) {
  R(destination) = run->machine.hi;
  run->progress->multiply[index].mfhi_pc = pc;
  run->progress->multiply[index].mfhi = R(destination);
  publish(run);
}

static Nba97GameCameraFrameTransformWord
load_unsigned_half(Nba97GameCameraFrameTransformWord value) {
  value.word &= UINT32_C(0x0000ffff);
  value.known_mask = (uint8_t)((value.known_mask & 3u) | 12u);
  return value;
}

static Nba97GameCameraFrameTransformWord
load_signed_half(Nba97GameCameraFrameTransformWord value) {
  uint32_t half = value.word & UINT32_C(0x0000ffff);
  value.word =
      (half & UINT32_C(0x8000)) != 0u ? half | UINT32_C(0xffff0000) : half;
  value.known_mask = (uint8_t)((value.known_mask & 3u) |
                               ((value.known_mask & 2u) != 0u ? 12u : 0u));
  return value;
}

static uint32_t width_mask(unsigned width) {
  return width == 4u ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - UINT32_C(1);
}

static int spend(Run *run) {
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width,
                    const Nba97GameCameraFrameTransformWord *value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameCameraFrameTransformAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word & width_mask(width);
    event->operation = run->progress->operations;
    event->width = (uint8_t)width;
    event->known_mask = (uint8_t)(value->known_mask & ((1u << width) - 1u));
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t address, unsigned width, uint32_t pc,
                  uint8_t **data, uint8_t **known_bytes) {
  size_t index, byte;
  stop(run, pc, address, 0u);
  TRY(spend(run));
  ++run->progress->accesses;
  if ((address & (width - 1u)) != 0u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (index = 0; index != run->context->memory.count; ++index) {
    Nba97GameTextRegion *region = &run->context->memory.region[index];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        width > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known_bytes =
        region->known != NULL ? region->known + (size_t)offset : NULL;
    if (*known_bytes != NULL)
      for (byte = 0; byte != width; ++byte)
        if ((*known_bytes)[byte] > 1u)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int read_memory(Run *run, uint32_t address, unsigned width, uint32_t pc,
                       Nba97GameCameraFrameTransformWord *value) {
  Nba97GameCameraFrameTransformWord loaded = {0u, 0u};
  uint8_t *data, *known_bytes;
  unsigned byte;
  TRY(locate(run, address, width, pc, &data, &known_bytes));
  for (byte = 0; byte != width; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (8u * byte);
    if (known_bytes == NULL || known_bytes[byte] != 0u)
      loaded.known_mask = (uint8_t)(loaded.known_mask | (uint8_t)(1u << byte));
  }
  *value = loaded;
  ++run->progress->reads;
  journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_memory(Run *run, uint32_t address, unsigned width, uint32_t pc,
                        const Nba97GameCameraFrameTransformWord *value) {
  uint8_t *data, *known_bytes;
  unsigned byte;
  TRY(locate(run, address, width, pc, &data, &known_bytes));
  if (known_bytes == NULL &&
      (value->known_mask & ((1u << width) - 1u)) != (1u << width) - 1u)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0; byte != width; ++byte) {
    data[byte] = (uint8_t)(value->word >> (8u * byte));
    if (known_bytes != NULL)
      known_bytes[byte] = (uint8_t)((value->known_mask >> byte) & 1u);
  }
  ++run->progress->stores;
  journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int address_from(Run *run, Nba97GameCameraFrameTransformWord base,
                        uint32_t offset, uint32_t pc, uint32_t *address) {
  Nba97GameCameraFrameTransformWord sum = add_constant(base, offset);
  if (sum.known_mask != 15u) {
    stop(run, pc, sum.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = sum.word;
  return NBA97_TEXT_COMPLETE;
}

static int read_at(Run *run, unsigned base_register, uint32_t offset,
                   unsigned width, uint32_t pc,
                   Nba97GameCameraFrameTransformWord *value) {
  uint32_t address;
  TRY(address_from(run, R(base_register), offset, pc, &address));
  return read_memory(run, address, width, pc, value);
}

static int write_at(Run *run, unsigned base_register, uint32_t offset,
                    unsigned width, uint32_t pc,
                    const Nba97GameCameraFrameTransformWord *value) {
  uint32_t address;
  TRY(address_from(run, R(base_register), offset, pc, &address));
  return write_memory(run, address, width, pc, value);
}

static int decide_zero(Run *run, const Nba97GameCameraFrameTransformWord *value,
                       uint32_t pc, int *is_zero) {
  unsigned byte;
  for (byte = 0; byte != 4u; ++byte)
    if ((value->known_mask & (1u << byte)) != 0u &&
        ((value->word >> (8u * byte)) & 255u) != 0u) {
      *is_zero = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (value->known_mask == 15u) {
    *is_zero = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0u, 0u);
  return NBA97_TEXT_UNKNOWN;
}

static int invoke(Run *run, uint32_t pc, uint32_t entry, uint8_t kind,
                  uint8_t argument_count) {
  Nba97GameCameraFrameTransformEvent event;
  int accepted;
  stop(run, pc, 0u, entry);
  TRY(spend(run));
  memset(&event, 0, sizeof(event));
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = entry;
  event.operation = run->progress->operations;
  event.invocation = run->progress->call_count[kind] + 1u;
  event.kind = kind;
  event.argument_count = argument_count;
  publish(run);
  if (run->context->io == NULL)
    return NBA97_TEXT_IO_REFUSED;
  accepted = run->context->io(run->context->user, &run->context->memory, &event,
                              &run->machine);
  publish(run);
  if (accepted != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!valid_machine(&run->machine))
    return NBA97_TEXT_ARGUMENT;
  ++run->progress->callbacks_completed;
  ++run->progress->call_count[kind];
  return NBA97_TEXT_COMPLETE;
}

static int restore(Run *run, uint32_t pc, uint32_t offset, unsigned reg,
                   Nba97GameCameraFrameTransformWord *restored) {
  TRY(read_at(run, 29u, offset, 4u, pc, &R(reg)));
  *restored = R(reg);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_camera_frame_transform(
    Nba97GameCameraFrameTransformContext *context,
    Nba97GameCameraFrameTransformProgress *progress) {
  Run state;
  Run *run = &state;
  Nba97GameCameraFrameTransformWord branch, loaded;
  Nba97GameCameraFrameTransformWord source_half[3];
  int is_zero;

  TRY(initialize(context, progress, run));

  /* 0x80051098..0x800510B8: read the controller gate before allocating the
   * frame; the branch delay always saves live s0. */
  known(&R(2), UINT32_C(0x800f0000));
  TRY(read_memory(run, UINT32_C(0x800eb678), 4u, UINT32_C(0x8005109c), &R(2)));
  R(29) = add_constant(R(29), UINT32_C(0xffffffd0));
  progress->frame_stack_pointer = R(29).word;
  TRY(write_at(run, 29u, 0x28u, 4u, UINT32_C(0x800510a4), &R(31)));
  TRY(write_at(run, 29u, 0x24u, 4u, UINT32_C(0x800510a8), &R(17)));
  branch = R(2);
  TRY(write_at(run, 29u, 0x20u, 4u, UINT32_C(0x800510b0), &R(16)));
  TRY(decide_zero(run, &branch, UINT32_C(0x800510ac), &is_zero));
  if (is_zero) {
    known(&R(31), UINT32_C(0x800510bc));
    TRY(invoke(run, UINT32_C(0x800510b4), UINT32_C(0x8004ea88),
               NBA97_GAME_CAMERA_FRAME_TRANSFORM_CONTROLLER_8004EA88, 0u));
  }

  /* 0x800510BC..0x8005116C: load inputs in source order, mask retained
   * angles, interleave each sum, copy position, and execute the matrix-call
   * delay store before dispatch. */
  R(4) = add_constant(R(29), 0x10u);
  known(&R(7), UINT32_C(0x80100000));
  R(7) = add_constant(R(7), UINT32_C(0xffffa638));
  TRY(read_at(run, 7u, 0u, 2u, UINT32_C(0x800510c8), &loaded));
  R(2) = load_unsigned_half(loaded);
  known(&R(3), UINT32_C(0x80100000));
  TRY(read_memory(run, UINT32_C(0x800fa63a), 2u, UINT32_C(0x800510d0),
                  &loaded));
  R(3) = load_unsigned_half(loaded);
  known(&R(6), UINT32_C(0x80100000));
  TRY(read_memory(run, UINT32_C(0x800fa63c), 2u, UINT32_C(0x800510d8),
                  &loaded));
  R(6) = load_unsigned_half(loaded);
  known(&R(8), UINT32_C(0x80100000));
  TRY(read_memory(run, UINT32_C(0x800fa630), 2u, UINT32_C(0x800510e0),
                  &loaded));
  R(8) = load_unsigned_half(loaded);
  known(&R(9), UINT32_C(0x80100000));
  TRY(read_memory(run, UINT32_C(0x800fa632), 2u, UINT32_C(0x800510e8),
                  &loaded));
  R(9) = load_unsigned_half(loaded);
  known(&R(10), UINT32_C(0x80100000));
  TRY(read_memory(run, UINT32_C(0x800fa634), 2u, UINT32_C(0x800510f0),
                  &loaded));
  R(10) = load_unsigned_half(loaded);
  known(&R(11), UINT32_C(0x80100000));
  TRY(read_memory(run, UINT32_C(0x800fb858), 2u, UINT32_C(0x800510f8),
                  &loaded));
  R(11) = load_unsigned_half(loaded);
  known(&R(12), UINT32_C(0x80100000));
  TRY(read_memory(run, UINT32_C(0x800fb85a), 2u, UINT32_C(0x80051100),
                  &loaded));
  R(12) = load_unsigned_half(loaded);
  known(&R(13), UINT32_C(0x80100000));
  TRY(read_memory(run, UINT32_C(0x800fb85c), 2u, UINT32_C(0x80051108),
                  &loaded));
  R(13) = load_unsigned_half(loaded);
  known(&R(16), UINT32_C(0x80100000));
  R(16) = add_constant(R(16), UINT32_C(0xffff9fd8));
  R(5) = R(16);
  known(&R(17), UINT32_C(0x80100000));
  R(17) = add_constant(R(17), UINT32_C(0xffffb828));
  R(2) = and_immediate(R(2), UINT16_C(0x0fff));
  R(3) = and_immediate(R(3), UINT16_C(0x0fff));
  R(6) = and_immediate(R(6), UINT16_C(0x0fff));
  TRY(write_at(run, 7u, 0u, 2u, UINT32_C(0x8005112c), &R(2)));
  R(2) = add_words(R(2), R(11));
  known(&R(1), UINT32_C(0x80100000));
  TRY(write_memory(run, UINT32_C(0x800fa63a), 2u, UINT32_C(0x80051138), &R(3)));
  R(3) = add_words(R(3), R(12));
  known(&R(1), UINT32_C(0x80100000));
  TRY(write_memory(run, UINT32_C(0x800fa63c), 2u, UINT32_C(0x80051144), &R(6)));
  R(6) = add_words(R(6), R(13));
  TRY(write_at(run, 17u, 0u, 2u, UINT32_C(0x8005114c), &R(8)));
  known(&R(1), UINT32_C(0x80100000));
  TRY(write_memory(run, UINT32_C(0x800fb82a), 2u, UINT32_C(0x80051154), &R(9)));
  known(&R(1), UINT32_C(0x80100000));
  TRY(write_memory(run, UINT32_C(0x800fb82c), 2u, UINT32_C(0x8005115c),
                   &R(10)));
  TRY(write_at(run, 29u, 0x10u, 2u, UINT32_C(0x80051160), &R(2)));
  TRY(write_at(run, 29u, 0x12u, 2u, UINT32_C(0x80051164), &R(3)));
  known(&R(31), UINT32_C(0x80051170));
  TRY(write_at(run, 29u, 0x14u, 2u, UINT32_C(0x8005116c), &R(6)));
  TRY(invoke(run, UINT32_C(0x80051168), UINT32_C(0x80056080),
             NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATRIX_80056080, 2u));

  /* 0x80051170..0x80051200: preserve the three signed MULT/MFHI pipelines
   * and the interleaved translation clears before writing scaled halves. */
  TRY(read_at(run, 16u, 0u, 2u, UINT32_C(0x80051170), &loaded));
  R(5) = load_signed_half(loaded);
  source_half[0] = R(5);
  known(&R(2), UINT32_C(0x66660000));
  R(2).word |= UINT32_C(0x00006667);
  R(5) = shift_left(R(5), 4u);
  multiply(run, 0u, UINT32_C(0x80051180), R(5), R(2), source_half[0]);
  known(&R(3), UINT32_C(0x80100000));
  TRY(read_memory(run, UINT32_C(0x800f9fda), 2u, UINT32_C(0x80051188),
                  &loaded));
  R(3) = load_signed_half(loaded);
  source_half[1] = R(3);
  move_from_hi(run, 0u, UINT32_C(0x8005118c), 9u);
  R(3) = shift_left(R(3), 4u);
  multiply(run, 1u, UINT32_C(0x80051198), R(3), R(2), source_half[1]);
  known(&R(6), UINT32_C(0x80100000));
  TRY(read_memory(run, UINT32_C(0x800f9fdc), 2u, UINT32_C(0x800511a0),
                  &loaded));
  R(6) = load_signed_half(loaded);
  source_half[2] = R(6);
  move_from_hi(run, 1u, UINT32_C(0x800511a4), 7u);
  known(&R(1), UINT32_C(0x80100000));
  known(&loaded, 0u);
  TRY(write_memory(run, UINT32_C(0x800f9fec), 4u, UINT32_C(0x800511ac),
                   &loaded));
  R(6) = shift_left(R(6), 4u);
  multiply(run, 2u, UINT32_C(0x800511b4), R(6), R(2), source_half[2]);
  known(&R(1), UINT32_C(0x80100000));
  TRY(write_memory(run, UINT32_C(0x800f9ff0), 4u, UINT32_C(0x800511bc),
                   &loaded));
  known(&R(1), UINT32_C(0x80100000));
  TRY(write_memory(run, UINT32_C(0x800f9ff4), 4u, UINT32_C(0x800511c4),
                   &loaded));
  R(5) = shift_right_arithmetic(R(5), 31u);
  R(3) = shift_right_arithmetic(R(3), 31u);
  R(2) = shift_right_arithmetic(R(9), 2u);
  R(2) = subtract_words(R(2), R(5));
  TRY(write_at(run, 16u, 0u, 2u, UINT32_C(0x800511d8), &R(2)));
  R(2) = shift_right_arithmetic(R(7), 2u);
  R(2) = subtract_words(R(2), R(3));
  R(6) = shift_right_arithmetic(R(6), 31u);
  known(&R(1), UINT32_C(0x80100000));
  TRY(write_memory(run, UINT32_C(0x800f9fda), 2u, UINT32_C(0x800511ec), &R(2)));
  move_from_hi(run, 2u, UINT32_C(0x800511f0), 8u);
  R(2) = shift_right_arithmetic(R(8), 2u);
  R(2) = subtract_words(R(2), R(6));
  known(&R(1), UINT32_C(0x80100000));
  TRY(write_memory(run, UINT32_C(0x800f9fdc), 2u, UINT32_C(0x80051200), &R(2)));

  /* 0x80051204..0x8005122C: both geometry-engine calls consume live s0 in
   * their delay slots; the reference transform derives a2 from live sp. */
  known(&R(31), UINT32_C(0x8005120c));
  R(4) = R(16);
  TRY(invoke(run, UINT32_C(0x80051204), UINT32_C(0x80055f18),
             NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_ROTATION_80055F18, 1u));
  known(&R(31), UINT32_C(0x80051214));
  R(4) = R(16);
  TRY(invoke(run, UINT32_C(0x8005120c), UINT32_C(0x80055f44),
             NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_TRANSLATION_80055F44, 1u));
  known(&R(4), UINT32_C(0x80100000));
  R(4) = add_constant(R(4), UINT32_C(0xffffab98));
  known(&R(16), UINT32_C(0x80100000));
  R(16) = add_constant(R(16), UINT32_C(0xffffc61c));
  R(5) = R(16);
  known(&R(31), UINT32_C(0x80051230));
  R(6) = add_constant(R(29), 0x18u);
  TRY(invoke(run, UINT32_C(0x80051228), UINT32_C(0x80056650),
             NBA97_GAME_CAMERA_FRAME_TRANSFORM_REFERENCE_80056650, 3u));

  /* 0x80051230..0x80051278: the first position and transform loads use
   * callback-live s1/s0; later components use their fixed source addresses. */
  TRY(read_at(run, 17u, 0u, 2u, UINT32_C(0x80051230), &loaded));
  R(2) = load_signed_half(loaded);
  TRY(read_at(run, 16u, 0u, 4u, UINT32_C(0x80051234), &R(5)));
  known(&R(3), UINT32_C(0x80100000));
  TRY(read_memory(run, UINT32_C(0x800fb82a), 2u, UINT32_C(0x8005123c),
                  &loaded));
  R(3) = load_signed_half(loaded);
  known(&R(6), UINT32_C(0x80100000));
  TRY(read_memory(run, UINT32_C(0x800fc620), 4u, UINT32_C(0x80051244), &R(6)));
  known(&R(4), UINT32_C(0x80100000));
  TRY(read_memory(run, UINT32_C(0x800fb82c), 2u, UINT32_C(0x8005124c),
                  &loaded));
  R(4) = load_signed_half(loaded);
  known(&R(7), UINT32_C(0x80100000));
  TRY(read_memory(run, UINT32_C(0x800fc624), 4u, UINT32_C(0x80051254), &R(7)));
  R(2) = add_words(R(2), R(5));
  R(3) = add_words(R(3), R(6));
  R(4) = add_words(R(4), R(7));
  known(&R(1), UINT32_C(0x80100000));
  TRY(write_memory(run, UINT32_C(0x800f9fec), 4u, UINT32_C(0x80051268), &R(2)));
  known(&R(1), UINT32_C(0x80100000));
  TRY(write_memory(run, UINT32_C(0x800f9ff0), 4u, UINT32_C(0x80051270), &R(3)));
  known(&R(1), UINT32_C(0x80100000));
  TRY(write_memory(run, UINT32_C(0x800f9ff4), 4u, UINT32_C(0x80051278), &R(4)));

  /* 0x8005127C..0x80051290: restore through callback-live sp and consume the
   * restored return address after the wrapped frame increment. */
  TRY(restore(run, UINT32_C(0x8005127c), 0x28u, 31u,
              &progress->restored_return_address));
  TRY(restore(run, UINT32_C(0x80051280), 0x24u, 17u, &progress->restored_s1));
  TRY(restore(run, UINT32_C(0x80051284), 0x20u, 16u, &progress->restored_s0));
  R(29) = add_constant(R(29), 0x30u);
  if (R(31).known_mask != 15u) {
    stop(run, UINT32_C(0x8005128c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  progress->completed = 1u;
  stop(run, 0u, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
