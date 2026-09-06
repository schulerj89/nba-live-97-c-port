#include "frontend_exit_cleanup.h"

#include <string.h>

typedef Nba97FrontendExitCleanupWord Word;

typedef struct Run {
  Nba97FrontendExitCleanupContext *context;
  Nba97FrontendExitCleanupProgress *out;
  Nba97FrontendExitCleanupMachine machine;
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

static int machine_valid(const Nba97FrontendExitCleanupMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_FRONTEND_EXIT_CLEANUP_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int initialize(Nba97FrontendExitCleanupContext *context,
                      Nba97FrontendExitCleanupProgress *out, Run *run) {
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
    Nba97FrontendExitCleanupAccess *event =
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
                  uint8_t **known_bytes) {
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
    *known_bytes = region->known ? region->known + (size_t)offset : 0;
    if (*known_bytes)
      for (byte = 0; byte < 4; ++byte)
        if ((*known_bytes)[byte] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
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

static int store_word(Run *run, unsigned source, unsigned base,
                      uint32_t offset, uint32_t pc) {
  uint32_t guest;
  uint8_t *data;
  uint8_t *known_bytes;
  unsigned byte;
  TRY(address(run, base, offset, pc, &guest));
  TRY(locate(run, guest, pc, &data, &known_bytes));
  if (!known_bytes && REG(source).known_mask != 0x0fu)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0; byte < 4; ++byte) {
    data[byte] = (uint8_t)(REG(source).word >> (byte * 8u));
    if (known_bytes)
      known_bytes[byte] = (uint8_t)((REG(source).known_mask >> byte) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_FRONTEND_EXIT_CLEANUP_STORE, pc, guest, &REG(source));
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int load_word(Run *run, unsigned destination, unsigned base,
                     uint32_t offset, uint32_t pc) {
  uint32_t guest;
  uint8_t *data;
  uint8_t *known_bytes;
  Word loaded = {0, 0};
  unsigned byte;
  TRY(address(run, base, offset, pc, &guest));
  TRY(locate(run, guest, pc, &data, &known_bytes));
  for (byte = 0; byte < 4; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (byte * 8u);
    if (!known_bytes || known_bytes[byte])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << byte));
  }
  REG(destination) = loaded;
  ++run->out->reads;
  journal(run, NBA97_FRONTEND_EXIT_CLEANUP_READ, pc, guest, &loaded);
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
  Nba97FrontendExitCleanupEvent event;
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

int nba97_frontend_exit_cleanup(Nba97FrontendExitCleanupContext *context,
                                Nba97FrontendExitCleanupProgress *out) {
  Run storage;
  Run *run = &storage;
  int decision;
  TRY(initialize(context, out, run));

  /* 0x8002F084..0x8002F090: establish the 24-byte frame, preserve entry ra,
   * and execute the first service after its NOP delay. */
  STEP(0x8002f084);
  REG(NBA97_FRONTEND_EXIT_CLEANUP_SP) = add_constant(
      REG(NBA97_FRONTEND_EXIT_CLEANUP_SP), UINT32_C(0xffffffe8));
  out->frame_stack_pointer = REG(NBA97_FRONTEND_EXIT_CLEANUP_SP).word;
  STEP(0x8002f088);
  TRY(store_word(run, NBA97_FRONTEND_EXIT_CLEANUP_RA,
                 NBA97_FRONTEND_EXIT_CLEANUP_SP, 16,
                 UINT32_C(0x8002f088)));
  out->saved_return_address = REG(NBA97_FRONTEND_EXIT_CLEANUP_RA);
  STEP(0x8002f08c);
  set_known(&REG(NBA97_FRONTEND_EXIT_CLEANUP_RA), UINT32_C(0x8002f094));
  STEP(0x8002f090);
  TRY(invoke(run, NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F08C,
             UINT32_C(0x8002f08c), UINT32_C(0x8002f090),
             UINT32_C(0x8002efbc), 0));

  /* 0x8002F094..0x8002F0A8: the third child receives the exact word loaded
   * from 0x80021D6C in a0, including partial knownness. */
  STEP(0x8002f094);
  set_known(&REG(NBA97_FRONTEND_EXIT_CLEANUP_RA), UINT32_C(0x8002f09c));
  STEP(0x8002f098);
  TRY(invoke(run, NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F094,
             UINT32_C(0x8002f094), UINT32_C(0x8002f098),
             UINT32_C(0x800394d4), 0));
  STEP(0x8002f09c);
  set_known(&REG(NBA97_FRONTEND_EXIT_CLEANUP_A0), UINT32_C(0x80020000));
  STEP(0x8002f0a0);
  TRY(load_word(run, NBA97_FRONTEND_EXIT_CLEANUP_A0,
                NBA97_FRONTEND_EXIT_CLEANUP_A0, UINT32_C(0x1d6c),
                UINT32_C(0x8002f0a0)));
  out->loaded_cleanup_selector = REG(NBA97_FRONTEND_EXIT_CLEANUP_A0);
  STEP(0x8002f0a4);
  set_known(&REG(NBA97_FRONTEND_EXIT_CLEANUP_RA), UINT32_C(0x8002f0ac));
  STEP(0x8002f0a8);
  TRY(invoke(run, NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F0A4,
             UINT32_C(0x8002f0a4), UINT32_C(0x8002f0a8),
             UINT32_C(0x80028c90), 1));

  /* 0x8002F0AC..0x8002F0CC: reload the release flag after the callbacks.
   * The branch NOP executes on both paths; the clear follows the optional
   * release callback and can alias callback-live saved ra. */
  STEP(0x8002f0ac);
  set_known(&REG(NBA97_FRONTEND_EXIT_CLEANUP_A0), UINT32_C(0x80010000));
  STEP(0x8002f0b0);
  TRY(load_word(run, NBA97_FRONTEND_EXIT_CLEANUP_A0,
                NBA97_FRONTEND_EXIT_CLEANUP_A0, UINT32_C(0x502c),
                UINT32_C(0x8002f0b0)));
  out->loaded_release_flag = REG(NBA97_FRONTEND_EXIT_CLEANUP_A0);
  STEP(0x8002f0b4);
  STEP(0x8002f0b8);
  STEP(0x8002f0bc);
  TRY(zero_decision(run, REG(NBA97_FRONTEND_EXIT_CLEANUP_A0),
                    UINT32_C(0x8002f0b8), &decision));
  if (!decision) {
    STEP(0x8002f0c0);
    set_known(&REG(NBA97_FRONTEND_EXIT_CLEANUP_RA), UINT32_C(0x8002f0c8));
    STEP(0x8002f0c4);
    TRY(invoke(run, NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F0C0,
               UINT32_C(0x8002f0c0), UINT32_C(0x8002f0c4),
               UINT32_C(0x8007760c), 1));
    STEP(0x8002f0c8);
    set_known(&REG(NBA97_FRONTEND_EXIT_CLEANUP_AT), UINT32_C(0x80010000));
    STEP(0x8002f0cc);
    TRY(store_word(run, NBA97_FRONTEND_EXIT_CLEANUP_ZERO,
                   NBA97_FRONTEND_EXIT_CLEANUP_AT, UINT32_C(0x502c),
                   UINT32_C(0x8002f0cc)));
  }

  /* 0x8002F0D0..0x8002F0E4: final service, then restore and return through
   * callback-live sp and the potentially aliased saved-ra word. */
  STEP(0x8002f0d0);
  set_known(&REG(NBA97_FRONTEND_EXIT_CLEANUP_RA), UINT32_C(0x8002f0d8));
  STEP(0x8002f0d4);
  TRY(invoke(run, NBA97_FRONTEND_EXIT_CLEANUP_SITE_8002F0D0,
             UINT32_C(0x8002f0d0), UINT32_C(0x8002f0d4),
             UINT32_C(0x80076540), 0));
  STEP(0x8002f0d8);
  TRY(load_word(run, NBA97_FRONTEND_EXIT_CLEANUP_RA,
                NBA97_FRONTEND_EXIT_CLEANUP_SP, 16,
                UINT32_C(0x8002f0d8)));
  out->restored_return_address = REG(NBA97_FRONTEND_EXIT_CLEANUP_RA);
  STEP(0x8002f0dc);
  REG(NBA97_FRONTEND_EXIT_CLEANUP_SP) = add_constant(
      REG(NBA97_FRONTEND_EXIT_CLEANUP_SP), 24);
  publish(run);
  STEP(0x8002f0e0);
  STEP(0x8002f0e4);
  if (REG(NBA97_FRONTEND_EXIT_CLEANUP_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x8002f0e0), 0,
         REG(NBA97_FRONTEND_EXIT_CLEANUP_RA).word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (REG(NBA97_FRONTEND_EXIT_CLEANUP_RA).word & 3u) {
    stop(run, UINT32_C(0x8002f0e0), 0,
         REG(NBA97_FRONTEND_EXIT_CLEANUP_RA).word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
