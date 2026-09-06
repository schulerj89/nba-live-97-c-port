#include "game_camera_elapsed_dispatch.h"

#include <limits.h>
#include <string.h>

typedef Nba97GameCameraElapsedDispatchWord Word;

typedef struct Run {
  Nba97GameCameraElapsedDispatchContext *context;
  Nba97GameCameraElapsedDispatchProgress *out;
  Nba97GameCameraElapsedDispatchMachine machine;
} Run;

#define R(index_) (run->machine.registers.gpr[(index_)])
#define ZERO R(0)
#define AT R(1)
#define V0 R(2)
#define V1 R(3)
#define A0 R(4)
#define SP R(29)
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

static Word immediate(uint32_t value) {
  Word result;
  known(&result, value);
  return result;
}

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t entry) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  run->out->stopped_entry = entry;
  publish(run);
}

static int valid_machine(const Nba97GameCameraElapsedDispatchMachine *machine) {
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

static int initialize(Nba97GameCameraElapsedDispatchContext *context,
                      Nba97GameCameraElapsedDispatchProgress *out, Run *run) {
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
  out->requested_delta = run->machine.registers.gpr[4];
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static uint32_t width_mask(unsigned width) {
  return width == 4u ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - UINT32_C(1);
}

static uint8_t knowledge_mask(unsigned width) {
  return (uint8_t)((1u << width) - 1u);
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width, Word value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameCameraElapsedDispatchAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word & width_mask(width);
    event->operation = run->out->operations;
    event->width = (uint8_t)width;
    event->known_mask = (uint8_t)(value.known_mask & knowledge_mask(width));
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t address, unsigned width, uint32_t pc,
                  uint8_t **data, uint8_t **known_bytes) {
  size_t index;
  size_t byte;
  stop(run, pc, address, 0u);
  TRY(spend(run));
  ++run->out->accesses;
  if ((width == 2u && (address & 1u)) || (width == 4u && (address & 3u)))
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (index = 0u; index != run->context->memory.count; ++index) {
    Nba97GameTextRegion *region = &run->context->memory.region[index];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        width > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known_bytes =
        region->known == NULL ? NULL : region->known + (size_t)offset;
    if (*known_bytes != NULL)
      for (byte = 0u; byte != width; ++byte)
        if ((*known_bytes)[byte] > 1u)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static Word extend(Word value, unsigned width, int sign) {
  Word result;
  value.word &= width_mask(width);
  value.known_mask &= knowledge_mask(width);
  result.word = value.word;
  if (sign && (value.word & (UINT32_C(1) << (width * 8u - 1u))))
    result.word |= ~width_mask(width);
  result.known_mask = value.known_mask;
  if (!sign)
    result.known_mask |= (uint8_t)(15u ^ knowledge_mask(width));
  else if (value.known_mask & (1u << (width - 1u)))
    result.known_mask |= (uint8_t)(15u ^ knowledge_mask(width));
  return result;
}

static int read_value(Run *run, uint32_t address, unsigned width, int sign,
                      uint32_t pc, Word *result) {
  uint8_t *data;
  uint8_t *known_bytes;
  unsigned byte;
  Word loaded = {0u, 0u};
  TRY(locate(run, address, width, pc, &data, &known_bytes));
  for (byte = 0u; byte != width; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (byte * 8u);
    if (known_bytes == NULL || known_bytes[byte])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (uint8_t)(1u << byte));
  }
  ++run->out->reads;
  journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, address, width, loaded);
  *result = extend(loaded, width, sign);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, uint32_t address, unsigned width, uint32_t pc,
                       Word value) {
  uint8_t *data;
  uint8_t *known_bytes;
  unsigned byte;
  value.word &= width_mask(width);
  value.known_mask &= knowledge_mask(width);
  TRY(locate(run, address, width, pc, &data, &known_bytes));
  if (known_bytes == NULL && value.known_mask != knowledge_mask(width))
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0u; byte != width; ++byte) {
    data[byte] = (uint8_t)(value.word >> (byte * 8u));
    if (known_bytes != NULL)
      known_bytes[byte] = (uint8_t)((value.known_mask >> byte) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

/* Byte carry enumeration retains every invariant ADDU/ADDIU result byte. */
static Word add(Word left, Word right) {
  Word result;
  unsigned carry_mask = 1u;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned next_mask = 0u;
    unsigned first_output = 0u;
    int first = 1;
    int invariant = 1;
    unsigned left_start = (left.known_mask & (1u << byte))
                              ? ((left.word >> (byte * 8u)) & 255u)
                              : 0u;
    unsigned left_end = (left.known_mask & (1u << byte)) ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte))
                               ? ((right.word >> (byte * 8u)) & 255u)
                               : 0u;
    unsigned right_end = (right.known_mask & (1u << byte)) ? right_start : 255u;
    unsigned carry;
    for (carry = 0u; carry != 2u; ++carry) {
      unsigned a;
      if (!(carry_mask & (1u << carry)))
        continue;
      for (a = left_start; a <= left_end; ++a) {
        unsigned b;
        for (b = right_start; b <= right_end; ++b) {
          unsigned sum = a + b + carry;
          unsigned output = sum & 255u;
          next_mask |= 1u << (sum >> 8u);
          if (first) {
            first_output = output;
            first = 0;
          } else if (output != first_output) {
            invariant = 0;
          }
        }
      }
    }
    if (invariant)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
    carry_mask = next_mask;
  }
  return result;
}

static Word and_constant(Word value, uint32_t mask) {
  Word result;
  unsigned byte;
  result.word = value.word & mask;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    uint32_t part = (mask >> (byte * 8u)) & 255u;
    if (part == 0u || (value.known_mask & (1u << byte)))
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static int32_t signed_word(uint32_t value) {
  return value <= (uint32_t)INT32_MAX ? (int32_t)value
                                      : -1 - (int32_t)(UINT32_MAX - value);
}

static void signed_bounds(Word value, int64_t *minimum, int64_t *maximum) {
  uint32_t low = 0u;
  uint32_t high = 0u;
  unsigned byte;
  for (byte = 0u; byte != 3u; ++byte) {
    uint32_t part = (value.word >> (byte * 8u)) & 255u;
    low |= ((value.known_mask & (1u << byte)) ? part : 0u) << (byte * 8u);
    high |= ((value.known_mask & (1u << byte)) ? part : 255u) << (byte * 8u);
  }
  if (value.known_mask & 8u) {
    uint32_t top = value.word & UINT32_C(0xff000000);
    low |= top;
    high |= top;
  } else {
    low |= UINT32_C(0x80000000);
    high |= UINT32_C(0x7f000000);
  }
  *minimum = (low & UINT32_C(0x80000000)) ? (int64_t)low - INT64_C(0x100000000)
                                          : (int64_t)low;
  *maximum = (high & UINT32_C(0x80000000))
                 ? (int64_t)high - INT64_C(0x100000000)
                 : (int64_t)high;
}

static Word signed_less(Word left, Word right) {
  Word result;
  int64_t left_minimum;
  int64_t left_maximum;
  int64_t right_minimum;
  int64_t right_maximum;
  known(&result, signed_word(left.word) < signed_word(right.word));
  signed_bounds(left, &left_minimum, &left_maximum);
  signed_bounds(right, &right_minimum, &right_maximum);
  if (left_maximum < right_minimum || left_minimum >= right_maximum)
    return result;
  result.known_mask = 14u;
  return result;
}

static int equal_decision(Word left, Word right, int *equal) {
  unsigned byte;
  for (byte = 0u; byte != 4u; ++byte)
    if ((left.known_mask & right.known_mask & (1u << byte)) &&
        (((left.word ^ right.word) >> (byte * 8u)) & 255u)) {
      *equal = 0;
      return 1;
    }
  if (left.known_mask == 15u && right.known_mask == 15u) {
    *equal = left.word == right.word;
    return 1;
  }
  return 0;
}

static int branch_equal(Run *run, Word left, Word right, uint32_t pc,
                        int *equal) {
  if (!equal_decision(left, right, equal)) {
    stop(run, pc, 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  return NBA97_TEXT_COMPLETE;
}

static int branch_zero(Run *run, Word value, uint32_t pc, int *is_zero) {
  return branch_equal(run, value, ZERO, pc, is_zero);
}

static int negative_decision(Run *run, Word value, uint32_t pc, int *negative) {
  int64_t minimum;
  int64_t maximum;
  signed_bounds(value, &minimum, &maximum);
  if (maximum < 0) {
    *negative = 1;
    return NBA97_TEXT_COMPLETE;
  }
  if (minimum >= 0) {
    *negative = 0;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0u, 0u);
  return NBA97_TEXT_UNKNOWN;
}

static int read_dynamic(Run *run, Word address, uint32_t pc, Word *result) {
  if (address.known_mask != 15u) {
    stop(run, pc, address.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  return read_value(run, address.word, 4u, 0, pc, result);
}

static int invoke(Run *run, uint32_t pc, Word entry, uint8_t kind) {
  Nba97GameCameraElapsedDispatchEvent event;
  int accepted;
  stop(run, pc, 0u, entry.word);
  TRY(spend(run));
  ++run->out->call_attempts[kind];
  if (entry.known_mask != 15u)
    return NBA97_TEXT_UNKNOWN;
  if (entry.word == 0u)
    return NBA97_TEXT_RESOURCE;
  if (entry.word & 3u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  memset(&event, 0, sizeof(event));
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = entry.word;
  event.operation = run->out->operations;
  event.invocation = run->out->call_attempts[kind];
  event.kind = kind;
  event.argument_count = 0u;
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
  ++run->out->callbacks_completed;
  ++run->out->call_count[kind];
  return NBA97_TEXT_COMPLETE;
}

static int epilogue(Run *run) {
  Word address;
  STEP(0x800799bc);
  address = add(SP, immediate(0x10u));
  if (address.known_mask != 15u) {
    stop(run, UINT32_C(0x800799bc), address.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  TRY(read_value(run, address.word, 4u, 0, UINT32_C(0x800799bc), &RA));
  run->out->restored_return_address = RA;
  STEP(0x800799c0);
  SP = add(SP, immediate(0x18u));
  STEP(0x800799c4);
  STEP(0x800799c8);
  if (RA.known_mask != 15u) {
    stop(run, UINT32_C(0x800799c4), RA.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, UINT32_C(0x800799c4), RA.word, 0u);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  run->out->completed = 1u;
  stop(run, 0u, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_camera_elapsed_dispatch(
    Nba97GameCameraElapsedDispatchContext *context,
    Nba97GameCameraElapsedDispatchProgress *out) {
  Run storage;
  Run *run = &storage;
  Word branch_value;
  Word target;
  int condition = 0;
  TRY(initialize(context, out, run));

  /* 0x800798B4..0x800798E0: spill ra in the argument branch delay, then
   * either reload the lower bound for -1 or add the signed delta with wrap. */
  STEP(0x800798b4);
  SP = add(SP, immediate(UINT32_C(0xffffffe8)));
  out->frame_stack_pointer = SP.word;
  STEP(0x800798b8);
  known(&V0, UINT32_MAX);
  STEP(0x800798bc);
  branch_value = A0;
  STEP(0x800798c0);
  target = add(SP, immediate(0x10u));
  if (target.known_mask != 15u) {
    stop(run, UINT32_C(0x800798c0), target.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  TRY(write_value(run, target.word, 4u, UINT32_C(0x800798c0), RA));
  TRY(branch_equal(run, branch_value, V0, UINT32_C(0x800798bc), &condition));
  if (condition) {
    STEP(0x800798c4);
    known(&V0, UINT32_C(0x800c0000));
    STEP(0x800798c8);
    TRY(read_value(run, UINT32_C(0x800bc1f8), 4u, 0, UINT32_C(0x800798c8),
                   &V0));
    STEP(0x800798cc);
    STEP(0x800798d0);
  } else {
    STEP(0x800798d4);
    known(&V0, UINT32_C(0x80100000));
    STEP(0x800798d8);
    TRY(read_value(run, UINT32_C(0x80106074), 4u, 0, UINT32_C(0x800798d8),
                   &V0));
    STEP(0x800798dc);
    STEP(0x800798e0);
    V0 = add(V0, A0);
  }

  /* 0x800798E4..0x80079910: publish the candidate elapsed word, reread both
   * it and the signed upper cap, and clamp only when upper < elapsed. */
  STEP(0x800798e4);
  known(&AT, UINT32_C(0x80100000));
  STEP(0x800798e8);
  TRY(write_value(run, UINT32_C(0x80106074), 4u, UINT32_C(0x800798e8), V0));
  STEP(0x800798ec);
  known(&V0, UINT32_C(0x80100000));
  STEP(0x800798f0);
  TRY(read_value(run, UINT32_C(0x80106074), 4u, 0, UINT32_C(0x800798f0), &V0));
  STEP(0x800798f4);
  known(&V1, UINT32_C(0x800c0000));
  STEP(0x800798f8);
  TRY(read_value(run, UINT32_C(0x800bc1fc), 4u, 0, UINT32_C(0x800798f8), &V1));
  STEP(0x800798fc);
  STEP(0x80079900);
  V0 = signed_less(V1, V0);
  branch_value = V0;
  STEP(0x80079904);
  STEP(0x80079908);
  TRY(branch_zero(run, branch_value, UINT32_C(0x80079904), &condition));
  if (!condition) {
    STEP(0x8007990c);
    known(&AT, UINT32_C(0x80100000));
    STEP(0x80079910);
    TRY(write_value(run, UINT32_C(0x80106074), 4u, UINT32_C(0x80079910), V1));
  }

  /* 0x80079914..0x80079930: a fresh elapsed read below the signed lower
   * threshold exits immediately with the SLT Boolean still in v0. */
  STEP(0x80079914);
  known(&V0, UINT32_C(0x80100000));
  STEP(0x80079918);
  TRY(read_value(run, UINT32_C(0x80106074), 4u, 0, UINT32_C(0x80079918), &V0));
  out->elapsed_value = V0;
  STEP(0x8007991c);
  known(&V1, UINT32_C(0x800c0000));
  STEP(0x80079920);
  TRY(read_value(run, UINT32_C(0x800bc1f8), 4u, 0, UINT32_C(0x80079920), &V1));
  STEP(0x80079924);
  STEP(0x80079928);
  V0 = signed_less(V0, V1);
  branch_value = V0;
  STEP(0x8007992c);
  STEP(0x80079930);
  TRY(branch_zero(run, branch_value, UINT32_C(0x8007992c), &condition));
  if (!condition)
    return epilogue(run);

  /* 0x80079934..0x80079960: a zero indirect gate follows the descriptor
   * pointer to +0x5C. JALR assigns ra before its NOP and target validation. */
  STEP(0x80079934);
  known(&V0, UINT32_C(0x800c0000));
  STEP(0x80079938);
  TRY(read_value(run, UINT32_C(0x800bc200), 4u, 0, UINT32_C(0x80079938), &V0));
  STEP(0x8007993c);
  branch_value = V0;
  STEP(0x80079940);
  STEP(0x80079944);
  TRY(branch_zero(run, branch_value, UINT32_C(0x80079940), &condition));
  if (condition) {
    STEP(0x80079948);
    known(&V0, UINT32_C(0x80100000));
    STEP(0x8007994c);
    TRY(read_value(run, UINT32_C(0x800fc9d0), 4u, 0, UINT32_C(0x8007994c),
                   &V0));
    STEP(0x80079950);
    STEP(0x80079954);
    target = add(V0, immediate(0x5cu));
    TRY(read_dynamic(run, target, UINT32_C(0x80079954), &V0));
    STEP(0x80079958);
    target = V0;
    STEP(0x8007995c);
    known(&RA, UINT32_C(0x80079964));
    STEP(0x80079960);
    out->indirect_target = target;
    TRY(invoke(run, UINT32_C(0x8007995c), target,
               NBA97_GAME_CAMERA_ELAPSED_DISPATCH_INDIRECT_8007995C));
  }

  /* 0x80079964..0x80079998: negative cached state skips the probe. A zero
   * low-byte probe result falls through to refresh; nonzero reloads live state.
   */
  STEP(0x80079964);
  known(&V0, UINT32_C(0x800c0000));
  STEP(0x80079968);
  TRY(read_value(run, UINT32_C(0x800bc1f4), 4u, 0, UINT32_C(0x80079968), &V0));
  STEP(0x8007996c);
  branch_value = V0;
  STEP(0x80079970);
  STEP(0x80079974);
  TRY(negative_decision(run, branch_value, UINT32_C(0x80079970), &condition));
  if (!condition) {
    STEP(0x80079978);
    known(&RA, UINT32_C(0x80079980));
    STEP(0x8007997c);
    TRY(invoke(run, UINT32_C(0x80079978), immediate(UINT32_C(0x8007a468)),
               NBA97_GAME_CAMERA_ELAPSED_DISPATCH_PROBE_8007A468));
    STEP(0x80079980);
    V0 = and_constant(V0, 0xffu);
    branch_value = V0;
    STEP(0x80079984);
    STEP(0x80079988);
    TRY(branch_zero(run, branch_value, UINT32_C(0x80079984), &condition));
    if (!condition) {
      STEP(0x8007998c);
      known(&V0, UINT32_C(0x800c0000));
      STEP(0x80079990);
      TRY(read_value(run, UINT32_C(0x800bc1f4), 4u, 0, UINT32_C(0x80079990),
                     &V0));
      STEP(0x80079994);
      STEP(0x80079998);
      goto publish_value;
    }
  }

  /* 0x8007999C..0x800799A8: refresh returns a raw v0 and stores it back to
   * cached camera state before publication. */
  STEP(0x8007999c);
  known(&RA, UINT32_C(0x800799a4));
  STEP(0x800799a0);
  TRY(invoke(run, UINT32_C(0x8007999c), immediate(UINT32_C(0x8007a410)),
             NBA97_GAME_CAMERA_ELAPSED_DISPATCH_REFRESH_8007A410));
  STEP(0x800799a4);
  known(&AT, UINT32_C(0x800c0000));
  STEP(0x800799a8);
  TRY(write_value(run, UINT32_C(0x800bc1f4), 4u, UINT32_C(0x800799a8), V0));

publish_value:
  /* 0x800799AC..0x800799B8: source order publishes v0 first, then resets the
   * elapsed word to known zero. */
  STEP(0x800799ac);
  known(&AT, UINT32_C(0x800e0000));
  STEP(0x800799b0);
  TRY(write_value(run, UINT32_C(0x800d8eec), 4u, UINT32_C(0x800799b0), V0));
  out->published_value = V0;
  STEP(0x800799b4);
  known(&AT, UINT32_C(0x80100000));
  STEP(0x800799b8);
  TRY(write_value(run, UINT32_C(0x80106074), 4u, UINT32_C(0x800799b8), ZERO));
  out->elapsed_reset = 1u;
  return epilogue(run);
}
