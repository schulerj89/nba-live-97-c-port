#include "game_cross_half_rule.h"

#include <string.h>

#define PHASE UINT32_C(0x800fdb90)
#define TEAM UINT32_C(0x800fdb94)
#define DELTA UINT32_C(0x800fdb6c)
#define OWNER UINT32_C(0x800fdbcc)
#define TIMER UINT32_C(0x800fdbac)
#define ARM_VALUE UINT32_C(0x800fdbaa)
#define FIRST_ACTOR_POINTER UINT32_C(0x800fdc38)
#define SECOND_ACTOR_POINTER UINT32_C(0x800fdc34)
#define ENABLE UINT32_C(0x80021d8b)
#define ACTOR_BLOCK UINT32_C(0x800fe8cc)
#define CROSSING_BLOCK UINT32_C(0x800fe8e0)
#define RULE_CODE UINT32_C(0x800fe882)

typedef struct Run {
  Nba97GameCrossHalfRuleContext *context;
  Nba97GameCrossHalfRuleProgress *out;
  Nba97GameCrossHalfRuleMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int nba97_status_ = (expression);                                          \
    if (nba97_status_ != NBA97_TEXT_COMPLETE)                                  \
      return nba97_status_;                                                    \
  } while (0)

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t entry) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  run->out->stopped_entry = entry;
  publish(run);
}

static void set_known(Nba97GameCrossHalfRuleWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameCrossHalfRuleMachine *machine) {
  unsigned index;
  if (machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0u ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (index = 0u; index != NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++index)
    if (machine->registers.gpr[index].known_mask > 0x0fu)
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

static int initialize(Nba97GameCrossHalfRuleContext *context,
                      Nba97GameCrossHalfRuleProgress *out, Run *run) {
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

static Nba97GameCrossHalfRuleWord add_words(Nba97GameCrossHalfRuleWord left,
                                            Nba97GameCrossHalfRuleWord right) {
  Nba97GameCrossHalfRuleWord result;
  unsigned carry_mask = 1u;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0u;
  if (left.known_mask == 0x0fu && right.known_mask == 0x0fu) {
    result.known_mask = 0x0fu;
    return result;
  }
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
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
    carry_mask = next_carry_mask;
  }
  return result;
}

static Nba97GameCrossHalfRuleWord add_constant(Nba97GameCrossHalfRuleWord value,
                                               uint32_t constant) {
  Nba97GameCrossHalfRuleWord immediate;
  set_known(&immediate, constant);
  return add_words(value, immediate);
}

static uint32_t width_mask(unsigned width) {
  return width == 4u ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - 1u;
}

static uint8_t known_width_mask(unsigned width) {
  return (uint8_t)((1u << width) - 1u);
}

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width, const Nba97GameCrossHalfRuleWord *value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameCrossHalfRuleAccess *event = &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word & width_mask(width);
    event->operation = run->out->operations;
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
  ++run->out->accesses;
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

static int address_from(Run *run, Nba97GameCrossHalfRuleWord base,
                        uint32_t offset, uint32_t pc, uint32_t *address) {
  Nba97GameCrossHalfRuleWord effective = add_constant(base, offset);
  if (effective.known_mask != 0x0fu) {
    stop(run, pc, effective.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = effective.word;
  return NBA97_TEXT_COMPLETE;
}

static int read_value(Run *run, uint32_t address, unsigned width, uint32_t pc,
                      Nba97GameCrossHalfRuleWord *destination) {
  uint8_t *data;
  uint8_t *known;
  Nba97GameCrossHalfRuleWord loaded;
  unsigned byte;
  TRY(locate(run, address, width, pc, &data, &known));
  loaded.word = 0u;
  loaded.known_mask = 0u;
  for (byte = 0u; byte != width; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (8u * byte);
    if (known == NULL || known[byte] != 0u)
      loaded.known_mask = (uint8_t)(loaded.known_mask | (uint8_t)(1u << byte));
  }
  *destination = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_CROSS_HALF_RULE_READ, pc, address, width, &loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, uint32_t address, unsigned width, uint32_t pc,
                       Nba97GameCrossHalfRuleWord value) {
  uint8_t *data;
  uint8_t *known;
  unsigned byte;
  value.word &= width_mask(width);
  value.known_mask = (uint8_t)(value.known_mask & known_width_mask(width));
  TRY(locate(run, address, width, pc, &data, &known));
  if (known == NULL && value.known_mask != known_width_mask(width))
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0u; byte != width; ++byte) {
    data[byte] = (uint8_t)(value.word >> (8u * byte));
    if (known != NULL)
      known[byte] = (uint8_t)((value.known_mask >> byte) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_CROSS_HALF_RULE_STORE, pc, address, width, &value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static Nba97GameCrossHalfRuleWord load_lbu(Nba97GameCrossHalfRuleWord raw) {
  raw.word &= 255u;
  raw.known_mask = (uint8_t)((raw.known_mask & 1u) | 0x0eu);
  return raw;
}

static Nba97GameCrossHalfRuleWord load_lhu(Nba97GameCrossHalfRuleWord raw) {
  raw.word &= 0xffffu;
  raw.known_mask = (uint8_t)((raw.known_mask & 3u) | 0x0cu);
  return raw;
}

static Nba97GameCrossHalfRuleWord load_lh(Nba97GameCrossHalfRuleWord raw) {
  uint32_t half = raw.word & 0xffffu;
  raw.word = (half & 0x8000u) != 0u ? half | UINT32_C(0xffff0000) : half;
  raw.known_mask = (uint8_t)(raw.known_mask & 3u);
  if ((raw.known_mask & 2u) != 0u)
    raw.known_mask = (uint8_t)(raw.known_mask | 0x0cu);
  return raw;
}

static Nba97GameCrossHalfRuleWord xor_words(Nba97GameCrossHalfRuleWord left,
                                            Nba97GameCrossHalfRuleWord right) {
  Nba97GameCrossHalfRuleWord result;
  result.word = left.word ^ right.word;
  result.known_mask = (uint8_t)(left.known_mask & right.known_mask);
  return result;
}

static Nba97GameCrossHalfRuleWord
signed_less_constant(Nba97GameCrossHalfRuleWord value, uint32_t constant) {
  Nba97GameCrossHalfRuleWord result;
  uint32_t minimum = 0u;
  uint32_t maximum = 0u;
  uint32_t biased = value.word ^ UINT32_C(0x80000000);
  uint32_t biased_constant = constant ^ UINT32_C(0x80000000);
  unsigned byte;
  result.word = biased < biased_constant ? 1u : 0u;
  result.known_mask = 0x0eu;
  for (byte = 0u; byte != 4u; ++byte) {
    uint32_t bits = UINT32_C(255) << (8u * byte);
    if ((value.known_mask & (1u << byte)) != 0u) {
      minimum |= biased & bits;
      maximum |= biased & bits;
    } else {
      maximum |= bits;
    }
  }
  if (maximum < biased_constant)
    set_known(&result, 1u);
  else if (minimum >= biased_constant)
    set_known(&result, 0u);
  return result;
}

static int decide_zero(Run *run, Nba97GameCrossHalfRuleWord value, uint32_t pc,
                       int *is_zero) {
  unsigned byte;
  for (byte = 0u; byte != 4u; ++byte)
    if ((value.known_mask & (1u << byte)) != 0u &&
        ((value.word >> (8u * byte)) & 255u) != 0u) {
      *is_zero = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (value.known_mask == 0x0fu) {
    *is_zero = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0u, 0u);
  return NBA97_TEXT_UNKNOWN;
}

static int decide_nonnegative(Run *run, Nba97GameCrossHalfRuleWord value,
                              uint32_t pc, int *nonnegative) {
  if ((value.known_mask & 8u) == 0u) {
    stop(run, pc, 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *nonnegative = (value.word & UINT32_C(0x80000000)) == 0u;
  return NBA97_TEXT_COMPLETE;
}

static int invoke(Run *run, uint32_t pc, uint32_t entry, uint8_t kind,
                  uint8_t argument_count, int delay_sets_a0,
                  uint32_t delay_a0) {
  Nba97GameCrossHalfRuleEvent event;
  int accepted;
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
  if (delay_sets_a0 != 0)
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), delay_a0);
  stop(run, pc, 0u, entry);
  TRY(spend(run));
  memset(&event, 0, sizeof(event));
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = entry;
  event.operation = run->out->operations;
  event.invocation = run->out->call_count[kind] + 1u;
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

static int restore_return_address(Run *run) {
  uint32_t address;
  TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u,
                   UINT32_C(0x800682fc), &address));
  TRY(read_value(run, address, 4u, UINT32_C(0x800682fc),
                 &R(NBA97_MATCH_INITIALIZE_RA)));
  run->out->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x18u);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x80068304), R(NBA97_MATCH_INITIALIZE_RA).word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if ((R(NBA97_MATCH_INITIALIZE_RA).word & 3u) != 0u) {
    stop(run, UINT32_C(0x80068304), R(NBA97_MATCH_INITIALIZE_RA).word, 0u);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  run->out->completed = 1u;
  stop(run, 0u, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_cross_half_rule(Nba97GameCrossHalfRuleContext *context,
                               Nba97GameCrossHalfRuleProgress *out) {
  Run storage;
  Run *run = &storage;
  Nba97GameCrossHalfRuleWord raw;
  Nba97GameCrossHalfRuleWord branch_value;
  Nba97GameCrossHalfRuleWord zero;
  uint32_t address;
  int branch;
  TRY(initialize(context, out, run));
  set_known(&zero, 0u);

  /* 0x8006817C..0x80068190: the phase load precedes frame allocation. The
   * BEQ delay always saves ra, including when SLTI remains unknown. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(read_value(run, PHASE, 2u, UINT32_C(0x80068180), &raw));
  R(NBA97_MATCH_INITIALIZE_V0) = load_lh(raw);
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe8));
  out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  R(NBA97_MATCH_INITIALIZE_V0) =
      signed_less_constant(R(NBA97_MATCH_INITIALIZE_V0), 128u);
  branch_value = R(NBA97_MATCH_INITIALIZE_V0);
  TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u,
                   UINT32_C(0x80068190), &address));
  TRY(write_value(run, address, 4u, UINT32_C(0x80068190),
                  R(NBA97_MATCH_INITIALIZE_RA)));
  TRY(decide_zero(run, branch_value, UINT32_C(0x8006818c), &branch));
  if (branch)
    goto clear_block;

  /* 0x80068194..0x800681B8: actor block clears the crossing state, while a
   * negative owner takes the distinct uncleared epilogue. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(read_value(run, ACTOR_BLOCK, 2u, UINT32_C(0x80068198), &raw));
  R(NBA97_MATCH_INITIALIZE_V0) = load_lh(raw);
  TRY(decide_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800681a0),
                  &branch));
  if (!branch)
    goto clear_block;
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(read_value(run, OWNER, 2u, UINT32_C(0x800681ac), &raw));
  R(NBA97_MATCH_INITIALIZE_V0) = load_lh(raw);
  TRY(decide_nonnegative(run, R(NBA97_MATCH_INITIALIZE_V0),
                         UINT32_C(0x800681b4), &branch));
  if (!branch)
    goto epilogue;

  /* 0x800681BC..0x800681E0: latch both pointers and compare the signs of
   * their fullword fields. Native aliases retain the exact load order. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(read_value(run, FIRST_ACTOR_POINTER, 4u, UINT32_C(0x800681c0),
                 &R(NBA97_MATCH_INITIALIZE_V0)));
  set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80100000));
  TRY(read_value(run, SECOND_ACTOR_POINTER, 4u, UINT32_C(0x800681c8),
                 &R(NBA97_MATCH_INITIALIZE_A0)));
  TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_V0), 0x10u,
                   UINT32_C(0x800681cc), &address));
  TRY(read_value(run, address, 4u, UINT32_C(0x800681cc),
                 &R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 8u, UINT32_C(0x800681d0),
                   &address));
  TRY(read_value(run, address, 4u, UINT32_C(0x800681d0),
                 &R(NBA97_MATCH_INITIALIZE_V1)));
  R(NBA97_MATCH_INITIALIZE_V0) =
      xor_words(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
  TRY(decide_nonnegative(run, R(NBA97_MATCH_INITIALIZE_V0),
                         UINT32_C(0x800681dc), &branch));
  if (branch)
    goto arm_crossing;

  /* 0x800681E4..0x800681FC: a disabled rule clears the crossing state; the
   * enabled path jumps to timer processing without reloading actor fields. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
  TRY(read_value(run, ENABLE, 1u, UINT32_C(0x800681e8), &raw));
  R(NBA97_MATCH_INITIALIZE_V0) = load_lbu(raw);
  TRY(decide_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800681f0),
                  &branch));
  if (branch)
    goto clear_block;
  goto accumulate_timer;

arm_crossing:
  /* 0x80068200..0x80068234: BNE tests the old blocker, but its delay slot
   * always makes v0=1. A new arm publishes 1, 0, and 0x7FFF in order. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(read_value(run, CROSSING_BLOCK, 2u, UINT32_C(0x80068204), &raw));
  R(NBA97_MATCH_INITIALIZE_V0) = load_lh(raw);
  branch_value = R(NBA97_MATCH_INITIALIZE_V0);
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1u);
  TRY(decide_zero(run, branch_value, UINT32_C(0x8006820c), &branch));
  if (!branch)
    goto epilogue;
  set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  TRY(write_value(run, CROSSING_BLOCK, 2u, UINT32_C(0x80068218),
                  R(NBA97_MATCH_INITIALIZE_V0)));
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0x7fffu);
  set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  TRY(write_value(run, TIMER, 2u, UINT32_C(0x80068224), zero));
  set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  TRY(write_value(run, ARM_VALUE, 2u, UINT32_C(0x8006822c),
                  R(NBA97_MATCH_INITIALIZE_V0)));
  out->armed = 1u;
  goto epilogue;

accumulate_timer:
  /* 0x80068238..0x8006827C: the wrapped ADDU low half is stored before the
   * SLL/SRA signed-low-half comparison against 13. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(read_value(run, CROSSING_BLOCK, 2u, UINT32_C(0x8006823c), &raw));
  R(NBA97_MATCH_INITIALIZE_V0) = load_lh(raw);
  TRY(decide_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80068244),
                  &branch));
  if (branch)
    goto epilogue;
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(read_value(run, TIMER, 2u, UINT32_C(0x80068250), &raw));
  R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(raw);
  set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
  TRY(read_value(run, DELTA, 2u, UINT32_C(0x80068258), &raw));
  R(NBA97_MATCH_INITIALIZE_V1) = load_lhu(raw);
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_words(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
  set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  TRY(write_value(run, TIMER, 2u, UINT32_C(0x80068268),
                  R(NBA97_MATCH_INITIALIZE_V0)));
  out->timer_accumulated = 1u;
  R(NBA97_MATCH_INITIALIZE_V0) = load_lh(R(NBA97_MATCH_INITIALIZE_V0));
  R(NBA97_MATCH_INITIALIZE_V0) =
      signed_less_constant(R(NBA97_MATCH_INITIALIZE_V0), 13u);
  TRY(decide_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80068278),
                  &branch));
  if (!branch)
    goto epilogue;

  /* 0x80068280..0x8006829C: the latched second actor pointer gates the
   * service. Its full v0 return is consumed after the NOP delay slot. */
  TRY(address_from(run, R(NBA97_MATCH_INITIALIZE_A0), 0x10u,
                   UINT32_C(0x80068280), &address));
  TRY(read_value(run, address, 4u, UINT32_C(0x80068280),
                 &R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(decide_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80068288),
                  &branch));
  if (!branch)
    goto clear_block;
  TRY(invoke(run, UINT32_C(0x80068290), UINT32_C(0x80062d84),
             NBA97_GAME_CROSS_HALF_RULE_CHILD_80062D84, 0u, 0, 0u));
  TRY(decide_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80068298),
                  &branch));
  if (!branch)
    goto clear_block;

  /* 0x800682A0..0x800682CC: select announcement 11/5000 or 12/20000.
   * The first duration assignment is a J delay slot after its child. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(read_value(run, TEAM, 2u, UINT32_C(0x800682a4), &raw));
  R(NBA97_MATCH_INITIALIZE_V0) = load_lh(raw);
  TRY(decide_zero(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800682ac),
                  &branch));
  if (branch) {
    TRY(invoke(run, UINT32_C(0x800682b4), UINT32_C(0x80029590),
               NBA97_GAME_CROSS_HALF_RULE_CHILD_80029590, 1u, 1, 11u));
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), 5000u);
  } else {
    TRY(invoke(run, UINT32_C(0x800682c4), UINT32_C(0x80029590),
               NBA97_GAME_CROSS_HALF_RULE_CHILD_80029590, 1u, 1, 12u));
    set_known(&R(NBA97_MATCH_INITIALIZE_A0), 20000u);
  }

  /* 0x800682D0..0x800682E4: duration no-op, rule code 5, and final service.
   * The 0x80062300 JAL delay overwrites a0 before that child observes it. */
  TRY(invoke(run, UINT32_C(0x800682d0), UINT32_C(0x800295c8),
             NBA97_GAME_CROSS_HALF_RULE_CHILD_800295C8, 1u, 0, 0u));
  TRY(invoke(run, UINT32_C(0x800682d8), UINT32_C(0x80062300),
             NBA97_GAME_CROSS_HALF_RULE_CHILD_80062300, 1u, 1, 5u));
  TRY(invoke(run, UINT32_C(0x800682e0), UINT32_C(0x80062660),
             NBA97_GAME_CROSS_HALF_RULE_CHILD_80062660, 0u, 0, 0u));
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), 8u);
  set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  TRY(write_value(run, RULE_CODE, 2u, UINT32_C(0x800682f0),
                  R(NBA97_MATCH_INITIALIZE_V0)));
  out->rule_dispatched = 1u;

clear_block:
  /* 0x800682F4..0x800682F8: every clearing exit shares this final store. */
  set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  TRY(write_value(run, CROSSING_BLOCK, 2u, UINT32_C(0x800682f8), zero));
  out->blocker_cleared = 1u;

epilogue:
  /* 0x800682FC..0x80068308: reload ra through callback-live sp, advance sp,
   * then consume the known, aligned JR target after its NOP delay slot. */
  return restore_return_address(run);
}
