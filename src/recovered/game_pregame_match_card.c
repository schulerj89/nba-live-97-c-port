#include "game_pregame_match_card.h"

#include <limits.h>
#include <string.h>

typedef Nba97GamePregameMatchCardWord Word;

typedef struct Run {
  Nba97GamePregameMatchCardContext *context;
  Nba97GamePregameMatchCardProgress *out;
  Nba97GamePregameMatchCardMachine machine;
} Run;

#define R(i) (run->machine.registers.gpr[(i)])
#define ZERO R(0)
#define AT R(1)
#define V0 R(2)
#define V1 R(3)
#define A0 R(4)
#define A1 R(5)
#define A2 R(6)
#define A3 R(7)
#define T0 R(8)
#define T1 R(9)
#define T2 R(10)
#define T3 R(11)
#define T4 R(12)
#define S0 R(16)
#define S1 R(17)
#define S2 R(18)
#define S3 R(19)
#define S4 R(20)
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
    ++run->out->instruction_count;                                             \
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
  run->out->stopped_entry = 0;
  publish(run);
}

static int machine_valid(const Nba97GamePregameMatchCardMachine *machine) {
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

static int initialize(Nba97GamePregameMatchCardContext *context,
                      Nba97GamePregameMatchCardProgress *out, Run *run) {
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

/* Enumerating byte carries and borrows retains each invariant result byte. */
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

static Word subtract(Word left, Word right) {
  Word result;
  unsigned borrow_mask = 1;
  unsigned byte;
  result.word = left.word - right.word;
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
    unsigned borrow;
    for (borrow = 0; borrow < 2; ++borrow) {
      unsigned a;
      if (!(borrow_mask & (1u << borrow)))
        continue;
      for (a = left_start; a <= left_end; ++a) {
        unsigned b;
        for (b = right_start; b <= right_end; ++b) {
          unsigned output = (a - b - borrow) & 255u;
          next_mask |= 1u << (a < b + borrow);
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
    borrow_mask = next_mask;
  }
  return result;
}

static Word and_constant(Word value, uint32_t constant) {
  Word result;
  unsigned byte;
  result.word = value.word & constant;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    uint32_t mask = (constant >> (byte * 8u)) & 255u;
    if ((value.known_mask & (1u << byte)) || mask == 0)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
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
    Nba97GamePregameMatchCardAccess *event =
        &run->context->access_journal[index];
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
  if (width == 1) {
    loaded.word &= 255u;
    loaded.known_mask = (uint8_t)(loaded.known_mask | 14u);
  } else if (width == 2) {
    loaded.word &= 65535u;
    loaded.known_mask = (uint8_t)(loaded.known_mask | 12u);
  }
  *value = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_PREGAME_MATCH_CARD_READ, pc, address, width, loaded);
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
  journal(run, NBA97_GAME_PREGAME_MATCH_CARD_STORE, pc, address, width, value);
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

static int zero_decision(Run *run, Word value, uint32_t pc, int *is_zero) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((value.known_mask & (1u << byte)) &&
        ((value.word >> (byte * 8u)) & 255u)) {
      *is_zero = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (value.known_mask == 15) {
    *is_zero = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0);
  return NBA97_TEXT_UNKNOWN;
}

static Word sign_extend_half(Word value) {
  value.word = (value.word & 0x8000u) ? value.word | UINT32_C(0xffff0000)
                                      : value.word & UINT32_C(0x0000ffff);
  value.known_mask =
      (uint8_t)((value.known_mask & 3u) | ((value.known_mask & 2u) ? 12u : 0u));
  return value;
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
  shifted_known = (known_bits << amount) | ((UINT32_C(1) << amount) - 1u);
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte)
    if (((shifted_known >> (byte * 8u)) & 255u) == 255u)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  return result;
}

static Word arithmetic_shift_right(Word value, unsigned amount) {
  Word result;
  unsigned output;
  result.word = value.word >> amount;
  if (value.word & UINT32_C(0x80000000))
    result.word |= UINT32_MAX << (32u - amount);
  result.known_mask = 0;
  for (output = 0; output < 4; ++output) {
    unsigned low = output * 8u + amount;
    unsigned high = output * 8u + 7u + amount;
    unsigned source;
    int all = 1;
    if (low >= 32) {
      if (value.known_mask & 8u)
        result.known_mask = (uint8_t)(result.known_mask | (1u << output));
      continue;
    }
    if (high >= 32) {
      if (!(value.known_mask & 8u))
        continue;
      high = 31;
    }
    for (source = low / 8u; source <= high / 8u; ++source)
      if (!(value.known_mask & (1u << source)))
        all = 0;
    if (all)
      result.known_mask = (uint8_t)(result.known_mask | (1u << output));
  }
  return result;
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

static Word signed_less_constant(Word value, int32_t constant) {
  Word result;
  int64_t minimum;
  int64_t maximum;
  int32_t actual = value.word <= (uint32_t)INT32_MAX
                       ? (int32_t)value.word
                       : -1 - (int32_t)(UINT32_MAX - value.word);
  known(&result, actual < constant);
  signed_bounds(value, &minimum, &maximum);
  if (!(maximum < constant || minimum >= constant))
    result.known_mask = 14;
  return result;
}

static int load_signed_half(Run *run, unsigned destination, unsigned base,
                            uint32_t offset, uint32_t pc) {
  TRY(load(run, destination, base, offset, 2, 2, pc));
  R(destination) = sign_extend_half(R(destination));
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int invoke(Run *run, uint32_t pc, uint32_t entry, uint8_t kind,
                  uint8_t argument_count) {
  Nba97GamePregameMatchCardEvent event;
  int accepted;
  stop(run, pc, 0);
  run->out->stopped_entry = entry;
  TRY(spend(run));
  ++run->out->call_attempts[kind];
  memset(&event, 0, sizeof event);
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = entry;
  event.operation = run->out->operations;
  event.invocation = run->out->call_attempts[kind];
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

int nba97_game_pregame_match_card(Nba97GamePregameMatchCardContext *context,
                                  Nba97GamePregameMatchCardProgress *out) {
  Run storage;
  Run *run = &storage;
  Word branch_value;
  int condition;
  TRY(initialize(context, out, run));

  /* 0x80044550..0x80044574: allocate the frame, save ra/s4..s1, then
   * install the JAL return before its delay-slot s0 spill. */
  STEP(0x80044550);
  SP = add(SP, immediate(UINT32_C(0xffffff98)));
  out->frame_stack_pointer = SP.word;
  STEP(0x80044554);
  out->saved_return_address = RA;
  TRY(store(run, 31, 29, 0x64, 4, 4, 0x80044554));
  STEP(0x80044558);
  TRY(store(run, 20, 29, 0x60, 4, 4, 0x80044558));
  STEP(0x8004455c);
  TRY(store(run, 19, 29, 0x5c, 4, 4, 0x8004455c));
  STEP(0x80044560);
  TRY(store(run, 18, 29, 0x58, 4, 4, 0x80044560));
  STEP(0x80044564);
  TRY(store(run, 17, 29, 0x54, 4, 4, 0x80044564));
  STEP(0x80044568);
  known(&RA, 0x80044570);
  STEP(0x8004456c);
  TRY(store(run, 16, 29, 0x50, 4, 4, 0x8004456c));
  TRY(invoke(run, 0x80044568, 0x800810a4,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_800810A4, 0));
  STEP(0x80044570);
  known(&RA, 0x80044578);
  STEP(0x80044574);
  known(&S0, 0x131);
  TRY(invoke(run, 0x80044570, 0x8003081c,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_8003081C, 0));

  /* 0x80044578..0x80044698: seven layout-service calls retain all stack
   * arguments and callback mutations in source order. */
#define SET(reg_, value_, pc_)                                                 \
  do {                                                                         \
    STEP(pc_);                                                                 \
    known(&(reg_), (value_));                                                  \
  } while (0)
#define STACK_STORE(reg_, offset_, pc_)                                        \
  do {                                                                         \
    STEP(pc_);                                                                 \
    TRY(store(run, (reg_), 29, (offset_), 4, 4, (pc_)));                       \
  } while (0)
#define LAYOUT_CALL(pc_, delay_reg_, delay_offset_)                            \
  do {                                                                         \
    STEP(pc_);                                                                 \
    known(&RA, (pc_) + 8u);                                                    \
    STACK_STORE((delay_reg_), (delay_offset_), (pc_) + 4u);                    \
    TRY(invoke(run, (pc_), 0x80031614,                                         \
               NBA97_GAME_PREGAME_MATCH_CARD_CALL_80031614, 8));               \
  } while (0)
  SET(A0, UINT32_MAX, 0x80044578);
  SET(A1, 0x59, 0x8004457c);
  SET(V1, 0x800b0000, 0x80044580);
  STEP(0x80044584);
  TRY(load(run, 3, 3, 0x2048, 4, 4, 0x80044584));
  SET(A2, 0x82, 0x80044588);
  SET(A3, 0x34, 0x8004458c);
  SET(V0, 0x300, 0x80044590);
  SET(S2, 8, 0x80044594);
  STEP(0x80044598);
  TRY(store(run, 2, 3, 0x26, 2, 2, 0x80044598));
  STACK_STORE(0, 0x10, 0x8004459c);
  STACK_STORE(16, 0x14, 0x800445a0);
  STACK_STORE(18, 0x18, 0x800445a4);
  LAYOUT_CALL(0x800445a8, 0, 0x1c);

  SET(A0, UINT32_MAX, 0x800445b0);
  SET(A1, 0x69, 0x800445b4);
  SET(A2, 0x82, 0x800445b8);
  SET(A3, 0x3c, 0x800445bc);
  SET(S4, 0x28, 0x800445c0);
  SET(S3, 1, 0x800445c4);
  STACK_STORE(0, 0x10, 0x800445c8);
  STACK_STORE(16, 0x14, 0x800445cc);
  STACK_STORE(20, 0x18, 0x800445d0);
  LAYOUT_CALL(0x800445d4, 19, 0x1c);

  SET(A0, UINT32_MAX, 0x800445dc);
  SET(A1, 0x63, 0x800445e0);
  SET(A2, 0x55, 0x800445e4);
  SET(A3, 0x64, 0x800445e8);
  SET(S1, 0x15e, 0x800445ec);
  SET(V0, 0xc, 0x800445f0);
  STACK_STORE(0, 0x10, 0x800445f4);
  STACK_STORE(17, 0x14, 0x800445f8);
  STACK_STORE(2, 0x18, 0x800445fc);
  LAYOUT_CALL(0x80044600, 0, 0x1c);

  SET(A0, UINT32_MAX, 0x80044608);
  SET(A1, 0x59, 0x8004460c);
  SET(A2, 0x82, 0x80044610);
  SET(A3, 0x70, 0x80044614);
  STACK_STORE(0, 0x10, 0x80044618);
  STACK_STORE(16, 0x14, 0x8004461c);
  STACK_STORE(18, 0x18, 0x80044620);
  LAYOUT_CALL(0x80044624, 0, 0x1c);

  SET(A0, UINT32_MAX, 0x8004462c);
  SET(A1, 0x69, 0x80044630);
  SET(A2, 0x82, 0x80044634);
  SET(A3, 0x78, 0x80044638);
  STACK_STORE(0, 0x10, 0x8004463c);
  STACK_STORE(16, 0x14, 0x80044640);
  STACK_STORE(20, 0x18, 0x80044644);
  LAYOUT_CALL(0x80044648, 19, 0x1c);

  SET(A0, UINT32_MAX, 0x80044650);
  SET(A1, 0x59, 0x80044654);
  SET(A2, 0x55, 0x80044658);
  SET(A3, 0xa0, 0x8004465c);
  STACK_STORE(0, 0x10, 0x80044660);
  STACK_STORE(17, 0x14, 0x80044664);
  STACK_STORE(18, 0x18, 0x80044668);
  LAYOUT_CALL(0x8004466c, 0, 0x1c);

  SET(A0, UINT32_MAX, 0x80044674);
  SET(A1, 0x2f, 0x80044678);
  SET(A2, 0x55, 0x8004467c);
  SET(A3, 0xa8, 0x80044680);
  SET(V0, 0x12, 0x80044684);
  STACK_STORE(0, 0x10, 0x80044688);
  STACK_STORE(17, 0x14, 0x8004468c);
  STACK_STORE(2, 0x18, 0x80044690);
  LAYOUT_CALL(0x80044694, 0, 0x1c);
#undef LAYOUT_CALL

  /* 0x8004469C..0x80044850: construct stack strings and card labels, changing
   * the live font mode around team-name and location services. */
#define TEXT_CALL(pc_, delay_reg_)                                             \
  do {                                                                         \
    STEP(pc_);                                                                 \
    known(&RA, (pc_) + 8u);                                                    \
    STACK_STORE((delay_reg_), 0x10, (pc_) + 4u);                               \
    TRY(invoke(run, (pc_), 0x80030d18,                                         \
               NBA97_GAME_PREGAME_MATCH_CARD_CALL_80030D18, 5));               \
  } while (0)
  SET(A0, UINT32_MAX, 0x8004469c);
  STEP(0x800446a0);
  A1 = add(SP, immediate(0x20));
  SET(A2, 0x82, 0x800446a4);
  SET(A3, 0x34, 0x800446a8);
  SET(V0, 0x55, 0x800446ac);
  SET(S0, 2, 0x800446b0);
  STEP(0x800446b4);
  TRY(store(run, 2, 29, 0x20, 1, 1, 0x800446b4));
  STEP(0x800446b8);
  TRY(store(run, 0, 29, 0x21, 1, 1, 0x800446b8));
  TEXT_CALL(0x800446bc, 16);

  SET(A0, UINT32_MAX, 0x800446c4);
  STEP(0x800446c8);
  A1 = add(SP, immediate(0x20));
  SET(A2, 0x82, 0x800446cc);
  SET(A3, 0x70, 0x800446d0);
  SET(V0, 0x54, 0x800446d4);
  STEP(0x800446d8);
  TRY(store(run, 2, 29, 0x20, 1, 1, 0x800446d8));
  TEXT_CALL(0x800446dc, 16);

  SET(A0, UINT32_MAX, 0x800446e4);
  SET(A1, 0x80020000, 0x800446e8);
  STEP(0x800446ec);
  A1 = add(A1, immediate(0x5ce0));
  SET(A2, 0x81, 0x800446f0);
  SET(A3, 0x62, 0x800446f4);
  TEXT_CALL(0x800446f8, 19);

  SET(V0, 0x800b0000, 0x80044700);
  STEP(0x80044704);
  TRY(load(run, 2, 2, 0x2048, 4, 4, 0x80044704));
  STEP(0x80044708);
  A0 = add(SP, immediate(0x20));
  SET(A1, 5, 0x8004470c);
  SET(A2, 1, 0x80044710);
  STEP(0x80044714);
  known(&RA, 0x8004471c);
  STEP(0x80044718);
  TRY(store(run, 0, 2, 0x26, 2, 2, 0x80044718));
  TRY(invoke(run, 0x80044714, 0x80036688,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_80036688, 3));

  SET(A0, UINT32_MAX, 0x8004471c);
  STEP(0x80044720);
  A1 = add(SP, immediate(0x20));
  SET(A2, 0x96, 0x80044724);
  SET(A3, 0x3e, 0x80044728);
  TEXT_CALL(0x8004472c, 0);

  SET(V0, 0x800b0000, 0x80044734);
  STEP(0x80044738);
  TRY(load(run, 2, 2, 0x2048, 4, 4, 0x80044738));
  SET(S0, 0x200, 0x8004473c);
  STEP(0x80044740);
  TRY(store(run, 16, 2, 0x26, 2, 2, 0x80044740));
  SET(V0, 0x80020000, 0x80044744);
  STEP(0x80044748);
  TRY(load(run, 2, 2, UINT32_C(0xffffef24), 4, 4, 0x80044748));
  STEP(0x8004474c);
  STEP(0x80044750);
  TRY(load(run, 5, 2, 0x40, 4, 4, 0x80044750));
  STEP(0x80044754);
  known(&RA, 0x8004475c);
  STEP(0x80044758);
  A0 = add(SP, immediate(0x20));
  TRY(invoke(run, 0x80044754, 0x8009cb6c,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_8009CB6C, 2));

  SET(A0, UINT32_MAX, 0x8004475c);
  STEP(0x80044760);
  A1 = add(SP, immediate(0x20));
  SET(A2, 0x96, 0x80044764);
  SET(A3, 0x4a, 0x80044768);
  TEXT_CALL(0x8004476c, 0);

  SET(V0, 0x800b0000, 0x80044774);
  STEP(0x80044778);
  TRY(load(run, 2, 2, 0x2048, 4, 4, 0x80044778));
  STEP(0x8004477c);
  A0 = add(SP, immediate(0x20));
  SET(A1, 0, 0x80044780);
  SET(A2, 1, 0x80044784);
  STEP(0x80044788);
  known(&RA, 0x80044790);
  STEP(0x8004478c);
  TRY(store(run, 0, 2, 0x26, 2, 2, 0x8004478c));
  TRY(invoke(run, 0x80044788, 0x80036688,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_80036688, 3));

  SET(A0, UINT32_MAX, 0x80044790);
  STEP(0x80044794);
  A1 = add(SP, immediate(0x20));
  SET(A2, 0x96, 0x80044798);
  SET(A3, 0x7a, 0x8004479c);
  TEXT_CALL(0x800447a0, 0);

  SET(V0, 0x800b0000, 0x800447a8);
  STEP(0x800447ac);
  TRY(load(run, 2, 2, 0x2048, 4, 4, 0x800447ac));
  STEP(0x800447b0);
  STEP(0x800447b4);
  TRY(store(run, 16, 2, 0x26, 2, 2, 0x800447b4));
  SET(V0, 0x80020000, 0x800447b8);
  STEP(0x800447bc);
  TRY(load(run, 2, 2, UINT32_C(0xffffee60), 4, 4, 0x800447bc));
  STEP(0x800447c0);
  STEP(0x800447c4);
  TRY(load(run, 5, 2, 0x40, 4, 4, 0x800447c4));
  STEP(0x800447c8);
  known(&RA, 0x800447d0);
  STEP(0x800447cc);
  A0 = add(SP, immediate(0x20));
  TRY(invoke(run, 0x800447c8, 0x8009cb6c,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_8009CB6C, 2));

  SET(A0, UINT32_MAX, 0x800447d0);
  STEP(0x800447d4);
  A1 = add(SP, immediate(0x20));
  SET(A2, 0x96, 0x800447d8);
  SET(A3, 0x86, 0x800447dc);
  TEXT_CALL(0x800447e0, 0);

  SET(V0, 0x800b0000, 0x800447e8);
  STEP(0x800447ec);
  TRY(load(run, 2, 2, 0x2048, 4, 4, 0x800447ec));
  SET(A0, 0x80020000, 0x800447f0);
  STEP(0x800447f4);
  TRY(load(run, 4, 4, UINT32_C(0xffffec94), 4, 4, 0x800447f4));
  STEP(0x800447f8);
  STEP(0x800447fc);
  branch_value = A0;
  STEP(0x80044800);
  TRY(store(run, 0, 2, 0x26, 2, 2, 0x80044800));
  TRY(zero_decision(run, branch_value, 0x800447fc, &condition));
  if (!condition) {
    STEP(0x80044804);
    A0 = shift_left(A0, 16);
    STEP(0x80044808);
    known(&RA, 0x80044810);
    STEP(0x8004480c);
    A0 = arithmetic_shift_right(A0, 16);
    TRY(invoke(run, 0x80044808, 0x80081b50,
               NBA97_GAME_PREGAME_MATCH_CARD_CALL_80081B50, 1));
    STEP(0x80044810);
    STEP(0x80044814);
    A0 = add(SP, immediate(0x20));
  } else {
    SET(V0, 0x80020000, 0x80044818);
    STEP(0x8004481c);
    TRY(load(run, 2, 2, UINT32_C(0xffffee60), 4, 4, 0x8004481c));
    STEP(0x80044820);
    STEP(0x80044824);
    TRY(load(run, 2, 2, 0x4c, 4, 4, 0x80044824));
    STEP(0x80044828);
    A0 = add(SP, immediate(0x20));
  }
  SET(A1, 0x80020000, 0x8004482c);
  STEP(0x80044830);
  A1 = add(A1, immediate(0x5ce4));
  STEP(0x80044834);
  known(&RA, 0x8004483c);
  STEP(0x80044838);
  A2 = V0;
  TRY(invoke(run, 0x80044834, 0x8009cb7c,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_8009CB7C, 3));
  SET(A0, UINT32_MAX, 0x8004483c);
  STEP(0x80044840);
  A1 = add(SP, immediate(0x20));
  SET(A2, 0x96, 0x80044844);
  SET(A3, 0xa8, 0x80044848);
  TEXT_CALL(0x8004484c, 0);
#undef TEXT_CALL

  /* 0x80044854..0x80044914: initialize optional demo audio, then pump the
   * typed input/readiness/clock/frame services with the source signed gates. */
  SET(V0, 0x80020000, 0x80044854);
  STEP(0x80044858);
  TRY(load(run, 2, 2, UINT32_C(0xffffedec), 2, 2, 0x80044858));
  STEP(0x8004485c);
  STEP(0x80044860);
  branch_value = V0;
  STEP(0x80044864);
  known(&S1, UINT32_MAX);
  TRY(zero_decision(run, branch_value, 0x80044860, &condition));
  if (!condition) {
    STEP(0x80044868);
    known(&RA, 0x80044870);
    STEP(0x8004486c);
    TRY(invoke(run, 0x80044868, 0x80035678,
               NBA97_GAME_PREGAME_MATCH_CARD_CALL_80035678, 0));
  }
  STEP(0x80044870);
  known(&S4, 0);
  STEP(0x80044874);
  known(&RA, 0x8004487c);
  STEP(0x80044878);
  known(&S2, 0);
  TRY(invoke(run, 0x80044874, 0x800a5810,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_800A5810, 0));
  STEP(0x8004487c);
  S0 = V0;
  STEP(0x80044880);
  A0 = add(SP, immediate(0x48));
  STEP(0x80044884);
  known(&RA, 0x8004488c);
  STEP(0x80044888);
  A1 = add(SP, immediate(0x4a));
  TRY(invoke(run, 0x80044884, 0x800363dc,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_800363DC, 2));

  STEP(0x8004488c);
  V0 = signed_less_constant(S2, 3600);
poll_branch:
  ++out->polling_iterations;
  STEP(0x80044890);
  branch_value = V0;
  STEP(0x80044894);
  TRY(zero_decision(run, branch_value, 0x80044890, &condition));
  if (condition) {
    out->exited_for_timeout = 1;
    goto exit_poll;
  }
  STEP(0x80044898);
  known(&RA, 0x800448a0);
  STEP(0x8004489c);
  TRY(invoke(run, 0x80044898, 0x80083eec,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_80083EEC, 0));
  STEP(0x800448a0);
  TRY(load_signed_half(run, 4, 29, 0x48, 0x800448a0));
  STEP(0x800448a4);
  known(&RA, 0x800448ac);
  STEP(0x800448a8);
  A1 = add(SP, immediate(0x4a));
  TRY(invoke(run, 0x800448a4, 0x80036478,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_80036478, 2));
  STEP(0x800448ac);
  known(&RA, 0x800448b4);
  STEP(0x800448b0);
  S4 = V0;
  TRY(invoke(run, 0x800448ac, 0x800a5810,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_800A5810, 0));
  STEP(0x800448b4);
  S3 = V0;
  STEP(0x800448b8);
  known(&RA, 0x800448c0);
  STEP(0x800448bc);
  S0 = subtract(S3, S0);
  TRY(invoke(run, 0x800448b8, 0x80088d0c,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_80088D0C, 0));
  STEP(0x800448c0);
  branch_value = V0;
  STEP(0x800448c4);
  TRY(zero_decision(run, branch_value, 0x800448c0, &condition));
  if (!condition) {
    STEP(0x800448c8);
    branch_value = signed_less_constant(S1, 0);
    STEP(0x800448cc);
    TRY(zero_decision(run, branch_value, 0x800448c8, &condition));
    if (condition) {
      STEP(0x800448d0);
      S1 = add(S1, S0);
      STEP(0x800448d4);
      V0 = signed_less_constant(S1, 480);
      STEP(0x800448d8);
      branch_value = V0;
      STEP(0x800448dc);
      TRY(zero_decision(run, branch_value, 0x800448d8, &condition));
      if (condition) {
        STEP(0x800448e0);
        STEP(0x800448e4);
        known(&S2, 3610);
      }
    }
  } else {
    STEP(0x800448e8);
    known(&S1, 0);
  }

  STEP(0x800448ec);
  branch_value = signed_less_constant(S0, 1);
  STEP(0x800448f0);
  TRY(zero_decision(run, branch_value, 0x800448ec, &condition));
  if (condition) {
    STEP(0x800448f4);
    known(&RA, 0x800448fc);
    STEP(0x800448f8);
    S2 = add(S2, S0);
    TRY(invoke(run, 0x800448f4, 0x8002de34,
               NBA97_GAME_PREGAME_MATCH_CARD_CALL_8002DE34, 0));
    STEP(0x800448fc);
    known(&RA, 0x80044904);
    STEP(0x80044900);
    known(&A0, 0x5f);
    TRY(invoke(run, 0x800448fc, 0x80029880,
               NBA97_GAME_PREGAME_MATCH_CARD_CALL_80029880, 1));
  }
  STEP(0x80044904);
  known(&RA, 0x8004490c);
  STEP(0x80044908);
  S0 = S3;
  TRY(invoke(run, 0x80044904, 0x80049018,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_80049018, 0));
  STEP(0x8004490c);
  V0 = and_constant(S4, 0x180);
  STEP(0x80044910);
  branch_value = V0;
  STEP(0x80044914);
  V0 = signed_less_constant(S2, 3600);
  TRY(zero_decision(run, branch_value, 0x80044910, &condition));
  if (condition)
    goto poll_branch;
  out->exited_for_input = 1;

exit_poll:
  /* 0x80044918..0x80044970: acknowledge any full nonzero input, publish demo
   * skip state, close UI state, clear presentation, and invoke final cleanup.
   */
  STEP(0x80044918);
  branch_value = S4;
  STEP(0x8004491c);
  TRY(zero_decision(run, branch_value, 0x80044918, &condition));
  if (!condition) {
    STEP(0x80044920);
    known(&RA, 0x80044928);
    STEP(0x80044924);
    known(&A0, 0x61);
    TRY(invoke(run, 0x80044920, 0x80029258,
               NBA97_GAME_PREGAME_MATCH_CARD_CALL_80029258, 1));
    SET(V1, 0x80020000, 0x80044928);
    STEP(0x8004492c);
    V1 = add(V1, immediate(UINT32_C(0xffffedec)));
    STEP(0x80044930);
    TRY(load(run, 2, 3, 0, 2, 2, 0x80044930));
    STEP(0x80044934);
    STEP(0x80044938);
    branch_value = V0;
    STEP(0x8004493c);
    known(&V0, 1);
    TRY(zero_decision(run, branch_value, 0x80044938, &condition));
    if (!condition) {
      SET(AT, 0x80100000, 0x80044940);
      STEP(0x80044944);
      TRY(store(run, 2, 1, UINT32_C(0xffffdb78), 1, 1, 0x80044944));
      SET(V0, 0x63, 0x80044948);
      STEP(0x8004494c);
      TRY(store(run, 2, 3, 0, 2, 2, 0x8004494c));
    }
  }
  STEP(0x80044950);
  TRY(load_signed_half(run, 4, 29, 0x4a, 0x80044950));
  STEP(0x80044954);
  known(&RA, 0x8004495c);
  STEP(0x80044958);
  TRY(invoke(run, 0x80044954, 0x80036600,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_80036600, 1));
  STEP(0x8004495c);
  known(&RA, 0x80044964);
  STEP(0x80044960);
  TRY(invoke(run, 0x8004495c, 0x8003081c,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_8003081C, 0));
  SET(AT, 0x800f0000, 0x80044964);
  STEP(0x80044968);
  TRY(store(run, 0, 1, UINT32_C(0xffffb680), 1, 1, 0x80044968));
  STEP(0x8004496c);
  known(&RA, 0x80044974);
  STEP(0x80044970);
  TRY(invoke(run, 0x8004496c, 0x8008048c,
             NBA97_GAME_PREGAME_MATCH_CARD_CALL_8008048C, 0));

  /* 0x80044974..0x80044994: reload all six saved words through callback-live
   * sp, advance that live pointer, and validate restored ra after JR/NOP. */
  STEP(0x80044974);
  TRY(load(run, 31, 29, 0x64, 4, 4, 0x80044974));
  out->restored_return_address = RA;
  STEP(0x80044978);
  TRY(load(run, 20, 29, 0x60, 4, 4, 0x80044978));
  STEP(0x8004497c);
  TRY(load(run, 19, 29, 0x5c, 4, 4, 0x8004497c));
  STEP(0x80044980);
  TRY(load(run, 18, 29, 0x58, 4, 4, 0x80044980));
  STEP(0x80044984);
  TRY(load(run, 17, 29, 0x54, 4, 4, 0x80044984));
  STEP(0x80044988);
  TRY(load(run, 16, 29, 0x50, 4, 4, 0x80044988));
  STEP(0x8004498c);
  SP = add(SP, immediate(0x68));
  STEP(0x80044990);
  STEP(0x80044994);
  if (RA.known_mask != 15) {
    stop(run, 0x80044990, RA.word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, 0x80044990, RA.word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0);
  return NBA97_TEXT_COMPLETE;
#undef STACK_STORE
#undef SET
}
