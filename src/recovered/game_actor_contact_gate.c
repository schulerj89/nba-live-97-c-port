#include "game_actor_contact_gate.h"

#include <string.h>

typedef struct Run {
  Nba97GameActorContactGateContext *context;
  Nba97GameActorContactGateProgress *out;
  Nba97GameActorContactGateMachine machine;
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

static void set_known(Nba97GameActorContactGateWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameActorContactGateMachine *machine) {
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

static int validate(Nba97GameActorContactGateContext *context,
                    Nba97GameActorContactGateProgress *out, Run *run) {
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

/* Carry and borrow enumeration retains a byte only when every represented
 * source value produces the same byte. */
static Nba97GameActorContactGateWord add_words(
    Nba97GameActorContactGateWord left,
    Nba97GameActorContactGateWord right) {
  Nba97GameActorContactGateWord result;
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

static Nba97GameActorContactGateWord subtract_words(
    Nba97GameActorContactGateWord left,
    Nba97GameActorContactGateWord right) {
  Nba97GameActorContactGateWord result;
  unsigned borrow_mask = 1u;
  unsigned byte;
  result.word = left.word - right.word;
  result.known_mask = 0;
  if (left.known_mask == 0x0fu && right.known_mask == 0x0fu) {
    result.known_mask = 0x0fu;
    return result;
  }
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_borrow_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned ls = (left.known_mask & (1u << byte)) ?
        ((left.word >> (byte * 8u)) & 0xffu) : 0u;
    unsigned le = (left.known_mask & (1u << byte)) ? ls : 255u;
    unsigned rs = (right.known_mask & (1u << byte)) ?
        ((right.word >> (byte * 8u)) & 0xffu) : 0u;
    unsigned re = (right.known_mask & (1u << byte)) ? rs : 255u;
    unsigned borrow;
    for (borrow = 0; borrow <= 1; ++borrow) {
      unsigned a;
      if (!(borrow_mask & (1u << borrow)))
        continue;
      for (a = ls; a <= le; ++a) {
        unsigned b;
        for (b = rs; b <= re; ++b) {
          int difference = (int)a - (int)b - (int)borrow;
          unsigned output = (unsigned)difference & 0xffu;
          next_borrow_mask |= 1u << (difference < 0);
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
    borrow_mask = next_borrow_mask;
  }
  return result;
}

static Nba97GameActorContactGateWord add_constant(
    Nba97GameActorContactGateWord source, uint32_t constant) {
  Nba97GameActorContactGateWord value;
  set_known(&value, constant);
  return add_words(source, value);
}

static Nba97GameActorContactGateWord sra_word(
    Nba97GameActorContactGateWord value, unsigned shift) {
  Nba97GameActorContactGateWord result;
  unsigned byte;
  result.word = value.word >> shift;
  if (value.word & UINT32_C(0x80000000))
    result.word |= ~(UINT32_MAX >> shift);
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned low_bit = byte * 8u + shift;
    unsigned high_bit = low_bit + 7u;
    unsigned first_source = low_bit / 8u;
    unsigned last_source = high_bit < 32u ? high_bit / 8u : 3u;
    unsigned source;
    int known = 1;
    for (source = first_source; source <= last_source; ++source)
      if (!(value.known_mask & (1u << source)))
        known = 0;
    if (high_bit >= 32u && !(value.known_mask & 8u))
      known = 0;
    if (known)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static int64_t signed_word(uint32_t word) {
  return (word & UINT32_C(0x80000000)) ?
      (int64_t)word - INT64_C(0x100000000) : (int64_t)word;
}

static void signed_bounds(const Nba97GameActorContactGateWord *value,
                          int64_t *minimum, int64_t *maximum) {
  uint32_t low = 0;
  uint32_t high = 0;
  unsigned i;
  if (!(value->known_mask & 8u)) {
    *minimum = INT32_MIN;
    *maximum = INT32_MAX;
    return;
  }
  for (i = 0; i < 4; ++i) {
    uint32_t byte = (value->word >> (i * 8u)) & 0xffu;
    low |= ((value->known_mask & (1u << i)) ? byte : 0u) << (i * 8u);
    high |= ((value->known_mask & (1u << i)) ? byte : 0xffu) << (i * 8u);
  }
  *minimum = signed_word(low);
  *maximum = signed_word(high);
}

static Nba97GameActorContactGateWord signed_less_constant(
    const Nba97GameActorContactGateWord *value, int32_t constant) {
  Nba97GameActorContactGateWord result;
  int64_t minimum;
  int64_t maximum;
  signed_bounds(value, &minimum, &maximum);
  result.word = signed_word(value->word) < constant;
  result.known_mask = 0x0eu;
  if (maximum < constant)
    set_known(&result, 1);
  else if (minimum >= constant)
    set_known(&result, 0);
  return result;
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
                    const Nba97GameActorContactGateWord *value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameActorContactGateAccess *event = &run->context->access_journal[index];
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

static int read_value(Run *run, uint32_t address, uint32_t pc,
                      Nba97GameActorContactGateWord *value) {
  Nba97GameActorContactGateWord loaded = {0, 0};
  uint8_t *data;
  uint8_t *known;
  unsigned i;
  TRY(locate(run, address, 4, 4, pc, &data, &known));
  for (i = 0; i < 4; ++i) {
    loaded.word |= (uint32_t)data[i] << (i * 8u);
    if (!known || known[i])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
  }
  *value = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_MATCH_CLOCKS_READ, pc, address, 4, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, uint32_t address, uint32_t pc,
                       const Nba97GameActorContactGateWord *value) {
  Nba97GameActorContactGateWord stored = *value;
  uint8_t *data;
  uint8_t *known;
  unsigned i;
  TRY(locate(run, address, 4, 4, pc, &data, &known));
  if (!known && stored.known_mask != 0x0fu)
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < 4; ++i) {
    data[i] = (uint8_t)(stored.word >> (i * 8u));
    if (known)
      known[i] = (uint8_t)((stored.known_mask >> i) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, 4, &stored);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int register_address(Run *run, Nba97GameActorContactGateWord base,
                            uint32_t offset, uint32_t pc, uint32_t *address) {
  Nba97GameActorContactGateWord value = add_constant(base, offset);
  if (value.known_mask != 0x0fu) {
    stop(run, pc, value.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = value.word;
  return NBA97_TEXT_COMPLETE;
}

static int decide_zero(Run *run, const Nba97GameActorContactGateWord *value,
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
  Nba97GameActorContactGateEvent event;
  int accepted;
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x8005fad4));
  R(NBA97_MATCH_INITIALIZE_A2) =
      sra_word(R(NBA97_MATCH_INITIALIZE_A2), 8);
  run->out->shifted_difference = R(NBA97_MATCH_INITIALIZE_A2);
  stop(run, UINT32_C(0x8005facc), 0, UINT32_C(0x8005f948));
  TRY(spend(run));
  memset(&event, 0, sizeof event);
  event.pc = UINT32_C(0x8005facc);
  event.delay_slot_pc = UINT32_C(0x8005fad0);
  event.entry = UINT32_C(0x8005f948);
  event.operation = run->out->operations;
  event.invocation = run->out->call_count[
      NBA97_GAME_ACTOR_CONTACT_GATE_CHILD_8005F948] + 1u;
  event.kind = NBA97_GAME_ACTOR_CONTACT_GATE_CHILD_8005F948;
  event.argument_count = 3;
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
  ++run->out->call_count[NBA97_GAME_ACTOR_CONTACT_GATE_CHILD_8005F948];
  return NBA97_TEXT_COMPLETE;
}

static int restore_ra(Run *run) {
  uint32_t address;
  TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u,
                       UINT32_C(0x8005fad8), &address));
  TRY(read_value(run, address, UINT32_C(0x8005fad8),
                 &R(NBA97_MATCH_INITIALIZE_RA)));
  run->out->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_actor_contact_gate(Nba97GameActorContactGateContext *context,
                                  Nba97GameActorContactGateProgress *out) {
  Run storage;
  Run *run = &storage;
  Nba97GameActorContactGateWord branch_value;
  uint32_t address;
  int branch;
  TRY(validate(context, out, run));

  /* 0x8005FAA8..0x8005FAAC: allocate the wrapping frame and spill ra. */
  R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
      R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe8));
  out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u,
                       UINT32_C(0x8005faac), &address));
  out->saved_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  TRY(write_value(run, address, UINT32_C(0x8005faac),
                  &R(NBA97_MATCH_INITIALIZE_RA)));

  /* 0x8005FAB0..0x8005FAC8: read second then first, subtract with 32-bit
   * wrap, and retain SLTI's signed one-sided gate. The BEQ delay clears v0
   * before even an unknown predicate can stop. */
  TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_A1), 8,
                       UINT32_C(0x8005fab0), &address));
  TRY(read_value(run, address, UINT32_C(0x8005fab0),
                 &R(NBA97_MATCH_INITIALIZE_V0)));
  out->second_coordinate = R(NBA97_MATCH_INITIALIZE_V0);
  TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_A0), 8,
                       UINT32_C(0x8005fab4), &address));
  TRY(read_value(run, address, UINT32_C(0x8005fab4),
                 &R(NBA97_MATCH_INITIALIZE_V1)));
  out->first_coordinate = R(NBA97_MATCH_INITIALIZE_V1);
  R(NBA97_MATCH_INITIALIZE_A2) = subtract_words(
      R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
  out->coordinate_difference = R(NBA97_MATCH_INITIALIZE_A2);
  R(NBA97_MATCH_INITIALIZE_V0) = signed_less_constant(
      &R(NBA97_MATCH_INITIALIZE_A2), 0x1001);
  branch_value = R(NBA97_MATCH_INITIALIZE_V0);
  out->coordinate_gate = branch_value;
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0);
  TRY(decide_zero(run, &branch_value, UINT32_C(0x8005fac4), &branch));
  if (!branch) {
    /* 0x8005FACC..0x8005FAD4: JAL publishes ra before its delay shifts the
     * raw difference; only a completed child reaches the v0=1 overwrite. */
    TRY(invoke_child(run));
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1);
  }

  /* 0x8005FAD8..0x8005FAE4: restore ra through child-mutable live sp,
   * advance that sp, then consume the restored return after JR's NOP. */
  TRY(restore_ra(run));
  R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
      R(NBA97_MATCH_INITIALIZE_SP), 0x18u);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x8005fae0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
