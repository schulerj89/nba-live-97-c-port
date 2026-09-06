#include "game_team_strategy_apply.h"

#include <string.h>

typedef struct Run {
  Nba97GameTeamStrategyApplyContext *context;
  Nba97GameTeamStrategyApplyProgress *progress;
  Nba97GameTeamStrategyApplyMachine machine;
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

static void set_known(Nba97GameTeamStrategyApplyWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 15u;
}

static Nba97GameTeamStrategyApplyWord known_word(uint32_t word) {
  Nba97GameTeamStrategyApplyWord value;
  set_known(&value, word);
  return value;
}

static int valid_machine(const Nba97GameTeamStrategyApplyMachine *machine) {
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

static int initialize(Nba97GameTeamStrategyApplyContext *context,
                      Nba97GameTeamStrategyApplyProgress *progress,
                      Run *run) {
  if (progress == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof(*progress));
  if (context == NULL || !valid_machine(&context->machine) ||
      !valid_memory(&context->memory) ||
      (context->access_journal_capacity != 0u &&
       context->access_journal == NULL))
    return NBA97_TEXT_ARGUMENT;
  run->context = context;
  run->progress = progress;
  run->machine = context->machine;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static Nba97GameTeamStrategyApplyWord
add_words(Nba97GameTeamStrategyApplyWord left,
          Nba97GameTeamStrategyApplyWord right) {
  Nba97GameTeamStrategyApplyWord result;
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

static Nba97GameTeamStrategyApplyWord
add_constant(Nba97GameTeamStrategyApplyWord value, uint32_t constant) {
  return add_words(value, known_word(constant));
}

static Nba97GameTeamStrategyApplyWord
shift_left(Nba97GameTeamStrategyApplyWord value, unsigned shift) {
  Nba97GameTeamStrategyApplyWord result;
  unsigned byte, bit;
  result.word = value.word << shift;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned byte_known = 1u;
    for (bit = byte * 8u; bit != byte * 8u + 8u; ++bit)
      if (bit >= shift &&
          (value.known_mask & (1u << ((bit - shift) / 8u))) == 0u)
        byte_known = 0u;
    if (byte_known)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static Nba97GameTeamStrategyApplyWord
zero_extend(Nba97GameTeamStrategyApplyWord value, unsigned width) {
  value.known_mask =
      (uint8_t)(value.known_mask | (15u ^ ((1u << width) - 1u)));
  return value;
}

static Nba97GameTeamStrategyApplyWord
sign_extend_half(Nba97GameTeamStrategyApplyWord value) {
  value.word = (value.word & UINT32_C(0xffff)) |
               ((value.word & UINT32_C(0x8000)) != 0u
                    ? UINT32_C(0xffff0000)
                    : 0u);
  if ((value.known_mask & 2u) != 0u)
    value.known_mask = (uint8_t)(value.known_mask | 12u);
  return value;
}

static int spend(Run *run) {
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width, Nba97GameTeamStrategyApplyWord value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameTeamStrategyApplyAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word;
    event->operation = run->progress->operations;
    event->width = (uint8_t)width;
    event->known_mask =
        (uint8_t)(value.known_mask & ((1u << width) - 1u));
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

static int address(Run *run, Nba97GameTeamStrategyApplyWord base,
                   uint32_t offset, uint32_t pc, uint32_t *result) {
  Nba97GameTeamStrategyApplyWord effective = add_constant(base, offset);
  if (effective.known_mask != 15u) {
    stop(run, pc, effective.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = effective.word;
  return NBA97_TEXT_COMPLETE;
}

static int load(Run *run, unsigned destination, unsigned base,
                uint32_t offset, unsigned width, uint32_t pc, int signed_half) {
  Nba97GameTeamStrategyApplyWord value = {0u, 0u};
  uint32_t guest_address;
  uint8_t *data, *known;
  unsigned byte;
  TRY(address(run, R(base), offset, pc, &guest_address));
  TRY(locate(run, pc, guest_address, width, &data, &known));
  for (byte = 0u; byte != width; ++byte) {
    value.word |= (uint32_t)data[byte] << (8u * byte);
    if (known == NULL || known[byte] != 0u)
      value.known_mask = (uint8_t)(value.known_mask | (1u << byte));
  }
  if (signed_half)
    value = sign_extend_half(value);
  else
    value = zero_extend(value, width);
  R(destination) = value;
  ++run->progress->reads;
  journal(run, NBA97_GAME_MATCH_STATE_RESET_READ, pc, guest_address, width,
          value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int store(Run *run, unsigned source, unsigned base, uint32_t offset,
                 unsigned width, uint32_t pc) {
  uint32_t guest_address;
  uint8_t *data, *known;
  uint8_t required = (uint8_t)((1u << width) - 1u);
  unsigned byte;
  TRY(address(run, R(base), offset, pc, &guest_address));
  TRY(locate(run, pc, guest_address, width, &data, &known));
  if (known == NULL && (R(source).known_mask & required) != required)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0u; byte != width; ++byte) {
    data[byte] = (uint8_t)(R(source).word >> (8u * byte));
    if (known != NULL)
      known[byte] = (uint8_t)((R(source).known_mask >> byte) & 1u);
  }
  ++run->progress->stores;
  journal(run, NBA97_GAME_MATCH_STATE_RESET_STORE, pc, guest_address, width,
          R(source));
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int branch_equal(Run *run, Nba97GameTeamStrategyApplyWord left,
                        Nba97GameTeamStrategyApplyWord right, uint32_t pc,
                        int *equal) {
  unsigned byte;
  for (byte = 0u; byte != 4u; ++byte)
    if ((left.known_mask & right.known_mask & (1u << byte)) != 0u &&
        (((left.word ^ right.word) >> (8u * byte)) & 255u) != 0u) {
      *equal = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (left.known_mask == 15u && right.known_mask == 15u) {
    *equal = left.word == right.word;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0u, 0u);
  return NBA97_TEXT_UNKNOWN;
}

static Nba97GameTeamStrategyApplyWord
predicate_nonzero(Nba97GameTeamStrategyApplyWord value) {
  Nba97GameTeamStrategyApplyWord result;
  unsigned byte;
  result.word = value.word != 0u;
  result.known_mask = 14u;
  for (byte = 0u; byte != 4u; ++byte)
    if ((value.known_mask & (1u << byte)) != 0u &&
        ((value.word >> (8u * byte)) & 255u) != 0u) {
      result.known_mask = 15u;
      return result;
    }
  if (value.known_mask == 15u)
    result.known_mask = 15u;
  return result;
}

static Nba97GameTeamStrategyApplyWord
predicate_unsigned_less(Nba97GameTeamStrategyApplyWord value,
                        uint32_t constant) {
  Nba97GameTeamStrategyApplyWord result;
  uint32_t low = value.word;
  uint32_t high = value.word;
  unsigned byte;
  for (byte = 0u; byte != 4u; ++byte)
    if ((value.known_mask & (1u << byte)) == 0u) {
      low &= ~(UINT32_C(0xff) << (8u * byte));
      high |= UINT32_C(0xff) << (8u * byte);
    }
  result.word = value.word < constant;
  result.known_mask =
      (uint8_t)((high < constant || low >= constant) ? 15u : 14u);
  return result;
}

static Nba97GameTeamStrategyApplyWord
predicate_signed_less(Nba97GameTeamStrategyApplyWord value, int32_t constant) {
  Nba97GameTeamStrategyApplyWord biased = value;
  uint32_t low, high;
  unsigned byte;
  biased.word ^= UINT32_C(0x80000000);
  low = biased.word;
  high = biased.word;
  for (byte = 0u; byte != 4u; ++byte)
    if ((biased.known_mask & (1u << byte)) == 0u) {
      low &= ~(UINT32_C(0xff) << (8u * byte));
      high |= UINT32_C(0xff) << (8u * byte);
    }
  biased.word =
      (value.word ^ UINT32_C(0x80000000)) <
      ((uint32_t)constant ^ UINT32_C(0x80000000));
  biased.known_mask =
      (uint8_t)((high < ((uint32_t)constant ^ UINT32_C(0x80000000)) ||
                 low >= ((uint32_t)constant ^ UINT32_C(0x80000000)))
                    ? 15u
                    : 14u);
  return biased;
}

static int branch_nonzero(Run *run, Nba97GameTeamStrategyApplyWord value,
                          uint32_t pc, int *nonzero) {
  Nba97GameTeamStrategyApplyWord zero = known_word(0u);
  int equal;
  TRY(branch_equal(run, value, zero, pc, &equal));
  *nonzero = !equal;
  return NBA97_TEXT_COMPLETE;
}

static int signed_nonnegative(Run *run,
                              Nba97GameTeamStrategyApplyWord value,
                              uint32_t pc, int *nonnegative) {
  if ((value.known_mask & 8u) == 0u) {
    stop(run, pc, 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *nonnegative = (value.word & UINT32_C(0x80000000)) == 0u;
  return NBA97_TEXT_COMPLETE;
}

static int invoke(Run *run, uint32_t pc, uint32_t entry, uint8_t kind,
                  uint8_t argument_count) {
  Nba97GameTeamStrategyApplyEvent event;
  int accepted;
  stop(run, pc, 0u, entry);
  TRY(spend(run));
  ++run->progress->call_attempts[kind];
  memset(&event, 0, sizeof(event));
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = entry;
  event.operation = run->progress->operations;
  event.invocation = run->progress->call_attempts[kind];
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

static void jal(Run *run, uint32_t pc) {
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
  publish(run);
}

static Nba97GameTeamStrategyApplyWord multiply_34(
    Nba97GameTeamStrategyApplyWord value) {
  Nba97GameTeamStrategyApplyWord times_16 = shift_left(value, 4u);
  return shift_left(add_words(times_16, value), 1u);
}

int nba97_game_team_strategy_apply(
    Nba97GameTeamStrategyApplyContext *context,
    Nba97GameTeamStrategyApplyProgress *progress) {
  Run storage;
  Run *run = &storage;
  Nba97GameTeamStrategyApplyWord zero = known_word(0u);
  Nba97GameTeamStrategyApplyWord side;
  int branch;

  TRY(initialize(context, progress, run));

  /* 0x80065820..0x80065830: create the live frame in source order, retaining
   * the caller's s0 before assigning the team pointer and saving ra. */
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe8));
  progress->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  TRY(store(run, NBA97_MATCH_INITIALIZE_S0, NBA97_MATCH_INITIALIZE_SP, 0x10u,
            4u, UINT32_C(0x80065824)));
  R(NBA97_MATCH_INITIALIZE_S0) = R(NBA97_MATCH_INITIALIZE_A0);
  TRY(store(run, NBA97_MATCH_INITIALIZE_RA, NBA97_MATCH_INITIALIZE_SP, 0x14u,
            4u, UINT32_C(0x8006582c)));
  TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_S0, 0x14u,
           2u, UINT32_C(0x80065830), 0));
  side = R(NBA97_MATCH_INITIALIZE_V0);

  /* 0x80065834..0x800658F4: select launch, CPU, or human strategy setup.
   * The mode branch delay always publishes a0=(side != 0). */
  set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80020000));
  TRY(load(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_V1,
           UINT32_C(0xffffedec), 2u, UINT32_C(0x80065838), 0));
  R(NBA97_MATCH_INITIALIZE_A0) = predicate_nonzero(side);
  TRY(branch_equal(run, R(NBA97_MATCH_INITIALIZE_V1), zero,
                   UINT32_C(0x80065840), &branch));
  if (!branch) {
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1u);
    TRY(store(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_S0, 0x76u,
              1u, UINT32_C(0x80065850)));
  } else {
    Nba97GameTeamStrategyApplyWord count;
    int count_zero;
    TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_S0, 0x42u,
             2u, UINT32_C(0x80065854), 0));
    count = R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1u);
    TRY(branch_equal(run, count, zero, UINT32_C(0x8006585c), &count_zero));
    if (count_zero) {
        TRY(store(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_S0,
                  0x76u, 1u, UINT32_C(0x800658f0)));
        TRY(store(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_S0,
                  0x77u, 1u, UINT32_C(0x800658f4)));
    } else {
        static const uint32_t source_offsets[7] = {
            UINT32_C(0x1dea), UINT32_C(0x1de8), UINT32_C(0x1de6),
            UINT32_C(0x1dec), UINT32_C(0x1dee), UINT32_C(0x1df0),
            UINT32_C(0x1df2)};
        static const uint32_t destination_offsets[7] = {
            0x78u, 0x77u, 0x76u, 0x38u, 0x39u, 0x36u, 0x37u};
        static const uint32_t load_pcs[7] = {
            UINT32_C(0x8006586c), UINT32_C(0x80065880),
            UINT32_C(0x80065894), UINT32_C(0x800658a8),
            UINT32_C(0x800658bc), UINT32_C(0x800658d0),
            UINT32_C(0x800658e4)};
        static const uint32_t store_pcs[7] = {
            UINT32_C(0x80065874), UINT32_C(0x80065888),
            UINT32_C(0x8006589c), UINT32_C(0x800658b0),
            UINT32_C(0x800658c4), UINT32_C(0x800658d8),
            UINT32_C(0x800658ec)};
        unsigned index;
        for (index = 0u; index != 7u; ++index) {
          set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
          R(NBA97_MATCH_INITIALIZE_AT) =
              add_words(R(NBA97_MATCH_INITIALIZE_AT),
                        R(NBA97_MATCH_INITIALIZE_A0));
          TRY(load(run, NBA97_MATCH_INITIALIZE_V0,
                   NBA97_MATCH_INITIALIZE_AT, source_offsets[index], 1u,
                   load_pcs[index], 0));
          TRY(store(run, NBA97_MATCH_INITIALIZE_V0,
                    NBA97_MATCH_INITIALIZE_S0, destination_offsets[index], 1u,
                    store_pcs[index]));
        }
    }
  }

  /* 0x800658F8..0x80065950: reread side and select the away/home injury byte.
   * The branch delay sets a2=12 before the home path clears it. */
  TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_S0, 0x14u,
           2u, UINT32_C(0x800658f8), 0));
  set_known(&R(NBA97_MATCH_INITIALIZE_A2), 12u);
  TRY(branch_equal(run, R(NBA97_MATCH_INITIALIZE_V0), zero,
                   UINT32_C(0x80065900), &branch));
  set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80020000));
  if (!branch) {
    TRY(load(run, NBA97_MATCH_INITIALIZE_A1, NBA97_MATCH_INITIALIZE_A1,
             UINT32_C(0x1ed6), 1u, UINT32_C(0x8006590c), 0));
    R(NBA97_MATCH_INITIALIZE_V0) =
        predicate_unsigned_less(R(NBA97_MATCH_INITIALIZE_A1), 12u);
  } else {
    TRY(load(run, NBA97_MATCH_INITIALIZE_A1, NBA97_MATCH_INITIALIZE_A1,
             UINT32_C(0x1ed5), 1u, UINT32_C(0x80065940), 0));
    set_known(&R(NBA97_MATCH_INITIALIZE_A2), 0u);
    R(NBA97_MATCH_INITIALIZE_V0) =
        predicate_unsigned_less(R(NBA97_MATCH_INITIALIZE_A1), 12u);
  }

  {
    Nba97GameTeamStrategyApplyWord below_twelve =
        R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) =
        predicate_signed_less(R(NBA97_MATCH_INITIALIZE_A1), 5);
    Nba97GameTeamStrategyApplyWord below_five =
        R(NBA97_MATCH_INITIALIZE_V0);
    int injury_valid;
    TRY(branch_nonzero(run, below_twelve, UINT32_C(0x8006594c),
                       &injury_valid));
    if (injury_valid) {
      Nba97GameTeamStrategyApplyWord injury_slot =
          add_words(R(NBA97_MATCH_INITIALIZE_A1),
                    R(NBA97_MATCH_INITIALIZE_A2));
      int direct;
      R(NBA97_MATCH_INITIALIZE_V0) = injury_slot;
      TRY(branch_nonzero(run, below_five, UINT32_C(0x80065954), &direct));
      if (direct) {
        R(NBA97_MATCH_INITIALIZE_A2) = multiply_34(injury_slot);
        set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
        R(NBA97_MATCH_INITIALIZE_V0) =
            add_constant(R(NBA97_MATCH_INITIALIZE_V0),
                         UINT32_C(0xfffff7ec));
        R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
        R(NBA97_MATCH_INITIALIZE_A2) =
            add_words(R(NBA97_MATCH_INITIALIZE_A2),
                      R(NBA97_MATCH_INITIALIZE_V0));
        jal(run, UINT32_C(0x800659c4));
        set_known(&R(NBA97_MATCH_INITIALIZE_A3), 0u);
        TRY(invoke(run, UINT32_C(0x800659c4), UINT32_C(0x80064dbc),
                   NBA97_GAME_TEAM_STRATEGY_APPLY_80064DBC, 4u));
      } else {
        int call_ready = 0;
        set_known(&R(NBA97_MATCH_INITIALIZE_A0), 11u);
        while (!call_ready) {
          Nba97GameTeamStrategyApplyWord candidate_slot;
          int equal;
          candidate_slot = add_words(R(NBA97_MATCH_INITIALIZE_A0),
                                     R(NBA97_MATCH_INITIALIZE_A2));
          R(NBA97_MATCH_INITIALIZE_V0) = candidate_slot;
          TRY(branch_equal(run, R(NBA97_MATCH_INITIALIZE_A0),
                           R(NBA97_MATCH_INITIALIZE_A1),
                           UINT32_C(0x80065960), &equal));
          if (equal) {
            call_ready = 1;
          } else {
            int available;
            R(NBA97_MATCH_INITIALIZE_V1) = multiply_34(candidate_slot);
            set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
            R(NBA97_MATCH_INITIALIZE_AT) =
                add_words(R(NBA97_MATCH_INITIALIZE_AT),
                          R(NBA97_MATCH_INITIALIZE_V1));
            TRY(load(run, NBA97_MATCH_INITIALIZE_V0,
                     NBA97_MATCH_INITIALIZE_AT, UINT32_C(0xfffff80c), 2u,
                     UINT32_C(0x8006597c), 1));
            TRY(signed_nonnegative(run, R(NBA97_MATCH_INITIALIZE_V0),
                                   UINT32_C(0x80065984), &available));
            if (available) {
              R(NBA97_MATCH_INITIALIZE_V1) =
                  add_words(shift_left(R(NBA97_MATCH_INITIALIZE_A0), 1u),
                            R(NBA97_MATCH_INITIALIZE_S0));
              R(NBA97_MATCH_INITIALIZE_V0) =
                  add_words(shift_left(R(NBA97_MATCH_INITIALIZE_A1), 1u),
                            R(NBA97_MATCH_INITIALIZE_S0));
              TRY(load(run, NBA97_MATCH_INITIALIZE_A0,
                       NBA97_MATCH_INITIALIZE_V0, 0x16u, 2u,
                       UINT32_C(0x80065928), 1));
              TRY(load(run, NBA97_MATCH_INITIALIZE_A1,
                       NBA97_MATCH_INITIALIZE_V1, 0x16u, 2u,
                       UINT32_C(0x8006592c), 1));
              TRY(store(run, NBA97_MATCH_INITIALIZE_A0,
                        NBA97_MATCH_INITIALIZE_V1, 0x16u, 2u,
                        UINT32_C(0x80065930)));
              TRY(store(run, NBA97_MATCH_INITIALIZE_A1,
                        NBA97_MATCH_INITIALIZE_V0, 0x16u, 2u,
                        UINT32_C(0x80065938)));
              call_ready = 1;
            } else {
              R(NBA97_MATCH_INITIALIZE_A0) =
                  add_constant(R(NBA97_MATCH_INITIALIZE_A0), UINT32_MAX);
              TRY(signed_nonnegative(run, R(NBA97_MATCH_INITIALIZE_A0),
                                     UINT32_C(0x80065990), &available));
              if (!available)
                call_ready = 1;
            }
          }
        }
        jal(run, UINT32_C(0x80065998));
        TRY(invoke(run, UINT32_C(0x80065998), UINT32_C(0x800646a8),
                   NBA97_GAME_TEAM_STRATEGY_APPLY_800646A8, 0u));
      }

      /* 0x800659CC..0x800659D8: use callback-live s0 and preserve unsigned
       * halfword underflow when decrementing the team count. */
      TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_S0,
               0x66u, 2u, UINT32_C(0x800659cc), 0));
      R(NBA97_MATCH_INITIALIZE_V0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_MAX);
      TRY(store(run, NBA97_MATCH_INITIALIZE_V0,
                NBA97_MATCH_INITIALIZE_S0, 0x66u, 2u,
                UINT32_C(0x800659d8)));
    }
  }

  /* 0x800659DC..0x800659EC: restore through callback-live sp, then validate
   * the reloaded ra only after s0 and the wrapping frame release. */
  TRY(load(run, NBA97_MATCH_INITIALIZE_RA, NBA97_MATCH_INITIALIZE_SP, 0x14u,
           4u, UINT32_C(0x800659dc), 0));
  progress->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  TRY(load(run, NBA97_MATCH_INITIALIZE_S0, NBA97_MATCH_INITIALIZE_SP, 0x10u,
           4u, UINT32_C(0x800659e0), 0));
  progress->restored_s0 = R(NBA97_MATCH_INITIALIZE_S0);
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x18u);
  progress->return_v0 = R(NBA97_MATCH_INITIALIZE_V0);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15u) {
    stop(run, UINT32_C(0x800659e8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if ((R(NBA97_MATCH_INITIALIZE_RA).word & 3u) != 0u) {
    stop(run, UINT32_C(0x800659e8), R(NBA97_MATCH_INITIALIZE_RA).word, 0u);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  progress->completed = 1u;
  stop(run, 0u, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
