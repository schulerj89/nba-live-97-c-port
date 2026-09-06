#include "game_rectangle_upload_submit.h"

#include <stdint.h>
#include <string.h>

typedef Nba97GameRectangleUploadSubmitWord Word;

typedef struct Run {
  Nba97GameRectangleUploadSubmitContext *context;
  Nba97GameRectangleUploadSubmitProgress *out;
  Nba97GameRectangleUploadSubmitMachine machine;
} Run;

#define R(i) (run->machine.registers.gpr[(i)])
#define AT R(1)
#define V0 R(2)
#define A0 R(4)
#define A1 R(5)
#define S0 R(16)
#define S1 R(17)
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

static int machine_valid(const Nba97GameRectangleUploadSubmitMachine *machine) {
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

static int initialize(Nba97GameRectangleUploadSubmitContext *context,
                      Nba97GameRectangleUploadSubmitProgress *out, Run *run) {
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
    Nba97GameRectangleUploadSubmitAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word;
    event->operation = run->out->operations;
    event->width = (uint8_t)width;
    event->known_mask = value.known_mask;
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

static int read_value(Run *run, uint32_t address, uint32_t pc, Word *value) {
  uint8_t *data;
  uint8_t *known_bytes;
  Word loaded = {0, 0};
  unsigned i;
  TRY(locate(run, address, 4, 4, pc, &data, &known_bytes));
  for (i = 0; i < 4; ++i) {
    loaded.word |= (uint32_t)data[i] << (i * 8u);
    if (!known_bytes || known_bytes[i])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
  }
  *value = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_READ, pc, address, 4, loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, uint32_t address, uint32_t pc, Word value) {
  uint8_t *data;
  uint8_t *known_bytes;
  unsigned i;
  TRY(locate(run, address, 4, 4, pc, &data, &known_bytes));
  if (!known_bytes && value.known_mask != 15)
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < 4; ++i) {
    data[i] = (uint8_t)(value.word >> (i * 8u));
    if (known_bytes)
      known_bytes[i] = (uint8_t)((value.known_mask >> i) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_STORE, pc, address, 4, value);
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
                uint32_t pc) {
  uint32_t effective;
  Word value;
  TRY(address(run, R(base), offset, pc, &effective));
  TRY(read_value(run, effective, pc, &value));
  R(destination) = value;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int store(Run *run, unsigned source, unsigned base, uint32_t offset,
                 uint32_t pc) {
  uint32_t effective;
  TRY(address(run, R(base), offset, pc, &effective));
  return write_value(run, effective, pc, R(source));
}

static int invoke(Run *run, uint8_t kind, uint32_t pc, uint32_t delay_pc,
                  uint32_t entry, uint8_t argument_count) {
  Nba97GameRectangleUploadSubmitEvent event;
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

int nba97_game_rectangle_upload_submit(
    Nba97GameRectangleUploadSubmitContext *context,
    Nba97GameRectangleUploadSubmitProgress *out) {
  Run storage;
  Run *run = &storage;
  TRY(initialize(context, out, run));

  /* 0x800944F4..0x80094504: allocate the frame, save incoming s0/s1/ra in
   * source order, and retain the rectangle pointer in s0. */
  STEP(0x800944f4);
  SP = add(SP, immediate(UINT32_C(0xffffffe0)));
  out->frame_stack_pointer = SP.word;
  STEP(0x800944f8);
  TRY(store(run, 16, 29, 0x10, 0x800944f8));
  STEP(0x800944fc);
  S0 = A0;
  STEP(0x80094500);
  TRY(store(run, 17, 29, 0x14, 0x80094500));
  STEP(0x80094504);
  out->saved_return_address = RA;
  TRY(store(run, 31, 29, 0x18, 0x80094504));

  /* 0x80094508/0x8009450C: set the JAL return before the delay captures a1,
   * then expose the full callback-live machine to typed child 0x80094440. */
  STEP(0x80094508);
  known(&RA, 0x80094510u);
  STEP(0x8009450c);
  S1 = A1;
  TRY(invoke(run, NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_80094440,
             0x80094508, 0x8009450c, 0x80094440, 1));

  /* 0x80094510..0x80094518: rebuild both arguments from callback-live s0/s1;
   * the second argument move remains in the second JAL delay slot. */
  STEP(0x80094510);
  A0 = S0;
  STEP(0x80094514);
  known(&RA, 0x8009451cu);
  STEP(0x80094518);
  A1 = S1;
  TRY(invoke(run, NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_8009971C,
             0x80094514, 0x80094518, 0x8009971c, 2));

  /* 0x8009451C..0x80094524: overwrite callback v0/AT and publish the pending
   * upload flag only after both typed services have completed. */
  STEP(0x8009451c);
  known(&V0, 1);
  STEP(0x80094520);
  known(&AT, 0x800d0000u);
  STEP(0x80094524);
  TRY(store(run, 2, 1, 0x7b14, 0x80094524));

  /* 0x80094528..0x8009453C: restore through callback-live sp, advance it,
   * execute the JR NOP delay, then validate the restored branch target. */
  STEP(0x80094528);
  TRY(load(run, 31, 29, 0x18, 0x80094528));
  out->restored_return_address = RA;
  STEP(0x8009452c);
  TRY(load(run, 17, 29, 0x14, 0x8009452c));
  STEP(0x80094530);
  TRY(load(run, 16, 29, 0x10, 0x80094530));
  STEP(0x80094534);
  SP = add(SP, immediate(0x20u));
  STEP(0x80094538);
  STEP(0x8009453c);
  if (RA.known_mask != 15) {
    stop(run, 0x80094538, RA.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, 0x80094538, RA.word, 0);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  stop(run, 0, 0, 0);
  out->completed = 1;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}
