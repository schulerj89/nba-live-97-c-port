#include "game_camera_phase_select.h"

#include <limits.h>
#include <string.h>

typedef Nba97GameCameraPhaseSelectWord Word;

typedef struct Run {
  Nba97GameCameraPhaseSelectContext *context;
  Nba97GameCameraPhaseSelectProgress *out;
  Nba97GameCameraPhaseSelectMachine machine;
} Run;

#define R(index_) (run->machine.registers.gpr[(index_)])
#define ZERO R(0)
#define AT R(1)
#define V0 R(2)
#define V1 R(3)
#define A0 R(4)
#define A1 R(5)
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

static int valid_machine(const Nba97GameCameraPhaseSelectMachine *machine) {
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

static int initialize(Nba97GameCameraPhaseSelectContext *context,
                      Nba97GameCameraPhaseSelectProgress *out, Run *run) {
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
    Nba97GameCameraPhaseSelectAccess *event =
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

/* Byte carry enumeration retains every invariant ADDIU result byte. */
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

static void unsigned_bounds(Word value, uint32_t *minimum, uint32_t *maximum) {
  unsigned byte;
  *minimum = 0u;
  *maximum = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    uint32_t part = (value.word >> (byte * 8u)) & 255u;
    *minimum |= ((value.known_mask & (1u << byte)) ? part : 0u) << (byte * 8u);
    *maximum |= ((value.known_mask & (1u << byte)) ? part : 255u)
                << (byte * 8u);
  }
}

static Word unsigned_less_constant(Word value, uint32_t limit) {
  Word result;
  uint32_t minimum;
  uint32_t maximum;
  known(&result, value.word < limit);
  unsigned_bounds(value, &minimum, &maximum);
  if (maximum < limit || minimum >= limit)
    return result;
  result.known_mask = 14u;
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
  if (!(value.known_mask & 8u)) {
    *minimum = INT32_MIN;
    *maximum = INT32_MAX;
    return;
  }
  for (byte = 0u; byte != 4u; ++byte) {
    uint32_t part = (value.word >> (byte * 8u)) & 255u;
    low |= ((value.known_mask & (1u << byte)) ? part : 0u) << (byte * 8u);
    high |= ((value.known_mask & (1u << byte)) ? part : 255u) << (byte * 8u);
  }
  *minimum = (low & UINT32_C(0x80000000)) ? (int64_t)low - INT64_C(0x100000000)
                                          : (int64_t)low;
  *maximum = (high & UINT32_C(0x80000000))
                 ? (int64_t)high - INT64_C(0x100000000)
                 : (int64_t)high;
}

static Word signed_less_constant(Word value, int32_t limit) {
  Word result;
  int64_t minimum;
  int64_t maximum;
  known(&result, signed_word(value.word) < limit);
  signed_bounds(value, &minimum, &maximum);
  if (maximum < limit || minimum >= limit)
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

static uint8_t call_kind(uint32_t entry) {
  if (entry == UINT32_C(0x800799cc))
    return NBA97_GAME_CAMERA_PHASE_SELECT_CAMERA_800799CC;
  if (entry == UINT32_C(0x80079ebc))
    return NBA97_GAME_CAMERA_PHASE_SELECT_ADJUST_80079EBC;
  return NBA97_GAME_CAMERA_PHASE_SELECT_FINALIZE_80079F78;
}

static uint8_t call_arguments(uint32_t entry) {
  return (uint8_t)(entry == UINT32_C(0x800799cc) ? 2u : 1u);
}

static int invoke(Run *run, uint32_t pc, uint32_t entry) {
  Nba97GameCameraPhaseSelectEvent event;
  uint8_t kind = call_kind(entry);
  int accepted;
  stop(run, pc, 0u, entry);
  TRY(spend(run));
  ++run->out->call_attempts[kind];
  memset(&event, 0, sizeof(event));
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = entry;
  event.operation = run->out->operations;
  event.invocation = run->out->call_attempts[kind];
  event.kind = kind;
  event.argument_count = call_arguments(entry);
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
  Word stack_address;
  STEP(0x8007e454);
  stack_address = add(SP, immediate(0x10u));
  if (stack_address.known_mask != 15u) {
    stop(run, UINT32_C(0x8007e454), stack_address.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  TRY(read_value(run, stack_address.word, 4u, 0, UINT32_C(0x8007e454), &RA));
  run->out->restored_return_address = RA;
  STEP(0x8007e458);
  SP = add(SP, immediate(0x18u));
  STEP(0x8007e45c);
  STEP(0x8007e460);
  if (RA.known_mask != 15u) {
    stop(run, UINT32_C(0x8007e45c), RA.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, UINT32_C(0x8007e45c), RA.word, 0u);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  run->out->completed = 1u;
  stop(run, 0u, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_camera_phase_select(Nba97GameCameraPhaseSelectContext *context,
                                   Nba97GameCameraPhaseSelectProgress *out) {
  Run storage;
  Run *run = &storage;
  Word branch_value;
  Word compared_value;
  int condition;
  TRY(initialize(context, out, run));

  /* 0x8007E26C..0x8007E27C: the busy predicate is captured before the branch
   * delay stores ra through the newly allocated stack frame. */
  STEP(0x8007e26c);
  known(&V0, UINT32_C(0x80100000));
  STEP(0x8007e270);
  TRY(read_value(run, UINT32_C(0x800fc99c), 4u, 0, UINT32_C(0x8007e270), &V0));
  STEP(0x8007e274);
  SP = add(SP, immediate(UINT32_C(0xffffffe8)));
  out->frame_stack_pointer = SP.word;
  branch_value = V0;
  STEP(0x8007e278);
  STEP(0x8007e27c);
  compared_value = add(SP, immediate(0x10u));
  if (compared_value.known_mask != 15u) {
    stop(run, UINT32_C(0x8007e27c), compared_value.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  TRY(write_value(run, compared_value.word, 4u, UINT32_C(0x8007e27c), RA));
  TRY(branch_zero(run, branch_value, UINT32_C(0x8007e278), &condition));
  if (!condition)
    return epilogue(run);

  /* 0x8007E280..0x8007E2A8: the second early branch always leaves the low
   * a0 ANDI in v0; a nonzero low byte clears both retained phase words. */
  STEP(0x8007e280);
  known(&V0, UINT32_C(0x80100000));
  STEP(0x8007e284);
  TRY(read_value(run, UINT32_C(0x800f9ffe), 2u, 1, UINT32_C(0x8007e284), &V0));
  STEP(0x8007e288);
  branch_value = V0;
  STEP(0x8007e28c);
  STEP(0x8007e290);
  V0 = and_constant(A0, 0xffu);
  TRY(branch_zero(run, branch_value, UINT32_C(0x8007e28c), &condition));
  if (!condition)
    return epilogue(run);
  STEP(0x8007e294);
  STEP(0x8007e298);
  TRY(branch_zero(run, V0, UINT32_C(0x8007e294), &condition));
  if (!condition) {
    STEP(0x8007e29c);
    known(&AT, UINT32_C(0x800c0000));
    STEP(0x8007e2a0);
    TRY(write_value(run, UINT32_C(0x800bc940), 4u, UINT32_C(0x8007e2a0), ZERO));
    STEP(0x8007e2a4);
    known(&AT, UINT32_C(0x800c0000));
    STEP(0x8007e2a8);
    TRY(write_value(run, UINT32_C(0x800bc944), 4u, UINT32_C(0x8007e2a8), ZERO));
  }

  /* 0x8007E2AC..0x8007E340: derive phase 1/2/3 or retain an existing phase
   * 3/4, preserving every branch-delay constant assignment. */
  STEP(0x8007e2ac);
  known(&V1, UINT32_C(0x80100000));
  STEP(0x8007e2b0);
  TRY(read_value(run, UINT32_C(0x800fdb90), 2u, 1, UINT32_C(0x8007e2b0), &V1));
  STEP(0x8007e2b4);
  known(&V0, 0x81u);
  compared_value = V0;
  STEP(0x8007e2b8);
  STEP(0x8007e2bc);
  known(&V0, 0x82u);
  TRY(branch_equal(run, V1, compared_value, UINT32_C(0x8007e2b8), &condition));
  if (condition) {
    STEP(0x8007e2c0);
    STEP(0x8007e2c4);
    known(&V0, 1u);
    goto set_current_phase;
  }
  compared_value = V0;
  STEP(0x8007e2c8);
  STEP(0x8007e2cc);
  TRY(branch_equal(run, V1, compared_value, UINT32_C(0x8007e2c8), &condition));
  if (!condition)
    goto retain_prior_phase;

  STEP(0x8007e2d0);
  known(&V0, UINT32_C(0x80100000));
  STEP(0x8007e2d4);
  TRY(read_value(run, UINT32_C(0x800fe884), 2u, 1, UINT32_C(0x8007e2d4), &V0));
  STEP(0x8007e2d8);
  STEP(0x8007e2dc);
  V0 = signed_less_constant(V0, 2);
  branch_value = V0;
  STEP(0x8007e2e0);
  STEP(0x8007e2e4);
  TRY(branch_zero(run, branch_value, UINT32_C(0x8007e2e0), &condition));
  if (!condition)
    goto retain_prior_phase;

  STEP(0x8007e2e8);
  known(&V1, UINT32_C(0x80100000));
  STEP(0x8007e2ec);
  TRY(read_value(run, UINT32_C(0x800fdb68), 2u, 1, UINT32_C(0x8007e2ec), &V1));
  STEP(0x8007e2f0);
  STEP(0x8007e2f4);
  STEP(0x8007e2f8);
  known(&V0, 4u);
  TRY(branch_zero(run, V1, UINT32_C(0x8007e2f4), &condition));
  if (!condition) {
    compared_value = V0;
    STEP(0x8007e2fc);
    STEP(0x8007e300);
    known(&V0, 2u);
    TRY(branch_equal(run, V1, compared_value, UINT32_C(0x8007e2fc),
                     &condition));
    if (!condition)
      goto set_current_phase;
  }

  STEP(0x8007e304);
  known(&V1, UINT32_C(0x80100000));
  STEP(0x8007e308);
  TRY(read_value(run, UINT32_C(0x800fdb58), 4u, 0, UINT32_C(0x8007e308), &V1));
  STEP(0x8007e30c);
  known(&V0, UINT32_C(0x80100000));
  STEP(0x8007e310);
  TRY(read_value(run, UINT32_C(0x800fdb60), 4u, 0, UINT32_C(0x8007e310), &V0));
  STEP(0x8007e314);
  compared_value = V0;
  STEP(0x8007e318);
  STEP(0x8007e31c);
  known(&V0, 2u);
  TRY(branch_equal(run, V1, compared_value, UINT32_C(0x8007e318), &condition));
  if (!condition)
    goto set_current_phase;

retain_prior_phase:
  STEP(0x8007e320);
  known(&V0, UINT32_C(0x800c0000));
  STEP(0x8007e324);
  TRY(read_value(run, UINT32_C(0x800bc940), 4u, 0, UINT32_C(0x8007e324), &V0));
  STEP(0x8007e328);
  STEP(0x8007e32c);
  V0 = add(V0, immediate(UINT32_C(0xfffffffd)));
  STEP(0x8007e330);
  V0 = unsigned_less_constant(V0, 2u);
  branch_value = V0;
  STEP(0x8007e334);
  STEP(0x8007e338);
  known(&V0, 3u);
  TRY(branch_zero(run, branch_value, UINT32_C(0x8007e334), &condition));
  if (!condition)
    goto dispatch_phase;

set_current_phase:
  STEP(0x8007e33c);
  known(&AT, UINT32_C(0x800c0000));
  STEP(0x8007e340);
  TRY(write_value(run, UINT32_C(0x800bc940), 4u, UINT32_C(0x8007e340), V0));

dispatch_phase:
  /* 0x8007E344..0x8007E388: signed dispatch keeps SLTI's Boolean mask and
   * routes invalid phases directly to common publication. */
  STEP(0x8007e344);
  known(&V1, UINT32_C(0x800c0000));
  STEP(0x8007e348);
  TRY(read_value(run, UINT32_C(0x800bc940), 4u, 0, UINT32_C(0x8007e348), &V1));
  out->selected_phase = V1;
  STEP(0x8007e34c);
  known(&V0, 2u);
  compared_value = V0;
  STEP(0x8007e350);
  STEP(0x8007e354);
  V0 = signed_less_constant(V1, 3);
  TRY(branch_equal(run, V1, compared_value, UINT32_C(0x8007e350), &condition));
  if (condition)
    goto phase_two;
  branch_value = V0;
  STEP(0x8007e358);
  STEP(0x8007e35c);
  known(&V0, 1u);
  TRY(branch_zero(run, branch_value, UINT32_C(0x8007e358), &condition));
  if (condition)
    goto phase_three_or_four;
  STEP(0x8007e360);
  STEP(0x8007e364);
  TRY(branch_equal(run, V1, V0, UINT32_C(0x8007e360), &condition));
  if (condition)
    goto phase_one;
  STEP(0x8007e368);
  STEP(0x8007e36c);
  goto publish_phase;

phase_three_or_four:
  STEP(0x8007e370);
  known(&V0, 3u);
  compared_value = V0;
  STEP(0x8007e374);
  STEP(0x8007e378);
  known(&V0, 4u);
  TRY(branch_equal(run, V1, compared_value, UINT32_C(0x8007e374), &condition));
  if (condition)
    goto phase_three;
  STEP(0x8007e37c);
  STEP(0x8007e380);
  TRY(branch_equal(run, V1, V0, UINT32_C(0x8007e37c), &condition));
  if (condition)
    goto phase_four;
  STEP(0x8007e384);
  STEP(0x8007e388);
  goto publish_phase;

phase_one:
  STEP(0x8007e38c);
  known(&V0, UINT32_C(0x800c0000));
  STEP(0x8007e390);
  TRY(read_value(run, UINT32_C(0x800bc944), 4u, 0, UINT32_C(0x8007e390), &V0));
  STEP(0x8007e394);
  STEP(0x8007e398);
  STEP(0x8007e39c);
  known(&A0, 3u);
  TRY(branch_equal(run, V0, V1, UINT32_C(0x8007e398), &condition));
  if (condition)
    goto publish_phase;
  out->phase_changed = 1u;
  STEP(0x8007e3a0);
  known(&RA, UINT32_C(0x8007e3a8));
  STEP(0x8007e3a4);
  known(&A1, 1u);
  TRY(invoke(run, UINT32_C(0x8007e3a0), UINT32_C(0x800799cc)));
  STEP(0x8007e3a8);
  known(&RA, UINT32_C(0x8007e3b0));
  STEP(0x8007e3ac);
  known(&A0, 15u);
  TRY(invoke(run, UINT32_C(0x8007e3a8), UINT32_C(0x80079ebc)));
  STEP(0x8007e3b0);
  known(&RA, UINT32_C(0x8007e3b8));
  STEP(0x8007e3b4);
  known(&A0, 8u);
  TRY(invoke(run, UINT32_C(0x8007e3b0), UINT32_C(0x80079ebc)));
  STEP(0x8007e3b8);
  known(&RA, UINT32_C(0x8007e3c0));
  STEP(0x8007e3bc);
  known(&A0, 8u);
  TRY(invoke(run, UINT32_C(0x8007e3b8), UINT32_C(0x80079ebc)));
  STEP(0x8007e3c0);
  STEP(0x8007e3c4);
  goto clear_busy;

phase_two:
  STEP(0x8007e3c8);
  known(&V0, UINT32_C(0x800c0000));
  STEP(0x8007e3cc);
  TRY(read_value(run, UINT32_C(0x800bc944), 4u, 0, UINT32_C(0x8007e3cc), &V0));
  STEP(0x8007e3d0);
  STEP(0x8007e3d4);
  STEP(0x8007e3d8);
  known(&A0, 7u);
  TRY(branch_equal(run, V0, V1, UINT32_C(0x8007e3d4), &condition));
  if (condition)
    goto publish_phase;
  out->phase_changed = 1u;
  STEP(0x8007e3dc);
  known(&RA, UINT32_C(0x8007e3e4));
  STEP(0x8007e3e0);
  known(&A1, 0u);
  TRY(invoke(run, UINT32_C(0x8007e3dc), UINT32_C(0x800799cc)));
  STEP(0x8007e3e4);
  known(&RA, UINT32_C(0x8007e3ec));
  STEP(0x8007e3e8);
  known(&A0, 13u);
  TRY(invoke(run, UINT32_C(0x8007e3e4), UINT32_C(0x80079ebc)));
  STEP(0x8007e3ec);
  STEP(0x8007e3f0);
  goto clear_busy;

phase_three:
  STEP(0x8007e3f4);
  known(&V0, UINT32_C(0x800c0000));
  STEP(0x8007e3f8);
  TRY(read_value(run, UINT32_C(0x800bc944), 4u, 0, UINT32_C(0x8007e3f8), &V0));
  STEP(0x8007e3fc);
  STEP(0x8007e400);
  STEP(0x8007e404);
  known(&A0, 1u);
  TRY(branch_equal(run, V0, V1, UINT32_C(0x8007e400), &condition));
  if (condition)
    goto publish_phase;
  STEP(0x8007e408);
  STEP(0x8007e40c);
  goto phase_three_or_four_change;

phase_four:
  STEP(0x8007e410);
  known(&V0, UINT32_C(0x800c0000));
  STEP(0x8007e414);
  TRY(read_value(run, UINT32_C(0x800bc944), 4u, 0, UINT32_C(0x8007e414), &V0));
  STEP(0x8007e418);
  STEP(0x8007e41c);
  STEP(0x8007e420);
  known(&A0, 2u);
  TRY(branch_equal(run, V0, V1, UINT32_C(0x8007e41c), &condition));
  if (condition)
    goto publish_phase;

phase_three_or_four_change:
  out->phase_changed = 1u;
  STEP(0x8007e424);
  known(&RA, UINT32_C(0x8007e42c));
  STEP(0x8007e428);
  known(&A1, 0u);
  TRY(invoke(run, UINT32_C(0x8007e424), UINT32_C(0x800799cc)));
  STEP(0x8007e42c);
  known(&A0, UINT32_C(0x80020000));
  STEP(0x8007e430);
  TRY(read_value(run, UINT32_C(0x80021ed8), 1u, 0, UINT32_C(0x8007e430), &A0));
  STEP(0x8007e434);
  known(&RA, UINT32_C(0x8007e43c));
  STEP(0x8007e438);
  TRY(invoke(run, UINT32_C(0x8007e434), UINT32_C(0x80079f78)));

clear_busy:
  STEP(0x8007e43c);
  known(&AT, UINT32_C(0x80100000));
  STEP(0x8007e440);
  TRY(write_value(run, UINT32_C(0x800fc99c), 4u, UINT32_C(0x8007e440), ZERO));

publish_phase:
  STEP(0x8007e444);
  known(&V0, UINT32_C(0x800c0000));
  STEP(0x8007e448);
  TRY(read_value(run, UINT32_C(0x800bc940), 4u, 0, UINT32_C(0x8007e448), &V0));
  out->published_phase = V0;
  STEP(0x8007e44c);
  known(&AT, UINT32_C(0x800c0000));
  STEP(0x8007e450);
  TRY(write_value(run, UINT32_C(0x800bc944), 4u, UINT32_C(0x8007e450), V0));
  return epilogue(run);
}
