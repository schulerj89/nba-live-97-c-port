#include "game_match_state_reset.h"

#include <string.h>

#define R(i) (run->machine.registers.gpr[(i)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int result_ = (expression);                                                \
    if (result_ != NBA97_TEXT_COMPLETE)                                        \
      return result_;                                                          \
  } while (0)

typedef struct Run {
  Nba97GameMatchStateResetContext *context;
  Nba97GameMatchStateResetProgress *out;
  Nba97GameMatchStateResetMachine machine;
} Run;

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t entry) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  run->out->stopped_entry = entry;
  publish(run);
}

static void known(Nba97GameMatchStateResetWord *word, uint32_t value) {
  word->word = value;
  word->known_mask = 15;
}

static int machine_valid(const Nba97GameMatchStateResetMachine *machine) {
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

static int initialize(Nba97GameMatchStateResetContext *context,
                      Nba97GameMatchStateResetProgress *out, Run *run) {
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

/* Enumerating carry values preserves every invariant result byte. */
static Nba97GameMatchStateResetWord add(Nba97GameMatchStateResetWord left,
                                        Nba97GameMatchStateResetWord right) {
  Nba97GameMatchStateResetWord result;
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

static Nba97GameMatchStateResetWord
add_constant(Nba97GameMatchStateResetWord source, uint32_t value) {
  Nba97GameMatchStateResetWord constant;
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
                    unsigned width, Nba97GameMatchStateResetWord value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameMatchStateResetAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word;
    event->operation = run->out->operations;
    event->width = (uint8_t)width;
    event->known_mask = (uint8_t)(value.known_mask & ((1u << width) - 1u));
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
                      Nba97GameMatchStateResetWord *value) {
  uint8_t *data;
  uint8_t *known_bytes;
  Nba97GameMatchStateResetWord loaded = {0, 0};
  unsigned i;
  TRY(locate(run, address, width, alignment, pc, &data, &known_bytes));
  for (i = 0; i < width; ++i) {
    loaded.word |= (uint32_t)data[i] << (i * 8u);
    if (!known_bytes || known_bytes[i])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (uint8_t)(1u << i));
  }
  *value = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_MATCH_STATE_RESET_READ, pc, address, width, loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, uint32_t address, unsigned width,
                       unsigned alignment, uint32_t pc,
                       Nba97GameMatchStateResetWord value) {
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
  journal(run, NBA97_GAME_MATCH_STATE_RESET_STORE, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int address(Run *run, Nba97GameMatchStateResetWord base, uint32_t offset,
                   uint32_t pc, uint32_t *result) {
  Nba97GameMatchStateResetWord effective = add_constant(base, offset);
  if (effective.known_mask != 15) {
    stop(run, pc, effective.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = effective.word;
  return NBA97_TEXT_COMPLETE;
}

static int load(Run *run, unsigned destination, unsigned base, uint32_t offset,
                unsigned width, unsigned alignment, uint32_t pc) {
  uint32_t effective;
  Nba97GameMatchStateResetWord value;
  TRY(address(run, R(base), offset, pc, &effective));
  TRY(read_value(run, effective, width, alignment, pc, &value));
  if (width == 2) {
    value.word &= UINT32_C(0xffff);
    value.known_mask = (uint8_t)(value.known_mask | 12u);
  }
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

static int equal(Run *run, Nba97GameMatchStateResetWord left,
                 Nba97GameMatchStateResetWord right, uint32_t pc, int *result) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((left.known_mask & right.known_mask & (1u << byte)) &&
        (((left.word ^ right.word) >> (byte * 8u)) & 255u)) {
      *result = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (left.known_mask == 15 && right.known_mask == 15) {
    *result = left.word == right.word;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

static int invoke_after_delay(Run *run, uint32_t pc, uint32_t entry,
                              uint8_t kind, uint8_t argument_count) {
  Nba97GameMatchStateResetEvent event;
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
  if (!machine_valid(&run->machine))
    return NBA97_TEXT_ARGUMENT;
  ++run->out->callbacks_completed;
  ++run->out->call_count[kind];
  return NBA97_TEXT_COMPLETE;
}

static void jal(Run *run, uint32_t pc) {
  known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
  publish(run);
}

static int call_nop(Run *run, uint32_t pc, uint32_t entry, uint8_t kind,
                    uint8_t argument_count) {
  jal(run, pc);
  return invoke_after_delay(run, pc, entry, kind, argument_count);
}

int nba97_game_match_state_reset(Nba97GameMatchStateResetContext *context,
                                 Nba97GameMatchStateResetProgress *out) {
  Run storage;
  Run *run = &storage;
  Nba97GameMatchStateResetWord zero;
  Nba97GameMatchStateResetWord mode;
  int matches;
  TRY(initialize(context, out, run));
  known(&zero, 0);

  /* 0x800659F0..0x80065A10: create the frame. The first JAL assigns ra
   * before its delay-slot save of live s1. */
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
  out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  TRY(store(run, NBA97_MATCH_INITIALIZE_S0, NBA97_MATCH_INITIALIZE_SP, 0x10, 4,
            4, 0x800659f4));
  known(&R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0x80020000));
  R(NBA97_MATCH_INITIALIZE_S0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0xfffff33c));
  R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
  known(&R(NBA97_MATCH_INITIALIZE_A1), 0x4b0);
  TRY(store(run, NBA97_MATCH_INITIALIZE_RA, NBA97_MATCH_INITIALIZE_SP, 0x18, 4,
            4, 0x80065a08));
  jal(run, 0x80065a0c);
  TRY(store(run, NBA97_MATCH_INITIALIZE_S0 + 1, NBA97_MATCH_INITIALIZE_SP, 0x14,
            4, 4, 0x80065a10));
  TRY(invoke_after_delay(run, 0x80065a0c, 0x800a3a74,
                         NBA97_GAME_MATCH_STATE_RESET_ZERO, 2));

  /* 0x80065A14..0x80065A34: each zero-fill argument derives from callback-live
   * s0, and each JAL delay slot supplies its exact byte count. */
  R(NBA97_MATCH_INITIALIZE_A0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_S0), 0x4b0);
  jal(run, 0x80065a18);
  known(&R(NBA97_MATCH_INITIALIZE_A1), 0x1320);
  TRY(invoke_after_delay(run, 0x80065a18, 0x800a3a74,
                         NBA97_GAME_MATCH_STATE_RESET_ZERO, 2));
  R(NBA97_MATCH_INITIALIZE_A0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0xfffffab8));
  jal(run, 0x80065a24);
  known(&R(NBA97_MATCH_INITIALIZE_A1), 0xc4);
  TRY(invoke_after_delay(run, 0x80065a24, 0x800a3a74,
                         NBA97_GAME_MATCH_STATE_RESET_ZERO, 2));
  R(NBA97_MATCH_INITIALIZE_A0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0xfffffb7c));
  jal(run, 0x80065a30);
  known(&R(NBA97_MATCH_INITIALIZE_A1), 0xc4);
  TRY(invoke_after_delay(run, 0x80065a30, 0x800a3a74,
                         NBA97_GAME_MATCH_STATE_RESET_ZERO, 2));

  /* 0x80065A38..0x80065A58: reset the first service, publish two halfwords,
   * then rebuild roster bindings. */
  jal(run, 0x80065a38);
  R(NBA97_MATCH_INITIALIZE_A0) = zero;
  TRY(invoke_after_delay(run, 0x80065a38, 0x80083490,
                         NBA97_GAME_MATCH_STATE_RESET_80083490, 1));
  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_MAX);
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
  TRY(store(run, NBA97_MATCH_INITIALIZE_ZERO, NBA97_MATCH_INITIALIZE_AT,
            UINT32_C(0xffffedf2), 2, 2, 0x80065a48));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  TRY(store(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_AT,
            UINT32_C(0xffffdb9c), 2, 2, 0x80065a50));
  TRY(call_nop(run, 0x80065a54, 0x80063d58,
               NBA97_GAME_MATCH_STATE_RESET_80063D58, 0));

  /* 0x80065A5C..0x80065A68: retain the original fixed decrement spin. The
   * branch tests 22 through -1 and its delay decrement leaves v0=-2. */
  known(&R(NBA97_MATCH_INITIALIZE_V0), 0x17);
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_MAX);
  for (;;) {
    int nonnegative =
        !(R(NBA97_MATCH_INITIALIZE_V0).word & UINT32_C(0x80000000));
    R(NBA97_MATCH_INITIALIZE_V0) =
        add_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_MAX);
    ++out->spin_iterations;
    if (!nonnegative)
      break;
  }

  /* 0x80065A6C..0x80065AA8: form the paired structure pointers, store five in
   * the first JAL delay slot, and preserve every callback-live argument. */
  known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80020000));
  R(NBA97_MATCH_INITIALIZE_V1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xffffeecc));
  R(NBA97_MATCH_INITIALIZE_S0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xffffff28));
  R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
  R(NBA97_MATCH_INITIALIZE_S0 + 1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xffffffec));
  R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_S0 + 1);
  known(&R(NBA97_MATCH_INITIALIZE_V0), 5);
  jal(run, 0x80065a88);
  TRY(store(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_V1, 0, 2, 2,
            0x80065a8c));
  TRY(invoke_after_delay(run, 0x80065a88, 0x800655b0,
                         NBA97_GAME_MATCH_STATE_RESET_800655B0, 2));
  R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0 + 1);
  jal(run, 0x80065a94);
  R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_S0);
  TRY(invoke_after_delay(run, 0x80065a94, 0x800655b0,
                         NBA97_GAME_MATCH_STATE_RESET_800655B0, 2));
  TRY(call_nop(run, 0x80065a9c, 0x80065328,
               NBA97_GAME_MATCH_STATE_RESET_80065328, 0));
  TRY(call_nop(run, 0x80065aa4, 0x80065db0,
               NBA97_GAME_MATCH_STATE_RESET_80065DB0, 0));

  /* 0x80065AAC..0x80065AD0: both 65820 calls use live arguments. The final
   * zero store addresses callback-live s0 in the 646A8 delay slot. */
  R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
  known(&R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0x80100000));
  R(NBA97_MATCH_INITIALIZE_S0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_S0), UINT32_C(0xffffdb54));
  known(&R(NBA97_MATCH_INITIALIZE_V0), 1);
  jal(run, 0x80065abc);
  TRY(store(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_S0, 0, 2, 2,
            0x80065ac0));
  TRY(invoke_after_delay(run, 0x80065abc, 0x80065820,
                         NBA97_GAME_MATCH_STATE_RESET_80065820, 1));
  jal(run, 0x80065ac4);
  R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0 + 1);
  TRY(invoke_after_delay(run, 0x80065ac4, 0x80065820,
                         NBA97_GAME_MATCH_STATE_RESET_80065820, 1));
  jal(run, 0x80065acc);
  TRY(store(run, NBA97_MATCH_INITIALIZE_ZERO, NBA97_MATCH_INITIALIZE_S0, 0, 2,
            2, 0x80065ad0));
  TRY(invoke_after_delay(run, 0x80065acc, 0x800646a8,
                         NBA97_GAME_MATCH_STATE_RESET_800646A8, 0));

  /* 0x80065AD4..0x80065AFC: the LHU completes before its load-delay
   * assignment v0=98; an unknown mode stops at the BNE after that assignment.
   */
  known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x80020000));
  TRY(load(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_V1,
           UINT32_C(0xffffedec), 2, 2, 0x80065ad8));
  known(&R(NBA97_MATCH_INITIALIZE_V0), 0x62);
  mode = R(NBA97_MATCH_INITIALIZE_V0);
  TRY(equal(run, R(NBA97_MATCH_INITIALIZE_V1), mode, 0x80065ae0, &matches));
  if (matches) {
    out->mode_98 = 1;
    TRY(call_nop(run, 0x80065ae8, 0x80076ad0,
                 NBA97_GAME_MATCH_STATE_RESET_80076AD0, 0));
  } else {
    TRY(call_nop(run, 0x80065af8, 0x8006432c,
                 NBA97_GAME_MATCH_STATE_RESET_8006432C, 0));
  }

  /* 0x80065B00..0x80065B14: restore through callback-live sp in exact source
   * order, advance that live frame, then consume ra after the NOP delay. */
  TRY(load(run, NBA97_MATCH_INITIALIZE_RA, NBA97_MATCH_INITIALIZE_SP, 0x18, 4,
           4, 0x80065b00));
  out->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  TRY(load(run, NBA97_MATCH_INITIALIZE_S0 + 1, NBA97_MATCH_INITIALIZE_SP, 0x14,
           4, 4, 0x80065b04));
  out->restored_s1 = R(NBA97_MATCH_INITIALIZE_S0 + 1);
  TRY(load(run, NBA97_MATCH_INITIALIZE_S0, NBA97_MATCH_INITIALIZE_SP, 0x10, 4,
           4, 0x80065b08));
  out->restored_s0 = R(NBA97_MATCH_INITIALIZE_S0);
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x20);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15) {
    stop(run, 0x80065b10, 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
