#include "game_opponent_contact.h"

#include <string.h>

typedef Nba97GameOpponentContactWord Word;
typedef struct Run {
  Nba97GameOpponentContactContext *context;
  Nba97GameOpponentContactProgress *out;
  Nba97GameOpponentContactMachine machine;
} Run;

#define R(n) (run->machine.registers.gpr[(n)])
#define ZERO R(0)
#define V0 R(2)
#define V1 R(3)
#define A0 R(4)
#define A1 R(5)
#define A2 R(6)
#define SP R(29)
#define RA R(31)
#define TRY(expression)                                                        \
  do {                                                                         \
    int result_ = (expression);                                                \
    if (result_ != NBA97_TEXT_COMPLETE)                                        \
      return result_;                                                          \
  } while (0)
#define STEP(pc)                                                               \
  do {                                                                         \
    (void)(pc);                                                                \
    ++out->instruction_count;                                                  \
  } while (0)

static void set_known(Word *value, uint32_t word) {
  value->word = word;
  value->known_mask = 0x0f;
}

static void publish(Run *run) {
  run->out->machine = run->machine;
  run->out->returned_value = V0;
}

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t entry) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  run->out->stopped_entry = entry;
  publish(run);
}

static int valid_machine(const Nba97GameOpponentContactMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0f ||
      machine->hi.known_mask > 0x0f || machine->lo.known_mask > 0x0f)
    return 0;
  for (i = 0; i < 32; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0f)
      return 0;
  return 1;
}

static int validate(Nba97GameOpponentContactContext *context,
                    Nba97GameOpponentContactProgress *out, Run *run) {
  size_t i;
  size_t j;
  if (!out)
    return NBA97_TEXT_ARGUMENT;
  memset(out, 0, sizeof *out);
  if (!context || (!context->memory.region && context->memory.count) ||
      (!context->access_journal && context->access_journal_capacity) ||
      !valid_machine(&context->machine))
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

/* Enumerating byte carries retains a result byte only when it is invariant
 * over every concrete input represented by the two byte masks. */
static Word add_words(Word left, Word right) {
  Word result;
  unsigned carry_mask = 1u;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_carry_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned left_start = (left.known_mask & (1u << byte))
                              ? ((left.word >> (byte * 8u)) & 0xffu)
                              : 0u;
    unsigned left_end = (left.known_mask & (1u << byte)) ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte))
                               ? ((right.word >> (byte * 8u)) & 0xffu)
                               : 0u;
    unsigned right_end = (right.known_mask & (1u << byte)) ? right_start : 255u;
    unsigned carry;
    for (carry = 0; carry <= 1; ++carry) {
      unsigned a;
      if (!(carry_mask & (1u << carry)))
        continue;
      for (a = left_start; a <= left_end; ++a) {
        unsigned b;
        for (b = right_start; b <= right_end; ++b) {
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
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
    carry_mask = next_carry_mask;
  }
  return result;
}

static Word constant(uint32_t word) {
  Word value;
  set_known(&value, word);
  return value;
}

static Word add_constant(Word source, uint32_t value) {
  return add_words(source, constant(value));
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
                    unsigned width, Word value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameOpponentContactAccess *event =
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
                  uint8_t **data, uint8_t **known) {
  size_t i;
  size_t j;
  stop(run, pc, address, 0);
  TRY(spend(run));
  ++run->out->accesses;
  if (address & (width - 1u))
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

static int read_memory(Run *run, uint32_t address, unsigned width, uint32_t pc,
                       Word *value) {
  uint8_t *data;
  uint8_t *known;
  Word result = {0, 0};
  unsigned i;
  TRY(locate(run, address, width, pc, &data, &known));
  for (i = 0; i < width; ++i) {
    result.word |= (uint32_t)data[i] << (i * 8u);
    if (!known || known[i])
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << i));
  }
  *value = result;
  ++run->out->reads;
  journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, address, width, result);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_memory(Run *run, uint32_t address, unsigned width, uint32_t pc,
                        Word value) {
  uint8_t *data;
  uint8_t *known;
  unsigned i;
  value.word &= width_mask(width);
  value.known_mask &= knowledge_mask(width);
  TRY(locate(run, address, width, pc, &data, &known));
  if (!known && value.known_mask != knowledge_mask(width))
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < width; ++i) {
    data[i] = (uint8_t)(value.word >> (i * 8u));
    if (known)
      known[i] = (uint8_t)((value.known_mask >> i) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int address(Run *run, Word base, int32_t offset, uint32_t pc,
                   uint32_t *result) {
  Word sum = add_constant(base, (uint32_t)offset);
  if (sum.known_mask != 0x0f) {
    stop(run, pc, sum.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = sum.word;
  return NBA97_TEXT_COMPLETE;
}

static Word extend(Word value, unsigned width, int sign) {
  Word result;
  value.word &= width_mask(width);
  value.known_mask &= knowledge_mask(width);
  result = value;
  if (sign && (value.word & (UINT32_C(1) << (width * 8u - 1u))))
    result.word |= ~width_mask(width);
  if (!sign)
    result.known_mask =
        (uint8_t)(result.known_mask | (0x0fu ^ knowledge_mask(width)));
  else if (value.known_mask & (1u << (width - 1u)))
    result.known_mask =
        (uint8_t)(result.known_mask | (0x0fu ^ knowledge_mask(width)));
  return result;
}

static int load(Run *run, unsigned target, unsigned base, int32_t offset,
                unsigned width, int sign, uint32_t pc) {
  uint32_t guest_address;
  Word value;
  TRY(address(run, R(base), offset, pc, &guest_address));
  TRY(read_memory(run, guest_address, width, pc, &value));
  R(target) = extend(value, width, sign);
  return NBA97_TEXT_COMPLETE;
}

static int store(Run *run, unsigned source, unsigned base, int32_t offset,
                 unsigned width, uint32_t pc) {
  uint32_t guest_address;
  TRY(address(run, R(base), offset, pc, &guest_address));
  return write_memory(run, guest_address, width, pc, R(source));
}

static Word and_constant(Word value, uint32_t mask) {
  Word result;
  unsigned byte;
  result.word = value.word & mask;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    uint32_t byte_mask = (mask >> (byte * 8u)) & 0xffu;
    if (byte_mask == 0 || (value.known_mask & (1u << byte)))
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
  }
  return result;
}

static int equal_words(Word left, Word right, int *equal) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((left.known_mask & right.known_mask & (1u << byte)) &&
        (((left.word ^ right.word) >> (byte * 8u)) & 0xffu)) {
      *equal = 0;
      return 1;
    }
  if ((left.known_mask & right.known_mask) == 0x0f) {
    *equal = 1;
    return 1;
  }
  return 0;
}

static int64_t signed_word(uint32_t word) {
  return (word & UINT32_C(0x80000000)) ? (int64_t)word - INT64_C(0x100000000)
                                       : (int64_t)word;
}

static void signed_bounds(Word value, int64_t *minimum, int64_t *maximum) {
  uint32_t low = 0;
  uint32_t high = 0;
  unsigned byte;
  if (!(value.known_mask & 8u)) {
    *minimum = INT32_MIN;
    *maximum = INT32_MAX;
    return;
  }
  for (byte = 0; byte < 4; ++byte) {
    uint32_t source = (value.word >> (byte * 8u)) & 0xffu;
    low |= ((value.known_mask & (1u << byte)) ? source : 0u) << (byte * 8u);
    high |= ((value.known_mask & (1u << byte)) ? source : 0xffu) << (byte * 8u);
  }
  *minimum = signed_word(low);
  *maximum = signed_word(high);
}

static Word signed_less_constant(Word value, int32_t limit) {
  Word result;
  int64_t minimum;
  int64_t maximum;
  signed_bounds(value, &minimum, &maximum);
  result.word = signed_word(value.word) < limit;
  result.known_mask = 0x0e;
  if (maximum < limit)
    set_known(&result, 1);
  else if (minimum >= limit)
    set_known(&result, 0);
  return result;
}

static int invoke_child(Run *run, uint32_t pc) {
  Nba97GameOpponentContactEvent event;
  int accepted;
  stop(run, pc, 0, UINT32_C(0x8005f3bc));
  TRY(spend(run));
  memset(&event, 0, sizeof event);
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = UINT32_C(0x8005f3bc);
  event.operation = run->out->operations;
  event.invocation = run->out->child_calls + 1u;
  event.argument_count = 2;
  publish(run);
  if (!run->context->io)
    return NBA97_TEXT_IO_REFUSED;
  accepted = run->context->io(run->context->user, &run->context->memory, &event,
                              &run->machine);
  publish(run);
  if (accepted != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!valid_machine(&run->machine))
    return NBA97_TEXT_ARGUMENT;
  ++run->out->callbacks_completed;
  ++run->out->child_calls;
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_opponent_contact(Nba97GameOpponentContactContext *context,
                                Nba97GameOpponentContactProgress *out) {
  Run state;
  Run *run = &state;
  Word jump_target;
  int branch = 0;
  int decided = 0;
  TRY(validate(context, out, run));

  /* 0x8005F888..0x8005F890: retain first actor in a2 and save live ra. */
  STEP(0x8005f888);
  SP = add_constant(SP, UINT32_C(0xffffffe8));
  out->frame_stack_pointer = SP.word;
  STEP(0x8005f88c);
  A2 = A0;
  STEP(0x8005f890);
  TRY(store(run, 31, 29, 0x10, 4, UINT32_C(0x8005f890)));

  /* The first nonzero C2 short-circuits the second actor C2 read. */
  STEP(0x8005f894);
  TRY(load(run, 2, 6, 0xc2, 2, 1, UINT32_C(0x8005f894)));
  out->first_c2 = V0;
  STEP(0x8005f898);
  STEP(0x8005f89c);
  decided = equal_words(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x8005f8a0);
  if (!decided) {
    stop(run, UINT32_C(0x8005f89c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto option_gate;
  STEP(0x8005f8a4);
  TRY(load(run, 2, 5, 0xc2, 2, 1, UINT32_C(0x8005f8a4)));
  out->second_c2 = V0;
  STEP(0x8005f8a8);
  STEP(0x8005f8ac);
  decided = equal_words(V0, ZERO, &branch);
  STEP(0x8005f8b0);
  if (!decided) {
    stop(run, UINT32_C(0x8005f8ac), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto contact_order;

option_gate:
  /* 0x8005F8B4..0x8005F8DC: option and signed phase reject to zero. */
  STEP(0x8005f8b4);
  set_known(&V0, UINT32_C(0x80020000));
  STEP(0x8005f8b8);
  TRY(load(run, 2, 2, 0x1d8a, 1, 0, UINT32_C(0x8005f8b8)));
  out->option = V0;
  STEP(0x8005f8bc);
  STEP(0x8005f8c0);
  decided = equal_words(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x8005f8c4);
  set_known(&V0, 0);
  if (!decided) {
    stop(run, UINT32_C(0x8005f8c0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto epilogue;
  STEP(0x8005f8c8);
  set_known(&V0, UINT32_C(0x80100000));
  STEP(0x8005f8cc);
  TRY(load(run, 2, 2, (int32_t)UINT32_C(0xffffdb90), 2, 1,
           UINT32_C(0x8005f8cc)));
  out->phase = V0;
  STEP(0x8005f8d0);
  STEP(0x8005f8d4);
  V0 = signed_less_constant(V0, 129);
  out->last_predicate = V0;
  STEP(0x8005f8d8);
  decided = equal_words(V0, ZERO, &branch);
  STEP(0x8005f8dc);
  set_known(&V0, 0);
  if (!decided) {
    stop(run, UINT32_C(0x8005f8d8), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto epilogue;

contact_order:
  /* 0x8005F8E0..0x8005F904: DA bit zero keeps first-first order; each branch
   * publishes its source a0 move even when its predicate is unknown. */
  STEP(0x8005f8e0);
  TRY(load(run, 2, 5, 0xda, 1, 0, UINT32_C(0x8005f8e0)));
  out->second_da = V0;
  STEP(0x8005f8e4);
  STEP(0x8005f8e8);
  V0 = and_constant(V0, 1);
  STEP(0x8005f8ec);
  decided = equal_words(V0, ZERO, &branch);
  STEP(0x8005f8f0);
  A0 = A2;
  if (!decided) {
    stop(run, UINT32_C(0x8005f8ec), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto dispatch;
  STEP(0x8005f8f4);
  TRY(load(run, 2, 6, 0xda, 1, 0, UINT32_C(0x8005f8f4)));
  out->first_da = V0;
  STEP(0x8005f8f8);
  STEP(0x8005f8fc);
  V0 = and_constant(V0, 1);
  STEP(0x8005f900);
  decided = equal_words(V0, ZERO, &branch);
  STEP(0x8005f904);
  A0 = A1;
  if (!decided) {
    stop(run, UINT32_C(0x8005f900), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto swap;

  /* Both DA bits set: read signed owner before the first actor's full ID. */
  STEP(0x8005f908);
  set_known(&V1, UINT32_C(0x80100000));
  STEP(0x8005f90c);
  TRY(load(run, 3, 3, (int32_t)UINT32_C(0xffffdbcc), 2, 1,
           UINT32_C(0x8005f90c)));
  out->owner = V1;
  STEP(0x8005f910);
  TRY(load(run, 2, 6, 0, 4, 0, UINT32_C(0x8005f910)));
  out->first_id = V0;
  STEP(0x8005f914);
  STEP(0x8005f918);
  decided = equal_words(V0, V1, &branch);
  branch = !branch;
  STEP(0x8005f91c);
  if (!decided) {
    stop(run, UINT32_C(0x8005f918), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto swap;
  STEP(0x8005f920);
  STEP(0x8005f924);
  A0 = A2;
  goto dispatch;

swap:
  STEP(0x8005f928);
  A1 = A2;

dispatch:
  /* 0x8005F92C: dispatch the ordered pair after the source NOP delay. */
  STEP(0x8005f92c);
  set_known(&RA, UINT32_C(0x8005f934));
  STEP(0x8005f930);
  TRY(invoke_child(run, UINT32_C(0x8005f92c)));
  STEP(0x8005f934);
  V0 = and_constant(V0, 0xff);

epilogue:
  /* 0x8005F938..0x8005F944: reload ra via live sp and execute JR's NOP. */
  STEP(0x8005f938);
  TRY(load(run, 31, 29, 0x10, 4, 0, UINT32_C(0x8005f938)));
  STEP(0x8005f93c);
  SP = add_constant(SP, UINT32_C(0x00000018));
  STEP(0x8005f940);
  jump_target = RA;
  STEP(0x8005f944);
  if (jump_target.known_mask != 0x0f) {
    stop(run, UINT32_C(0x8005f940), jump_target.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  out->restored_return_address = jump_target;
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
