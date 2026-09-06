#include "game_camera_overlay_packets.h"

#include <limits.h>
#include <string.h>

typedef struct Run {
  Nba97GameCameraOverlayPacketsContext *context;
  Nba97GameCameraOverlayPacketsProgress *progress;
  Nba97GameCameraOverlayPacketsMachine machine;
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

static void known(Nba97GameCameraOverlayPacketsWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 15u;
}

static int valid_machine(const Nba97GameCameraOverlayPacketsMachine *machine) {
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

static int initialize(Nba97GameCameraOverlayPacketsContext *context,
                      Nba97GameCameraOverlayPacketsProgress *progress,
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
  for (index = 0u; index != context->memory.count; ++index) {
    const Nba97GameTextRegion *region = &context->memory.region[index];
    if (region->data == NULL || region->size == 0u ||
        region->size > UINT64_C(0x100000000) ||
        (uint64_t)region->base + region->size > UINT64_C(0x100000000))
      return NBA97_TEXT_ARGUMENT;
    for (earlier = 0u; earlier != index; ++earlier) {
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

static Nba97GameCameraOverlayPacketsWord
add_words(Nba97GameCameraOverlayPacketsWord left,
          Nba97GameCameraOverlayPacketsWord right) {
  Nba97GameCameraOverlayPacketsWord result;
  unsigned carry_mask = 1u;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0u;
  if (left.known_mask == 15u && right.known_mask == 15u) {
    result.known_mask = 15u;
    return result;
  }
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned next_carry_mask = 0u, first_output = 0u, first = 1u;
    unsigned invariant = 1u;
    unsigned ls = (left.known_mask & (1u << byte))
                      ? (left.word >> (8u * byte)) & 255u
                      : 0u;
    unsigned le = (left.known_mask & (1u << byte)) ? ls : 255u;
    unsigned rs = (right.known_mask & (1u << byte))
                      ? (right.word >> (8u * byte)) & 255u
                      : 0u;
    unsigned re = (right.known_mask & (1u << byte)) ? rs : 255u;
    unsigned carry, a, b;
    for (carry = 0u; carry != 2u; ++carry) {
      if ((carry_mask & (1u << carry)) == 0u)
        continue;
      for (a = ls; a <= le; ++a)
        for (b = rs; b <= re; ++b) {
          unsigned sum = a + b + carry;
          next_carry_mask |= 1u << (sum >> 8u);
          if (first) {
            first_output = sum & 255u;
            first = 0u;
          } else if ((sum & 255u) != first_output) {
            invariant = 0u;
          }
        }
    }
    if (invariant)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
    carry_mask = next_carry_mask;
  }
  return result;
}

static Nba97GameCameraOverlayPacketsWord
add_constant(Nba97GameCameraOverlayPacketsWord value, uint32_t constant) {
  Nba97GameCameraOverlayPacketsWord right;
  known(&right, constant);
  return add_words(value, right);
}

static Nba97GameCameraOverlayPacketsWord
shift_left(Nba97GameCameraOverlayPacketsWord value, unsigned amount) {
  Nba97GameCameraOverlayPacketsWord result;
  unsigned byte;
  result.word = value.word << amount;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    int low_bit = (int)(byte * 8u) - (int)amount;
    int high_bit = low_bit + 7;
    int source, all_known = 1;
    if (high_bit < 0) {
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
      continue;
    }
    if (low_bit < 0)
      low_bit = 0;
    for (source = low_bit / 8; source <= high_bit / 8 && source < 4;
         ++source)
      if ((value.known_mask & (1u << source)) == 0u)
        all_known = 0;
    if (all_known)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static Nba97GameCameraOverlayPacketsWord
and_words(Nba97GameCameraOverlayPacketsWord left,
          Nba97GameCameraOverlayPacketsWord right) {
  Nba97GameCameraOverlayPacketsWord result;
  unsigned byte;
  result.word = left.word & right.word;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    uint32_t mask = UINT32_C(255) << (8u * byte);
    if (((left.known_mask & (1u << byte)) && (left.word & mask) == 0u) ||
        ((right.known_mask & (1u << byte)) && (right.word & mask) == 0u) ||
        ((left.known_mask & right.known_mask & (1u << byte)) != 0u))
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static Nba97GameCameraOverlayPacketsWord
and_immediate(Nba97GameCameraOverlayPacketsWord value, uint16_t immediate) {
  Nba97GameCameraOverlayPacketsWord right;
  known(&right, immediate);
  return and_words(value, right);
}

static int64_t signed_word(uint32_t word) {
  return word < UINT32_C(0x80000000) ? (int64_t)word
                                     : (int64_t)word - INT64_C(0x100000000);
}

static Nba97GameCameraOverlayPacketsWord
signed_less_than(Nba97GameCameraOverlayPacketsWord value, int32_t constant) {
  Nba97GameCameraOverlayPacketsWord result = {0u, 14u};
  uint32_t minimum = value.word, maximum = value.word;
  unsigned byte;
  if ((value.known_mask & 8u) == 0u) {
    minimum = UINT32_C(0x80000000);
    maximum = UINT32_C(0x7fffffff);
  } else {
    for (byte = 0u; byte != 4u; ++byte)
      if ((value.known_mask & (1u << byte)) == 0u) {
        minimum &= ~(UINT32_C(255) << (8u * byte));
        maximum |= UINT32_C(255) << (8u * byte);
      }
  }
  result.word = signed_word(value.word) < constant;
  if (signed_word(maximum) < constant || signed_word(minimum) >= constant)
    result.known_mask = 15u;
  return result;
}

static Nba97GameCameraOverlayPacketsWord
load_signed_half(Nba97GameCameraOverlayPacketsWord value) {
  uint32_t half = value.word & UINT32_C(0xffff);
  value.word = (half & UINT32_C(0x8000)) ? half | UINT32_C(0xffff0000) : half;
  value.known_mask = (uint8_t)((value.known_mask & 3u) |
                               ((value.known_mask & 2u) ? 12u : 0u));
  return value;
}

static Nba97GameCameraOverlayPacketsWord
load_unsigned_half(Nba97GameCameraOverlayPacketsWord value) {
  value.word &= UINT32_C(0xffff);
  value.known_mask = (uint8_t)((value.known_mask & 3u) | 12u);
  return value;
}

static Nba97GameCameraOverlayPacketsWord
load_unsigned_byte(Nba97GameCameraOverlayPacketsWord value) {
  value.word &= UINT32_C(0xff);
  value.known_mask = (uint8_t)((value.known_mask & 1u) | 14u);
  return value;
}

static int branch_zero(Run *run, Nba97GameCameraOverlayPacketsWord value,
                       uint32_t pc, int *zero) {
  unsigned byte;
  for (byte = 0u; byte != 4u; ++byte)
    if ((value.known_mask & (1u << byte)) != 0u &&
        (value.word & (UINT32_C(255) << (8u * byte))) != 0u) {
      *zero = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (value.known_mask == 15u) {
    *zero = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0u, 0u);
  return NBA97_TEXT_UNKNOWN;
}

static int branch_negative(Run *run, Nba97GameCameraOverlayPacketsWord value,
                           uint32_t pc, int *negative) {
  if ((value.known_mask & 8u) == 0u) {
    stop(run, pc, 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *negative = (value.word & UINT32_C(0x80000000)) != 0u;
  return NBA97_TEXT_COMPLETE;
}

static uint32_t width_mask(unsigned width) {
  return width == 4u ? UINT32_MAX
                     : (UINT32_C(1) << (width * 8u)) - UINT32_C(1);
}

static int spend(Run *run) {
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width,
                    const Nba97GameCameraOverlayPacketsWord *value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameCameraOverlayPacketsAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word & width_mask(width);
    event->operation = run->progress->operations;
    event->width = (uint8_t)width;
    event->known_mask =
        (uint8_t)(value->known_mask & ((1u << width) - 1u));
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
  for (index = 0u; index != run->context->memory.count; ++index) {
    Nba97GameTextRegion *region = &run->context->memory.region[index];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        width > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known_bytes = region->known ? region->known + (size_t)offset : NULL;
    if (*known_bytes)
      for (byte = 0u; byte != width; ++byte)
        if ((*known_bytes)[byte] > 1u)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int read_memory(Run *run, uint32_t address, unsigned width, uint32_t pc,
                       Nba97GameCameraOverlayPacketsWord *value) {
  Nba97GameCameraOverlayPacketsWord loaded = {0u, 0u};
  uint8_t *data, *known_bytes;
  unsigned byte;
  TRY(locate(run, address, width, pc, &data, &known_bytes));
  for (byte = 0u; byte != width; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (8u * byte);
    if (known_bytes == NULL || known_bytes[byte])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << byte));
  }
  *value = loaded;
  ++run->progress->reads;
  journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_memory(Run *run, uint32_t address, unsigned width, uint32_t pc,
                        const Nba97GameCameraOverlayPacketsWord *value) {
  uint8_t *data, *known_bytes;
  unsigned byte;
  TRY(locate(run, address, width, pc, &data, &known_bytes));
  if (known_bytes == NULL &&
      (value->known_mask & ((1u << width) - 1u)) != (1u << width) - 1u)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0u; byte != width; ++byte) {
    data[byte] = (uint8_t)(value->word >> (8u * byte));
    if (known_bytes)
      known_bytes[byte] = (uint8_t)((value->known_mask >> byte) & 1u);
  }
  ++run->progress->stores;
  journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int address_from(Run *run, Nba97GameCameraOverlayPacketsWord base,
                        uint32_t offset, uint32_t pc, uint32_t *address) {
  Nba97GameCameraOverlayPacketsWord sum = add_constant(base, offset);
  if (sum.known_mask != 15u) {
    stop(run, pc, sum.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = sum.word;
  return NBA97_TEXT_COMPLETE;
}

static int load_at(Run *run, unsigned base, uint32_t offset, unsigned width,
                   uint32_t pc, unsigned destination, int signed_load) {
  Nba97GameCameraOverlayPacketsWord value;
  uint32_t address;
  TRY(address_from(run, R(base), offset, pc, &address));
  TRY(read_memory(run, address, width, pc, &value));
  if (width == 2u)
    value = signed_load ? load_signed_half(value) : load_unsigned_half(value);
  else if (width == 1u)
    value = load_unsigned_byte(value);
  R(destination) = value;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int store_at(Run *run, unsigned base, uint32_t offset, unsigned width,
                    uint32_t pc, unsigned source) {
  uint32_t address;
  TRY(address_from(run, R(base), offset, pc, &address));
  return write_memory(run, address, width, pc, &R(source));
}

static int load_word(Run *run, uint32_t address, uint32_t pc, unsigned reg) {
  return read_memory(run, address, 4u, pc, &R(reg));
}

static int load_half(Run *run, uint32_t address, uint32_t pc, unsigned reg,
                     int is_signed) {
  Nba97GameCameraOverlayPacketsWord value;
  TRY(read_memory(run, address, 2u, pc, &value));
  R(reg) = is_signed ? load_signed_half(value) : load_unsigned_half(value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int load_byte(Run *run, uint32_t address, uint32_t pc, unsigned reg) {
  Nba97GameCameraOverlayPacketsWord value;
  TRY(read_memory(run, address, 1u, pc, &value));
  R(reg) = load_unsigned_byte(value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int store_half(Run *run, uint32_t address, uint32_t pc, unsigned reg) {
  return write_memory(run, address, 2u, pc, &R(reg));
}

static int call_child(Run *run, uint32_t pc, uint32_t entry, uint8_t kind,
                      uint8_t argument_count) {
  Nba97GameCameraOverlayPacketsEvent event;
  int status;
  size_t invocation = run->progress->call_count[kind];
  stop(run, pc, 0u, entry);
  TRY(spend(run));
  if (run->context->io == NULL)
    return NBA97_TEXT_IO_REFUSED;
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = entry;
  event.operation = run->progress->operations;
  event.invocation = invocation;
  event.kind = kind;
  event.argument_count = argument_count;
  status = run->context->io(run->context->user, &run->context->memory, &event,
                            &run->machine);
  if (status != NBA97_TEXT_COMPLETE)
    return status;
  if (!valid_machine(&run->machine)) {
    publish(run);
    return NBA97_TEXT_ARGUMENT;
  }
  ++run->progress->callbacks_completed;
  ++run->progress->call_count[kind];
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int rectangle(Run *run, int secondary) {
  const uint32_t x_address = secondary ? UINT32_C(0x800fe8dc)
                                       : UINT32_C(0x800fe8d8);
  const uint32_t y_address = x_address + 2u;
  const uint32_t load_x_pc = secondary ? UINT32_C(0x80075dd8)
                                       : UINT32_C(0x80075f24);
  const uint32_t load_y_pc = secondary ? UINT32_C(0x80075df0)
                                       : UINT32_C(0x80075f3c);
  const uint32_t p = secondary ? UINT32_C(0x80075e04)
                               : UINT32_C(0x80075f50);
  Nba97GameCameraOverlayPacketsWord x, y;
  int zero;

  /* 75DD4..75F18 / 75F1C..7605C: source-ordered secondary
   * and primary rectangle construction. */
  if (secondary) {
    known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80100000));
    TRY(load_half(run, x_address, load_x_pc, NBA97_MATCH_INITIALIZE_A1, 1));
  } else {
    known(&R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0x80100000));
    R(NBA97_MATCH_INITIALIZE_A3) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0xffffe8d8));
    TRY(load_at(run, NBA97_MATCH_INITIALIZE_A3, 0u, 2u, load_x_pc,
                NBA97_MATCH_INITIALIZE_A1, 1));
  }
  R(NBA97_MATCH_INITIALIZE_V0) =
      signed_less_than(R(NBA97_MATCH_INITIALIZE_A1), 56);
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0), load_x_pc + 12u, &zero));
  if (zero)
    return NBA97_TEXT_COMPLETE;
  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(load_half(run, y_address, load_y_pc, NBA97_MATCH_INITIALIZE_V0, 1));
  R(NBA97_MATCH_INITIALIZE_V0) =
      signed_less_than(R(NBA97_MATCH_INITIALIZE_V0), 56);
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0), load_y_pc + 12u, &zero));
  if (zero)
    return NBA97_TEXT_COMPLETE;
  x = R(NBA97_MATCH_INITIALIZE_A1);

  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(load_half(run, UINT32_C(0x800fe8f8), p + 4u,
                NBA97_MATCH_INITIALIZE_V0, 1));
  known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
  TRY(load_word(run, UINT32_C(0x8001ede8), p + 12u,
                NBA97_MATCH_INITIALIZE_A0));
  R(NBA97_MATCH_INITIALIZE_V0) =
      shift_left(R(NBA97_MATCH_INITIALIZE_V0), 2u);
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800c0000));
  R(NBA97_MATCH_INITIALIZE_AT) =
      add_words(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(load_at(run, NBA97_MATCH_INITIALIZE_AT, UINT32_C(0xffffc160), 2u,
              p + 28u, NBA97_MATCH_INITIALIZE_V1, 0));
  R(NBA97_MATCH_INITIALIZE_V0) = shift_left(x, 1u);
  R(NBA97_MATCH_INITIALIZE_A2) =
      shift_left(R(NBA97_MATCH_INITIALIZE_A0), 2u);
  R(NBA97_MATCH_INITIALIZE_A2) =
      add_words(R(NBA97_MATCH_INITIALIZE_A2), R(NBA97_MATCH_INITIALIZE_A0));
  R(NBA97_MATCH_INITIALIZE_A2) =
      shift_left(R(NBA97_MATCH_INITIALIZE_A2), 3u);
  known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80100000));
  R(NBA97_MATCH_INITIALIZE_A1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0xffff9c58));
  if (secondary)
    R(NBA97_MATCH_INITIALIZE_A3) =
        add_words(R(NBA97_MATCH_INITIALIZE_A2), R(NBA97_MATCH_INITIALIZE_A1));
  else
    R(NBA97_MATCH_INITIALIZE_A1) =
        add_words(R(NBA97_MATCH_INITIALIZE_A2), R(NBA97_MATCH_INITIALIZE_A1));
  R(NBA97_MATCH_INITIALIZE_V1) =
      add_words(R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0));
  R(NBA97_MATCH_INITIALIZE_V1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V1), 4u);
  TRY(store_at(run,
               secondary ? NBA97_MATCH_INITIALIZE_A3
                         : NBA97_MATCH_INITIALIZE_A1,
               secondary ? 0x68u : 0x18u, 2u, p + 68u,
               NBA97_MATCH_INITIALIZE_V1));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  R(NBA97_MATCH_INITIALIZE_AT) =
      add_words(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_A2));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_AT,
               secondary ? UINT32_C(0xffff9cb0) : UINT32_C(0xffff9c60), 2u,
               p + 80u, NBA97_MATCH_INITIALIZE_V1));

  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(load_half(run, UINT32_C(0x800fe8f8), p + 88u,
                NBA97_MATCH_INITIALIZE_V0, 1));
  if (secondary) {
    known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
    TRY(load_half(run, x_address, p + 96u, NBA97_MATCH_INITIALIZE_V1, 1));
  } else {
    TRY(load_at(run, NBA97_MATCH_INITIALIZE_A3, 0u, 2u, p + 92u,
                NBA97_MATCH_INITIALIZE_V1, 1));
  }
  R(NBA97_MATCH_INITIALIZE_V0) =
      shift_left(R(NBA97_MATCH_INITIALIZE_V0), 2u);
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800c0000));
  R(NBA97_MATCH_INITIALIZE_AT) =
      add_words(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(load_at(run, NBA97_MATCH_INITIALIZE_AT, UINT32_C(0xffffc160), 2u,
              p + (secondary ? 112u : 108u), NBA97_MATCH_INITIALIZE_V0, 0));
  R(NBA97_MATCH_INITIALIZE_V1) =
      shift_left(R(NBA97_MATCH_INITIALIZE_V1), 1u);
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_words(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V0), 15u);
  TRY(store_at(run,
               secondary ? NBA97_MATCH_INITIALIZE_A3
                         : NBA97_MATCH_INITIALIZE_A1,
               secondary ? 0x70u : 0x20u, 2u,
               p + (secondary ? 128u : 124u), NBA97_MATCH_INITIALIZE_V0));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  R(NBA97_MATCH_INITIALIZE_AT) =
      add_words(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_A2));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_AT,
               secondary ? UINT32_C(0xffff9cb8) : UINT32_C(0xffff9c68), 2u,
               p + (secondary ? 140u : 136u), NBA97_MATCH_INITIALIZE_V0));

  known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
  TRY(load_half(run, UINT32_C(0x800fe8f8),
                p + (secondary ? 148u : 144u),
                NBA97_MATCH_INITIALIZE_V1, 1));
  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(load_half(run, y_address, p + (secondary ? 156u : 152u),
                NBA97_MATCH_INITIALIZE_V0, 0));
  y = R(NBA97_MATCH_INITIALIZE_V0);
  R(NBA97_MATCH_INITIALIZE_V1) =
      shift_left(R(NBA97_MATCH_INITIALIZE_V1), 2u);
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800c0000));
  R(NBA97_MATCH_INITIALIZE_AT) =
      add_words(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_V1));
  TRY(load_at(run, NBA97_MATCH_INITIALIZE_AT, UINT32_C(0xffffc162), 2u,
              p + (secondary ? 172u : 168u), NBA97_MATCH_INITIALIZE_V1, 0));
  R(NBA97_MATCH_INITIALIZE_V0) = add_words(y, R(NBA97_MATCH_INITIALIZE_V1));
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V0), 3u);
  TRY(store_at(run,
               secondary ? NBA97_MATCH_INITIALIZE_A3
                         : NBA97_MATCH_INITIALIZE_A1,
               secondary ? 0x62u : 0x12u, 2u,
               p + (secondary ? 188u : 184u), NBA97_MATCH_INITIALIZE_V0));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  R(NBA97_MATCH_INITIALIZE_AT) =
      add_words(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_A2));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_AT,
               secondary ? UINT32_C(0xffff9cb2) : UINT32_C(0xffff9c62), 2u,
               p + (secondary ? 200u : 196u), NBA97_MATCH_INITIALIZE_V0));

  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(load_half(run, UINT32_C(0x800fe8f8),
                p + (secondary ? 208u : 204u),
                NBA97_MATCH_INITIALIZE_V0, 1));
  known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
  TRY(load_half(run, y_address, p + (secondary ? 216u : 212u),
                NBA97_MATCH_INITIALIZE_V1, 0));
  R(NBA97_MATCH_INITIALIZE_V0) =
      shift_left(R(NBA97_MATCH_INITIALIZE_V0), 2u);
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800c0000));
  R(NBA97_MATCH_INITIALIZE_AT) =
      add_words(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(load_at(run, NBA97_MATCH_INITIALIZE_AT, UINT32_C(0xffffc162), 2u,
              p + (secondary ? 232u : 228u), NBA97_MATCH_INITIALIZE_V0, 0));
  known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80100000));
  TRY(load_word(run, UINT32_C(0x80102924),
                p + (secondary ? 240u : 236u),
                NBA97_MATCH_INITIALIZE_A0));
  if (secondary)
    R(NBA97_MATCH_INITIALIZE_A1) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A1), 0x50u);
  R(NBA97_MATCH_INITIALIZE_V1) =
      add_words(R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0));
  R(NBA97_MATCH_INITIALIZE_V1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V1), 10u);
  TRY(store_at(run,
               secondary ? NBA97_MATCH_INITIALIZE_A3
                         : NBA97_MATCH_INITIALIZE_A1,
               secondary ? 0x72u : 0x22u, 2u,
               p + (secondary ? 256u : 248u), NBA97_MATCH_INITIALIZE_V1));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  R(NBA97_MATCH_INITIALIZE_AT) =
      add_words(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_A2));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_AT,
               secondary ? UINT32_C(0xffff9cc2) : UINT32_C(0xffff9c72), 2u,
               p + (secondary ? 268u : 260u), NBA97_MATCH_INITIALIZE_V1));
  known(&R(NBA97_MATCH_INITIALIZE_RA),
        secondary ? UINT32_C(0x80075f1c) : UINT32_C(0x80076060));
  if (secondary)
    R(NBA97_MATCH_INITIALIZE_A1) =
        add_words(R(NBA97_MATCH_INITIALIZE_A2), R(NBA97_MATCH_INITIALIZE_A1));
  TRY(call_child(run, secondary ? UINT32_C(0x80075f14)
                                : UINT32_C(0x80076058),
                 UINT32_C(0x80056914),
                 NBA97_GAME_CAMERA_OVERLAY_PACKETS_LINK_80056914, 2u));
  return NBA97_TEXT_COMPLETE;
}

static int overlay(Run *run) {
  int zero, negative;
  Nba97GameCameraOverlayPacketsWord saved;

  /* Entry, frame creation, and state-selection route. */
  known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
  TRY(load_half(run, UINT32_C(0x800fe8cc), UINT32_C(0x80075d44),
                NBA97_MATCH_INITIALIZE_V1, 1));
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffb8));
  run->progress->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_SP, 0x40u, 4u,
               UINT32_C(0x80075d4c), NBA97_MATCH_INITIALIZE_RA));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_SP, 0x3cu, 4u,
               UINT32_C(0x80075d50), NBA97_GAME_MATCH_CLOCKS_S2 + 1u));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_SP, 0x38u, 4u,
               UINT32_C(0x80075d54), NBA97_GAME_MATCH_CLOCKS_S2));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_SP, 0x34u, 4u,
               UINT32_C(0x80075d58), NBA97_GAME_MATCH_CLOCKS_S1));
  R(NBA97_MATCH_INITIALIZE_V0) =
      signed_less_than(R(NBA97_MATCH_INITIALIZE_V1), 3);
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_SP, 0x30u, 4u,
               UINT32_C(0x80075d64), NBA97_MATCH_INITIALIZE_S0));
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0),
                  UINT32_C(0x80075d60), &zero));
  if (!zero)
    goto alternate_check;
  known(&R(NBA97_MATCH_INITIALIZE_V0), 7u);
  saved = add_constant(R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xfffffff9));
  TRY(branch_zero(run, saved, UINT32_C(0x80075d6c), &zero));
  if (zero)
    goto alternate_check;
  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
  TRY(load_byte(run, UINT32_C(0x800bc1f0), UINT32_C(0x80075d78),
                NBA97_MATCH_INITIALIZE_V0));
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0),
                  UINT32_C(0x80075d80), &zero));
  if (zero)
    goto alternate_check;
  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(load_half(run, UINT32_C(0x800fe8ca), UINT32_C(0x80075d8c),
                NBA97_MATCH_INITIALIZE_V0, 1));
  R(NBA97_MATCH_INITIALIZE_V0) =
      shift_left(R(NBA97_MATCH_INITIALIZE_V0), 2u);
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
  R(NBA97_MATCH_INITIALIZE_AT) =
      add_words(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(load_at(run, NBA97_MATCH_INITIALIZE_AT, 0xbecu, 4u,
              UINT32_C(0x80075da0), NBA97_MATCH_INITIALIZE_V0, 0));
  TRY(load_at(run, NBA97_MATCH_INITIALIZE_V0, 4u, 2u,
              UINT32_C(0x80075da8), NBA97_MATCH_INITIALIZE_V0, 1));
  TRY(branch_negative(run, R(NBA97_MATCH_INITIALIZE_V0),
                      UINT32_C(0x80075db0), &negative));
  if (negative)
    goto alternate_check;
  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(load_half(run, UINT32_C(0x800f9ffe), UINT32_C(0x80075dbc),
                NBA97_MATCH_INITIALIZE_V0, 1));
  saved = R(NBA97_MATCH_INITIALIZE_V0);
  R(NBA97_MATCH_INITIALIZE_V0) =
      signed_less_than(R(NBA97_MATCH_INITIALIZE_V1), 5);
  TRY(branch_zero(run, saved, UINT32_C(0x80075dc4), &zero));
  if (!zero)
    goto alternate;
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0),
                  UINT32_C(0x80075dcc), &zero));
  if (zero)
    TRY(rectangle(run, 1));
  TRY(rectangle(run, 0));
  known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80100000));
  TRY(load_word(run, UINT32_C(0x80102924), UINT32_C(0x80076064),
                NBA97_MATCH_INITIALIZE_A0));
  known(&R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0x80100000));
  R(NBA97_MATCH_INITIALIZE_S0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0xffffa25c));
  known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x80076078));
  R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_S0);
  TRY(call_child(run, UINT32_C(0x80076070), UINT32_C(0x80056914),
                 NBA97_GAME_CAMERA_OVERLAY_PACKETS_LINK_80056914, 2u));
  known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80100000));
  TRY(load_word(run, UINT32_C(0x80102924), UINT32_C(0x8007607c),
                NBA97_MATCH_INITIALIZE_A0));
  known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x80076088));
  R(NBA97_MATCH_INITIALIZE_A1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_S0), 0x28u);
  TRY(call_child(run, UINT32_C(0x80076080), UINT32_C(0x80056914),
                 NBA97_GAME_CAMERA_OVERLAY_PACKETS_LINK_80056914, 2u));

alternate_check:
  /* 76088..76124: alternate overlay gate and callback-live mask loop. */
  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(load_half(run, UINT32_C(0x800f9ffe), UINT32_C(0x8007608c),
                NBA97_MATCH_INITIALIZE_V0, 1));
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0),
                  UINT32_C(0x80076094), &zero));
  if (zero)
    goto epilogue;

alternate:
  known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80100000));
  R(NBA97_MATCH_INITIALIZE_A1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0xffffa038));
  TRY(load_half(run, R(NBA97_MATCH_INITIALIZE_A1).word,
                UINT32_C(0x800760a4), NBA97_MATCH_INITIALIZE_V0, 1));
  known(&R(NBA97_GAME_MATCH_CLOCKS_S2), 1u);
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0),
                  UINT32_C(0x800760ac), &zero));
  if (!zero)
    goto epilogue;
  known(&R(NBA97_GAME_MATCH_CLOCKS_S1), 0u);
  R(NBA97_GAME_MATCH_CLOCKS_S2 + 1u) =
      add_constant(R(NBA97_MATCH_INITIALIZE_A1), 0x18u);
  R(NBA97_MATCH_INITIALIZE_S0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_A1), 0x1cu);
  do {
    TRY(load_at(run, NBA97_GAME_MATCH_CLOCKS_S2 + 1u, 0u, 4u,
                UINT32_C(0x800760c0), NBA97_MATCH_INITIALIZE_V0, 0));
    R(NBA97_MATCH_INITIALIZE_V0) =
        and_words(R(NBA97_MATCH_INITIALIZE_V0),
                  R(NBA97_GAME_MATCH_CLOCKS_S2));
    TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0),
                    UINT32_C(0x800760cc), &zero));
    if (!zero) {
      known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80100000));
      TRY(load_word(run, UINT32_C(0x80102924), UINT32_C(0x800760d8),
                    NBA97_MATCH_INITIALIZE_A0));
      known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x800760e4));
      R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_S0);
      TRY(call_child(run, UINT32_C(0x800760dc), UINT32_C(0x80056914),
                     NBA97_GAME_CAMERA_OVERLAY_PACKETS_LINK_80056914, 2u));
    }
    R(NBA97_GAME_MATCH_CLOCKS_S2) =
        shift_left(R(NBA97_GAME_MATCH_CLOCKS_S2), 1u);
    R(NBA97_GAME_MATCH_CLOCKS_S1) =
        add_constant(R(NBA97_GAME_MATCH_CLOCKS_S1), 1u);
    R(NBA97_MATCH_INITIALIZE_V0) =
        signed_less_than(R(NBA97_GAME_MATCH_CLOCKS_S1), 13);
    R(NBA97_MATCH_INITIALIZE_S0) =
        add_constant(R(NBA97_MATCH_INITIALIZE_S0), 0x28u);
    TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0),
                    UINT32_C(0x800760f0), &zero));
  } while (!zero);
  known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80100000));
  TRY(load_word(run, UINT32_C(0x80102924), UINT32_C(0x800760fc),
                NBA97_MATCH_INITIALIZE_A0));
  known(&R(NBA97_GAME_MATCH_CLOCKS_S2), UINT32_C(0x80100000));
  R(NBA97_GAME_MATCH_CLOCKS_S2) =
      add_constant(R(NBA97_GAME_MATCH_CLOCKS_S2), UINT32_C(0xffffa054));
  known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x80076110));
  R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_GAME_MATCH_CLOCKS_S2);
  TRY(call_child(run, UINT32_C(0x80076108), UINT32_C(0x80056914),
                 NBA97_GAME_CAMERA_OVERLAY_PACKETS_LINK_80056914, 2u));
  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(load_half(run, UINT32_C(0x800f9ffe), UINT32_C(0x80076114),
                NBA97_MATCH_INITIALIZE_V0, 0));
  R(NBA97_MATCH_INITIALIZE_V0) =
      and_immediate(R(NBA97_MATCH_INITIALIZE_V0), 0x80u);
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0),
                  UINT32_C(0x80076120), &zero));
  if (zero)
    goto epilogue;

  /* Optional projected quad. */
  known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
  TRY(load_word(run, UINT32_C(0x800fe770), UINT32_C(0x8007612c),
                NBA97_MATCH_INITIALIZE_V1));
  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(load_word(run, UINT32_C(0x800fe774), UINT32_C(0x80076134),
                NBA97_MATCH_INITIALIZE_V0));
  known(&R(NBA97_GAME_MATCH_CLOCKS_S1), UINT32_C(0x80100000));
  R(NBA97_GAME_MATCH_CLOCKS_S1) =
      add_constant(R(NBA97_GAME_MATCH_CLOCKS_S1), UINT32_C(0xffff9fd8));
  known(&R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0x800e0000));
  R(NBA97_MATCH_INITIALIZE_S0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0xffff8ef4));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800e0000));
  TRY(write_memory(run, UINT32_C(0x800d8ef6), 2u, UINT32_C(0x8007614c),
                   &R(0)));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800e0000));
  TRY(write_memory(run, UINT32_C(0x800d8efe), 2u, UINT32_C(0x80076154),
                   &R(0)));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800e0000));
  TRY(write_memory(run, UINT32_C(0x800d8f06), 2u, UINT32_C(0x8007615c),
                   &R(0)));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800e0000));
  TRY(write_memory(run, UINT32_C(0x800d8f0e), 2u, UINT32_C(0x80076164),
                   &R(0)));
  R(NBA97_MATCH_INITIALIZE_V1) =
      shift_left(R(NBA97_MATCH_INITIALIZE_V1), 3u);
  R(NBA97_MATCH_INITIALIZE_A2) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xffffff80));
  R(NBA97_MATCH_INITIALIZE_V0) =
      shift_left(R(NBA97_MATCH_INITIALIZE_V0), 3u);
  R(NBA97_MATCH_INITIALIZE_A1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V0), 0x80u);
  R(NBA97_MATCH_INITIALIZE_V1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V1), 0x80u);
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xffffff80));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_S0, 0u, 2u,
               UINT32_C(0x80076180), NBA97_MATCH_INITIALIZE_A2));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800e0000));
  TRY(store_half(run, UINT32_C(0x800d8ef8), UINT32_C(0x80076188),
                 NBA97_MATCH_INITIALIZE_A1));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_S0, 8u, 2u,
               UINT32_C(0x8007618c), NBA97_MATCH_INITIALIZE_V1));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800e0000));
  TRY(store_half(run, UINT32_C(0x800d8f00), UINT32_C(0x80076194),
                 NBA97_MATCH_INITIALIZE_A1));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_S0, 0x10u, 2u,
               UINT32_C(0x80076198), NBA97_MATCH_INITIALIZE_A2));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800e0000));
  TRY(store_half(run, UINT32_C(0x800d8f08), UINT32_C(0x800761a0),
                 NBA97_MATCH_INITIALIZE_V0));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_S0, 0x18u, 2u,
               UINT32_C(0x800761a4), NBA97_MATCH_INITIALIZE_V1));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800e0000));
  TRY(store_half(run, UINT32_C(0x800d8f10), UINT32_C(0x800761ac),
                 NBA97_MATCH_INITIALIZE_V0));
  known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x800761b8));
  R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_GAME_MATCH_CLOCKS_S1);
  TRY(call_child(run, UINT32_C(0x800761b0), UINT32_C(0x80055f18),
                 NBA97_GAME_CAMERA_OVERLAY_PACKETS_SET_ROTATION_80055F18, 1u));
  known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x800761c0));
  R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_GAME_MATCH_CLOCKS_S1);
  TRY(call_child(run, UINT32_C(0x800761b8), UINT32_C(0x80055f44),
                 NBA97_GAME_CAMERA_OVERLAY_PACKETS_SET_TRANSLATION_80055F44,
                 1u));
  known(&R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x80020000));
  TRY(load_word(run, UINT32_C(0x8001ede8), UINT32_C(0x800761c4),
                NBA97_MATCH_INITIALIZE_A2));
  R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
  R(NBA97_MATCH_INITIALIZE_A1) = add_constant(R(NBA97_MATCH_INITIALIZE_A0), 8u);
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x28u);
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_SP, 0x20u, 4u,
               UINT32_C(0x800761d4), NBA97_MATCH_INITIALIZE_V0));
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x2cu);
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_SP, 0x24u, 4u,
               UINT32_C(0x800761dc), NBA97_MATCH_INITIALIZE_V0));
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_GAME_MATCH_CLOCKS_S2), 0x260u);
  R(NBA97_MATCH_INITIALIZE_A3) =
      add_constant(R(NBA97_MATCH_INITIALIZE_A0), 0x18u);
  R(NBA97_MATCH_INITIALIZE_V1) =
      shift_left(R(NBA97_MATCH_INITIALIZE_A2), 2u);
  R(NBA97_MATCH_INITIALIZE_V1) =
      add_words(R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_A2));
  R(NBA97_MATCH_INITIALIZE_V1) =
      shift_left(R(NBA97_MATCH_INITIALIZE_V1), 3u);
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_words(R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_SP, 0x10u, 4u,
               UINT32_C(0x800761f8), NBA97_MATCH_INITIALIZE_V0));
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_GAME_MATCH_CLOCKS_S2), 0x268u);
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_words(R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_SP, 0x14u, 4u,
               UINT32_C(0x80076204), NBA97_MATCH_INITIALIZE_V0));
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_GAME_MATCH_CLOCKS_S2), 0x270u);
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_words(R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_SP, 0x18u, 4u,
               UINT32_C(0x80076210), NBA97_MATCH_INITIALIZE_V0));
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_GAME_MATCH_CLOCKS_S2), 0x278u);
  R(NBA97_MATCH_INITIALIZE_V1) =
      add_words(R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0));
  R(NBA97_MATCH_INITIALIZE_A2) =
      add_constant(R(NBA97_MATCH_INITIALIZE_A0), 0x10u);
  known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x80076228));
  TRY(store_at(run, NBA97_MATCH_INITIALIZE_SP, 0x1cu, 4u,
               UINT32_C(0x80076224), NBA97_MATCH_INITIALIZE_V1));
  TRY(call_child(run, UINT32_C(0x80076220), UINT32_C(0x80055fe4),
                 NBA97_GAME_CAMERA_OVERLAY_PACKETS_PROJECT_QUAD_80055FE4,
                 10u));
  known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80100000));
  TRY(load_word(run, UINT32_C(0x80102924), UINT32_C(0x8007622c),
                NBA97_MATCH_INITIALIZE_A0));
  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
  TRY(load_word(run, UINT32_C(0x8001ede8), UINT32_C(0x80076234),
                NBA97_MATCH_INITIALIZE_V0));
  R(NBA97_MATCH_INITIALIZE_A0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_A0), 0x3ff8u);
  R(NBA97_MATCH_INITIALIZE_A1) =
      shift_left(R(NBA97_MATCH_INITIALIZE_V0), 2u);
  R(NBA97_MATCH_INITIALIZE_A1) =
      add_words(R(NBA97_MATCH_INITIALIZE_A1), R(NBA97_MATCH_INITIALIZE_V0));
  R(NBA97_MATCH_INITIALIZE_A1) =
      shift_left(R(NBA97_MATCH_INITIALIZE_A1), 3u);
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_GAME_MATCH_CLOCKS_S2), 0x258u);
  known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x80076254));
  R(NBA97_MATCH_INITIALIZE_A1) =
      add_words(R(NBA97_MATCH_INITIALIZE_A1), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(call_child(run, UINT32_C(0x8007624c), UINT32_C(0x80056914),
                 NBA97_GAME_CAMERA_OVERLAY_PACKETS_LINK_80056914, 2u));

epilogue:
  /* 76254..76270: restore through the callback-live stack pointer. */
  TRY(load_at(run, NBA97_MATCH_INITIALIZE_SP, 0x40u, 4u,
              UINT32_C(0x80076254), NBA97_MATCH_INITIALIZE_RA, 0));
  run->progress->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  TRY(load_at(run, NBA97_MATCH_INITIALIZE_SP, 0x3cu, 4u,
              UINT32_C(0x80076258), NBA97_GAME_MATCH_CLOCKS_S2 + 1u, 0));
  run->progress->restored_s3 = R(NBA97_GAME_MATCH_CLOCKS_S2 + 1u);
  TRY(load_at(run, NBA97_MATCH_INITIALIZE_SP, 0x38u, 4u,
              UINT32_C(0x8007625c), NBA97_GAME_MATCH_CLOCKS_S2, 0));
  run->progress->restored_s2 = R(NBA97_GAME_MATCH_CLOCKS_S2);
  TRY(load_at(run, NBA97_MATCH_INITIALIZE_SP, 0x34u, 4u,
              UINT32_C(0x80076260), NBA97_GAME_MATCH_CLOCKS_S1, 0));
  run->progress->restored_s1 = R(NBA97_GAME_MATCH_CLOCKS_S1);
  TRY(load_at(run, NBA97_MATCH_INITIALIZE_SP, 0x30u, 4u,
              UINT32_C(0x80076264), NBA97_MATCH_INITIALIZE_S0, 0));
  run->progress->restored_s0 = R(NBA97_MATCH_INITIALIZE_S0);
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x48u);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15u) {
    stop(run, UINT32_C(0x8007626c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_camera_overlay_packets(
    Nba97GameCameraOverlayPacketsContext *context,
    Nba97GameCameraOverlayPacketsProgress *progress) {
  Run run;
  int status = initialize(context, progress, &run);
  if (status != NBA97_TEXT_COMPLETE)
    return status;
  status = overlay(&run);
  publish(&run);
  if (status == NBA97_TEXT_COMPLETE) {
    progress->completed = 1u;
    progress->stopped_pc = 0u;
    progress->stopped_address = 0u;
    progress->stopped_entry = 0u;
  }
  return status;
}
