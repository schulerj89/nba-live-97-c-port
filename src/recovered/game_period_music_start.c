#include "game_period_music_start.h"

#include <string.h>

typedef struct Run {
  Nba97GamePeriodMusicStartContext *context;
  Nba97GamePeriodMusicStartProgress *progress;
  Nba97GamePeriodMusicStartMachine machine;
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

static void set_known(Nba97GamePeriodMusicStartWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 15u;
}

static int valid_registers(const Nba97GamePeriodMusicStartMachine *machine) {
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

static int initialize(Nba97GamePeriodMusicStartContext *context,
                      Nba97GamePeriodMusicStartProgress *progress, Run *run) {
  if (progress == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof(*progress));
  if (context == NULL || !valid_memory(&context->memory) ||
      !valid_registers(&context->machine) ||
      (context->access_journal_capacity != 0u &&
       context->access_journal == NULL))
    return NBA97_TEXT_ARGUMENT;
  run->context = context;
  run->progress = progress;
  run->machine = context->machine;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static Nba97GamePeriodMusicStartWord
add_words(Nba97GamePeriodMusicStartWord left,
          Nba97GamePeriodMusicStartWord right) {
  Nba97GamePeriodMusicStartWord result;
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
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
    carry_mask = next_carry_mask;
  }
  return result;
}

static Nba97GamePeriodMusicStartWord
add_constant(Nba97GamePeriodMusicStartWord value, uint32_t constant) {
  Nba97GamePeriodMusicStartWord immediate;
  set_known(&immediate, constant);
  return add_words(value, immediate);
}

static int spend(Run *run) {
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  return NBA97_TEXT_COMPLETE;
}

static uint32_t width_mask(unsigned width) {
  return width == 4u ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - 1u;
}

static uint8_t known_width_mask(unsigned width) {
  return (uint8_t)((1u << width) - 1u);
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width,
                    const Nba97GamePeriodMusicStartWord *value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GamePeriodMusicStartAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word & width_mask(width);
    event->operation = run->progress->operations;
    event->width = (uint8_t)width;
    event->known_mask = (uint8_t)(value->known_mask & known_width_mask(width));
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t address, unsigned width, uint32_t pc,
                  uint8_t **data, uint8_t **known) {
  size_t index;
  stop(run, pc, address, 0u);
  TRY(spend(run));
  ++run->progress->accesses;
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

static int effective_address(Run *run, Nba97GamePeriodMusicStartWord base,
                             uint32_t offset, uint32_t pc, uint32_t *address) {
  if (base.known_mask != 15u) {
    stop(run, pc, 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = base.word + offset;
  return NBA97_TEXT_COMPLETE;
}

static int read_value(Run *run, Nba97GamePeriodMusicStartWord base,
                      uint32_t offset, unsigned width, uint32_t pc,
                      Nba97GamePeriodMusicStartWord *destination) {
  uint32_t address;
  uint8_t *data;
  uint8_t *known;
  Nba97GamePeriodMusicStartWord loaded;
  unsigned byte;
  TRY(effective_address(run, base, offset, pc, &address));
  TRY(locate(run, address, width, pc, &data, &known));
  loaded.word = 0u;
  loaded.known_mask = 0u;
  for (byte = 0u; byte != width; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (8u * byte);
    if (known == NULL || known[byte] != 0u)
      loaded.known_mask = (uint8_t)(loaded.known_mask | (uint8_t)(1u << byte));
  }
  *destination = loaded;
  ++run->progress->reads;
  journal(run, NBA97_GAME_PERIOD_MUSIC_START_READ, pc, address, width, &loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, Nba97GamePeriodMusicStartWord base,
                       uint32_t offset, unsigned width, uint32_t pc,
                       Nba97GamePeriodMusicStartWord value) {
  uint32_t address;
  uint8_t *data;
  uint8_t *known;
  unsigned byte;
  value.word &= width_mask(width);
  value.known_mask = (uint8_t)(value.known_mask & known_width_mask(width));
  TRY(effective_address(run, base, offset, pc, &address));
  TRY(locate(run, address, width, pc, &data, &known));
  if (known == NULL && value.known_mask != known_width_mask(width))
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0u; byte != width; ++byte) {
    data[byte] = (uint8_t)(value.word >> (8u * byte));
    if (known != NULL)
      known[byte] = (uint8_t)((value.known_mask >> byte) & 1u);
  }
  ++run->progress->stores;
  journal(run, NBA97_GAME_PERIOD_MUSIC_START_STORE, pc, address, width, &value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static Nba97GamePeriodMusicStartWord
zero_extend_byte(Nba97GamePeriodMusicStartWord value) {
  value.word &= 255u;
  value.known_mask = (uint8_t)((value.known_mask & 1u) | 14u);
  return value;
}

static int branch_zero(Run *run, Nba97GamePeriodMusicStartWord value,
                       uint32_t pc, int *zero) {
  unsigned byte;
  for (byte = 0u; byte != 4u; ++byte)
    if ((value.known_mask & (1u << byte)) != 0u &&
        ((value.word >> (8u * byte)) & 255u) != 0u) {
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

static void note_invariant_byte(uint32_t candidate, uint32_t *first,
                                unsigned *first_set, unsigned *same,
                                unsigned byte) {
  uint32_t current = (candidate >> (8u * byte)) & 255u;
  if (first_set[byte] == 0u) {
    first[byte] = current;
    first_set[byte] = 1u;
  } else if (first[byte] != current) {
    same[byte] = 0u;
  }
}

/* The source LBU leaves either a known byte or all 256 byte values. Enumerate
 * those exact possibilities so the correlated SLL/ADDU pair keeps every
 * invariant output byte rather than treating v0 and v1 as independent. */
static void scale_volume(Nba97GamePeriodMusicStartWord source,
                         Nba97GamePeriodMusicStartWord *shifted,
                         Nba97GamePeriodMusicStartWord *scaled,
                         Nba97GamePeriodMusicStartWord *less_than_128) {
  uint32_t first_shift[4] = {0u, 0u, 0u, 0u};
  uint32_t first_scale[4] = {0u, 0u, 0u, 0u};
  unsigned set_shift[4] = {0u, 0u, 0u, 0u};
  unsigned set_scale[4] = {0u, 0u, 0u, 0u};
  unsigned same_shift[4] = {1u, 1u, 1u, 1u};
  unsigned same_scale[4] = {1u, 1u, 1u, 1u};
  unsigned first_predicate = 0u;
  unsigned predicate_same = 1u;
  unsigned predicate_set = 0u;
  unsigned start = (source.known_mask & 1u) != 0u ? source.word & 255u : 0u;
  unsigned end = (source.known_mask & 1u) != 0u ? start : 255u;
  unsigned value;
  unsigned byte;
  shifted->word = (source.word & 255u) << 3u;
  scaled->word = (source.word & 255u) * 9u;
  less_than_128->word = scaled->word < 128u ? 1u : 0u;
  shifted->known_mask = 0u;
  scaled->known_mask = 0u;
  less_than_128->known_mask = 14u;
  for (value = start; value <= end; ++value) {
    uint32_t shift = value << 3u;
    uint32_t scale = value * 9u;
    unsigned predicate = scale < 128u ? 1u : 0u;
    for (byte = 0u; byte != 4u; ++byte) {
      note_invariant_byte(shift, first_shift, set_shift, same_shift, byte);
      note_invariant_byte(scale, first_scale, set_scale, same_scale, byte);
    }
    if (predicate_set == 0u) {
      first_predicate = predicate;
      predicate_set = 1u;
    } else if (first_predicate != predicate) {
      predicate_same = 0u;
    }
  }
  for (byte = 0u; byte != 4u; ++byte) {
    if (same_shift[byte] != 0u)
      shifted->known_mask =
          (uint8_t)(shifted->known_mask | (uint8_t)(1u << byte));
    if (same_scale[byte] != 0u)
      scaled->known_mask =
          (uint8_t)(scaled->known_mask | (uint8_t)(1u << byte));
  }
  if (predicate_same != 0u)
    less_than_128->known_mask = 15u;
}

static int invoke(Run *run, uint32_t pc, uint32_t entry, uint8_t kind,
                  uint8_t argument_count,
                  Nba97GamePeriodMusicStartWord delay_a0) {
  Nba97GamePeriodMusicStartEvent event;
  int accepted;
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
  R(NBA97_MATCH_INITIALIZE_A0) = delay_a0;
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
  if (!valid_registers(&run->machine))
    return NBA97_TEXT_ARGUMENT;
  ++run->progress->callbacks_completed;
  ++run->progress->call_count[kind];
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_period_music_start(Nba97GamePeriodMusicStartContext *context,
                                  Nba97GamePeriodMusicStartProgress *progress) {
  Run storage;
  Run *run = &storage;
  Nba97GamePeriodMusicStartWord loaded;
  Nba97GamePeriodMusicStartWord shifted;
  Nba97GamePeriodMusicStartWord predicate;
  Nba97GamePeriodMusicStartWord zero;
  int is_zero;

  TRY(initialize(context, progress, run));
  set_known(&zero, 0u);

  /* 0x800295D0..0x800295F0: establish the frame in source order and test the
   * unsigned enable byte. The branch NOP precedes an unknown-data refusal. */
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe8));
  progress->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  TRY(write_value(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u, 4u,
                  UINT32_C(0x800295d4), R(NBA97_MATCH_INITIALIZE_S0)));
  set_known(&R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0x80020000));
  R(NBA97_MATCH_INITIALIZE_S0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0x1d7f));
  TRY(write_value(run, R(NBA97_MATCH_INITIALIZE_SP), 0x14u, 4u,
                  UINT32_C(0x800295e0), R(NBA97_MATCH_INITIALIZE_RA)));
  TRY(read_value(run, R(NBA97_MATCH_INITIALIZE_S0), 0u, 1u,
                 UINT32_C(0x800295e4), &R(NBA97_MATCH_INITIALIZE_V0)));
  R(NBA97_MATCH_INITIALIZE_V0) = zero_extend_byte(R(NBA97_MATCH_INITIALIZE_V0));
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800295ec),
                  &is_zero));
  if (is_zero)
    goto epilogue;

  /* 0x800295F4..0x80029628: read the load flag, and when clear load both
   * descriptors before calling 0x800AAE7C and publishing flag 0x800B1F38. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800b0000));
  TRY(read_value(run, R(NBA97_MATCH_INITIALIZE_V0), 0x1f38u, 1u,
                 UINT32_C(0x800295f8), &R(NBA97_MATCH_INITIALIZE_V0)));
  R(NBA97_MATCH_INITIALIZE_V0) = zero_extend_byte(R(NBA97_MATCH_INITIALIZE_V0));
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80029600),
                  &is_zero));
  if (is_zero) {
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
    TRY(read_value(run, R(NBA97_MATCH_INITIALIZE_A0), 0x1d6cu, 4u,
                   UINT32_C(0x8002960c), &R(NBA97_MATCH_INITIALIZE_A0)));
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x800b0000));
    TRY(read_value(run, R(NBA97_MATCH_INITIALIZE_A1), 0x1f34u, 4u,
                   UINT32_C(0x80029614), &R(NBA97_MATCH_INITIALIZE_A1)));
    TRY(invoke(run, UINT32_C(0x80029618), UINT32_C(0x800aae7c),
               NBA97_GAME_PERIOD_MUSIC_START_LOAD_800AAE7C, 2u,
               R(NBA97_MATCH_INITIALIZE_A0)));
    progress->load_music_executed = 1u;
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1u);
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800b0000));
    TRY(write_value(run, R(NBA97_MATCH_INITIALIZE_AT), 0x1f38u, 1u,
                    UINT32_C(0x80029628), R(NBA97_MATCH_INITIALIZE_V0)));
  }

  /* 0x8002962C..0x80029648: the callback-live s0 selects the volume reread.
   * Preserve the correlated SLL/ADDU byte knowledge and clamp nine-times the
   * unsigned byte at 127 after the source SLTI branch. */
  TRY(read_value(run, R(NBA97_MATCH_INITIALIZE_S0), 0u, 1u,
                 UINT32_C(0x8002962c), &R(NBA97_MATCH_INITIALIZE_V0)));
  R(NBA97_MATCH_INITIALIZE_V0) = zero_extend_byte(R(NBA97_MATCH_INITIALIZE_V0));
  scale_volume(R(NBA97_MATCH_INITIALIZE_V0), &shifted,
               &R(NBA97_MATCH_INITIALIZE_S0), &predicate);
  R(NBA97_MATCH_INITIALIZE_V1) = shifted;
  R(NBA97_MATCH_INITIALIZE_V0) = predicate;
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80029640),
                  &is_zero));
  if (is_zero)
    set_known(&R(NBA97_MATCH_INITIALIZE_S0), 127u);
  progress->scaled_volume = R(NBA97_MATCH_INITIALIZE_S0);

  /* 0x8002964C..0x80029668: dispatch all four playback services in source
   * order. Each callback sees JAL's ra and its exact delay-slot a0 value. */
  TRY(invoke(run, UINT32_C(0x8002964c), UINT32_C(0x800aafa0),
             NBA97_GAME_PERIOD_MUSIC_START_800AAFA0, 1u, zero));
  TRY(invoke(run, UINT32_C(0x80029654), UINT32_C(0x800ab224),
             NBA97_GAME_PERIOD_MUSIC_START_800AB224, 1u, zero));
  TRY(invoke(run, UINT32_C(0x8002965c), UINT32_C(0x800ab388),
             NBA97_GAME_PERIOD_MUSIC_START_800AB388, 1u,
             R(NBA97_MATCH_INITIALIZE_S0)));
  set_known(&loaded, 120u);
  TRY(invoke(run, UINT32_C(0x80029664), UINT32_C(0x800ab2c8),
             NBA97_GAME_PERIOD_MUSIC_START_800AB2C8, 1u, loaded));
  progress->playback_executed = 1u;

  /* 0x8002966C..0x80029674: the final callback's machine stays live before
   * the routine writes the playback-started byte. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1u);
  set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800b0000));
  TRY(write_value(run, R(NBA97_MATCH_INITIALIZE_AT), 0x1f39u, 1u,
                  UINT32_C(0x80029674), R(NBA97_MATCH_INITIALIZE_V0)));

epilogue:
  /* 0x80029678..0x80029688: reload ra then s0 through callback-live sp,
   * release the exact frame, and consume restored ra after the NOP delay. */
  TRY(read_value(run, R(NBA97_MATCH_INITIALIZE_SP), 0x14u, 4u,
                 UINT32_C(0x80029678), &R(NBA97_MATCH_INITIALIZE_RA)));
  progress->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  TRY(read_value(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u, 4u,
                 UINT32_C(0x8002967c), &R(NBA97_MATCH_INITIALIZE_S0)));
  progress->restored_s0 = R(NBA97_MATCH_INITIALIZE_S0);
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x18u);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15u) {
    stop(run, UINT32_C(0x80029684), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if ((R(NBA97_MATCH_INITIALIZE_RA).word & 3u) != 0u) {
    stop(run, UINT32_C(0x80029684), R(NBA97_MATCH_INITIALIZE_RA).word, 0u);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  progress->completed = 1u;
  stop(run, 0u, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
