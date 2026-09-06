#include "game_actor_resume.h"

#include <string.h>

#define PHASE UINT32_C(0x800fdb90)
#define PHASE_82_TEAM UINT32_C(0x800fe880)
#define ACTIVE_TEAM UINT32_C(0x800fdb94)

typedef struct Run {
  Nba97GameActorResumeContext *context;
  Nba97GameActorResumeProgress *out;
  Nba97GameActorResumeMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int nba97_result_ = (expression);                                          \
    if (nba97_result_ != NBA97_TEXT_COMPLETE)                                  \
      return nba97_result_;                                                    \
  } while (0)

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t entry) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  run->out->stopped_entry = entry;
  publish(run);
}

static void set_known(Nba97GameActorResumeWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameActorResumeMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int validate(Nba97GameActorResumeContext *context,
                    Nba97GameActorResumeProgress *out, Run *run) {
  size_t i;
  size_t j;
  if (!out)
    return NBA97_TEXT_ARGUMENT;
  memset(out, 0, sizeof *out);
  if (!context || (!context->memory.region && context->memory.count) ||
      (!context->access_journal && context->access_journal_capacity) ||
      !machine_valid(&context->machine))
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < context->memory.count; ++i) {
    const Nba97GameTextRegion *a = &context->memory.region[i];
    if (!a->data || !a->size || a->size > UINT64_C(0x100000000) ||
        (uint64_t)a->base + a->size > UINT64_C(0x100000000))
      return NBA97_TEXT_ARGUMENT;
    for (j = 0; j < i; ++j) {
      const Nba97GameTextRegion *b = &context->memory.region[j];
      if ((uint64_t)a->base < (uint64_t)b->base + b->size &&
          (uint64_t)b->base < (uint64_t)a->base + a->size)
        return NBA97_TEXT_ARGUMENT;
    }
  }
  run->context = context;
  run->out = out;
  run->machine = context->machine;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

/* Enumerating possible byte carries retains each output byte only when every
 * concrete address represented by the input knownness produces that byte. */
static Nba97GameActorResumeWord add_words(Nba97GameActorResumeWord left,
                                          Nba97GameActorResumeWord right) {
  Nba97GameActorResumeWord result;
  unsigned carry_mask = 1u;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0;
  if (left.known_mask == 0x0fu && right.known_mask == 0x0fu) {
    result.known_mask = 0x0fu;
    return result;
  }
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_carry_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned ls = (left.known_mask & (1u << byte))
                      ? ((left.word >> (byte * 8u)) & 0xffu)
                      : 0u;
    unsigned le = (left.known_mask & (1u << byte)) ? ls : 255u;
    unsigned rs = (right.known_mask & (1u << byte))
                      ? ((right.word >> (byte * 8u)) & 0xffu)
                      : 0u;
    unsigned re = (right.known_mask & (1u << byte)) ? rs : 255u;
    unsigned carry;
    for (carry = 0; carry <= 1; ++carry) {
      unsigned a;
      if (!(carry_mask & (1u << carry)))
        continue;
      for (a = ls; a <= le; ++a) {
        unsigned b;
        for (b = rs; b <= re; ++b) {
          unsigned sum = a + b + carry;
          unsigned output = sum & 0xffu;
          next_carry_mask |= 1u << (sum >> 8u);
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
    carry_mask = next_carry_mask;
  }
  return result;
}

static Nba97GameActorResumeWord add_constant(Nba97GameActorResumeWord source,
                                             uint32_t constant) {
  Nba97GameActorResumeWord value;
  set_known(&value, constant);
  return add_words(source, value);
}

static uint32_t width_mask(unsigned width) {
  return width == 4 ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - 1u;
}

static uint8_t knowledge_mask(unsigned width) {
  return (uint8_t)((1u << width) - 1u);
}

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    uint8_t width, const Nba97GameActorResumeWord *value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameActorResumeAccess *event = &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word & width_mask(width);
    event->operation = run->out->operations;
    event->width = width;
    event->known_mask = (uint8_t)(value->known_mask & knowledge_mask(width));
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t address, size_t width, size_t alignment,
                  uint32_t pc, uint8_t **data, uint8_t **known) {
  size_t i;
  size_t j;
  stop(run, pc, address, 0);
  TRY(spend(run));
  ++run->out->accesses;
  if (address & (uint32_t)(alignment - 1u))
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0; i < run->context->memory.count; ++i) {
    Nba97GameTextRegion *region = &run->context->memory.region[i];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        width > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known = region->known ? region->known + (size_t)offset : 0;
    if (*known)
      for (j = 0; j < width; ++j)
        if ((*known)[j] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int read_value(Run *run, uint32_t address, uint8_t width, uint32_t pc,
                      Nba97GameActorResumeWord *value) {
  Nba97GameActorResumeWord loaded = {0, 0};
  uint8_t *data;
  uint8_t *known;
  unsigned i;
  TRY(locate(run, address, width, width, pc, &data, &known));
  for (i = 0; i < width; ++i) {
    loaded.word |= (uint32_t)data[i] << (i * 8u);
    if (!known || known[i])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
  }
  *value = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, uint32_t address, uint8_t width, uint32_t pc,
                       const Nba97GameActorResumeWord *value) {
  Nba97GameActorResumeWord stored = *value;
  uint8_t *data;
  uint8_t *known;
  unsigned i;
  stored.word &= width_mask(width);
  stored.known_mask = (uint8_t)(stored.known_mask & knowledge_mask(width));
  TRY(locate(run, address, width, width, pc, &data, &known));
  if (!known && stored.known_mask != knowledge_mask(width))
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < width; ++i) {
    data[i] = (uint8_t)(stored.word >> (i * 8u));
    if (known)
      known[i] = (uint8_t)((stored.known_mask >> i) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, width, &stored);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int register_address(Run *run, Nba97GameActorResumeWord base,
                            uint32_t offset, uint32_t pc, uint32_t *address) {
  Nba97GameActorResumeWord value = add_constant(base, offset);
  if (value.known_mask != 0x0fu) {
    stop(run, pc, value.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = value.word;
  return NBA97_TEXT_COMPLETE;
}

static int read_register_offset(Run *run, unsigned base, uint32_t offset,
                                uint8_t width, uint32_t pc,
                                Nba97GameActorResumeWord *value) {
  uint32_t address;
  TRY(register_address(run, R(base), offset, pc, &address));
  return read_value(run, address, width, pc, value);
}

static int write_register_offset(Run *run, unsigned base, uint32_t offset,
                                 uint8_t width, uint32_t pc,
                                 const Nba97GameActorResumeWord *value) {
  uint32_t address;
  TRY(register_address(run, R(base), offset, pc, &address));
  return write_value(run, address, width, pc, value);
}

static Nba97GameActorResumeWord load_lh(Nba97GameActorResumeWord raw) {
  Nba97GameActorResumeWord result;
  uint32_t value = raw.word & 0xffffu;
  result.word = (value & 0x8000u) ? value | UINT32_C(0xffff0000) : value;
  result.known_mask = (uint8_t)(raw.known_mask & 3u);
  if (raw.known_mask & 2u)
    result.known_mask = (uint8_t)(result.known_mask | 0x0cu);
  return result;
}

static Nba97GameActorResumeWord load_lhu(Nba97GameActorResumeWord raw) {
  Nba97GameActorResumeWord result;
  result.word = raw.word & 0xffffu;
  result.known_mask = (uint8_t)((raw.known_mask & 3u) | 0x0cu);
  return result;
}

static Nba97GameActorResumeWord load_lbu(Nba97GameActorResumeWord raw) {
  Nba97GameActorResumeWord result;
  result.word = raw.word & 0xffu;
  result.known_mask = (uint8_t)((raw.known_mask & 1u) | 0x0eu);
  return result;
}

static int decide_zero(Run *run, const Nba97GameActorResumeWord *value,
                       uint32_t pc, int *is_zero) {
  unsigned i;
  for (i = 0; i < 4; ++i)
    if ((value->known_mask & (1u << i)) &&
        ((value->word >> (i * 8u)) & 0xffu)) {
      *is_zero = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (value->known_mask == 0x0fu) {
    *is_zero = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

static int decide_equal(Run *run, const Nba97GameActorResumeWord *left,
                        const Nba97GameActorResumeWord *right, uint32_t pc,
                        int *equal) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    uint8_t bit = (uint8_t)(1u << i);
    if ((left->known_mask & right->known_mask & bit) &&
        ((left->word >> (i * 8u)) & 0xffu) !=
            ((right->word >> (i * 8u)) & 0xffu)) {
      *equal = 0;
      return NBA97_TEXT_COMPLETE;
    }
  }
  if (left->known_mask == 0x0fu && right->known_mask == 0x0fu) {
    *equal = left->word == right->word;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

static Nba97GameActorResumeWord
unsigned_less_constant(const Nba97GameActorResumeWord *value,
                       uint32_t constant) {
  Nba97GameActorResumeWord result;
  uint32_t minimum = 0;
  uint32_t maximum = 0;
  unsigned i;
  for (i = 0; i < 4; ++i) {
    uint32_t byte = (value->word >> (i * 8u)) & 0xffu;
    minimum |= ((value->known_mask & (1u << i)) ? byte : 0u) << (i * 8u);
    maximum |= ((value->known_mask & (1u << i)) ? byte : 0xffu) << (i * 8u);
  }
  result.word = value->word < constant;
  result.known_mask = 0x0eu;
  if (maximum < constant)
    set_known(&result, 1);
  else if (minimum >= constant)
    set_known(&result, 0);
  return result;
}

static Nba97GameActorResumeWord and_constant(Nba97GameActorResumeWord value,
                                             uint32_t mask) {
  Nba97GameActorResumeWord result;
  unsigned i;
  result.word = value.word & mask;
  result.known_mask = 0;
  for (i = 0; i < 4; ++i) {
    uint32_t byte_mask = (mask >> (i * 8u)) & 0xffu;
    if (!byte_mask || (value.known_mask & (1u << i)))
      result.known_mask = (uint8_t)(result.known_mask | (1u << i));
  }
  return result;
}

static int invoke(Run *run, uint32_t pc, uint32_t entry, uint8_t kind,
                  uint8_t argument_count, int move_current_s0_to_a0) {
  Nba97GameActorResumeEvent event;
  int accepted;
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
  if (move_current_s0_to_a0)
    R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
  stop(run, pc, 0, entry);
  TRY(spend(run));
  memset(&event, 0, sizeof event);
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = entry;
  event.operation = run->out->operations;
  event.invocation = run->out->call_count[kind] + 1u;
  event.kind = kind;
  event.argument_count = argument_count;
  publish(run);
  if (!run->context->io)
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

static int restore(Run *run, uint32_t pc, uint32_t offset, unsigned reg,
                   Nba97GameActorResumeWord *reported) {
  TRY(read_register_offset(run, NBA97_MATCH_INITIALIZE_SP, offset, 4, pc,
                           &R(reg)));
  *reported = R(reg);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_actor_resume(Nba97GameActorResumeContext *context,
                            Nba97GameActorResumeProgress *out) {
  Run storage;
  Run *run = &storage;
  Nba97GameActorResumeWord value;
  Nba97GameActorResumeWord left;
  Nba97GameActorResumeWord right;
  Nba97GameActorResumeWord zero;
  int branch;
  int first_below;
  int second_below;
  TRY(validate(context, out, run));
  set_known(&zero, 0);

  /* GAMEONLY 0x800582DC..0x800582F8: phase is read before frame allocation;
   * the BNE delay always saves ra after s0 has captured entry a0. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80100000));
  TRY(read_value(run, PHASE, 2, UINT32_C(0x800582e0), &value));
  R(NBA97_MATCH_INITIALIZE_V1) = load_lh(value);
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe8));
  out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  TRY(write_register_offset(run, NBA97_MATCH_INITIALIZE_SP, 0x10u, 4,
                            UINT32_C(0x800582e8),
                            &R(NBA97_MATCH_INITIALIZE_S0)));
  R(NBA97_MATCH_INITIALIZE_S0) = R(NBA97_MATCH_INITIALIZE_A0);
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0x82u);
  left = R(NBA97_MATCH_INITIALIZE_V1);
  right = R(NBA97_MATCH_INITIALIZE_V0);
  TRY(write_register_offset(run, NBA97_MATCH_INITIALIZE_SP, 0x14u, 4,
                            UINT32_C(0x800582f8),
                            &R(NBA97_MATCH_INITIALIZE_RA)));
  TRY(decide_equal(run, &left, &right, UINT32_C(0x800582f4), &branch));

  /* 0x800582FC..0x80058338: the actor byte is zero-extended, while both
   * source team selectors are signed halfwords. Both comparisons publish
   * v0=2 in their branch delay slots before the selected state byte store. */
  if (branch) {
    TRY(read_register_offset(run, NBA97_MATCH_INITIALIZE_S0, 0xd9u, 1,
                             UINT32_C(0x800582fc), &value));
    R(NBA97_MATCH_INITIALIZE_V1) = load_lbu(value);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, PHASE_82_TEAM, 2, UINT32_C(0x80058304), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    left = R(NBA97_MATCH_INITIALIZE_V1);
    right = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 2);
    TRY(decide_equal(run, &left, &right, UINT32_C(0x8005830c), &branch));
    if (!branch) {
      TRY(write_register_offset(run, NBA97_MATCH_INITIALIZE_S0, 0x1au, 1,
                                UINT32_C(0x80058318),
                                &R(NBA97_MATCH_INITIALIZE_V0)));
      goto animation_thresholds;
    }
  } else {
    TRY(read_register_offset(run, NBA97_MATCH_INITIALIZE_S0, 0xd9u, 1,
                             UINT32_C(0x8005831c), &value));
    R(NBA97_MATCH_INITIALIZE_V1) = load_lbu(value);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
    TRY(read_value(run, ACTIVE_TEAM, 2, UINT32_C(0x80058324), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lh(value);
    left = R(NBA97_MATCH_INITIALIZE_V1);
    right = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 2);
    TRY(decide_equal(run, &left, &right, UINT32_C(0x8005832c), &branch));
    if (!branch) {
      TRY(write_register_offset(run, NBA97_MATCH_INITIALIZE_S0, 0x1au, 1,
                                UINT32_C(0x80058338),
                                &R(NBA97_MATCH_INITIALIZE_V0)));
      goto animation_thresholds;
    }
  }
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1);
  TRY(write_register_offset(run, NBA97_MATCH_INITIALIZE_S0, 0x1au, 1,
                            UINT32_C(0x80058338),
                            &R(NBA97_MATCH_INITIALIZE_V0)));

animation_thresholds:
  /* 0x8005833C..0x80058380: clearing +0x4E is the first SLTIU branch
   * delay. Either unsigned animation value below 37 forces a1=1; otherwise
   * the caller's live a1 survives. The second JAL delay reloads a0 from
   * callback-mutated live s0. */
  TRY(read_register_offset(run, NBA97_MATCH_INITIALIZE_S0, 0x46u, 2,
                           UINT32_C(0x8005833c), &value));
  R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(value);
  R(NBA97_MATCH_INITIALIZE_V0) =
      unsigned_less_constant(&R(NBA97_MATCH_INITIALIZE_V0), 0x25u);
  left = R(NBA97_MATCH_INITIALIZE_V0);
  TRY(write_register_offset(run, NBA97_MATCH_INITIALIZE_S0, 0x4eu, 2,
                            UINT32_C(0x8005834c), &zero));
  TRY(decide_zero(run, &left, UINT32_C(0x80058348), &branch));
  first_below = !branch;
  second_below = 0;
  if (!first_below) {
    TRY(read_register_offset(run, NBA97_MATCH_INITIALIZE_S0, 0x4au, 2,
                             UINT32_C(0x80058350), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = load_lhu(value);
    R(NBA97_MATCH_INITIALIZE_V0) =
        unsigned_less_constant(&R(NBA97_MATCH_INITIALIZE_V0), 0x25u);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x8005835c),
                    &branch));
    second_below = !branch;
  }
  R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
  if (first_below || second_below)
    set_known(&R(NBA97_MATCH_INITIALIZE_A1), 1);
  TRY(invoke(run, UINT32_C(0x80058374), UINT32_C(0x80056ffc),
             NBA97_GAME_ACTOR_RESUME_CHILD_80056FFC, 2, 0));
  TRY(invoke(run, UINT32_C(0x8005837c), UINT32_C(0x8005703c),
             NBA97_GAME_ACTOR_RESUME_CHILD_8005703C, 1, 1));

  /* 0x80058384..0x800583CC: only the low two bits of +0x60/+0x64 gate
   * the nested pointer. If reached, pointer+0x0D selects a +0x9A halfword
   * of zero or three; otherwise +0x9A is deliberately preserved. */
  TRY(read_register_offset(run, NBA97_MATCH_INITIALIZE_S0, 0x60u, 2,
                           UINT32_C(0x80058384), &value));
  R(NBA97_MATCH_INITIALIZE_V0) = and_constant(load_lhu(value), 3u);
  TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80058390),
                  &branch));
  if (branch) {
    TRY(read_register_offset(run, NBA97_MATCH_INITIALIZE_S0, 0x64u, 2,
                             UINT32_C(0x80058398), &value));
    R(NBA97_MATCH_INITIALIZE_V0) = and_constant(load_lhu(value), 3u);
    TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800583a4),
                    &branch));
    if (branch) {
      TRY(read_register_offset(run, NBA97_MATCH_INITIALIZE_S0, 0x20u, 4,
                               UINT32_C(0x800583ac),
                               &R(NBA97_MATCH_INITIALIZE_V0)));
      TRY(read_register_offset(run, NBA97_MATCH_INITIALIZE_V0, 0x0du, 1,
                               UINT32_C(0x800583b4), &value));
      R(NBA97_MATCH_INITIALIZE_V0) = load_lbu(value);
      left = R(NBA97_MATCH_INITIALIZE_V0);
      set_known(&R(NBA97_MATCH_INITIALIZE_V0), 3);
      TRY(decide_zero(run, &left, UINT32_C(0x800583bc), &branch));
      if (branch)
        TRY(write_register_offset(run, NBA97_MATCH_INITIALIZE_S0, 0x9au, 2,
                                  UINT32_C(0x800583cc), &zero));
      else
        TRY(write_register_offset(run, NBA97_MATCH_INITIALIZE_S0, 0x9au, 2,
                                  UINT32_C(0x800583c8),
                                  &R(NBA97_MATCH_INITIALIZE_V0)));
    }
  }

  /* 0x800583D0..0x800583E4: +0xB8 becomes 47 before the third child, and
   * the copied +0xA2 halfword is stored at +0xA6 in that JAL's delay slot. */
  TRY(read_register_offset(run, NBA97_MATCH_INITIALIZE_S0, 0xa2u, 2,
                           UINT32_C(0x800583d0), &value));
  R(NBA97_MATCH_INITIALIZE_V1) = load_lhu(value);
  R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0x2fu);
  TRY(write_register_offset(run, NBA97_MATCH_INITIALIZE_A0, 0xb8u, 2,
                            UINT32_C(0x800583dc),
                            &R(NBA97_MATCH_INITIALIZE_V0)));
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x800583e8));
  TRY(write_register_offset(run, NBA97_MATCH_INITIALIZE_A0, 0xa6u, 2,
                            UINT32_C(0x800583e4),
                            &R(NBA97_MATCH_INITIALIZE_V1)));
  TRY(invoke(run, UINT32_C(0x800583e0), UINT32_C(0x800582cc),
             NBA97_GAME_ACTOR_RESUME_CHILD_800582CC, 0, 0));

  /* 0x800583E8..0x800583F8: both words reload through third-child mutable
   * live sp. ADDIU completes before JR consumes possibly unknown live ra. */
  TRY(restore(run, UINT32_C(0x800583e8), 0x14u, NBA97_MATCH_INITIALIZE_RA,
              &out->restored_return_address));
  TRY(restore(run, UINT32_C(0x800583ec), 0x10u, NBA97_MATCH_INITIALIZE_S0,
              &out->restored_s0));
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x18u);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x800583f4), R(NBA97_MATCH_INITIALIZE_RA).word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
