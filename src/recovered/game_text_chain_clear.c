#include "game_text_chain_clear.h"

#include <limits.h>
#include <string.h>

typedef Nba97GameTextChainClearWord Word;

typedef struct Run {
  Nba97GameTextChainClearContext *context;
  Nba97GameTextChainClearProgress *out;
  Nba97GameTextChainClearMachine machine;
} Run;

#define R(i) (run->machine.registers.gpr[(i)])
#define V0 R(2)
#define V1 R(3)
#define A0 R(4)
#define A1 R(5)
#define A2 R(6)
#define RA R(31)
#define STEP(pc)                                                               \
  do {                                                                         \
    (void)(pc);                                                                \
    ++run->out->instruction_count;                                             \
  } while (0)
#define TRY(expression)                                                        \
  do {                                                                         \
    int result_ = (expression);                                                \
    if (result_ != NBA97_TEXT_COMPLETE)                                        \
      return result_;                                                          \
  } while (0)

static void known(Word *word, uint32_t value) {
  word->word = value;
  word->known_mask = 15;
}

static Word immediate(uint32_t value) {
  Word result;
  known(&result, value);
  return result;
}

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  publish(run);
}

static int machine_valid(const Nba97GameTextChainClearMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 15 ||
      machine->hi.known_mask > 15 || machine->lo.known_mask > 15)
    return 0;
  for (i = 0; i < 32; ++i)
    if (machine->registers.gpr[i].known_mask > 15)
      return 0;
  return 1;
}

static int initialize(Nba97GameTextChainClearContext *context,
                      Nba97GameTextChainClearProgress *out, Run *run) {
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

/* Enumerating byte carries retains every provably invariant output byte. */
static Word add(Word left, Word right) {
  Word result;
  unsigned carry_mask = 1;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned left_start = (left.known_mask & (1u << byte))
                              ? ((left.word >> (byte * 8u)) & 255u)
                              : 0;
    unsigned left_end = (left.known_mask & (1u << byte)) ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte))
                               ? ((right.word >> (byte * 8u)) & 255u)
                               : 0;
    unsigned right_end = (right.known_mask & (1u << byte)) ? right_start : 255u;
    unsigned carry;
    for (carry = 0; carry < 2; ++carry) {
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

static Word shift_left(Word value, unsigned amount) {
  Word result;
  uint32_t known_bits = 0;
  uint32_t shifted_known;
  unsigned byte;
  result.word = value.word << amount;
  for (byte = 0; byte < 4; ++byte)
    if (value.known_mask & (1u << byte))
      known_bits |= UINT32_C(255) << (byte * 8u);
  shifted_known =
      (known_bits << amount) | (amount ? (UINT32_C(1) << amount) - 1u : 0u);
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte)
    if (((shifted_known >> (byte * 8u)) & 255u) == 255u)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  return result;
}

static Word shift_right_arithmetic(Word value, unsigned amount) {
  Word result;
  unsigned byte;
  result.word = value.word >> amount;
  if (value.word & UINT32_C(0x80000000))
    result.word |= ~(UINT32_MAX >> amount);
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned low_bit = byte * 8u + amount;
    unsigned high_bit = low_bit + 7u;
    unsigned first_source = low_bit / 8u;
    unsigned last_source = high_bit < 32u ? high_bit / 8u : 3u;
    unsigned source;
    int all_known = 1;
    for (source = first_source; source <= last_source; ++source)
      if (!(value.known_mask & (1u << source)))
        all_known = 0;
    if (high_bit >= 32u && !(value.known_mask & 8u))
      all_known = 0;
    if (all_known)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static int negative_decision(Run *run, Word value, uint32_t pc, int *negative) {
  if (value.known_mask & 8u) {
    *negative = !!(value.word & UINT32_C(0x80000000));
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0);
  return NBA97_TEXT_UNKNOWN;
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
    Nba97GameTextChainClearAccess *event = &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value =
        value.word &
        (width == 4 ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - 1u);
    event->operation = run->out->operations;
    event->width = (uint8_t)width;
    event->known_mask =
        (uint8_t)(value.known_mask & (uint8_t)((1u << width) - 1u));
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t address, unsigned width,
                  unsigned alignment, uint32_t pc, uint8_t **data,
                  uint8_t **known_bytes) {
  size_t i;
  size_t j;
  stop(run, pc, address);
  TRY(spend(run));
  ++run->out->accesses;
  if (address & (alignment - 1u))
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0; i < run->context->memory.count; ++i) {
    Nba97GameTextRegion *region = &run->context->memory.region[i];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        width > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known_bytes = region->known ? region->known + (size_t)offset : 0;
    if (*known_bytes)
      for (j = 0; j < width; ++j)
        if ((*known_bytes)[j] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int read_value(Run *run, uint32_t address, unsigned width,
                      unsigned alignment, uint32_t pc, Word *value) {
  uint8_t *data;
  uint8_t *known_bytes;
  Word loaded = {0, 0};
  unsigned i;
  TRY(locate(run, address, width, alignment, pc, &data, &known_bytes));
  for (i = 0; i < width; ++i) {
    loaded.word |= (uint32_t)data[i] << (i * 8u);
    if (!known_bytes || known_bytes[i])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
  }
  if (width == 2)
    loaded.known_mask = (uint8_t)(loaded.known_mask | 12u);
  *value = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_TEXT_CHAIN_CLEAR_READ, pc, address, width, loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, uint32_t address, unsigned width,
                       unsigned alignment, uint32_t pc, Word value) {
  uint8_t *data;
  uint8_t *known_bytes;
  uint8_t required = (uint8_t)((1u << width) - 1u);
  unsigned i;
  TRY(locate(run, address, width, alignment, pc, &data, &known_bytes));
  if (!known_bytes && (value.known_mask & required) != required)
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < width; ++i) {
    data[i] = (uint8_t)(value.word >> (i * 8u));
    if (known_bytes)
      known_bytes[i] = (uint8_t)((value.known_mask >> i) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_TEXT_CHAIN_CLEAR_STORE, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int address(Run *run, Word base, uint32_t offset, uint32_t pc,
                   uint32_t *effective) {
  Word value = add(base, immediate(offset));
  if (value.known_mask != 15) {
    stop(run, pc, value.word);
    return NBA97_TEXT_UNKNOWN;
  }
  *effective = value.word;
  return NBA97_TEXT_COMPLETE;
}

static int load(Run *run, unsigned destination, unsigned base, uint32_t offset,
                unsigned width, unsigned alignment, uint32_t pc) {
  uint32_t effective;
  Word value;
  TRY(address(run, R(base), offset, pc, &effective));
  TRY(read_value(run, effective, width, alignment, pc, &value));
  R(destination) = value;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int store(Run *run, unsigned source, unsigned base, uint32_t offset,
                 unsigned width, unsigned alignment, uint32_t pc) {
  uint32_t effective;
  TRY(address(run, R(base), offset, pc, &effective));
  return write_value(run, effective, width, alignment, pc, R(source));
}

int nba97_game_text_chain_clear(Nba97GameTextChainClearContext *context,
                                Nba97GameTextChainClearProgress *out) {
  Run state;
  Run *run = &state;
  int negative;
  Word branch_value;

  TRY(initialize(context, out, run));

  /* 0x8003066C..0x80030684: retain raw a0, load the font pointer before the
   * signed-low-half gate, and execute the branch's index-doubling delay. */
  STEP(0x8003066c);
  A2 = A0;
  STEP(0x80030670);
  known(&A1, 0x800b0000);
  STEP(0x80030674);
  TRY(load(run, 5, 5, 0x2048, 4, 4, 0x80030674));
  STEP(0x80030678);
  A0 = shift_left(A0, 16);
  STEP(0x8003067c);
  V0 = shift_right_arithmetic(A0, 16);
  branch_value = V0;
  STEP(0x80030680);
  STEP(0x80030684);
  V0 = shift_left(V0, 1);
  TRY(negative_decision(run, branch_value, 0x80030680, &negative));
  if (negative)
    goto epilogue;

  /* 0x80030688..0x800306A4: fetch the initial head from the live table and
   * classify its signed halfword after the branch NOP. */
  STEP(0x80030688);
  TRY(load(run, 3, 5, 0x14, 4, 4, 0x80030688));
  STEP(0x8003068c);
  STEP(0x80030690);
  V0 = add(V0, V1);
  STEP(0x80030694);
  TRY(load(run, 2, 2, 0, 2, 2, 0x80030694));
  STEP(0x80030698);
  STEP(0x8003069c);
  V0 = shift_left(V0, 16);
  STEP(0x800306a0);
  STEP(0x800306a4);
  TRY(negative_decision(run, V0, 0x800306a0, &negative));
  if (negative)
    goto finish_table;

chain_loop:
  ++out->chain_iterations;
  /* 0x800306A8..0x800306C4: reload the link base, fetch the next link, then
   * clear the current entry in the BGEZ delay before resolving its predicate.
   */
  STEP(0x800306a8);
  TRY(load(run, 3, 5, 0x10, 4, 4, 0x800306a8));
  STEP(0x800306ac);
  V0 = shift_right_arithmetic(V0, 10);
  STEP(0x800306b0);
  V1 = add(V1, V0);
  STEP(0x800306b4);
  TRY(load(run, 2, 3, 0x18, 2, 2, 0x800306b4));
  STEP(0x800306b8);
  STEP(0x800306bc);
  V0 = shift_left(V0, 16);
  branch_value = V0;
  STEP(0x800306c0);
  STEP(0x800306c4);
  known(&R(0), 0);
  TRY(store(run, 0, 3, 0x12, 2, 2, 0x800306c4));
  TRY(negative_decision(run, branch_value, 0x800306c0, &negative));
  if (!negative)
    goto chain_loop;

finish_table:
  /* 0x800306C8..0x800306DC: reload the possibly aliased head-table pointer,
   * recompute the index from raw a2, and publish the -1 sentinel. */
  STEP(0x800306c8);
  TRY(load(run, 2, 5, 0x14, 4, 4, 0x800306c8));
  STEP(0x800306cc);
  V1 = shift_left(A2, 16);
  STEP(0x800306d0);
  V1 = shift_right_arithmetic(V1, 15);
  STEP(0x800306d4);
  V1 = add(V1, V0);
  STEP(0x800306d8);
  known(&V0, UINT32_MAX);
  STEP(0x800306dc);
  TRY(store(run, 2, 3, 0, 2, 2, 0x800306dc));

epilogue:
  /* 0x800306E0..0x800306E4: execute the JR NOP delay before validating the
   * caller-supplied return target. */
  STEP(0x800306e0);
  STEP(0x800306e4);
  if (RA.known_mask != 15) {
    stop(run, 0x800306e0, RA.word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, 0x800306e0, RA.word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
