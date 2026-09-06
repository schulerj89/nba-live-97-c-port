#include "game_controller_profile_reset.h"

#include <limits.h>
#include <string.h>

typedef Nba97GameControllerProfileResetWord Word;

typedef struct Run {
  Nba97GameControllerProfileResetContext *context;
  Nba97GameControllerProfileResetProgress *out;
  Nba97GameControllerProfileResetMachine machine;
} Run;

#define R(i) (run->machine.registers.gpr[(i)])
#define ZERO R(0)
#define AT R(1)
#define V0 R(2)
#define V1 R(3)
#define A0 R(4)
#define A1 R(5)
#define A2 R(6)
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

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t entry) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  run->out->stopped_entry = entry;
  publish(run);
}

static int
valid_machine(const Nba97GameControllerProfileResetMachine *machine) {
  unsigned index;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 15 ||
      machine->hi.known_mask > 15 || machine->lo.known_mask > 15)
    return 0;
  for (index = 0; index < 32; ++index)
    if (machine->registers.gpr[index].known_mask > 15)
      return 0;
  return 1;
}

static int initialize(Nba97GameControllerProfileResetContext *context,
                      Nba97GameControllerProfileResetProgress *out, Run *run) {
  size_t first;
  size_t second;
  if (!out)
    return NBA97_TEXT_ARGUMENT;
  memset(out, 0, sizeof *out);
  if (!context || (!context->memory.region && context->memory.count) ||
      (!context->access_journal && context->access_journal_capacity) ||
      !valid_machine(&context->machine))
    return NBA97_TEXT_ARGUMENT;
  for (first = 0; first < context->memory.count; ++first) {
    const Nba97GameTextRegion *a = &context->memory.region[first];
    if (!a->data || !a->size ||
        (uint64_t)a->base + (uint64_t)a->size > UINT64_C(0x100000000))
      return NBA97_TEXT_ARGUMENT;
    for (second = 0; second < first; ++second) {
      const Nba97GameTextRegion *b = &context->memory.region[second];
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

/* Enumerating byte carries and borrows retains every invariant result byte. */
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

static Word shift(Word value, unsigned amount, int right, int arithmetic) {
  Word result;
  unsigned output;
  result.word = right ? value.word >> amount : value.word << amount;
  if (right && arithmetic && (value.word & UINT32_C(0x80000000)))
    result.word |= UINT32_MAX << (32u - amount);
  result.known_mask = 0;
  for (output = 0; output < 4; ++output) {
    unsigned low = output * 8u;
    unsigned high = low + 7u;
    unsigned source_low;
    unsigned source_high;
    unsigned source;
    int all = 1;
    if (!right) {
      if (high < amount) {
        result.known_mask = (uint8_t)(result.known_mask | (1u << output));
        continue;
      }
      source_low = low < amount ? 0 : low - amount;
      source_high = high - amount;
    } else {
      source_low = low + amount;
      source_high = high + amount;
      if (source_low >= 32) {
        if (!arithmetic || (value.known_mask & 8u))
          result.known_mask = (uint8_t)(result.known_mask | (1u << output));
        continue;
      }
      if (source_high >= 32) {
        if (arithmetic && !(value.known_mask & 8u))
          continue;
        source_high = 31;
      }
    }
    for (source = source_low / 8u; source <= source_high / 8u; ++source)
      if (!(value.known_mask & (1u << source)))
        all = 0;
    if (all)
      result.known_mask = (uint8_t)(result.known_mask | (1u << output));
  }
  return result;
}

static int32_t signed_word(uint32_t word) {
  return word <= (uint32_t)INT32_MAX ? (int32_t)word
                                     : -1 - (int32_t)(UINT32_MAX - word);
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

static Word signed_less_constant(Word value, int32_t limit) {
  Word result;
  int64_t minimum;
  int64_t maximum;
  known(&result, signed_word(value.word) < limit);
  signed_bounds(value, &minimum, &maximum);
  if (maximum < limit || minimum >= limit)
    return result;
  result.known_mask = 14;
  return result;
}

static int zero_decision(Word value, int *is_zero) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((value.known_mask & (1u << byte)) &&
        ((value.word >> (byte * 8u)) & 255u)) {
      *is_zero = 0;
      return 1;
    }
  if (value.known_mask == 15) {
    *is_zero = 1;
    return 1;
  }
  return 0;
}

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static uint32_t width_mask(unsigned width) {
  return width == 4 ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - 1u;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width, Word value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameControllerProfileResetAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word & width_mask(width);
    event->operation = run->out->operations;
    event->width = (uint8_t)width;
    event->known_mask = (uint8_t)(value.known_mask & ((1u << width) - 1u));
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t address_value, unsigned width,
                  unsigned alignment, uint32_t pc, uint8_t **data,
                  uint8_t **known_bytes) {
  size_t index;
  size_t byte;
  stop(run, pc, address_value, 0);
  TRY(spend(run));
  ++run->out->accesses;
  if (address_value & (alignment - 1u))
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (index = 0; index < run->context->memory.count; ++index) {
    Nba97GameTextRegion *region = &run->context->memory.region[index];
    uint64_t offset = (uint64_t)address_value - region->base;
    if (address_value < region->base || offset > region->size ||
        width > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known_bytes = region->known ? region->known + (size_t)offset : 0;
    if (*known_bytes)
      for (byte = 0; byte < width; ++byte)
        if ((*known_bytes)[byte] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int address(Run *run, Word base, int32_t offset, uint32_t pc,
                   uint32_t *value) {
  Word effective = add(base, immediate((uint32_t)offset));
  if (effective.known_mask != 15) {
    stop(run, pc, effective.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *value = effective.word;
  return NBA97_TEXT_COMPLETE;
}

static int read_memory(Run *run, uint32_t address_value, unsigned width,
                       unsigned alignment, uint32_t pc, Word *value) {
  uint8_t *data;
  uint8_t *known_bytes;
  Word loaded = {0, 0};
  unsigned byte;
  TRY(locate(run, address_value, width, alignment, pc, &data, &known_bytes));
  for (byte = 0; byte < width; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (byte * 8u);
    if (!known_bytes || known_bytes[byte])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << byte));
  }
  *value = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, address_value, width, loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_memory(Run *run, uint32_t address_value, unsigned width,
                        unsigned alignment, uint32_t pc, Word value) {
  uint8_t *data;
  uint8_t *known_bytes;
  uint8_t required = (uint8_t)((1u << width) - 1u);
  unsigned byte;
  value.word &= width_mask(width);
  value.known_mask &= required;
  TRY(locate(run, address_value, width, alignment, pc, &data, &known_bytes));
  if (!known_bytes && value.known_mask != required)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0; byte < width; ++byte) {
    data[byte] = (uint8_t)(value.word >> (byte * 8u));
    if (known_bytes)
      known_bytes[byte] = (uint8_t)((value.known_mask >> byte) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address_value, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int load(Run *run, unsigned destination, unsigned base, int32_t offset,
                unsigned width, int sign, uint32_t pc) {
  uint32_t effective;
  Word value;
  TRY(address(run, R(base), offset, pc, &effective));
  TRY(read_memory(run, effective, width, width, pc, &value));
  value.word &= width_mask(width);
  value.known_mask &= (uint8_t)((1u << width) - 1u);
  if (sign && (value.word & (UINT32_C(1) << (width * 8u - 1u))))
    value.word |= ~width_mask(width);
  if (!sign)
    value.known_mask =
        (uint8_t)(value.known_mask | (15u ^ ((1u << width) - 1u)));
  else if (value.known_mask & (1u << (width - 1u)))
    value.known_mask =
        (uint8_t)(value.known_mask | (15u ^ ((1u << width) - 1u)));
  R(destination) = value;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int store(Run *run, unsigned source, unsigned base, int32_t offset,
                 unsigned width, uint32_t pc) {
  uint32_t effective;
  TRY(address(run, R(base), offset, pc, &effective));
  return write_memory(run, effective, width, width, pc, R(source));
}

static int invoke_zero(Run *run) {
  Nba97GameControllerProfileResetEvent event;
  int accepted;
  stop(run, UINT32_C(0x800834d8), 0, UINT32_C(0x800a3a74));
  TRY(spend(run));
  ++run->out->call_attempts;
  memset(&event, 0, sizeof event);
  event.pc = UINT32_C(0x800834d8);
  event.delay_slot_pc = UINT32_C(0x800834dc);
  event.entry = UINT32_C(0x800a3a74);
  event.operation = run->out->operations;
  event.invocation = run->out->call_attempts;
  event.kind = NBA97_GAME_CONTROLLER_PROFILE_RESET_ZERO_800A3A74;
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
  ++run->out->call_count;
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_controller_profile_reset(
    Nba97GameControllerProfileResetContext *context,
    Nba97GameControllerProfileResetProgress *out) {
  Run storage;
  Run *run = &storage;
  int condition;
  TRY(initialize(context, out, run));

  /* 0x80083490..0x800834C0: build the live frame and retained table bases. */
  STEP(0x80083490);
  SP = add(SP, immediate(UINT32_C(0xffffffd8)));
  out->frame_stack_pointer = SP.word;
  STEP(0x80083494);
  TRY(store(run, 19, 29, 0x1c, 4, UINT32_C(0x80083494)));
  STEP(0x80083498);
  S3 = A0;
  STEP(0x8008349c);
  TRY(store(run, 17, 29, 0x14, 4, UINT32_C(0x8008349c)));
  STEP(0x800834a0);
  known(&S1, 0);
  STEP(0x800834a4);
  TRY(store(run, 18, 29, 0x18, 4, UINT32_C(0x800834a4)));
  STEP(0x800834a8);
  known(&S2, UINT32_C(0x80020000));
  STEP(0x800834ac);
  S2 = add(S2, immediate(UINT32_C(0xffffef7c)));
  STEP(0x800834b0);
  TRY(store(run, 20, 29, 0x20, 4, UINT32_C(0x800834b0)));
  STEP(0x800834b4);
  S4 = add(S2, immediate(0x1ca0));
  STEP(0x800834b8);
  TRY(store(run, 31, 29, 0x24, 4, UINT32_C(0x800834b8)));
  STEP(0x800834bc);
  TRY(store(run, 16, 29, 0x10, 4, UINT32_C(0x800834bc)));
  STEP(0x800834c0);
  S0 = shift(S1, 16, 0, 0);

  for (;;) {
    ++out->records_started;
    /* 0x800834C4..0x800834DC: signed index times 120, then the exact
     * JAL/ORI pair that clears the record's 0x24-byte header. */
    STEP(0x800834c4);
    S0 = shift(S0, 16, 1, 1);
    STEP(0x800834c8);
    A0 = shift(S0, 4, 0, 0);
    STEP(0x800834cc);
    A0 = subtract(A0, S0);
    STEP(0x800834d0);
    A0 = shift(A0, 3, 0, 0);
    STEP(0x800834d4);
    A0 = add(A0, S2);
    STEP(0x800834d8);
    known(&RA, UINT32_C(0x800834e0));
    STEP(0x800834dc);
    known(&A1, 0x24);
    TRY(invoke_zero(run));

    /* 0x800834E0..0x80083508: read selection unconditionally. The override
     * branch and signed-negative branch both execute their delay slots. */
    STEP(0x800834e0);
    known(&AT, UINT32_C(0x80020000));
    STEP(0x800834e4);
    AT = add(AT, S0);
    STEP(0x800834e8);
    TRY(load(run, 2, 1, 0x1dde, 1, 0, UINT32_C(0x800834e8)));
    STEP(0x800834ec);
    STEP(0x800834f0);
    V0 = shift(V0, 24, 0, 0);
    STEP(0x800834f4);
    STEP(0x800834f8);
    V0 = shift(V0, 24, 1, 1);
    if (!zero_decision(S3, &condition)) {
      stop(run, UINT32_C(0x800834f4), 0, 0);
      return NBA97_TEXT_UNKNOWN;
    }
    if (condition) {
      STEP(0x800834fc);
      V0 = shift(V0, 16, 0, 0);
      STEP(0x80083500);
      V1 = shift(V0, 16, 1, 1);
      STEP(0x80083504);
      STEP(0x80083508);
      V0 = add(S1, immediate(1));
      if (!(V1.known_mask & 8u)) {
        stop(run, UINT32_C(0x80083504), 0, 0);
        return NBA97_TEXT_UNKNOWN;
      }
      if (V1.word & UINT32_C(0x80000000))
        goto advance_record;

      /* 0x8008350C..0x80083530: multiply selection by 108 and select the
       * profile tail only when its signed flag byte is nonzero. */
      STEP(0x8008350c);
      V0 = shift(V1, 3, 0, 0);
      STEP(0x80083510);
      V0 = subtract(V0, V1);
      STEP(0x80083514);
      V0 = shift(V0, 2, 0, 0);
      STEP(0x80083518);
      V0 = subtract(V0, V1);
      STEP(0x8008351c);
      V0 = shift(V0, 2, 0, 0);
      STEP(0x80083520);
      V1 = add(V0, S4);
      STEP(0x80083524);
      TRY(load(run, 2, 3, 0x6b, 1, 1, UINT32_C(0x80083524)));
      STEP(0x80083528);
      STEP(0x8008352c);
      STEP(0x80083530);
      A2 = add(V1, immediate(0x22));
      if (!zero_decision(V0, &condition)) {
        stop(run, UINT32_C(0x8008352c), 0, 0);
        return NBA97_TEXT_UNKNOWN;
      }
      if (!condition)
        goto profile_selected;
    }

    /* 0x80083534..0x80083538: override or zero flag selects the default. */
    STEP(0x80083534);
    known(&A2, UINT32_C(0x800c0000));
    STEP(0x80083538);
    A2 = add(A2, immediate(UINT32_C(0xffffc94c)));

  profile_selected:
    /* 0x8008353C..0x80083558: recompute destination from callback-live s1/s2.
     */
    STEP(0x8008353c);
    V1 = shift(S1, 16, 0, 0);
    STEP(0x80083540);
    V1 = shift(V1, 16, 1, 1);
    STEP(0x80083544);
    V0 = shift(V1, 4, 0, 0);
    STEP(0x80083548);
    V0 = subtract(V0, V1);
    STEP(0x8008354c);
    V0 = shift(V0, 3, 0, 0);
    STEP(0x80083550);
    V0 = add(V0, S2);
    STEP(0x80083554);
    A0 = add(V0, immediate(0x3c));
    STEP(0x80083558);
    known(&A1, 0);

    for (;;) {
      /* 0x8008355C..0x80083580: source load precedes destination store; the
       * branch-delay increment makes overlap a bytewise forward copy. */
      STEP(0x8008355c);
      TRY(load(run, 3, 6, 0, 1, 0, UINT32_C(0x8008355c)));
      STEP(0x80083560);
      V0 = add(A1, immediate(1));
      STEP(0x80083564);
      A1 = V0;
      STEP(0x80083568);
      A2 = add(A2, immediate(1));
      STEP(0x8008356c);
      V0 = shift(V0, 16, 0, 0);
      STEP(0x80083570);
      V0 = shift(V0, 16, 1, 1);
      STEP(0x80083574);
      V0 = signed_less_constant(V0, 0x3b);
      STEP(0x80083578);
      TRY(store(run, 3, 4, 0, 1, UINT32_C(0x80083578)));
      ++out->bytes_copied;
      STEP(0x8008357c);
      STEP(0x80083580);
      A0 = add(A0, immediate(1));
      if (!zero_decision(V0, &condition)) {
        stop(run, UINT32_C(0x8008357c), 0, 0);
        return NBA97_TEXT_UNKNOWN;
      }
      if (!condition)
        continue;
      break;
    }
    ++out->records_copied;
    STEP(0x80083584);
    V0 = add(S1, immediate(1));

  advance_record:
    /* 0x80083588..0x8008359C: signed-half loop condition and live next index.
     */
    STEP(0x80083588);
    S1 = V0;
    STEP(0x8008358c);
    V0 = shift(V0, 16, 0, 0);
    STEP(0x80083590);
    V0 = shift(V0, 16, 1, 1);
    STEP(0x80083594);
    V0 = signed_less_constant(V0, 8);
    STEP(0x80083598);
    STEP(0x8008359c);
    S0 = shift(S1, 16, 0, 0);
    if (!zero_decision(V0, &condition)) {
      stop(run, UINT32_C(0x80083598), 0, 0);
      return NBA97_TEXT_UNKNOWN;
    }
    if (!condition)
      continue;
    break;
  }

  /* 0x800835A0..0x800835C0: restore through callback-live sp and execute the
   * JR delay NOP before deciding whether the retained return PC is known. */
  STEP(0x800835a0);
  TRY(load(run, 31, 29, 0x24, 4, 0, UINT32_C(0x800835a0)));
  out->restored_return_address = RA;
  STEP(0x800835a4);
  TRY(load(run, 20, 29, 0x20, 4, 0, UINT32_C(0x800835a4)));
  out->restored_s4 = S4;
  STEP(0x800835a8);
  TRY(load(run, 19, 29, 0x1c, 4, 0, UINT32_C(0x800835a8)));
  out->restored_s3 = S3;
  STEP(0x800835ac);
  TRY(load(run, 18, 29, 0x18, 4, 0, UINT32_C(0x800835ac)));
  out->restored_s2 = S2;
  STEP(0x800835b0);
  TRY(load(run, 17, 29, 0x14, 4, 0, UINT32_C(0x800835b0)));
  out->restored_s1 = S1;
  STEP(0x800835b4);
  TRY(load(run, 16, 29, 0x10, 4, 0, UINT32_C(0x800835b4)));
  out->restored_s0 = S0;
  STEP(0x800835b8);
  SP = add(SP, immediate(0x28));
  STEP(0x800835bc);
  STEP(0x800835c0);
  if (RA.known_mask != 15) {
    stop(run, UINT32_C(0x800835bc), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
