#include "game_frame_ui_service.h"

#include <string.h>

typedef Nba97GameFrameUiServiceWord Word;

typedef struct Run {
  Nba97GameFrameUiServiceContext *context;
  Nba97GameFrameUiServiceProgress *out;
  Nba97GameFrameUiServiceMachine machine;
} Run;

#define R(i) (run->machine.registers.gpr[(i)])
#define V0 R(2)
#define A0 R(4)
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

static int machine_valid(const Nba97GameFrameUiServiceMachine *machine) {
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

static int initialize(Nba97GameFrameUiServiceContext *context,
                      Nba97GameFrameUiServiceProgress *out, Run *run) {
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
    Nba97GameFrameUiServiceAccess *event = &run->context->access_journal[index];
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
  journal(run, NBA97_GAME_FRAME_UI_SERVICE_READ, pc, address, width, loaded);
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
  journal(run, NBA97_GAME_FRAME_UI_SERVICE_STORE, pc, address, width, value);
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

static int invoke(Run *run, uint8_t kind, uint32_t pc, uint32_t delay_pc,
                  uint32_t entry, uint8_t argument_count) {
  Nba97GameFrameUiServiceEvent event;
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

int nba97_game_frame_ui_service(Nba97GameFrameUiServiceContext *context,
                                Nba97GameFrameUiServiceProgress *out) {
  Run storage;
  Run *run = &storage;
  int is_zero;
  TRY(initialize(context, out, run));

  /* 0x80032B10..0x80032B1C: create the small frame, save caller ra, then
   * dispatch the unconditional frame-UI prerequisite through its NOP delay. */
  STEP(0x80032b10);
  SP = add(SP, immediate(UINT32_C(0xffffffe8)));
  out->frame_stack_pointer = SP.word;
  STEP(0x80032b14);
  out->saved_return_address = RA;
  TRY(store(run, 31, 29, 0x10, 4, 4, 0x80032b14));
  STEP(0x80032b18);
  known(&RA, 0x80032b20);
  STEP(0x80032b1c);
  TRY(invoke(run, NBA97_GAME_FRAME_UI_SERVICE_CHILD_8003287C, 0x80032b18,
             0x80032b1c, 0x8003287c, 0));

  /* 0x80032B20..0x80032B30: read the signed retained mode halfword and branch
   * after its explicit load-delay and branch-delay NOPs. */
  STEP(0x80032b20);
  known(&V0, 0x80100000);
  STEP(0x80032b24);
  TRY(load(run, 2, 2, UINT32_C(0xffffa038), 2, 2, 0x80032b24));
  V0 = sign_extend_half(V0);
  publish(run);
  STEP(0x80032b28);
  STEP(0x80032b2c);
  STEP(0x80032b30);
  TRY(zero_decision(run, V0, 0x80032b2c, &is_zero));
  if (is_zero)
    goto zero_mode;

  /* 0x80032B34..0x80032B44: query command D4 in the JAL delay, truncate the
   * callback's raw v0 to its low byte, then select the second query path. */
  STEP(0x80032b34);
  known(&RA, 0x80032b3c);
  STEP(0x80032b38);
  known(&A0, 0xd4);
  TRY(invoke(run, NBA97_GAME_FRAME_UI_SERVICE_CHILD_80031C5C, 0x80032b34,
             0x80032b38, 0x80031c5c, 1));
  STEP(0x80032b3c);
  V0 = and_constant(V0, 0xff);
  publish(run);
  STEP(0x80032b40);
  STEP(0x80032b44);
  TRY(zero_decision(run, V0, 0x80032b40, &is_zero));
  if (is_zero)
    goto second_query;

  /* 0x80032B48..0x80032B5C: a nonzero D4 query issues D3 then D4 through
   * independent JAL delays before the jump and its NOP reach the epilogue. */
  STEP(0x80032b48);
  known(&RA, 0x80032b50);
  STEP(0x80032b4c);
  known(&A0, 0xd3);
  TRY(invoke(run, NBA97_GAME_FRAME_UI_SERVICE_CHILD_8003066C, 0x80032b48,
             0x80032b4c, 0x8003066c, 1));
  STEP(0x80032b50);
  known(&RA, 0x80032b58);
  STEP(0x80032b54);
  known(&A0, 0xd4);
  TRY(invoke(run, NBA97_GAME_FRAME_UI_SERVICE_CHILD_8003066C, 0x80032b50,
             0x80032b54, 0x8003066c, 1));
  STEP(0x80032b58);
  STEP(0x80032b5c);
  goto epilogue;

second_query:
  /* 0x80032B60..0x80032B70: query C8 in its delay slot and branch on only
   * the callback result's low byte after ANDI and the branch-delay NOP. */
  STEP(0x80032b60);
  known(&RA, 0x80032b68);
  STEP(0x80032b64);
  known(&A0, 0xc8);
  TRY(invoke(run, NBA97_GAME_FRAME_UI_SERVICE_CHILD_80031C5C, 0x80032b60,
             0x80032b64, 0x80031c5c, 1));
  STEP(0x80032b68);
  V0 = and_constant(V0, 0xff);
  publish(run);
  STEP(0x80032b6c);
  STEP(0x80032b70);
  TRY(zero_decision(run, V0, 0x80032b6c, &is_zero));
  if (is_zero)
    goto epilogue;

  /* 0x80032B74..0x80032B88: a nonzero C8 query issues C8 then command two,
   * retaining both JAL delay arguments and the final jump-delay NOP. */
  STEP(0x80032b74);
  known(&RA, 0x80032b7c);
  STEP(0x80032b78);
  known(&A0, 0xc8);
  TRY(invoke(run, NBA97_GAME_FRAME_UI_SERVICE_CHILD_8003066C, 0x80032b74,
             0x80032b78, 0x8003066c, 1));
  STEP(0x80032b7c);
  known(&RA, 0x80032b84);
  STEP(0x80032b80);
  known(&A0, 2);
  TRY(invoke(run, NBA97_GAME_FRAME_UI_SERVICE_CHILD_8003066C, 0x80032b7c,
             0x80032b80, 0x8003066c, 1));
  STEP(0x80032b84);
  STEP(0x80032b88);
  goto epilogue;

zero_mode:
  /* 0x80032B8C..0x80032BA4: zero mode loads the presentation byte. A nonzero
   * byte exits raw; zero invokes the idle child through a NOP delay. */
  STEP(0x80032b8c);
  known(&V0, 0x800f0000);
  STEP(0x80032b90);
  TRY(load(run, 2, 2, UINT32_C(0xffffb680), 1, 1, 0x80032b90));
  STEP(0x80032b94);
  STEP(0x80032b98);
  STEP(0x80032b9c);
  TRY(zero_decision(run, V0, 0x80032b98, &is_zero));
  if (!is_zero)
    goto epilogue;
  STEP(0x80032ba0);
  known(&RA, 0x80032ba8);
  STEP(0x80032ba4);
  TRY(invoke(run, NBA97_GAME_FRAME_UI_SERVICE_CHILD_80032774, 0x80032ba0,
             0x80032ba4, 0x80032774, 0));

epilogue:
  /* 0x80032BA8..0x80032BB4: reload ra through callback-live sp, advance that
   * same live sp, and execute the JR's NOP delay before validating its target.
   */
  STEP(0x80032ba8);
  TRY(load(run, 31, 29, 0x10, 4, 4, 0x80032ba8));
  out->restored_return_address = RA;
  STEP(0x80032bac);
  SP = add(SP, immediate(0x18));
  publish(run);
  STEP(0x80032bb0);
  STEP(0x80032bb4);
  if (RA.known_mask != 15) {
    stop(run, 0x80032bb0, RA.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, 0x80032bb0, RA.word, 0);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
