#include "game_match_buffer_initialize.h"

#include <string.h>

typedef struct Run {
  Nba97GameMatchBufferInitializeContext *context;
  Nba97GameMatchBufferInitializeProgress *out;
  Nba97GameMatchBufferInitializeMachine machine;
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

static void set_known(Nba97GameMatchBufferInitializeWord *value,
                      uint32_t word) {
  value->word = word;
  value->known_mask = 0x0fu;
}

static int machine_valid(const Nba97GameMatchBufferInitializeMachine *machine) {
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

static int validate(Nba97GameMatchBufferInitializeContext *context,
                    Nba97GameMatchBufferInitializeProgress *out, Run *run) {
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

static Nba97GameMatchBufferInitializeWord add_words(
    Nba97GameMatchBufferInitializeWord left,
    Nba97GameMatchBufferInitializeWord right) {
  Nba97GameMatchBufferInitializeWord result;
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

static Nba97GameMatchBufferInitializeWord add_constant(
    Nba97GameMatchBufferInitializeWord source, uint32_t constant) {
  Nba97GameMatchBufferInitializeWord value;
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
                    const Nba97GameMatchBufferInitializeWord *value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameMatchBufferInitializeAccess *event =
        &run->context->access_journal[index];
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

static int read_word(Run *run, uint32_t address, uint32_t pc,
                     Nba97GameMatchBufferInitializeWord *value) {
  Nba97GameMatchBufferInitializeWord loaded = {0, 0};
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

static int write_value(Run *run, uint32_t address, uint8_t width, uint32_t pc,
                       const Nba97GameMatchBufferInitializeWord *value) {
  Nba97GameMatchBufferInitializeWord stored = *value;
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

static int register_address(Run *run,
    Nba97GameMatchBufferInitializeWord base, uint32_t offset, uint32_t pc,
    uint32_t *address) {
  Nba97GameMatchBufferInitializeWord value = add_constant(base, offset);
  if (value.known_mask != 0x0fu) {
    stop(run, pc, value.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = value.word;
  return NBA97_TEXT_COMPLETE;
}

static int invoke(Run *run, uint32_t pc, uint32_t entry, uint8_t kind,
                  uint8_t arguments) {
  Nba97GameMatchBufferInitializeEvent event;
  int accepted;
  stop(run, pc, 0, entry);
  TRY(spend(run));
  memset(&event, 0, sizeof event);
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = entry;
  event.operation = run->out->operations;
  event.invocation = run->out->call_count[kind] + 1u;
  event.kind = kind;
  event.argument_count = arguments;
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
  ++run->out->call_count[kind];
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_match_buffer_initialize(
    Nba97GameMatchBufferInitializeContext *context,
    Nba97GameMatchBufferInitializeProgress *out) {
  Run storage;
  Run *run = &storage;
  Nba97GameMatchBufferInitializeWord value;
  uint32_t address;
  TRY(validate(context, out, run));

  /* 0x8006432C..0x80064340: allocate the wrapping frame, form the fixed
   * zero destination, spill ra, then publish JAL ra before delay-slot length. */
  R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
      R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe0));
  out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  set_known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x800f9ffc));
  TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x18u,
                       UINT32_C(0x80064338), &address));
  out->saved_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  TRY(write_value(run, address, 4, UINT32_C(0x80064338),
                  &R(NBA97_MATCH_INITIALIZE_RA)));
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x80064344));
  set_known(&R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x378));
  TRY(invoke(run, UINT32_C(0x8006433c), UINT32_C(0x800a3a74),
             NBA97_GAME_MATCH_BUFFER_INITIALIZE_ZERO_800A3A74, 2));

  /* 0x80064344..0x8006436C: overwrite zero-child scratch with the three
   * fixed header values and publish halfword, first pointer, then second. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0x76u);
  set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  TRY(write_value(run, UINT32_C(0x800fa000), 2,
                  UINT32_C(0x8006434c), &R(NBA97_MATCH_INITIALIZE_V0)));
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x800ccc00));
  set_known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0x8b34));
  set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  TRY(write_value(run, UINT32_C(0x800fa004), 4,
                  UINT32_C(0x80064360), &R(NBA97_MATCH_INITIALIZE_V0)));
  R(NBA97_MATCH_INITIALIZE_V0) = add_words(
      R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
  set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80100000));
  TRY(write_value(run, UINT32_C(0x800fa008), 4,
                  UINT32_C(0x8006436c), &R(NBA97_MATCH_INITIALIZE_V0)));

  /* 0x80064370..0x80064374: the second JAL publishes ra before its NOP; no
   * argument register is normalized for this zero-argument typed boundary. */
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x80064378));
  TRY(invoke(run, UINT32_C(0x80064370), UINT32_C(0x80076ad0),
             NBA97_GAME_MATCH_BUFFER_INITIALIZE_CHILD_80076AD0, 0));

  /* 0x80064378..0x80064384: reload ra through callback-live sp, advance that
   * sp by 0x20, and consume restored ra after JR's NOP. */
  TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x18u,
                       UINT32_C(0x80064378), &address));
  TRY(read_word(run, address, UINT32_C(0x80064378),
                &R(NBA97_MATCH_INITIALIZE_RA)));
  out->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
      R(NBA97_MATCH_INITIALIZE_SP), 0x20u);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x80064380), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  value = R(NBA97_MATCH_INITIALIZE_V0);
  out->completed = 1;
  stop(run, 0, 0, 0);
  out->returned_value = value;
  return NBA97_TEXT_COMPLETE;
}
