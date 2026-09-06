#include "game_image_record_upload.h"

#include <limits.h>
#include <string.h>

typedef Nba97GameImageRecordUploadWord Word;

typedef struct Run {
  Nba97GameImageRecordUploadContext *context;
  Nba97GameImageRecordUploadProgress *out;
  Nba97GameImageRecordUploadMachine machine;
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

static int machine_valid(const Nba97GameImageRecordUploadMachine *machine) {
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

static int initialize(Nba97GameImageRecordUploadContext *context,
                      Nba97GameImageRecordUploadProgress *out, Run *run) {
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

/* Enumerating byte carries retains every invariant result byte. */
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
    Nba97GameImageRecordUploadAccess *event =
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
  stop(run, pc, address, 0);
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
  journal(run, NBA97_GAME_IMAGE_RECORD_UPLOAD_READ, pc, address, width, loaded);
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
  journal(run, NBA97_GAME_IMAGE_RECORD_UPLOAD_STORE, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int address(Run *run, Word base, uint32_t offset, uint32_t pc,
                   uint32_t *effective) {
  Word value = add(base, immediate(offset));
  if (value.known_mask != 15) {
    stop(run, pc, value.word, 0);
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

static int32_t signed_word(uint32_t value);
static void multiply_signed(Run *run, Word left, Word right) {
  uint64_t bits = (uint64_t)((int64_t)signed_word(left.word) *
                             (int64_t)signed_word(right.word));
  unsigned byte;
  run->machine.lo.word = (uint32_t)bits;
  run->machine.hi.word = (uint32_t)(bits >> 32u);
  run->machine.lo.known_mask = 0;
  run->machine.hi.known_mask = 0;
  if (left.known_mask == 15 && right.known_mask == 15) {
    run->machine.lo.known_mask = 15;
    run->machine.hi.known_mask = 15;
  } else {
    for (byte = 0; byte < 4; ++byte) {
      uint8_t prefix = (uint8_t)((1u << (byte + 1u)) - 1u);
      if ((left.known_mask & prefix) == prefix &&
          (right.known_mask & prefix) == prefix)
        run->machine.lo.known_mask =
            (uint8_t)(run->machine.lo.known_mask | (1u << byte));
    }
  }
  publish(run);
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
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

static Word sign_extend_half(Word value) {
  value.word = (value.word & 0x8000u) ? value.word | UINT32_C(0xffff0000)
                                      : value.word & UINT32_C(0x0000ffff);
  value.known_mask =
      (uint8_t)((value.known_mask & 3u) | ((value.known_mask & 2u) ? 12u : 0u));
  return value;
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

static Word or_constant(Word value, uint32_t constant) {
  Word result;
  unsigned byte;
  result.word = value.word | constant;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    uint32_t bits = (constant >> (byte * 8u)) & 255u;
    if ((value.known_mask & (1u << byte)) || bits == 255u)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
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

static int32_t signed_word(uint32_t value) {
  return value <= (uint32_t)INT32_MAX ? (int32_t)value
                                      : -1 - (int32_t)(UINT32_MAX - value);
}

static int equal_decision(Run *run, Word left, Word right, uint32_t pc,
                          int *equal) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((left.known_mask & right.known_mask & (1u << byte)) &&
        (((left.word ^ right.word) >> (byte * 8u)) & 255u)) {
      *equal = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (left.known_mask == 15 && right.known_mask == 15) {
    *equal = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

static int invoke(Run *run, uint8_t kind, uint32_t pc, uint32_t delay_pc,
                  uint32_t entry, uint8_t argument_count) {
  Nba97GameImageRecordUploadEvent event;
  int accepted;
  stop(run, pc, 0, entry);
  TRY(spend(run));
  ++run->out->call_attempts[kind];
  memset(&event, 0, sizeof event);
  event.pc = pc;
  event.delay_slot_pc = delay_pc;
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

static Word unsigned_less_immediate(Word value, uint32_t limit) {
  Word result;
  known(&result, value.word < limit);
  if (value.known_mask != 15)
    result.known_mask = 14;
  return result;
}

static Word or_words(Word left, Word right) {
  Word result;
  unsigned byte;
  result.word = left.word | right.word;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    uint8_t bit = (uint8_t)(1u << byte);
    uint32_t left_byte = (left.word >> (byte * 8u)) & 255u;
    uint32_t right_byte = (right.word >> (byte * 8u)) & 255u;
    if (((left.known_mask & bit) && (right.known_mask & bit)) ||
        ((left.known_mask & bit) && left_byte == 255u) ||
        ((right.known_mask & bit) && right_byte == 255u))
      result.known_mask = (uint8_t)(result.known_mask | bit);
  }
  return result;
}

static int nonnegative_decision(Run *run, Word value, uint32_t pc,
                                int *nonnegative) {
  if (value.known_mask & 8u) {
    *nonnegative = (value.word & UINT32_C(0x80000000)) == 0;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

int nba97_game_image_record_upload(Nba97GameImageRecordUploadContext *context,
                                   Nba97GameImageRecordUploadProgress *out) {
  Run storage;
  Run *run = &storage;
  Word predicate;
  int decision;
  TRY(initialize(context, out, run));

  /* 0x80094540..0x80094570: allocate the frame, fetch the fifth argument
   * from the caller's stack, save s0-s4, and save ra in the BEQ delay. */
  STEP(0x80094540);
  SP = add(SP, immediate(UINT32_C(0xffffffd0)));
  out->frame_stack_pointer = SP.word;
  STEP(0x80094544);
  TRY(store(run, 20, 29, 0x28, 4, 4, 0x80094544));
  STEP(0x80094548);
  TRY(load(run, 20, 29, 0x40, 4, 4, 0x80094548));
  STEP(0x8009454c);
  TRY(store(run, 16, 29, 0x18, 4, 4, 0x8009454c));
  STEP(0x80094550);
  S0 = A0;
  STEP(0x80094554);
  TRY(store(run, 18, 29, 0x20, 4, 4, 0x80094554));
  STEP(0x80094558);
  S2 = A1;
  STEP(0x8009455c);
  TRY(store(run, 19, 29, 0x24, 4, 4, 0x8009455c));
  STEP(0x80094560);
  S3 = A2;
  STEP(0x80094564);
  TRY(store(run, 17, 29, 0x1c, 4, 4, 0x80094564));
  STEP(0x80094568);
  S1 = A3;
  STEP(0x8009456c);
  STEP(0x80094570);
  out->saved_return_address = RA;
  TRY(store(run, 31, 29, 0x2c, 4, 4, 0x80094570));
  TRY(zero_decision(run, S0, 0x8009456c, &decision));
  if (decision)
    goto epilogue;

record_loop:
  ++out->records_visited;
  /* 0x80094574..0x800945A0: mask only header bit 3, select type 0x23 or
   * 0x40..0x43, and preserve a0=s0 from the final branch delay. */
  STEP(0x80094574);
  TRY(load(run, 2, 16, 0, 1, 1, 0x80094574));
  STEP(0x80094578);
  STEP(0x8009457c);
  V1 = and_constant(V0, 0xf7u);
  STEP(0x80094580);
  known(&V0, 0x23u);
  STEP(0x80094584);
  STEP(0x80094588);
  V0 = unsigned_less_immediate(V1, 0x23u);
  TRY(equal_decision(run, V1, immediate(0x23u), 0x80094584, &decision));
  if (decision)
    goto type_23;

  STEP(0x8009458c);
  predicate = V0;
  STEP(0x80094590);
  V0 = unsigned_less_immediate(V1, 0x44u);
  TRY(zero_decision(run, predicate, 0x8009458c, &decision));
  if (!decision)
    goto follow_link;
  STEP(0x80094594);
  predicate = V0;
  STEP(0x80094598);
  V0 = unsigned_less_immediate(V1, 0x40u);
  TRY(zero_decision(run, predicate, 0x80094594, &decision));
  if (decision)
    goto follow_link;
  STEP(0x8009459c);
  predicate = V0;
  STEP(0x800945a0);
  A0 = S0;
  TRY(zero_decision(run, predicate, 0x8009459c, &decision));
  if (!decision)
    goto follow_link;

  /* 0x800945A4..0x80094608: update a 0x40..0x43 record, ask 0x800A3BF8
   * for the row count, retain its MULT HI/LO, and build the descriptor. */
  STEP(0x800945a4);
  TRY(load(run, 2, 16, 0x0c, 2, 2, 0x800945a4));
  STEP(0x800945a8);
  TRY(load(run, 3, 16, 0, 1, 1, 0x800945a8));
  STEP(0x800945ac);
  TRY(store(run, 19, 16, 0x0e, 2, 2, 0x800945ac));
  STEP(0x800945b0);
  V0 = and_constant(V0, 0xc000u);
  STEP(0x800945b4);
  V0 = or_words(S2, V0);
  STEP(0x800945b8);
  V1 = or_constant(V1, 8u);
  STEP(0x800945bc);
  TRY(store(run, 2, 16, 0x0c, 2, 2, 0x800945bc));
  STEP(0x800945c0);
  TRY(store(run, 3, 16, 0, 1, 1, 0x800945c0));
  STEP(0x800945c4);
  TRY(store(run, 18, 29, 0x10, 2, 2, 0x800945c4));
  STEP(0x800945c8);
  known(&RA, 0x800945d0u);
  STEP(0x800945cc);
  TRY(store(run, 19, 29, 0x12, 2, 2, 0x800945cc));
  TRY(invoke(run, NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800A3BF8,
             0x800945c8, 0x800945cc, 0x800a3bf8, 1));
  STEP(0x800945d0);
  TRY(load(run, 3, 16, 4, 2, 2, 0x800945d0));
  V1 = sign_extend_half(V1);
  STEP(0x800945d4);
  STEP(0x800945d8);
  multiply_signed(run, V1, V0);
  STEP(0x800945dc);
  V1 = run->machine.lo;
  STEP(0x800945e0);
  V0 = add(V1, immediate(0x0fu));
  STEP(0x800945e4);
  predicate = V0;
  STEP(0x800945e8);
  V0 = shift_right_arithmetic(V0, 4);
  TRY(nonnegative_decision(run, predicate, 0x800945e4, &decision));
  if (!decision) {
    STEP(0x800945ec);
    V0 = add(V1, immediate(0x1eu));
    STEP(0x800945f0);
    V0 = shift_right_arithmetic(V0, 4);
  }
  STEP(0x800945f4);
  TRY(store(run, 2, 29, 0x14, 2, 2, 0x800945f4));
  STEP(0x800945f8);
  TRY(load(run, 2, 16, 6, 2, 2, 0x800945f8));
  STEP(0x800945fc);
  A0 = add(SP, immediate(0x10u));
  STEP(0x80094600);
  A1 = add(S0, immediate(0x10u));
  STEP(0x80094604);
  STEP(0x80094608);
  TRY(store(run, 2, 29, 0x16, 2, 2, 0x80094608));
  goto upload;

type_23:
  /* 0x8009460C..0x80094640: update type 0x23 from full s1/s4 inputs. The
   * first zero branch stores the record width in its delay slot. */
  STEP(0x8009460c);
  TRY(load(run, 2, 16, 0, 1, 1, 0x8009460c));
  STEP(0x80094610);
  TRY(store(run, 17, 16, 0x0c, 2, 2, 0x80094610));
  STEP(0x80094614);
  TRY(store(run, 20, 16, 0x0e, 2, 2, 0x80094614));
  STEP(0x80094618);
  V0 = or_constant(V0, 8u);
  STEP(0x8009461c);
  TRY(store(run, 2, 16, 0, 1, 1, 0x8009461c));
  STEP(0x80094620);
  TRY(store(run, 17, 29, 0x10, 2, 2, 0x80094620));
  STEP(0x80094624);
  TRY(store(run, 20, 29, 0x12, 2, 2, 0x80094624));
  STEP(0x80094628);
  TRY(load(run, 3, 16, 4, 2, 2, 0x80094628));
  STEP(0x8009462c);
  known(&V0, 1);
  STEP(0x80094630);
  TRY(store(run, 2, 29, 0x16, 2, 2, 0x80094630));
  STEP(0x80094634);
  STEP(0x80094638);
  TRY(store(run, 3, 29, 0x14, 2, 2, 0x80094638));
  TRY(zero_decision(run, S1, 0x80094634, &decision));
  if (!decision)
    goto prepare_upload;
  STEP(0x8009463c);
  STEP(0x80094640);
  TRY(zero_decision(run, S4, 0x8009463c, &decision));
  if (decision)
    goto follow_link;

prepare_upload:
  STEP(0x80094644);
  A0 = add(SP, immediate(0x10u));
  STEP(0x80094648);
  A1 = add(S0, immediate(0x10u));

upload:
  STEP(0x8009464c);
  known(&RA, 0x80094654u);
  STEP(0x80094650);
  TRY(invoke(run, NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800944F4,
             0x8009464c, 0x80094650, 0x800944f4, 2));

follow_link:
  /* 0x80094654..0x8009467C: reload the callback-live record header, form
   * its signed relative link in the jump delay, and continue if non-null. */
  STEP(0x80094654);
  TRY(load(run, 3, 16, 0, 4, 4, 0x80094654));
  STEP(0x80094658);
  known(&V0, UINT32_C(0xffffff00));
  STEP(0x8009465c);
  V0 = and_constant(V1, UINT32_C(0xffffff00));
  STEP(0x80094660);
  predicate = V0;
  STEP(0x80094664);
  V0 = shift_right_arithmetic(V1, 8);
  TRY(zero_decision(run, predicate, 0x80094660, &decision));
  if (decision) {
    STEP(0x80094670);
    known(&A0, 0);
  } else {
    STEP(0x80094668);
    STEP(0x8009466c);
    A0 = add(S0, V0);
  }
  STEP(0x80094674);
  S0 = A0;
  STEP(0x80094678);
  STEP(0x8009467c);
  TRY(zero_decision(run, S0, 0x80094678, &decision));
  if (!decision)
    goto record_loop;

epilogue:
  /* 0x80094680..0x800946A0: restore through callback-live sp, advance that
   * sp, execute the JR NOP delay, then validate the restored target. */
  STEP(0x80094680);
  TRY(load(run, 31, 29, 0x2c, 4, 4, 0x80094680));
  out->restored_return_address = RA;
  STEP(0x80094684);
  TRY(load(run, 20, 29, 0x28, 4, 4, 0x80094684));
  STEP(0x80094688);
  TRY(load(run, 19, 29, 0x24, 4, 4, 0x80094688));
  STEP(0x8009468c);
  TRY(load(run, 18, 29, 0x20, 4, 4, 0x8009468c));
  STEP(0x80094690);
  TRY(load(run, 17, 29, 0x1c, 4, 4, 0x80094690));
  STEP(0x80094694);
  TRY(load(run, 16, 29, 0x18, 4, 4, 0x80094694));
  STEP(0x80094698);
  SP = add(SP, immediate(0x30u));
  STEP(0x8009469c);
  STEP(0x800946a0);
  if (RA.known_mask != 15) {
    stop(run, 0x8009469c, RA.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, 0x8009469c, RA.word, 0);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  stop(run, 0, 0, 0);
  out->completed = 1;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}
