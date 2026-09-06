#include "game_draw_packet.h"

#include <limits.h>
#include <string.h>

typedef struct Run {
  Nba97GameDrawPacketContext *context;
  Nba97GameDrawPacketProgress *progress;
  Nba97GameDrawPacketMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
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

static void set_known(Nba97GameDrawPacketWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 15u;
}

static int valid_machine(const Nba97GameDrawPacketMachine *machine) {
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

static int initialize(Nba97GameDrawPacketContext *context,
                      Nba97GameDrawPacketProgress *progress, Run *run) {
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

static Nba97GameDrawPacketWord
add_words(Nba97GameDrawPacketWord left, Nba97GameDrawPacketWord right) {
  Nba97GameDrawPacketWord result;
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

static Nba97GameDrawPacketWord
subtract_words(Nba97GameDrawPacketWord left, Nba97GameDrawPacketWord right) {
  Nba97GameDrawPacketWord result;
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

static Nba97GameDrawPacketWord
constant_word(uint32_t word) {
  Nba97GameDrawPacketWord result;
  set_known(&result, word);
  return result;
}

static Nba97GameDrawPacketWord
add_constant(Nba97GameDrawPacketWord value, uint32_t constant) {
  return add_words(value, constant_word(constant));
}

static Nba97GameDrawPacketWord
and_constant(Nba97GameDrawPacketWord value, uint32_t constant) {
  Nba97GameDrawPacketWord result;
  unsigned byte;
  result.word = value.word & constant;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned mask = 1u << byte;
    unsigned constant_byte = (constant >> (8u * byte)) & 255u;
    if ((value.known_mask & mask) != 0u || constant_byte == 0u)
      result.known_mask = (uint8_t)(result.known_mask | mask);
  }
  return result;
}

static Nba97GameDrawPacketWord
or_words(Nba97GameDrawPacketWord left, Nba97GameDrawPacketWord right) {
  Nba97GameDrawPacketWord result;
  unsigned byte;
  result.word = left.word | right.word;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned mask = 1u << byte;
    unsigned l = (left.word >> (8u * byte)) & 255u;
    unsigned r = (right.word >> (8u * byte)) & 255u;
    if (((left.known_mask & right.known_mask & mask) != 0u) ||
        ((left.known_mask & mask) != 0u && l == 255u) ||
        ((right.known_mask & mask) != 0u && r == 255u))
      result.known_mask = (uint8_t)(result.known_mask | mask);
  }
  return result;
}

static Nba97GameDrawPacketWord
shift_left(Nba97GameDrawPacketWord value, unsigned shift) {
  Nba97GameDrawPacketWord result;
  unsigned byte, bit;
  result.word = value.word << shift;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned known = 1u;
    for (bit = byte * 8u; bit != byte * 8u + 8u; ++bit) {
      if (bit >= shift &&
          (value.known_mask & (1u << ((bit - shift) / 8u))) == 0u)
        known = 0u;
    }
    if (known)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static Nba97GameDrawPacketWord
shift_right_arithmetic(Nba97GameDrawPacketWord value, unsigned shift) {
  Nba97GameDrawPacketWord result;
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

static void biased_signed_bounds(Nba97GameDrawPacketWord value,
                                 uint32_t *minimum, uint32_t *maximum) {
  uint32_t low = 0u;
  uint32_t high = 0u;
  unsigned byte;
  value.word ^= UINT32_C(0x80000000);
  for (byte = 0u; byte != 4u; ++byte) {
    uint32_t bits = UINT32_C(255) << (8u * byte);
    if ((value.known_mask & (1u << byte)) != 0u) {
      low |= value.word & bits;
      high |= value.word & bits;
    } else {
      high |= bits;
    }
  }
  *minimum = low;
  *maximum = high;
}

static Nba97GameDrawPacketWord
signed_less(Nba97GameDrawPacketWord left, Nba97GameDrawPacketWord right) {
  Nba97GameDrawPacketWord result;
  uint32_t left_min, left_max, right_min, right_max;
  result.word = (left.word ^ UINT32_C(0x80000000)) <
                        (right.word ^ UINT32_C(0x80000000))
                    ? 1u
                    : 0u;
  result.known_mask = 14u;
  biased_signed_bounds(left, &left_min, &left_max);
  biased_signed_bounds(right, &right_min, &right_max);
  if (left_max < right_min) {
    result.word = 1u;
    result.known_mask = 15u;
  } else if (left_min >= right_max) {
    result.word = 0u;
    result.known_mask = 15u;
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
                    unsigned width, const Nba97GameDrawPacketWord *value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameDrawPacketAccess *event = &run->context->access_journal[index];
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
  stop(run, pc, address, 0u);
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

static int read_memory(Run *run, uint32_t pc, uint32_t address, unsigned width,
                       Nba97GameDrawPacketWord *destination) {
  Nba97GameDrawPacketWord loaded = {0u, 0u};
  uint8_t *data, *known;
  unsigned byte;
  TRY(locate(run, pc, address, width, &data, &known));
  for (byte = 0u; byte != width; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (8u * byte);
    if (known == NULL || known[byte] != 0u)
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << byte));
  }
  *destination = loaded;
  ++run->progress->reads;
  journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, address, width, &loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_memory(Run *run, uint32_t pc, uint32_t address,
                        unsigned width,
                        const Nba97GameDrawPacketWord *source) {
  uint8_t *data, *known;
  unsigned byte;
  TRY(locate(run, pc, address, width, &data, &known));
  if (known == NULL &&
      (source->known_mask & width_mask(width)) != width_mask(width))
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0u; byte != width; ++byte) {
    data[byte] = (uint8_t)(source->word >> (8u * byte));
    if (known != NULL)
      known[byte] = (uint8_t)((source->known_mask >> byte) & 1u);
  }
  ++run->progress->stores;
  journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, width, source);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int address(Run *run, Nba97GameDrawPacketWord base, uint32_t offset,
                   uint32_t pc, uint32_t *result) {
  Nba97GameDrawPacketWord sum = add_constant(base, offset);
  if (sum.known_mask != 15u) {
    stop(run, pc, sum.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = sum.word;
  return NBA97_TEXT_COMPLETE;
}

static int read_at(Run *run, unsigned base_register, uint32_t offset,
                   unsigned width, uint32_t pc,
                   Nba97GameDrawPacketWord *destination) {
  uint32_t guest_address;
  TRY(address(run, R(base_register), offset, pc, &guest_address));
  return read_memory(run, pc, guest_address, width, destination);
}

static int write_at(Run *run, unsigned base_register, uint32_t offset,
                    unsigned width, uint32_t pc,
                    const Nba97GameDrawPacketWord *source) {
  uint32_t guest_address;
  TRY(address(run, R(base_register), offset, pc, &guest_address));
  return write_memory(run, pc, guest_address, width, source);
}

static Nba97GameDrawPacketWord zero_extend(Nba97GameDrawPacketWord value,
                                           unsigned width) {
  value.known_mask = (uint8_t)(value.known_mask | (uint8_t)(15u ^ width_mask(width)));
  return value;
}

static Nba97GameDrawPacketWord sign_extend_half(Nba97GameDrawPacketWord value) {
  value.word = (value.word & UINT32_C(0x0000ffff)) |
               ((value.word & UINT32_C(0x00008000)) != 0u
                    ? UINT32_C(0xffff0000)
                    : 0u);
  if ((value.known_mask & 2u) != 0u)
    value.known_mask = (uint8_t)(value.known_mask | 12u);
  return value;
}

static int load_unsigned(Run *run, unsigned base, uint32_t offset,
                         unsigned width, uint32_t pc,
                         Nba97GameDrawPacketWord *destination) {
  Nba97GameDrawPacketWord loaded;
  TRY(read_at(run, base, offset, width, pc, &loaded));
  *destination = zero_extend(loaded, width);
  return NBA97_TEXT_COMPLETE;
}

static int load_half_signed(Run *run, unsigned base, uint32_t offset,
                            uint32_t pc,
                            Nba97GameDrawPacketWord *destination) {
  Nba97GameDrawPacketWord loaded;
  TRY(read_at(run, base, offset, 2u, pc, &loaded));
  *destination = sign_extend_half(loaded);
  return NBA97_TEXT_COMPLETE;
}

static int branch_zero(Run *run, Nba97GameDrawPacketWord value, uint32_t pc,
                       int *is_zero) {
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
  stop(run, pc, 0u, 0u);
  return NBA97_TEXT_UNKNOWN;
}

static int branch_negative(Run *run, Nba97GameDrawPacketWord value,
                           uint32_t pc, int *negative) {
  if ((value.known_mask & 8u) == 0u) {
    stop(run, pc, 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *negative = (value.word & UINT32_C(0x80000000)) != 0u;
  return NBA97_TEXT_COMPLETE;
}

static int invoke(Run *run, uint32_t pc, uint32_t entry, uint8_t kind,
                  uint8_t argument_count) {
  Nba97GameDrawPacketEvent event;
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
  accepted = run->context->io(run->context->user, &run->context->memory,
                              &event, &run->machine);
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
                   Nba97GameDrawPacketWord *restored) {
  Nba97GameDrawPacketWord loaded;
  TRY(read_at(run, NBA97_MATCH_INITIALIZE_SP, offset, 4u, pc, &loaded));
  R(reg) = loaded;
  *restored = loaded;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_draw_packet(Nba97GameDrawPacketContext *context,
                           Nba97GameDrawPacketProgress *progress) {
  Run storage;
  Run *run = &storage;
  Nba97GameDrawPacketWord temporary;
  int condition;

  TRY(initialize(context, progress, run));

  /* 0x8009A344..0x8009A368: allocate the wrapping frame, save s0/s1/ra,
   * load signed origin coordinates, then invoke the first packet-word child. */
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffd8));
  progress->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x18u, 4u,
               UINT32_C(0x8009a348), &R(NBA97_MATCH_INITIALIZE_S0)));
  R(NBA97_MATCH_INITIALIZE_S0) = R(NBA97_MATCH_INITIALIZE_A1);
  TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x1cu, 4u,
               UINT32_C(0x8009a350), &R(NBA97_GAME_MATCH_CLOCKS_S1)));
  R(NBA97_GAME_MATCH_CLOCKS_S1) = R(NBA97_MATCH_INITIALIZE_A0);
  TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x20u, 4u,
               UINT32_C(0x8009a358), &R(NBA97_MATCH_INITIALIZE_RA)));
  TRY(load_half_signed(run, NBA97_MATCH_INITIALIZE_S0, 0u,
                       UINT32_C(0x8009a35c), &R(NBA97_MATCH_INITIALIZE_A0)));
  TRY(load_half_signed(run, NBA97_MATCH_INITIALIZE_S0, 2u,
                       UINT32_C(0x8009a360), &R(NBA97_MATCH_INITIALIZE_A1)));
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x8009a36c));
  TRY(invoke(run, UINT32_C(0x8009a364), UINT32_C(0x8009a644),
             NBA97_GAME_DRAW_PACKET_CHILD_8009A644, 2u));

  /* 0x8009A36C..0x8009A3A0: store child one, compute inclusive signed
   * bottom-right coordinates with low-half wrapping, and call child two. */
  TRY(write_at(run, NBA97_GAME_MATCH_CLOCKS_S1, 4u, 4u,
               UINT32_C(0x8009a36c), &R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 4u, 2u,
                    UINT32_C(0x8009a370), &R(NBA97_MATCH_INITIALIZE_A0)));
  TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 0u, 2u,
                    UINT32_C(0x8009a374), &R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 2u, 2u,
                    UINT32_C(0x8009a378), &R(NBA97_MATCH_INITIALIZE_A1)));
  R(NBA97_MATCH_INITIALIZE_A0) =
      add_words(R(NBA97_MATCH_INITIALIZE_A0), R(NBA97_MATCH_INITIALIZE_V0));
  R(NBA97_MATCH_INITIALIZE_A0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0xffffffff));
  R(NBA97_MATCH_INITIALIZE_A0) =
      shift_left(R(NBA97_MATCH_INITIALIZE_A0), 16u);
  TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 6u, 2u,
                    UINT32_C(0x8009a388), &R(NBA97_MATCH_INITIALIZE_V0)));
  R(NBA97_MATCH_INITIALIZE_A0) =
      shift_right_arithmetic(R(NBA97_MATCH_INITIALIZE_A0), 16u);
  R(NBA97_MATCH_INITIALIZE_A1) =
      add_words(R(NBA97_MATCH_INITIALIZE_A1), R(NBA97_MATCH_INITIALIZE_V0));
  R(NBA97_MATCH_INITIALIZE_A1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0xffffffff));
  R(NBA97_MATCH_INITIALIZE_A1) =
      shift_left(R(NBA97_MATCH_INITIALIZE_A1), 16u);
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x8009a3a4));
  R(NBA97_MATCH_INITIALIZE_A1) =
      shift_right_arithmetic(R(NBA97_MATCH_INITIALIZE_A1), 16u);
  TRY(invoke(run, UINT32_C(0x8009a39c), UINT32_C(0x8009a710),
             NBA97_GAME_DRAW_PACKET_CHILD_8009A710, 2u));

  /* 0x8009A3A4..0x8009A3D8: build words three through five. The final JAL
   * delay stores child four's v0 before child five can replace it. */
  TRY(write_at(run, NBA97_GAME_MATCH_CLOCKS_S1, 8u, 4u,
               UINT32_C(0x8009a3a4), &R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(load_half_signed(run, NBA97_MATCH_INITIALIZE_S0, 8u,
                       UINT32_C(0x8009a3a8), &R(NBA97_MATCH_INITIALIZE_A0)));
  TRY(load_half_signed(run, NBA97_MATCH_INITIALIZE_S0, 10u,
                       UINT32_C(0x8009a3ac), &R(NBA97_MATCH_INITIALIZE_A1)));
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x8009a3b8));
  TRY(invoke(run, UINT32_C(0x8009a3b0), UINT32_C(0x8009a7dc),
             NBA97_GAME_DRAW_PACKET_CHILD_8009A7DC, 2u));
  TRY(write_at(run, NBA97_GAME_MATCH_CLOCKS_S1, 12u, 4u,
               UINT32_C(0x8009a3b8), &R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 0x17u, 1u,
                    UINT32_C(0x8009a3bc), &R(NBA97_MATCH_INITIALIZE_A0)));
  TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 0x16u, 1u,
                    UINT32_C(0x8009a3c0), &R(NBA97_MATCH_INITIALIZE_A1)));
  TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 0x14u, 2u,
                    UINT32_C(0x8009a3c4), &R(NBA97_MATCH_INITIALIZE_A2)));
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x8009a3d0));
  TRY(invoke(run, UINT32_C(0x8009a3c8), UINT32_C(0x8009a5e8),
             NBA97_GAME_DRAW_PACKET_CHILD_8009A5E8, 3u));
  R(NBA97_MATCH_INITIALIZE_A0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_S0), 12u);
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x8009a3dc));
  TRY(write_at(run, NBA97_GAME_MATCH_CLOCKS_S1, 0x10u, 4u,
               UINT32_C(0x8009a3d8), &R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(invoke(run, UINT32_C(0x8009a3d4), UINT32_C(0x8009a824),
             NBA97_GAME_DRAW_PACKET_CHILD_8009A824, 1u));
  TRY(write_at(run, NBA97_GAME_MATCH_CLOCKS_S1, 0x14u, 4u,
               UINT32_C(0x8009a3dc), &R(NBA97_MATCH_INITIALIZE_V0)));
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xe6000000));
  TRY(write_at(run, NBA97_GAME_MATCH_CLOCKS_S1, 0x18u, 4u,
               UINT32_C(0x8009a3e4), &R(NBA97_MATCH_INITIALIZE_V0)));

  /* 0x8009A3E8..0x8009A3F4: test the background byte; the branch delay
   * always replaces callback-live t0 with seven. */
  TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 0x18u, 1u,
                    UINT32_C(0x8009a3e8), &R(NBA97_MATCH_INITIALIZE_V0)));
  set_known(&R(NBA97_MATCH_INITIALIZE_T0), 7u);
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0),
                  UINT32_C(0x8009a3f0), &condition));
  if (!condition) {
    /* 0x8009A3F8..0x8009A4A8: snapshot the rectangle on the live frame and
     * clamp signed width and height separately against signed globals-1. */
    TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 0u, 2u,
                      UINT32_C(0x8009a3f8), &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x10u, 2u,
                 UINT32_C(0x8009a400), &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 2u, 2u,
                      UINT32_C(0x8009a404), &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x12u, 2u,
                 UINT32_C(0x8009a40c), &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 4u, 2u,
                      UINT32_C(0x8009a410), &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x14u, 2u,
                 UINT32_C(0x8009a418), &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 6u, 2u,
                      UINT32_C(0x8009a41c), &R(NBA97_MATCH_INITIALIZE_V1)));
    R(NBA97_MATCH_INITIALIZE_V0) =
        shift_left(R(NBA97_MATCH_INITIALIZE_V0), 16u);
    TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x16u, 2u,
                 UINT32_C(0x8009a424), &R(NBA97_MATCH_INITIALIZE_V1)));
    R(NBA97_MATCH_INITIALIZE_V1) =
        shift_right_arithmetic(R(NBA97_MATCH_INITIALIZE_V0), 16u);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0u);
    TRY(branch_negative(run, R(NBA97_MATCH_INITIALIZE_V1),
                        UINT32_C(0x8009a42c), &condition));
    if (!condition) {
      set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
      R(NBA97_MATCH_INITIALIZE_V0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x55c4));
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_V0, 0u, 2u,
                        UINT32_C(0x8009a43c), &R(NBA97_MATCH_INITIALIZE_V0)));
      R(NBA97_MATCH_INITIALIZE_V0) =
          shift_left(R(NBA97_MATCH_INITIALIZE_V0), 16u);
      R(NBA97_MATCH_INITIALIZE_V0) =
          shift_right_arithmetic(R(NBA97_MATCH_INITIALIZE_V0), 16u);
      R(NBA97_MATCH_INITIALIZE_A0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xffffffff));
      R(NBA97_MATCH_INITIALIZE_V0) =
          signed_less(R(NBA97_MATCH_INITIALIZE_A0),
                      R(NBA97_MATCH_INITIALIZE_V1));
      temporary = R(NBA97_MATCH_INITIALIZE_V1);
      R(NBA97_MATCH_INITIALIZE_V0) = temporary;
      TRY(branch_zero(run,
                      signed_less(R(NBA97_MATCH_INITIALIZE_A0), temporary),
                      UINT32_C(0x8009a454), &condition));
      if (!condition) {
        R(NBA97_MATCH_INITIALIZE_V1) = R(NBA97_MATCH_INITIALIZE_A0);
        R(NBA97_MATCH_INITIALIZE_V0) = R(NBA97_MATCH_INITIALIZE_V1);
      }
    }

    TRY(load_half_signed(run, NBA97_MATCH_INITIALIZE_SP, 0x16u,
                         UINT32_C(0x8009a464), &R(NBA97_MATCH_INITIALIZE_V1)));
    TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x14u, 2u,
                 UINT32_C(0x8009a470), &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(branch_negative(run, R(NBA97_MATCH_INITIALIZE_V1),
                        UINT32_C(0x8009a46c), &condition));
    if (condition) {
      set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0u);
    } else {
      set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
      R(NBA97_MATCH_INITIALIZE_V0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x55c6));
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_V0, 0u, 2u,
                        UINT32_C(0x8009a47c), &R(NBA97_MATCH_INITIALIZE_V0)));
      R(NBA97_MATCH_INITIALIZE_V0) =
          shift_left(R(NBA97_MATCH_INITIALIZE_V0), 16u);
      R(NBA97_MATCH_INITIALIZE_V0) =
          shift_right_arithmetic(R(NBA97_MATCH_INITIALIZE_V0), 16u);
      R(NBA97_MATCH_INITIALIZE_A0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xffffffff));
      temporary = signed_less(R(NBA97_MATCH_INITIALIZE_A0),
                              R(NBA97_MATCH_INITIALIZE_V1));
      R(NBA97_MATCH_INITIALIZE_V0) = R(NBA97_MATCH_INITIALIZE_V1);
      TRY(branch_zero(run, temporary, UINT32_C(0x8009a494), &condition));
      if (!condition) {
        R(NBA97_MATCH_INITIALIZE_V1) = R(NBA97_MATCH_INITIALIZE_A0);
        R(NBA97_MATCH_INITIALIZE_V0) = R(NBA97_MATCH_INITIALIZE_V1);
      }
    }

    /* 0x8009A4AC..0x8009A4D0: publish clamped height and choose between
     * offset rectangle (0x60 command) and fully 64-aligned fill (0x02). */
    TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_SP, 0x10u, 2u,
                      UINT32_C(0x8009a4ac), &R(NBA97_MATCH_INITIALIZE_V1)));
    TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x16u, 2u,
                 UINT32_C(0x8009a4b0), &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_V0) =
        and_constant(R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x3f));
    R(NBA97_MATCH_INITIALIZE_A2) =
        shift_left(R(NBA97_MATCH_INITIALIZE_T0), 2u);
    TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0),
                    UINT32_C(0x8009a4b8), &condition));
    if (condition) {
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_SP, 0x14u, 2u,
                        UINT32_C(0x8009a4c0), &R(NBA97_MATCH_INITIALIZE_V0)));
      R(NBA97_MATCH_INITIALIZE_V0) =
          and_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x3f));
      R(NBA97_MATCH_INITIALIZE_A1) =
          shift_left(R(NBA97_MATCH_INITIALIZE_T0), 2u);
      TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0),
                      UINT32_C(0x8009a4cc), &condition));
    }

    if (!condition) {
      /* 0x8009A4D4..0x8009A56C: append the offset 0x60 rectangle. Later
       * loads intentionally reread stack and environment after earlier stores. */
      R(NBA97_MATCH_INITIALIZE_T0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_T0), 1u);
      R(NBA97_MATCH_INITIALIZE_A1) =
          shift_left(R(NBA97_MATCH_INITIALIZE_T0), 2u);
      R(NBA97_MATCH_INITIALIZE_T0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_T0), 1u);
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 8u, 2u,
                        UINT32_C(0x8009a4e0), &R(NBA97_MATCH_INITIALIZE_V0)));
      R(NBA97_MATCH_INITIALIZE_A2) =
          add_words(R(NBA97_MATCH_INITIALIZE_A2),
                    R(NBA97_GAME_MATCH_CLOCKS_S1));
      R(NBA97_MATCH_INITIALIZE_V0) =
          subtract_words(R(NBA97_MATCH_INITIALIZE_V1),
                         R(NBA97_MATCH_INITIALIZE_V0));
      TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x10u, 2u,
                   UINT32_C(0x8009a4ec), &R(NBA97_MATCH_INITIALIZE_V0)));
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_SP, 0x12u, 2u,
                        UINT32_C(0x8009a4f0), &R(NBA97_MATCH_INITIALIZE_V0)));
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 10u, 2u,
                        UINT32_C(0x8009a4f4), &R(NBA97_MATCH_INITIALIZE_V1)));
      set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x60000000));
      R(NBA97_MATCH_INITIALIZE_V0) =
          subtract_words(R(NBA97_MATCH_INITIALIZE_V0),
                         R(NBA97_MATCH_INITIALIZE_V1));
      TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x12u, 2u,
                   UINT32_C(0x8009a500), &R(NBA97_MATCH_INITIALIZE_V0)));
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 0x1bu, 1u,
                        UINT32_C(0x8009a504), &R(NBA97_MATCH_INITIALIZE_V0)));
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 0x1au, 1u,
                        UINT32_C(0x8009a508), &R(NBA97_MATCH_INITIALIZE_V1)));
      R(NBA97_MATCH_INITIALIZE_V0) =
          shift_left(R(NBA97_MATCH_INITIALIZE_V0), 16u);
      R(NBA97_MATCH_INITIALIZE_V1) =
          shift_left(R(NBA97_MATCH_INITIALIZE_V1), 8u);
      R(NBA97_MATCH_INITIALIZE_V1) =
          or_words(R(NBA97_MATCH_INITIALIZE_V1),
                   R(NBA97_MATCH_INITIALIZE_A0));
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 0x19u, 1u,
                        UINT32_C(0x8009a518), &R(NBA97_MATCH_INITIALIZE_A0)));
      R(NBA97_MATCH_INITIALIZE_V0) =
          or_words(R(NBA97_MATCH_INITIALIZE_V0),
                   R(NBA97_MATCH_INITIALIZE_V1));
      R(NBA97_MATCH_INITIALIZE_V0) =
          or_words(R(NBA97_MATCH_INITIALIZE_V0),
                   R(NBA97_MATCH_INITIALIZE_A0));
      TRY(write_at(run, NBA97_MATCH_INITIALIZE_A2, 0u, 4u,
                   UINT32_C(0x8009a524), &R(NBA97_MATCH_INITIALIZE_V0)));
      TRY(read_at(run, NBA97_MATCH_INITIALIZE_SP, 0x10u, 4u,
                  UINT32_C(0x8009a528), &R(NBA97_MATCH_INITIALIZE_V0)));
      R(NBA97_MATCH_INITIALIZE_A1) =
          add_words(R(NBA97_MATCH_INITIALIZE_A1),
                    R(NBA97_GAME_MATCH_CLOCKS_S1));
      TRY(write_at(run, NBA97_MATCH_INITIALIZE_A1, 0u, 4u,
                   UINT32_C(0x8009a530), &R(NBA97_MATCH_INITIALIZE_V0)));
      R(NBA97_MATCH_INITIALIZE_V0) =
          shift_left(R(NBA97_MATCH_INITIALIZE_T0), 2u);
      TRY(read_at(run, NBA97_MATCH_INITIALIZE_SP, 0x14u, 4u,
                  UINT32_C(0x8009a538), &R(NBA97_MATCH_INITIALIZE_V1)));
      R(NBA97_MATCH_INITIALIZE_V0) =
          add_words(R(NBA97_MATCH_INITIALIZE_V0),
                    R(NBA97_GAME_MATCH_CLOCKS_S1));
      TRY(write_at(run, NBA97_MATCH_INITIALIZE_V0, 0u, 4u,
                   UINT32_C(0x8009a540), &R(NBA97_MATCH_INITIALIZE_V1)));
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_SP, 0x10u, 2u,
                        UINT32_C(0x8009a544), &R(NBA97_MATCH_INITIALIZE_V0)));
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 8u, 2u,
                        UINT32_C(0x8009a548), &R(NBA97_MATCH_INITIALIZE_V1)));
      R(NBA97_MATCH_INITIALIZE_V0) =
          add_words(R(NBA97_MATCH_INITIALIZE_V0),
                    R(NBA97_MATCH_INITIALIZE_V1));
      TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x10u, 2u,
                   UINT32_C(0x8009a554), &R(NBA97_MATCH_INITIALIZE_V0)));
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_SP, 0x12u, 2u,
                        UINT32_C(0x8009a558), &R(NBA97_MATCH_INITIALIZE_V0)));
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 10u, 2u,
                        UINT32_C(0x8009a55c), &R(NBA97_MATCH_INITIALIZE_V1)));
      R(NBA97_MATCH_INITIALIZE_T0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_T0), 1u);
      R(NBA97_MATCH_INITIALIZE_V0) =
          add_words(R(NBA97_MATCH_INITIALIZE_V0),
                    R(NBA97_MATCH_INITIALIZE_V1));
      TRY(write_at(run, NBA97_MATCH_INITIALIZE_SP, 0x12u, 2u,
                   UINT32_C(0x8009a56c), &R(NBA97_MATCH_INITIALIZE_V0)));
    } else {
      /* 0x8009A570..0x8009A5C4: append the aligned 0x02 fill using the
       * original RGB byte load order and the stack rectangle words. */
      R(NBA97_MATCH_INITIALIZE_T0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_T0), 1u);
      R(NBA97_MATCH_INITIALIZE_A2) =
          shift_left(R(NBA97_MATCH_INITIALIZE_T0), 2u);
      R(NBA97_MATCH_INITIALIZE_T0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_T0), 1u);
      R(NBA97_MATCH_INITIALIZE_A3) =
          shift_left(R(NBA97_MATCH_INITIALIZE_T0), 2u);
      R(NBA97_MATCH_INITIALIZE_T0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_T0), 1u);
      R(NBA97_MATCH_INITIALIZE_A1) =
          add_words(R(NBA97_MATCH_INITIALIZE_A1),
                    R(NBA97_GAME_MATCH_CLOCKS_S1));
      set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x02000000));
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 0x1bu, 1u,
                        UINT32_C(0x8009a58c), &R(NBA97_MATCH_INITIALIZE_V0)));
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 0x1au, 1u,
                        UINT32_C(0x8009a590), &R(NBA97_MATCH_INITIALIZE_V1)));
      R(NBA97_MATCH_INITIALIZE_V0) =
          shift_left(R(NBA97_MATCH_INITIALIZE_V0), 16u);
      R(NBA97_MATCH_INITIALIZE_V1) =
          shift_left(R(NBA97_MATCH_INITIALIZE_V1), 8u);
      R(NBA97_MATCH_INITIALIZE_V1) =
          or_words(R(NBA97_MATCH_INITIALIZE_V1),
                   R(NBA97_MATCH_INITIALIZE_A0));
      TRY(load_unsigned(run, NBA97_MATCH_INITIALIZE_S0, 0x19u, 1u,
                        UINT32_C(0x8009a5a0), &R(NBA97_MATCH_INITIALIZE_A0)));
      R(NBA97_MATCH_INITIALIZE_V0) =
          or_words(R(NBA97_MATCH_INITIALIZE_V0),
                   R(NBA97_MATCH_INITIALIZE_V1));
      R(NBA97_MATCH_INITIALIZE_V0) =
          or_words(R(NBA97_MATCH_INITIALIZE_V0),
                   R(NBA97_MATCH_INITIALIZE_A0));
      TRY(write_at(run, NBA97_MATCH_INITIALIZE_A1, 0u, 4u,
                   UINT32_C(0x8009a5ac), &R(NBA97_MATCH_INITIALIZE_V0)));
      TRY(read_at(run, NBA97_MATCH_INITIALIZE_SP, 0x10u, 4u,
                  UINT32_C(0x8009a5b0), &R(NBA97_MATCH_INITIALIZE_V0)));
      R(NBA97_MATCH_INITIALIZE_A2) =
          add_words(R(NBA97_MATCH_INITIALIZE_A2),
                    R(NBA97_GAME_MATCH_CLOCKS_S1));
      TRY(write_at(run, NBA97_MATCH_INITIALIZE_A2, 0u, 4u,
                   UINT32_C(0x8009a5b8), &R(NBA97_MATCH_INITIALIZE_V0)));
      TRY(read_at(run, NBA97_MATCH_INITIALIZE_SP, 0x14u, 4u,
                  UINT32_C(0x8009a5bc), &R(NBA97_MATCH_INITIALIZE_V0)));
      R(NBA97_MATCH_INITIALIZE_A3) =
          add_words(R(NBA97_MATCH_INITIALIZE_A3),
                    R(NBA97_GAME_MATCH_CLOCKS_S1));
      TRY(write_at(run, NBA97_MATCH_INITIALIZE_A3, 0u, 4u,
                   UINT32_C(0x8009a5c4), &R(NBA97_MATCH_INITIALIZE_V0)));
    }
  }

  /* 0x8009A5C8..0x8009A5E4: write payload count, restore through live sp,
   * advance the frame, and refuse an unknown JR target after the epilogue. */
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_T0), UINT32_C(0xffffffff));
  TRY(write_at(run, NBA97_GAME_MATCH_CLOCKS_S1, 3u, 1u,
               UINT32_C(0x8009a5cc), &R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(restore(run, UINT32_C(0x8009a5d0), 0x20u, NBA97_MATCH_INITIALIZE_RA,
              &progress->restored_return_address));
  TRY(restore(run, UINT32_C(0x8009a5d4), 0x1cu,
              NBA97_GAME_MATCH_CLOCKS_S1, &progress->restored_s1));
  TRY(restore(run, UINT32_C(0x8009a5d8), 0x18u,
              NBA97_MATCH_INITIALIZE_S0, &progress->restored_s0));
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x28u);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15u) {
    stop(run, UINT32_C(0x8009a5e0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  progress->completed = 1u;
  stop(run, 0u, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
