#include "game_period_presentation_finish.h"

#include <string.h>

#define SOURCE_ADDRESS UINT32_C(0x8001ede8)
#define PRESENTATION_FLAG_ADDRESS UINT32_C(0x800eb680)
#define GATE_ADDRESS UINT32_C(0x800fdb78)
#define ACTIVE_ADDRESS UINT32_C(0x80109afc)
#define SOURCE_PUBLISH_ADDRESS UINT32_C(0x80109ae4)

typedef struct Run {
  Nba97GamePeriodPresentationFinishContext *context;
  Nba97GamePeriodPresentationFinishProgress *out;
  Nba97GamePeriodPresentationFinishMachine machine;
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

static void set_known(Nba97GamePeriodPresentationFinishWord *value,
                      uint32_t word) {
  value->word = word;
  value->known_mask = 0x0fu;
}

static int machine_valid(
    const Nba97GamePeriodPresentationFinishMachine *machine) {
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

static int validate(Nba97GamePeriodPresentationFinishContext *context,
                    Nba97GamePeriodPresentationFinishProgress *out,
                    Run *run) {
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

static Nba97GamePeriodPresentationFinishWord add_words(
    Nba97GamePeriodPresentationFinishWord left,
    Nba97GamePeriodPresentationFinishWord right) {
  Nba97GamePeriodPresentationFinishWord result;
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

static Nba97GamePeriodPresentationFinishWord add_constant(
    Nba97GamePeriodPresentationFinishWord source, uint32_t constant) {
  Nba97GamePeriodPresentationFinishWord value;
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
                    const Nba97GamePeriodPresentationFinishWord *value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GamePeriodPresentationFinishAccess *event =
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

static int read_value(Run *run, uint32_t address, uint8_t width, uint32_t pc,
                      Nba97GamePeriodPresentationFinishWord *value) {
  Nba97GamePeriodPresentationFinishWord loaded = {0, 0};
  uint8_t *data;
  uint8_t *known;
  unsigned i;
  TRY(locate(run, address, width, width, pc, &data, &known));
  for (i = 0; i < width; ++i) {
    loaded.word |= (uint32_t)data[i] << (i * 8u);
    if (!known || known[i])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
  }
  *value = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_PERIOD_PRESENTATION_FINISH_READ, pc, address,
          width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, uint32_t address, uint8_t width, uint32_t pc,
                       const Nba97GamePeriodPresentationFinishWord *value) {
  Nba97GamePeriodPresentationFinishWord stored = *value;
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
  journal(run, NBA97_GAME_PERIOD_PRESENTATION_FINISH_STORE, pc, address,
          width, &stored);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int register_address(Run *run,
    Nba97GamePeriodPresentationFinishWord base, uint32_t offset, uint32_t pc,
    uint32_t *address) {
  Nba97GamePeriodPresentationFinishWord value = add_constant(base, offset);
  if (value.known_mask != 0x0fu) {
    stop(run, pc, value.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = value.word;
  return NBA97_TEXT_COMPLETE;
}

static Nba97GamePeriodPresentationFinishWord zero_extend_byte(
    Nba97GamePeriodPresentationFinishWord source) {
  Nba97GamePeriodPresentationFinishWord value;
  value.word = source.word & 0xffu;
  value.known_mask = (uint8_t)((source.known_mask & 1u) | 0x0eu);
  return value;
}

static int decide_zero(Run *run,
                       const Nba97GamePeriodPresentationFinishWord *value,
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

static int invoke(Run *run, uint32_t pc, uint32_t entry, uint8_t kind) {
  Nba97GamePeriodPresentationFinishEvent event;
  int accepted;
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8u);
  stop(run, pc, 0, entry);
  TRY(spend(run));
  memset(&event, 0, sizeof event);
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = entry;
  event.operation = run->out->operations;
  event.invocation = run->out->call_count[kind] + 1u;
  event.kind = kind;
  event.argument_count = 0;
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

int nba97_game_period_presentation_finish(
    Nba97GamePeriodPresentationFinishContext *context,
    Nba97GamePeriodPresentationFinishProgress *out) {
  Run storage;
  Run *run = &storage;
  Nba97GamePeriodPresentationFinishWord value;
  Nba97GamePeriodPresentationFinishWord zero;
  uint32_t address;
  int is_zero;
  TRY(validate(context, out, run));

  /* GAMEONLY 0x8002DDCC..0x8002DDDC: capture the source word before the LW
   * delay allocates the wrapping frame, then save entry ra at frame+0x10. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
  TRY(read_value(run, SOURCE_ADDRESS, 4, UINT32_C(0x8002ddd0),
                 &R(NBA97_MATCH_INITIALIZE_V0)));
  out->source_word = R(NBA97_MATCH_INITIALIZE_V0);
  R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
      R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffe8));
  out->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  set_known(&R(NBA97_MATCH_INITIALIZE_V1), 1);
  TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u,
                       UINT32_C(0x8002dddc), &address));
  out->saved_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  TRY(write_value(run, address, 4, UINT32_C(0x8002dddc),
                  &R(NBA97_MATCH_INITIALIZE_RA)));

  /* GAMEONLY 0x8002DDE0..0x8002DDF4: clear the outer presentation byte,
   * publish active=1, then publish the captured source word in exact order. */
  set_known(&zero, 0);
  set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x800f0000));
  TRY(write_value(run, PRESENTATION_FLAG_ADDRESS, 1,
                  UINT32_C(0x8002dde4), &zero));
  set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80110000));
  TRY(write_value(run, ACTIVE_ADDRESS, 4, UINT32_C(0x8002ddec),
                  &R(NBA97_MATCH_INITIALIZE_V1)));
  set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80110000));
  TRY(write_value(run, SOURCE_PUBLISH_ADDRESS, 4,
                  UINT32_C(0x8002ddf4), &R(NBA97_MATCH_INITIALIZE_V0)));

  /* GAMEONLY 0x8002DDF8..0x8002DE10: the mandatory callback completes before
   * LUI/LBU rereads its mutable gate; BNE and its NOP consume that live byte. */
  TRY(invoke(run, UINT32_C(0x8002ddf8), UINT32_C(0x80044550),
             NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80044550));
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80100000));
  TRY(read_value(run, GATE_ADDRESS, 1, UINT32_C(0x8002de04), &value));
  R(NBA97_MATCH_INITIALIZE_V0) = zero_extend_byte(value);
  out->gate_flag = R(NBA97_MATCH_INITIALIZE_V0);
  TRY(decide_zero(run, &R(NBA97_MATCH_INITIALIZE_V0),
                  UINT32_C(0x8002de0c), &is_zero));
  if (is_zero) {
    out->optional_child_called = 1;
    TRY(invoke(run, UINT32_C(0x8002de14), UINT32_C(0x80046c2c),
               NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80046C2C));
  }

  /* GAMEONLY 0x8002DE1C..0x8002DE30: clear active state, reload ra through
   * callback-live sp, apply the LW delay adjustment, then consume JR/NOP. */
  set_known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80110000));
  TRY(write_value(run, ACTIVE_ADDRESS, 4, UINT32_C(0x8002de20), &zero));
  TRY(register_address(run, R(NBA97_MATCH_INITIALIZE_SP), 0x10u,
                       UINT32_C(0x8002de24), &address));
  TRY(read_value(run, address, 4, UINT32_C(0x8002de24),
                 &R(NBA97_MATCH_INITIALIZE_RA)));
  out->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  R(NBA97_MATCH_INITIALIZE_SP) = add_constant(
      R(NBA97_MATCH_INITIALIZE_SP), 0x18u);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 0x0fu) {
    stop(run, UINT32_C(0x8002de2c), R(NBA97_MATCH_INITIALIZE_RA).word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (R(NBA97_MATCH_INITIALIZE_RA).word & 3u) {
    stop(run, UINT32_C(0x8002de2c), R(NBA97_MATCH_INITIALIZE_RA).word, 0);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
