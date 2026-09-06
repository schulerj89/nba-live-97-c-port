#include "game_actor_collision_response.h"

#include <string.h>

typedef Nba97GameActorCollisionResponseWord Word;
typedef struct Run {
  Nba97GameActorCollisionResponseContext *c;
  Nba97GameActorCollisionResponseProgress *p;
  Nba97GameActorCollisionResponseMachine m;
} Run;

#define R(n) (r->m.registers.gpr[(n)])
#define ZERO R(0)
#define AT R(1)
#define V0 R(2)
#define V1 R(3)
#define A0 R(4)
#define A1 R(5)
#define A2 R(6)
#define A3 R(7)
#define T0 R(8)
#define S0 R(16)
#define S1 R(17)
#define S2 R(18)
#define S3 R(19)
#define S4 R(20)
#define S5 R(21)
#define S6 R(22)
#define S7 R(23)
#define SP R(29)
#define S8 R(30)
#define RA R(31)
#define OK(x)                                                                  \
  do {                                                                         \
    int q_ = (x);                                                              \
    if (q_ != NBA97_TEXT_COMPLETE)                                             \
      return q_;                                                               \
  } while (0)
#define STEP(pc_)                                                              \
  do {                                                                         \
    (void)(pc_);                                                               \
    ++p->instruction_count;                                                    \
  } while (0)

static void known(Word *v, uint32_t x) {
  v->word = x;
  v->known_mask = 15;
}
static Word imm(uint32_t x) {
  Word v;
  known(&v, x);
  return v;
}
static void publish(Run *r) {
  r->p->machine = r->m;
  r->p->returned_value = V0;
}
static void stop(Run *r, uint32_t pc, uint32_t address, uint32_t entry) {
  r->p->stopped_pc = pc;
  r->p->stopped_address = address;
  r->p->stopped_entry = entry;
  publish(r);
}
static int valid_machine(const Nba97GameActorCollisionResponseMachine *m) {
  unsigned i;
  if (m->registers.gpr[0].word || m->registers.gpr[0].known_mask != 15 ||
      m->hi.known_mask > 15 || m->lo.known_mask > 15)
    return 0;
  for (i = 0; i < 32; ++i)
    if (m->registers.gpr[i].known_mask > 15)
      return 0;
  return 1;
}
static int spend(Run *r) {
  if (r->p->operations >= r->c->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++r->p->operations;
  return NBA97_TEXT_COMPLETE;
}
static uint32_t width_mask(unsigned width) {
  return width == 4 ? UINT32_MAX : (UINT32_C(1) << (width * 8u)) - UINT32_C(1);
}
static uint8_t knowledge_mask(unsigned width) {
  return (uint8_t)((1u << width) - 1u);
}
static void journal(Run *r, uint8_t kind, uint32_t pc, uint32_t address,
                    unsigned width, Word value) {
  size_t index = r->p->access_events++;
  if (index < r->c->access_journal_capacity) {
    Nba97GameActorCollisionResponseAccess *event = &r->c->access_journal[index];
    event->pc = pc;
    event->address = address;
    event->value = value.word & width_mask(width);
    event->operation = r->p->operations;
    event->width = (uint8_t)width;
    event->known_mask = (uint8_t)(value.known_mask & knowledge_mask(width));
    event->kind = kind;
  }
}
static int locate(Run *r, uint32_t address, unsigned width, uint32_t pc,
                  uint8_t **data, uint8_t **known_bytes) {
  size_t i;
  size_t j;
  stop(r, pc, address, 0);
  OK(spend(r));
  ++r->p->accesses;
  if (address & (width - 1u))
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0; i < r->c->memory.count; ++i) {
    Nba97GameTextRegion *region = &r->c->memory.region[i];
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
static int memread(Run *r, uint32_t address, unsigned width, uint32_t pc,
                   Word *value) {
  uint8_t *data;
  uint8_t *known_bytes;
  unsigned i;
  Word loaded = {0, 0};
  OK(locate(r, address, width, pc, &data, &known_bytes));
  for (i = 0; i < width; ++i) {
    loaded.word |= (uint32_t)data[i] << (i * 8u);
    if (!known_bytes || known_bytes[i])
      loaded.known_mask = (uint8_t)(loaded.known_mask | (1u << i));
  }
  *value = loaded;
  ++r->p->reads;
  journal(r, NBA97_GAME_MATCH_CLOCKS_READ, pc, address, width, loaded);
  publish(r);
  return NBA97_TEXT_COMPLETE;
}
static int memwrite(Run *r, uint32_t address, unsigned width, uint32_t pc,
                    Word value) {
  uint8_t *data;
  uint8_t *known_bytes;
  unsigned i;
  value.word &= width_mask(width);
  value.known_mask &= knowledge_mask(width);
  OK(locate(r, address, width, pc, &data, &known_bytes));
  if (!known_bytes && value.known_mask != knowledge_mask(width))
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < width; ++i) {
    data[i] = (uint8_t)(value.word >> (i * 8u));
    if (known_bytes)
      known_bytes[i] = (uint8_t)((value.known_mask >> i) & 1u);
  }
  ++r->p->stores;
  journal(r, NBA97_GAME_MATCH_CLOCKS_STORE, pc, address, width, value);
  publish(r);
  return NBA97_TEXT_COMPLETE;
}

/* Byte carry and borrow enumeration preserves each provably invariant byte. */
static Word add(Word left, Word right) {
  Word result;
  unsigned carry_mask = 1;
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
                      : 0;
    unsigned le = (left.known_mask & (1u << byte)) ? ls : 255u;
    unsigned rs = (right.known_mask & (1u << byte))
                      ? ((right.word >> (byte * 8u)) & 255u)
                      : 0;
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
static Word sub(Word left, Word right) {
  Word result;
  unsigned borrow_mask = 1;
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
                      : 0;
    unsigned le = (left.known_mask & (1u << byte)) ? ls : 255u;
    unsigned rs = (right.known_mask & (1u << byte))
                      ? ((right.word >> (byte * 8u)) & 255u)
                      : 0;
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
          } else if (output != first_output) {
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
static int address(Run *r, Word base, int32_t offset, uint32_t pc,
                   uint32_t *result) {
  Word value = add(base, imm((uint32_t)offset));
  if (value.known_mask != 15) {
    stop(r, pc, value.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *result = value.word;
  return NBA97_TEXT_COMPLETE;
}
static Word extend(Word value, unsigned width, int sign) {
  Word result;
  value.word &= width_mask(width);
  value.known_mask &= knowledge_mask(width);
  result.word = value.word;
  if (sign && (value.word & (UINT32_C(1) << (width * 8u - 1u))))
    result.word |= ~width_mask(width);
  result.known_mask = value.known_mask;
  if (!sign)
    result.known_mask |= (uint8_t)(15u ^ knowledge_mask(width));
  else if (value.known_mask & (1u << (width - 1u)))
    result.known_mask |= (uint8_t)(15u ^ knowledge_mask(width));
  return result;
}
static int load(Run *r, unsigned rt, unsigned base, int32_t offset,
                unsigned width, int sign, uint32_t pc) {
  uint32_t guest_address;
  Word value;
  OK(address(r, R(base), offset, pc, &guest_address));
  OK(memread(r, guest_address, width, pc, &value));
  R(rt) = extend(value, width, sign);
  return NBA97_TEXT_COMPLETE;
}
static int store(Run *r, unsigned rt, unsigned base, int32_t offset,
                 unsigned width, uint32_t pc) {
  uint32_t guest_address;
  OK(address(r, R(base), offset, pc, &guest_address));
  return memwrite(r, guest_address, width, pc, R(rt));
}
static Word shift(Word value, unsigned amount, int right, int arithmetic) {
  Word result;
  unsigned output;
  result.word = right ? value.word >> amount : value.word << amount;
  if (right && arithmetic && (value.word & UINT32_C(0x80000000)))
    result.word |= UINT32_MAX << (32u - amount);
  result.known_mask = 0;
  for (output = 0; output < 4; ++output) {
    unsigned low = output * 8u;
    unsigned high = low + 7u;
    unsigned source_low;
    unsigned source_high;
    unsigned source;
    int all = 1;
    if (!right) {
      if (high < amount) {
        result.known_mask |= (uint8_t)(1u << output);
        continue;
      }
      source_low = low < amount ? 0 : low - amount;
      source_high = high - amount;
    } else {
      source_low = low + amount;
      source_high = high + amount;
      if (source_low >= 32) {
        if (!arithmetic || (value.known_mask & 8u))
          result.known_mask |= (uint8_t)(1u << output);
        continue;
      }
      if (source_high >= 32) {
        if (arithmetic && !(value.known_mask & 8u))
          continue;
        source_high = 31;
      }
    }
    for (source = source_low / 8u; source <= source_high / 8u; ++source)
      if (!(value.known_mask & (1u << source)))
        all = 0;
    if (all)
      result.known_mask |= (uint8_t)(1u << output);
  }
  return result;
}
static Word bitand_constant(Word value, uint32_t mask) {
  Word result;
  unsigned byte;
  result.word = value.word & mask;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    uint32_t part = (mask >> (byte * 8u)) & 255u;
    if (!part || (value.known_mask & (1u << byte)))
      result.known_mask |= (uint8_t)(1u << byte);
  }
  return result;
}
static int equal(Word left, Word right, int *answer) {
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((left.known_mask & right.known_mask & (1u << byte)) &&
        (((left.word >> (byte * 8u)) & 255u) !=
         ((right.word >> (byte * 8u)) & 255u))) {
      *answer = 0;
      return 1;
    }
  if (left.known_mask == 15 && right.known_mask == 15) {
    *answer = left.word == right.word;
    return 1;
  }
  return 0;
}
static int sign_condition(Word value, int positive, int *answer) {
  if (!(value.known_mask & 8u))
    return 0;
  if (positive) {
    if (value.word & UINT32_C(0x80000000)) {
      *answer = 0;
      return 1;
    }
    if (value.known_mask == 15) {
      *answer = value.word != 0;
      return 1;
    }
    for (unsigned byte = 0; byte < 4; ++byte)
      if ((value.known_mask & (1u << byte)) &&
          ((value.word >> (byte * 8u)) & 255u)) {
        *answer = 1;
        return 1;
      }
    return 0;
  }
  *answer = (value.word & UINT32_C(0x80000000)) != 0;
  return 1;
}
static void signed_bounds(Word value, int64_t *minimum, int64_t *maximum) {
  uint32_t low = 0;
  uint32_t high = 0;
  unsigned byte;
  if (!(value.known_mask & 8u)) {
    *minimum = INT32_MIN;
    *maximum = INT32_MAX;
    return;
  }
  for (byte = 0; byte < 4; ++byte) {
    uint32_t part = (value.word >> (byte * 8u)) & 255u;
    low |= ((value.known_mask & (1u << byte)) ? part : 0u) << (byte * 8u);
    high |= ((value.known_mask & (1u << byte)) ? part : 255u) << (byte * 8u);
  }
  *minimum = (low & UINT32_C(0x80000000)) ? (int64_t)low - INT64_C(0x100000000)
                                          : (int64_t)low;
  *maximum = (high & UINT32_C(0x80000000))
                 ? (int64_t)high - INT64_C(0x100000000)
                 : (int64_t)high;
}
static int32_t signed_word(uint32_t word) {
  return word <= (uint32_t)INT32_MAX ? (int32_t)word
                                     : -1 - (int32_t)(UINT32_MAX - word);
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
  result.known_mask = 14;
  return result;
}
static Word signed_less_constant(Word value, int32_t limit) {
  return signed_less(value, imm((uint32_t)limit));
}
static void multiply(Run *r, Word left, Word right) {
  uint64_t product = (uint64_t)((int64_t)signed_word(left.word) *
                                (int64_t)signed_word(right.word));
  unsigned byte;
  r->m.lo.word = (uint32_t)product;
  r->m.hi.word = (uint32_t)(product >> 32u);
  r->m.lo.known_mask = 0;
  r->m.hi.known_mask = 0;
  if ((left.known_mask == 15 && left.word == 0) ||
      (right.known_mask == 15 && right.word == 0)) {
    r->m.lo.known_mask = 15;
    r->m.hi.known_mask = 15;
  } else if (left.known_mask == 15 && right.known_mask == 15) {
    r->m.lo.known_mask = 15;
    r->m.hi.known_mask = 15;
  } else {
    for (byte = 0; byte < 4; ++byte) {
      uint8_t prefix = (uint8_t)((1u << (byte + 1u)) - 1u);
      if ((left.known_mask & prefix) == prefix &&
          (right.known_mask & prefix) == prefix)
        r->m.lo.known_mask = (uint8_t)(r->m.lo.known_mask | (1u << byte));
    }
  }
  publish(r);
}
static void divide(Run *r, Word numerator, Word denominator) {
  int32_t n = signed_word(numerator.word);
  int32_t d = signed_word(denominator.word);
  int denominator_proven_nonzero = 0;
  unsigned byte;
  for (byte = 0; byte < 4; ++byte)
    if ((denominator.known_mask & (1u << byte)) &&
        ((denominator.word >> (byte * 8u)) & 255u))
      denominator_proven_nonzero = 1;
  if (d == 0) {
    r->m.lo.word = n < 0 ? 1u : UINT32_MAX;
    r->m.hi.word = numerator.word;
  } else if (n == INT32_MIN && d == -1) {
    r->m.lo.word = UINT32_C(0x80000000);
    r->m.hi.word = 0;
  } else {
    r->m.lo.word = (uint32_t)(n / d);
    r->m.hi.word = (uint32_t)(n % d);
  }
  r->m.lo.known_mask = r->m.hi.known_mask = 0;
  if (numerator.known_mask == 15 && numerator.word == 0 &&
      denominator_proven_nonzero) {
    r->m.lo.known_mask = r->m.hi.known_mask = 15;
  } else if (denominator.known_mask == 15 && denominator.word == 1) {
    r->m.lo = numerator;
    known(&r->m.hi, 0);
  } else if (denominator.known_mask == 15 && denominator.word == UINT32_MAX) {
    r->m.lo = sub(ZERO, numerator);
    known(&r->m.hi, 0);
  } else if (numerator.known_mask == 15 && denominator.known_mask == 15) {
    r->m.lo.known_mask = r->m.hi.known_mask = 15;
  }
  publish(r);
}
static uint8_t call_kind(uint32_t entry) {
  switch (entry) {
  case UINT32_C(0x8007066c):
    return NBA97_GAME_ACTOR_COLLISION_RESPONSE_GEOMETRY_8007066C;
  case UINT32_C(0x8005ea28):
    return NBA97_GAME_ACTOR_COLLISION_RESPONSE_RESOLVE_8005EA28;
  case UINT32_C(0x800706e4):
    return NBA97_GAME_ACTOR_COLLISION_RESPONSE_ANGLE_800706E4;
  case UINT32_C(0x80056b78):
    return NBA97_GAME_ACTOR_COLLISION_RESPONSE_ANIMATION_80056B78;
  case UINT32_C(0x80056ce0):
    return NBA97_GAME_ACTOR_COLLISION_RESPONSE_ANIMATION_80056CE0;
  case UINT32_C(0x80056c28):
    return NBA97_GAME_ACTOR_COLLISION_RESPONSE_ANIMATION_80056C28;
  case UINT32_C(0x80056c84):
    return NBA97_GAME_ACTOR_COLLISION_RESPONSE_ANIMATION_80056C84;
  default:
    return 0;
  }
}
static uint8_t call_arguments(uint32_t entry) {
  if (entry == UINT32_C(0x8005ea28))
    return 8;
  if (entry == UINT32_C(0x800706e4) || entry == UINT32_C(0x80056ce0) ||
      entry == UINT32_C(0x80056c28) || entry == UINT32_C(0x80056c84))
    return 3;
  return 2;
}
static int invoke(Run *r, uint32_t pc, uint32_t entry) {
  Nba97GameActorCollisionResponseEvent event;
  uint8_t kind = call_kind(entry);
  int accepted;
  stop(r, pc, 0, entry);
  OK(spend(r));
  memset(&event, 0, sizeof event);
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = entry;
  event.operation = r->p->operations;
  event.invocation = r->p->call_count[kind] + 1u;
  event.kind = kind;
  event.argument_count = call_arguments(entry);
  publish(r);
  if (!r->c->io)
    return NBA97_TEXT_IO_REFUSED;
  accepted = r->c->io(r->c->user, &r->c->memory, &event, &r->m);
  publish(r);
  if (accepted != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!valid_machine(&r->m))
    return NBA97_TEXT_ARGUMENT;
  ++r->p->callbacks_completed;
  ++r->p->call_count[kind];
  return NBA97_TEXT_COMPLETE;
}
static int unknown_branch(Run *r, uint32_t pc) {
  stop(r, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}
static int arithmetic_trap(Run *r, uint32_t pc, uint32_t code) {
  r->p->arithmetic_trap_code = code;
  stop(r, pc, 0, 0);
  return NBA97_GAME_ACTOR_COLLISION_RESPONSE_ARITHMETIC_TRAP;
}

int nba97_game_actor_collision_response(
    Nba97GameActorCollisionResponseContext *c,
    Nba97GameActorCollisionResponseProgress *p) {
  Run run;
  Run *r = &run;
  size_t i;
  size_t j;
  int branch = 0;
  int decided = 0;
  if (!p)
    return NBA97_TEXT_ARGUMENT;
  memset(p, 0, sizeof *p);
  if (!c || (!c->memory.region && c->memory.count) ||
      (!c->access_journal && c->access_journal_capacity) ||
      !valid_machine(&c->machine))
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < c->memory.count; ++i) {
    Nba97GameTextRegion *a = &c->memory.region[i];
    if (!a->data || !a->size || a->size > UINT64_C(0x100000000) ||
        (uint64_t)a->base + a->size > UINT64_C(0x100000000))
      return NBA97_TEXT_ARGUMENT;
    for (j = 0; j < i; ++j) {
      Nba97GameTextRegion *b = &c->memory.region[j];
      if ((uint64_t)a->base < (uint64_t)b->base + b->size &&
          (uint64_t)b->base < (uint64_t)a->base + a->size)
        return NBA97_TEXT_ARGUMENT;
    }
  }
  memset(r, 0, sizeof *r);
  r->c = c;
  r->p = p;
  r->m = c->machine;
  publish(r);

  /* Save the full source frame and capture both actors' relative geometry. */
  STEP(0x8005f3bc);
  SP = add(SP, imm(UINT32_C(0xffffffa8)));
  p->frame_stack_pointer = SP.word;
  STEP(0x8005f3c0);
  OK(store(r, 16, 29, 0x30, 4, UINT32_C(0x8005f3c0)));
  STEP(0x8005f3c4);
  S0 = A0;
  STEP(0x8005f3c8);
  OK(store(r, 17, 29, 0x34, 4, UINT32_C(0x8005f3c8)));
  STEP(0x8005f3cc);
  S1 = A1;
  STEP(0x8005f3d0);
  OK(store(r, 31, 29, 0x54, 4, UINT32_C(0x8005f3d0)));
  STEP(0x8005f3d4);
  OK(store(r, 30, 29, 0x50, 4, UINT32_C(0x8005f3d4)));
  STEP(0x8005f3d8);
  OK(store(r, 23, 29, 0x4c, 4, UINT32_C(0x8005f3d8)));
  STEP(0x8005f3dc);
  OK(store(r, 22, 29, 0x48, 4, UINT32_C(0x8005f3dc)));
  STEP(0x8005f3e0);
  OK(store(r, 21, 29, 0x44, 4, UINT32_C(0x8005f3e0)));
  STEP(0x8005f3e4);
  OK(store(r, 20, 29, 0x40, 4, UINT32_C(0x8005f3e4)));
  STEP(0x8005f3e8);
  OK(store(r, 19, 29, 0x3c, 4, UINT32_C(0x8005f3e8)));
  STEP(0x8005f3ec);
  OK(store(r, 18, 29, 0x38, 4, UINT32_C(0x8005f3ec)));
  STEP(0x8005f3f0);
  OK(load(r, 3, 16, 8, 4, 0, UINT32_C(0x8005f3f0)));
  STEP(0x8005f3f4);
  OK(load(r, 2, 17, 8, 4, 0, UINT32_C(0x8005f3f4)));
  STEP(0x8005f3f8);
  OK(load(r, 8, 17, 0x14, 2, 1, UINT32_C(0x8005f3f8)));
  STEP(0x8005f3fc);
  OK(load(r, 5, 16, 0x0c, 4, 0, UINT32_C(0x8005f3fc)));
  STEP(0x8005f400);
  OK(load(r, 6, 16, 0x14, 2, 1, UINT32_C(0x8005f400)));
  STEP(0x8005f404);
  OK(load(r, 7, 17, 0x16, 2, 1, UINT32_C(0x8005f404)));
  STEP(0x8005f408);
  S6 = sub(V1, V0);
  STEP(0x8005f40c);
  OK(load(r, 2, 17, 0x0c, 4, 0, UINT32_C(0x8005f40c)));
  STEP(0x8005f410);
  A0 = S6;
  STEP(0x8005f414);
  OK(load(r, 3, 16, 0x16, 2, 1, UINT32_C(0x8005f414)));
  STEP(0x8005f418);
  S2 = sub(T0, A2);
  STEP(0x8005f41c);
  S5 = sub(A1, V0);
  STEP(0x8005f420);
  A1 = S5;
  STEP(0x8005f424);
  known(&RA, UINT32_C(0x8005f42c));
  STEP(0x8005f428);
  S3 = sub(A3, V1);
  OK(invoke(r, UINT32_C(0x8005f424), UINT32_C(0x8007066c)));
  STEP(0x8005f42c);
  V1 = V0;

  /* Normalize the relative position. Source DIV guards and BREAKs stay live. */
  STEP(0x8005f430);
  decided = sign_condition(V1, 1, &branch);
  branch = !branch;
  STEP(0x8005f434);
  V0 = shift(S6, 8, 0, 0);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f430));
  if (branch)
    goto reject;
  STEP(0x8005f438);
  divide(r, V0, V1);
  STEP(0x8005f43c);
  decided = equal(V1, ZERO, &branch);
  branch = !branch;
  STEP(0x8005f440);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f43c));
  if (!branch) {
    STEP(0x8005f444);
    return arithmetic_trap(r, UINT32_C(0x8005f444), UINT32_C(0x1c00));
  }
  STEP(0x8005f448);
  known(&AT, UINT32_MAX);
  STEP(0x8005f44c);
  decided = equal(V1, AT, &branch);
  branch = !branch;
  STEP(0x8005f450);
  known(&AT, UINT32_C(0x80000000));
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f44c));
  if (!branch) {
    STEP(0x8005f454);
    decided = equal(V0, AT, &branch);
    branch = !branch;
    STEP(0x8005f458);
    if (!decided)
      return unknown_branch(r, UINT32_C(0x8005f454));
    if (!branch) {
      STEP(0x8005f45c);
      return arithmetic_trap(r, UINT32_C(0x8005f45c), UINT32_C(0x1800));
    }
  }
  STEP(0x8005f460);
  S8 = r->m.lo;
  p->normal_x = S8;
  STEP(0x8005f464);
  V0 = shift(S5, 8, 0, 0);
  STEP(0x8005f468);
  STEP(0x8005f46c);
  divide(r, V0, V1);
  STEP(0x8005f470);
  decided = equal(V1, ZERO, &branch);
  branch = !branch;
  STEP(0x8005f474);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f470));
  if (!branch) {
    STEP(0x8005f478);
    return arithmetic_trap(r, UINT32_C(0x8005f478), UINT32_C(0x1c00));
  }
  STEP(0x8005f47c);
  known(&AT, UINT32_MAX);
  STEP(0x8005f480);
  decided = equal(V1, AT, &branch);
  branch = !branch;
  STEP(0x8005f484);
  known(&AT, UINT32_C(0x80000000));
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f480));
  if (!branch) {
    STEP(0x8005f488);
    decided = equal(V0, AT, &branch);
    branch = !branch;
    STEP(0x8005f48c);
    if (!decided)
      return unknown_branch(r, UINT32_C(0x8005f488));
    if (!branch) {
      STEP(0x8005f490);
      return arithmetic_trap(r, UINT32_C(0x8005f490), UINT32_C(0x1800));
    }
  }
  STEP(0x8005f494);
  S7 = r->m.lo;
  p->normal_y = S7;
  STEP(0x8005f498);
  STEP(0x8005f49c);
  STEP(0x8005f4a0);
  multiply(r, S2, S8);
  STEP(0x8005f4a4);
  V0 = r->m.lo;
  STEP(0x8005f4a8);
  STEP(0x8005f4ac);
  STEP(0x8005f4b0);
  multiply(r, S3, S7);
  STEP(0x8005f4b4);
  V1 = r->m.lo;
  STEP(0x8005f4b8);
  V0 = add(V0, V1);
  STEP(0x8005f4bc);
  S4 = shift(V0, 8, 1, 1);
  p->normal_velocity = S4;
  STEP(0x8005f4c0);
  decided = sign_condition(S4, 1, &branch);
  STEP(0x8005f4c4);
  multiply(r, S2, S7);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f4c0));
  if (!branch)
    goto reject;
  STEP(0x8005f4d0);
  V0 = r->m.lo;
  STEP(0x8005f4d4);
  STEP(0x8005f4d8);
  STEP(0x8005f4dc);
  multiply(r, S3, S8);
  STEP(0x8005f4e0);
  V1 = r->m.lo;
  STEP(0x8005f4e4);
  V0 = sub(V0, V1);
  STEP(0x8005f4e8);
  S2 = shift(V0, 8, 1, 1);
  p->tangent_velocity = S2;

  /* Clamp the tangent/normal response exactly around signed +/-64. */
  STEP(0x8005f4ec);
  decided = sign_condition(S2, 0, &branch);
  STEP(0x8005f4f0);
  V0 = signed_less(S2, S4);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f4ec));
  if (branch)
    goto negative_tangent;
  STEP(0x8005f4f4);
  decided = equal(V0, ZERO, &branch);
  STEP(0x8005f4f8);
  V0 = signed_less_constant(S2, 0x40);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f4f4));
  if (branch)
    goto clamp_normal;
  STEP(0x8005f4fc);
  decided = equal(V0, ZERO, &branch);
  STEP(0x8005f500);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f4fc));
  if (branch)
    goto factors;
  STEP(0x8005f504);
  STEP(0x8005f508);
  known(&S2, 0x40);
  goto factors;

negative_tangent:
  STEP(0x8005f50c);
  V0 = sub(ZERO, S2);
  STEP(0x8005f510);
  V0 = signed_less(V0, S4);
  STEP(0x8005f514);
  decided = equal(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x8005f518);
  V0 = signed_less_constant(S2, -63);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f514));
  if (branch)
    goto negative_min;
clamp_normal:
  STEP(0x8005f51c);
  V0 = signed_less_constant(S4, 0x40);
  STEP(0x8005f520);
  decided = equal(V0, ZERO, &branch);
  STEP(0x8005f524);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f520));
  if (branch)
    goto factors;
  STEP(0x8005f528);
  STEP(0x8005f52c);
  known(&S4, 0x40);
  goto factors;
negative_min:
  STEP(0x8005f530);
  decided = equal(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x8005f534);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f530));
  if (branch)
    goto factors;
  STEP(0x8005f538);
  known(&S2, UINT32_C(0xffffffc0));

factors:
  /* Descriptor material selects signed factors before the eight-argument
   * resolver callback receives the four register and four stack arguments. */
  STEP(0x8005f53c);
  OK(load(r, 2, 16, 0x20, 4, 0, UINT32_C(0x8005f53c)));
  STEP(0x8005f540);
  OK(load(r, 3, 17, 0x20, 4, 0, UINT32_C(0x8005f540)));
  STEP(0x8005f544);
  OK(load(r, 2, 2, 0x0a, 1, 0, UINT32_C(0x8005f544)));
  STEP(0x8005f548);
  OK(load(r, 3, 3, 0x0a, 1, 0, UINT32_C(0x8005f548)));
  STEP(0x8005f54c);
  V0 = shift(V0, 3, 1, 0);
  STEP(0x8005f550);
  V0 = shift(V0, 1, 0, 0);
  STEP(0x8005f554);
  known(&AT, UINT32_C(0x800c0000));
  STEP(0x8005f558);
  AT = add(AT, V0);
  STEP(0x8005f55c);
  OK(load(r, 19, 1, -0x7cdc, 2, 1, UINT32_C(0x8005f55c)));
  STEP(0x8005f560);
  V1 = shift(V1, 3, 1, 0);
  STEP(0x8005f564);
  V1 = shift(V1, 1, 0, 0);
  STEP(0x8005f568);
  known(&AT, UINT32_C(0x800c0000));
  STEP(0x8005f56c);
  AT = add(AT, V1);
  STEP(0x8005f570);
  OK(load(r, 7, 1, -0x7cdc, 2, 1, UINT32_C(0x8005f570)));
  STEP(0x8005f574);
  A0 = S0;
  STEP(0x8005f578);
  A1 = S1;
  STEP(0x8005f57c);
  known(&V0, 5);
  STEP(0x8005f580);
  OK(store(r, 2, 16, 0xe2, 2, UINT32_C(0x8005f580)));
  STEP(0x8005f584);
  OK(store(r, 2, 17, 0xe2, 2, UINT32_C(0x8005f584)));
  STEP(0x8005f588);
  OK(store(r, 22, 29, 0x10, 4, UINT32_C(0x8005f588)));
  STEP(0x8005f58c);
  OK(store(r, 21, 29, 0x14, 4, UINT32_C(0x8005f58c)));
  STEP(0x8005f590);
  OK(store(r, 20, 29, 0x18, 4, UINT32_C(0x8005f590)));
  STEP(0x8005f594);
  OK(store(r, 18, 29, 0x1c, 4, UINT32_C(0x8005f594)));
  STEP(0x8005f598);
  known(&RA, UINT32_C(0x8005f5a0));
  STEP(0x8005f59c);
  A2 = S3;
  OK(invoke(r, UINT32_C(0x8005f598), UINT32_C(0x8005ea28)));

  /* Reload resolver-written factors. Preserve the unsigned-first E2 quirk. */
  STEP(0x8005f5a0);
  OK(load(r, 2, 16, 0xe2, 2, 0, UINT32_C(0x8005f5a0)));
  STEP(0x8005f5a4);
  OK(load(r, 7, 17, 0xe2, 2, 1, UINT32_C(0x8005f5a4)));
  STEP(0x8005f5a8);
  V1 = shift(V0, 16, 0, 0);
  STEP(0x8005f5ac);
  S3 = shift(V1, 16, 1, 1);
  STEP(0x8005f5b0);
  decided = equal(S3, ZERO, &branch);
  branch = !branch;
  STEP(0x8005f5b4);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f5b0));
  if (branch)
    goto second_actor;
  STEP(0x8005f5b8);
  decided = equal(A3, ZERO, &branch);
  branch = !branch;
  STEP(0x8005f5bc);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f5b8));
  if (branch)
    goto second_actor;
  STEP(0x8005f5c0);
  OK(load(r, 2, 16, 0x10, 4, 0, UINT32_C(0x8005f5c0)));
  STEP(0x8005f5c4);
  STEP(0x8005f5c8);
  decided = equal(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x8005f5cc);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f5c8));
  if (branch)
    goto halve_factor;
  STEP(0x8005f5d0);
  OK(load(r, 2, 17, 0x10, 4, 0, UINT32_C(0x8005f5d0)));
  STEP(0x8005f5d4);
  STEP(0x8005f5d8);
  decided = equal(V0, ZERO, &branch);
  STEP(0x8005f5dc);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f5d8));
  if (branch)
    goto second_actor;
halve_factor:
  STEP(0x8005f5e0);
  S3 = shift(V1, 17, 1, 1);
  STEP(0x8005f5e4);
  A3 = S3;

second_actor:
  /* Resolve the second actor first. The branch delay computes its projection
   * even when the E6 or factor gate skips every following store. */
  STEP(0x8005f5e8);
  OK(load(r, 3, 17, 0xe6, 2, 1, UINT32_C(0x8005f5e8)));
  STEP(0x8005f5ec);
  known(&V0, 0x0e);
  STEP(0x8005f5f0);
  decided = equal(V1, V0, &branch);
  STEP(0x8005f5f4);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f5f0));
  if (branch)
    goto first_actor;
  STEP(0x8005f5f8);
  decided = equal(A3, ZERO, &branch);
  STEP(0x8005f5fc);
  multiply(r, S2, S7);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f5f8));
  if (branch)
    goto first_actor;
  STEP(0x8005f600);
  OK(load(r, 3, 16, 0x14, 2, 1, UINT32_C(0x8005f600)));
  STEP(0x8005f604);
  T0 = r->m.lo;
  STEP(0x8005f608);
  V0 = shift(T0, 8, 1, 1);
  STEP(0x8005f60c);
  V0 = add(V0, V1);
  STEP(0x8005f610);
  multiply(r, A3, V0);
  STEP(0x8005f614);
  V1 = r->m.lo;
  STEP(0x8005f618);
  V0 = sub(ZERO, S2);
  STEP(0x8005f61c);
  STEP(0x8005f620);
  multiply(r, V0, S8);
  STEP(0x8005f624);
  V0 = shift(V1, 3, 1, 1);
  STEP(0x8005f628);
  OK(store(r, 2, 17, 0x14, 2, UINT32_C(0x8005f628)));
  STEP(0x8005f62c);
  OK(load(r, 3, 16, 0x16, 2, 1, UINT32_C(0x8005f62c)));
  STEP(0x8005f630);
  A0 = r->m.lo;
  STEP(0x8005f634);
  V0 = shift(A0, 8, 1, 1);
  STEP(0x8005f638);
  V0 = add(V0, V1);
  STEP(0x8005f63c);
  multiply(r, A3, V0);
  STEP(0x8005f640);
  OK(load(r, 3, 17, 0x1a, 1, 0, UINT32_C(0x8005f640)));
  STEP(0x8005f644);
  known(&V0, 0x14);
  STEP(0x8005f648);
  T0 = r->m.lo;
  STEP(0x8005f64c);
  A1 = shift(T0, 3, 1, 1);
  STEP(0x8005f650);
  decided = equal(V1, V0, &branch);
  STEP(0x8005f654);
  OK(store(r, 5, 17, 0x16, 2, UINT32_C(0x8005f654)));
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f650));
  if (!branch) {
    STEP(0x8005f658);
    known(&V0, 0x0e);
    STEP(0x8005f65c);
    STEP(0x8005f660);
    OK(store(r, 2, 17, 0xe6, 2, UINT32_C(0x8005f660)));
    goto first_actor;
  }
  STEP(0x8005f664);
  OK(load(r, 3, 17, 0x46, 2, 0, UINT32_C(0x8005f664)));
  STEP(0x8005f668);
  known(&V0, 0x50);
  STEP(0x8005f66c);
  decided = equal(V1, V0, &branch);
  branch = !branch;
  STEP(0x8005f670);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f66c));
  if (branch)
    goto first_actor;
  STEP(0x8005f674);
  OK(load(r, 2, 17, 0x50, 2, 0, UINT32_C(0x8005f674)));
  STEP(0x8005f678);
  STEP(0x8005f67c);
  decided = equal(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x8005f680);
  A1 = shift(A1, 16, 0, 0);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f67c));
  if (branch)
    goto first_actor;
  STEP(0x8005f684);
  OK(load(r, 4, 17, 0x14, 2, 1, UINT32_C(0x8005f684)));
  STEP(0x8005f688);
  A1 = shift(A1, 16, 1, 1);
  STEP(0x8005f68c);
  known(&RA, UINT32_C(0x8005f694));
  STEP(0x8005f690);
  A2 = add(SP, imm(0x20));
  OK(invoke(r, UINT32_C(0x8005f68c), UINT32_C(0x800706e4)));
  STEP(0x8005f694);
  OK(load(r, 2, 17, 0xa8, 2, 1, UINT32_C(0x8005f694)));
  STEP(0x8005f698);
  OK(load(r, 3, 29, 0x20, 2, 1, UINT32_C(0x8005f698)));
  STEP(0x8005f69c);
  STEP(0x8005f6a0);
  V0 = sub(V0, V1);
  STEP(0x8005f6a4);
  V0 = add(V0, imm(0x100));
  STEP(0x8005f6a8);
  V0 = bitand_constant(V0, 0x3ff);
  STEP(0x8005f6ac);
  V0 = signed_less_constant(V0, 0x201);
  STEP(0x8005f6b0);
  decided = equal(V0, ZERO, &branch);
  STEP(0x8005f6b4);
  A0 = S1;
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f6b0));
  if (branch)
    goto first_actor;
  STEP(0x8005f6b8);
  known(&A1, 0x52);
  STEP(0x8005f6bc);
  known(&V0, UINT32_MAX);
  STEP(0x8005f6c0);
  OK(store(r, 0, 17, 0x64, 2, UINT32_C(0x8005f6c0)));
  STEP(0x8005f6c4);
  OK(store(r, 0, 17, 0x60, 2, UINT32_C(0x8005f6c4)));
  STEP(0x8005f6c8);
  OK(store(r, 2, 17, 0x4c, 2, UINT32_C(0x8005f6c8)));
  STEP(0x8005f6cc);
  known(&RA, UINT32_C(0x8005f6d4));
  STEP(0x8005f6d0);
  OK(store(r, 2, 17, 0x48, 2, UINT32_C(0x8005f6d0)));
  OK(invoke(r, UINT32_C(0x8005f6cc), UINT32_C(0x80056b78)));
  STEP(0x8005f6d4);
  A0 = S1;
  STEP(0x8005f6d8);
  known(&A1, 0x53);
  STEP(0x8005f6dc);
  known(&RA, UINT32_C(0x8005f6e4));
  STEP(0x8005f6e0);
  known(&A2, 0);
  OK(invoke(r, UINT32_C(0x8005f6dc), UINT32_C(0x80056ce0)));
  STEP(0x8005f6e4);
  A0 = S1;
  STEP(0x8005f6e8);
  known(&A1, 0x29);
  STEP(0x8005f6ec);
  known(&RA, UINT32_C(0x8005f6f4));
  STEP(0x8005f6f0);
  known(&A2, 0);
  OK(invoke(r, UINT32_C(0x8005f6ec), UINT32_C(0x80056c28)));
  STEP(0x8005f6f4);
  A0 = S1;
  STEP(0x8005f6f8);
  known(&A1, 0);
  STEP(0x8005f6fc);
  known(&RA, UINT32_C(0x8005f704));
  STEP(0x8005f700);
  known(&A2, 0);
  OK(invoke(r, UINT32_C(0x8005f6fc), UINT32_C(0x80056c84)));

first_actor:
  /* Resolve the first actor from its original velocities, respecting E6, DA,
   * and resolver factor gates before the symmetric animation checks. */
  STEP(0x8005f704);
  OK(load(r, 3, 16, 0xe6, 2, 1, UINT32_C(0x8005f704)));
  STEP(0x8005f708);
  known(&V0, 0x0e);
  STEP(0x8005f70c);
  decided = equal(V1, V0, &branch);
  STEP(0x8005f710);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f70c));
  if (branch)
    goto publish_contact;
  STEP(0x8005f714);
  OK(load(r, 2, 16, 0xda, 1, 0, UINT32_C(0x8005f714)));
  STEP(0x8005f718);
  STEP(0x8005f71c);
  V0 = bitand_constant(V0, 1);
  STEP(0x8005f720);
  decided = equal(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x8005f724);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f720));
  if (branch)
    goto publish_contact;
  STEP(0x8005f728);
  decided = equal(S3, ZERO, &branch);
  STEP(0x8005f72c);
  multiply(r, S4, S8);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f728));
  if (branch)
    goto publish_contact;
  STEP(0x8005f730);
  T0 = r->m.lo;
  STEP(0x8005f734);
  STEP(0x8005f738);
  STEP(0x8005f73c);
  multiply(r, S4, S7);
  STEP(0x8005f740);
  OK(load(r, 3, 16, 0x14, 2, 1, UINT32_C(0x8005f740)));
  STEP(0x8005f744);
  A1 = r->m.lo;
  STEP(0x8005f748);
  V0 = shift(T0, 8, 1, 1);
  STEP(0x8005f74c);
  V0 = add(V0, V1);
  STEP(0x8005f750);
  multiply(r, S3, V0);
  STEP(0x8005f754);
  OK(load(r, 3, 16, 0x16, 2, 1, UINT32_C(0x8005f754)));
  STEP(0x8005f758);
  A0 = r->m.lo;
  STEP(0x8005f75c);
  V0 = shift(A1, 8, 1, 1);
  STEP(0x8005f760);
  V0 = add(V0, V1);
  STEP(0x8005f764);
  multiply(r, S3, V0);
  STEP(0x8005f768);
  OK(load(r, 3, 16, 0x1a, 1, 0, UINT32_C(0x8005f768)));
  STEP(0x8005f76c);
  V0 = shift(A0, 3, 1, 1);
  STEP(0x8005f770);
  OK(store(r, 2, 16, 0x14, 2, UINT32_C(0x8005f770)));
  STEP(0x8005f774);
  known(&V0, 0x14);
  STEP(0x8005f778);
  T0 = r->m.lo;
  STEP(0x8005f77c);
  A1 = shift(T0, 3, 1, 1);
  STEP(0x8005f780);
  decided = equal(V1, V0, &branch);
  STEP(0x8005f784);
  OK(store(r, 5, 16, 0x16, 2, UINT32_C(0x8005f784)));
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f780));
  if (!branch) {
    STEP(0x8005f788);
    known(&V0, 0x0e);
    STEP(0x8005f78c);
    STEP(0x8005f790);
    OK(store(r, 2, 16, 0xe6, 2, UINT32_C(0x8005f790)));
    goto publish_contact;
  }
  STEP(0x8005f794);
  OK(load(r, 3, 16, 0x46, 2, 0, UINT32_C(0x8005f794)));
  STEP(0x8005f798);
  known(&V0, 0x50);
  STEP(0x8005f79c);
  decided = equal(V1, V0, &branch);
  branch = !branch;
  STEP(0x8005f7a0);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f79c));
  if (branch)
    goto publish_contact;
  STEP(0x8005f7a4);
  OK(load(r, 2, 16, 0x50, 2, 0, UINT32_C(0x8005f7a4)));
  STEP(0x8005f7a8);
  STEP(0x8005f7ac);
  decided = equal(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x8005f7b0);
  A1 = shift(A1, 16, 0, 0);
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f7ac));
  if (branch)
    goto publish_contact;
  STEP(0x8005f7b4);
  OK(load(r, 4, 16, 0x14, 2, 1, UINT32_C(0x8005f7b4)));
  STEP(0x8005f7b8);
  A1 = shift(A1, 16, 1, 1);
  STEP(0x8005f7bc);
  known(&RA, UINT32_C(0x8005f7c4));
  STEP(0x8005f7c0);
  A2 = add(SP, imm(0x20));
  OK(invoke(r, UINT32_C(0x8005f7bc), UINT32_C(0x800706e4)));
  STEP(0x8005f7c4);
  OK(load(r, 2, 16, 0xa8, 2, 1, UINT32_C(0x8005f7c4)));
  STEP(0x8005f7c8);
  OK(load(r, 3, 29, 0x20, 2, 1, UINT32_C(0x8005f7c8)));
  STEP(0x8005f7cc);
  STEP(0x8005f7d0);
  V0 = sub(V0, V1);
  STEP(0x8005f7d4);
  V0 = add(V0, imm(0x100));
  STEP(0x8005f7d8);
  V0 = bitand_constant(V0, 0x3ff);
  STEP(0x8005f7dc);
  V0 = signed_less_constant(V0, 0x201);
  STEP(0x8005f7e0);
  decided = equal(V0, ZERO, &branch);
  STEP(0x8005f7e4);
  A0 = S0;
  if (!decided)
    return unknown_branch(r, UINT32_C(0x8005f7e0));
  if (branch)
    goto publish_contact;
  STEP(0x8005f7e8);
  known(&A1, 0x52);
  STEP(0x8005f7ec);
  known(&V0, UINT32_MAX);
  STEP(0x8005f7f0);
  OK(store(r, 0, 16, 0x64, 2, UINT32_C(0x8005f7f0)));
  STEP(0x8005f7f4);
  OK(store(r, 0, 16, 0x60, 2, UINT32_C(0x8005f7f4)));
  STEP(0x8005f7f8);
  OK(store(r, 2, 16, 0x4c, 2, UINT32_C(0x8005f7f8)));
  STEP(0x8005f7fc);
  known(&RA, UINT32_C(0x8005f804));
  STEP(0x8005f800);
  OK(store(r, 2, 16, 0x48, 2, UINT32_C(0x8005f800)));
  OK(invoke(r, UINT32_C(0x8005f7fc), UINT32_C(0x80056b78)));
  STEP(0x8005f804);
  A0 = S0;
  STEP(0x8005f808);
  known(&A1, 0x53);
  STEP(0x8005f80c);
  known(&RA, UINT32_C(0x8005f814));
  STEP(0x8005f810);
  known(&A2, 0);
  OK(invoke(r, UINT32_C(0x8005f80c), UINT32_C(0x80056ce0)));
  STEP(0x8005f814);
  A0 = S0;
  STEP(0x8005f818);
  known(&A1, 0x29);
  STEP(0x8005f81c);
  known(&RA, UINT32_C(0x8005f824));
  STEP(0x8005f820);
  known(&A2, 0);
  OK(invoke(r, UINT32_C(0x8005f81c), UINT32_C(0x80056c28)));
  STEP(0x8005f824);
  A0 = S0;
  STEP(0x8005f828);
  known(&A1, 0);
  STEP(0x8005f82c);
  known(&RA, UINT32_C(0x8005f834));
  STEP(0x8005f830);
  known(&A2, 0);
  OK(invoke(r, UINT32_C(0x8005f82c), UINT32_C(0x80056c84)));

publish_contact:
  /* Publish symmetric low-byte IDs and the global contact flag. */
  STEP(0x8005f834);
  OK(load(r, 3, 17, 0, 4, 0, UINT32_C(0x8005f834)));
  STEP(0x8005f838);
  OK(load(r, 4, 16, 0, 4, 0, UINT32_C(0x8005f838)));
  STEP(0x8005f83c);
  known(&V0, 1);
  STEP(0x8005f840);
  OK(store(r, 3, 16, 0xdc, 1, UINT32_C(0x8005f840)));
  STEP(0x8005f844);
  known(&V1, 1);
  STEP(0x8005f848);
  OK(store(r, 4, 17, 0xdc, 1, UINT32_C(0x8005f848)));
  STEP(0x8005f84c);
  known(&AT, UINT32_C(0x80100000));
  STEP(0x8005f850);
  OK(store(r, 3, 1, -0x2478, 2, UINT32_C(0x8005f850)));
  goto epilogue;

reject:
  STEP(0x8005f4c8);
  STEP(0x8005f4cc);
  known(&V0, 0);

epilogue:
  /* Callback-mutable SP selects every restore and the final wrapped SP. */
  STEP(0x8005f854);
  OK(load(r, 31, 29, 0x54, 4, 0, UINT32_C(0x8005f854)));
  STEP(0x8005f858);
  OK(load(r, 30, 29, 0x50, 4, 0, UINT32_C(0x8005f858)));
  STEP(0x8005f85c);
  OK(load(r, 23, 29, 0x4c, 4, 0, UINT32_C(0x8005f85c)));
  STEP(0x8005f860);
  OK(load(r, 22, 29, 0x48, 4, 0, UINT32_C(0x8005f860)));
  STEP(0x8005f864);
  OK(load(r, 21, 29, 0x44, 4, 0, UINT32_C(0x8005f864)));
  STEP(0x8005f868);
  OK(load(r, 20, 29, 0x40, 4, 0, UINT32_C(0x8005f868)));
  STEP(0x8005f86c);
  OK(load(r, 19, 29, 0x3c, 4, 0, UINT32_C(0x8005f86c)));
  STEP(0x8005f870);
  OK(load(r, 18, 29, 0x38, 4, 0, UINT32_C(0x8005f870)));
  STEP(0x8005f874);
  OK(load(r, 17, 29, 0x34, 4, 0, UINT32_C(0x8005f874)));
  STEP(0x8005f878);
  OK(load(r, 16, 29, 0x30, 4, 0, UINT32_C(0x8005f878)));
  STEP(0x8005f87c);
  SP = add(SP, imm(0x58));
  STEP(0x8005f880);
  STEP(0x8005f884);
  if (RA.known_mask != 15) {
    stop(r, UINT32_C(0x8005f880), RA.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  p->restored_return_address = RA;
  p->completed = 1;
  stop(r, 0, 0, 0);
  publish(r);
  return NBA97_TEXT_COMPLETE;
}
