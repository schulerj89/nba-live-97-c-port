#include "frontend_main.h"

#include <limits.h>
#include <string.h>

typedef Nba97FrontendMainWord Word;
typedef struct Run {
  Nba97FrontendMainContext *context;
  Nba97FrontendMainProgress *out;
  Nba97FrontendMainMachine machine;
} Run;

#define R(i) (run->machine.registers.gpr[(i)])
#define ZERO R(0)
#define AT R(1)
#define V0 R(2)
#define V1 R(3)
#define A0 R(4)
#define A1 R(5)
#define A2 R(6)
#define A3 R(7)
#define S0 R(16)
#define S1 R(17)
#define S2 R(18)
#define SP R(29)
#define RA R(31)
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

static int machine_valid(const Nba97FrontendMainMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 0x0fu ||
      machine->hi.known_mask > 0x0fu || machine->lo.known_mask > 0x0fu)
    return 0;
  for (i = 0; i < NBA97_FRONTEND_MAIN_REGISTER_COUNT; ++i)
    if (machine->registers.gpr[i].known_mask > 0x0fu)
      return 0;
  return 1;
}

static int initialize(Nba97FrontendMainContext *context,
                      Nba97FrontendMainProgress *out, Run *run) {
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

static void known(Word *word, uint32_t value) {
  word->word = value;
  word->known_mask = 0x0fu;
}

static Word immediate(uint32_t value) {
  Word result;
  known(&result, value);
  return result;
}

static Word add(Word left, Word right) {
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

static Word subtract(Word left, Word right) {
  Word result;
  unsigned borrow_mask = 1u;
  unsigned byte;
  result.word = left.word - right.word;
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
    unsigned borrow;
    for (borrow = 0; borrow < 2; ++borrow) {
      unsigned a;
      if (!(borrow_mask & (1u << borrow)))
        continue;
      for (a = ls; a <= le; ++a) {
        unsigned b;
        for (b = rs; b <= re; ++b) {
          unsigned output = (a - b - borrow) & 255u;
          next_mask |= 1u << (a < b + borrow);
          if (first) {
            first_output = output;
            first = 0;
          } else if (first_output != output) {
            invariant = 0;
          }
        }
      }
    }
    if (invariant)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
    borrow_mask = next_mask;
  }
  return result;
}

static Word or_immediate(Word input, uint32_t value) {
  Word result;
  unsigned byte;
  result.word = input.word | value;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned part = (value >> (byte * 8u)) & 255u;
    if ((input.known_mask & (1u << byte)) || part == 255u)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}

static Word unsigned_less(Word left, Word right) {
  Word result;
  uint64_t lmin = 0;
  uint64_t lmax = 0;
  uint64_t rmin = 0;
  uint64_t rmax = 0;
  unsigned byte;
  known(&result, left.word < right.word);
  for (byte = 0; byte < 4; ++byte) {
    uint32_t lp = (left.word >> (byte * 8u)) & 255u;
    uint32_t rp = (right.word >> (byte * 8u)) & 255u;
    lmin |= (uint64_t)((left.known_mask & (1u << byte)) ? lp : 0u)
            << (byte * 8u);
    lmax |= (uint64_t)((left.known_mask & (1u << byte)) ? lp : 255u)
            << (byte * 8u);
    rmin |= (uint64_t)((right.known_mask & (1u << byte)) ? rp : 0u)
            << (byte * 8u);
    rmax |= (uint64_t)((right.known_mask & (1u << byte)) ? rp : 255u)
            << (byte * 8u);
  }
  if (lmax < rmin || lmin >= rmax)
    return result;
  result.known_mask = 0x0eu;
  return result;
}

static int32_t signed_word(uint32_t value) {
  return value <= (uint32_t)INT32_MAX ? (int32_t)value
                                      : -1 - (int32_t)(UINT32_MAX - value);
}

static void signed_bounds(Word value, int64_t *minimum, int64_t *maximum) {
  uint32_t low = 0;
  uint32_t high = 0;
  unsigned byte;
  for (byte = 0; byte < 3; ++byte) {
    uint32_t part = (value.word >> (byte * 8u)) & 255u;
    low |= ((value.known_mask & (1u << byte)) ? part : 0u) << (byte * 8u);
    high |= ((value.known_mask & (1u << byte)) ? part : 255u) << (byte * 8u);
  }
  if (value.known_mask & 8u) {
    low |= value.word & UINT32_C(0xff000000);
    high |= value.word & UINT32_C(0xff000000);
  } else {
    low |= UINT32_C(0x80000000);
    high |= UINT32_C(0x7f000000);
  }
  *minimum = (low & UINT32_C(0x80000000))
                 ? (int64_t)low - INT64_C(0x100000000)
                 : (int64_t)low;
  *maximum = (high & UINT32_C(0x80000000))
                 ? (int64_t)high - INT64_C(0x100000000)
                 : (int64_t)high;
}

static Word signed_less(Word left, Word right) {
  Word result;
  int64_t lmin;
  int64_t lmax;
  int64_t rmin;
  int64_t rmax;
  known(&result, signed_word(left.word) < signed_word(right.word));
  signed_bounds(left, &lmin, &lmax);
  signed_bounds(right, &rmin, &rmax);
  if (lmax < rmin || lmin >= rmax)
    return result;
  result.known_mask = 0x0eu;
  return result;
}

static int zero_decision(Run *run, Word value, uint32_t pc, int *is_zero) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((value.known_mask & (1u << byte)) &&
        ((value.word >> (byte * 8u)) & 255u)) {
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

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width, Word value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97FrontendMainAccess *event = &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value =
        value.word &
        (width == 4 ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - 1u);
    event->operation = run->out->operations;
    event->width = (uint8_t)width;
    event->known_mask =
        (uint8_t)(value.known_mask & (uint8_t)((1u << width) - 1u));
    event->kind = kind;
  }
}

static int locate(Run *run, uint32_t address, unsigned width,
                  unsigned alignment, uint32_t pc, uint8_t **data,
                  uint8_t **known_bytes) {
  size_t i;
  unsigned byte;
  stop(run, pc, address, 0);
  TRY(spend(run));
  ++run->out->accesses;
  if (address & (alignment - 1u))
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0; i < run->context->memory.count; ++i) {
    Nba97GameTextRegion *region = &run->context->memory.region[i];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        width > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known_bytes = region->known ? region->known + (size_t)offset : 0;
    if (*known_bytes)
      for (byte = 0; byte < width; ++byte)
        if ((*known_bytes)[byte] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int effective_address(Run *run, unsigned base, uint32_t offset,
                             uint32_t pc, uint32_t *address) {
  Word computed = add(R(base), immediate(offset));
  if (computed.known_mask != 0x0fu) {
    stop(run, pc, computed.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *address = computed.word;
  return NBA97_TEXT_COMPLETE;
}

static int read_value(Run *run, unsigned destination, unsigned base,
                      uint32_t offset, unsigned width, unsigned alignment,
                      uint32_t pc) {
  uint32_t address;
  uint8_t *data;
  uint8_t *known_bytes;
  Word loaded = {0, 0};
  unsigned byte;
  TRY(effective_address(run, base, offset, pc, &address));
  TRY(locate(run, address, width, alignment, pc, &data, &known_bytes));
  for (byte = 0; byte < width; ++byte) {
    loaded.word |= (uint32_t)data[byte] << (byte * 8u);
    if (!known_bytes || known_bytes[byte])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << byte));
  }
  if (width == 1)
    loaded.known_mask = (uint8_t)(loaded.known_mask | 0x0eu);
  else if (width == 2)
    loaded.known_mask = (uint8_t)(loaded.known_mask | 0x0cu);
  R(destination) = loaded;
  ++run->out->reads;
  journal(run, NBA97_FRONTEND_MAIN_READ, pc, address, width, loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, unsigned source, unsigned base,
                       uint32_t offset, unsigned width, unsigned alignment,
                       uint32_t pc) {
  uint32_t address;
  uint8_t *data;
  uint8_t *known_bytes;
  uint8_t required = (uint8_t)((1u << width) - 1u);
  unsigned byte;
  TRY(effective_address(run, base, offset, pc, &address));
  TRY(locate(run, address, width, alignment, pc, &data, &known_bytes));
  if (!known_bytes && (R(source).known_mask & required) != required)
    return NBA97_TEXT_ARGUMENT;
  for (byte = 0; byte < width; ++byte) {
    data[byte] = (uint8_t)(R(source).word >> (byte * 8u));
    if (known_bytes)
      known_bytes[byte] = (uint8_t)((R(source).known_mask >> byte) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_FRONTEND_MAIN_STORE, pc, address, width, R(source));
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int invoke(Run *run, uint8_t site, uint32_t pc, uint32_t delay_pc,
                  uint32_t target, uint8_t argument_count,
                  uint8_t target_program, int *transferred) {
  Nba97FrontendMainEvent event;
  Nba97FrontendMainCalleeOutcome outcome =
      NBA97_FRONTEND_MAIN_CALLEE_RETURNED;
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
  publish(run);
  if (!run->context->io)
    return NBA97_TEXT_IO_REFUSED;
  accepted = run->context->io(run->context->user, &run->context->memory,
                              &event, &run->machine, &outcome);
  publish(run);
  if (accepted != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!machine_valid(&run->machine))
    return NBA97_TEXT_ARGUMENT;
  if (outcome != NBA97_FRONTEND_MAIN_CALLEE_RETURNED &&
      outcome != NBA97_FRONTEND_MAIN_CALLEE_TRANSFERRED)
    return NBA97_TEXT_ARGUMENT;
  if (target_program != NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD &&
      outcome != NBA97_FRONTEND_MAIN_CALLEE_RETURNED)
    return NBA97_TEXT_ARGUMENT;
  ++run->out->callbacks_completed;
  ++run->out->call_count[site];
  *transferred = outcome == NBA97_FRONTEND_MAIN_CALLEE_TRANSFERRED;
  return NBA97_TEXT_COMPLETE;
}

static int call(Run *run, uint8_t site, uint32_t pc, uint32_t delay_pc,
                uint32_t target, uint8_t argument_count) {
  int transferred = 0;
  TRY(invoke(run, site, pc, delay_pc, target, argument_count,
             NBA97_FRONTEND_MAIN_PROGRAM_FEONLY, &transferred));
  return NBA97_TEXT_COMPLETE;
}

int nba97_frontend_main(Nba97FrontendMainContext *context,
                        Nba97FrontendMainProgress *out) {
  Run storage;
  Run *run = &storage;
  int decision;
  int transferred;
  uint32_t dynamic_target;
  TRY(initialize(context, out, run));

  /* 0x80028800..0x8002881C: create the 40-byte frame and preserve all four
   * callee-saved words before the first two FEONLY services. */
  STEP(0x80028800); SP = add(SP, immediate(UINT32_C(0xffffffd8)));
  out->frame_stack_pointer = SP.word;
  STEP(0x80028804); TRY(write_value(run, 31, 29, 36, 4, 4, UINT32_C(0x80028804)));
  out->saved_return_address = RA;
  STEP(0x80028808); TRY(write_value(run, 18, 29, 32, 4, 4, UINT32_C(0x80028808)));
  STEP(0x8002880c); TRY(write_value(run, 17, 29, 28, 4, 4, UINT32_C(0x8002880c)));
  STEP(0x80028810); known(&RA, UINT32_C(0x80028818));
  STEP(0x80028814); TRY(write_value(run, 16, 29, 24, 4, 4, UINT32_C(0x80028814)));
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028810, UINT32_C(0x80028810), UINT32_C(0x80028814), UINT32_C(0x8007b844), 0));
  STEP(0x80028818); known(&RA, UINT32_C(0x80028820));
  STEP(0x8002881c);
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028818, UINT32_C(0x80028818), UINT32_C(0x8002881c), UINT32_C(0x8008b368), 0));

  /* 0x80028820..0x80028838: derive the frontend RAM span in the JAL delay. */
  STEP(0x80028820); known(&A2, UINT32_C(0x801f0000));
  STEP(0x80028824); A2 = or_immediate(A2, UINT32_C(0xd800));
  STEP(0x80028828); known(&A0, UINT32_C(0xdc));
  STEP(0x8002882c); known(&A1, UINT32_C(0x80100000));
  STEP(0x80028830); A1 = add(A1, immediate(UINT32_C(0xfffff5c8)));
  STEP(0x80028834); known(&RA, UINT32_C(0x8002883c));
  STEP(0x80028838); A2 = subtract(A2, A1);
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028834, UINT32_C(0x80028834), UINT32_C(0x80028838), UINT32_C(0x800769e0), 3));

  /* 0x8002883C..0x80028864: the first live initialized-flag load controls
   * cold setup; its branch delay initializes s1 on both paths. */
  STEP(0x8002883c); known(&V0, UINT32_C(0x80020000));
  STEP(0x80028840); TRY(read_value(run, 2, 2, UINT32_C(0x1ee4), 4, 4, UINT32_C(0x80028840)));
  out->loaded_initial_frontend_flag = V0;
  STEP(0x80028844);
  STEP(0x80028848);
  STEP(0x8002884c); known(&S1, UINT32_C(0x80010000));
  TRY(zero_decision(run, V0, UINT32_C(0x80028848), &decision));
  if (decision) {
    STEP(0x80028850); known(&AT, UINT32_C(0x80020000));
    STEP(0x80028854); TRY(write_value(run, 0, 1, UINT32_C(0xffffedec), 2, 2, UINT32_C(0x80028854)));
    STEP(0x80028858); known(&RA, UINT32_C(0x80028860));
    STEP(0x8002885c); known(&A0, 1);
    TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028858, UINT32_C(0x80028858), UINT32_C(0x8002885c), UINT32_C(0x80061674), 1));
    STEP(0x80028860); known(&S1, UINT32_C(0x80010000));
  }
  STEP(0x80028864); S1 = or_immediate(S1, UINT32_C(0x3800));

  /* 0x80028868..0x80028910: publish the first frontend pointers and execute
   * the fixed graphics, input, audio, and presentation initialization calls. */
  STEP(0x80028868); known(&A0, UINT32_C(0x80010000));
  STEP(0x8002886c); A0 = or_immediate(A0, UINT32_C(0x3800));
  STEP(0x80028870); known(&S0, UINT32_C(0x80020000));
  STEP(0x80028874); S0 = add(S0, immediate(UINT32_C(0x4844)));
  STEP(0x80028878); known(&AT, UINT32_C(0x80010000));
  STEP(0x8002887c); TRY(write_value(run, 17, 1, UINT32_C(0x70c4), 4, 4, UINT32_C(0x8002887c)));
  STEP(0x80028880); known(&RA, UINT32_C(0x80028888));
  STEP(0x80028884); A1 = S0;
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028880, UINT32_C(0x80028880), UINT32_C(0x80028884), UINT32_C(0x8008bfb0), 2));
  STEP(0x80028888); known(&AT, UINT32_C(0x80010000));
  STEP(0x8002888c); TRY(write_value(run, 2, 1, UINT32_C(0x5094), 4, 4, UINT32_C(0x8002888c)));
  STEP(0x80028890); known(&AT, UINT32_C(0x800e0000));
  STEP(0x80028894); TRY(write_value(run, 0, 1, UINT32_C(0xffff9b4c), 4, 4, UINT32_C(0x80028894)));
#define SIMPLE_CALL(site, pc, delay, target, argc)                             \
  do {                                                                         \
    STEP(pc); known(&RA, UINT32_C(pc) + 8u); STEP(delay);                      \
    TRY(call(run, site, UINT32_C(pc), UINT32_C(delay), UINT32_C(target), argc)); \
  } while (0)
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_80028898, 0x80028898, 0x8002889c, 0x80078b7c, 0);
  STEP(0x800288a0); known(&A0, UINT32_C(0x80020000));
  STEP(0x800288a4); A0 = add(A0, immediate(UINT32_C(0x484c)));
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_800288A8, 0x800288a8, 0x800288ac, 0x8008a4f8, 1);
  STEP(0x800288b0); known(&A0, UINT32_C(0x80010000));
  STEP(0x800288b4); A0 = or_immediate(A0, UINT32_C(0x000c));
  STEP(0x800288b8); known(&RA, UINT32_C(0x800288c0));
  STEP(0x800288bc); known(&A1, UINT32_C(0x2c3));
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_800288B8, UINT32_C(0x800288b8), UINT32_C(0x800288bc), UINT32_C(0x80079bf0), 2));
  STEP(0x800288c0); known(&RA, UINT32_C(0x800288c8));
  STEP(0x800288c4); known(&A0, 0);
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_800288C0, UINT32_C(0x800288c0), UINT32_C(0x800288c4), UINT32_C(0x8007f5a8), 1));
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_800288C8, 0x800288c8, 0x800288cc, 0x8007f5d0, 0);
  STEP(0x800288d0); known(&RA, UINT32_C(0x800288d8));
  STEP(0x800288d4); known(&A0, 8);
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_800288D0, UINT32_C(0x800288d0), UINT32_C(0x800288d4), UINT32_C(0x80076148), 1));
  STEP(0x800288d8); known(&RA, UINT32_C(0x800288e0));
  STEP(0x800288dc); known(&A0, 3);
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_800288D8, UINT32_C(0x800288d8), UINT32_C(0x800288dc), UINT32_C(0x8008004c), 1));
  STEP(0x800288e0); known(&V0, UINT32_C(0x78));
  STEP(0x800288e4); known(&AT, UINT32_C(0x800e0000));
  STEP(0x800288e8); TRY(write_value(run, 2, 1, UINT32_C(0xffff9adc), 4, 4, UINT32_C(0x800288e8)));
  STEP(0x800288ec); known(&RA, UINT32_C(0x800288f4));
  STEP(0x800288f0); known(&A0, UINT32_C(0x78));
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_800288EC, UINT32_C(0x800288ec), UINT32_C(0x800288f0), UINT32_C(0x8007844c), 1));
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_800288F4, 0x800288f4, 0x800288f8, 0x8008b104, 0);
  STEP(0x800288fc); known(&RA, UINT32_C(0x80028904));
  STEP(0x80028900); known(&A0, 0);
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_800288FC, UINT32_C(0x800288fc), UINT32_C(0x80028900), UINT32_C(0x800802b8), 1));
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_80028904, 0x80028904, 0x80028908, 0x80028b8c, 0);
  STEP(0x8002890c); known(&RA, UINT32_C(0x80028914));
  STEP(0x80028910); known(&A0, 0);
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_8002890C, UINT32_C(0x8002890c), UINT32_C(0x80028910), UINT32_C(0x80028ed0), 1));

  /* 0x80028914..0x80028960: build the four-halfword display rectangle on
   * the live frame; the final 0x100 halfword is a JAL delay store. */
  STEP(0x80028914); A0 = add(SP, immediate(16));
  STEP(0x80028918); known(&A1, 0);
  STEP(0x8002891c); known(&A2, 0);
  STEP(0x80028920); known(&V0, UINT32_C(0x200));
  STEP(0x80028924); TRY(write_value(run, 2, 29, 16, 2, 2, UINT32_C(0x80028924)));
  STEP(0x80028928); TRY(write_value(run, 2, 29, 20, 2, 2, UINT32_C(0x80028928)));
  STEP(0x8002892c); known(&V0, UINT32_C(0x100));
  STEP(0x80028930); TRY(write_value(run, 0, 29, 18, 2, 2, UINT32_C(0x80028930)));
  STEP(0x80028934); known(&RA, UINT32_C(0x8002893c));
  STEP(0x80028938); TRY(write_value(run, 2, 29, 22, 2, 2, UINT32_C(0x80028938)));
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028934, UINT32_C(0x80028934), UINT32_C(0x80028938), UINT32_C(0x800807d8), 3));
  STEP(0x8002893c); known(&RA, UINT32_C(0x80028944));
  STEP(0x80028940); known(&A0, 0);
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_8002893C, UINT32_C(0x8002893c), UINT32_C(0x80028940), UINT32_C(0x800804e8), 1));
  STEP(0x80028944); A0 = add(SP, immediate(16));
  STEP(0x80028948); known(&A1, 0);
  STEP(0x8002894c); known(&RA, UINT32_C(0x80028954));
  STEP(0x80028950); known(&A2, UINT32_C(0x100));
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_8002894C, UINT32_C(0x8002894c), UINT32_C(0x80028950), UINT32_C(0x800807d8), 3));
  STEP(0x80028954); known(&RA, UINT32_C(0x8002895c));
  STEP(0x80028958); known(&A0, 0);
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028954, UINT32_C(0x80028954), UINT32_C(0x80028958), UINT32_C(0x800804e8), 1));
  STEP(0x8002895c); known(&RA, UINT32_C(0x80028964));
  STEP(0x80028960); known(&A0, 1);
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_8002895C, UINT32_C(0x8002895c), UINT32_C(0x80028960), UINT32_C(0x8008044c), 1));

  /* 0x80028964..0x800289F0: republish frontend pointers, then choose one of
   * the two context pointer pairs from the unsigned halfword selector. */
  STEP(0x80028964); known(&A0, UINT32_C(0x80010000));
  STEP(0x80028968); A0 = or_immediate(A0, UINT32_C(0x3800));
  STEP(0x8002896c); known(&AT, UINT32_C(0x80010000));
  STEP(0x80028970); TRY(write_value(run, 17, 1, UINT32_C(0x70c4), 4, 4, UINT32_C(0x80028970)));
  STEP(0x80028974); known(&RA, UINT32_C(0x8002897c));
  STEP(0x80028978); A1 = S0;
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028974, UINT32_C(0x80028974), UINT32_C(0x80028978), UINT32_C(0x8008bfb0), 2));
  STEP(0x8002897c); known(&V1, UINT32_C(0x80020000));
  STEP(0x80028980); TRY(read_value(run, 3, 3, UINT32_C(0x1568), 2, 2, UINT32_C(0x80028980)));
  out->loaded_context_selector = V1;
  STEP(0x80028984); known(&AT, UINT32_C(0x80010000));
  STEP(0x80028988); TRY(write_value(run, 2, 1, UINT32_C(0x5094), 4, 4, UINT32_C(0x80028988)));
  STEP(0x8002898c); known(&V0, UINT32_C(0x80020000));
  STEP(0x80028990); V0 = add(V0, immediate(UINT32_C(0x14f0)));
  STEP(0x80028994); known(&A0, UINT32_C(0x80010000));
  STEP(0x80028998); A0 = add(A0, immediate(UINT32_C(0x726c)));
  STEP(0x8002899c); known(&AT, UINT32_C(0x80010000));
  STEP(0x800289a0); TRY(write_value(run, 2, 1, UINT32_C(0x70c0), 4, 4, UINT32_C(0x800289a0)));
  STEP(0x800289a4); known(&AT, UINT32_C(0x80020000));
  STEP(0x800289a8); TRY(write_value(run, 4, 1, UINT32_C(0x1504), 4, 4, UINT32_C(0x800289a8)));
  STEP(0x800289ac);
  STEP(0x800289b0); A1 = V0;
  TRY(zero_decision(run, V1, UINT32_C(0x800289ac), &decision));
  if (!decision) {
    STEP(0x800289b4); V0 = add(A0, immediate(UINT32_C(0x1708)));
    STEP(0x800289b8); known(&AT, UINT32_C(0x80020000));
    STEP(0x800289bc); TRY(write_value(run, 2, 1, UINT32_C(0x199c), 4, 4, UINT32_C(0x800289bc)));
    STEP(0x800289c0);
    STEP(0x800289c4); V0 = add(A0, immediate(UINT32_C(0x1de4)));
  } else {
    STEP(0x800289c8); V0 = add(A1, immediate(UINT32_C(0x7c)));
    STEP(0x800289cc); known(&AT, UINT32_C(0x80020000));
    STEP(0x800289d0); TRY(write_value(run, 2, 1, UINT32_C(0x199c), 4, 4, UINT32_C(0x800289d0)));
    STEP(0x800289d4); known(&V0, UINT32_C(0x80020000));
    STEP(0x800289d8); V0 = add(V0, immediate(UINT32_C(0x2ae0)));
  }
  STEP(0x800289dc); known(&AT, UINT32_C(0x80020000));
  STEP(0x800289e0); TRY(write_value(run, 2, 1, UINT32_C(0x1520), 4, 4, UINT32_C(0x800289e0)));
  STEP(0x800289e4); known(&V0, UINT32_C(0x800c0000));
  STEP(0x800289e8); V0 = add(V0, immediate(UINT32_C(0xffffc424)));
  STEP(0x800289ec); known(&AT, UINT32_C(0x80010000));
  STEP(0x800289f0); TRY(write_value(run, 2, 1, UINT32_C(0x5030), 4, 4, UINT32_C(0x800289f0)));

  /* 0x800289F4..0x80028A64: finish fixed initialization. The first call's
   * delay clears context+0x730 through the live a1 pointer. */
  STEP(0x800289f4); known(&RA, UINT32_C(0x800289fc));
  STEP(0x800289f8); TRY(write_value(run, 0, 5, UINT32_C(0x730), 1, 1, UINT32_C(0x800289f8)));
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_800289F4, UINT32_C(0x800289f4), UINT32_C(0x800289f8), UINT32_C(0x80035d80), 0));
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_800289FC, 0x800289fc, 0x80028a00, 0x800517bc, 0);
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_80028A04, 0x80028a04, 0x80028a08, 0x800673a0, 0);
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_80028A0C, 0x80028a0c, 0x80028a10, 0x8008da98, 0);
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_80028A14, 0x80028a14, 0x80028a18, 0x8008acb0, 0);

  /* 0x80028A1C..0x80028A4C: independently reload the unsigned intro flag;
   * only values 1..99 enter the optional service. */
  STEP(0x80028a1c); known(&V1, UINT32_C(0x80020000));
  STEP(0x80028a20); TRY(read_value(run, 3, 3, UINT32_C(0xffffedec), 2, 2, UINT32_C(0x80028a20)));
  out->loaded_intro_flag = V1;
  STEP(0x80028a24); known(&V0, 1);
  STEP(0x80028a28); known(&AT, UINT32_C(0x800e0000));
  STEP(0x80028a2c); TRY(write_value(run, 0, 1, UINT32_C(0xffff9b3c), 4, 4, UINT32_C(0x80028a2c)));
  STEP(0x80028a30); known(&AT, UINT32_C(0x800e0000));
  STEP(0x80028a34); TRY(write_value(run, 2, 1, UINT32_C(0xffff9b40), 4, 4, UINT32_C(0x80028a34)));
  STEP(0x80028a38);
  STEP(0x80028a3c); V0 = unsigned_less(V1, immediate(100));
  TRY(zero_decision(run, V1, UINT32_C(0x80028a38), &decision));
  if (!decision) {
    STEP(0x80028a40);
    STEP(0x80028a44);
    TRY(zero_decision(run, V0, UINT32_C(0x80028a40), &decision));
    if (!decision) {
      STEP(0x80028a48); known(&RA, UINT32_C(0x80028a50));
      STEP(0x80028a4c);
      TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028A48, UINT32_C(0x80028a48), UINT32_C(0x80028a4c), UINT32_C(0x80036008), 0));
    }
  }
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_80028A50, 0x80028a50, 0x80028a54, 0x80035984, 0);
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_80028A58, 0x80028a58, 0x80028a5c, 0x8008e5a0, 0);
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_80028A60, 0x80028a60, 0x80028a64, 0x80064c90, 0);

  /* 0x80028A68..0x80028A9C: the second live initialized-flag load controls
   * an unbounded signed callback-live countdown loop. */
  STEP(0x80028a68); known(&V0, UINT32_C(0x80020000));
  STEP(0x80028a6c); TRY(read_value(run, 2, 2, UINT32_C(0x1ee4), 4, 4, UINT32_C(0x80028a6c)));
  out->loaded_menu_frontend_flag = V0;
  STEP(0x80028a70);
  STEP(0x80028a74);
  STEP(0x80028a78);
  TRY(zero_decision(run, V0, UINT32_C(0x80028a74), &decision));
  if (decision) {
    STEP(0x80028a7c); known(&RA, UINT32_C(0x80028a84));
    STEP(0x80028a80);
    TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028A7C, UINT32_C(0x80028a7c), UINT32_C(0x80028a80), UINT32_C(0x8008da5c), 0));
    STEP(0x80028a84); S0 = V0;
    STEP(0x80028a88);
    STEP(0x80028a8c);
    TRY(zero_decision(run, S0, UINT32_C(0x80028a88), &decision));
    while (!decision) {
      STEP(0x80028a90); known(&RA, UINT32_C(0x80028a98));
      STEP(0x80028a94); S0 = add(S0, immediate(UINT32_C(0xffffffff)));
      TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028A90, UINT32_C(0x80028a90), UINT32_C(0x80028a94), UINT32_C(0x80029b20), 0));
      ++out->intro_iterations;
      STEP(0x80028a98);
      STEP(0x80028a9c);
      TRY(zero_decision(run, S0, UINT32_C(0x80028a98), &decision));
    }
  }

  /* 0x80028AA0..0x80028AB4: clear s0 in the recovered wrapper's JAL delay,
   * then execute the first two teardown services. */
  STEP(0x80028aa0); known(&RA, UINT32_C(0x80028aa8));
  STEP(0x80028aa4); known(&S0, 0);
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028AA0, UINT32_C(0x80028aa0), UINT32_C(0x80028aa4), UINT32_C(0x800360d4), 0));
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_80028AA8, 0x80028aa8, 0x80028aac, 0x8002f084, 0);
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_80028AB0, 0x80028ab0, 0x80028ab4, 0x80028e08, 0);

  /* 0x80028AB8..0x80028B18: load gameload.bin, capture its size before the
   * first shutdown callback, then perform the signed callback-live wait. */
  STEP(0x80028ab8); known(&A0, UINT32_C(0x80020000));
  STEP(0x80028abc); A0 = add(A0, immediate(UINT32_C(0x4854)));
  STEP(0x80028ac0); known(&V0, 1);
  STEP(0x80028ac4); known(&AT, UINT32_C(0x800e0000));
  STEP(0x80028ac8); TRY(write_value(run, 2, 1, UINT32_C(0xffff9b40), 4, 4, UINT32_C(0x80028ac8)));
  STEP(0x80028acc); known(&RA, UINT32_C(0x80028ad4));
  STEP(0x80028ad0); known(&A1, 0);
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028ACC, UINT32_C(0x80028acc), UINT32_C(0x80028ad0), UINT32_C(0x8007b11c), 2));
  STEP(0x80028ad4); S1 = V0; out->gameload_handle = S1;
  STEP(0x80028ad8); known(&RA, UINT32_C(0x80028ae0));
  STEP(0x80028adc); A0 = S1;
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028AD8, UINT32_C(0x80028ad8), UINT32_C(0x80028adc), UINT32_C(0x80077cd4), 1));
  STEP(0x80028ae0); known(&A0, 0);
  STEP(0x80028ae4); known(&A1, 0);
  STEP(0x80028ae8); known(&AT, UINT32_C(0x800e0000));
  STEP(0x80028aec); TRY(write_value(run, 0, 1, UINT32_C(0xffff9b40), 4, 4, UINT32_C(0x80028aec)));
  STEP(0x80028af0); known(&RA, UINT32_C(0x80028af8));
  STEP(0x80028af4); S2 = V0; out->gameload_size = S2;
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028AF0, UINT32_C(0x80028af0), UINT32_C(0x80028af4), UINT32_C(0x80084c44), 2));
  STEP(0x80028af8); known(&RA, UINT32_C(0x80028b00));
  STEP(0x80028afc); known(&A0, 0);
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028AF8, UINT32_C(0x80028af8), UINT32_C(0x80028afc), UINT32_C(0x80084c84), 1));
  STEP(0x80028b00); known(&RA, UINT32_C(0x80028b08));
  STEP(0x80028b04); known(&A0, 0);
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028B00, UINT32_C(0x80028b00), UINT32_C(0x80028b04), UINT32_C(0x80084c9c), 1));
  do {
    STEP(0x80028b08); known(&RA, UINT32_C(0x80028b10));
    STEP(0x80028b0c); S0 = add(S0, immediate(1));
    TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028B08, UINT32_C(0x80028b08), UINT32_C(0x80028b0c), UINT32_C(0x80028b8c), 0));
    ++out->wait_iterations;
    STEP(0x80028b10); V0 = signed_less(S0, immediate(20));
    STEP(0x80028b14);
    STEP(0x80028b18);
    TRY(zero_decision(run, V0, UINT32_C(0x80028b14), &decision));
  } while (!decision);

  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_80028B1C, 0x80028b1c, 0x80028b20, 0x8008b1f0, 0);
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_80028B24, 0x80028b24, 0x80028b28, 0x800785f0, 0);
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_80028B2C, 0x80028b2c, 0x80028b30, 0x80076110, 0);
  SIMPLE_CALL(NBA97_FRONTEND_MAIN_SITE_80028B34, 0x80028b34, 0x80028b38, 0x80051b44, 0);
  STEP(0x80028b3c); known(&A0, UINT32_C(0x800e0000));
  STEP(0x80028b40); A0 = add(A0, immediate(UINT32_C(0xffff96e8)));
  STEP(0x80028b44); known(&RA, UINT32_C(0x80028b4c));
  STEP(0x80028b48); known(&A1, UINT32_C(0x20));
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028B44, UINT32_C(0x80028b44), UINT32_C(0x80028b48), UINT32_C(0x8008a944), 2));
  STEP(0x80028b4c); A0 = S1;
  STEP(0x80028b50); known(&A1, UINT32_C(0x801e0000));
  STEP(0x80028b54); known(&RA, UINT32_C(0x80028b5c));
  STEP(0x80028b58); A2 = S2;
  TRY(call(run, NBA97_FRONTEND_MAIN_SITE_80028B54, UINT32_C(0x80028b54), UINT32_C(0x80028b58), UINT32_C(0x800909a8), 3));

  /* 0x80028B5C..0x80028B6C: the loaded word names a GAMELOAD overlay entry,
   * never a same-address FELOAD owner. JALR writes ra before its NOP delay. */
  STEP(0x80028b5c); known(&V0, UINT32_C(0x801e0000));
  STEP(0x80028b60); TRY(read_value(run, 2, 2, 0, 4, 4, UINT32_C(0x80028b60)));
  out->dynamic_entry = V0;
  STEP(0x80028b64);
  dynamic_target = V0.word;
  STEP(0x80028b68); known(&RA, UINT32_C(0x80028b70));
  STEP(0x80028b6c);
  if (out->dynamic_entry.known_mask != 0x0fu) {
    stop(run, UINT32_C(0x80028b68), 0, dynamic_target);
    return NBA97_TEXT_UNKNOWN;
  }
  if (dynamic_target & 3u) {
    stop(run, UINT32_C(0x80028b68), 0, dynamic_target);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  transferred = 0;
  TRY(invoke(run, NBA97_FRONTEND_MAIN_SITE_80028B68, UINT32_C(0x80028b68),
             UINT32_C(0x80028b6c), dynamic_target, 0,
             NBA97_FRONTEND_MAIN_PROGRAM_GAMELOAD, &transferred));
  if (transferred) {
    out->completed = 1;
    out->transferred = 1;
    stop(run, 0, 0, 0);
    return NBA97_TEXT_COMPLETE;
  }

  /* 0x80028B70..0x80028B88: a returning GAMELOAD callback restores through
   * callback-live sp and consumes the restored ra after the JR NOP delay. */
  STEP(0x80028b70); TRY(read_value(run, 31, 29, 36, 4, 4, UINT32_C(0x80028b70))); out->restored_return_address = RA;
  STEP(0x80028b74); TRY(read_value(run, 18, 29, 32, 4, 4, UINT32_C(0x80028b74))); out->restored_s2 = S2;
  STEP(0x80028b78); TRY(read_value(run, 17, 29, 28, 4, 4, UINT32_C(0x80028b78))); out->restored_s1 = S1;
  STEP(0x80028b7c); TRY(read_value(run, 16, 29, 24, 4, 4, UINT32_C(0x80028b7c))); out->restored_s0 = S0;
  STEP(0x80028b80); SP = add(SP, immediate(40)); publish(run);
  STEP(0x80028b84);
  STEP(0x80028b88);
  if (RA.known_mask != 0x0fu) {
    stop(run, UINT32_C(0x80028b84), 0, RA.word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (RA.word & 3u) {
    stop(run, UINT32_C(0x80028b84), 0, RA.word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
#undef SIMPLE_CALL
}
