#include "gameload_main.h"

#include <string.h>

typedef Nba97GameloadMainWord Word;
typedef struct Run {
  Nba97GameloadMainContext *context;
  Nba97GameloadMainProgress *out;
  Nba97GameloadMainMachine machine;
} Run;

#define R(index) (run->machine.registers.gpr[(index)])
#define V0 R(NBA97_GAMELOAD_MAIN_V0)
#define A0 R(NBA97_GAMELOAD_MAIN_A0)
#define A1 R(NBA97_GAMELOAD_MAIN_A1)
#define A2 R(NBA97_GAMELOAD_MAIN_A2)
#define S0 R(NBA97_GAMELOAD_MAIN_S0)
#define SP R(NBA97_GAMELOAD_MAIN_SP)
#define RA R(NBA97_GAMELOAD_MAIN_RA)
#define TRY(expression)                                                        \
  do {                                                                         \
    int result_ = (expression);                                                \
    if (result_ != NBA97_TEXT_COMPLETE)                                        \
      return result_;                                                          \
  } while (0)

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t target) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  run->out->stopped_target = target;
  publish(run);
}

static void step(Run *run, uint32_t pc) {
  size_t index = run->out->instruction_events++;
  if (index < run->context->instruction_journal_capacity)
    run->context->instruction_journal[index] = pc;
  ++run->out->instruction_count;
}

#define STEP(pc) step(run, UINT32_C(pc))

static void set_known(Word *word, uint32_t value) {
  word->word = value;
  word->known_mask = 0x0fu;
}

static Word immediate(uint32_t value) {
  Word result;
  set_known(&result, value);
  return result;
}

static Word add_words(Word left, Word right) {
  Word result;
  unsigned carry_mask = 1u;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned ls = (left.known_mask & (1u << byte))
                      ? ((left.word >> (byte * 8u)) & 255u)
                      : 0u;
    unsigned le = (left.known_mask & (1u << byte)) ? ls : 255u;
    unsigned rs = (right.known_mask & (1u << byte))
                      ? ((right.word >> (byte * 8u)) & 255u)
                      : 0u;
    unsigned re = (right.known_mask & (1u << byte)) ? rs : 255u;
    unsigned carry;
    for (carry = 0; carry < 2; ++carry) {
      unsigned a;
      if (!(carry_mask & (1u << carry)))
        continue;
      for (a = ls; a <= le; ++a) {
        unsigned b;
        for (b = rs; b <= re; ++b) {
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

static Word or_immediate(Word input, uint32_t value) {
  Word result;
  unsigned byte;
  result.word = input.word | value;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    uint32_t part = (value >> (byte * 8u)) & 255u;
    if ((input.known_mask & (1u << byte)) || part == 255u)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static int machine_valid(const Nba97GameloadMainMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_GAMELOAD_MAIN_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    Word value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameloadMainAccess *access = &run->context->access_journal[index];
    access->pc = pc;
    access->address = address;
    access->value = value.word;
    access->operation = run->out->operations;
    access->width = 4;
    access->known_mask = value.known_mask;
    access->kind = kind;
  }
}

static int locate(Run *run, uint32_t address, uint32_t pc, uint8_t **data,
                  uint8_t **known) {
  size_t i;
  unsigned byte;
  stop(run, pc, address, 0);
  TRY(spend(run));
  ++run->out->accesses;
  if (address & 3u) {
    run->out->trapped = 1;
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
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

static int effective_address(Run *run, Word base, uint32_t offset,
                             uint32_t pc, uint32_t *address) {
  Word computed = add_words(base, immediate(offset));
  if (computed.known_mask != 0x0fu) {
    stop(run, pc, computed.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = computed.word;
  return NBA97_TEXT_COMPLETE;
}

static int load_word(Run *run, Word base, uint32_t offset, uint32_t pc,
                     Word *destination) {
  uint32_t address;
  uint8_t *data;
  uint8_t *known;
  Word value = {0, 0};
  unsigned byte;
  TRY(effective_address(run, base, offset, pc, &address));
  TRY(locate(run, address, pc, &data, &known));
  for (byte = 0; byte < 4; ++byte) {
    value.word |= (uint32_t)data[byte] << (byte * 8u);
    if (!known || known[byte])
      value.known_mask = (uint8_t)(value.known_mask | (1u << byte));
  }
  *destination = value;
  ++run->out->reads;
  journal(run, NBA97_GAMELOAD_MAIN_READ, pc, address, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int store_word(Run *run, Word base, uint32_t offset, uint32_t pc,
                      Word value) {
  uint32_t address;
  uint8_t *data;
  uint8_t *known;
  unsigned byte;
  TRY(effective_address(run, base, offset, pc, &address));
  TRY(locate(run, address, pc, &data, &known));
  if (!known && value.known_mask != 0x0fu)
    return NBA97_TEXT_UNKNOWN;
  for (byte = 0; byte < 4; ++byte) {
    data[byte] = (uint8_t)(value.word >> (byte * 8u));
    if (known)
      known[byte] = (uint8_t)((value.known_mask >> byte) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAMELOAD_MAIN_STORE, pc, address, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int invoke(Run *run, uint8_t site, uint32_t pc, uint32_t delay_pc,
                  uint32_t target, uint8_t argument_count,
                  uint8_t target_program,
                  Nba97GameloadMainCalleeOutcome *outcome) {
  Nba97GameloadMainEvent event;
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
  event.target_program = target_program;
  *outcome = NBA97_GAMELOAD_MAIN_CALLEE_RETURNED;
  publish(run);
  if (!run->context->io)
    return NBA97_TEXT_IO_REFUSED;
  accepted = run->context->io(run->context->user, &run->context->memory,
                              &event, &run->machine, outcome);
  publish(run);
  if (accepted != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!machine_valid(&run->machine) ||
      (*outcome != NBA97_GAMELOAD_MAIN_CALLEE_RETURNED &&
       *outcome != NBA97_GAMELOAD_MAIN_CALLEE_TRANSFERRED))
    return NBA97_TEXT_ARGUMENT;
  if (target_program != NBA97_GAMELOAD_MAIN_PROGRAM_GAMEONLY &&
      *outcome != NBA97_GAMELOAD_MAIN_CALLEE_RETURNED)
    return NBA97_TEXT_ARGUMENT;
  ++run->out->call_count[site];
  ++run->out->callbacks_completed;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int initialize(Nba97GameloadMainContext *context,
                      Nba97GameloadMainProgress *out, Run *run) {
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

static int finish_transfer(Run *run) {
  run->out->completed = 1;
  run->out->transferred = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}

static int finish_return(Run *run) {
  STEP(0x801e1408);
  STEP(0x801e140c);
  if (RA.known_mask != 0x0fu) {
    stop(run, UINT32_C(0x801e1408), 0, RA.word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    run->out->trapped = 1;
    stop(run, UINT32_C(0x801e1408), 0, RA.word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  run->out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}

int nba97_gameload_main(Nba97GameloadMainContext *context,
                        Nba97GameloadMainProgress *out) {
  Run storage;
  Run *run = &storage;
  Nba97GameloadMainCalleeOutcome outcome;
  uint32_t dynamic_target;
  TRY(initialize(context, out, run));

  /* 0x801E136C..0x801E1378: establish the live frame. JAL writes RA before
   * its delay-slot S0 store, so a refused store prevents the first call. */
  STEP(0x801e136c);
  SP = add_words(SP, immediate(UINT32_C(0xffffffe8)));
  out->frame_stack_pointer = SP.word;
  publish(run);
  STEP(0x801e1370);
  out->saved_return_address = RA;
  TRY(store_word(run, SP, 0x14, UINT32_C(0x801e1370), RA));
  STEP(0x801e1374);
  set_known(&RA, UINT32_C(0x801e137c));
  publish(run);
  STEP(0x801e1378);
  out->saved_s0 = S0;
  TRY(store_word(run, SP, 0x10, UINT32_C(0x801e1378), S0));
  TRY(invoke(run, NBA97_GAMELOAD_MAIN_SITE_801E1374,
             UINT32_C(0x801e1374), UINT32_C(0x801e1378),
             UINT32_C(0x801e14b8), 0,
             NBA97_GAMELOAD_MAIN_PROGRAM_GAMELOAD, &outcome));

  STEP(0x801e137c);
  set_known(&RA, UINT32_C(0x801e1384));
  publish(run);
  STEP(0x801e1380);
  TRY(invoke(run, NBA97_GAMELOAD_MAIN_SITE_801E137C,
             UINT32_C(0x801e137c), UINT32_C(0x801e1380),
             UINT32_C(0x801e000c), 0,
             NBA97_GAMELOAD_MAIN_PROGRAM_GAMELOAD, &outcome));

  STEP(0x801e1384);
  set_known(&RA, UINT32_C(0x801e138c));
  publish(run);
  STEP(0x801e1388);
  TRY(invoke(run, NBA97_GAMELOAD_MAIN_SITE_801E1384,
             UINT32_C(0x801e1384), UINT32_C(0x801e1388),
             UINT32_C(0x801e059c), 0,
             NBA97_GAMELOAD_MAIN_PROGRAM_GAMELOAD, &outcome));

  /* 0x801E138C..0x801E1398: forward the registration pointer and value 707.
   * The second argument is written in the JAL delay slot and is visible to the
   * callback. */
  STEP(0x801e138c);
  set_known(&A0, UINT32_C(0x80010000));
  STEP(0x801e1390);
  A0 = or_immediate(A0, 0x0c);
  publish(run);
  STEP(0x801e1394);
  set_known(&RA, UINT32_C(0x801e139c));
  publish(run);
  STEP(0x801e1398);
  set_known(&A1, 0x2c3);
  publish(run);
  TRY(invoke(run, NBA97_GAMELOAD_MAIN_SITE_801E1394,
             UINT32_C(0x801e1394), UINT32_C(0x801e1398),
             UINT32_C(0x801e0938), 2,
             NBA97_GAMELOAD_MAIN_PROGRAM_GAMELOAD, &outcome));

  /* 0x801E139C..0x801E13B4: load the copy size late, then forward its full
   * word and byte-known mask through the first copy call's delay slot. */
  STEP(0x801e139c);
  set_known(&S0, UINT32_C(0x80010000));
  STEP(0x801e13a0);
  TRY(load_word(run, S0, 0x5004, UINT32_C(0x801e13a0), &S0));
  out->loaded_copy_size = S0;
  STEP(0x801e13a4);
  set_known(&A0, UINT32_C(0x801b0000));
  STEP(0x801e13a8);
  set_known(&A1, UINT32_C(0x80010000));
  STEP(0x801e13ac);
  A1 = or_immediate(A1, 0x5008);
  publish(run);
  STEP(0x801e13b0);
  set_known(&RA, UINT32_C(0x801e13b8));
  publish(run);
  STEP(0x801e13b4);
  A2 = S0;
  out->first_copy_length = A2;
  publish(run);
  TRY(invoke(run, NBA97_GAMELOAD_MAIN_SITE_801E13B0,
             UINT32_C(0x801e13b0), UINT32_C(0x801e13b4),
             UINT32_C(0x801e1344), 3,
             NBA97_GAMELOAD_MAIN_PROGRAM_GAMELOAD, &outcome));

  /* 0x801E13B8..0x801E13C8: request GAMEONLY.BIN at the fixed filename and
   * destination. The destination ORI is the loader call delay slot. */
  STEP(0x801e13b8);
  set_known(&A0, UINT32_C(0x801e0000));
  STEP(0x801e13bc);
  A0 = add_words(A0, immediate(0x60));
  publish(run);
  STEP(0x801e13c0);
  set_known(&A1, UINT32_C(0x80010000));
  STEP(0x801e13c4);
  set_known(&RA, UINT32_C(0x801e13cc));
  publish(run);
  STEP(0x801e13c8);
  A1 = or_immediate(A1, 0x5000);
  publish(run);
  TRY(invoke(run, NBA97_GAMELOAD_MAIN_SITE_801E13C4,
             UINT32_C(0x801e13c4), UINT32_C(0x801e13c8),
             UINT32_C(0x801e1300), 2,
             NBA97_GAMELOAD_MAIN_PROGRAM_GAMELOAD, &outcome));

  STEP(0x801e13cc);
  set_known(&RA, UINT32_C(0x801e13d4));
  publish(run);
  STEP(0x801e13d0);
  TRY(invoke(run, NBA97_GAMELOAD_MAIN_SITE_801E13CC,
             UINT32_C(0x801e13cc), UINT32_C(0x801e13d0),
             UINT32_C(0x801e1670), 0,
             NBA97_GAMELOAD_MAIN_PROGRAM_GAMELOAD, &outcome));

  /* 0x801E13D4..0x801E13E4: restore the staged bytes. A2 is re-read from
   * callback-live S0 rather than from the earlier global-load snapshot. */
  STEP(0x801e13d4);
  set_known(&A0, UINT32_C(0x80010000));
  STEP(0x801e13d8);
  A0 = or_immediate(A0, 0x5008);
  publish(run);
  STEP(0x801e13dc);
  set_known(&A1, UINT32_C(0x801b0000));
  STEP(0x801e13e0);
  set_known(&RA, UINT32_C(0x801e13e8));
  publish(run);
  STEP(0x801e13e4);
  A2 = S0;
  out->second_copy_length = A2;
  publish(run);
  TRY(invoke(run, NBA97_GAMELOAD_MAIN_SITE_801E13E0,
             UINT32_C(0x801e13e0), UINT32_C(0x801e13e4),
             UINT32_C(0x801e1344), 3,
             NBA97_GAMELOAD_MAIN_PROGRAM_GAMELOAD, &outcome));

  /* 0x801E13E8..0x801E13F8: reload and latch the GAMEONLY entry before JALR.
   * RA is written before the NOP delay; target faults are reported afterward. */
  STEP(0x801e13e8);
  set_known(&V0, UINT32_C(0x80010000));
  STEP(0x801e13ec);
  TRY(load_word(run, V0, 0x5000, UINT32_C(0x801e13ec), &V0));
  out->loaded_gameonly_entry = V0;
  STEP(0x801e13f0);
  STEP(0x801e13f4);
  dynamic_target = V0.word;
  set_known(&RA, UINT32_C(0x801e13fc));
  publish(run);
  STEP(0x801e13f8);
  if (out->loaded_gameonly_entry.known_mask != 0x0fu) {
    stop(run, UINT32_C(0x801e13f4), 0, dynamic_target);
    return NBA97_TEXT_UNKNOWN;
  }
  if (dynamic_target & 3u) {
    out->trapped = 1;
    stop(run, UINT32_C(0x801e13f4), 0, dynamic_target);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  TRY(invoke(run, NBA97_GAMELOAD_MAIN_SITE_801E13F4,
             UINT32_C(0x801e13f4), UINT32_C(0x801e13f8), dynamic_target, 0,
             NBA97_GAMELOAD_MAIN_PROGRAM_GAMEONLY, &outcome));
  if (outcome == NBA97_GAMELOAD_MAIN_CALLEE_TRANSFERRED)
    return finish_transfer(run);

  /* 0x801E13FC..0x801E140C: a returning GAMEONLY child restores through its
   * callback-live SP, then returns through the restored live RA after NOP. */
  STEP(0x801e13fc);
  TRY(load_word(run, SP, 0x14, UINT32_C(0x801e13fc), &RA));
  out->restored_return_address = RA;
  STEP(0x801e1400);
  TRY(load_word(run, SP, 0x10, UINT32_C(0x801e1400), &S0));
  out->restored_s0 = S0;
  STEP(0x801e1404);
  SP = add_words(SP, immediate(0x18));
  publish(run);
  return finish_return(run);
}
