#include "frontend_load_payload.h"

#include <string.h>

typedef Nba97FrontendLoadPayloadWord Word;

typedef struct Run {
  Nba97FrontendLoadPayloadContext *context;
  Nba97FrontendLoadPayloadProgress *out;
  Nba97FrontendLoadPayloadMachine machine;
} Run;

#define REG(index) (run->machine.registers.gpr[(index)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int result_ = (expression);                                                \
    if (result_ != NBA97_TEXT_COMPLETE)                                        \
      return result_;                                                          \
  } while (0)
#define STEP(pc_)                                                              \
  do {                                                                         \
    size_t index_ = run->out->instruction_events++;                            \
    if (index_ < run->context->instruction_journal_capacity)                   \
      run->context->instruction_journal[index_] = UINT32_C(pc_);               \
    ++run->out->instruction_count;                                             \
  } while (0)

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t target) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  run->out->stopped_target = target;
  publish(run);
}

static int machine_valid(const Nba97FrontendLoadPayloadMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_FRONTEND_LOAD_PAYLOAD_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int initialize(Nba97FrontendLoadPayloadContext *context,
                      Nba97FrontendLoadPayloadProgress *out, Run *run) {
  size_t i;
  size_t j;
  if (!out)
    return NBA97_TEXT_ARGUMENT;
  memset(out, 0, sizeof *out);
  if (!context || (!context->memory.region && context->memory.count) ||
      (!context->access_journal && context->access_journal_capacity) ||
      (!context->instruction_journal && context->instruction_journal_capacity) ||
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
    Nba97FrontendLoadPayloadAccess *event =
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

static int address(Run *run, Word base, uint32_t offset, uint32_t pc,
                   uint32_t *result) {
  Word computed = add_constant(base, offset);
  if (computed.known_mask != 0x0fu) {
    stop(run, pc, computed.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = computed.word;
  return NBA97_TEXT_COMPLETE;
}

static int locate(Run *run, uint32_t guest, uint32_t pc, uint8_t **data,
                  uint8_t **known) {
  size_t i;
  unsigned byte;
  stop(run, pc, guest, 0);
  TRY(spend(run));
  ++run->out->accesses;
  if (guest & 3u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0; i < run->context->memory.count; ++i) {
    Nba97GameTextRegion *region = &run->context->memory.region[i];
    uint64_t offset = (uint64_t)guest - region->base;
    if (guest < region->base || offset > region->size ||
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

static int store_word(Run *run, Word value, Word base, uint32_t offset,
                      uint32_t pc) {
  uint32_t guest;
  uint8_t *data;
  uint8_t *known;
  unsigned byte;
  TRY(address(run, base, offset, pc, &guest));
  TRY(locate(run, guest, pc, &data, &known));
  if (!known && value.known_mask != 0x0fu)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0; byte < 4; ++byte) {
    data[byte] = (uint8_t)(value.word >> (byte * 8u));
    if (known)
      known[byte] = (uint8_t)((value.known_mask >> byte) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_FRONTEND_LOAD_PAYLOAD_STORE, pc, guest, &value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int load_word(Run *run, Word base, uint32_t offset, uint32_t pc,
                     Word *loaded) {
  uint32_t guest;
  uint8_t *data;
  uint8_t *known;
  unsigned byte;
  TRY(address(run, base, offset, pc, &guest));
  TRY(locate(run, guest, pc, &data, &known));
  loaded->word = 0;
  loaded->known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    loaded->word |= (uint32_t)data[byte] << (byte * 8u);
    if (!known || known[byte])
      loaded->known_mask =
          (uint8_t)(loaded->known_mask | (uint8_t)(1u << byte));
  }
  ++run->out->reads;
  journal(run, NBA97_FRONTEND_LOAD_PAYLOAD_READ, pc, guest, loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int nonzero_decision(Run *run, Word value, uint32_t pc,
                            int *is_nonzero) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((value.known_mask & (1u << byte)) &&
        ((value.word >> (byte * 8u)) & 0xffu)) {
      *is_nonzero = 1;
      return NBA97_TEXT_COMPLETE;
    }
  if (value.known_mask == 0x0fu) {
    *is_nonzero = 0;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

static int invoke(Run *run) {
  Nba97FrontendLoadPayloadEvent event;
  int accepted;
  stop(run, UINT32_C(0x8007b164), 0, UINT32_C(0x8007b1d0));
  TRY(spend(run));
  ++run->out->call_attempts[NBA97_FRONTEND_LOAD_PAYLOAD_SITE_8007B164];
  memset(&event, 0, sizeof event);
  event.pc = UINT32_C(0x8007b164);
  event.delay_slot_pc = UINT32_C(0x8007b168);
  event.entry = UINT32_C(0x8007b1d0);
  event.operation = run->out->operations;
  event.invocation =
      run->out->call_attempts[NBA97_FRONTEND_LOAD_PAYLOAD_SITE_8007B164];
  event.site = NBA97_FRONTEND_LOAD_PAYLOAD_SITE_8007B164;
  event.argument_count = 3;
  event.target_program = NBA97_FRONTEND_MAIN_PROGRAM_FEONLY;
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
  ++run->out->call_count[NBA97_FRONTEND_LOAD_PAYLOAD_SITE_8007B164];
  return NBA97_TEXT_COMPLETE;
}

int nba97_frontend_load_payload(Nba97FrontendLoadPayloadContext *context,
                                Nba97FrontendLoadPayloadProgress *out) {
  Run storage;
  Run *run = &storage;
  Word branch_value;
  Word loaded;
  int decision;
  TRY(initialize(context, out, run));

  /* 0x8007B15C..0x8007B168: allocate the frame, save entry ra, and call the
   * unresolved loader with live a0/a1/a2 and a NOP delay. */
  STEP(0x8007b15c);
  REG(NBA97_FRONTEND_LOAD_PAYLOAD_SP) = add_constant(
      REG(NBA97_FRONTEND_LOAD_PAYLOAD_SP), UINT32_C(0xffffffe8));
  out->frame_stack_pointer = REG(NBA97_FRONTEND_LOAD_PAYLOAD_SP).word;
  publish(run);
  STEP(0x8007b160);
  TRY(store_word(run, REG(NBA97_FRONTEND_LOAD_PAYLOAD_RA),
                 REG(NBA97_FRONTEND_LOAD_PAYLOAD_SP), 16,
                 UINT32_C(0x8007b160)));
  out->saved_return_address = REG(NBA97_FRONTEND_LOAD_PAYLOAD_RA);
  STEP(0x8007b164);
  set_known(&REG(NBA97_FRONTEND_LOAD_PAYLOAD_RA), UINT32_C(0x8007b16c));
  STEP(0x8007b168);
  out->forwarded_a0 = REG(NBA97_FRONTEND_LOAD_PAYLOAD_A0);
  out->forwarded_a1 = REG(NBA97_FRONTEND_LOAD_PAYLOAD_A1);
  out->forwarded_a2 = REG(NBA97_FRONTEND_LOAD_PAYLOAD_A2);
  publish(run);
  TRY(invoke(run));
  out->child_return = REG(NBA97_FRONTEND_LOAD_PAYLOAD_V0);

  /* 0x8007B16C..0x8007B17C: the BNE latches child v0 before its NOP. A
   * definitely nonzero partial pointer selects the load but may not address. */
  branch_value = REG(NBA97_FRONTEND_LOAD_PAYLOAD_V0);
  STEP(0x8007b16c);
  STEP(0x8007b170);
  TRY(nonzero_decision(run, branch_value, UINT32_C(0x8007b16c), &decision));
  if (!decision) {
    STEP(0x8007b174);
    STEP(0x8007b178);
    set_known(&REG(NBA97_FRONTEND_LOAD_PAYLOAD_V0), 0);
    out->payload_result = REG(NBA97_FRONTEND_LOAD_PAYLOAD_V0);
  } else {
    STEP(0x8007b17c);
    TRY(load_word(run, branch_value, 0, UINT32_C(0x8007b17c), &loaded));
    REG(NBA97_FRONTEND_LOAD_PAYLOAD_V0) = loaded;
    out->payload_result = loaded;
    publish(run);
  }

  /* 0x8007B180..0x8007B18C: callback-live sp selects the saved-ra load;
   * the source then adds 24 with wrap and consumes restored ra after NOP. */
  STEP(0x8007b180);
  TRY(load_word(run, REG(NBA97_FRONTEND_LOAD_PAYLOAD_SP), 16,
                UINT32_C(0x8007b180), &loaded));
  REG(NBA97_FRONTEND_LOAD_PAYLOAD_RA) = loaded;
  out->restored_return_address = loaded;
  publish(run);
  STEP(0x8007b184);
  REG(NBA97_FRONTEND_LOAD_PAYLOAD_SP) = add_constant(
      REG(NBA97_FRONTEND_LOAD_PAYLOAD_SP), 24);
  publish(run);
  STEP(0x8007b188);
  STEP(0x8007b18c);
  if (REG(NBA97_FRONTEND_LOAD_PAYLOAD_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x8007b188), 0,
         REG(NBA97_FRONTEND_LOAD_PAYLOAD_RA).word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (REG(NBA97_FRONTEND_LOAD_PAYLOAD_RA).word & 3u) {
    stop(run, UINT32_C(0x8007b188), 0,
         REG(NBA97_FRONTEND_LOAD_PAYLOAD_RA).word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
