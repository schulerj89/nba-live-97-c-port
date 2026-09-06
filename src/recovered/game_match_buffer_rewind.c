#include "game_match_buffer_rewind.h"

#include <string.h>

#define R(i) (run->machine.registers.gpr[(i)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int result_ = (expression);                                                \
    if (result_ != NBA97_TEXT_COMPLETE)                                        \
      return result_;                                                          \
  } while (0)

typedef struct Run {
  Nba97GameMatchBufferRewindContext *context;
  Nba97GameMatchBufferRewindProgress *out;
  Nba97GameMatchBufferRewindMachine machine;
} Run;

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t entry) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  run->out->stopped_entry = entry;
  publish(run);
}

static void known(Nba97GameMatchBufferRewindWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 15;
}

static int machine_valid(const Nba97GameMatchBufferRewindMachine *machine) {
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

static int initialize(Nba97GameMatchBufferRewindContext *context,
                      Nba97GameMatchBufferRewindProgress *out, Run *run) {
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

static Nba97GameMatchBufferRewindWord
add(Nba97GameMatchBufferRewindWord left, Nba97GameMatchBufferRewindWord right) {
  Nba97GameMatchBufferRewindWord result;
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
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
    carry_mask = next_mask;
  }
  return result;
}

static Nba97GameMatchBufferRewindWord
add_constant(Nba97GameMatchBufferRewindWord source, uint32_t value) {
  Nba97GameMatchBufferRewindWord constant;
  known(&constant, value);
  return add(source, constant);
}

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width, Nba97GameMatchBufferRewindWord value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameMatchBufferRewindAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word;
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
                      unsigned alignment, uint32_t pc,
                      Nba97GameMatchBufferRewindWord *value) {
  uint8_t *data;
  uint8_t *known_bytes;
  Nba97GameMatchBufferRewindWord loaded = {0, 0};
  unsigned i;
  TRY(locate(run, address, width, alignment, pc, &data, &known_bytes));
  for (i = 0; i < width; ++i) {
    loaded.word |= (uint32_t)data[i] << (i * 8u);
    if (!known_bytes || known_bytes[i])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (uint8_t)(1u << i));
  }
  *value = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_MATCH_BUFFER_REWIND_READ, pc, address, width, loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, uint32_t address, unsigned width,
                       unsigned alignment, uint32_t pc,
                       Nba97GameMatchBufferRewindWord value) {
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
  journal(run, NBA97_GAME_MATCH_BUFFER_REWIND_STORE, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int effective_address(Run *run, Nba97GameMatchBufferRewindWord base,
                             uint32_t offset, uint32_t pc, uint32_t *result) {
  Nba97GameMatchBufferRewindWord effective = add_constant(base, offset);
  if (effective.known_mask != 15) {
    stop(run, pc, effective.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = effective.word;
  return NBA97_TEXT_COMPLETE;
}

static int load_register(Run *run, unsigned destination, unsigned base,
                         uint32_t offset, unsigned width, unsigned alignment,
                         uint32_t pc) {
  uint32_t address;
  Nba97GameMatchBufferRewindWord value;
  TRY(effective_address(run, R(base), offset, pc, &address));
  TRY(read_value(run, address, width, alignment, pc, &value));
  R(destination) = value;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int store_register(Run *run, unsigned source, unsigned base,
                          uint32_t offset, unsigned width, unsigned alignment,
                          uint32_t pc) {
  uint32_t address;
  TRY(effective_address(run, R(base), offset, pc, &address));
  return write_value(run, address, width, alignment, pc, R(source));
}

static int invoke_zero(Run *run) {
  Nba97GameMatchBufferRewindEvent event;
  int accepted;
  stop(run, 0x80076af8, 0, 0x800a3a74);
  TRY(spend(run));
  ++run->out->call_attempts[NBA97_GAME_MATCH_BUFFER_REWIND_ZERO];
  memset(&event, 0, sizeof event);
  event.pc = 0x80076af8;
  event.delay_slot_pc = 0x80076afc;
  event.entry = 0x800a3a74;
  event.operation = run->out->operations;
  event.invocation =
      run->out->call_attempts[NBA97_GAME_MATCH_BUFFER_REWIND_ZERO];
  event.kind = NBA97_GAME_MATCH_BUFFER_REWIND_ZERO;
  event.argument_count = 2;
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
  ++run->out->call_count[NBA97_GAME_MATCH_BUFFER_REWIND_ZERO];
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_match_buffer_rewind(Nba97GameMatchBufferRewindContext *context,
                                   Nba97GameMatchBufferRewindProgress *out) {
  Run storage;
  Run *run = &storage;
  TRY(initialize(context, out, run));

  /* 0x80076AD0..0x80076AF4: copy the retained pointer twice after creating
   * the frame. The source LW completes before the SP load-delay instruction. */
  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(load_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_V0,
                    UINT32_C(0xffffa004), 4, 4, 0x80076ad4));
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe8));
  out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x800f0000));
  R(NBA97_MATCH_INITIALIZE_A0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_A0), 0x1918);
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_RA, NBA97_MATCH_INITIALIZE_SP,
                     0x10, 4, 4, 0x80076ae4));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_AT,
                     UINT32_C(0xffffa00c), 4, 4, 0x80076aec));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_AT,
                     UINT32_C(0xffffa010), 4, 4, 0x80076af4));

  /* 0x80076AF8/FC: JAL assigns ra before its delay slot supplies length four.
   */
  known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x80076b00));
  known(&R(NBA97_MATCH_INITIALIZE_A1), 4);
  TRY(invoke_zero(run));

  /* 0x80076B00..0x80076B14: after the callback-live machine returns, clear
   * the three retained flags in their original word/halfword/byte order. */
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_ZERO,
                     NBA97_MATCH_INITIALIZE_AT, UINT32_C(0xffffe860), 4, 4,
                     0x80076b04));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_ZERO,
                     NBA97_MATCH_INITIALIZE_AT, 0x148c, 2, 2, 0x80076b0c));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_ZERO,
                     NBA97_MATCH_INITIALIZE_AT, UINT32_C(0xffffe864), 1, 1,
                     0x80076b14));

  /* 0x80076B18..0x80076B24: restore ra through callback-live sp, apply the
   * following load-delay SP adjustment, then validate JR before its NOP. */
  TRY(load_register(run, NBA97_MATCH_INITIALIZE_RA, NBA97_MATCH_INITIALIZE_SP,
                    0x10, 4, 4, 0x80076b18));
  out->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x18);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15) {
    stop(run, 0x80076b20, R(NBA97_MATCH_INITIALIZE_RA).word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (R(NBA97_MATCH_INITIALIZE_RA).word & 3u) {
    stop(run, 0x80076b20, R(NBA97_MATCH_INITIALIZE_RA).word, 0);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
