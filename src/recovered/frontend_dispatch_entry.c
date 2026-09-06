#include "frontend_dispatch_entry.h"

#include <string.h>

typedef Nba97FrontendDispatchEntryWord Word;

typedef struct Run {
  Nba97FrontendDispatchEntryContext *context;
  Nba97FrontendDispatchEntryProgress *out;
  Nba97FrontendDispatchEntryMachine machine;
} Run;

#define REG(index) (run->machine.registers.gpr[(index)])
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

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t target) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  run->out->stopped_target = target;
  publish(run);
}

static int machine_valid(const Nba97FrontendDispatchEntryMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_FRONTEND_DISPATCH_ENTRY_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int initialize(Nba97FrontendDispatchEntryContext *context,
                      Nba97FrontendDispatchEntryProgress *out, Run *run) {
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

/* Enumerating the carry inputs preserves byte knownness through ADDIU. */
static Word add_constant(Word input, uint32_t constant) {
  Word result;
  unsigned byte;
  unsigned carry_mask = 1u;
  result.word = input.word + constant;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_carry_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned carry;
    unsigned start = (input.known_mask & (1u << byte))
                         ? ((input.word >> (byte * 8u)) & 0xffu)
                         : 0u;
    unsigned end = (input.known_mask & (1u << byte)) ? start : 255u;
    unsigned addend = (constant >> (byte * 8u)) & 0xffu;
    for (carry = 0; carry < 2; ++carry) {
      unsigned source;
      if (!(carry_mask & (1u << carry)))
        continue;
      for (source = start; source <= end; ++source) {
        unsigned sum = source + addend + carry;
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
    if (invariant)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
    carry_mask = next_carry_mask;
  }
  return result;
}

static void set_known(Word *word, uint32_t value) {
  word->word = value;
  word->known_mask = 0x0fu;
}

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    const Word *value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97FrontendDispatchEntryAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word;
    event->operation = run->out->operations;
    event->width = 4;
    event->known_mask = value->known_mask;
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t address, uint32_t pc, uint8_t **data,
                  uint8_t **known) {
  size_t i;
  unsigned byte;
  stop(run, pc, address, 0);
  TRY(spend(run));
  ++run->out->accesses;
  if (address & 3u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0; i < run->context->memory.count; ++i) {
    Nba97GameTextRegion *region = &run->context->memory.region[i];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        4u > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known = region->known ? region->known + (size_t)offset : 0;
    if (*known)
      for (byte = 0; byte < 4; ++byte)
        if ((*known)[byte] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int effective_address(Run *run, unsigned base, uint32_t offset,
                             uint32_t pc, uint32_t *address) {
  Word computed = add_constant(REG(base), offset);
  if (computed.known_mask != 0x0fu) {
    stop(run, pc, computed.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = computed.word;
  return NBA97_TEXT_COMPLETE;
}

static int store_word(Run *run, unsigned source, unsigned base,
                      uint32_t offset, uint32_t pc) {
  uint32_t address;
  uint8_t *data;
  uint8_t *known;
  unsigned byte;
  TRY(effective_address(run, base, offset, pc, &address));
  TRY(locate(run, address, pc, &data, &known));
  if (!known && REG(source).known_mask != 0x0fu)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0; byte < 4; ++byte) {
    data[byte] = (uint8_t)(REG(source).word >> (byte * 8u));
    if (known)
      known[byte] = (uint8_t)((REG(source).known_mask >> byte) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_FRONTEND_DISPATCH_ENTRY_STORE, pc, address, &REG(source));
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int load_word(Run *run, unsigned destination, unsigned base,
                     uint32_t offset, uint32_t pc) {
  uint32_t address;
  uint8_t *data;
  uint8_t *known;
  Word loaded = {0, 0};
  unsigned byte;
  TRY(effective_address(run, base, offset, pc, &address));
  TRY(locate(run, address, pc, &data, &known));
  for (byte = 0; byte < 4; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (byte * 8u);
    if (!known || known[byte])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << byte));
  }
  REG(destination) = loaded;
  ++run->out->reads;
  journal(run, NBA97_FRONTEND_DISPATCH_ENTRY_READ, pc, address, &loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int invoke_dispatcher(Run *run) {
  Nba97FrontendDispatchEntryEvent event;
  int accepted;
  stop(run, UINT32_C(0x800360f4), 0, UINT32_C(0x8003f7c8));
  TRY(spend(run));
  ++run->out->callback_attempts;
  memset(&event, 0, sizeof event);
  event.pc = UINT32_C(0x800360f4);
  event.delay_slot_pc = UINT32_C(0x800360f8);
  event.entry = UINT32_C(0x8003f7c8);
  event.operation = run->out->operations;
  event.invocation = run->out->callback_attempts;
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
  return NBA97_TEXT_COMPLETE;
}

int nba97_frontend_dispatch_entry(Nba97FrontendDispatchEntryContext *context,
                                  Nba97FrontendDispatchEntryProgress *out) {
  Run storage;
  Run *run = &storage;
  TRY(initialize(context, out, run));

  /* 0x800360D4..0x800360E0: allocate the frame and publish the initialized
   * flag before the incoming ra has been saved. */
  STEP(0x800360d4);
  REG(NBA97_FRONTEND_DISPATCH_ENTRY_SP) = add_constant(
      REG(NBA97_FRONTEND_DISPATCH_ENTRY_SP), UINT32_C(0xffffffe8));
  out->frame_stack_pointer = REG(NBA97_FRONTEND_DISPATCH_ENTRY_SP).word;
  STEP(0x800360d8);
  set_known(&REG(NBA97_FRONTEND_DISPATCH_ENTRY_V0), 1);
  STEP(0x800360dc);
  set_known(&REG(NBA97_FRONTEND_DISPATCH_ENTRY_AT), UINT32_C(0x80020000));
  STEP(0x800360e0);
  TRY(store_word(run, NBA97_FRONTEND_DISPATCH_ENTRY_V0,
                 NBA97_FRONTEND_DISPATCH_ENTRY_AT, UINT32_C(0x1ee4),
                 UINT32_C(0x800360e0)));

  /* 0x800360E4..0x800360F0: save the exact entry ra after the flag, then
   * publish the independent value 32 global. */
  STEP(0x800360e4);
  set_known(&REG(NBA97_FRONTEND_DISPATCH_ENTRY_V0), 32);
  STEP(0x800360e8);
  TRY(store_word(run, NBA97_FRONTEND_DISPATCH_ENTRY_RA,
                 NBA97_FRONTEND_DISPATCH_ENTRY_SP, 16,
                 UINT32_C(0x800360e8)));
  out->saved_return_address = REG(NBA97_FRONTEND_DISPATCH_ENTRY_RA);
  STEP(0x800360ec);
  set_known(&REG(NBA97_FRONTEND_DISPATCH_ENTRY_AT), UINT32_C(0x800c0000));
  STEP(0x800360f0);
  TRY(store_word(run, NBA97_FRONTEND_DISPATCH_ENTRY_V0,
                 NBA97_FRONTEND_DISPATCH_ENTRY_AT, UINT32_C(0x6e68),
                 UINT32_C(0x800360f0)));

  /* JAL 0x800360F4 writes ra before its NOP delay at 0x800360F8. */
  STEP(0x800360f4);
  set_known(&REG(NBA97_FRONTEND_DISPATCH_ENTRY_RA), UINT32_C(0x800360fc));
  STEP(0x800360f8);
  TRY(invoke_dispatcher(run));

  /* 0x800360FC..0x80036108: reload through callback-live sp, then consume
   * callback/live memory ra after the JR NOP delay has executed. */
  STEP(0x800360fc);
  TRY(load_word(run, NBA97_FRONTEND_DISPATCH_ENTRY_RA,
                NBA97_FRONTEND_DISPATCH_ENTRY_SP, 16,
                UINT32_C(0x800360fc)));
  out->restored_return_address = REG(NBA97_FRONTEND_DISPATCH_ENTRY_RA);
  STEP(0x80036100);
  REG(NBA97_FRONTEND_DISPATCH_ENTRY_SP) = add_constant(
      REG(NBA97_FRONTEND_DISPATCH_ENTRY_SP), 24);
  publish(run);
  STEP(0x80036104);
  STEP(0x80036108);
  if (REG(NBA97_FRONTEND_DISPATCH_ENTRY_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x80036104), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
