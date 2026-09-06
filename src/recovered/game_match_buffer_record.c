#include "game_match_buffer_record.h"

#include <limits.h>
#include <string.h>

typedef Nba97GameMatchBufferRecordWord Word;

typedef struct Run {
  Nba97GameMatchBufferRecordContext *context;
  Nba97GameMatchBufferRecordProgress *out;
  Nba97GameMatchBufferRecordMachine machine;
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

static int valid_machine(const Nba97GameMatchBufferRecordMachine *machine) {
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

static int initialize(Nba97GameMatchBufferRecordContext *context,
                      Nba97GameMatchBufferRecordProgress *out, Run *run) {
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
    if (!a->data || !a->size || (uint64_t)a->size > UINT64_C(0x100000000) ||
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
    Nba97GameMatchBufferRecordAccess *event =
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
  journal(run, NBA97_GAME_MATCH_BUFFER_REWIND_READ, pc, address_value, width,
          loaded);
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
  journal(run, NBA97_GAME_MATCH_BUFFER_REWIND_STORE, pc, address_value, width,
          value);
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

static void unsigned_bounds(Word value, uint32_t *minimum, uint32_t *maximum) {
  unsigned byte;
  *minimum = 0;
  *maximum = 0;
  for (byte = 0; byte < 4; ++byte) {
    uint32_t part = (value.word >> (byte * 8u)) & 255u;
    *minimum |= ((value.known_mask & (1u << byte)) ? part : 0u) << (byte * 8u);
    *maximum |= ((value.known_mask & (1u << byte)) ? part : 255u)
                << (byte * 8u);
  }
}

static Word unsigned_less(Word left, Word right) {
  Word result;
  uint32_t left_min;
  uint32_t left_max;
  uint32_t right_min;
  uint32_t right_max;
  known(&result, left.word < right.word);
  unsigned_bounds(left, &left_min, &left_max);
  unsigned_bounds(right, &right_min, &right_max);
  if (left_max < right_min || left_min >= right_max)
    return result;
  result.known_mask = 14;
  return result;
}

static Word unsigned_less_constant(Word value, uint32_t limit) {
  return unsigned_less(value, immediate(limit));
}

static int equal_decision(Word left, Word right, int *equal_result) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((left.known_mask & right.known_mask & (1u << byte)) &&
        (((left.word ^ right.word) >> (byte * 8u)) & 255u)) {
      *equal_result = 0;
      return 1;
    }
  if (left.known_mask == 15 && right.known_mask == 15) {
    *equal_result = left.word == right.word;
    return 1;
  }
  return 0;
}

static int invoke(Run *run, uint32_t pc, uint32_t entry, uint8_t kind,
                  uint8_t argument_count) {
  Nba97GameMatchBufferRecordEvent event;
  int accepted;
  stop(run, pc, 0, entry);
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
  if (!valid_machine(&run->machine))
    return NBA97_TEXT_ARGUMENT;
  ++run->out->callbacks_completed;
  ++run->out->call_count[kind];
  return NBA97_TEXT_COMPLETE;
}

static int branch_zero(Run *run, Word value, uint32_t pc, int *is_zero) {
  if (!zero_decision(value, is_zero)) {
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  return NBA97_TEXT_COMPLETE;
}

static int branch_equal(Run *run, Word left, Word right, uint32_t pc,
                        int *is_equal) {
  if (!equal_decision(left, right, is_equal)) {
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_match_buffer_record(Nba97GameMatchBufferRecordContext *context,
                                   Nba97GameMatchBufferRecordProgress *out) {
  Run storage;
  Run *run = &storage;
  Word branch_value;
  int condition;
  TRY(initialize(context, out, run));

  /* 0x80076B3C..0x80076B54: save ra in the BEQ delay slot, then optionally
   * invoke the recovered rewind owner. */
  STEP(0x80076b3c);
  known(&V0, UINT32_C(0x80020000));
  STEP(0x80076b40);
  TRY(load(run, 2, 2, 0x148c, 2, 0, UINT32_C(0x80076b40)));
  STEP(0x80076b44);
  SP = add(SP, immediate(UINT32_C(0xffffffe8)));
  out->frame_stack_pointer = SP.word;
  STEP(0x80076b48);
  STEP(0x80076b4c);
  TRY(store(run, 31, 29, 0x10, 4, UINT32_C(0x80076b4c)));
  TRY(branch_zero(run, V0, UINT32_C(0x80076b48), &condition));
  if (!condition) {
    out->used_rewind = 1;
    STEP(0x80076b50);
    known(&RA, UINT32_C(0x80076b58));
    STEP(0x80076b54);
    TRY(invoke(run, UINT32_C(0x80076b50), UINT32_C(0x80076ad0),
               NBA97_GAME_MATCH_BUFFER_RECORD_REWIND_80076AD0, 0));
  }

  /* 0x80076B58..0x80076BA4: choose the current snapshot, record the low clock
   * byte, and clamp the signed auxiliary clock at zero before truncation. */
  STEP(0x80076b58);
  known(&V0, UINT32_C(0x80100000));
  STEP(0x80076b5c);
  TRY(load(run, 2, 2, UINT32_C(0xffff9ffc), 2, 1, UINT32_C(0x80076b5c)));
  STEP(0x80076b60);
  known(&A3, UINT32_C(0x800f0000));
  STEP(0x80076b64);
  A3 = add(A3, immediate(0x1918));
  STEP(0x80076b68);
  STEP(0x80076b6c);
  TRY(branch_zero(run, V0, UINT32_C(0x80076b68), &condition));
  if (condition) {
    STEP(0x80076b70);
    known(&A3, UINT32_C(0x800f0000));
    STEP(0x80076b74);
    A3 = add(A3, immediate(0x1814));
  }
  STEP(0x80076b78);
  known(&V1, UINT32_C(0x80100000));
  STEP(0x80076b7c);
  V1 = add(V1, immediate(UINT32_C(0xffffdb6c)));
  STEP(0x80076b80);
  TRY(load(run, 2, 3, 0, 2, 0, UINT32_C(0x80076b80)));
  STEP(0x80076b84);
  STEP(0x80076b88);
  TRY(store(run, 2, 7, 9, 1, UINT32_C(0x80076b88)));
  STEP(0x80076b8c);
  known(&A2, UINT32_C(0x80100000));
  STEP(0x80076b90);
  TRY(load(run, 6, 6, UINT32_C(0xffffdb94), 2, 1, UINT32_C(0x80076b90)));
  STEP(0x80076b94);
  STEP(0x80076b98);
  STEP(0x80076b9c);
  A1 = A3;
  if (!(A2.known_mask & 8u)) {
    stop(run, UINT32_C(0x80076b98), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (A2.word & UINT32_C(0x80000000)) {
    STEP(0x80076ba0);
    known(&A2, 0);
  }
  STEP(0x80076ba4);
  TRY(store(run, 6, 7, 8, 1, UINT32_C(0x80076ba4)));

  /* 0x80076BA8..0x80076BD0: record eight selected controller halfwords as
   * bytes. The pointer load and pointed LHU repeat for every controller. */
  STEP(0x80076ba8);
  known(&A2, 0);
  STEP(0x80076bac);
  A0 = add(V1, immediate(0xe4));
  for (;;) {
    STEP(0x80076bb0);
    TRY(load(run, 2, 4, 0, 4, 0, UINT32_C(0x80076bb0)));
    STEP(0x80076bb4);
    STEP(0x80076bb8);
    TRY(load(run, 3, 2, 0x26, 2, 0, UINT32_C(0x80076bb8)));
    STEP(0x80076bbc);
    V0 = add(A1, A2);
    STEP(0x80076bc0);
    A2 = add(A2, immediate(1));
    STEP(0x80076bc4);
    TRY(store(run, 3, 2, 0x0a, 1, UINT32_C(0x80076bc4)));
    STEP(0x80076bc8);
    V0 = signed_less_constant(A2, 8);
    STEP(0x80076bcc);
    STEP(0x80076bd0);
    A0 = add(A0, immediate(4));
    TRY(branch_zero(run, V0, UINT32_C(0x80076bcc), &condition));
    if (!condition)
      continue;
    break;
  }

  /* 0x80076BD4..0x80076CAC: copy retained globals with their exact source
   * truncation and logical-shift behavior. */
#define LOAD_ABSOLUTE(reg_, upper_, offset_, width_, sign_, pc_)               \
  do {                                                                         \
    STEP(pc_);                                                                 \
    known(&R(reg_), (upper_));                                                 \
    STEP((pc_) + 4u);                                                          \
    TRY(load(run, (reg_), (reg_), (offset_), (width_), (sign_), (pc_) + 4u));  \
    STEP((pc_) + 8u);                                                          \
  } while (0)
  LOAD_ABSOLUTE(2, UINT32_C(0x80100000), UINT32_C(0xffffdbcc), 2, 0,
                UINT32_C(0x80076bd4));
  STEP(0x80076be0);
  TRY(store(run, 2, 5, 0x12, 1, UINT32_C(0x80076be0)));
  LOAD_ABSOLUTE(2, UINT32_C(0x80100000), UINT32_C(0xffffdb58), 4, 0,
                UINT32_C(0x80076be4));
  STEP(0x80076bf0);
  TRY(store(run, 2, 5, 0x14, 2, UINT32_C(0x80076bf0)));
  LOAD_ABSOLUTE(2, UINT32_C(0x80100000), UINT32_C(0xffffdba4), 4, 0,
                UINT32_C(0x80076bf4));
  STEP(0x80076c00);
  TRY(store(run, 2, 5, 0x16, 2, UINT32_C(0x80076c00)));
  LOAD_ABSOLUTE(2, UINT32_C(0x80100000), UINT32_C(0xffffdb90), 2, 0,
                UINT32_C(0x80076c04));
  STEP(0x80076c10);
  TRY(store(run, 2, 5, 0x1e, 1, UINT32_C(0x80076c10)));
  LOAD_ABSOLUTE(2, UINT32_C(0x80020000), UINT32_C(0xffffee46), 2, 0,
                UINT32_C(0x80076c14));
  STEP(0x80076c20);
  TRY(store(run, 2, 5, 0x1f, 1, UINT32_C(0x80076c20)));
  LOAD_ABSOLUTE(2, UINT32_C(0x80020000), UINT32_C(0xffffef0a), 2, 0,
                UINT32_C(0x80076c24));
  STEP(0x80076c30);
  TRY(store(run, 2, 5, 0x20, 1, UINT32_C(0x80076c30)));
  LOAD_ABSOLUTE(2, UINT32_C(0x80100000), UINT32_C(0xffffdc38), 4, 0,
                UINT32_C(0x80076c34));
  STEP(0x80076c40);
  TRY(store(run, 2, 5, 4, 4, UINT32_C(0x80076c40)));
  LOAD_ABSOLUTE(2, UINT32_C(0x800b0000), 0x7a00, 4, 0, UINT32_C(0x80076c44));
  STEP(0x80076c50);
  TRY(store(run, 2, 5, 0x21, 1, UINT32_C(0x80076c50)));
  LOAD_ABSOLUTE(2, UINT32_C(0x800b0000), 0x7a04, 4, 0, UINT32_C(0x80076c54));
  STEP(0x80076c60);
  TRY(store(run, 2, 5, 0x22, 1, UINT32_C(0x80076c60)));
  LOAD_ABSOLUTE(2, UINT32_C(0x800b0000), 0x7a08, 4, 0, UINT32_C(0x80076c64));
  STEP(0x80076c70);
  TRY(store(run, 2, 5, 0x23, 1, UINT32_C(0x80076c70)));
  LOAD_ABSOLUTE(2, UINT32_C(0x800b0000), 0x7a0c, 4, 0, UINT32_C(0x80076c74));
  STEP(0x80076c80);
  TRY(store(run, 2, 5, 0x24, 1, UINT32_C(0x80076c80)));
  LOAD_ABSOLUTE(2, UINT32_C(0x80100000), 0x76e6, 2, 0, UINT32_C(0x80076c84));
  STEP(0x80076c90);
  V0 = shift(V0, 4, 1, 0);
  STEP(0x80076c94);
  TRY(store(run, 2, 5, 0x25, 1, UINT32_C(0x80076c94)));
  LOAD_ABSOLUTE(2, UINT32_C(0x80110000), UINT32_C(0xffff8a0a), 2, 0,
                UINT32_C(0x80076c98));
  STEP(0x80076ca4);
  V0 = shift(V0, 4, 1, 0);
  STEP(0x80076ca8);
  TRY(store(run, 2, 5, 0x26, 1, UINT32_C(0x80076ca8)));

  /* 0x80076CAC..0x80076D00: reread the live ball pointer for each position,
   * then consume and clear the frame accumulator. */
  STEP(0x80076cac);
  known(&V0, UINT32_C(0x80020000));
  STEP(0x80076cb0);
  TRY(load(run, 2, 2, 0x0c14, 4, 0, UINT32_C(0x80076cb0)));
  STEP(0x80076cb4);
  STEP(0x80076cb8);
  TRY(load(run, 2, 2, 0x14, 2, 0, UINT32_C(0x80076cb8)));
  STEP(0x80076cbc);
  STEP(0x80076cc0);
  TRY(store(run, 2, 5, 0x18, 2, UINT32_C(0x80076cc0)));
  STEP(0x80076cc4);
  known(&V0, UINT32_C(0x80020000));
  STEP(0x80076cc8);
  TRY(load(run, 2, 2, 0x0c14, 4, 0, UINT32_C(0x80076cc8)));
  STEP(0x80076ccc);
  STEP(0x80076cd0);
  TRY(load(run, 2, 2, 0x16, 2, 0, UINT32_C(0x80076cd0)));
  STEP(0x80076cd4);
  STEP(0x80076cd8);
  TRY(store(run, 2, 5, 0x1a, 2, UINT32_C(0x80076cd8)));
  STEP(0x80076cdc);
  known(&V0, UINT32_C(0x80020000));
  STEP(0x80076ce0);
  TRY(load(run, 2, 2, 0x0c14, 4, 0, UINT32_C(0x80076ce0)));
  STEP(0x80076ce4);
  STEP(0x80076ce8);
  TRY(load(run, 2, 2, 0x18, 2, 0, UINT32_C(0x80076ce8)));
  STEP(0x80076cec);
  STEP(0x80076cf0);
  TRY(store(run, 2, 5, 0x1c, 2, UINT32_C(0x80076cf0)));
  LOAD_ABSOLUTE(2, UINT32_C(0x80100000), UINT32_C(0xffffe860), 4, 0,
                UINT32_C(0x80076cf4));
  STEP(0x80076d00);
  TRY(store(run, 2, 5, 0, 4, UINT32_C(0x80076d00)));
  STEP(0x80076d04);
  known(&V0, UINT32_C(0x80020000));
  STEP(0x80076d08);
  TRY(load(run, 2, 2, 0x0bec, 4, 0, UINT32_C(0x80076d08)));
  STEP(0x80076d0c);
  known(&A2, 0);
  STEP(0x80076d10);
  A0 = add(A3, immediate(0x2c));
  STEP(0x80076d14);
  A1 = add(A3, immediate(0x28));
  STEP(0x80076d18);
  known(&AT, UINT32_C(0x80100000));
  STEP(0x80076d1c);
  TRY(store(run, 0, 1, UINT32_C(0xffffe860), 4, UINT32_C(0x80076d1c)));
  STEP(0x80076d20);
  V1 = add(V0, immediate(0x10));

  /* 0x80076D24..0x80076E14: serialize eleven physical entities. */
  for (;;) {
    ++out->entity_iterations;
    STEP(0x80076d24);
    V0 = signed_less_constant(A2, 10);
    STEP(0x80076d28);
    STEP(0x80076d2c);
    TRY(branch_zero(run, V0, UINT32_C(0x80076d28), &condition));
    if (!condition) {
#define ENTITY_BYTE(source_offset_, destination_offset_, load_pc_, store_pc_)  \
  do {                                                                         \
    STEP(load_pc_);                                                            \
    TRY(load(run, 2, 3, (source_offset_), 2, 0, (load_pc_)));                  \
    STEP((load_pc_) + 4u);                                                     \
    STEP(store_pc_);                                                           \
    TRY(store(run, 2, 4, (destination_offset_), 1, (store_pc_)));              \
  } while (0)
      ENTITY_BYTE(0x74, 2, UINT32_C(0x80076d30), UINT32_C(0x80076d38));
      ENTITY_BYTE(0x7c, 3, UINT32_C(0x80076d3c), UINT32_C(0x80076d44));
      ENTITY_BYTE(0x76, 4, UINT32_C(0x80076d48), UINT32_C(0x80076d50));
      ENTITY_BYTE(0x7e, 5, UINT32_C(0x80076d54), UINT32_C(0x80076d5c));
      ENTITY_BYTE(0x78, 6, UINT32_C(0x80076d60), UINT32_C(0x80076d68));
      ENTITY_BYTE(0x80, 7, UINT32_C(0x80076d6c), UINT32_C(0x80076d74));
      ENTITY_BYTE(0x7a, 8, UINT32_C(0x80076d78), UINT32_C(0x80076d80));
      ENTITY_BYTE(0x82, 9, UINT32_C(0x80076d84), UINT32_C(0x80076d8c));
      ENTITY_BYTE(0x84, 0x0a, UINT32_C(0x80076d90), UINT32_C(0x80076d98));
      ENTITY_BYTE(0x86, 0x0b, UINT32_C(0x80076d9c), UINT32_C(0x80076da4));
#undef ENTITY_BYTE
      STEP(0x80076da8);
      TRY(load(run, 2, 3, 0x98, 2, 0, UINT32_C(0x80076da8)));
      STEP(0x80076dac);
      STEP(0x80076db0);
      V0 = shift(V0, 2, 1, 0);
      STEP(0x80076db4);
      TRY(store(run, 2, 4, 0x0c, 1, UINT32_C(0x80076db4)));
      STEP(0x80076db8);
      TRY(load(run, 2, 3, 0x88, 2, 0, UINT32_C(0x80076db8)));
      STEP(0x80076dbc);
      STEP(0x80076dc0);
      V0 = shift(V0, 2, 1, 0);
      STEP(0x80076dc4);
      TRY(store(run, 2, 4, 0x0d, 1, UINT32_C(0x80076dc4)));
      STEP(0x80076dc8);
      TRY(load(run, 2, 3, 0x8a, 2, 0, UINT32_C(0x80076dc8)));
      STEP(0x80076dcc);
      STEP(0x80076dd0);
      TRY(store(run, 2, 4, 0x0e, 1, UINT32_C(0x80076dd0)));
    }

    STEP(0x80076dd4);
    TRY(load(run, 2, 3, -8, 4, 0, UINT32_C(0x80076dd4)));
    STEP(0x80076dd8);
    STEP(0x80076ddc);
    V0 = shift(V0, 8, 1, 1);
    STEP(0x80076de0);
    TRY(store(run, 2, 5, 0, 2, UINT32_C(0x80076de0)));
    STEP(0x80076de4);
    TRY(load(run, 2, 3, -4, 4, 0, UINT32_C(0x80076de4)));
    STEP(0x80076de8);
    STEP(0x80076dec);
    V0 = shift(V0, 8, 1, 1);
    STEP(0x80076df0);
    TRY(store(run, 2, 4, -2, 2, UINT32_C(0x80076df0)));
    STEP(0x80076df4);
    TRY(load(run, 2, 3, 0, 4, 0, UINT32_C(0x80076df4)));
    STEP(0x80076df8);
    A2 = add(A2, immediate(1));
    STEP(0x80076dfc);
    A1 = add(A1, immediate(0x14));
    STEP(0x80076e00);
    V1 = add(V1, immediate(0xf4));
    STEP(0x80076e04);
    V0 = shift(V0, 8, 1, 1);
    STEP(0x80076e08);
    TRY(store(run, 2, 4, 0, 2, UINT32_C(0x80076e08)));
    STEP(0x80076e0c);
    V0 = signed_less_constant(A2, 11);
    STEP(0x80076e10);
    STEP(0x80076e14);
    A0 = add(A0, immediate(0x14));
    TRY(branch_zero(run, V0, UINT32_C(0x80076e10), &condition));
    if (!condition)
      continue;
    break;
  }

  /* 0x80076E18..0x80076E5C: select old/new snapshots, then call the typed
   * compression child with a3 assigned in the JAL delay slot. */
  STEP(0x80076e18);
  known(&V0, UINT32_C(0x80100000));
  STEP(0x80076e1c);
  TRY(load(run, 2, 2, UINT32_C(0xffff9ffc), 2, 1, UINT32_C(0x80076e1c)));
  STEP(0x80076e20);
  known(&A2, UINT32_C(0x80100000));
  STEP(0x80076e24);
  TRY(load(run, 6, 6, UINT32_C(0xffffa010), 4, 0, UINT32_C(0x80076e24)));
  STEP(0x80076e28);
  STEP(0x80076e2c);
  TRY(branch_zero(run, V0, UINT32_C(0x80076e28), &condition));
  if (condition) {
    STEP(0x80076e30);
    known(&A0, UINT32_C(0x800f0000));
    STEP(0x80076e34);
    A0 = add(A0, immediate(0x1814));
    STEP(0x80076e38);
    known(&A1, UINT32_C(0x800f0000));
    STEP(0x80076e3c);
    A1 = add(A1, immediate(0x1918));
    STEP(0x80076e40);
    STEP(0x80076e44);
  } else {
    STEP(0x80076e48);
    known(&A0, UINT32_C(0x800f0000));
    STEP(0x80076e4c);
    A0 = add(A0, immediate(0x1918));
    STEP(0x80076e50);
    known(&A1, UINT32_C(0x800f0000));
    STEP(0x80076e54);
    A1 = add(A1, immediate(0x1814));
  }
  STEP(0x80076e58);
  known(&RA, UINT32_C(0x80076e60));
  STEP(0x80076e5c);
  known(&A3, 0x82);
  TRY(invoke(run, UINT32_C(0x80076e58), UINT32_C(0x800767fc),
             NBA97_GAME_MATCH_BUFFER_RECORD_COMPRESS_800767FC, 4));

  /* 0x80076E60..0x80076E9C: publish the returned cursor, wrapping the
   * unsigned limit comparison exactly, then gate the encoded-record walk. */
  STEP(0x80076e60);
  A2 = V0;
  STEP(0x80076e64);
  known(&V0, UINT32_C(0x80100000));
  STEP(0x80076e68);
  TRY(load(run, 2, 2, UINT32_C(0xffffa008), 4, 0, UINT32_C(0x80076e68)));
  STEP(0x80076e6c);
  V1 = add(A2, immediate(0x12e));
  STEP(0x80076e70);
  V0 = unsigned_less(V0, V1);
  STEP(0x80076e74);
  STEP(0x80076e78);
  TRY(branch_zero(run, V0, UINT32_C(0x80076e74), &condition));
  if (!condition) {
    STEP(0x80076e7c);
    known(&AT, UINT32_C(0x80100000));
    STEP(0x80076e80);
    TRY(store(run, 6, 1, UINT32_C(0xffffa014), 4, UINT32_C(0x80076e80)));
    STEP(0x80076e84);
    known(&A2, UINT32_C(0x80100000));
    STEP(0x80076e88);
    TRY(load(run, 6, 6, UINT32_C(0xffffa004), 4, 0, UINT32_C(0x80076e88)));
  }
  LOAD_ABSOLUTE(2, UINT32_C(0x80100000), UINT32_C(0xffffa014), 4, 0,
                UINT32_C(0x80076e8c));
  STEP(0x80076e98);
  STEP(0x80076e9c);
  TRY(branch_zero(run, V0, UINT32_C(0x80076e98), &condition));
  if (condition)
    goto finish;

  /* 0x80076EA0..0x80076ECC: establish the current cursor and two source
   * overlap gates before inspecting variable-length records. */
  STEP(0x80076ea0);
  known(&A1, UINT32_C(0x80100000));
  STEP(0x80076ea4);
  TRY(load(run, 5, 5, UINT32_C(0xffffa00c), 4, 0, UINT32_C(0x80076ea4)));
  LOAD_ABSOLUTE(2, UINT32_C(0x80100000), UINT32_C(0xffffa004), 4, 0,
                UINT32_C(0x80076ea8));
  STEP(0x80076eb4);
  STEP(0x80076eb8);
  TRY(branch_equal(run, A1, V0, UINT32_C(0x80076eb4), &condition));
  if (condition) {
    STEP(0x80076ebc);
    STEP(0x80076ec0);
    TRY(branch_equal(run, A1, A2, UINT32_C(0x80076ebc), &condition));
    if (!condition)
      goto finish;
  }

  STEP(0x80076ec4);
  V0 = add(A2, immediate(0x12e));
cursor_compare:
  ++out->cursor_iterations;
  STEP(0x80076ec8);
  V0 = unsigned_less(A1, V0);
  STEP(0x80076ecc);
  STEP(0x80076ed0);
  TRY(branch_zero(run, V0, UINT32_C(0x80076ecc), &condition));
  if (condition)
    goto publish_cursor;

  STEP(0x80076ed4);
  TRY(load(run, 3, 5, 0, 1, 0, UINT32_C(0x80076ed4)));
  STEP(0x80076ed8);
  STEP(0x80076edc);
  V0 = add(V1, immediate(UINT32_C(0xffffffde)));
  STEP(0x80076ee0);
  V0 = unsigned_less_constant(V0, 0x10d);
  STEP(0x80076ee4);
  STEP(0x80076ee8);
  TRY(branch_zero(run, V0, UINT32_C(0x80076ee4), &condition));
  if (!condition)
    goto advance_encoded;

  /* 0x80076EEC..0x80076F54: walk backward marker pairs for short records.
   * Every byte read and unsigned pointer gate remains separately observable. */
  STEP(0x80076eec);
  known(&A3, UINT32_C(0x80100000));
  STEP(0x80076ef0);
  TRY(load(run, 7, 7, UINT32_C(0xffffa014), 4, 0, UINT32_C(0x80076ef0)));
  STEP(0x80076ef4);
  STEP(0x80076ef8);
  A0 = add(A3, immediate(UINT32_MAX));
  STEP(0x80076efc);
  V0 = unsigned_less(A1, A0);
  STEP(0x80076f00);
  STEP(0x80076f04);
  TRY(branch_zero(run, V0, UINT32_C(0x80076f00), &condition));
  if (condition)
    goto settle_backward;
  STEP(0x80076f08);
  TRY(load(run, 2, 7, -1, 1, 0, UINT32_C(0x80076f08)));
  STEP(0x80076f0c);
  STEP(0x80076f10);
  V1 = subtract(A0, V0);

backward_marker:
  STEP(0x80076f48);
  TRY(load(run, 3, 3, 1, 1, 0, UINT32_C(0x80076f48)));
  STEP(0x80076f4c);
  STEP(0x80076f50);
  STEP(0x80076f54);
  TRY(branch_equal(run, V0, V1, UINT32_C(0x80076f50), &condition));
  if (!condition)
    goto settle_backward;

  STEP(0x80076f14);
  TRY(load(run, 3, 4, 0, 1, 0, UINT32_C(0x80076f14)));
  STEP(0x80076f18);
  STEP(0x80076f1c);
  V0 = unsigned_less_constant(V1, 0x22);
  STEP(0x80076f20);
  STEP(0x80076f24);
  TRY(branch_zero(run, V0, UINT32_C(0x80076f20), &condition));
  if (!condition)
    goto settle_backward;
  STEP(0x80076f28);
  A3 = add(A0, immediate(1));
  STEP(0x80076f2c);
  A0 = subtract(A0, V1);
  STEP(0x80076f30);
  V0 = unsigned_less(A1, A0);
  STEP(0x80076f34);
  STEP(0x80076f38);
  TRY(branch_zero(run, V0, UINT32_C(0x80076f34), &condition));
  if (condition)
    goto settle_backward;
  STEP(0x80076f3c);
  TRY(load(run, 2, 4, 0, 1, 0, UINT32_C(0x80076f3c)));
  STEP(0x80076f40);
  STEP(0x80076f44);
  V1 = subtract(A0, V0);
  goto backward_marker;

settle_backward:
  LOAD_ABSOLUTE(2, UINT32_C(0x80100000), UINT32_C(0xffffa014), 4, 0,
                UINT32_C(0x80076f58));
  STEP(0x80076f64);
  STEP(0x80076f68);
  A1 = A3;
  TRY(branch_equal(run, A3, V0, UINT32_C(0x80076f64), &condition));
  if (condition)
    goto reset_cursor;
  STEP(0x80076f6c);
  TRY(load(run, 3, 5, 0, 1, 0, UINT32_C(0x80076f6c)));

advance_encoded:
  STEP(0x80076f70);
  known(&V0, UINT32_C(0x80100000));
  STEP(0x80076f74);
  TRY(load(run, 2, 2, UINT32_C(0xffffa014), 4, 0, UINT32_C(0x80076f74)));
  STEP(0x80076f78);
  A1 = add(A1, V1);
  STEP(0x80076f7c);
  V0 = unsigned_less(A1, V0);
  STEP(0x80076f80);
  branch_value = V0;
  STEP(0x80076f84);
  V0 = add(A2, immediate(0x12e));
  TRY(branch_zero(run, branch_value, UINT32_C(0x80076f80), &condition));
  if (!condition)
    goto cursor_compare;

reset_cursor:
  STEP(0x80076f88);
  known(&V0, UINT32_C(0x80100000));
  STEP(0x80076f8c);
  TRY(load(run, 2, 2, UINT32_C(0xffffa004), 4, 0, UINT32_C(0x80076f8c)));
  STEP(0x80076f90);
  known(&AT, UINT32_C(0x80100000));
  STEP(0x80076f94);
  TRY(store(run, 2, 1, UINT32_C(0xffffa00c), 4, UINT32_C(0x80076f94)));
  STEP(0x80076f98);
  STEP(0x80076f9c);
  goto finish;

publish_cursor:
  STEP(0x80076fa0);
  known(&AT, UINT32_C(0x80100000));
  STEP(0x80076fa4);
  TRY(store(run, 5, 1, UINT32_C(0xffffa00c), 4, UINT32_C(0x80076fa4)));

finish:
  /* 0x80076FA8..0x80076FC4: publish the compression result, clear the flag,
   * and complete JR only after its NOP with callback-live sp/ra. */
  STEP(0x80076fa8);
  known(&AT, UINT32_C(0x80100000));
  STEP(0x80076fac);
  TRY(store(run, 6, 1, UINT32_C(0xffffa010), 4, UINT32_C(0x80076fac)));
  STEP(0x80076fb0);
  known(&AT, UINT32_C(0x80100000));
  STEP(0x80076fb4);
  TRY(store(run, 0, 1, UINT32_C(0xffffe864), 1, UINT32_C(0x80076fb4)));
  STEP(0x80076fb8);
  TRY(load(run, 31, 29, 0x10, 4, 0, UINT32_C(0x80076fb8)));
  out->restored_return_address = RA;
  STEP(0x80076fbc);
  SP = add(SP, immediate(0x18));
  STEP(0x80076fc0);
  STEP(0x80076fc4);
  if (RA.known_mask != 15) {
    stop(run, UINT32_C(0x80076fc0), RA.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, UINT32_C(0x80076fc0), RA.word, 0);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
#undef LOAD_ABSOLUTE
