#include "game_substitution_candidate_select.h"

#include <string.h>

typedef struct Run {
  Nba97GameSubstitutionCandidateSelectContext *context;
  Nba97GameSubstitutionCandidateSelectProgress *progress;
  Nba97GameSubstitutionCandidateSelectMachine machine;
} Run;

#define R(i) (run->machine.registers.gpr[(i)])
#define TRY(x)                                                                 \
  do {                                                                         \
    int status_ = (x);                                                         \
    if (status_ != NBA97_TEXT_COMPLETE)                                        \
      return status_;                                                          \
  } while (0)

static void publish(Run *run) { run->progress->machine = run->machine; }
static void stop(Run *run, uint32_t pc, uint32_t address, uint32_t entry) {
  run->progress->stopped_pc = pc;
  run->progress->stopped_address = address;
  run->progress->stopped_entry = entry;
  publish(run);
}
static void set_known(Nba97GameSubstitutionCandidateSelectWord *word,
                      uint32_t value) {
  word->word = value;
  word->known_mask = 15u;
}
static Nba97GameSubstitutionCandidateSelectWord known_word(uint32_t value) {
  Nba97GameSubstitutionCandidateSelectWord word;
  set_known(&word, value);
  return word;
}
static int
machine_valid(const Nba97GameSubstitutionCandidateSelectMachine *machine) {
  unsigned i;
  if (machine->registers.gpr[0].word != 0u ||
      machine->registers.gpr[0].known_mask != 15u ||
      machine->hi.known_mask > 15u || machine->lo.known_mask > 15u)
    return 0;
  for (i = 0u; i != 32u; ++i)
    if (machine->registers.gpr[i].known_mask > 15u)
      return 0;
  return 1;
}
static int memory_valid(const Nba97GameTextMemory *memory) {
  size_t i, j;
  if (memory->count != 0u && memory->region == NULL)
    return 0;
  for (i = 0u; i != memory->count; ++i) {
    const Nba97GameTextRegion *a = &memory->region[i];
    if (a->data == NULL || a->size == 0u || a->size > UINT64_C(0x100000000) ||
        (uint64_t)a->base + a->size > UINT64_C(0x100000000))
      return 0;
    for (j = 0u; j != i; ++j) {
      const Nba97GameTextRegion *b = &memory->region[j];
      if ((uint64_t)a->base < (uint64_t)b->base + b->size &&
          (uint64_t)b->base < (uint64_t)a->base + a->size)
        return 0;
    }
  }
  return 1;
}
static int initialize(Nba97GameSubstitutionCandidateSelectContext *context,
                      Nba97GameSubstitutionCandidateSelectProgress *progress,
                      Run *run) {
  if (progress == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof(*progress));
  if (context == NULL || !machine_valid(&context->machine) ||
      !memory_valid(&context->memory) ||
      (context->access_journal_capacity != 0u &&
       context->access_journal == NULL))
    return NBA97_TEXT_ARGUMENT;
  run->context = context;
  run->progress = progress;
  run->machine = context->machine;
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static Nba97GameSubstitutionCandidateSelectWord
add_words(Nba97GameSubstitutionCandidateSelectWord left,
          Nba97GameSubstitutionCandidateSelectWord right) {
  Nba97GameSubstitutionCandidateSelectWord result;
  unsigned carry_mask = 1u, byte;
  result.word = left.word + right.word;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned next_mask = 0u, first_output = 0u, first = 1u, invariant = 1u;
    unsigned ls = (left.known_mask & (1u << byte))
                      ? (left.word >> (byte * 8u)) & 255u
                      : 0u;
    unsigned le = (left.known_mask & (1u << byte)) ? ls : 255u;
    unsigned rs = (right.known_mask & (1u << byte))
                      ? (right.word >> (byte * 8u)) & 255u
                      : 0u;
    unsigned re = (right.known_mask & (1u << byte)) ? rs : 255u;
    unsigned carry;
    for (carry = 0u; carry != 2u; ++carry) {
      unsigned a;
      if ((carry_mask & (1u << carry)) == 0u)
        continue;
      for (a = ls; a <= le; ++a) {
        unsigned b;
        for (b = rs; b <= re; ++b) {
          unsigned sum = a + b + carry;
          next_mask |= 1u << (sum >> 8u);
          if (first) {
            first_output = sum & 255u;
            first = 0u;
          } else if (first_output != (sum & 255u)) {
            invariant = 0u;
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
static Nba97GameSubstitutionCandidateSelectWord
add_constant(Nba97GameSubstitutionCandidateSelectWord value,
             uint32_t constant) {
  return add_words(value, known_word(constant));
}
static Nba97GameSubstitutionCandidateSelectWord
shift_left(Nba97GameSubstitutionCandidateSelectWord value, unsigned shift) {
  Nba97GameSubstitutionCandidateSelectWord result;
  unsigned byte, bit;
  result.word = value.word << shift;
  result.known_mask = 0u;
  for (byte = 0u; byte != 4u; ++byte) {
    unsigned yes = 1u;
    for (bit = byte * 8u; bit != byte * 8u + 8u; ++bit)
      if (bit >= shift &&
          (value.known_mask & (1u << ((bit - shift) / 8u))) == 0u)
        yes = 0u;
    if (yes)
      result.known_mask = (uint8_t)(result.known_mask | (1u << byte));
  }
  return result;
}
static Nba97GameSubstitutionCandidateSelectWord
extend(Nba97GameSubstitutionCandidateSelectWord value, unsigned width,
       int signed_value) {
  if (width == 1u && signed_value) {
    value.word =
        (value.word & 255u) | ((value.word & 128u) ? UINT32_C(0xffffff00) : 0u);
    if ((value.known_mask & 1u) != 0u)
      value.known_mask = 15u;
  } else if (width == 2u && signed_value) {
    value.word = (value.word & UINT32_C(0xffff)) |
                 ((value.word & UINT32_C(0x8000)) ? UINT32_C(0xffff0000) : 0u);
    if ((value.known_mask & 2u) != 0u)
      value.known_mask = (uint8_t)(value.known_mask | 12u);
  } else {
    value.known_mask =
        (uint8_t)(value.known_mask | (15u ^ ((1u << width) - 1u)));
  }
  return value;
}
static int spend(Run *run) {
  if (run->progress->operations >= run->context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++run->progress->operations;
  return NBA97_TEXT_COMPLETE;
}
static void journal(Run *run, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width,
                    Nba97GameSubstitutionCandidateSelectWord value) {
  size_t index = run->progress->access_events++;
  if (index < run->context->access_journal_capacity) {
    Nba97GameSubstitutionCandidateSelectAccess *event =
        &run->context->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word;
    event->operation = run->progress->operations;
    event->width = (uint8_t)width;
    event->known_mask = (uint8_t)(value.known_mask & ((1u << width) - 1u));
    event->kind = kind;
  }
}
static int locate(Run *run, uint32_t pc, uint32_t address, unsigned width,
                  uint8_t **data, uint8_t **known) {
  size_t i, b;
  stop(run, pc, address, 0u);
  TRY(spend(run));
  ++run->progress->accesses;
  if ((address & (width - 1u)) != 0u)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0u; i != run->context->memory.count; ++i) {
    Nba97GameTextRegion *region = &run->context->memory.region[i];
    uint64_t offset = (uint64_t)address - region->base;
    if (address < region->base || offset > region->size ||
        width > region->size - (size_t)offset)
      continue;
    *data = region->data + (size_t)offset;
    *known = region->known ? region->known + (size_t)offset : NULL;
    if (*known)
      for (b = 0u; b != width; ++b)
        if ((*known)[b] > 1u)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}
static int address(Run *run, Nba97GameSubstitutionCandidateSelectWord base,
                   uint32_t offset, uint32_t pc, uint32_t *out) {
  Nba97GameSubstitutionCandidateSelectWord value = add_constant(base, offset);
  if (value.known_mask != 15u) {
    stop(run, pc, value.word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *out = value.word;
  return NBA97_TEXT_COMPLETE;
}
static int load(Run *run, unsigned destination, unsigned base, uint32_t offset,
                unsigned width, int signed_value, uint32_t pc) {
  Nba97GameSubstitutionCandidateSelectWord value = {0u, 0u};
  uint32_t guest;
  uint8_t *data, *known;
  unsigned b;
  TRY(address(run, R(base), offset, pc, &guest));
  TRY(locate(run, pc, guest, width, &data, &known));
  for (b = 0u; b != width; ++b) {
    value.word |= (uint32_t)data[b] << (b * 8u);
    if (!known || known[b])
      value.known_mask = (uint8_t)(value.known_mask | (1u << b));
  }
  value = extend(value, width, signed_value);
  R(destination) = value;
  ++run->progress->reads;
  journal(run, NBA97_GAME_MATCH_STATE_RESET_READ, pc, guest, width, value);
  publish(run);
  return NBA97_TEXT_COMPLETE;
}
static int store(Run *run, unsigned source, unsigned base, uint32_t offset,
                 unsigned width, uint32_t pc) {
  uint32_t guest;
  uint8_t *data, *known;
  unsigned b;
  uint8_t required = (uint8_t)((1u << width) - 1u);
  TRY(address(run, R(base), offset, pc, &guest));
  TRY(locate(run, pc, guest, width, &data, &known));
  if (!known && (R(source).known_mask & required) != required)
    return NBA97_TEXT_ARGUMENT;
  for (b = 0u; b != width; ++b) {
    data[b] = (uint8_t)(R(source).word >> (b * 8u));
    if (known)
      known[b] = (uint8_t)((R(source).known_mask >> b) & 1u);
  }
  ++run->progress->stores;
  journal(run, NBA97_GAME_MATCH_STATE_RESET_STORE, pc, guest, width, R(source));
  publish(run);
  return NBA97_TEXT_COMPLETE;
}

static void bounds(Nba97GameSubstitutionCandidateSelectWord value,
                   int signed_value, uint32_t *low, uint32_t *high) {
  unsigned b;
  if (signed_value)
    value.word ^= UINT32_C(0x80000000);
  *low = value.word;
  *high = value.word;
  for (b = 0u; b != 4u; ++b)
    if ((value.known_mask & (1u << b)) == 0u) {
      *low &= ~(UINT32_C(0xff) << (b * 8u));
      *high |= UINT32_C(0xff) << (b * 8u);
    }
}
static Nba97GameSubstitutionCandidateSelectWord
signed_less_constant(Nba97GameSubstitutionCandidateSelectWord value,
                     int32_t constant) {
  Nba97GameSubstitutionCandidateSelectWord result;
  uint32_t low, high;
  uint32_t compare = (uint32_t)constant ^ UINT32_C(0x80000000);
  bounds(value, 1, &low, &high);
  result.word = (value.word ^ UINT32_C(0x80000000)) < compare;
  result.known_mask = (uint8_t)((high < compare || low >= compare) ? 15u : 14u);
  return result;
}
static Nba97GameSubstitutionCandidateSelectWord
signed_less_words(Nba97GameSubstitutionCandidateSelectWord left,
                  Nba97GameSubstitutionCandidateSelectWord right) {
  Nba97GameSubstitutionCandidateSelectWord result;
  uint32_t ll, lh, rl, rh;
  bounds(left, 1, &ll, &lh);
  bounds(right, 1, &rl, &rh);
  result.word =
      (left.word ^ UINT32_C(0x80000000)) < (right.word ^ UINT32_C(0x80000000));
  result.known_mask = (uint8_t)((lh < rl || ll >= rh) ? 15u : 14u);
  return result;
}
static int equal(Run *run, Nba97GameSubstitutionCandidateSelectWord left,
                 Nba97GameSubstitutionCandidateSelectWord right, uint32_t pc,
                 int *yes) {
  unsigned b;
  for (b = 0u; b != 4u; ++b)
    if ((left.known_mask & right.known_mask & (1u << b)) &&
        (((left.word ^ right.word) >> (b * 8u)) & 255u)) {
      *yes = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (left.known_mask == 15u && right.known_mask == 15u) {
    *yes = left.word == right.word;
    return NBA97_TEXT_COMPLETE;
  }
  stop(run, pc, 0u, 0u);
  return NBA97_TEXT_UNKNOWN;
}
static int branch_word(Run *run,
                       Nba97GameSubstitutionCandidateSelectWord predicate,
                       uint32_t pc, int *yes) {
  return equal(run, predicate, known_word(0u), pc, yes) == NBA97_TEXT_COMPLETE
             ? (*yes = !*yes, NBA97_TEXT_COMPLETE)
             : NBA97_TEXT_UNKNOWN;
}
static int nonnegative(Run *run, Nba97GameSubstitutionCandidateSelectWord value,
                       uint32_t pc, int *yes) {
  if ((value.known_mask & 8u) == 0u) {
    stop(run, pc, 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  *yes = (value.word & UINT32_C(0x80000000)) == 0u;
  return NBA97_TEXT_COMPLETE;
}
static int invoke(Run *run) {
  Nba97GameSubstitutionCandidateSelectEvent event;
  int accepted;
  stop(run, UINT32_C(0x80065038), 0u, UINT32_C(0x800649d8));
  TRY(spend(run));
  ++run->progress
        ->call_attempts[NBA97_GAME_SUBSTITUTION_CANDIDATE_SELECT_800649D8];
  memset(&event, 0, sizeof(event));
  event.pc = UINT32_C(0x80065038);
  event.delay_slot_pc = UINT32_C(0x8006503c);
  event.entry = UINT32_C(0x800649d8);
  event.operation = run->progress->operations;
  event.invocation = 1u;
  event.kind = NBA97_GAME_SUBSTITUTION_CANDIDATE_SELECT_800649D8;
  event.argument_count = 5u;
  publish(run);
  if (!run->context->io)
    return NBA97_TEXT_IO_REFUSED;
  accepted = run->context->io(run->context->user, &run->context->memory, &event,
                              &run->machine);
  publish(run);
  if (accepted != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!machine_valid(&run->machine))
    return NBA97_TEXT_ARGUMENT;
  ++run->progress->callbacks_completed;
  ++run->progress
        ->call_count[NBA97_GAME_SUBSTITUTION_CANDIDATE_SELECT_800649D8];
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_substitution_candidate_select(
    Nba97GameSubstitutionCandidateSelectContext *context,
    Nba97GameSubstitutionCandidateSelectProgress *progress) {
  Run storage;
  Run *run = &storage;
  int yes, again;
  TRY(initialize(context, progress, run));

  /* 0x80064DBC..0x80064DE4: retain the fifth argument, signed injury value,
   * side-specific status base, and exact live frame prefix. */
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), UINT32_C(0xffffffb8));
  progress->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  TRY(store(run, NBA97_MATCH_INITIALIZE_RA, NBA97_MATCH_INITIALIZE_SP, 0x40u,
            4u, UINT32_C(0x80064dc0)));
  TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_A0, 0x14u, 2u,
           0, UINT32_C(0x80064dc4)));
  set_known(&R(NBA97_MATCH_INITIALIZE_T0 + 3), UINT32_C(0x80020000));
  R(NBA97_MATCH_INITIALIZE_T0 + 3) =
      add_constant(R(NBA97_MATCH_INITIALIZE_T0 + 3), UINT32_C(0xfffff7ec));
  R(NBA97_MATCH_INITIALIZE_T0 + 5) = R(NBA97_MATCH_INITIALIZE_A3);
  TRY(load(run, NBA97_MATCH_INITIALIZE_A3, NBA97_MATCH_INITIALIZE_A2, 0x20u, 2u,
           1, UINT32_C(0x80064dd4)));
  TRY(equal(run, R(NBA97_MATCH_INITIALIZE_V0), known_word(0u),
            UINT32_C(0x80064dd8), &yes));
  if (!yes)
    R(NBA97_MATCH_INITIALIZE_T0 + 3) =
        add_constant(R(NBA97_MATCH_INITIALIZE_T0 + 3), 0x198u);

  /* 0x80064DE4..0x80064E58: first pass matches the full a1 identifier and
   * uses the first count as a fixed t2 limit. */
  TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_A0, 0x68u, 2u,
           0, UINT32_C(0x80064de4)));
  R(NBA97_MATCH_INITIALIZE_T0) = R(NBA97_MATCH_INITIALIZE_T0 + 3);
  set_known(&R(NBA97_MATCH_INITIALIZE_A2), 0u);
  {
    Nba97GameSubstitutionCandidateSelectWord predicate =
        signed_less_constant(R(NBA97_MATCH_INITIALIZE_V0), 1);
    TRY(branch_word(run, predicate, UINT32_C(0x80064dec), &yes));
  }
  if (!yes) {
    R(NBA97_MATCH_INITIALIZE_T0 + 2) = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_T0 + 1) = R(NBA97_MATCH_INITIALIZE_A0);
    do {
      TRY(load(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_T0 + 1,
               0x80u, 2u, 1, UINT32_C(0x80064dfc)));
      R(NBA97_MATCH_INITIALIZE_V0) =
          signed_less_constant(R(NBA97_MATCH_INITIALIZE_V1), 5);
      {
        Nba97GameSubstitutionCandidateSelectWord predicate =
            R(NBA97_MATCH_INITIALIZE_V0);
        R(NBA97_MATCH_INITIALIZE_V1) =
            shift_left(R(NBA97_MATCH_INITIALIZE_A2), 2u);
        TRY(branch_word(run, predicate, UINT32_C(0x80064e08), &yes));
      }
      if (!yes) {
        TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_A0,
                 0x7cu, 4u, 0, UINT32_C(0x80064e10)));
        R(NBA97_MATCH_INITIALIZE_V1) = add_words(R(NBA97_MATCH_INITIALIZE_V1),
                                                 R(NBA97_MATCH_INITIALIZE_V0));
        TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_V1, 0u,
                 4u, 0, UINT32_C(0x80064e1c)));
        TRY(load(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_V0, 8u,
                 1u, 0, UINT32_C(0x80064e24)));
        TRY(equal(run, R(NBA97_MATCH_INITIALIZE_V1),
                  R(NBA97_MATCH_INITIALIZE_A1), UINT32_C(0x80064e2c), &yes));
        if (yes) {
          TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_T0,
                   0x20u, 2u, 1, UINT32_C(0x80064e34)));
          R(NBA97_MATCH_INITIALIZE_V0) =
              signed_less_constant(R(NBA97_MATCH_INITIALIZE_V0), 0x7332);
          {
            Nba97GameSubstitutionCandidateSelectWord predicate =
                R(NBA97_MATCH_INITIALIZE_V0);
            R(NBA97_MATCH_INITIALIZE_V0) =
                shift_left(R(NBA97_MATCH_INITIALIZE_A2), 1u);
            TRY(branch_word(run, predicate, UINT32_C(0x80064e40), &yes));
          }
          if (!yes)
            goto hit;
        }
      }
      R(NBA97_MATCH_INITIALIZE_T0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_T0), 0x22u);
      R(NBA97_MATCH_INITIALIZE_A2) =
          add_constant(R(NBA97_MATCH_INITIALIZE_A2), 1u);
      R(NBA97_MATCH_INITIALIZE_V0) = signed_less_words(
          R(NBA97_MATCH_INITIALIZE_A2), R(NBA97_MATCH_INITIALIZE_T0 + 2));
      {
        Nba97GameSubstitutionCandidateSelectWord predicate =
            R(NBA97_MATCH_INITIALIZE_V0);
        R(NBA97_MATCH_INITIALIZE_T0 + 1) =
            add_constant(R(NBA97_MATCH_INITIALIZE_T0 + 1), 2u);
        TRY(branch_word(run, predicate, UINT32_C(0x80064e54), &again));
      }
    } while (again);
  }

  /* 0x80064E5C..0x80064F88: passes two and three match signed rank-table
   * bytes. Each pass keeps its source count and delay-slot pointer shifts. */
  TRY(load(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_A0, 0x68u, 2u,
           0, UINT32_C(0x80064e5c)));
  {
    const uint32_t table_bases[2] = {UINT32_C(0x800b8904),
                                     UINT32_C(0x800b8910)};
    const uint32_t start_pcs[2] = {UINT32_C(0x80064e6c), UINT32_C(0x80064f04)};
    const uint32_t load_inverse_pcs[2] = {UINT32_C(0x80064e80),
                                          UINT32_C(0x80064f18)};
    const uint32_t inverse_branch_pcs[2] = {UINT32_C(0x80064e8c),
                                            UINT32_C(0x80064f24)};
    const uint32_t pointer_pcs[2] = {UINT32_C(0x80064e94),
                                     UINT32_C(0x80064f2c)};
    const uint32_t player_ptr_pcs[2] = {UINT32_C(0x80064ea0),
                                        UINT32_C(0x80064f38)};
    const uint32_t player_pcs[2] = {UINT32_C(0x80064ea8), UINT32_C(0x80064f40)};
    const uint32_t rank0_pcs[2] = {UINT32_C(0x80064eac), UINT32_C(0x80064f44)};
    const uint32_t eq0_pcs[2] = {UINT32_C(0x80064eb4), UINT32_C(0x80064f4c)};
    const uint32_t rank5_pcs[2] = {UINT32_C(0x80064ebc), UINT32_C(0x80064f54)};
    const uint32_t eq5_pcs[2] = {UINT32_C(0x80064ec4), UINT32_C(0x80064f5c)};
    const uint32_t status_pcs[2] = {UINT32_C(0x80064ecc), UINT32_C(0x80064f64)};
    const uint32_t status_branch_pcs[2] = {UINT32_C(0x80064ed8),
                                           UINT32_C(0x80064f70)};
    const uint32_t loop_pcs[2] = {UINT32_C(0x80064eec), UINT32_C(0x80064f84)};
    unsigned pass;
    for (pass = 0u; pass != 2u; ++pass) {
      Nba97GameSubstitutionCandidateSelectWord count =
          R(NBA97_MATCH_INITIALIZE_V1);
      set_known(&R(NBA97_MATCH_INITIALIZE_V0), table_bases[pass]);
      R(NBA97_MATCH_INITIALIZE_T0) = R(NBA97_MATCH_INITIALIZE_T0 + 3);
      set_known(&R(NBA97_MATCH_INITIALIZE_A2), 0u);
      {
        Nba97GameSubstitutionCandidateSelectWord predicate =
            signed_less_constant(count, 1);
        TRY(branch_word(run, predicate, start_pcs[pass], &yes));
      }
      if (!yes) {
        R(NBA97_MATCH_INITIALIZE_T0 + 2) = add_words(
            R(NBA97_MATCH_INITIALIZE_A1), R(NBA97_MATCH_INITIALIZE_V0));
        R(NBA97_MATCH_INITIALIZE_T0 + 4) = count;
        R(NBA97_MATCH_INITIALIZE_T0 + 1) = R(NBA97_MATCH_INITIALIZE_A0);
        do {
          TRY(load(run, NBA97_MATCH_INITIALIZE_V1,
                   NBA97_MATCH_INITIALIZE_T0 + 1, 0x80u, 2u, 1,
                   load_inverse_pcs[pass]));
          R(NBA97_MATCH_INITIALIZE_V0) =
              signed_less_constant(R(NBA97_MATCH_INITIALIZE_V1), 5);
          R(NBA97_MATCH_INITIALIZE_V0) =
              shift_left(R(NBA97_MATCH_INITIALIZE_A2), 2u);
          {
            Nba97GameSubstitutionCandidateSelectWord inverse_predicate =
                signed_less_constant(R(NBA97_MATCH_INITIALIZE_V1), 5);
            TRY(branch_word(run, inverse_predicate, inverse_branch_pcs[pass],
                            &yes));
          }
          if (!yes) {
            TRY(load(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_A0,
                     0x7cu, 4u, 0, pointer_pcs[pass]));
            R(NBA97_MATCH_INITIALIZE_V0) = add_words(
                R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
            TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_V0,
                     0u, 4u, 0, player_ptr_pcs[pass]));
            TRY(load(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_V0,
                     8u, 1u, 0, player_pcs[pass]));
            TRY(load(run, NBA97_MATCH_INITIALIZE_V0,
                     NBA97_MATCH_INITIALIZE_T0 + 2, 0u, 1u, 1,
                     rank0_pcs[pass]));
            TRY(equal(run, R(NBA97_MATCH_INITIALIZE_V1),
                      R(NBA97_MATCH_INITIALIZE_V0), eq0_pcs[pass], &yes));
            if (!yes) {
              TRY(load(run, NBA97_MATCH_INITIALIZE_V0,
                       NBA97_MATCH_INITIALIZE_T0 + 2, 5u, 1u, 1,
                       rank5_pcs[pass]));
              TRY(equal(run, R(NBA97_MATCH_INITIALIZE_V1),
                        R(NBA97_MATCH_INITIALIZE_V0), eq5_pcs[pass], &yes));
            }
            if (yes) {
              TRY(load(run, NBA97_MATCH_INITIALIZE_V0,
                       NBA97_MATCH_INITIALIZE_T0, 0x20u, 2u, 1,
                       status_pcs[pass]));
              R(NBA97_MATCH_INITIALIZE_V0) =
                  signed_less_constant(R(NBA97_MATCH_INITIALIZE_V0), 0x7332);
              {
                Nba97GameSubstitutionCandidateSelectWord predicate =
                    R(NBA97_MATCH_INITIALIZE_V0);
                R(NBA97_MATCH_INITIALIZE_V0) =
                    shift_left(R(NBA97_MATCH_INITIALIZE_A2), 1u);
                TRY(branch_word(run, predicate, status_branch_pcs[pass], &yes));
              }
              if (!yes)
                goto hit;
            }
          }
          R(NBA97_MATCH_INITIALIZE_T0) =
              add_constant(R(NBA97_MATCH_INITIALIZE_T0), 0x22u);
          R(NBA97_MATCH_INITIALIZE_A2) =
              add_constant(R(NBA97_MATCH_INITIALIZE_A2), 1u);
          R(NBA97_MATCH_INITIALIZE_V0) = signed_less_words(
              R(NBA97_MATCH_INITIALIZE_A2), R(NBA97_MATCH_INITIALIZE_T0 + 4));
          {
            Nba97GameSubstitutionCandidateSelectWord predicate =
                R(NBA97_MATCH_INITIALIZE_V0);
            R(NBA97_MATCH_INITIALIZE_T0 + 1) =
                add_constant(R(NBA97_MATCH_INITIALIZE_T0 + 1), 2u);
            TRY(branch_word(run, predicate, loop_pcs[pass], &again));
          }
        } while (again);
        if (pass == 0u)
          TRY(load(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_A0,
                   0x68u, 2u, 0, UINT32_C(0x80064ef4)));
      }
    }
  }

  /* 0x80064F8C..0x80064FEC: injury below 0x2000 permits the fourth scan,
   * which ignores player identity but retains inverse and status thresholds. */
  R(NBA97_MATCH_INITIALIZE_V0) =
      signed_less_constant(R(NBA97_MATCH_INITIALIZE_A3), 0x2000);
  {
    Nba97GameSubstitutionCandidateSelectWord predicate =
        R(NBA97_MATCH_INITIALIZE_V0);
    set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0u);
    TRY(branch_word(run, predicate, UINT32_C(0x80064f90), &yes));
  }
  if (!yes)
    goto epilogue;
  TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_A0, 0x68u, 2u,
           0, UINT32_C(0x80064f98)));
  R(NBA97_MATCH_INITIALIZE_T0) = R(NBA97_MATCH_INITIALIZE_T0 + 3);
  set_known(&R(NBA97_MATCH_INITIALIZE_A2), 0u);
  {
    Nba97GameSubstitutionCandidateSelectWord predicate =
        signed_less_constant(R(NBA97_MATCH_INITIALIZE_V0), 1);
    TRY(branch_word(run, predicate, UINT32_C(0x80064fa0), &yes));
  }
  if (!yes) {
    R(NBA97_MATCH_INITIALIZE_T0 + 2) = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_T0 + 1) = R(NBA97_MATCH_INITIALIZE_A0);
    do {
      TRY(load(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_T0 + 1,
               0x80u, 2u, 1, UINT32_C(0x80064fb0)));
      R(NBA97_MATCH_INITIALIZE_V0) =
          signed_less_constant(R(NBA97_MATCH_INITIALIZE_V1), 5);
      TRY(branch_word(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80064fbc),
                      &yes));
      if (!yes) {
        TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_T0,
                 0x20u, 2u, 1, UINT32_C(0x80064fc4)));
        R(NBA97_MATCH_INITIALIZE_V0) =
            signed_less_constant(R(NBA97_MATCH_INITIALIZE_V0), 0x7332);
        {
          Nba97GameSubstitutionCandidateSelectWord predicate =
              R(NBA97_MATCH_INITIALIZE_V0);
          R(NBA97_MATCH_INITIALIZE_V0) =
              shift_left(R(NBA97_MATCH_INITIALIZE_A2), 1u);
          TRY(branch_word(run, predicate, UINT32_C(0x80064fd0), &yes));
        }
        if (!yes)
          goto hit;
      }
      R(NBA97_MATCH_INITIALIZE_T0) =
          add_constant(R(NBA97_MATCH_INITIALIZE_T0), 0x22u);
      R(NBA97_MATCH_INITIALIZE_A2) =
          add_constant(R(NBA97_MATCH_INITIALIZE_A2), 1u);
      R(NBA97_MATCH_INITIALIZE_V0) = signed_less_words(
          R(NBA97_MATCH_INITIALIZE_A2), R(NBA97_MATCH_INITIALIZE_T0 + 2));
      {
        Nba97GameSubstitutionCandidateSelectWord predicate =
            R(NBA97_MATCH_INITIALIZE_V0);
        R(NBA97_MATCH_INITIALIZE_T0 + 1) =
            add_constant(R(NBA97_MATCH_INITIALIZE_T0 + 1), 2u);
        TRY(branch_word(run, predicate, UINT32_C(0x80064fe4), &again));
      }
    } while (again);
  }

  /* 0x80064FEC..0x80065058: only negative injury reaches pass five. Its
   * rejected-candidate tail rereads count before each loop comparison. */
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0u);
  TRY(nonnegative(run, R(NBA97_MATCH_INITIALIZE_A3), UINT32_C(0x80064fec),
                  &yes));
  if (yes)
    goto epilogue;
  TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_A0, 0x68u, 2u,
           0, UINT32_C(0x80064ff4)));
  R(NBA97_MATCH_INITIALIZE_T0) = R(NBA97_MATCH_INITIALIZE_T0 + 3);
  set_known(&R(NBA97_MATCH_INITIALIZE_A2), 0u);
  {
    Nba97GameSubstitutionCandidateSelectWord predicate =
        signed_less_constant(R(NBA97_MATCH_INITIALIZE_V0), 1);
    TRY(branch_word(run, predicate, UINT32_C(0x80064ffc), &yes));
  }
  if (yes)
    goto no_hit;
  do {
    R(NBA97_MATCH_INITIALIZE_V0) = shift_left(R(NBA97_MATCH_INITIALIZE_A2), 1u);
    R(NBA97_MATCH_INITIALIZE_V0) =
        add_words(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_A0));
    TRY(load(run, NBA97_MATCH_INITIALIZE_V1, NBA97_MATCH_INITIALIZE_V0, 0x80u,
             2u, 1, UINT32_C(0x8006500c)));
    R(NBA97_MATCH_INITIALIZE_V0) =
        signed_less_constant(R(NBA97_MATCH_INITIALIZE_V1), 5);
    TRY(branch_word(run, R(NBA97_MATCH_INITIALIZE_V0), UINT32_C(0x80065018),
                    &yes));
    if (!yes) {
      TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_T0, 0x20u,
               2u, 1, UINT32_C(0x80065020)));
      {
        Nba97GameSubstitutionCandidateSelectWord status =
            R(NBA97_MATCH_INITIALIZE_V0);
        R(NBA97_MATCH_INITIALIZE_V0) =
            shift_left(R(NBA97_MATCH_INITIALIZE_A2), 1u);
        TRY(nonnegative(run, status, UINT32_C(0x80065028), &yes));
      }
      if (yes)
        goto hit;
    }
    TRY(load(run, NBA97_MATCH_INITIALIZE_V0, NBA97_MATCH_INITIALIZE_A0, 0x68u,
             2u, 0, UINT32_C(0x80065048)));
    {
      Nba97GameSubstitutionCandidateSelectWord count =
          R(NBA97_MATCH_INITIALIZE_V0);
      R(NBA97_MATCH_INITIALIZE_A2) =
          add_constant(R(NBA97_MATCH_INITIALIZE_A2), 1u);
      R(NBA97_MATCH_INITIALIZE_V0) =
          signed_less_words(R(NBA97_MATCH_INITIALIZE_A2), count);
      {
        Nba97GameSubstitutionCandidateSelectWord predicate =
            R(NBA97_MATCH_INITIALIZE_V0);
        R(NBA97_MATCH_INITIALIZE_T0) =
            add_constant(R(NBA97_MATCH_INITIALIZE_T0), 0x22u);
        TRY(branch_word(run, predicate, UINT32_C(0x80065054), &again));
      }
    }
  } while (again);

no_hit:
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), 0u);
  goto epilogue;

hit:
  /* 0x80065030..0x80065044: all hit branches arrive with v0=2*index. The
   * fifth semantic argument is stored before callback entry in the JAL delay.
   */
  R(NBA97_MATCH_INITIALIZE_V0) =
      add_words(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_A0));
  TRY(load(run, NBA97_MATCH_INITIALIZE_A2, NBA97_MATCH_INITIALIZE_V0, 0x80u, 2u,
           1, UINT32_C(0x80065034)));
  set_known(&R(NBA97_MATCH_INITIALIZE_RA), UINT32_C(0x80065040));
  TRY(store(run, NBA97_MATCH_INITIALIZE_T0 + 5, NBA97_MATCH_INITIALIZE_SP,
            0x10u, 4u, UINT32_C(0x8006503c)));
  TRY(invoke(run));
  set_known(&R(NBA97_MATCH_INITIALIZE_V0), 1u);

epilogue:
  /* 0x80065060..0x8006506C: restore through callback-live sp, release that
   * exact frame, then consume the reloaded ra after the NOP delay. */
  TRY(load(run, NBA97_MATCH_INITIALIZE_RA, NBA97_MATCH_INITIALIZE_SP, 0x40u, 4u,
           0, UINT32_C(0x80065060)));
  progress->restored_return_address = R(NBA97_MATCH_INITIALIZE_RA);
  R(NBA97_MATCH_INITIALIZE_SP) =
      add_constant(R(NBA97_MATCH_INITIALIZE_SP), 0x48u);
  progress->return_v0 = R(NBA97_MATCH_INITIALIZE_V0);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15u) {
    stop(run, UINT32_C(0x80065068), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if ((R(NBA97_MATCH_INITIALIZE_RA).word & 3u) != 0u) {
    stop(run, UINT32_C(0x80065068), R(NBA97_MATCH_INITIALIZE_RA).word, 0u);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  progress->completed = 1u;
  stop(run, 0u, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
