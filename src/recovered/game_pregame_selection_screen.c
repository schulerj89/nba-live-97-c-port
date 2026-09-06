#include "game_pregame_selection_screen.h"

#include <string.h>

typedef struct Run {
  Nba97GamePregameSelectionScreenContext *context;
  Nba97GamePregameSelectionScreenProgress *out;
  Nba97GamePregameSelectionScreenMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define S(index) (NBA97_MATCH_INITIALIZE_S0 + (index))
#define TRY(expression)                                                        \
  do {                                                                         \
    int status_ = (expression);                                                \
    if (status_ != NBA97_TEXT_COMPLETE)                                        \
      return status_;                                                          \
  } while (0)

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t entry) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  run->out->stopped_entry = entry;
  publish(run);
}

static void set_known(Nba97GamePregameSelectionScreenWord *value,
                      uint32_t word) {
  value->word = word;
  value->known_mask = 15u;
}

static int
machine_valid(const Nba97GamePregameSelectionScreenMachine *machine) {
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

static int memory_valid(const Nba97GameTextMemory *memory) {
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

static int initialize(Nba97GamePregameSelectionScreenContext *context,
                      Nba97GamePregameSelectionScreenProgress *out, Run *run) {
  if (out == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(out, 0, sizeof(*out));
  if (context == NULL || !memory_valid(&context->memory) ||
      !machine_valid(&context->machine) ||
      (context->access_journal_capacity != 0u &&
       context->access_journal == NULL))
    return NBA97_TEXT_ARGUMENT;
  run->context = context;
  run->out = out;
  run->machine = context->machine;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static Nba97GamePregameSelectionScreenWord
add_words(Nba97GamePregameSelectionScreenWord left,
          Nba97GamePregameSelectionScreenWord right) {
  Nba97GamePregameSelectionScreenWord result;
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

static Nba97GamePregameSelectionScreenWord
add_constant(Nba97GamePregameSelectionScreenWord value, uint32_t constant) {
  Nba97GamePregameSelectionScreenWord immediate;
  set_known(&immediate, constant);
  return add_words(value, immediate);
}

static Nba97GamePregameSelectionScreenWord
subtract_words(Nba97GamePregameSelectionScreenWord left,
               Nba97GamePregameSelectionScreenWord right) {
  Nba97GamePregameSelectionScreenWord inverted;
  unsigned byte;
  inverted.word = ~right.word;
  inverted.known_mask = right.known_mask;
  inverted = add_constant(inverted, 1u);
  left = add_words(left, inverted);
  /* When both inputs are complete, keep the exact full-known SUBU result. */
  if (right.known_mask == 15u) {
    unsigned all = 1u;
    for (byte = 0u; byte != 4u; ++byte)
      if ((left.known_mask & (1u << byte)) == 0u)
        all = 0u;
    if (all != 0u)
      left.known_mask = 15u;
  }
  return left;
}

static Nba97GamePregameSelectionScreenWord
and_constant(Nba97GamePregameSelectionScreenWord value, uint32_t constant) {
  Nba97GamePregameSelectionScreenWord result;
  unsigned byte;
  result.word = value.word & constant;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    uint32_t mask = UINT32_C(255) << (8u * byte);
    if ((value.known_mask & (1u << byte)) != 0u || (constant & mask) == 0u)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static void signed_bounds(Nba97GamePregameSelectionScreenWord value,
                          uint32_t *minimum, uint32_t *maximum) {
  uint32_t biased = value.word ^ UINT32_C(0x80000000);
  unsigned byte;
  *minimum = 0u;
  *maximum = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    uint32_t mask = UINT32_C(255) << (8u * byte);
    if ((value.known_mask & (1u << byte)) != 0u) {
      *minimum |= biased & mask;
      *maximum |= biased & mask;
    } else {
      *maximum |= mask;
    }
  }
}

static Nba97GamePregameSelectionScreenWord
signed_less_constant(Nba97GamePregameSelectionScreenWord value,
                     int32_t constant) {
  Nba97GamePregameSelectionScreenWord result;
  uint32_t minimum;
  uint32_t maximum;
  uint32_t biased_constant = ((uint32_t)constant) ^ UINT32_C(0x80000000);
  signed_bounds(value, &minimum, &maximum);
  result.word = (value.word ^ UINT32_C(0x80000000)) < biased_constant ? 1u : 0u;
  result.known_mask = 14u;
  if (maximum < biased_constant) {
    result.word = 1u;
    result.known_mask = 15u;
  } else if (minimum >= biased_constant) {
    result.word = 0u;
    result.known_mask = 15u;
  }
  return result;
}

static Nba97GamePregameSelectionScreenWord
signed_greater_zero(Nba97GamePregameSelectionScreenWord value) {
  Nba97GamePregameSelectionScreenWord result;
  uint32_t minimum;
  uint32_t maximum;
  const uint32_t biased_zero = UINT32_C(0x80000000);
  signed_bounds(value, &minimum, &maximum);
  result.word = (value.word ^ UINT32_C(0x80000000)) > biased_zero ? 1u : 0u;
  result.known_mask = 14u;
  if (minimum > biased_zero) {
    result.word = 1u;
    result.known_mask = 15u;
  } else if (maximum <= biased_zero) {
    result.word = 0u;
    result.known_mask = 15u;
  }
  return result;
}

static int branch_zero(Run *run, Nba97GamePregameSelectionScreenWord value,
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

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static uint32_t width_mask(unsigned width) {
  return width == 4u ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - 1u;
}

static uint8_t known_width_mask(unsigned width) {
  return (uint8_t)((1u << width) - 1u);
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width, Nba97GamePregameSelectionScreenWord value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GamePregameSelectionScreenAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word & width_mask(width);
    event->operation = run->out->operations;
    event->width = (uint8_t)width;
    event->known_mask = (uint8_t)(value.known_mask & known_width_mask(width));
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t address, unsigned width,
                  unsigned alignment, uint32_t pc, uint8_t **data,
                  uint8_t **known) {
  size_t index;
  stop(run, pc, address, 0u);
  TRY(spend(run));
  ++run->out->accesses;
  if ((address & (alignment - 1u)) != 0u)
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

static int address(Run *run, Nba97GamePregameSelectionScreenWord base,
                   uint32_t offset, uint32_t pc, uint32_t *result) {
  Nba97GamePregameSelectionScreenWord effective = add_constant(base, offset);
  if (effective.known_mask != 15u) {
    stop(run, pc, effective.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = effective.word;
  return NBA97_TEXT_COMPLETE;
}

static int read_value(Run *run, unsigned destination, unsigned base,
                      uint32_t offset, unsigned width, unsigned alignment,
                      int sign_extend, uint32_t pc) {
  uint32_t effective;
  uint8_t *data;
  uint8_t *known;
  Nba97GamePregameSelectionScreenWord loaded;
  unsigned byte;
  TRY(address(run, R(base), offset, pc, &effective));
  TRY(locate(run, effective, width, alignment, pc, &data, &known));
  loaded.word = 0u;
  loaded.known_mask = 0u;
  for (byte = 0u; byte != width; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (8u * byte);
    if (known == NULL || known[byte] != 0u)
      loaded.known_mask = (uint8_t)(loaded.known_mask | (uint8_t)(1u << byte));
  }
  if (width == 2u) {
    if (sign_extend != 0) {
      if ((loaded.word & UINT32_C(0x8000)) != 0u)
        loaded.word |= UINT32_C(0xffff0000);
      if ((loaded.known_mask & 2u) != 0u)
        loaded.known_mask = (uint8_t)(loaded.known_mask | 12u);
    } else {
      loaded.known_mask = (uint8_t)(loaded.known_mask | 12u);
    }
  }
  R(destination) = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_PREGAME_SELECTION_SCREEN_READ, pc, effective, width,
          loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, unsigned source, unsigned base,
                       uint32_t offset, unsigned width, unsigned alignment,
                       uint32_t pc) {
  uint32_t effective;
  uint8_t *data;
  uint8_t *known;
  Nba97GamePregameSelectionScreenWord value = R(source);
  unsigned byte;
  uint8_t required = known_width_mask(width);
  TRY(address(run, R(base), offset, pc, &effective));
  TRY(locate(run, effective, width, alignment, pc, &data, &known));
  if (known == NULL && (value.known_mask & required) != required)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0u; byte != width; ++byte) {
    data[byte] = (uint8_t)(value.word >> (8u * byte));
    if (known != NULL)
      known[byte] = (uint8_t)((value.known_mask >> byte) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_PREGAME_SELECTION_SCREEN_STORE, pc, effective, width,
          value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static void jal(Run *run, uint32_t pc) {
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
  publish(run);
}

static int invoke_after_delay(Run *run, uint32_t pc, uint32_t entry,
                              uint8_t kind, uint8_t argument_count) {
  Nba97GamePregameSelectionScreenEvent event;
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
  event.argument_count = argument_count;
  publish(run);
  if (run->context->io == NULL)
    return NBA97_TEXT_IO_REFUSED;
  accepted = run->context->io(run->context->user, &run->context->memory, &event,
                              &run->machine);
  publish(run);
  if (accepted != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!machine_valid(&run->machine))
    return NBA97_TEXT_ARGUMENT;
  ++run->out->callbacks_completed;
  ++run->out->call_count[kind];
  return NBA97_TEXT_COMPLETE;
}

static int call_nop(Run *run, uint32_t pc, uint32_t entry, uint8_t kind,
                    uint8_t argument_count) {
  jal(run, pc);
  return invoke_after_delay(run, pc, entry, kind, argument_count);
}

static int restore(Run *run, unsigned destination, uint32_t offset, uint32_t pc,
                   Nba97GamePregameSelectionScreenWord *reported) {
  TRY(read_value(run, destination, NBA97_MATCH_INITIALIZE_SP, offset, 4u, 4u, 0,
                 pc));
  *reported = R(destination);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_pregame_selection_screen(
    Nba97GamePregameSelectionScreenContext *context,
    Nba97GamePregameSelectionScreenProgress *out) {
  Run storage;
  Run *run = &storage;
  Nba97GamePregameSelectionScreenWord predicate;
  int zero;

  TRY(initialize(context, out, run));

  /* 0x80046C2C..0x80046C58: save ra and s8..s0 in source order. The s0 spill
   * is the first JAL delay and therefore follows JAL's ra assignment. */
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffc0));
  out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  TRY(write_value(run, NBA97_MATCH_INITIALIZE_RA, NBA97_MATCH_INITIALIZE_SP,
                  0x3cu, 4u, 4u, UINT32_C(0x80046c30)));
  TRY(write_value(run, NBA97_MATCH_INITIALIZE_FP, NBA97_MATCH_INITIALIZE_SP,
                  0x38u, 4u, 4u, UINT32_C(0x80046c34)));
  TRY(write_value(run, S(7), NBA97_MATCH_INITIALIZE_SP, 0x34u, 4u, 4u,
                  UINT32_C(0x80046c38)));
  TRY(write_value(run, S(6), NBA97_MATCH_INITIALIZE_SP, 0x30u, 4u, 4u,
                  UINT32_C(0x80046c3c)));
  TRY(write_value(run, S(5), NBA97_MATCH_INITIALIZE_SP, 0x2cu, 4u, 4u,
                  UINT32_C(0x80046c40)));
  TRY(write_value(run, S(4), NBA97_MATCH_INITIALIZE_SP, 0x28u, 4u, 4u,
                  UINT32_C(0x80046c44)));
  TRY(write_value(run, S(3), NBA97_MATCH_INITIALIZE_SP, 0x24u, 4u, 4u,
                  UINT32_C(0x80046c48)));
  TRY(write_value(run, S(2), NBA97_MATCH_INITIALIZE_SP, 0x20u, 4u, 4u,
                  UINT32_C(0x80046c4c)));
  TRY(write_value(run, S(1), NBA97_MATCH_INITIALIZE_SP, 0x1cu, 4u, 4u,
                  UINT32_C(0x80046c50)));
  jal(run, UINT32_C(0x80046c54));
  TRY(write_value(run, S(0), NBA97_MATCH_INITIALIZE_SP, 0x18u, 4u, 4u,
                  UINT32_C(0x80046c58)));
  TRY(invoke_after_delay(run, UINT32_C(0x80046c54), UINT32_C(0x80081358),
                         NBA97_GAME_PREGAME_SELECTION_SCREEN_80081358, 0u));

  /* 0x80046C5C..0x80046C74: initialize coupled selections and ask 363DC to
   * populate signed stack halfwords through callback-live sp. */
  set_known(&R(S(4)), 0u);
  set_known(&R(S(1)), 0u);
  set_known(&R(S(7)), 0u);
  set_known(&R(S(2)), 12u);
  R(NBA97_MATCH_INITIALIZE_A0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x10u);
  jal(run, UINT32_C(0x80046c70));
  R(NBA97_MATCH_INITIALIZE_A1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x12u);
  TRY(invoke_after_delay(run, UINT32_C(0x80046c70), UINT32_C(0x800363dc),
                         NBA97_GAME_PREGAME_SELECTION_SCREEN_800363DC, 2u));

redraw:
  ++out->redraws;
  /* 0x80046C78..0x80046CCC: derive the coupled-selection delta in the 3081C
   * delay, draw the screen, snapshot demo state, and begin polling. */
  R(NBA97_MATCH_INITIALIZE_V0) = add_constant(R(S(2)), UINT32_C(0xfffffff4));
  jal(run, UINT32_C(0x80046c7c));
  R(S(3)) = subtract_words(R(S(1)), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(invoke_after_delay(run, UINT32_C(0x80046c7c), UINT32_C(0x8003081c),
                         NBA97_GAME_PREGAME_SELECTION_SCREEN_8003081C, 0u));
  set_known(&R(S(0)), UINT32_C(0x80020000));
  R(S(0)) = add_constant(R(S(0)), UINT32_C(0xffffedec));
  TRY(read_value(run, NBA97_MATCH_INITIALIZE_V0, S(0), 0u, 2u, 2u, 0,
                 UINT32_C(0x80046c8c)));
  predicate = R(NBA97_MATCH_INITIALIZE_V0);
  R(NBA97_MATCH_INITIALIZE_A0) = R(S(1));
  TRY(branch_zero(run, predicate, UINT32_C(0x80046c94), &zero));
  if (!zero) {
    TRY(call_nop(run, UINT32_C(0x80046c9c), UINT32_C(0x80035678),
                 NBA97_GAME_PREGAME_SELECTION_SCREEN_80035678, 1u));
    R(NBA97_MATCH_INITIALIZE_A0) = R(S(1));
  }
  jal(run, UINT32_C(0x80046ca8));
  R(NBA97_MATCH_INITIALIZE_A1) = R(S(2));
  TRY(invoke_after_delay(run, UINT32_C(0x80046ca8), UINT32_C(0x80046738),
                         NBA97_GAME_PREGAME_SELECTION_SCREEN_80046738, 2u));
  jal(run, UINT32_C(0x80046cb0));
  R(NBA97_MATCH_INITIALIZE_FP) = R(S(0));
  TRY(invoke_after_delay(run, UINT32_C(0x80046cb0), UINT32_C(0x80049018),
                         NBA97_GAME_PREGAME_SELECTION_SCREEN_80049018, 0u));
  TRY(call_nop(run, UINT32_C(0x80046cb8), UINT32_C(0x800a5810),
               NBA97_GAME_PREGAME_SELECTION_SCREEN_800A5810, 0u));
  R(S(5)) = R(NBA97_MATCH_INITIALIZE_V0);
  set_known(&R(S(6)), UINT32_C(0x80100000));
  R(S(6)) = add_constant(R(S(6)), UINT32_C(0xffffdb9c));

poll:
  ++out->polls;
  TRY(call_nop(run, UINT32_C(0x80046ccc), UINT32_C(0x80083eec),
               NBA97_GAME_PREGAME_SELECTION_SCREEN_80083EEC, 0u));
  TRY(read_value(run, NBA97_MATCH_INITIALIZE_A0, NBA97_MATCH_INITIALIZE_SP,
                 0x10u, 2u, 2u, 1, UINT32_C(0x80046cd4)));
  jal(run, UINT32_C(0x80046cd8));
  R(NBA97_MATCH_INITIALIZE_A1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x12u);
  TRY(invoke_after_delay(run, UINT32_C(0x80046cd8), UINT32_C(0x80036478),
                         NBA97_GAME_PREGAME_SELECTION_SCREEN_80036478, 2u));
  TRY(read_value(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_FP, 0u,
                 2u, 2u, 0, UINT32_C(0x80046ce0)));
  predicate = R(NBA97_MATCH_INITIALIZE_V1);
  R(S(0)) = R(NBA97_MATCH_INITIALIZE_V0);
  TRY(branch_zero(run, predicate, UINT32_C(0x80046ce8), &zero));
  if (zero) {
    TRY(branch_zero(run, R(S(7)), UINT32_C(0x80046cf0), &zero));
    if (!zero)
      goto masks;
  }

  TRY(branch_zero(run, R(S(0)), UINT32_C(0x80046cf8), &zero));
  if (!zero) {
    predicate = R(NBA97_MATCH_INITIALIZE_V1);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1u);
    TRY(branch_zero(run, predicate, UINT32_C(0x80046d00), &zero));
    if (zero) {
      set_known(&R(S(7)), 1u);
      out->input_latched = 1u;
      goto masks;
    }
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
    TRY(write_value(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_AT,
                    UINT32_C(0xffffdb78), 1u, 1u, UINT32_C(0x80046d14)));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 99u);
    TRY(write_value(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_FP,
                    0u, 2u, 2u, UINT32_C(0x80046d20)));
    out->demo_skip = 1u;
    goto exit_services;
  }

  /* 0x80046D24..0x80046D58: no input accumulates only positive wrapping
   * timer deltas. At 360 ticks it advances unless selection one reached four.
   */
  TRY(call_nop(run, UINT32_C(0x80046d24), UINT32_C(0x800a5810),
               NBA97_GAME_PREGAME_SELECTION_SCREEN_800A5810, 0u));
  R(NBA97_MATCH_INITIALIZE_V1) =
      subtract_words(R(NBA97_MATCH_INITIALIZE_V0), R(S(5)));
  predicate = signed_greater_zero(R(NBA97_MATCH_INITIALIZE_V1));
  R(S(5)) = R(NBA97_MATCH_INITIALIZE_V0);
  TRY(branch_zero(run, predicate, UINT32_C(0x80046d30), &zero));
  if (zero)
    goto masks;
  R(S(4)) = add_words(R(S(4)), R(NBA97_MATCH_INITIALIZE_V1));
  R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(R(S(4)), 360);
  predicate = R(NBA97_MATCH_INITIALIZE_V0);
  R(NBA97_MATCH_INITIALIZE_V0) = and_constant(R(S(0)), 0x20u);
  TRY(branch_zero(run, predicate, UINT32_C(0x80046d40), &zero));
  if (!zero)
    goto masks_after_delay;
  R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(R(S(1)), 4);
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80046d4c),
                  &zero));
  if (zero)
    goto exit_services;
  set_known(&R(S(4)), 0u);
  goto sound_next_coupled;

masks:
  R(NBA97_MATCH_INITIALIZE_V0) = and_constant(R(S(0)), 0x20u);
masks_after_delay:
  /* 0x80046D60..0x80046D94: mask 0x20 temporarily installs stack+0x12 into
   * the controller halfword, then restores callback-live s0 through live s6. */
  predicate = R(NBA97_MATCH_INITIALIZE_V0);
  R(NBA97_MATCH_INITIALIZE_V0) = and_constant(R(S(0)), 8u);
  TRY(branch_zero(run, predicate, UINT32_C(0x80046d60), &zero));
  if (!zero) {
    jal(run, UINT32_C(0x80046d68));
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), 97u);
    TRY(invoke_after_delay(run, UINT32_C(0x80046d68), UINT32_C(0x80029258),
                           NBA97_GAME_PREGAME_SELECTION_SCREEN_80029258, 1u));
    TRY(read_value(run, S(0), S(6), 0u, 2u, 2u, 1, UINT32_C(0x80046d70)));
    TRY(read_value(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_SP,
                   0x12u, 2u, 2u, 0, UINT32_C(0x80046d74)));
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x800b0000));
    R(NBA97_MATCH_INITIALIZE_A0) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A0), 0x2fd4u);
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), 1u);
    jal(run, UINT32_C(0x80046d84));
    TRY(write_value(run, NBA97_MATCH_INITIALIZE_V0, S(6), 0u, 2u, 2u,
                    UINT32_C(0x80046d88)));
    TRY(invoke_after_delay(run, UINT32_C(0x80046d84), UINT32_C(0x80036be4),
                           NBA97_GAME_PREGAME_SELECTION_SCREEN_80036BE4, 2u));
    TRY(write_value(run, S(0), S(6), 0u, 2u, 2u, UINT32_C(0x80046d8c)));
    set_known(&R(S(0)), 0u);
    goto frame_and_exit_bits;
  }

  /* 0x80046D98..0x80046E44: masks 8 and 4 move the coupled selections. All
   * bound checks retain their original delay writes even on unknown refusal. */
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80046d98),
                  &zero));
  if (!zero) {
    TRY(branch_zero(run, R(S(3)), UINT32_C(0x80046da0), &zero));
    if (zero) {
      predicate = signed_greater_zero(R(S(1)));
      R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(R(S(2)), 13);
      TRY(branch_zero(run, predicate, UINT32_C(0x80046da8), &zero));
      if (zero) {
        predicate = R(NBA97_MATCH_INITIALIZE_V0);
        R(NBA97_MATCH_INITIALIZE_V0) = and_constant(R(S(0)), 4u);
        TRY(branch_zero(run, predicate, UINT32_C(0x80046db0), &zero));
        if (!zero)
          goto process_next_mask;
      }
    }
    jal(run, UINT32_C(0x80046db8));
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), 98u);
    TRY(invoke_after_delay(run, UINT32_C(0x80046db8), UINT32_C(0x80029258),
                           NBA97_GAME_PREGAME_SELECTION_SCREEN_80029258, 1u));
    predicate = signed_greater_zero(R(S(3)));
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(R(S(2)), 13);
    TRY(branch_zero(run, predicate, UINT32_C(0x80046dc0), &zero));
    if (zero) {
      predicate = R(S(1));
      R(S(2)) = add_constant(R(S(1)), 12u);
      TRY(branch_zero(run, predicate, UINT32_C(0x80046dc8), &zero));
      if (zero)
        goto redraw;
      R(S(1)) = add_constant(R(S(1)), UINT32_MAX);
      R(S(2)) = add_constant(R(S(1)), 12u);
      goto redraw;
    }
    predicate = R(NBA97_MATCH_INITIALIZE_V0);
    R(S(1)) = add_constant(R(S(2)), UINT32_C(0xfffffff4));
    TRY(branch_zero(run, predicate, UINT32_C(0x80046dd8), &zero));
    if (!zero)
      goto redraw;
    R(S(2)) = add_constant(R(S(2)), UINT32_MAX);
    R(S(1)) = add_constant(R(S(2)), UINT32_C(0xfffffff4));
    goto redraw;
  }

process_next_mask:
  R(NBA97_MATCH_INITIALIZE_V0) = and_constant(R(S(0)), 4u);
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80046dec),
                  &zero));
  if (!zero) {
    predicate = R(S(3));
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(R(S(1)), 4);
    TRY(branch_zero(run, predicate, UINT32_C(0x80046df4), &zero));
    if (zero) {
      predicate = R(NBA97_MATCH_INITIALIZE_V0);
      R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(R(S(2)), 16);
      TRY(branch_zero(run, predicate, UINT32_C(0x80046dfc), &zero));
      if (zero) {
        predicate = R(NBA97_MATCH_INITIALIZE_V0);
        R(NBA97_MATCH_INITIALIZE_V0) = and_constant(R(S(0)), 0x200u);
        TRY(branch_zero(run, predicate, UINT32_C(0x80046e04), &zero));
        if (zero)
          goto secondary_masks_after_delay;
      }
    }
  sound_next_coupled:
    jal(run, UINT32_C(0x80046e0c));
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), 98u);
    TRY(invoke_after_delay(run, UINT32_C(0x80046e0c), UINT32_C(0x80029258),
                           NBA97_GAME_PREGAME_SELECTION_SCREEN_80029258, 1u));
    predicate = signed_less_constant(R(S(3)), 0);
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(R(S(1)), 4);
    TRY(branch_zero(run, predicate, UINT32_C(0x80046e14), &zero));
    if (zero) {
      predicate = R(NBA97_MATCH_INITIALIZE_V0);
      R(S(2)) = add_constant(R(S(1)), 12u);
      TRY(branch_zero(run, predicate, UINT32_C(0x80046e1c), &zero));
      if (zero)
        goto redraw;
      R(S(1)) = add_constant(R(S(1)), 1u);
      R(S(2)) = add_constant(R(S(1)), 12u);
      goto redraw;
    }
    R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(R(S(2)), 16);
    predicate = R(NBA97_MATCH_INITIALIZE_V0);
    R(S(1)) = add_constant(R(S(2)), UINT32_C(0xfffffff4));
    TRY(branch_zero(run, predicate, UINT32_C(0x80046e34), &zero));
    if (zero)
      goto redraw;
    R(S(2)) = add_constant(R(S(2)), 1u);
    R(S(1)) = add_constant(R(S(2)), UINT32_C(0xfffffff4));
    goto redraw;
  }

  /* 0x80046E48..0x80046ED4: independent masks adjust s2 within 12..16 and
   * s1 within 0..4, playing source sound 99 only for accepted changes. */
  R(NBA97_MATCH_INITIALIZE_V0) = and_constant(R(S(0)), 0x200u);
secondary_masks_after_delay:
  predicate = R(NBA97_MATCH_INITIALIZE_V0);
  R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(R(S(2)), 13);
  TRY(branch_zero(run, predicate, UINT32_C(0x80046e4c), &zero));
  if (!zero) {
    predicate = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) = and_constant(R(S(0)), 0x400u);
    TRY(branch_zero(run, predicate, UINT32_C(0x80046e54), &zero));
    if (zero) {
      jal(run, UINT32_C(0x80046e5c));
      set_known(&R(NBA97_MATCH_INITIALIZE_A0), 99u);
      TRY(invoke_after_delay(run, UINT32_C(0x80046e5c), UINT32_C(0x80029258),
                             NBA97_GAME_PREGAME_SELECTION_SCREEN_80029258, 1u));
      R(S(2)) = add_constant(R(S(2)), UINT32_MAX);
      goto redraw;
    }
  }

  R(NBA97_MATCH_INITIALIZE_V0) = and_constant(R(S(0)), 0x400u);
  predicate = R(NBA97_MATCH_INITIALIZE_V0);
  R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(R(S(2)), 16);
  TRY(branch_zero(run, predicate, UINT32_C(0x80046e70), &zero));
  if (!zero) {
    predicate = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) = and_constant(R(S(0)), 0x1000u);
    TRY(branch_zero(run, predicate, UINT32_C(0x80046e78), &zero));
    if (!zero) {
      jal(run, UINT32_C(0x80046e80));
      set_known(&R(NBA97_MATCH_INITIALIZE_A0), 99u);
      TRY(invoke_after_delay(run, UINT32_C(0x80046e80), UINT32_C(0x80029258),
                             NBA97_GAME_PREGAME_SELECTION_SCREEN_80029258, 1u));
      R(S(2)) = add_constant(R(S(2)), 1u);
      goto redraw;
    }
  }

  R(NBA97_MATCH_INITIALIZE_V0) = and_constant(R(S(0)), 0x1000u);
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80046e94),
                  &zero));
  if (!zero) {
    predicate = R(S(1));
    R(NBA97_MATCH_INITIALIZE_V0) = and_constant(R(S(0)), 0x2000u);
    TRY(branch_zero(run, predicate, UINT32_C(0x80046e9c), &zero));
    if (!zero) {
      jal(run, UINT32_C(0x80046ea4));
      set_known(&R(NBA97_MATCH_INITIALIZE_A0), 99u);
      TRY(invoke_after_delay(run, UINT32_C(0x80046ea4), UINT32_C(0x80029258),
                             NBA97_GAME_PREGAME_SELECTION_SCREEN_80029258, 1u));
      R(S(1)) = add_constant(R(S(1)), UINT32_MAX);
      goto redraw;
    }
  } else {
    R(NBA97_MATCH_INITIALIZE_V0) = and_constant(R(S(0)), 0x2000u);
  }

  predicate = R(NBA97_MATCH_INITIALIZE_V0);
  R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(R(S(1)), 4);
  TRY(branch_zero(run, predicate, UINT32_C(0x80046eb8), &zero));
  if (!zero) {
    TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80046ec0),
                    &zero));
    if (!zero) {
      jal(run, UINT32_C(0x80046ec8));
      set_known(&R(NBA97_MATCH_INITIALIZE_A0), 99u);
      TRY(invoke_after_delay(run, UINT32_C(0x80046ec8), UINT32_C(0x80029258),
                             NBA97_GAME_PREGAME_SELECTION_SCREEN_80029258, 1u));
      R(S(1)) = add_constant(R(S(1)), 1u);
      goto redraw;
    }
  }

frame_and_exit_bits:
  /* 0x80046ED8..0x80046F30: present one frame, loop unless input bits 0x180
   * request exit, then run the optional demo service and five exit services. */
  TRY(call_nop(run, UINT32_C(0x80046ed8), UINT32_C(0x80049018),
               NBA97_GAME_PREGAME_SELECTION_SCREEN_80049018, 0u));
  R(NBA97_MATCH_INITIALIZE_V0) = and_constant(R(S(0)), 0x180u);
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80046ee4),
                  &zero));
  if (zero)
    goto poll;
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
  TRY(read_value(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_V0,
                 UINT32_C(0xffffedec), 2u, 2u, 0, UINT32_C(0x80046ef0)));
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80046ef8),
                  &zero));
  if (!zero)
    TRY(call_nop(run, UINT32_C(0x80046f00), UINT32_C(0x80035678),
                 NBA97_GAME_PREGAME_SELECTION_SCREEN_80035678, 0u));

exit_services:
  jal(run, UINT32_C(0x80046f08));
  set_known(&R(NBA97_MATCH_INITIALIZE_A0), 97u);
  TRY(invoke_after_delay(run, UINT32_C(0x80046f08), UINT32_C(0x80029258),
                         NBA97_GAME_PREGAME_SELECTION_SCREEN_80029258, 1u));
  TRY(read_value(run, NBA97_MATCH_INITIALIZE_A0, NBA97_MATCH_INITIALIZE_SP,
                 0x12u, 2u, 2u, 1, UINT32_C(0x80046f10)));
  TRY(call_nop(run, UINT32_C(0x80046f14), UINT32_C(0x80036600),
               NBA97_GAME_PREGAME_SELECTION_SCREEN_80036600, 1u));
  TRY(call_nop(run, UINT32_C(0x80046f1c), UINT32_C(0x8003081c),
               NBA97_GAME_PREGAME_SELECTION_SCREEN_8003081C, 0u));
  TRY(call_nop(run, UINT32_C(0x80046f24), UINT32_C(0x80049018),
               NBA97_GAME_PREGAME_SELECTION_SCREEN_80049018, 0u));
  TRY(call_nop(run, UINT32_C(0x80046f2c), UINT32_C(0x8008048c),
               NBA97_GAME_PREGAME_SELECTION_SCREEN_8008048C, 0u));

  /* 0x80046F34..0x80046F64: restore ra and s8..s0 through callback-live sp,
   * advance the exact frame, and consume ra after the final NOP delay. */
  TRY(restore(run, NBA97_MATCH_INITIALIZE_RA, 0x3cu, UINT32_C(0x80046f34),
              &out->restored_return_address));
  TRY(restore(run, NBA97_MATCH_INITIALIZE_FP, 0x38u, UINT32_C(0x80046f38),
              &out->restored_s[8]));
  TRY(restore(run, S(7), 0x34u, UINT32_C(0x80046f3c), &out->restored_s[7]));
  TRY(restore(run, S(6), 0x30u, UINT32_C(0x80046f40), &out->restored_s[6]));
  TRY(restore(run, S(5), 0x2cu, UINT32_C(0x80046f44), &out->restored_s[5]));
  TRY(restore(run, S(4), 0x28u, UINT32_C(0x80046f48), &out->restored_s[4]));
  TRY(restore(run, S(3), 0x24u, UINT32_C(0x80046f4c), &out->restored_s[3]));
  TRY(restore(run, S(2), 0x20u, UINT32_C(0x80046f50), &out->restored_s[2]));
  TRY(restore(run, S(1), 0x1cu, UINT32_C(0x80046f54), &out->restored_s[1]));
  TRY(restore(run, S(0), 0x18u, UINT32_C(0x80046f58), &out->restored_s[0]));
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x40u);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15u) {
    stop(run, UINT32_C(0x80046f60), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if ((R(NBA97_MATCH_INITIALIZE_RA).word & 3u) != 0u) {
    stop(run, UINT32_C(0x80046f60), R(NBA97_MATCH_INITIALIZE_RA).word, 0u);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1u;
  stop(run, 0u, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
