#include "game_team_header_initialize.h"

#include <limits.h>
#include <string.h>

#define R(i) (run->machine.registers.gpr[(i)])
#define TRY(expression)                                                        \
  do {                                                                         \
    int result_ = (expression);                                                \
    if (result_ != NBA97_TEXT_COMPLETE)                                        \
      return result_;                                                          \
  } while (0)

typedef struct Run {
  Nba97GameTeamHeaderInitializeContext *context;
  Nba97GameTeamHeaderInitializeProgress *out;
  Nba97GameTeamHeaderInitializeMachine machine;
} Run;

static void publish(Run *run) { run->out->machine = run->machine; }

static void stop(Run *run, uint32_t pc, uint32_t address) {
  run->out->stopped_pc = pc;
  run->out->stopped_address = address;
  publish(run);
}

static void known(Nba97GameTeamHeaderInitializeWord *value, uint32_t word) {
  value->word = word;
  value->known_mask = 15;
}

static int machine_valid(const Nba97GameTeamHeaderInitializeMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0 ||
      machine->registers.gpr[0].known_mask != 15 ||
      machine->hi.known_mask > 15 || machine->lo.known_mask > 15)
    return 0;
  for (i = 0; i < 32; ++i)
    if (machine->registers.gpr[i].known_mask > 15)
      return 0;
  return 1;
}

static int initialize(Nba97GameTeamHeaderInitializeContext *context,
                      Nba97GameTeamHeaderInitializeProgress *out, Run *run) {
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

/* Enumerating byte carries preserves every source-provable result byte. */
static Nba97GameTeamHeaderInitializeWord
add(Nba97GameTeamHeaderInitializeWord left,
    Nba97GameTeamHeaderInitializeWord right) {
  Nba97GameTeamHeaderInitializeWord result;
  unsigned carry_mask = 1;
  unsigned byte;
  result.word = left.word + right.word;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned left_start = (left.known_mask & (1u << byte))
                              ? ((left.word >> (byte * 8u)) & 255u)
                              : 0;
    unsigned left_end = (left.known_mask & (1u << byte)) ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte))
                               ? ((right.word >> (byte * 8u)) & 255u)
                               : 0;
    unsigned right_end = (right.known_mask & (1u << byte)) ? right_start : 255u;
    unsigned carry;
    for (carry = 0; carry < 2; ++carry) {
      unsigned a;
      if (!(carry_mask & (1u << carry)))
        continue;
      for (a = left_start; a <= left_end; ++a) {
        unsigned b;
        for (b = right_start; b <= right_end; ++b) {
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
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
    carry_mask = next_mask;
  }
  return result;
}

static Nba97GameTeamHeaderInitializeWord
add_constant(Nba97GameTeamHeaderInitializeWord source, uint32_t value) {
  Nba97GameTeamHeaderInitializeWord constant;
  known(&constant, value);
  return add(source, constant);
}

static Nba97GameTeamHeaderInitializeWord
subtract(Nba97GameTeamHeaderInitializeWord left,
         Nba97GameTeamHeaderInitializeWord right) {
  Nba97GameTeamHeaderInitializeWord result;
  unsigned borrow_mask = 1;
  unsigned byte;
  result.word = left.word - right.word;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned left_start = (left.known_mask & (1u << byte))
                              ? ((left.word >> (byte * 8u)) & 255u)
                              : 0;
    unsigned left_end = (left.known_mask & (1u << byte)) ? left_start : 255u;
    unsigned right_start = (right.known_mask & (1u << byte))
                               ? ((right.word >> (byte * 8u)) & 255u)
                               : 0;
    unsigned right_end = (right.known_mask & (1u << byte)) ? right_start : 255u;
    unsigned borrow;
    for (borrow = 0; borrow < 2; ++borrow) {
      unsigned a;
      if (!(borrow_mask & (1u << borrow)))
        continue;
      for (a = left_start; a <= left_end; ++a) {
        unsigned b;
        for (b = right_start; b <= right_end; ++b) {
          unsigned output = (a - b - borrow) & 255u;
          next_mask |= 1u << (a < b + borrow);
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
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
    borrow_mask = next_mask;
  }
  return result;
}

static Nba97GameTeamHeaderInitializeWord
shift_left(Nba97GameTeamHeaderInitializeWord source, unsigned amount) {
  Nba97GameTeamHeaderInitializeWord result;
  uint32_t known_bits = 0;
  uint32_t shifted_known;
  unsigned byte;
  result.word = source.word << amount;
  for (byte = 0; byte < 4; ++byte)
    if (source.known_mask & (1u << byte))
      known_bits |= UINT32_C(255) << (byte * 8u);
  shifted_known = (known_bits << amount) | ((UINT32_C(1) << amount) - 1u);
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte)
    if (((shifted_known >> (byte * 8u)) & 255u) == 255u)
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
  return result;
}

static Nba97GameTeamHeaderInitializeWord
shift_right_arithmetic(Nba97GameTeamHeaderInitializeWord source,
                       unsigned amount) {
  Nba97GameTeamHeaderInitializeWord result;
  uint32_t known_bits = 0;
  uint32_t shifted_known;
  uint32_t fill = UINT32_MAX << (32u - amount);
  unsigned byte;
  result.word = source.word >> amount;
  if (source.word & UINT32_C(0x80000000))
    result.word |= fill;
  for (byte = 0; byte < 4; ++byte)
    if (source.known_mask & (1u << byte))
      known_bits |= UINT32_C(255) << (byte * 8u);
  shifted_known = known_bits >> amount;
  if (source.known_mask & 8u)
    shifted_known |= fill;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte)
    if (((shifted_known >> (byte * 8u)) & 255u) == 255u)
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
  return result;
}

static Nba97GameTeamHeaderInitializeWord
and_constant(Nba97GameTeamHeaderInitializeWord source, uint32_t constant) {
  Nba97GameTeamHeaderInitializeWord result;
  unsigned byte;
  result.word = source.word & constant;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned mask = (constant >> (byte * 8u)) & 255u;
    if ((source.known_mask & (1u << byte)) || mask == 0)
      result.known_mask = (uint8_t)(result.known_mask | (uint8_t)(1u << byte));
  }
  return result;
}

static void unsigned_bounds(Nba97GameTeamHeaderInitializeWord value,
                            uint32_t *minimum, uint32_t *maximum) {
  uint32_t low = 0;
  uint32_t high = 0;
  unsigned byte;
  for (byte = 0; byte < 4; ++byte) {
    uint32_t part = (value.word >> (byte * 8u)) & 255u;
    low |= ((value.known_mask & (1u << byte)) ? part : 0u) << (byte * 8u);
    high |= ((value.known_mask & (1u << byte)) ? part : 255u) << (byte * 8u);
  }
  *minimum = low;
  *maximum = high;
}

static void signed_bounds(Nba97GameTeamHeaderInitializeWord value,
                          int64_t *minimum, int64_t *maximum) {
  uint32_t low;
  uint32_t high;
  if (!(value.known_mask & 8u)) {
    *minimum = INT32_MIN;
    *maximum = INT32_MAX;
    return;
  }
  unsigned_bounds(value, &low, &high);
  *minimum = (low & UINT32_C(0x80000000)) ? (int64_t)low - INT64_C(0x100000000)
                                          : (int64_t)low;
  *maximum = (high & UINT32_C(0x80000000))
                 ? (int64_t)high - INT64_C(0x100000000)
                 : (int64_t)high;
}

static Nba97GameTeamHeaderInitializeWord
unsigned_less_constant(Nba97GameTeamHeaderInitializeWord source,
                       uint32_t constant) {
  Nba97GameTeamHeaderInitializeWord result;
  uint32_t minimum;
  uint32_t maximum;
  unsigned_bounds(source, &minimum, &maximum);
  result.word = source.word < constant;
  result.known_mask = (maximum < constant || minimum >= constant) ? 15 : 14;
  return result;
}

static Nba97GameTeamHeaderInitializeWord
signed_less(Nba97GameTeamHeaderInitializeWord left,
            Nba97GameTeamHeaderInitializeWord right) {
  Nba97GameTeamHeaderInitializeWord result;
  int64_t left_minimum;
  int64_t left_maximum;
  int64_t right_minimum;
  int64_t right_maximum;
  signed_bounds(left, &left_minimum, &left_maximum);
  signed_bounds(right, &right_minimum, &right_maximum);
  result.word =
      (left.word ^ UINT32_C(0x80000000)) < (right.word ^ UINT32_C(0x80000000));
  result.known_mask =
      (left_maximum < right_minimum || left_minimum >= right_maximum) ? 15 : 14;
  return result;
}

static int spend(Run *run) {
  if (run->out->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->out->operations;
  return NBA97_TEXT_COMPLETE;
}

static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width, Nba97GameTeamHeaderInitializeWord value) {
  size_t index = run->out->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameTeamHeaderInitializeAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word;
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
  size_t j;
  stop(run, pc, address);
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
      for (j = 0; j < width; ++j)
        if ((*known_bytes)[j] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

static int read_value(Run *run, uint32_t address, unsigned width,
                      unsigned alignment, uint32_t pc,
                      Nba97GameTeamHeaderInitializeWord *value) {
  uint8_t *data;
  uint8_t *known_bytes;
  Nba97GameTeamHeaderInitializeWord loaded = {0, 0};
  unsigned i;
  TRY(locate(run, address, width, alignment, pc, &data, &known_bytes));
  for (i = 0; i < width; ++i) {
    loaded.word |= (uint32_t)data[i] << (i * 8u);
    if (!known_bytes || known_bytes[i])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (uint8_t)(1u << i));
  }
  if (width == 1) {
    loaded.word &= 255u;
    loaded.known_mask = (uint8_t)(loaded.known_mask | 14u);
  } else if (width == 2) {
    loaded.word &= 65535u;
    loaded.known_mask = (uint8_t)(loaded.known_mask | 12u);
  }
  *value = loaded;
  ++run->out->reads;
  journal(run, NBA97_GAME_TEAM_HEADER_INITIALIZE_READ, pc, address, width,
          loaded);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int write_value(Run *run, uint32_t address, unsigned width,
                       unsigned alignment, uint32_t pc,
                       Nba97GameTeamHeaderInitializeWord value) {
  uint8_t *data;
  uint8_t *known_bytes;
  uint8_t required = (uint8_t)((1u << width) - 1u);
  unsigned i;
  TRY(locate(run, address, width, alignment, pc, &data, &known_bytes));
  if (!known_bytes && (value.known_mask & required) != required)
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < width; ++i) {
    data[i] = (uint8_t)(value.word >> (i * 8u));
    if (known_bytes)
      known_bytes[i] = (uint8_t)((value.known_mask >> i) & 1u);
  }
  ++run->out->stores;
  journal(run, NBA97_GAME_TEAM_HEADER_INITIALIZE_STORE, pc, address, width,
          value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int effective_address(Run *run, Nba97GameTeamHeaderInitializeWord base,
                             uint32_t offset, uint32_t pc, uint32_t *result) {
  Nba97GameTeamHeaderInitializeWord effective = add_constant(base, offset);
  if (effective.known_mask != 15) {
    stop(run, pc, effective.word);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = effective.word;
  return NBA97_TEXT_COMPLETE;
}

static int load_register(Run *run, unsigned destination, unsigned base,
                         uint32_t offset, unsigned width, unsigned alignment,
                         uint32_t pc) {
  uint32_t address;
  Nba97GameTeamHeaderInitializeWord value;
  TRY(effective_address(run, R(base), offset, pc, &address));
  TRY(read_value(run, address, width, alignment, pc, &value));
  R(destination) = value;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static int store_register(Run *run, unsigned source, unsigned base,
                          uint32_t offset, unsigned width, unsigned alignment,
                          uint32_t pc) {
  uint32_t address;
  TRY(effective_address(run, R(base), offset, pc, &address));
  return write_value(run, address, width, alignment, pc, R(source));
}

static int equal(Run *run, Nba97GameTeamHeaderInitializeWord left,
                 Nba97GameTeamHeaderInitializeWord right, uint32_t pc,
                 int *result) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((left.known_mask & right.known_mask & (1u << byte)) &&
        (((left.word ^ right.word) >> (byte * 8u)) & 255u)) {
      *result = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (left.known_mask == 15 && right.known_mask == 15) {
    *result = left.word == right.word;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0);
  return NBA97_TEXT_UNKNOWN;
}

static int branch_word(Run *run, Nba97GameTeamHeaderInitializeWord value,
                       uint32_t pc, int *nonzero) {
  Nba97GameTeamHeaderInitializeWord zero;
  int is_zero;
  known(&zero, 0);
  TRY(equal(run, value, zero, pc, &is_zero));
  *nonzero = !is_zero;
  return NBA97_TEXT_COMPLETE;
}

static int nonnegative(Run *run, Nba97GameTeamHeaderInitializeWord value,
                       uint32_t pc, int *result) {
  if (!(value.known_mask & 8u)) {
    stop(run, pc, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = !(value.word & UINT32_C(0x80000000));
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_team_header_initialize(
    Nba97GameTeamHeaderInitializeContext *context,
    Nba97GameTeamHeaderInitializeProgress *out) {
  Run storage;
  Run *run = &storage;
  Nba97GameTeamHeaderInitializeWord zero;
  Nba97GameTeamHeaderInitializeWord constant;
  Nba97GameTeamHeaderInitializeWord difficulty_test;
  int branch;
  TRY(initialize(context, out, run));
  known(&zero, 0);

  /* 0x800655B0..0x80065604: retain the destination and select the source's
   * side-specific status, injury, table-index, and alias globals. */
  R(NBA97_MATCH_INITIALIZE_T0) = R(NBA97_MATCH_INITIALIZE_A0);
  TRY(load_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_T0,
                    0x14, 2, 2, 0x800655b4));
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xfffffff0));
  TRY(equal(run, R(NBA97_MATCH_INITIALIZE_V0), zero, 0x800655bc, &branch));
  if (!branch) {
    known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
    R(NBA97_MATCH_INITIALIZE_A0) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0xfffff984));
    R(NBA97_MATCH_INITIALIZE_V0) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A0), 0x1238);
    TRY(store_register(run, NBA97_MATCH_INITIALIZE_V0,
                       NBA97_MATCH_INITIALIZE_T0, 0x7c, 4, 4, 0x800655d0));
    known(&R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0x80020000));
    TRY(load_register(run, NBA97_MATCH_INITIALIZE_A3, NBA97_MATCH_INITIALIZE_A3,
                      0x1ed6, 1, 1, 0x800655d8));
    known(&R(NBA97_MATCH_INITIALIZE_T0 + 4), 12);
    known(&R(NBA97_MATCH_INITIALIZE_T0 + 5), 24);
  } else {
    known(&R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0x80020000));
    R(NBA97_MATCH_INITIALIZE_A0) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A0), UINT32_C(0xfffff7ec));
    R(NBA97_MATCH_INITIALIZE_V0) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A0), 0x13a0);
    TRY(store_register(run, NBA97_MATCH_INITIALIZE_V0,
                       NBA97_MATCH_INITIALIZE_T0, 0x7c, 4, 4, 0x800655f4));
    known(&R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0x80020000));
    TRY(load_register(run, NBA97_MATCH_INITIALIZE_A3, NBA97_MATCH_INITIALIZE_A3,
                      0x1ed5, 1, 1, 0x800655fc));
    known(&R(NBA97_MATCH_INITIALIZE_T0 + 4), 0);
    known(&R(NBA97_MATCH_INITIALIZE_T0 + 5), 12);
  }

  /* 0x80065608..0x80065668: preserve duplicate team-id reads, resolve the
   * metadata pointer and count byte, clamp the count, and copy it twice. */
  TRY(load_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_T0,
                    0, 2, 2, 0x80065608));
  TRY(load_register(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_T0,
                    0, 2, 2, 0x8006560c));
  R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_V0), 2);
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
  R(NBA97_MATCH_INITIALIZE_AT) =
      add(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(load_register(run, NBA97_MATCH_INITIALIZE_T0 + 3,
                    NBA97_MATCH_INITIALIZE_AT, 0xb0c, 4, 4, 0x8006561c));
  R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_V1), 1);
  R(NBA97_MATCH_INITIALIZE_V0) =
      add(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
  R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_V0), 2);
  R(NBA97_MATCH_INITIALIZE_V0) =
      add(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
  R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_V0), 3);
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_T0 + 3,
                     NBA97_MATCH_INITIALIZE_T0, 0x6c, 4, 4, 0x80065634));
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
  R(NBA97_MATCH_INITIALIZE_AT) =
      add(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(load_register(run, NBA97_MATCH_INITIALIZE_A2, NBA97_MATCH_INITIALIZE_AT,
                    0x3aec, 1, 1, 0x80065640));
  R(NBA97_MATCH_INITIALIZE_V0) =
      unsigned_less_constant(R(NBA97_MATCH_INITIALIZE_A2), UINT32_C(12));
  known(&R(NBA97_MATCH_INITIALIZE_V1), 12);
  TRY(branch_word(run, R(NBA97_MATCH_INITIALIZE_V0), 0x8006564c, &branch));
  if (branch)
    R(NBA97_MATCH_INITIALIZE_V1) =
        and_constant(R(NBA97_MATCH_INITIALIZE_A2), 255);
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_T0,
                     0x66, 2, 2, 0x80065658));
  TRY(load_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_T0,
                    0x66, 2, 2, 0x8006565c));
  known(&R(NBA97_MATCH_INITIALIZE_A2), 0);
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_T0,
                     0x68, 2, 2, 0x80065668));
  {
    int64_t minimum;
    int64_t maximum;
    signed_bounds(R(NBA97_MATCH_INITIALIZE_V0), &minimum, &maximum);
    if (maximum <= 0)
      branch = 1;
    else if (minimum > 0)
      branch = 0;
    else {
      stop(run, 0x80065664, 0);
      return NBA97_TEXT_UNKNOWN;
    }
  }

  /* 0x8006566C..0x800656A0: mark the active records. The source reloads the
   * live count after each store, so aliases can change the loop immediately. */
  if (!branch) {
    known(&R(NBA97_MATCH_INITIALIZE_T0 + 2), UINT32_C(0xfffffffe));
    known(&R(NBA97_MATCH_INITIALIZE_T0 + 1), UINT32_C(0x7fff));
    R(NBA97_MATCH_INITIALIZE_V1) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A0), 0x20);
    for (;;) {
      TRY(equal(run, R(NBA97_MATCH_INITIALIZE_A3), R(NBA97_MATCH_INITIALIZE_A2),
                0x80065678, &branch));
      if (branch)
        TRY(store_register(run, NBA97_MATCH_INITIALIZE_T0 + 2,
                           NBA97_MATCH_INITIALIZE_V1, 0, 2, 2, 0x80065684));
      else
        TRY(store_register(run, NBA97_MATCH_INITIALIZE_T0 + 1,
                           NBA97_MATCH_INITIALIZE_V1, 0, 2, 2, 0x80065688));
      ++out->status_iterations;
      TRY(load_register(run, NBA97_MATCH_INITIALIZE_V0,
                        NBA97_MATCH_INITIALIZE_T0, 0x66, 2, 2, 0x8006568c));
      R(NBA97_MATCH_INITIALIZE_V1) =
          add_constant(R(NBA97_MATCH_INITIALIZE_V1), 0x22);
      R(NBA97_MATCH_INITIALIZE_A2) =
          add_constant(R(NBA97_MATCH_INITIALIZE_A2), 1);
      R(NBA97_MATCH_INITIALIZE_V0) = signed_less(R(NBA97_MATCH_INITIALIZE_A2),
                                                 R(NBA97_MATCH_INITIALIZE_V0));
      R(NBA97_MATCH_INITIALIZE_A0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_A0), 0x22);
      TRY(branch_word(run, R(NBA97_MATCH_INITIALIZE_V0), 0x8006569c, &branch));
      if (!branch)
        break;
    }
  }

  /* 0x800656A4..0x800656C0: fill every remaining slot with 0xFFFE. */
  known(&constant, 12);
  R(NBA97_MATCH_INITIALIZE_V0) =
      signed_less(R(NBA97_MATCH_INITIALIZE_A2), constant);
  known(&R(NBA97_MATCH_INITIALIZE_V1), UINT32_C(0xfffffffe));
  TRY(branch_word(run, R(NBA97_MATCH_INITIALIZE_V0), 0x800656a8, &branch));
  while (branch) {
    TRY(store_register(run, NBA97_MATCH_INITIALIZE_V1,
                       NBA97_MATCH_INITIALIZE_A0, 0x20, 2, 2, 0x800656b0));
    ++out->unused_iterations;
    R(NBA97_MATCH_INITIALIZE_A2) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A2), 1);
    R(NBA97_MATCH_INITIALIZE_V0) =
        signed_less(R(NBA97_MATCH_INITIALIZE_A2), constant);
    R(NBA97_MATCH_INITIALIZE_A0) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A0), 0x22);
    TRY(branch_word(run, R(NBA97_MATCH_INITIALIZE_V0), 0x800656bc, &branch));
  }

  /* 0x800656C4..0x80065740: register five actors in descending local-index
   * order and write each opponent index in the branch delay slot. */
  known(&R(NBA97_MATCH_INITIALIZE_A2), 4);
  known(&R(NBA97_MATCH_INITIALIZE_T0 + 1), UINT32_C(0x80020000));
  R(NBA97_MATCH_INITIALIZE_T0 + 1) =
      add_constant(R(NBA97_MATCH_INITIALIZE_T0 + 1), UINT32_C(0xbec));
  known(&R(NBA97_MATCH_INITIALIZE_T0 + 2), UINT32_C(0x80100000));
  R(NBA97_MATCH_INITIALIZE_T0 + 2) =
      add_constant(R(NBA97_MATCH_INITIALIZE_T0 + 2), UINT32_C(0xffffdcec));
  R(NBA97_MATCH_INITIALIZE_A3) = add_constant(R(NBA97_MATCH_INITIALIZE_T0), 8);
  for (;;) {
    TRY(load_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_A3,
                      0x16, 2, 2, 0x800656dc));
    TRY(store_register(run, NBA97_MATCH_INITIALIZE_V0,
                       NBA97_MATCH_INITIALIZE_A3, 0x98, 2, 2, 0x800656e4));
    TRY(load_register(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_T0,
                      0x14, 2, 2, 0x800656e8));
    R(NBA97_MATCH_INITIALIZE_V1) =
        add(R(NBA97_MATCH_INITIALIZE_A2), R(NBA97_MATCH_INITIALIZE_V1));
    R(NBA97_MATCH_INITIALIZE_A0) = shift_left(R(NBA97_MATCH_INITIALIZE_V1), 2);
    R(NBA97_MATCH_INITIALIZE_A0) =
        add(R(NBA97_MATCH_INITIALIZE_A0), R(NBA97_MATCH_INITIALIZE_T0 + 1));
    R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_V1), 4);
    R(NBA97_MATCH_INITIALIZE_V0) =
        subtract(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
    R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_V0), 2);
    R(NBA97_MATCH_INITIALIZE_V0) =
        add(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
    R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_V0), 2);
    R(NBA97_MATCH_INITIALIZE_V0) =
        add(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_T0 + 2));
    TRY(store_register(run, NBA97_MATCH_INITIALIZE_V0,
                       NBA97_MATCH_INITIALIZE_A0, 0, 4, 4, 0x80065714));
    TRY(load_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_T0,
                      0x14, 2, 2, 0x80065718));
    TRY(load_register(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_A1,
                      0x14, 2, 2, 0x8006571c));
    R(NBA97_MATCH_INITIALIZE_V0) =
        add(R(NBA97_MATCH_INITIALIZE_A2), R(NBA97_MATCH_INITIALIZE_V0));
    R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_V0), 2);
    R(NBA97_MATCH_INITIALIZE_V0) =
        add(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_T0 + 1));
    TRY(load_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_V0,
                      0, 4, 4, 0x8006572c));
    R(NBA97_MATCH_INITIALIZE_A3) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0xfffffffe));
    R(NBA97_MATCH_INITIALIZE_V1) =
        add(R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_A2));
    R(NBA97_MATCH_INITIALIZE_A2) =
        add_constant(R(NBA97_MATCH_INITIALIZE_A2), UINT32_MAX);
    TRY(store_register(run, NBA97_MATCH_INITIALIZE_V1,
                       NBA97_MATCH_INITIALIZE_V0, 0xd6, 2, 2, 0x80065740));
    ++out->actor_iterations;
    TRY(nonnegative(run, R(NBA97_MATCH_INITIALIZE_A2), 0x8006573c, &branch));
    if (!branch)
      break;
  }

  /* 0x80065744..0x800657A0: publish opponent/table links, direction, and
   * source defaults. The second table store is the BEQ delay slot. */
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_A1, NBA97_MATCH_INITIALIZE_T0,
                     4, 4, 4, 0x80065744));
  R(NBA97_MATCH_INITIALIZE_V0) =
      shift_left(R(NBA97_MATCH_INITIALIZE_T0 + 4), 2);
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
  R(NBA97_MATCH_INITIALIZE_AT) =
      add(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(load_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_AT,
                    0xbec, 4, 4, 0x80065754));
  TRY(load_register(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_T0,
                    0x14, 2, 2, 0x80065758));
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_T0,
                     8, 4, 4, 0x8006575c));
  R(NBA97_MATCH_INITIALIZE_V0) =
      shift_left(R(NBA97_MATCH_INITIALIZE_T0 + 5), 2);
  known(&R(NBA97_MATCH_INITIALIZE_AT), UINT32_C(0x80020000));
  R(NBA97_MATCH_INITIALIZE_AT) =
      add(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(load_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_AT,
                    0xbec, 4, 4, 0x8006576c));
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_T0,
                     0xc, 4, 4, 0x80065774));
  TRY(equal(run, R(NBA97_MATCH_INITIALIZE_V1), zero, 0x80065770, &branch));
  if (!branch) {
    known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x00010000));
    R(NBA97_MATCH_INITIALIZE_V0).word |= UINT32_C(0x4e00);
  } else {
    known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0xfffe0000));
    R(NBA97_MATCH_INITIALIZE_V0).word |= UINT32_C(0xb200);
  }
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_T0,
                     0x10, 4, 4, 0x8006578c));
  known(&R(NBA97_MATCH_INITIALIZE_V0), 7);
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_T0,
                     0x34, 1, 1, 0x80065794));
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_T0,
                     0x38, 1, 1, 0x80065798));
  known(&R(NBA97_MATCH_INITIALIZE_V0), 5);
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_T0,
                     0x39, 1, 1, 0x800657a0));

  /* 0x800657A4..0x800657F0: derive rank-57 thresholds. The LBU zero
   * extension proves BGEZ, so 0x800657B4/B8 remain unreachable source code. */
  TRY(load_register(run, NBA97_MATCH_INITIALIZE_V1,
                    NBA97_MATCH_INITIALIZE_T0 + 3, 0x57, 1, 1, 0x800657a4));
  R(NBA97_MATCH_INITIALIZE_A0) =
      add_constant(R(NBA97_MATCH_INITIALIZE_V1), 0x1c);
  TRY(nonnegative(run, R(NBA97_MATCH_INITIALIZE_V1), 0x800657ac, &branch));
  if (!branch) {
    /* Unreachable original instructions 0x800657B4 and 0x800657B8. */
    known(&R(NBA97_MATCH_INITIALIZE_V1), 0);
    R(NBA97_MATCH_INITIALIZE_A0) =
        add_constant(R(NBA97_MATCH_INITIALIZE_V1), 0x1c);
  }
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_A0, NBA97_MATCH_INITIALIZE_T0,
                     0x72, 2, 2, 0x800657bc));
  known(&R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80020000));
  TRY(load_register(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_V0,
                    0x1d72, 1, 1, 0x800657c4));
  difficulty_test =
      unsigned_less_constant(R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(2));
  R(NBA97_MATCH_INITIALIZE_V0) = difficulty_test;
  R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_V1), 1);
  TRY(branch_word(run, difficulty_test, 0x800657d0, &branch));
  if (!branch) {
    R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_A0), 16);
    R(NBA97_MATCH_INITIALIZE_V0) =
        shift_right_arithmetic(R(NBA97_MATCH_INITIALIZE_V0), 17);
    TRY(store_register(run, NBA97_MATCH_INITIALIZE_V0,
                       NBA97_MATCH_INITIALIZE_T0, 0x72, 2, 2, 0x800657e0));
    R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_V1), 1);
  }
  known(&R(NBA97_MATCH_INITIALIZE_V1), 0x78);
  R(NBA97_MATCH_INITIALIZE_V1) =
      subtract(R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_T0,
                     0x62, 2, 2, 0x800657f0));

  /* 0x800657F4..0x8006581C: derive the rank-54 threshold, restore sp, and
   * validate the indirect return after its NOP delay slot. */
  TRY(load_register(run, NBA97_MATCH_INITIALIZE_V0,
                    NBA97_MATCH_INITIALIZE_T0 + 3, 0x54, 1, 1, 0x800657f4));
  R(NBA97_MATCH_INITIALIZE_V1) = shift_left(R(NBA97_MATCH_INITIALIZE_V0), 5);
  known(&R(NBA97_MATCH_INITIALIZE_V0), 0x4ec);
  TRY(nonnegative(run, R(NBA97_MATCH_INITIALIZE_V1), 0x80065800, &branch));
  if (!branch) {
    /* Unreachable original instruction 0x80065808. */
    known(&R(NBA97_MATCH_INITIALIZE_V1), 0);
  }
  R(NBA97_MATCH_INITIALIZE_V1) =
      subtract(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
  TRY(store_register(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_T0,
                     0x74, 2, 2, 0x80065810));
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x10);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15) {
    stop(run, 0x80065818, R(NBA97_MATCH_INITIALIZE_RA).word);
    return NBA97_TEXT_UNKNOWN;
  }
  if (R(NBA97_MATCH_INITIALIZE_RA).word & 3u) {
    stop(run, 0x80065818, R(NBA97_MATCH_INITIALIZE_RA).word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  out->completed = 1;
  stop(run, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
