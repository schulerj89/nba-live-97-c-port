#include "frontend_resource_load.h"

#include <string.h>

typedef Nba97FrontendResourceLoadWord Word;

typedef struct Run {
  Nba97FrontendResourceLoadContext *context;
  Nba97FrontendResourceLoadProgress *out;
  Nba97FrontendResourceLoadMachine machine;
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

static int machine_valid(const Nba97FrontendResourceLoadMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_FRONTEND_RESOURCE_LOAD_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int initialize(Nba97FrontendResourceLoadContext *context,
                      Nba97FrontendResourceLoadProgress *out, Run *run) {
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
    Nba97FrontendResourceLoadAccess *event =
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

static int address(Run *run, unsigned base, uint32_t offset, uint32_t pc,
                   uint32_t *result) {
  Word computed = add_constant(REG(base), offset);
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

static int store_word(Run *run, unsigned source, unsigned base,
                      uint32_t offset, uint32_t pc) {
  uint32_t guest;
  uint8_t *data;
  uint8_t *known;
  unsigned byte;
  TRY(address(run, base, offset, pc, &guest));
  TRY(locate(run, guest, pc, &data, &known));
  if (!known && REG(source).known_mask != 0x0fu)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0; byte < 4; ++byte) {
    data[byte] = (uint8_t)(REG(source).word >> (byte * 8u));
    if (known)
      known[byte] = (uint8_t)((REG(source).known_mask >> byte) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_FRONTEND_RESOURCE_LOAD_STORE, pc, guest, &REG(source));
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int load_word(Run *run, unsigned destination, unsigned base,
                     uint32_t offset, uint32_t pc) {
  uint32_t guest;
  uint8_t *data;
  uint8_t *known;
  Word loaded = {0, 0};
  unsigned byte;
  TRY(address(run, base, offset, pc, &guest));
  TRY(locate(run, guest, pc, &data, &known));
  for (byte = 0; byte < 4; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (byte * 8u);
    if (!known || known[byte])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << byte));
  }
  REG(destination) = loaded;
  ++run->out->reads;
  journal(run, NBA97_FRONTEND_RESOURCE_LOAD_READ, pc, guest, &loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int zero_decision(Run *run, Word value, uint32_t pc, int *is_zero) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((value.known_mask & (1u << byte)) &&
        ((value.word >> (byte * 8u)) & 0xffu)) {
      *is_zero = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (value.known_mask == 0x0fu) {
    *is_zero = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}

static int invoke(Run *run, uint8_t site, uint32_t pc, uint32_t delay_pc,
                  uint32_t target, uint8_t argument_count) {
  Nba97FrontendResourceLoadEvent event;
  int accepted;
  stop(run, pc, 0, target);
  TRY(spend(run));
  ++run->out->call_attempts[site];
  memset(&event, 0, sizeof event);
  event.pc = pc;
  event.delay_slot_pc = delay_pc;
  event.entry = target;
  event.operation = run->out->operations;
  event.invocation = run->out->call_attempts[site];
  event.site = site;
  event.argument_count = argument_count;
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
  ++run->out->call_count[site];
  return NBA97_TEXT_COMPLETE;
}

int nba97_frontend_resource_load(Nba97FrontendResourceLoadContext *context,
                                 Nba97FrontendResourceLoadProgress *out) {
  Run storage;
  Run *run = &storage;
  int decision;
  uint32_t dynamic_target;
  TRY(initialize(context, out, run));
  out->input_filename = REG(NBA97_FRONTEND_RESOURCE_LOAD_A0);
  out->input_flags = REG(NBA97_FRONTEND_RESOURCE_LOAD_A1);
  out->input_mode = REG(NBA97_FRONTEND_RESOURCE_LOAD_A2);

  /* 0x8007B1D0..0x8007B1F4: create the 64-byte frame, retain all three
   * arguments in saved registers, and save s0 in the first JAL delay slot. */
  STEP(0x8007b1d0);
  REG(NBA97_FRONTEND_RESOURCE_LOAD_SP) = add_constant(
      REG(NBA97_FRONTEND_RESOURCE_LOAD_SP), UINT32_C(0xffffffc0));
  out->frame_stack_pointer = REG(NBA97_FRONTEND_RESOURCE_LOAD_SP).word;
  publish(run);
  STEP(0x8007b1d4);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_LOAD_S1,
                 NBA97_FRONTEND_RESOURCE_LOAD_SP, 44, UINT32_C(0x8007b1d4)));
  out->saved_s1 = REG(NBA97_FRONTEND_RESOURCE_LOAD_S1);
  STEP(0x8007b1d8);
  REG(NBA97_FRONTEND_RESOURCE_LOAD_S1) =
      REG(NBA97_FRONTEND_RESOURCE_LOAD_A0);
  STEP(0x8007b1dc);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_LOAD_S3,
                 NBA97_FRONTEND_RESOURCE_LOAD_SP, 52, UINT32_C(0x8007b1dc)));
  out->saved_s3 = REG(NBA97_FRONTEND_RESOURCE_LOAD_S3);
  STEP(0x8007b1e0);
  REG(NBA97_FRONTEND_RESOURCE_LOAD_S3) =
      REG(NBA97_FRONTEND_RESOURCE_LOAD_A1);
  STEP(0x8007b1e4);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_LOAD_S2,
                 NBA97_FRONTEND_RESOURCE_LOAD_SP, 48, UINT32_C(0x8007b1e4)));
  out->saved_s2 = REG(NBA97_FRONTEND_RESOURCE_LOAD_S2);
  STEP(0x8007b1e8);
  REG(NBA97_FRONTEND_RESOURCE_LOAD_S2) =
      REG(NBA97_FRONTEND_RESOURCE_LOAD_A2);
  STEP(0x8007b1ec);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_LOAD_RA,
                 NBA97_FRONTEND_RESOURCE_LOAD_SP, 56, UINT32_C(0x8007b1ec)));
  out->saved_return_address = REG(NBA97_FRONTEND_RESOURCE_LOAD_RA);
  STEP(0x8007b1f0);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_LOAD_RA), UINT32_C(0x8007b1f8));
  STEP(0x8007b1f4);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_LOAD_S0,
                 NBA97_FRONTEND_RESOURCE_LOAD_SP, 40, UINT32_C(0x8007b1f4)));
  out->saved_s0 = REG(NBA97_FRONTEND_RESOURCE_LOAD_S0);
  TRY(invoke(run, NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B1F0,
             UINT32_C(0x8007b1f0), UINT32_C(0x8007b1f4),
             UINT32_C(0x8008a2c8), 1));

  /* 0x8007B1F8..0x8007B218: discard any nonzero cached result, form three
   * output pointers, then store the fifth argument before invoking the child. */
  STEP(0x8007b1f8);
  REG(NBA97_FRONTEND_RESOURCE_LOAD_S0) =
      REG(NBA97_FRONTEND_RESOURCE_LOAD_V0);
  out->cached_lookup_result = REG(NBA97_FRONTEND_RESOURCE_LOAD_S0);
  STEP(0x8007b1fc);
  STEP(0x8007b200);
  REG(NBA97_FRONTEND_RESOURCE_LOAD_A0) =
      REG(NBA97_FRONTEND_RESOURCE_LOAD_S1);
  TRY(zero_decision(run, REG(NBA97_FRONTEND_RESOURCE_LOAD_S0),
                    UINT32_C(0x8007b1fc), &decision));
  if (!decision) {
    STEP(0x8007b204);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_LOAD_S0), 0);
    out->cached_result_discarded = 1;
  }
  STEP(0x8007b208);
  REG(NBA97_FRONTEND_RESOURCE_LOAD_A1) = add_constant(
      REG(NBA97_FRONTEND_RESOURCE_LOAD_SP), 24);
  STEP(0x8007b20c);
  REG(NBA97_FRONTEND_RESOURCE_LOAD_A2) = add_constant(
      REG(NBA97_FRONTEND_RESOURCE_LOAD_SP), 28);
  STEP(0x8007b210);
  REG(NBA97_FRONTEND_RESOURCE_LOAD_A3) = add_constant(
      REG(NBA97_FRONTEND_RESOURCE_LOAD_SP), 32);
  STEP(0x8007b214);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_LOAD_RA), UINT32_C(0x8007b21c));
  STEP(0x8007b218);
  TRY(store_word(run, NBA97_FRONTEND_RESOURCE_LOAD_S2,
                 NBA97_FRONTEND_RESOURCE_LOAD_SP, 16, UINT32_C(0x8007b218)));
  TRY(invoke(run, NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B214,
             UINT32_C(0x8007b214), UINT32_C(0x8007b218),
             UINT32_C(0x8008a594), 5));

  /* 0x8007B21C..0x8007B26C: reload the size written by the child. A zero
   * size skips allocation; a null allocation still closes the descriptor. */
  STEP(0x8007b21c);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOAD_A1,
                NBA97_FRONTEND_RESOURCE_LOAD_SP, 32,
                UINT32_C(0x8007b21c)));
  out->file_size = REG(NBA97_FRONTEND_RESOURCE_LOAD_A1);
  STEP(0x8007b220);
  STEP(0x8007b224);
  STEP(0x8007b228);
  REG(NBA97_FRONTEND_RESOURCE_LOAD_A0) =
      REG(NBA97_FRONTEND_RESOURCE_LOAD_S1);
  TRY(zero_decision(run, REG(NBA97_FRONTEND_RESOURCE_LOAD_A1),
                    UINT32_C(0x8007b224), &decision));
  if (!decision) {
    STEP(0x8007b22c);
    REG(NBA97_FRONTEND_RESOURCE_LOAD_A2) =
        REG(NBA97_FRONTEND_RESOURCE_LOAD_S3);
    STEP(0x8007b230);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_LOAD_RA), UINT32_C(0x8007b238));
    STEP(0x8007b234);
    REG(NBA97_FRONTEND_RESOURCE_LOAD_A3) =
        REG(NBA97_FRONTEND_RESOURCE_LOAD_S2);
    TRY(invoke(run, NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B230,
               UINT32_C(0x8007b230), UINT32_C(0x8007b234),
               UINT32_C(0x80077160), 4));
    STEP(0x8007b238);
    REG(NBA97_FRONTEND_RESOURCE_LOAD_S0) =
        REG(NBA97_FRONTEND_RESOURCE_LOAD_V0);
    out->allocation_result = REG(NBA97_FRONTEND_RESOURCE_LOAD_S0);
    STEP(0x8007b23c);
    STEP(0x8007b240);
    TRY(zero_decision(run, REG(NBA97_FRONTEND_RESOURCE_LOAD_S0),
                      UINT32_C(0x8007b23c), &decision));
    if (!decision) {
      STEP(0x8007b244);
      TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOAD_A0,
                    NBA97_FRONTEND_RESOURCE_LOAD_SP, 24,
                    UINT32_C(0x8007b244)));
      STEP(0x8007b248);
      TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOAD_A1,
                    NBA97_FRONTEND_RESOURCE_LOAD_S0, 0,
                    UINT32_C(0x8007b248)));
      out->descriptor_word = REG(NBA97_FRONTEND_RESOURCE_LOAD_A1);
      STEP(0x8007b24c);
      TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOAD_A2,
                    NBA97_FRONTEND_RESOURCE_LOAD_SP, 32,
                    UINT32_C(0x8007b24c)));
      STEP(0x8007b250);
      set_known(&REG(NBA97_FRONTEND_RESOURCE_LOAD_RA), UINT32_C(0x8007b258));
      STEP(0x8007b254);
      TRY(invoke(run, NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B250,
                 UINT32_C(0x8007b250), UINT32_C(0x8007b254),
                 UINT32_C(0x8008a810), 3));
      STEP(0x8007b258);
      TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOAD_V0,
                    NBA97_FRONTEND_RESOURCE_LOAD_SP, 32,
                    UINT32_C(0x8007b258)));
      STEP(0x8007b25c);
      set_known(&REG(NBA97_FRONTEND_RESOURCE_LOAD_AT), UINT32_C(0x800e0000));
      STEP(0x8007b260);
      TRY(store_word(run, NBA97_FRONTEND_RESOURCE_LOAD_V0,
                     NBA97_FRONTEND_RESOURCE_LOAD_AT, UINT32_C(0xffff9ae8),
                     UINT32_C(0x8007b260)));
    }
    STEP(0x8007b264);
    TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOAD_A0,
                  NBA97_FRONTEND_RESOURCE_LOAD_SP, 24,
                  UINT32_C(0x8007b264)));
    STEP(0x8007b268);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_LOAD_RA), UINT32_C(0x8007b270));
    STEP(0x8007b26c);
    TRY(invoke(run, NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B268,
               UINT32_C(0x8007b268), UINT32_C(0x8007b26c),
               UINT32_C(0x8008a7b0), 1));
  }

  /* 0x8007B270..0x8007B294: reload the optional callback pointer, latch it
   * for JALR validation, and apply all four live arguments before invocation. */
  STEP(0x8007b270);
  set_known(&REG(NBA97_FRONTEND_RESOURCE_LOAD_V0), UINT32_C(0x800e0000));
  STEP(0x8007b274);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOAD_V0,
                NBA97_FRONTEND_RESOURCE_LOAD_V0, UINT32_C(0xffff9b50),
                UINT32_C(0x8007b274)));
  out->callback_pointer = REG(NBA97_FRONTEND_RESOURCE_LOAD_V0);
  STEP(0x8007b278);
  STEP(0x8007b27c);
  STEP(0x8007b280);
  REG(NBA97_FRONTEND_RESOURCE_LOAD_A0) =
      REG(NBA97_FRONTEND_RESOURCE_LOAD_S0);
  TRY(zero_decision(run, out->callback_pointer, UINT32_C(0x8007b27c),
                    &decision));
  if (!decision) {
    STEP(0x8007b284);
    REG(NBA97_FRONTEND_RESOURCE_LOAD_A1) =
        REG(NBA97_FRONTEND_RESOURCE_LOAD_S1);
    STEP(0x8007b288);
    REG(NBA97_FRONTEND_RESOURCE_LOAD_A2) =
        REG(NBA97_FRONTEND_RESOURCE_LOAD_S3);
    dynamic_target = out->callback_pointer.word;
    STEP(0x8007b28c);
    set_known(&REG(NBA97_FRONTEND_RESOURCE_LOAD_RA), UINT32_C(0x8007b294));
    STEP(0x8007b290);
    REG(NBA97_FRONTEND_RESOURCE_LOAD_A3) =
        REG(NBA97_FRONTEND_RESOURCE_LOAD_S2);
    if (out->callback_pointer.known_mask != 0x0fu) {
      stop(run, UINT32_C(0x8007b28c), 0, dynamic_target);
      return NBA97_TEXT_UNKNOWN;
    }
    if (dynamic_target & 3u) {
      stop(run, UINT32_C(0x8007b28c), 0, dynamic_target);
      return NBA97_TEXT_ALIGNMENT_TRAP;
    }
    TRY(invoke(run, NBA97_FRONTEND_RESOURCE_LOAD_SITE_8007B28C,
               UINT32_C(0x8007b28c), UINT32_C(0x8007b290), dynamic_target, 4));
    STEP(0x8007b294);
    REG(NBA97_FRONTEND_RESOURCE_LOAD_S0) =
        REG(NBA97_FRONTEND_RESOURCE_LOAD_V0);
    out->dynamic_return = REG(NBA97_FRONTEND_RESOURCE_LOAD_S0);
  }

  /* 0x8007B298..0x8007B2B8: return s0 in v0, then restore every saved word
   * through callback-live sp and consume the restored ra after the NOP. */
  STEP(0x8007b298);
  REG(NBA97_FRONTEND_RESOURCE_LOAD_V0) =
      REG(NBA97_FRONTEND_RESOURCE_LOAD_S0);
  STEP(0x8007b29c);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOAD_RA,
                NBA97_FRONTEND_RESOURCE_LOAD_SP, 56,
                UINT32_C(0x8007b29c)));
  out->restored_return_address = REG(NBA97_FRONTEND_RESOURCE_LOAD_RA);
  STEP(0x8007b2a0);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOAD_S3,
                NBA97_FRONTEND_RESOURCE_LOAD_SP, 52,
                UINT32_C(0x8007b2a0)));
  out->restored_s3 = REG(NBA97_FRONTEND_RESOURCE_LOAD_S3);
  STEP(0x8007b2a4);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOAD_S2,
                NBA97_FRONTEND_RESOURCE_LOAD_SP, 48,
                UINT32_C(0x8007b2a4)));
  out->restored_s2 = REG(NBA97_FRONTEND_RESOURCE_LOAD_S2);
  STEP(0x8007b2a8);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOAD_S1,
                NBA97_FRONTEND_RESOURCE_LOAD_SP, 44,
                UINT32_C(0x8007b2a8)));
  out->restored_s1 = REG(NBA97_FRONTEND_RESOURCE_LOAD_S1);
  STEP(0x8007b2ac);
  TRY(load_word(run, NBA97_FRONTEND_RESOURCE_LOAD_S0,
                NBA97_FRONTEND_RESOURCE_LOAD_SP, 40,
                UINT32_C(0x8007b2ac)));
  out->restored_s0 = REG(NBA97_FRONTEND_RESOURCE_LOAD_S0);
  STEP(0x8007b2b0);
  REG(NBA97_FRONTEND_RESOURCE_LOAD_SP) = add_constant(
      REG(NBA97_FRONTEND_RESOURCE_LOAD_SP), 64);
  publish(run);
  STEP(0x8007b2b4);
  STEP(0x8007b2b8);
  if (REG(NBA97_FRONTEND_RESOURCE_LOAD_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x8007b2b4), 0,
         REG(NBA97_FRONTEND_RESOURCE_LOAD_RA).word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (REG(NBA97_FRONTEND_RESOURCE_LOAD_RA).word & 3u) {
    stop(run, UINT32_C(0x8007b2b4), 0,
         REG(NBA97_FRONTEND_RESOURCE_LOAD_RA).word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
