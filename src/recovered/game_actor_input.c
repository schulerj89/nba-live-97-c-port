#include "game_actor_input.h"

#include <string.h>

typedef Nba97GameActorInputWord Word;
typedef struct Run {
  Nba97GameActorInputContext *c;
  Nba97GameActorInputProgress *p;
  Nba97GameActorInputMachine m;
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
static void publish(Run *r) { r->p->machine = r->m; }
static void stop(Run *r, uint32_t pc, uint32_t address, uint32_t entry) {
  r->p->stopped_pc = pc;
  r->p->stopped_address = address;
  r->p->stopped_entry = entry;
  publish(r);
}
static int valid_machine(const Nba97GameActorInputMachine *m) {
  unsigned i;
  if (m->registers.gpr[0].word || m->registers.gpr[0].known_mask != 15 ||
      m->hi.known_mask > 15 || m->lo.known_mask > 15)
    return 0;
  for (i = 0; i < 32; i++)
    if (m->registers.gpr[i].known_mask > 15)
      return 0;
  return 1;
}
static int spend(Run *r) {
  if (r->p->operations >= r->c->operation_budget)
    return NBA97_TEXT_LIMIT;
  r->p->operations++;
  return NBA97_TEXT_COMPLETE;
}
static uint32_t wm(unsigned w) {
  return w == 4 ? UINT32_MAX : (UINT32_C(1) << (8 * w)) - 1;
}
static uint8_t km(unsigned w) { return (uint8_t)((1u << w) - 1u); }
static void journal(Run *r, uint8_t k, uint32_t pc, uint32_t a, unsigned w,
                    Word v) {
  size_t n = r->p->access_events++;
  if (n < r->c->access_journal_capacity) {
    Nba97GameActorInputAccess *e = &r->c->access_journal[n];
    e->pc = pc;
    e->address = a;
    e->value = v.word & wm(w);
    e->operation = r->p->operations;
    e->width = (uint8_t)w;
    e->known_mask = (uint8_t)(v.known_mask & km(w));
    e->kind = k;
  }
}
static int locate(Run *r, uint32_t a, unsigned w, uint32_t pc, uint8_t **d,
                  uint8_t **k) {
  size_t i, j;
  stop(r, pc, a, 0);
  OK(spend(r));
  r->p->accesses++;
  if (a & (w - 1u))
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0; i < r->c->memory.count; i++) {
    Nba97GameTextRegion *z = &r->c->memory.region[i];
    uint64_t o = (uint64_t)a - z->base;
    if (a < z->base || o > z->size || w > z->size - (size_t)o)
      continue;
    *d = z->data + (size_t)o;
    *k = z->known ? z->known + (size_t)o : 0;
    if (*k)
      for (j = 0; j < w; j++)
        if ((*k)[j] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}
static int memread(Run *r, uint32_t a, unsigned w, uint32_t pc, Word *v) {
  uint8_t *d, *k;
  unsigned i;
  Word x = {0, 0};
  OK(locate(r, a, w, pc, &d, &k));
  for (i = 0; i < w; i++) {
    x.word |= (uint32_t)d[i] << (8 * i);
    if (!k || k[i])
      x.known_mask |= (uint8_t)(1u << i);
  }
  *v = x;
  r->p->reads++;
  journal(r, NBA97_GAME_MATCH_CLOCKS_READ, pc, a, w, x);
  publish(r);
  return NBA97_TEXT_COMPLETE;
}
static int memwrite(Run *r, uint32_t a, unsigned w, uint32_t pc, Word v) {
  uint8_t *d, *k;
  unsigned i;
  v.word &= wm(w);
  v.known_mask &= km(w);
  OK(locate(r, a, w, pc, &d, &k));
  if (!k && v.known_mask != km(w))
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < w; i++) {
    d[i] = (uint8_t)(v.word >> (8 * i));
    if (k)
      k[i] = (uint8_t)((v.known_mask >> i) & 1u);
  }
  r->p->stores++;
  journal(r, NBA97_GAME_MATCH_CLOCKS_STORE, pc, a, w, v);
  publish(r);
  return NBA97_TEXT_COMPLETE;
}
/* Enumerating byte carries and borrows retains an output byte exactly when it
 * is invariant across every concrete value represented by the input masks. */
static Word add(Word a, Word b) {
  Word result;
  unsigned carry_mask = 1u;
  unsigned byte;
  result.word = a.word + b.word;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_carry_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned left_start =
        (a.known_mask & (1u << byte)) ? ((a.word >> (byte * 8u)) & 0xffu) : 0u;
    unsigned left_end = (a.known_mask & (1u << byte)) ? left_start : 255u;
    unsigned right_start =
        (b.known_mask & (1u << byte)) ? ((b.word >> (byte * 8u)) & 0xffu) : 0u;
    unsigned right_end = (b.known_mask & (1u << byte)) ? right_start : 255u;
    unsigned carry;
    for (carry = 0; carry <= 1; ++carry) {
      unsigned left;
      if (!(carry_mask & (1u << carry)))
        continue;
      for (left = left_start; left <= left_end; ++left) {
        unsigned right;
        for (right = right_start; right <= right_end; ++right) {
          unsigned sum = left + right + carry;
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
static Word sub(Word a, Word b) {
  Word result;
  unsigned byte;
  unsigned borrow_mask = 1;
  result.word = a.word - b.word;
  result.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_borrow_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned left_start =
        (a.known_mask & (1u << byte)) ? ((a.word >> (byte * 8u)) & 0xffu) : 0u;
    unsigned left_end = (a.known_mask & (1u << byte)) ? left_start : 255u;
    unsigned right_start =
        (b.known_mask & (1u << byte)) ? ((b.word >> (byte * 8u)) & 0xffu) : 0u;
    unsigned right_end = (b.known_mask & (1u << byte)) ? right_start : 255u;
    unsigned borrow;
    for (borrow = 0; borrow <= 1; ++borrow) {
      unsigned left;
      if (!(borrow_mask & (1u << borrow)))
        continue;
      for (left = left_start; left <= left_end; ++left) {
        unsigned right;
        for (right = right_start; right <= right_end; ++right) {
          unsigned output = (left - right - borrow) & 0xffu;
          next_borrow_mask |= 1u << (left < right + borrow);
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
static Word imm(uint32_t x) {
  Word z;
  known(&z, x);
  return z;
}
static int addr(Run *r, Word b, int32_t o, uint32_t pc, uint32_t *a) {
  Word z = add(b, imm((uint32_t)o));
  if (z.known_mask != 15) {
    stop(r, pc, z.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *a = z.word;
  return NBA97_TEXT_COMPLETE;
}
static Word ext(Word x, unsigned w, int sign) {
  Word z;
  x.word &= wm(w);
  x.known_mask &= km(w);
  z.word = x.word;
  if (sign && (x.word & (UINT32_C(1) << (w * 8 - 1))))
    z.word |= ~wm(w);
  z.known_mask = x.known_mask;
  if (!sign)
    z.known_mask |= (uint8_t)(15u ^ km(w));
  else if (x.known_mask & (1u << (w - 1)))
    z.known_mask |= (uint8_t)(15u ^ km(w));
  return z;
}
static int load(Run *r, unsigned rt, unsigned base, int32_t off, unsigned w,
                int sign, uint32_t pc) {
  uint32_t a;
  Word x;
  OK(addr(r, R(base), off, pc, &a));
  OK(memread(r, a, w, pc, &x));
  R(rt) = ext(x, w, sign);
  return NBA97_TEXT_COMPLETE;
}
static int store(Run *r, unsigned rt, unsigned base, int32_t off, unsigned w,
                 uint32_t pc) {
  uint32_t a;
  OK(addr(r, R(base), off, pc, &a));
  return memwrite(r, a, w, pc, R(rt));
}
static Word bitandc(Word a, uint32_t m) {
  Word z;
  unsigned i;
  z.word = a.word & m;
  z.known_mask = 0;
  for (i = 0; i < 4; i++) {
    uint32_t q = (m >> (8 * i)) & 255;
    if (!q || (a.known_mask & (1u << i)))
      z.known_mask |= (uint8_t)(1u << i);
  }
  return z;
}
static Word bitorc(Word a, uint32_t m) {
  Word z;
  unsigned i;
  z.word = a.word | m;
  z.known_mask = 0;
  for (i = 0; i < 4; i++) {
    uint32_t q = (m >> (8 * i)) & 255;
    if (q == 255 || (a.known_mask & (1u << i)))
      z.known_mask |= (uint8_t)(1u << i);
  }
  return z;
}
static Word shift(Word a, unsigned n, int right, int arithmetic) {
  Word z;
  unsigned output;
  z.word = right ? a.word >> n : a.word << n;
  if (right && arithmetic && (a.word & UINT32_C(0x80000000)))
    z.word |= UINT32_MAX << (32 - n);
  z.known_mask = 0;
  for (output = 0; output < 4; output++) {
    unsigned low = output * 8, high = low + 7, source_low, source_high, k,
             all = 1;
    if (!right) {
      if (high < n) {
        z.known_mask |= (uint8_t)(1u << output);
        continue;
      }
      source_low = low < n ? 0 : low - n;
      source_high = high - n;
    } else {
      source_low = low + n;
      source_high = high + n;
      if (source_low >= 32) {
        if (!arithmetic || (a.known_mask & 8))
          z.known_mask |= (uint8_t)(1u << output);
        continue;
      }
      if (source_high >= 32) {
        if (arithmetic && !(a.known_mask & 8))
          continue;
        source_high = 31;
      }
    }
    for (k = source_low / 8; k <= source_high / 8; k++)
      if (!(a.known_mask & (1u << k)))
        all = 0;
    if (all)
      z.known_mask |= (uint8_t)(1u << output);
  }
  return z;
}
static Word compare(Word a, uint32_t b, int sign) {
  Word z;
  uint32_t minimum = 0, maximum = 0;
  unsigned i;
  z.word = sign ? ((int32_t)a.word < (int32_t)b) : (a.word < b);
  z.known_mask = 14;
  if (sign && !(a.known_mask & 8))
    return z;
  for (i = 0; i < 4; i++) {
    uint32_t byte = (a.word >> (8 * i)) & 255;
    minimum |= ((a.known_mask & (1u << i)) ? byte : 0) << (8 * i);
    maximum |= ((a.known_mask & (1u << i)) ? byte : 255) << (8 * i);
  }
  if (sign) {
    int32_t smin = (int32_t)minimum, smax = (int32_t)maximum,
            limit = (int32_t)b;
    if (smax < limit || smin >= limit)
      z.known_mask = 15;
  } else if (maximum < b || minimum >= b)
    z.known_mask = 15;
  return z;
}
static int eq(Word a, Word b, int *v) {
  unsigned i;
  for (i = 0; i < 4; i++)
    if ((a.known_mask & b.known_mask & (1u << i)) &&
        ((a.word >> (8 * i) & 255) != (b.word >> (8 * i) & 255))) {
      *v = 0;
      return 1;
    }
  if (a.known_mask == 15 && b.known_mask == 15) {
    *v = a.word == b.word;
    return 1;
  }
  return 0;
}
static int signcond(Word a, int nonnegative, int *v) {
  if (!(a.known_mask & 8))
    return 0;
  *v = nonnegative ? !((a.word >> 31) & 1) : ((a.word >> 31) & 1);
  return 1;
}
static uint8_t call_kind(uint32_t entry) {
  switch (entry) {
  case UINT32_C(0x8008f224):
    return 1;
  case UINT32_C(0x8002d2dc):
    return 2;
  case UINT32_C(0x800700e4):
    return 3;
  case UINT32_C(0x80061760):
    return 4;
  case UINT32_C(0x80063b74):
    return 5;
  case UINT32_C(0x8006fac4):
    return 6;
  case UINT32_C(0x800670a8):
    return 7;
  case UINT32_C(0x8006afb0):
    return 8;
  case UINT32_C(0x8006c518):
    return 9;
  case UINT32_C(0x8006b064):
    return 10;
  case UINT32_C(0x8006c720):
    return 11;
  case UINT32_C(0x8006b168):
    return 12;
  case UINT32_C(0x8006cae0):
    return 13;
  case UINT32_C(0x800597ec):
    return 14;
  case UINT32_C(0x8005853c):
    return 15;
  case UINT32_C(0x8006ce60):
    return 16;
  case UINT32_C(0x8006ac0c):
    return 17;
  case UINT32_C(0x8006bd88):
    return 18;
  case UINT32_C(0x8006b170):
    return 19;
  case UINT32_C(0x8005c5e0):
    return 20;
  case UINT32_C(0x80059f44):
    return 21;
  case UINT32_C(0x8005b028):
    return 22;
  case UINT32_C(0x8005c438):
    return 23;
  case UINT32_C(0x80059968):
    return 24;
  case UINT32_C(0x8005d070):
    return 25;
  case UINT32_C(0x8005cf5c):
    return 26;
  case UINT32_C(0x8005d9f0):
    return 27;
  default:
    return 0;
  }
}
static uint8_t call_args(uint32_t entry) {
  switch (entry) {
  case UINT32_C(0x8008f224):
    return 1;
  case UINT32_C(0x8002d2dc):
  case UINT32_C(0x800700e4):
  case UINT32_C(0x8006fac4):
    return 2;
  case UINT32_C(0x80061760):
    return 4;
  case UINT32_C(0x800670a8):
  case UINT32_C(0x8006b168):
    return 0;
  default:
    return 1;
  }
}
static int invoke_site(Run *r, uint32_t pc, uint32_t entry) {
  Nba97GameActorInputEvent event;
  int accepted;
  uint8_t kind = call_kind(entry);
  stop(r, pc, 0, entry);
  OK(spend(r));
  memset(&event, 0, sizeof event);
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = entry;
  event.operation = r->p->operations;
  event.kind = kind;
  event.argument_count = call_args(entry);
  event.invocation = r->p->call_count[kind] + 1u;
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
static int nonpositive(Word value, int *answer) {
  if (value.known_mask & 8u) {
    if (value.word & UINT32_C(0x80000000)) {
      *answer = 1;
      return 1;
    }
    if (value.known_mask == 15u) {
      *answer = value.word == 0;
      return 1;
    }
    for (unsigned byte = 0; byte < 4; ++byte)
      if ((value.known_mask & (1u << byte)) &&
          ((value.word >> (byte * 8u)) & 0xffu)) {
        *answer = 0;
        return 1;
      }
  }
  return 0;
}
int nba97_game_actor_input(Nba97GameActorInputContext *c,
                           Nba97GameActorInputProgress *p) {
  Run run, *r = &run;
  size_t i, j;
  int branch = 0, decided = 0;
  Word jump_target;
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
  /* 0x800686B8..0x8006870C: option/countdown precedes loop setup. */
  STEP(0x800686b8);
  known(&V0, UINT32_C(0x80020000));
  STEP(0x800686bc);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0x00001d82), 1, 0, UINT32_C(0x800686bc)));
  STEP(0x800686c0);
  SP = add(SP, imm(UINT32_C(0xffffffb8)));
  p->frame_stack_pointer = SP.word;
  STEP(0x800686c4);
  OK(store(r, 31, 29, (int32_t)UINT32_C(0x00000044), 4, UINT32_C(0x800686c4)));
  STEP(0x800686c8);
  OK(store(r, 30, 29, (int32_t)UINT32_C(0x00000040), 4, UINT32_C(0x800686c8)));
  STEP(0x800686cc);
  OK(store(r, 23, 29, (int32_t)UINT32_C(0x0000003c), 4, UINT32_C(0x800686cc)));
  STEP(0x800686d0);
  OK(store(r, 22, 29, (int32_t)UINT32_C(0x00000038), 4, UINT32_C(0x800686d0)));
  STEP(0x800686d4);
  OK(store(r, 21, 29, (int32_t)UINT32_C(0x00000034), 4, UINT32_C(0x800686d4)));
  STEP(0x800686d8);
  OK(store(r, 20, 29, (int32_t)UINT32_C(0x00000030), 4, UINT32_C(0x800686d8)));
  STEP(0x800686dc);
  OK(store(r, 19, 29, (int32_t)UINT32_C(0x0000002c), 4, UINT32_C(0x800686dc)));
  STEP(0x800686e0);
  OK(store(r, 18, 29, (int32_t)UINT32_C(0x00000028), 4, UINT32_C(0x800686e0)));
  STEP(0x800686e4);
  OK(store(r, 17, 29, (int32_t)UINT32_C(0x00000024), 4, UINT32_C(0x800686e4)));
  /* 0x800686E8: evaluate before the source delay instruction. */
  STEP(0x800686e8);
  decided = eq(V0, ZERO, &branch);
  STEP(0x800686ec);
  OK(store(r, 16, 29, (int32_t)UINT32_C(0x00000020), 4, UINT32_C(0x800686ec)));
  if (!decided) {
    stop(r, UINT32_C(0x800686e8), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068710;
  STEP(0x800686f0);
  known(&A0, UINT32_C(0x80100000));
  STEP(0x800686f4);
  A0 = add(A0, imm(UINT32_C(0xffffdb8a)));
  STEP(0x800686f8);
  OK(load(r, 2, 4, (int32_t)UINT32_C(0x00000000), 2, 1, UINT32_C(0x800686f8)));
  STEP(0x800686fc);
  /* 0x80068700: evaluate before the source delay instruction. */
  STEP(0x80068700);
  decided = eq(V0, ZERO, &branch);
  STEP(0x80068704);
  V1 = V0;
  if (!decided) {
    stop(r, UINT32_C(0x80068700), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068710;
  STEP(0x80068708);
  V0 = add(V1, imm(UINT32_C(0xffffffff)));
  STEP(0x8006870c);
  OK(store(r, 2, 4, (int32_t)UINT32_C(0x00000000), 2, UINT32_C(0x8006870c)));
L_80068710:
  /* Establish the callback-live actor cursor and global/team bases. */
  STEP(0x80068710);
  known(&S4, 0);
  STEP(0x80068714);
  known(&T0, UINT32_C(0x80020000));
  STEP(0x80068718);
  T0 = add(T0, imm(UINT32_C(0x00000bec)));
  STEP(0x8006871c);
  S8 = add(T0, imm(UINT32_C(0xffffe2cc)));
  STEP(0x80068720);
  known(&S7, UINT32_C(0x80100000));
  STEP(0x80068724);
  S7 = add(S7, imm(UINT32_C(0xffffdc3c)));
  STEP(0x80068728);
  S2 = add(S7, imm(UINT32_C(0x00000008)));
  STEP(0x8006872c);
  S5 = add(S7, imm(UINT32_C(0x00000c90)));
  STEP(0x80068730);
  known(&S6, UINT32_C(0x80020000));
  STEP(0x80068734);
  S6 = add(S6, imm(UINT32_C(0x00000bec)));
L_80068738:
  /* Publish this actor and select its team before inspecting its claim. */
  STEP(0x80068738);
  OK(load(r, 2, 22, (int32_t)UINT32_C(0x00000000), 4, 0, UINT32_C(0x80068738)));
  STEP(0x8006873c);
  STEP(0x80068740);
  S1 = V0;
  STEP(0x80068744);
  OK(store(r, 2, 23, (int32_t)UINT32_C(0x00000000), 4, UINT32_C(0x80068744)));
  STEP(0x80068748);
  V0 = compare(S4, UINT32_C(0x00000005), 1);
  /* 0x8006874C: evaluate before the source delay instruction. */
  STEP(0x8006874c);
  decided = eq(V0, ZERO, &branch);
  STEP(0x80068750);
  S3 = S8;
  if (!decided) {
    stop(r, UINT32_C(0x8006874c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8006876c;
  STEP(0x80068754);
  known(&T0, UINT32_C(0x80020000));
  STEP(0x80068758);
  T0 = add(T0, imm(UINT32_C(0x00000bec)));
  STEP(0x8006875c);
  V0 = add(T0, imm(UINT32_C(0xffffe208)));
  STEP(0x80068760);
  OK(store(r, 2, 23, (int32_t)UINT32_C(0x00000004), 4, UINT32_C(0x80068760)));
  /* 0x80068764: transfer after the source delay instruction. */
  STEP(0x80068764);
  STEP(0x80068768);
  S3 = V0;
  goto L_80068770;
L_8006876c:
  STEP(0x8006876c);
  OK(store(r, 30, 23, (int32_t)UINT32_C(0x00000004), 4, UINT32_C(0x8006876c)));
L_80068770:
  /* A nonnegative claim selects a controller and performs the input pipeline.
   */
  STEP(0x80068770);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x00000004), 2, 1, UINT32_C(0x80068770)));
  STEP(0x80068774);
  /* 0x80068778: evaluate before the source delay instruction. */
  STEP(0x80068778);
  decided = signcond(V0, 0, &branch);
  STEP(0x8006877c);
  V0 = shift(V0, 2, 0, 0);
  if (!decided) {
    stop(r, UINT32_C(0x80068778), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068924;
  STEP(0x80068780);
  V0 = add(S2, V0);
  STEP(0x80068784);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0x0000000c), 4, 0, UINT32_C(0x80068784)));
  STEP(0x80068788);
  STEP(0x8006878c);
  OK(store(r, 2, 18, (int32_t)UINT32_C(0x00000000), 4, UINT32_C(0x8006878c)));
  STEP(0x80068790);
  known(&V1, UINT32_C(0x80100000));
  STEP(0x80068794);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdc44), 4, 0, UINT32_C(0x80068794)));
  STEP(0x80068798);
  OK(store(r, 2, 18, (int32_t)UINT32_C(0x00000008), 4, UINT32_C(0x80068798)));
  STEP(0x8006879c);
  OK(load(r, 2, 3, (int32_t)UINT32_C(0x00000028), 2, 1, UINT32_C(0x8006879c)));
  STEP(0x800687a0);
  /* 0x800687A4: evaluate before the source delay instruction. */
  STEP(0x800687a4);
  decided = eq(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x800687a8);
  if (!decided) {
    stop(r, UINT32_C(0x800687a4), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068924;
  STEP(0x800687ac);
  OK(load(r, 4, 17, (int32_t)UINT32_C(0x00000004), 2, 1, UINT32_C(0x800687ac)));
  /* 0x800687B0: JAL 0x8008F224; link is visible to the delay instruction. */
  STEP(0x800687b0);
  known(&RA, UINT32_C(0x800687b8));
  STEP(0x800687b4);
  OK(invoke_site(r, UINT32_C(0x800687b0), UINT32_C(0x8008f224)));
  STEP(0x800687b8);
  OK(load(r, 4, 19, (int32_t)UINT32_C(0x00000014), 2, 0, UINT32_C(0x800687b8)));
  STEP(0x800687bc);
  OK(load(r, 3, 18, (int32_t)UINT32_C(0xffffff50), 2, 1, UINT32_C(0x800687bc)));
  STEP(0x800687c0);
  /* 0x800687C4: evaluate before the source delay instruction. */
  STEP(0x800687c4);
  decided = eq(A0, V1, &branch);
  branch = !branch;
  STEP(0x800687c8);
  A0 = V0;
  if (!decided) {
    stop(r, UINT32_C(0x800687c4), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800687d8;
  STEP(0x800687cc);
  OK(load(r, 5, 18, (int32_t)UINT32_C(0x00000000), 4, 0, UINT32_C(0x800687cc)));
  /* 0x800687D0: transfer after the source delay instruction. */
  STEP(0x800687d0);
  STEP(0x800687d4);
  A1 = add(A1, imm(UINT32_C(0x0000003f)));
  goto L_800687e4;
L_800687d8:
  STEP(0x800687d8);
  OK(load(r, 5, 18, (int32_t)UINT32_C(0x00000000), 4, 0, UINT32_C(0x800687d8)));
  STEP(0x800687dc);
  STEP(0x800687e0);
  A1 = add(A1, imm(UINT32_C(0x00000047)));
L_800687e4:
  /* 0x800687E4: JAL 0x8002D2DC; link is visible to the delay instruction. */
  STEP(0x800687e4);
  known(&RA, UINT32_C(0x800687ec));
  STEP(0x800687e8);
  OK(invoke_site(r, UINT32_C(0x800687e4), UINT32_C(0x8002d2dc)));
  STEP(0x800687ec);
  OK(load(r, 4, 18, (int32_t)UINT32_C(0x00000000), 4, 0, UINT32_C(0x800687ec)));
  STEP(0x800687f0);
  A1 = V0;
  /* 0x800687F4: JAL 0x800700E4; link is visible to the delay instruction. */
  STEP(0x800687f4);
  known(&RA, UINT32_C(0x800687fc));
  STEP(0x800687f8);
  S0 = A1;
  OK(invoke_site(r, UINT32_C(0x800687f4), UINT32_C(0x800700e4)));
  STEP(0x800687fc);
  OK(load(r, 4, 18, (int32_t)UINT32_C(0xfffffff8), 4, 0, UINT32_C(0x800687fc)));
  STEP(0x80068800);
  OK(load(r, 5, 18, (int32_t)UINT32_C(0x00000000), 4, 0, UINT32_C(0x80068800)));
  STEP(0x80068804);
  A2 = V0;
  /* 0x80068808: JAL 0x80061760; link is visible to the delay instruction. */
  STEP(0x80068808);
  known(&RA, UINT32_C(0x80068810));
  STEP(0x8006880c);
  A3 = S0;
  OK(invoke_site(r, UINT32_C(0x80068808), UINT32_C(0x80061760)));
  STEP(0x80068810);
  OK(load(r, 4, 18, (int32_t)UINT32_C(0xfffffff8), 4, 0, UINT32_C(0x80068810)));
  /* 0x80068814: JAL 0x80063B74; link is visible to the delay instruction. */
  STEP(0x80068814);
  known(&RA, UINT32_C(0x8006881c));
  STEP(0x80068818);
  OK(invoke_site(r, UINT32_C(0x80068814), UINT32_C(0x80063b74)));
  STEP(0x8006881c);
  /* The first special-mode gate decides whether direction input is eligible. */
  OK(load(r, 3, 18, (int32_t)UINT32_C(0x00000c88), 2, 1, UINT32_C(0x8006881c)));
  STEP(0x80068820);
  /* 0x80068824: evaluate before the source delay instruction. */
  STEP(0x80068824);
  decided = eq(V1, ZERO, &branch);
  STEP(0x80068828);
  V0 = bitorc(ZERO, UINT32_C(0x0000000a));
  if (!decided) {
    stop(r, UINT32_C(0x80068824), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068874;
  /* 0x8006882C: evaluate before the source delay instruction. */
  STEP(0x8006882c);
  decided = eq(V1, V0, &branch);
  branch = !branch;
  STEP(0x80068830);
  if (!decided) {
    stop(r, UINT32_C(0x8006882c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068a20;
  STEP(0x80068834);
  OK(load(r, 2, 18, (int32_t)UINT32_C(0x00000c86), 2, 1, UINT32_C(0x80068834)));
  STEP(0x80068838);
  /* 0x8006883C: evaluate before the source delay instruction. */
  STEP(0x8006883c);
  decided = eq(S4, V0, &branch);
  STEP(0x80068840);
  if (!decided) {
    stop(r, UINT32_C(0x8006883c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068a20;
  STEP(0x80068844);
  OK(load(r, 2, 18, (int32_t)UINT32_C(0x00000ca0), 2, 1, UINT32_C(0x80068844)));
  STEP(0x80068848);
  /* 0x8006884C: evaluate before the source delay instruction. */
  STEP(0x8006884c);
  decided = signcond(V0, 1, &branch);
  STEP(0x80068850);
  if (!decided) {
    stop(r, UINT32_C(0x8006884c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068a20;
  STEP(0x80068854);
  OK(load(r, 2, 18, (int32_t)UINT32_C(0xffffff88), 2, 1, UINT32_C(0x80068854)));
  STEP(0x80068858);
  /* 0x8006885C: evaluate before the source delay instruction. */
  STEP(0x8006885c);
  decided = signcond(V0, 1, &branch);
  STEP(0x80068860);
  if (!decided) {
    stop(r, UINT32_C(0x8006885c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068a20;
  STEP(0x80068864);
  OK(load(r, 2, 18, (int32_t)UINT32_C(0x00000c8a), 2, 1, UINT32_C(0x80068864)));
  STEP(0x80068868);
  /* 0x8006886C: evaluate before the source delay instruction. */
  STEP(0x8006886c);
  decided = eq(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x80068870);
  if (!decided) {
    stop(r, UINT32_C(0x8006886c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068a20;
L_80068874:
  /* Motion, repeat, and controller flags gate the direction child call. */
  STEP(0x80068874);
  OK(load(r, 3, 17, (int32_t)UINT32_C(0x00000046), 2, 0, UINT32_C(0x80068874)));
  STEP(0x80068878);
  V0 = bitorc(ZERO, UINT32_C(0x0000002b));
  /* 0x8006887C: evaluate before the source delay instruction. */
  STEP(0x8006887c);
  decided = eq(V1, V0, &branch);
  STEP(0x80068880);
  if (!decided) {
    stop(r, UINT32_C(0x8006887c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068924;
  STEP(0x80068884);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x000000e6), 2, 1, UINT32_C(0x80068884)));
  STEP(0x80068888);
  /* 0x8006888C: evaluate before the source delay instruction. */
  STEP(0x8006888c);
  decided = eq(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x80068890);
  if (!decided) {
    stop(r, UINT32_C(0x8006888c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068924;
  STEP(0x80068894);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x000000da), 1, 0, UINT32_C(0x80068894)));
  STEP(0x80068898);
  STEP(0x8006889c);
  V0 = bitandc(V0, UINT32_C(0x00000004));
  /* 0x800688A0: evaluate before the source delay instruction. */
  STEP(0x800688a0);
  decided = eq(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x800688a4);
  if (!decided) {
    stop(r, UINT32_C(0x800688a0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068924;
  STEP(0x800688a8);
  known(&V1, UINT32_C(0x80100000));
  STEP(0x800688ac);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdc44), 4, 0, UINT32_C(0x800688ac)));
  STEP(0x800688b0);
  OK(load(r, 2, 18, (int32_t)UINT32_C(0xffffff4c), 2, 1, UINT32_C(0x800688b0)));
  STEP(0x800688b4);
  OK(load(r, 5, 3, (int32_t)UINT32_C(0x0000002a), 2, 1, UINT32_C(0x800688b4)));
  STEP(0x800688b8);
  V0 = compare(V0, UINT32_C(0x00000080), 1);
  /* 0x800688BC: evaluate before the source delay instruction. */
  STEP(0x800688bc);
  decided = eq(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x800688c0);
  if (!decided) {
    stop(r, UINT32_C(0x800688bc), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8006891c;
  STEP(0x800688c4);
  OK(load(r, 3, 18, (int32_t)UINT32_C(0x00000c3c), 2, 1, UINT32_C(0x800688c4)));
  STEP(0x800688c8);
  OK(load(r, 2, 19, (int32_t)UINT32_C(0x00000014), 2, 0, UINT32_C(0x800688c8)));
  STEP(0x800688cc);
  /* 0x800688D0: evaluate before the source delay instruction. */
  STEP(0x800688d0);
  decided = eq(V1, V0, &branch);
  branch = !branch;
  STEP(0x800688d4);
  V0 = bitorc(ZERO, UINT32_C(0x0000000b));
  if (!decided) {
    stop(r, UINT32_C(0x800688d0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068908;
  STEP(0x800688d8);
  OK(load(r, 3, 17, (int32_t)UINT32_C(0x0000001a), 1, 0, UINT32_C(0x800688d8)));
  STEP(0x800688dc);
  /* 0x800688E0: evaluate before the source delay instruction. */
  STEP(0x800688e0);
  decided = eq(V1, V0, &branch);
  STEP(0x800688e4);
  if (!decided) {
    stop(r, UINT32_C(0x800688e0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068924;
  STEP(0x800688e8);
  OK(load(r, 2, 18, (int32_t)UINT32_C(0xffffff88), 2, 1, UINT32_C(0x800688e8)));
  STEP(0x800688ec);
  /* 0x800688F0: evaluate before the source delay instruction. */
  STEP(0x800688f0);
  decided = signcond(V0, 1, &branch);
  STEP(0x800688f4);
  V0 = bitorc(ZERO, UINT32_C(0x00000003));
  if (!decided) {
    stop(r, UINT32_C(0x800688f0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8006891c;
  /* 0x800688F8: evaluate before the source delay instruction. */
  STEP(0x800688f8);
  decided = eq(V1, V0, &branch);
  STEP(0x800688fc);
  if (!decided) {
    stop(r, UINT32_C(0x800688f8), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068924;
  /* 0x80068900: transfer after the source delay instruction. */
  STEP(0x80068900);
  STEP(0x80068904);
  goto L_8006891c;
L_80068908:
  STEP(0x80068908);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x000000c2), 2, 1, UINT32_C(0x80068908)));
  STEP(0x8006890c);
  /* 0x80068910: evaluate before the source delay instruction. */
  STEP(0x80068910);
  decided = eq(V0, ZERO, &branch);
  STEP(0x80068914);
  if (!decided) {
    stop(r, UINT32_C(0x80068910), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8006891c;
  STEP(0x80068918);
  A1 = add(V0, imm(UINT32_C(0xffffffff)));
L_8006891c:
  /* 0x8006891C: JAL 0x8006FAC4; link is visible to the delay instruction. */
  STEP(0x8006891c);
  known(&RA, UINT32_C(0x80068924));
  STEP(0x80068920);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x8006891c), UINT32_C(0x8006fac4)));
L_80068924:
  /* Re-read the special mode after callbacks before ordinary state handling. */
  STEP(0x80068924);
  OK(load(r, 3, 21, (int32_t)UINT32_C(0x00000000), 2, 1, UINT32_C(0x80068924)));
  STEP(0x80068928);
  /* 0x8006892C: evaluate before the source delay instruction. */
  STEP(0x8006892c);
  decided = eq(V1, ZERO, &branch);
  STEP(0x80068930);
  V0 = bitorc(ZERO, UINT32_C(0x0000000a));
  if (!decided) {
    stop(r, UINT32_C(0x8006892c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8006897c;
  /* 0x80068934: evaluate before the source delay instruction. */
  STEP(0x80068934);
  decided = eq(V1, V0, &branch);
  branch = !branch;
  STEP(0x80068938);
  if (!decided) {
    stop(r, UINT32_C(0x80068934), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068a20;
  STEP(0x8006893c);
  OK(load(r, 2, 21, (int32_t)UINT32_C(0xfffffffe), 2, 1, UINT32_C(0x8006893c)));
  STEP(0x80068940);
  /* 0x80068944: evaluate before the source delay instruction. */
  STEP(0x80068944);
  decided = eq(S4, V0, &branch);
  STEP(0x80068948);
  if (!decided) {
    stop(r, UINT32_C(0x80068944), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068a20;
  STEP(0x8006894c);
  OK(load(r, 2, 21, (int32_t)UINT32_C(0x00000018), 2, 1, UINT32_C(0x8006894c)));
  STEP(0x80068950);
  /* 0x80068954: evaluate before the source delay instruction. */
  STEP(0x80068954);
  decided = signcond(V0, 1, &branch);
  STEP(0x80068958);
  if (!decided) {
    stop(r, UINT32_C(0x80068954), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068a20;
  STEP(0x8006895c);
  OK(load(r, 2, 21, (int32_t)UINT32_C(0xfffff300), 2, 1, UINT32_C(0x8006895c)));
  STEP(0x80068960);
  /* 0x80068964: evaluate before the source delay instruction. */
  STEP(0x80068964);
  decided = signcond(V0, 1, &branch);
  STEP(0x80068968);
  if (!decided) {
    stop(r, UINT32_C(0x80068964), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068a20;
  STEP(0x8006896c);
  OK(load(r, 2, 21, (int32_t)UINT32_C(0x00000002), 2, 1, UINT32_C(0x8006896c)));
  STEP(0x80068970);
  /* 0x80068974: evaluate before the source delay instruction. */
  STEP(0x80068974);
  decided = eq(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x80068978);
  if (!decided) {
    stop(r, UINT32_C(0x80068974), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068a20;
L_8006897c:
  /* Countdown, height, low flag bits, and motion select B6 or action dispatch.
   */
  STEP(0x8006897c);
  OK(load(r, 2, 21, (int32_t)UINT32_C(0xfffff2b0), 2, 1, UINT32_C(0x8006897c)));
  STEP(0x80068980);
  /* 0x80068984: evaluate before the source delay instruction. */
  STEP(0x80068984);
  decided = eq(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x80068988);
  if (!decided) {
    stop(r, UINT32_C(0x80068984), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068b94;
  STEP(0x8006898c);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x000000d8), 1, 0, UINT32_C(0x8006898c)));
  STEP(0x80068990);
  /* 0x80068994: evaluate before the source delay instruction. */
  STEP(0x80068994);
  decided = eq(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x80068998);
  if (!decided) {
    stop(r, UINT32_C(0x80068994), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068b94;
  STEP(0x8006899c);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x0000001a), 1, 0, UINT32_C(0x8006899c)));
  STEP(0x800689a0);
  STEP(0x800689a4);
  V0 = compare(V0, UINT32_C(0x00000007), 0);
  /* 0x800689A8: evaluate before the source delay instruction. */
  STEP(0x800689a8);
  decided = eq(V0, ZERO, &branch);
  STEP(0x800689ac);
  if (!decided) {
    stop(r, UINT32_C(0x800689a8), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068a20;
  STEP(0x800689b0);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x00000010), 4, 0, UINT32_C(0x800689b0)));
  STEP(0x800689b4);
  /* 0x800689B8: evaluate before the source delay instruction. */
  STEP(0x800689b8);
  decided = eq(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x800689bc);
  if (!decided) {
    stop(r, UINT32_C(0x800689b8), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800689e8;
  STEP(0x800689c0);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x00000060), 2, 0, UINT32_C(0x800689c0)));
  STEP(0x800689c4);
  STEP(0x800689c8);
  V0 = bitandc(V0, UINT32_C(0x00000003));
  /* 0x800689CC: evaluate before the source delay instruction. */
  STEP(0x800689cc);
  decided = eq(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x800689d0);
  if (!decided) {
    stop(r, UINT32_C(0x800689cc), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800689e8;
  STEP(0x800689d4);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x00000064), 2, 0, UINT32_C(0x800689d4)));
  STEP(0x800689d8);
  STEP(0x800689dc);
  V0 = bitandc(V0, UINT32_C(0x00000003));
  /* 0x800689E0: evaluate before the source delay instruction. */
  STEP(0x800689e0);
  decided = eq(V0, ZERO, &branch);
  STEP(0x800689e4);
  if (!decided) {
    stop(r, UINT32_C(0x800689e0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068a20;
L_800689e8:
  STEP(0x800689e8);
  OK(load(r, 3, 17, (int32_t)UINT32_C(0x00000046), 2, 0, UINT32_C(0x800689e8)));
  STEP(0x800689ec);
  V0 = bitorc(ZERO, UINT32_C(0x0000002b));
  /* 0x800689F0: evaluate before the source delay instruction. */
  STEP(0x800689f0);
  decided = eq(V1, V0, &branch);
  STEP(0x800689f4);
  if (!decided) {
    stop(r, UINT32_C(0x800689f0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068a20;
  STEP(0x800689f8);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x000000b6), 2, 1, UINT32_C(0x800689f8)));
  STEP(0x800689fc);
  /* 0x80068A00: evaluate before the source delay instruction. */
  STEP(0x80068a00);
  decided = nonpositive(V0, &branch);
  STEP(0x80068a04);
  V1 = V0;
  if (!decided) {
    stop(r, UINT32_C(0x80068a00), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068b94;
  STEP(0x80068a08);
  known(&V0, UINT32_C(0x80100000));
  STEP(0x80068a0c);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdb6e), 2, 0, UINT32_C(0x80068a0c)));
  STEP(0x80068a10);
  STEP(0x80068a14);
  V0 = sub(V1, V0);
  /* 0x80068A18: transfer after the source delay instruction. */
  STEP(0x80068a18);
  STEP(0x80068a1c);
  OK(store(r, 2, 17, (int32_t)UINT32_C(0x000000b6), 2, UINT32_C(0x80068a1c)));
  goto L_80068b94;
L_80068a20:
  /* Read the actor state and then its real guest jump-table entry. */
  STEP(0x80068a20);
  OK(load(r, 3, 17, (int32_t)UINT32_C(0x0000001a), 1, 0, UINT32_C(0x80068a20)));
  STEP(0x80068a24);
  STEP(0x80068a28);
  V0 = compare(V1, UINT32_C(0x00000015), 0);
  /* 0x80068A2C: evaluate before the source delay instruction. */
  STEP(0x80068a2c);
  decided = eq(V0, ZERO, &branch);
  STEP(0x80068a30);
  V0 = shift(V1, 2, 0, 0);
  if (!decided) {
    stop(r, UINT32_C(0x80068a2c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068b94;
  STEP(0x80068a34);
  known(&AT, UINT32_C(0x80020000));
  STEP(0x80068a38);
  AT = add(AT, V0);
  STEP(0x80068a3c);
  OK(load(r, 2, 1, (int32_t)UINT32_C(0x000075c4), 4, 0, UINT32_C(0x80068a3c)));
  STEP(0x80068a40);
  /* 0x80068A44: capture the jump target before its NOP delay. */
  STEP(0x80068a44);
  jump_target = V0;
  STEP(0x80068a48);
  if (jump_target.known_mask != 15) {
    stop(r, UINT32_C(0x80068a44), jump_target.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  p->computed_action_target = jump_target.word;
  if (jump_target.word & 3u) {
    stop(r, UINT32_C(0x80068a44), jump_target.word, 0);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  switch (jump_target.word) {
  case UINT32_C(0x80068a4c):
    goto L_80068a4c;
  case UINT32_C(0x80068a5c):
    goto L_80068a5c;
  case UINT32_C(0x80068a6c):
    goto L_80068a6c;
  case UINT32_C(0x80068a7c):
    goto L_80068a7c;
  case UINT32_C(0x80068a8c):
    goto L_80068a8c;
  case UINT32_C(0x80068a9c):
    goto L_80068a9c;
  case UINT32_C(0x80068aac):
    goto L_80068aac;
  case UINT32_C(0x80068abc):
    goto L_80068abc;
  case UINT32_C(0x80068acc):
    goto L_80068acc;
  case UINT32_C(0x80068adc):
    goto L_80068adc;
  case UINT32_C(0x80068aec):
    goto L_80068aec;
  case UINT32_C(0x80068afc):
    goto L_80068afc;
  case UINT32_C(0x80068b0c):
    goto L_80068b0c;
  case UINT32_C(0x80068b1c):
    goto L_80068b1c;
  case UINT32_C(0x80068b2c):
    goto L_80068b2c;
  case UINT32_C(0x80068b3c):
    goto L_80068b3c;
  case UINT32_C(0x80068b4c):
    goto L_80068b4c;
  case UINT32_C(0x80068b5c):
    goto L_80068b5c;
  case UINT32_C(0x80068b6c):
    goto L_80068b6c;
  case UINT32_C(0x80068b7c):
    goto L_80068b7c;
  case UINT32_C(0x80068b8c):
    goto L_80068b8c;
  default:
    stop(r, UINT32_C(0x80068a44), jump_target.word, 0);
    return NBA97_TEXT_RESOURCE;
  }
  /* Each table-selected source case preserves its own JAL and delay prefix. */
L_80068a4c:
  /* 0x80068A4C: JAL 0x800670A8; link is visible to the delay instruction. */
  STEP(0x80068a4c);
  known(&RA, UINT32_C(0x80068a54));
  STEP(0x80068a50);
  OK(invoke_site(r, UINT32_C(0x80068a4c), UINT32_C(0x800670a8)));
  /* 0x80068A54: transfer after the source delay instruction. */
  STEP(0x80068a54);
  STEP(0x80068a58);
  goto L_80068b94;
L_80068a5c:
  /* 0x80068A5C: JAL 0x8006AFB0; link is visible to the delay instruction. */
  STEP(0x80068a5c);
  known(&RA, UINT32_C(0x80068a64));
  STEP(0x80068a60);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068a5c), UINT32_C(0x8006afb0)));
  /* 0x80068A64: transfer after the source delay instruction. */
  STEP(0x80068a64);
  STEP(0x80068a68);
  goto L_80068b94;
L_80068a6c:
  /* 0x80068A6C: JAL 0x8006C518; link is visible to the delay instruction. */
  STEP(0x80068a6c);
  known(&RA, UINT32_C(0x80068a74));
  STEP(0x80068a70);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068a6c), UINT32_C(0x8006c518)));
  /* 0x80068A74: transfer after the source delay instruction. */
  STEP(0x80068a74);
  STEP(0x80068a78);
  goto L_80068b94;
L_80068a7c:
  /* 0x80068A7C: JAL 0x8006B064; link is visible to the delay instruction. */
  STEP(0x80068a7c);
  known(&RA, UINT32_C(0x80068a84));
  STEP(0x80068a80);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068a7c), UINT32_C(0x8006b064)));
  /* 0x80068A84: transfer after the source delay instruction. */
  STEP(0x80068a84);
  STEP(0x80068a88);
  goto L_80068b94;
L_80068a8c:
  /* 0x80068A8C: JAL 0x8006C720; link is visible to the delay instruction. */
  STEP(0x80068a8c);
  known(&RA, UINT32_C(0x80068a94));
  STEP(0x80068a90);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068a8c), UINT32_C(0x8006c720)));
  /* 0x80068A94: transfer after the source delay instruction. */
  STEP(0x80068a94);
  STEP(0x80068a98);
  goto L_80068b94;
L_80068a9c:
  /* 0x80068A9C: JAL 0x8006B168; link is visible to the delay instruction. */
  STEP(0x80068a9c);
  known(&RA, UINT32_C(0x80068aa4));
  STEP(0x80068aa0);
  OK(invoke_site(r, UINT32_C(0x80068a9c), UINT32_C(0x8006b168)));
  /* 0x80068AA4: transfer after the source delay instruction. */
  STEP(0x80068aa4);
  STEP(0x80068aa8);
  goto L_80068b94;
L_80068aac:
  /* 0x80068AAC: JAL 0x8006CAE0; link is visible to the delay instruction. */
  STEP(0x80068aac);
  known(&RA, UINT32_C(0x80068ab4));
  STEP(0x80068ab0);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068aac), UINT32_C(0x8006cae0)));
  /* 0x80068AB4: transfer after the source delay instruction. */
  STEP(0x80068ab4);
  STEP(0x80068ab8);
  goto L_80068b94;
L_80068abc:
  /* 0x80068ABC: JAL 0x800597EC; link is visible to the delay instruction. */
  STEP(0x80068abc);
  known(&RA, UINT32_C(0x80068ac4));
  STEP(0x80068ac0);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068abc), UINT32_C(0x800597ec)));
  /* 0x80068AC4: transfer after the source delay instruction. */
  STEP(0x80068ac4);
  STEP(0x80068ac8);
  goto L_80068b94;
L_80068acc:
  /* 0x80068ACC: JAL 0x8005853C; link is visible to the delay instruction. */
  STEP(0x80068acc);
  known(&RA, UINT32_C(0x80068ad4));
  STEP(0x80068ad0);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068acc), UINT32_C(0x8005853c)));
  /* 0x80068AD4: transfer after the source delay instruction. */
  STEP(0x80068ad4);
  STEP(0x80068ad8);
  goto L_80068b94;
L_80068adc:
  /* 0x80068ADC: JAL 0x8006CE60; link is visible to the delay instruction. */
  STEP(0x80068adc);
  known(&RA, UINT32_C(0x80068ae4));
  STEP(0x80068ae0);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068adc), UINT32_C(0x8006ce60)));
  /* 0x80068AE4: transfer after the source delay instruction. */
  STEP(0x80068ae4);
  STEP(0x80068ae8);
  goto L_80068b94;
L_80068aec:
  /* 0x80068AEC: JAL 0x8006AC0C; link is visible to the delay instruction. */
  STEP(0x80068aec);
  known(&RA, UINT32_C(0x80068af4));
  STEP(0x80068af0);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068aec), UINT32_C(0x8006ac0c)));
  /* 0x80068AF4: transfer after the source delay instruction. */
  STEP(0x80068af4);
  STEP(0x80068af8);
  goto L_80068b94;
L_80068afc:
  /* 0x80068AFC: JAL 0x8006BD88; link is visible to the delay instruction. */
  STEP(0x80068afc);
  known(&RA, UINT32_C(0x80068b04));
  STEP(0x80068b00);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068afc), UINT32_C(0x8006bd88)));
  /* 0x80068B04: transfer after the source delay instruction. */
  STEP(0x80068b04);
  STEP(0x80068b08);
  goto L_80068b94;
L_80068b0c:
  /* 0x80068B0C: JAL 0x8006B170; link is visible to the delay instruction. */
  STEP(0x80068b0c);
  known(&RA, UINT32_C(0x80068b14));
  STEP(0x80068b10);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068b0c), UINT32_C(0x8006b170)));
  /* 0x80068B14: transfer after the source delay instruction. */
  STEP(0x80068b14);
  STEP(0x80068b18);
  goto L_80068b94;
L_80068b1c:
  /* 0x80068B1C: JAL 0x8005C5E0; link is visible to the delay instruction. */
  STEP(0x80068b1c);
  known(&RA, UINT32_C(0x80068b24));
  STEP(0x80068b20);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068b1c), UINT32_C(0x8005c5e0)));
  /* 0x80068B24: transfer after the source delay instruction. */
  STEP(0x80068b24);
  STEP(0x80068b28);
  goto L_80068b94;
L_80068b2c:
  /* 0x80068B2C: JAL 0x80059F44; link is visible to the delay instruction. */
  STEP(0x80068b2c);
  known(&RA, UINT32_C(0x80068b34));
  STEP(0x80068b30);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068b2c), UINT32_C(0x80059f44)));
  /* 0x80068B34: transfer after the source delay instruction. */
  STEP(0x80068b34);
  STEP(0x80068b38);
  goto L_80068b94;
L_80068b3c:
  /* 0x80068B3C: JAL 0x8005B028; link is visible to the delay instruction. */
  STEP(0x80068b3c);
  known(&RA, UINT32_C(0x80068b44));
  STEP(0x80068b40);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068b3c), UINT32_C(0x8005b028)));
  /* 0x80068B44: transfer after the source delay instruction. */
  STEP(0x80068b44);
  STEP(0x80068b48);
  goto L_80068b94;
L_80068b4c:
  /* 0x80068B4C: JAL 0x8005C438; link is visible to the delay instruction. */
  STEP(0x80068b4c);
  known(&RA, UINT32_C(0x80068b54));
  STEP(0x80068b50);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068b4c), UINT32_C(0x8005c438)));
  /* 0x80068B54: transfer after the source delay instruction. */
  STEP(0x80068b54);
  STEP(0x80068b58);
  goto L_80068b94;
L_80068b5c:
  /* 0x80068B5C: JAL 0x80059968; link is visible to the delay instruction. */
  STEP(0x80068b5c);
  known(&RA, UINT32_C(0x80068b64));
  STEP(0x80068b60);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068b5c), UINT32_C(0x80059968)));
  /* 0x80068B64: transfer after the source delay instruction. */
  STEP(0x80068b64);
  STEP(0x80068b68);
  goto L_80068b94;
L_80068b6c:
  /* 0x80068B6C: JAL 0x8005D070; link is visible to the delay instruction. */
  STEP(0x80068b6c);
  known(&RA, UINT32_C(0x80068b74));
  STEP(0x80068b70);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068b6c), UINT32_C(0x8005d070)));
  /* 0x80068B74: transfer after the source delay instruction. */
  STEP(0x80068b74);
  STEP(0x80068b78);
  goto L_80068b94;
L_80068b7c:
  /* 0x80068B7C: JAL 0x8005CF5C; link is visible to the delay instruction. */
  STEP(0x80068b7c);
  known(&RA, UINT32_C(0x80068b84));
  STEP(0x80068b80);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068b7c), UINT32_C(0x8005cf5c)));
  /* 0x80068B84: transfer after the source delay instruction. */
  STEP(0x80068b84);
  STEP(0x80068b88);
  goto L_80068b94;
L_80068b8c:
  /* 0x80068B8C: JAL 0x8005D9F0; link is visible to the delay instruction. */
  STEP(0x80068b8c);
  known(&RA, UINT32_C(0x80068b94));
  STEP(0x80068b90);
  A0 = S1;
  OK(invoke_site(r, UINT32_C(0x80068b8c), UINT32_C(0x8005d9f0)));
L_80068b94:
  /* Mark the live controller, advance callback-live loop state, then restore.
   */
  STEP(0x80068b94);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x00000004), 2, 1, UINT32_C(0x80068b94)));
  STEP(0x80068b98);
  /* 0x80068B9C: evaluate before the source delay instruction. */
  STEP(0x80068b9c);
  decided = signcond(V0, 0, &branch);
  STEP(0x80068ba0);
  V0 = bitorc(ZERO, UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x80068b9c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068bb4;
  STEP(0x80068ba4);
  known(&V1, UINT32_C(0x80100000));
  STEP(0x80068ba8);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdc4c), 4, 0, UINT32_C(0x80068ba8)));
  STEP(0x80068bac);
  STEP(0x80068bb0);
  OK(store(r, 2, 3, (int32_t)UINT32_C(0x00000028), 2, UINT32_C(0x80068bb0)));
L_80068bb4:
  STEP(0x80068bb4);
  S4 = add(S4, imm(UINT32_C(0x00000001)));
  STEP(0x80068bb8);
  V0 = compare(S4, UINT32_C(0x0000000a), 1);
  /* 0x80068BBC: evaluate before the source delay instruction. */
  STEP(0x80068bbc);
  decided = eq(V0, ZERO, &branch);
  branch = !branch;
  STEP(0x80068bc0);
  S6 = add(S6, imm(UINT32_C(0x00000004)));
  if (!decided) {
    stop(r, UINT32_C(0x80068bbc), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80068738;
  /* The epilogue reloads all ten saved words through the live SP. */
  STEP(0x80068bc4);
  OK(load(r, 31, 29, (int32_t)UINT32_C(0x00000044), 4, 0,
          UINT32_C(0x80068bc4)));
  STEP(0x80068bc8);
  OK(load(r, 30, 29, (int32_t)UINT32_C(0x00000040), 4, 0,
          UINT32_C(0x80068bc8)));
  STEP(0x80068bcc);
  OK(load(r, 23, 29, (int32_t)UINT32_C(0x0000003c), 4, 0,
          UINT32_C(0x80068bcc)));
  STEP(0x80068bd0);
  OK(load(r, 22, 29, (int32_t)UINT32_C(0x00000038), 4, 0,
          UINT32_C(0x80068bd0)));
  STEP(0x80068bd4);
  OK(load(r, 21, 29, (int32_t)UINT32_C(0x00000034), 4, 0,
          UINT32_C(0x80068bd4)));
  STEP(0x80068bd8);
  OK(load(r, 20, 29, (int32_t)UINT32_C(0x00000030), 4, 0,
          UINT32_C(0x80068bd8)));
  STEP(0x80068bdc);
  OK(load(r, 19, 29, (int32_t)UINT32_C(0x0000002c), 4, 0,
          UINT32_C(0x80068bdc)));
  STEP(0x80068be0);
  OK(load(r, 18, 29, (int32_t)UINT32_C(0x00000028), 4, 0,
          UINT32_C(0x80068be0)));
  STEP(0x80068be4);
  OK(load(r, 17, 29, (int32_t)UINT32_C(0x00000024), 4, 0,
          UINT32_C(0x80068be4)));
  STEP(0x80068be8);
  OK(load(r, 16, 29, (int32_t)UINT32_C(0x00000020), 4, 0,
          UINT32_C(0x80068be8)));
  STEP(0x80068bec);
  SP = add(SP, imm(UINT32_C(0x00000048)));
  /* 0x80068BF0: capture the jump target before its NOP delay. */
  STEP(0x80068bf0);
  jump_target = RA;
  STEP(0x80068bf4);
  if (jump_target.known_mask != 15) {
    stop(r, UINT32_C(0x80068bf0), jump_target.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  p->restored_return_address = jump_target;
  p->completed = 1;
  stop(r, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
