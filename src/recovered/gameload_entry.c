#include "gameload_entry.h"

#include <limits.h>
#include <string.h>

typedef Nba97GameloadEntryWord Word;

typedef struct Run {
  Nba97GameloadEntryContext *context;
  Nba97GameloadEntryProgress *out;
  Nba97GameloadEntryMachine machine;
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

static int machine_valid(const Nba97GameloadEntryMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_GAMELOAD_ENTRY_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int initialize(Nba97GameloadEntryContext *context,
                      Nba97GameloadEntryProgress *out, Run *run) {
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

static void set_known(Word *word, uint32_t value) {
  word->word = value;
  word->known_mask = 0x0fu;
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

static Word sub_words(Word left, Word right) {
  Word result;
  unsigned byte;
  unsigned borrow_mask = 1u;
  result.word = left.word - right.word;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_borrow_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned borrow;
    unsigned left_start = (left.known_mask & (1u << byte))
                              ? ((left.word >> (byte * 8u)) & 0xffu)
                              : 0u;
    unsigned left_end = (left.known_mask & (1u << byte)) ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte))
                               ? ((right.word >> (byte * 8u)) & 0xffu)
                               : 0u;
    unsigned right_end =
        (right.known_mask & (1u << byte)) ? right_start : 255u;
    for (borrow = 0; borrow < 2; ++borrow) {
      unsigned a;
      if (!(borrow_mask & (1u << borrow)))
        continue;
      for (a = left_start; a <= left_end; ++a) {
        unsigned b;
        for (b = right_start; b <= right_end; ++b) {
          unsigned subtrahend = b + borrow;
          unsigned output = (a - subtrahend) & 0xffu;
          next_borrow_mask |= 1u << (a < subtrahend);
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
    Nba97GameloadEntryAccess *event = &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value->word;
    event->operation = run->out->operations;
    event->width = 4;
    event->known_mask = value->known_mask;
    event->kind = kind;
  }
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

static int store_word(Run *run, Word value, uint32_t address, uint32_t pc) {
  uint8_t *data;
  uint8_t *known;
  unsigned byte;
  TRY(locate(run, address, pc, &data, &known));
  if (!known && value.known_mask != 0x0fu)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0; byte < 4; ++byte) {
    data[byte] = (uint8_t)(value.word >> (byte * 8u));
    if (known)
      known[byte] = (uint8_t)((value.known_mask >> byte) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAMELOAD_ENTRY_STORE, pc, address, &value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int load_word(Run *run, uint32_t address, uint32_t pc, Word *loaded) {
  uint8_t *data;
  uint8_t *known;
  unsigned byte;
  TRY(locate(run, address, pc, &data, &known));
  loaded->word = 0;
  loaded->known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    loaded->word |= (uint32_t)data[byte] << (byte * 8u);
    if (!known || known[byte])
      loaded->known_mask =
          (uint8_t)(loaded->known_mask | (uint8_t)(1u << byte));
  }
  ++run->out->reads;
  journal(run, NBA97_GAMELOAD_ENTRY_READ, pc, address, loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int matches_word(Word word, uint32_t concrete) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((word.known_mask & (1u << byte)) &&
        ((word.word >> (byte * 8u)) & 0xffu) !=
            ((concrete >> (byte * 8u)) & 0xffu))
      return 0;
  return 1;
}

static int trapping_addi(Run *run, Word input, int32_t immediate,
                         uint32_t pc, Word *result) {
  uint32_t candidate;
  uint32_t first;
  uint32_t last;
  int possible_overflow = 0;
  if (immediate < 0) {
    first = UINT32_C(0x80000000);
    last = first + (uint32_t)(-immediate) - 1u;
  } else {
    first = UINT32_C(0x7fffffff) - (uint32_t)immediate + 1u;
    last = UINT32_C(0x7fffffff);
  }
  for (candidate = first; candidate <= last; ++candidate)
    if (matches_word(input, candidate)) {
      possible_overflow = 1;
      break;
    }
  if (possible_overflow && input.known_mask != 0x0fu) {
    stop(run, pc, 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (possible_overflow) {
    stop(run, pc, 0, 0);
    run->out->trapped = 1;
    return NBA97_GAMELOAD_ENTRY_ARITHMETIC_TRAP;
  }
  *result = add_constant(input, (uint32_t)immediate);
  return NBA97_TEXT_COMPLETE;
}

static int invoke(Run *run, uint8_t site, uint32_t pc, uint32_t delay,
                  uint32_t target, uint8_t argument_count,
                  Nba97GameloadEntryCalleeOutcome *outcome) {
  Nba97GameloadEntryEvent event;
  int accepted;
  stop(run, pc, 0, target);
  TRY(spend(run));
  ++run->out->call_attempts[site];
  memset(&event, 0, sizeof event);
  event.pc = pc;
  event.delay_slot_pc = delay;
  event.entry = target;
  event.operation = run->out->operations;
  event.invocation = run->out->call_attempts[site];
  event.site = site;
  event.argument_count = argument_count;
  event.target_program = NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD;
  publish(run);
  if (!run->context->io)
    return NBA97_TEXT_IO_REFUSED;
  *outcome = (Nba97GameloadEntryCalleeOutcome)0;
  accepted = run->context->io(run->context->user, &run->context->memory,
                              &event, &run->machine, outcome);
  publish(run);
  if (accepted != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!machine_valid(&run->machine) ||
      (*outcome != NBA97_GAMELOAD_ENTRY_CALLEE_RETURNED &&
       *outcome != NBA97_GAMELOAD_ENTRY_CALLEE_TRANSFERRED))
    return NBA97_TEXT_ARGUMENT;
  ++run->out->callbacks_completed;
  ++run->out->call_count[site];
  return NBA97_TEXT_COMPLETE;
}

int nba97_gameload_entry(Nba97GameloadEntryContext *context,
                         Nba97GameloadEntryProgress *out) {
  Run storage;
  Run *run = &storage;
  Word loaded;
  Word temporary;
  Word zero;
  Nba97GameloadEntryCalleeOutcome outcome;
  uint32_t clear_address;
  TRY(initialize(context, out, run));
  set_known(&zero, 0);

  STEP(0x801e1410);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_V0), UINT32_C(0x801f0000));
  STEP(0x801e1414);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_V0), UINT32_C(0x801e903c));
  STEP(0x801e1418);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_V1), UINT32_C(0x801f0000));
  STEP(0x801e141c);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_V1), UINT32_C(0x801eb0a0));
  for (clear_address = UINT32_C(0x801e903c);
       clear_address != UINT32_C(0x801eb0a0); clear_address += 4u) {
    STEP(0x801e1420);
    TRY(store_word(run, zero, REG(NBA97_GAMELOAD_ENTRY_V0).word,
                   UINT32_C(0x801e1420)));
    ++out->words_cleared;
    STEP(0x801e1424);
    set_known(&REG(NBA97_GAMELOAD_ENTRY_V0),
              REG(NBA97_GAMELOAD_ENTRY_V0).word + 4u);
    STEP(0x801e1428);
    set_known(&REG(NBA97_GAMELOAD_ENTRY_AT),
              REG(NBA97_GAMELOAD_ENTRY_V0).word <
                      REG(NBA97_GAMELOAD_ENTRY_V1).word
                  ? 1u
                  : 0u);
    STEP(0x801e142c);
    STEP(0x801e1430);
  }

  STEP(0x801e1434);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_V0), UINT32_C(0x801f0000));
  STEP(0x801e1438);
  TRY(load_word(run, UINT32_C(0x801e8b70), UINT32_C(0x801e1438), &loaded));
  REG(NBA97_GAMELOAD_ENTRY_V0) = loaded;
  out->loaded_stack_top = loaded;
  STEP(0x801e143c);
  STEP(0x801e1440);
  TRY(trapping_addi(run, REG(NBA97_GAMELOAD_ENTRY_V0), -8,
                    UINT32_C(0x801e1440), &temporary));
  REG(NBA97_GAMELOAD_ENTRY_V0) = temporary;
  STEP(0x801e1444);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_T0), UINT32_C(0x80000000));
  STEP(0x801e1448);
  REG(NBA97_GAMELOAD_ENTRY_SP).word =
      REG(NBA97_GAMELOAD_ENTRY_V0).word | UINT32_C(0x80000000);
  REG(NBA97_GAMELOAD_ENTRY_SP).known_mask =
      REG(NBA97_GAMELOAD_ENTRY_V0).known_mask;
  STEP(0x801e144c);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_A0), UINT32_C(0x801f0000));
  STEP(0x801e1450);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_A0), UINT32_C(0x801eb0a0));
  STEP(0x801e1454);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_A0), UINT32_C(0x00f58500));
  STEP(0x801e1458);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_A0), UINT32_C(0x001eb0a0));
  STEP(0x801e145c);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_V1), UINT32_C(0x801f0000));
  STEP(0x801e1460);
  TRY(load_word(run, UINT32_C(0x801e8b6c), UINT32_C(0x801e1460), &loaded));
  REG(NBA97_GAMELOAD_ENTRY_V1) = loaded;
  out->loaded_heap_reserve = loaded;
  STEP(0x801e1464);
  STEP(0x801e1468);
  REG(NBA97_GAMELOAD_ENTRY_A1) =
      sub_words(REG(NBA97_GAMELOAD_ENTRY_V0),
                REG(NBA97_GAMELOAD_ENTRY_V1));
  STEP(0x801e146c);
  REG(NBA97_GAMELOAD_ENTRY_A1) =
      sub_words(REG(NBA97_GAMELOAD_ENTRY_A1),
                REG(NBA97_GAMELOAD_ENTRY_A0));
  out->heap_size = REG(NBA97_GAMELOAD_ENTRY_A1);
  STEP(0x801e1470);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_AT), UINT32_C(0x801f0000));
  STEP(0x801e1474);
  TRY(store_word(run, REG(NBA97_GAMELOAD_ENTRY_A1), UINT32_C(0x801e8b50),
                 UINT32_C(0x801e1474)));
  STEP(0x801e1478);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_A0), UINT32_C(0x801eb0a0));
  out->heap_base = REG(NBA97_GAMELOAD_ENTRY_A0);
  STEP(0x801e147c);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_AT), UINT32_C(0x801f0000));
  STEP(0x801e1480);
  TRY(store_word(run, REG(NBA97_GAMELOAD_ENTRY_A0), UINT32_C(0x801e8b4c),
                 UINT32_C(0x801e1480)));
  STEP(0x801e1484);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_AT), UINT32_C(0x801f0000));
  STEP(0x801e1488);
  out->saved_return_address = REG(NBA97_GAMELOAD_ENTRY_RA);
  TRY(store_word(run, REG(NBA97_GAMELOAD_ENTRY_RA), UINT32_C(0x801e903c),
                 UINT32_C(0x801e1488)));
  STEP(0x801e148c);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_GP), UINT32_C(0x801f0000));
  STEP(0x801e1490);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_GP), UINT32_C(0x801e903c));
  STEP(0x801e1494);
  REG(NBA97_GAMELOAD_ENTRY_S8) = REG(NBA97_GAMELOAD_ENTRY_SP);
  STEP(0x801e1498);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_RA), UINT32_C(0x801e14a0));
  STEP(0x801e149c);
  TRY(trapping_addi(run, REG(NBA97_GAMELOAD_ENTRY_A0), 4,
                    UINT32_C(0x801e149c), &temporary));
  REG(NBA97_GAMELOAD_ENTRY_A0) = temporary;
  out->first_child_entered = 1;
  TRY(invoke(run, NBA97_GAMELOAD_ENTRY_SITE_801E1498,
             UINT32_C(0x801e1498), UINT32_C(0x801e149c),
             UINT32_C(0x801e1590), 2, &outcome));
  if (outcome == NBA97_GAMELOAD_ENTRY_CALLEE_TRANSFERRED) {
    out->completed = 1;
    out->transferred = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
  }
  STEP(0x801e14a0);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_RA), UINT32_C(0x801f0000));
  STEP(0x801e14a4);
  TRY(load_word(run, UINT32_C(0x801e903c), UINT32_C(0x801e14a4), &loaded));
  REG(NBA97_GAMELOAD_ENTRY_RA) = loaded;
  out->restored_return_address = loaded;
  STEP(0x801e14a8);
  STEP(0x801e14ac);
  set_known(&REG(NBA97_GAMELOAD_ENTRY_RA), UINT32_C(0x801e14b4));
  STEP(0x801e14b0);
  out->second_child_entered = 1;
  TRY(invoke(run, NBA97_GAMELOAD_ENTRY_SITE_801E14AC,
             UINT32_C(0x801e14ac), UINT32_C(0x801e14b0),
             UINT32_C(0x801e136c), 0, &outcome));
  if (outcome == NBA97_GAMELOAD_ENTRY_CALLEE_TRANSFERRED) {
    out->completed = 1;
    out->transferred = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
  }
  STEP(0x801e14b4);
  out->trapped = 1;
  stop(run, UINT32_C(0x801e14b4), 0, 0);
  return NBA97_GAMELOAD_ENTRY_BREAK_TRAP;
}
