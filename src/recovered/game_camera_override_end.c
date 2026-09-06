#include "game_camera_override_end.h"

#include <string.h>

typedef struct Run {
  Nba97GameCameraOverrideEndContext *context;
  Nba97GameCameraOverrideEndProgress *out;
  Nba97GameCameraOverrideEndMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define TRY(expression) do { \
  int nba97_result_ = (expression); \
  if (nba97_result_ != NBA97_TEXT_COMPLETE) return nba97_result_; \
} while (0)

static void publish(Run *run) {
  run->out->machine = run->machine;
  run->out->returned_value = R(NBA97_MATCH_INITIALIZE_V0);
}

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t entry) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  run->out->stopped_entry = entry;
  publish(run);
}

static void set_known(Nba97GameCameraOverrideEndWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameCameraOverrideEndMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].word != 0 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_ZERO].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int validate(Nba97GameCameraOverrideEndContext *context,
                    Nba97GameCameraOverrideEndProgress *out, Run *run) {
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
    uint64_t size = (uint64_t)a->size;
    if (!a->data || !a->size || size > UINT64_C(0x100000000) ||
        (uint64_t)a->base + size > UINT64_C(0x100000000))
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

/* Per-byte carry enumeration keeps address bytes only when they are invariant
 * over every concrete source represented by the known masks. */
static Nba97GameCameraOverrideEndWord add_words(
    Nba97GameCameraOverrideEndWord left,
    Nba97GameCameraOverrideEndWord right) {
  Nba97GameCameraOverrideEndWord result;
  unsigned carry_mask = 1u;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0;
  if (left.known_mask == 0x0fu && right.known_mask == 0x0fu) {
    result.known_mask = 0x0fu;
    return result;
  }
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_carry_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned ls = (left.known_mask & (1u << byte)) ?
        ((left.word >> (byte * 8u)) & 0xffu) : 0u;
    unsigned le = (left.known_mask & (1u << byte)) ? ls : 255u;
    unsigned rs = (right.known_mask & (1u << byte)) ?
        ((right.word >> (byte * 8u)) & 0xffu) : 0u;
    unsigned re = (right.known_mask & (1u << byte)) ? rs : 255u;
    unsigned carry;
    for (carry = 0; carry <= 1; ++carry) {
      unsigned a;
      if (!(carry_mask & (1u << carry)))
        continue;
      for (a = ls; a <= le; ++a) {
        unsigned b;
        for (b = rs; b <= re; ++b) {
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
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
    carry_mask = next_carry_mask;
  }
  return result;
}

static Nba97GameCameraOverrideEndWord add_constant(
    Nba97GameCameraOverrideEndWord source, uint32_t constant) {
  Nba97GameCameraOverrideEndWord value;
  set_known(&value, constant);
  return add_words(source, value);
}

static uint32_t width_mask(unsigned width) {
  return width == 4 ? UINT32_MAX :
      (UINT32_C(1) << (width * 8u)) - 1u;
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
                    uint8_t width,
                    const Nba97GameCameraOverrideEndWord *value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameCameraOverrideEndAccess *event = &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word & width_mask(width);
    event->operation = run->out->operations;
    event->width = width;
    event->known_mask = (uint8_t)(value->known_mask & knowledge_mask(width));
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t address, size_t width, size_t alignment,
                  uint32_t pc, uint8_t **data, uint8_t **known) {
  size_t i;
  size_t j;
  stop(run, pc, address, 0);
  TRY(spend(run));
  ++run->out->accesses;
  if (address & (uint32_t)(alignment - 1u))
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

static int read_value(Run *run, uint32_t address, uint8_t width, uint32_t pc,
                      Nba97GameCameraOverrideEndWord *value) {
  Nba97GameCameraOverrideEndWord loaded = {0, 0};
  uint8_t *data;
  uint8_t *known;
  unsigned i;
  TRY(locate(run, address, width, width, pc, &data, &known));
  for (i = 0; i < width; ++i) {
    loaded.word |= (uint32_t)data[i] << (i * 8u);
    if (!known || known[i])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
  }
  for (i = width; i < 4; ++i)
    loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
  *value = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, address, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, uint32_t address, uint8_t width, uint32_t pc,
                       const Nba97GameCameraOverrideEndWord *value) {
  Nba97GameCameraOverrideEndWord stored = *value;
  uint8_t *data;
  uint8_t *known;
  unsigned i;
  stored.word &= width_mask(width);
  stored.known_mask =
      (uint8_t)(stored.known_mask & knowledge_mask(width));
  TRY(locate(run, address, width, width, pc, &data, &known));
  if (!known && stored.known_mask != knowledge_mask(width))
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < width; ++i) {
    data[i] = (uint8_t)(stored.word >> (i * 8u));
    if (known)
      known[i] = (uint8_t)((stored.known_mask >> i) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, width, &stored);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int register_address(Run *run, Nba97GameCameraOverrideEndWord base,
                            uint32_t offset, uint32_t pc, uint32_t *address) {
  Nba97GameCameraOverrideEndWord value = add_constant(base, offset);
  if (value.known_mask != 0x0fu) {
    stop(run, pc, value.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = value.word;
  return NBA97_TEXT_COMPLETE;
}

static int decide_zero(Run *run, const Nba97GameCameraOverrideEndWord *value,
                       uint32_t pc, int *is_zero) {
  unsigned i;
  for (i = 0; i < 4; ++i)
    if ((value->known_mask & (1u << i)) &&
        ((value->word >> (i * 8u)) & 0xffu)) {
      *is_zero = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (value->known_mask == 0x0fu) {
    *is_zero = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

static int invoke_child(Run *run) {
  Nba97GameCameraOverrideEndEvent event;
  int accepted;
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x8007a388));
  set_known(&R(NBA97_MATCH_INITIALIZE_A0), 0);
  stop(run, UINT32_C(0x8007a380), 0, UINT32_C(0x8007a114));
  TRY(spend(run));
  memset(&event, 0, sizeof event);
  event.pc = UINT32_C(0x8007a380);
  event.delay_slot_pc = UINT32_C(0x8007a384);
  event.entry = UINT32_C(0x8007a114);
  event.operation = run->out->operations;
  event.invocation = run->out->call_count[
      NBA97_GAME_CAMERA_OVERRIDE_END_CHILD_8007A114] + 1u;
  event.kind = NBA97_GAME_CAMERA_OVERRIDE_END_CHILD_8007A114;
  event.argument_count = 1;
  publish(run);
  if (!run->context->io)
    return NBA97_TEXT_IO_REFUSED;
  accepted = run->context->io(run->context->user, &run->context->memory,
                              &event, &run->machine);
  publish(run);
  if (accepted != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!machine_valid(&run->machine))
    return NBA97_TEXT_ARGUMENT;
  ++run->out->callbacks_completed;
  ++run->out->call_count[NBA97_GAME_CAMERA_OVERRIDE_END_CHILD_8007A114];
  return NBA97_TEXT_COMPLETE;
}

static int restore_ra(Run *run) {
  uint32_t address;
  TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u,
                       UINT32_C(0x8007a390), &address));
  TRY(read_value(run, address, 4, UINT32_C(0x8007a390),
                 &R(NBA97_MATCH_INITIALIZE_RA)));
  run->out->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_camera_override_end(Nba97GameCameraOverrideEndContext *context,
                                   Nba97GameCameraOverrideEndProgress *out) {
  Run storage;
  Run *run = &storage;
  Nba97GameCameraOverrideEndWord branch_value;
  Nba97GameCameraOverrideEndWord zero;
  uint32_t address;
  int branch;
  TRY(validate(context, out, run));

  /* 0x8007A36C..0x8007A370: LUI and LBU read the flag before the frame is
   * allocated, so an early memory failure leaves entry sp untouched. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800c0000));
  TRY(read_value(run, UINT32_C(0x800bc1f0), 1, UINT32_C(0x8007a370),
                 &R(NBA97_MATCH_INITIALIZE_V0)));
  out->flag = R(NBA97_MATCH_INITIALIZE_V0);

  /* 0x8007A374..0x8007A37C: the BEQ delay saves ra for both outcomes and
   * before an unknown flag predicate can stop. */
  R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
      R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe8));
  out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  branch_value = R(NBA97_MATCH_INITIALIZE_V0);
  TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u,
                       UINT32_C(0x8007a37c), &address));
  out->saved_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  TRY(write_value(run, address, 4, UINT32_C(0x8007a37c),
                  &R(NBA97_MATCH_INITIALIZE_RA)));
  TRY(decide_zero(run, &branch_value, UINT32_C(0x8007a378), &branch));

  if (!branch) {
    /* 0x8007A380..0x8007A38C: JAL publishes ra and delay-slot a0 before the
     * child. Only a completed child reaches LUI at and the final flag clear. */
    TRY(invoke_child(run));
    set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800c0000));
    set_known(&zero, 0);
    TRY(write_value(run, UINT32_C(0x800bc1f0), 1,
                    UINT32_C(0x8007a38c), &zero));
  }

  /* 0x8007A390..0x8007A39C: reload ra through child-mutable live sp,
   * advance it by 0x18, and consume the restored value after JR's NOP. */
  TRY(restore_ra(run));
  R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
      R(NBA97_MATCH_INITIALIZE_SP), 0x18u);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x8007a398), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
