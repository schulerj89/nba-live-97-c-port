#include "game_draw_mode_command.h"

#include <string.h>

typedef struct Run {
  Nba97GameDrawModeCommandContext *context;
  Nba97GameDrawModeCommandProgress *progress;
  Nba97GameDrawModeCommandMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int status_ = (expression);                                                \
    if (status_ != NBA97_TEXT_COMPLETE)                                        \
      return status_;                                                          \
  } while (0)

static void publish(Run *run) { run->progress->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address) {
  run->progress->stopped_pc = pc;
  run->progress->stopped_address = address;
  publish(run);
}

static void set_known(Nba97GameDrawModeCommandWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 15u;
}

static int valid_machine(const Nba97GameDrawModeCommandMachine *machine) {
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

static int initialize(Nba97GameDrawModeCommandContext *context,
                      Nba97GameDrawModeCommandProgress *progress, Run *run) {
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

static Nba97GameDrawModeCommandWord
add_words(Nba97GameDrawModeCommandWord left,
          Nba97GameDrawModeCommandWord right) {
  Nba97GameDrawModeCommandWord result;
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

static Nba97GameDrawModeCommandWord
add_constant(Nba97GameDrawModeCommandWord value, uint32_t constant) {
  Nba97GameDrawModeCommandWord immediate;
  set_known(&immediate, constant);
  return add_words(value, immediate);
}

static Nba97GameDrawModeCommandWord
and_constant(Nba97GameDrawModeCommandWord value, uint32_t constant) {
  Nba97GameDrawModeCommandWord result;
  unsigned byte;
  result.word = value.word & constant;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned bit = 1u << byte;
    unsigned constant_byte = (constant >> (8u * byte)) & 255u;
    if ((value.known_mask & bit) != 0u || constant_byte == 0u)
      result.known_mask = (uint8_t)(result.known_mask | bit);
  }
  return result;
}

static Nba97GameDrawModeCommandWord
or_words(Nba97GameDrawModeCommandWord left,
         Nba97GameDrawModeCommandWord right) {
  Nba97GameDrawModeCommandWord result;
  unsigned byte;
  result.word = left.word | right.word;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned bit = 1u << byte;
    unsigned l = (left.word >> (8u * byte)) & 255u;
    unsigned r = (right.word >> (8u * byte)) & 255u;
    if (((left.known_mask & right.known_mask & bit) != 0u) ||
        ((left.known_mask & bit) != 0u && l == 255u) ||
        ((right.known_mask & bit) != 0u && r == 255u))
      result.known_mask = (uint8_t)(result.known_mask | bit);
  }
  return result;
}

static Nba97GameDrawModeCommandWord
or_constant(Nba97GameDrawModeCommandWord value, uint32_t constant) {
  Nba97GameDrawModeCommandWord immediate;
  set_known(&immediate, constant);
  return or_words(value, immediate);
}

static Nba97GameDrawModeCommandWord
unsigned_less_two(Nba97GameDrawModeCommandWord value) {
  Nba97GameDrawModeCommandWord result;
  uint32_t minimum = 0u;
  uint32_t maximum = 0u;
  unsigned byte;
  result.word = value.word < 2u ? 1u : 0u;
  result.known_mask = 14u;
  for (byte = 0u; byte != 4u; ++byte) {
    uint32_t bits = UINT32_C(255) << (8u * byte);
    if ((value.known_mask & (1u << byte)) != 0u) {
      minimum |= value.word & bits;
      maximum |= value.word & bits;
    } else {
      maximum |= bits;
    }
  }
  if (maximum < 2u) {
    result.word = 1u;
    result.known_mask = 15u;
  } else if (minimum >= 2u) {
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

static void journal_read(Run *run, uint32_t pc, uint32_t address,
                         const Nba97GameDrawModeCommandWord *value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameDrawModeCommandAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word;
    event->operation = run->progress->operations;
    event->width = 1u;
    event->known_mask = (uint8_t)(value->known_mask & 1u);
    event->kind = NBA97_GAME_MATCH_CLOCKS_READ;
  }
}

static int read_type(Run *run, Nba97GameDrawModeCommandWord *destination) {
  const uint32_t address = UINT32_C(0x800c55c0);
  size_t index;
  uint8_t *data = NULL;
  uint8_t *known = NULL;
  Nba97GameDrawModeCommandWord loaded;
  stop(run, UINT32_C(0x8009a5f0), address);
  TRY(spend(run));
  ++run->progress->accesses;
  for (index = 0u; index != run->context->memory.count; ++index) {
    Nba97GameTextRegion *region = &run->context->memory.region[index];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset >= region->size)
      continue;
    data = region->data + (size_t)offset;
    known = region->known != NULL ? region->known + (size_t)offset : NULL;
    break;
  }
  if (data == NULL)
    return NBA97_TEXT_RESOURCE;
  if (known != NULL && *known > 1u)
    return NBA97_TEXT_ARGUMENT;
  loaded.word = *data;
  loaded.known_mask =
      (uint8_t)(14u | (known == NULL || *known != 0u ? 1u : 0u));
  *destination = loaded;
  ++run->progress->reads;
  journal_read(run, UINT32_C(0x8009a5f0), address, &loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int branch_zero(Run *run, Nba97GameDrawModeCommandWord value,
                       uint32_t pc, int *is_zero) {
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
  stop(run, pc, 0u);
  return NBA97_TEXT_UNKNOWN;
}

int nba97_game_draw_mode_command(Nba97GameDrawModeCommandContext *context,
                                  Nba97GameDrawModeCommandProgress *progress) {
  Run storage;
  Run *run = &storage;
  int zero;

  TRY(initialize(context, progress, run));

  /* 0x8009A5E8..0x8009A604: read the display type, subtract one, and select
   * the type-1/2 encoding with the original unsigned comparison. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x55c0));
  TRY(read_type(run, &R(NBA97_MATCH_INITIALIZE_V0)));
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xffffffff));
  R(NBA97_MATCH_INITIALIZE_V0) =
      unsigned_less_two(R(NBA97_MATCH_INITIALIZE_V0));
  TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_V0),
                  UINT32_C(0x8009a600), &zero));

  if (!zero) {
    /* 0x8009A608..0x8009A620: types one and two use the 0x27FF payload.
     * Both full-word branch predicates retain their LUI/ANDI delay effects. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xe1000000));
    TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_A1),
                    UINT32_C(0x8009a608), &zero));
    if (!zero)
      R(NBA97_MATCH_INITIALIZE_V1) =
          or_constant(R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x800));
    R(NBA97_MATCH_INITIALIZE_V0) =
        and_constant(R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x27ff));
    TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_A0),
                    UINT32_C(0x8009a614), &zero));
    if (!zero)
      R(NBA97_MATCH_INITIALIZE_V0) =
          or_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x1000));
  } else {
    /* 0x8009A624..0x8009A638: all other type bytes use the narrower 0x09FF
     * payload and the alternate a1/a0 flag positions. */
    set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xe1000000));
    TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_A1),
                    UINT32_C(0x8009a624), &zero));
    if (!zero)
      R(NBA97_MATCH_INITIALIZE_V1) =
          or_constant(R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x200));
    R(NBA97_MATCH_INITIALIZE_V0) =
        and_constant(R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(0x09ff));
    TRY(branch_zero(run, R(NBA97_MATCH_INITIALIZE_A0),
                    UINT32_C(0x8009a630), &zero));
    if (!zero)
      R(NBA97_MATCH_INITIALIZE_V0) =
          or_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x400));
  }

  /* 0x8009A63C..0x8009A640: JR consumes live ra after its delay slot merges
   * the E1 base and payload. Unknown ra therefore retains the final command. */
  R(NBA97_MATCH_INITIALIZE_V0) =
      or_words(R(NBA97_MATCH_INITIALIZE_V1),
               R(NBA97_MATCH_INITIALIZE_V0));
  progress->return_v0 = R(NBA97_MATCH_INITIALIZE_V0);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15u) {
    stop(run, UINT32_C(0x8009a63c), 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  progress->completed = 1u;
  stop(run, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
