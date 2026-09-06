#include "game_ball_actor_contact.h"

#include <string.h>

typedef Nba97GameBallActorContactWord Word;
typedef struct Run {
  Nba97GameBallActorContactContext *c;
  Nba97GameBallActorContactProgress *p;
  Nba97GameBallActorContactMachine m;
} Run;

#define R(n) (r->m.registers.gpr[(n)])
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
static int valid_machine(const Nba97GameBallActorContactMachine *m) {
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
    Nba97GameBallActorContactAccess *e = &r->c->access_journal[n];
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
static uint8_t call_kind(uint32_t e) {
  switch (e) {
  case 0x8007066c:
    return 1;
  case 0x800601b8:
    return 2;
  case 0x80060240:
    return 3;
  case 0x80060008:
    return 4;
  case 0x8002ab70:
    return 5;
  case 0x800581c0:
    return 6;
  case 0x80058120:
    return 7;
  case 0x80029258:
    return 8;
  case 0x800295c8:
    return 9;
  case 0x80029590:
    return 10;
  case 0x8007059c:
    return 11;
  case 0x8005d140:
    return 12;
  case 0x80058260:
    return 13;
  case 0x8005bc34:
    return 14;
  case 0x800582dc:
    return 15;
  case 0x8006e7ac:
    return 16;
  case 0x8006229c:
    return 17;
  case 0x80062660:
    return 18;
  case 0x80035318:
    return 19;
  case 0x8005699c:
    return 20;
  case 0x800a5638:
    return 21;
  case 0x800aa788:
    return 22;
  case 0x800a5634:
    return 23;
  case 0x8005828c:
    return 24;
  default:
    return 0;
  }
}
static uint8_t call_args(uint32_t pc, uint32_t e) {
  (void)pc;
  switch (e) {
  case 0x8007066c:
    return 2;
  case 0x800601b8:
  case 0x80060240:
    return 3;
  case 0x80060008:
    return 5;
  case 0x800581c0:
  case 0x80058120:
  case 0x80029258:
  case 0x800295c8:
  case 0x80029590:
  case 0x8007059c:
  case 0x8005d140:
  case 0x8005bc34:
  case 0x800a5638:
  case 0x800a5634:
    return 1;
  case 0x800582dc:
  case 0x8005699c:
  case 0x8006229c:
  case 0x80035318:
  case 0x800aa788:
    return 2;
  default:
    return 0;
  }
}
static int invoke_site(Run *r, uint32_t pc, uint32_t entry, uint8_t args) {
  Nba97GameBallActorContactEvent e;
  int q;
  uint8_t k = call_kind(entry);
  stop(r, pc, 0, entry);
  OK(spend(r));
  memset(&e, 0, sizeof e);
  e.pc = pc;
  e.delay_slot_pc = pc + 4;
  e.entry = entry;
  e.operation = r->p->operations;
  e.kind = k;
  e.argument_count = args;
  e.invocation = r->p->call_count[k] + 1;
  publish(r);
  if (!r->c->io)
    return NBA97_TEXT_IO_REFUSED;
  q = r->c->io(r->c->user, &r->c->memory, &e, &r->m);
  publish(r);
  if (q != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!valid_machine(&r->m))
    return NBA97_TEXT_ARGUMENT;
  r->p->callbacks_completed++;
  r->p->call_count[k]++;
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_ball_actor_contact(Nba97GameBallActorContactContext *c,
                                  Nba97GameBallActorContactProgress *p) {
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
  /* 0x800602CC..0x80060344: frame creation and inexpensive eligibility gates.
   */
  STEP(0x800602cc);
  R(29) = add(R(29), imm(UINT32_C(0xffffffc0)));
  p->frame_stack_pointer = R(29).word;
  STEP(0x800602d0);
  OK(store(r, 16, 29, (int32_t)UINT32_C(0x00000020), 4, UINT32_C(0x800602d0)));
  STEP(0x800602d4);
  known(&R(16), UINT32_C(0x80100000));
  STEP(0x800602d8);
  R(16) = add(R(16), imm(UINT32_C(0xffffdbcc)));
  STEP(0x800602dc);
  OK(store(r, 31, 29, (int32_t)UINT32_C(0x00000038), 4, UINT32_C(0x800602dc)));
  STEP(0x800602e0);
  OK(store(r, 21, 29, (int32_t)UINT32_C(0x00000034), 4, UINT32_C(0x800602e0)));
  STEP(0x800602e4);
  OK(store(r, 20, 29, (int32_t)UINT32_C(0x00000030), 4, UINT32_C(0x800602e4)));
  STEP(0x800602e8);
  OK(store(r, 19, 29, (int32_t)UINT32_C(0x0000002c), 4, UINT32_C(0x800602e8)));
  STEP(0x800602ec);
  OK(store(r, 18, 29, (int32_t)UINT32_C(0x00000028), 4, UINT32_C(0x800602ec)));
  STEP(0x800602f0);
  OK(store(r, 17, 29, (int32_t)UINT32_C(0x00000024), 4, UINT32_C(0x800602f0)));
  STEP(0x800602f4);
  OK(load(r, 2, 16, (int32_t)UINT32_C(0x00000000), 2, 1, UINT32_C(0x800602f4)));
  STEP(0x800602f8);
  R(20) = R(4);
  STEP(0x800602fc);
  R(17) = R(5);
  /* 0x80060300: capture the branch predicate before its delay instruction. */
  STEP(0x80060300);
  decided = signcond(R(2), 0, &branch);
  STEP(0x80060304);
  R(4) = R(6);
  if (!decided) {
    stop(r, UINT32_C(0x80060300), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060328;
  STEP(0x80060308);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x8006030c);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdc34), 4, 0, UINT32_C(0x8006030c)));
  STEP(0x80060310);
  STEP(0x80060314);
  OK(load(r, 3, 2, (int32_t)UINT32_C(0x000000d9), 1, 0, UINT32_C(0x80060314)));
  STEP(0x80060318);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x000000d9), 1, 0, UINT32_C(0x80060318)));
  STEP(0x8006031c);
  /* 0x80060320: capture the branch predicate before its delay instruction. */
  STEP(0x80060320);
  decided = eq(R(3), R(2), &branch);
  STEP(0x80060324);
  if (!decided) {
    stop(r, UINT32_C(0x80060320), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
L_80060328:
  /* 0x80060328..0x800603A0: timers, wrapped X window, distance, and height. */
  STEP(0x80060328);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x000000b4), 2, 1, UINT32_C(0x80060328)));
  STEP(0x8006032c);
  /* 0x80060330: capture the branch predicate before its delay instruction. */
  STEP(0x80060330);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x80060334);
  if (!decided) {
    stop(r, UINT32_C(0x80060330), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  STEP(0x80060338);
  OK(load(r, 2, 20, (int32_t)UINT32_C(0x000000b4), 2, 1, UINT32_C(0x80060338)));
  STEP(0x8006033c);
  /* 0x80060340: capture the branch predicate before its delay instruction. */
  STEP(0x80060340);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x80060344);
  if (!decided) {
    stop(r, UINT32_C(0x80060340), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  STEP(0x80060348);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x0000000c), 4, 0, UINT32_C(0x80060348)));
  STEP(0x8006034c);
  OK(load(r, 3, 20, (int32_t)UINT32_C(0x0000000c), 4, 0, UINT32_C(0x8006034c)));
  STEP(0x80060350);
  STEP(0x80060354);
  R(2) = sub(R(2), R(3));
  STEP(0x80060358);
  R(5) = shift(R(2), 8, 1, 1);
  STEP(0x8006035c);
  R(2) = add(R(5), imm(UINT32_C(0x00000020)));
  STEP(0x80060360);
  R(2) = compare(R(2), UINT32_C(0x00000041), 0);
  /* 0x80060364: capture the branch predicate before its delay instruction. */
  STEP(0x80060364);
  decided = eq(R(2), R(0), &branch);
  STEP(0x80060368);
  if (!decided) {
    stop(r, UINT32_C(0x80060364), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  /* 0x8006036C: JAL 0x8007066C; link precedes delay-slot work. */
  STEP(0x8006036c);
  known(&R(31), UINT32_C(0x80060374));
  STEP(0x80060370);
  OK(invoke_site(r, UINT32_C(0x8006036c), UINT32_C(0x8007066c),
                 call_args(UINT32_C(0x8006036c), UINT32_C(0x8007066c))));
  STEP(0x80060374);
  R(18) = R(2);
  STEP(0x80060378);
  R(2) = compare(R(18), UINT32_C(0x00000021), 1);
  /* 0x8006037C: capture the branch predicate before its delay instruction. */
  STEP(0x8006037c);
  decided = eq(R(2), R(0), &branch);
  STEP(0x80060380);
  if (!decided) {
    stop(r, UINT32_C(0x8006037c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  STEP(0x80060384);
  OK(load(r, 2, 20, (int32_t)UINT32_C(0x00000010), 4, 0, UINT32_C(0x80060384)));
  STEP(0x80060388);
  OK(load(r, 3, 17, (int32_t)UINT32_C(0x00000010), 4, 0, UINT32_C(0x80060388)));
  STEP(0x8006038c);
  STEP(0x80060390);
  R(2) = sub(R(2), R(3));
  STEP(0x80060394);
  R(21) = shift(R(2), 8, 1, 1);
  STEP(0x80060398);
  R(2) = compare(R(21), UINT32_C(0x00000051), 0);
  /* 0x8006039C: capture the branch predicate before its delay instruction. */
  STEP(0x8006039c);
  decided = eq(R(2), R(0), &branch);
  STEP(0x800603a0);
  if (!decided) {
    stop(r, UINT32_C(0x8006039c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  STEP(0x800603a4);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x800603a8);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdb58), 4, 0, UINT32_C(0x800603a8)));
  STEP(0x800603ac);
  /* 0x800603B0: capture the branch predicate before its delay instruction. */
  STEP(0x800603b0);
  decided = eq(R(2), R(0), &branch);
  STEP(0x800603b4);
  if (!decided) {
    stop(r, UINT32_C(0x800603b0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  STEP(0x800603b8);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x800603bc);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffe8c4), 2, 1, UINT32_C(0x800603bc)));
  STEP(0x800603c0);
  /* 0x800603C4: capture the branch predicate before its delay instruction. */
  STEP(0x800603c4);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x800603c8);
  if (!decided) {
    stop(r, UINT32_C(0x800603c4), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  STEP(0x800603cc);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x800603d0);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffe8cc), 2, 1, UINT32_C(0x800603d0)));
  STEP(0x800603d4);
  /* 0x800603D8: capture the branch predicate before its delay instruction. */
  STEP(0x800603d8);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x800603dc);
  if (!decided) {
    stop(r, UINT32_C(0x800603d8), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060480;
  STEP(0x800603e0);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x800603e4);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdb90), 2, 1, UINT32_C(0x800603e4)));
  STEP(0x800603e8);
  STEP(0x800603ec);
  R(2) = compare(R(3), UINT32_C(0x00000080), 1);
  /* 0x800603F0: capture the branch predicate before its delay instruction. */
  STEP(0x800603f0);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x800603f4);
  known(&R(19), 0);
  if (!decided) {
    stop(r, UINT32_C(0x800603f0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8006040c;
  STEP(0x800603f8);
  R(2) = bitorc(R(0), UINT32_C(0x00000082));
  /* 0x800603FC: capture the branch predicate before its delay instruction. */
  STEP(0x800603fc);
  decided = eq(R(3), R(2), &branch);
  STEP(0x80060400);
  R(2) = bitorc(R(0), UINT32_C(0x00000081));
  if (!decided) {
    stop(r, UINT32_C(0x800603fc), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800604ac;
  /* 0x80060404: capture the branch predicate before its delay instruction. */
  STEP(0x80060404);
  decided = eq(R(3), R(2), &branch);
  STEP(0x80060408);
  if (!decided) {
    stop(r, UINT32_C(0x80060404), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060490;
L_8006040c:
  /* 0x8006040C..0x8006047C: ordinary phase chooses offense/defense contact. */
  STEP(0x8006040c);
  OK(load(r, 3, 17, (int32_t)UINT32_C(0x000000d9), 1, 0, UINT32_C(0x8006040c)));
  STEP(0x80060410);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x80060414);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdb94), 2, 1, UINT32_C(0x80060414)));
  STEP(0x80060418);
  /* 0x8006041C: capture the branch predicate before its delay instruction. */
  STEP(0x8006041c);
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  STEP(0x80060420);
  if (!decided) {
    stop(r, UINT32_C(0x8006041c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060454;
  STEP(0x80060424);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x80060428);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdbd4), 2, 1, UINT32_C(0x80060428)));
  STEP(0x8006042c);
  /* 0x80060430: capture the branch predicate before its delay instruction. */
  STEP(0x80060430);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x80060434);
  if (!decided) {
    stop(r, UINT32_C(0x80060430), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  STEP(0x80060438);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x8006043c);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdbd2), 2, 1, UINT32_C(0x8006043c)));
  STEP(0x80060440);
  /* 0x80060444: capture the branch predicate before its delay instruction. */
  STEP(0x80060444);
  decided = signcond(R(3), 0, &branch);
  STEP(0x80060448);
  if (!decided) {
    stop(r, UINT32_C(0x80060444), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800605a4;
  /* 0x8006044C: unconditional transfer after its delay instruction. */
  STEP(0x8006044c);
  STEP(0x80060450);
  goto L_800604ec;
L_80060454:
  STEP(0x80060454);
  OK(load(r, 2, 16, (int32_t)UINT32_C(0x00000000), 2, 1, UINT32_C(0x80060454)));
  STEP(0x80060458);
  /* 0x8006045C: capture the branch predicate before its delay instruction. */
  STEP(0x8006045c);
  decided = signcond(R(2), 1, &branch);
  STEP(0x80060460);
  if (!decided) {
    stop(r, UINT32_C(0x8006045c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060630;
  STEP(0x80060464);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x80060468);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdbd6), 2, 1, UINT32_C(0x80060468)));
  STEP(0x8006046c);
  /* 0x80060470: capture the branch predicate before its delay instruction. */
  STEP(0x80060470);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x80060474);
  R(4) = R(20);
  if (!decided) {
    stop(r, UINT32_C(0x80060470), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800605ac;
  /* 0x80060478: unconditional transfer after its delay instruction. */
  STEP(0x80060478);
  STEP(0x8006047c);
  R(19) = bitorc(R(0), UINT32_C(0x00000001));
  goto L_800605ac;
L_80060480:
  /* 0x80060480..0x80060538: special and phase-82 ownership gates. */
  STEP(0x80060480);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x80060484);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffe8ca), 2, 1, UINT32_C(0x80060484)));
  /* 0x80060488: unconditional transfer after its delay instruction. */
  STEP(0x80060488);
  STEP(0x8006048c);
  goto L_800604ec;
L_80060490:
  STEP(0x80060490);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x80060494);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdbd2), 2, 1, UINT32_C(0x80060494)));
  STEP(0x80060498);
  /* 0x8006049C: capture the branch predicate before its delay instruction. */
  STEP(0x8006049c);
  decided = signcond(R(3), 0, &branch);
  STEP(0x800604a0);
  R(19) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x8006049c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800605a8;
  /* 0x800604A4: unconditional transfer after its delay instruction. */
  STEP(0x800604a4);
  STEP(0x800604a8);
  goto L_800604ec;
L_800604ac:
  STEP(0x800604ac);
  OK(load(r, 3, 17, (int32_t)UINT32_C(0x000000d9), 1, 0, UINT32_C(0x800604ac)));
  STEP(0x800604b0);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x800604b4);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffe880), 2, 1, UINT32_C(0x800604b4)));
  STEP(0x800604b8);
  /* 0x800604BC: capture the branch predicate before its delay instruction. */
  STEP(0x800604bc);
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  STEP(0x800604c0);
  R(2) = bitorc(R(0), UINT32_C(0x00000003));
  if (!decided) {
    stop(r, UINT32_C(0x800604bc), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060504;
  STEP(0x800604c4);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x800604c8);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffe884), 2, 1, UINT32_C(0x800604c8)));
  STEP(0x800604cc);
  /* 0x800604D0: capture the branch predicate before its delay instruction. */
  STEP(0x800604d0);
  decided = eq(R(2), R(0), &branch);
  STEP(0x800604d4);
  if (!decided) {
    stop(r, UINT32_C(0x800604d0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8006053c;
  STEP(0x800604d8);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x800604dc);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdbd2), 2, 1, UINT32_C(0x800604dc)));
  STEP(0x800604e0);
  /* 0x800604E4: capture the branch predicate before its delay instruction. */
  STEP(0x800604e4);
  decided = signcond(R(3), 0, &branch);
  STEP(0x800604e8);
  if (!decided) {
    stop(r, UINT32_C(0x800604e4), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8006053c;
L_800604ec:
  STEP(0x800604ec);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x00000000), 4, 0, UINT32_C(0x800604ec)));
  STEP(0x800604f0);
  /* 0x800604F4: capture the branch predicate before its delay instruction. */
  STEP(0x800604f4);
  decided = eq(R(3), R(2), &branch);
  STEP(0x800604f8);
  R(19) = bitorc(R(0), UINT32_C(0x00000002));
  if (!decided) {
    stop(r, UINT32_C(0x800604f4), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060540;
  /* 0x800604FC: unconditional transfer after its delay instruction. */
  STEP(0x800604fc);
  STEP(0x80060500);
  goto L_80060e64;
L_80060504:
  STEP(0x80060504);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x80060508);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffe884), 2, 1, UINT32_C(0x80060508)));
  STEP(0x8006050c);
  /* 0x80060510: capture the branch predicate before its delay instruction. */
  STEP(0x80060510);
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  STEP(0x80060514);
  if (!decided) {
    stop(r, UINT32_C(0x80060510), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  STEP(0x80060518);
  OK(load(r, 2, 16, (int32_t)UINT32_C(0x00000000), 2, 1, UINT32_C(0x80060518)));
  STEP(0x8006051c);
  /* 0x80060520: capture the branch predicate before its delay instruction. */
  STEP(0x80060520);
  decided = signcond(R(2), 1, &branch);
  STEP(0x80060524);
  if (!decided) {
    stop(r, UINT32_C(0x80060520), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  STEP(0x80060528);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x8006052c);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdbd2), 2, 1, UINT32_C(0x8006052c)));
  STEP(0x80060530);
  /* 0x80060534: capture the branch predicate before its delay instruction. */
  STEP(0x80060534);
  decided = signcond(R(2), 1, &branch);
  STEP(0x80060538);
  R(4) = R(20);
  if (!decided) {
    stop(r, UINT32_C(0x80060534), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800605ac;
L_8006053c:
  /* 0x8006053C..0x800605A0: forced mode 2 hand/body probes. */
  STEP(0x8006053c);
  R(19) = bitorc(R(0), UINT32_C(0x00000002));
L_80060540:
  STEP(0x80060540);
  R(4) = R(20);
  STEP(0x80060544);
  R(5) = R(17);
  /* 0x80060548: JAL 0x800601B8; link precedes delay-slot work. */
  STEP(0x80060548);
  known(&R(31), UINT32_C(0x80060550));
  STEP(0x8006054c);
  R(6) = bitorc(R(0), UINT32_C(0x00000002));
  OK(invoke_site(r, UINT32_C(0x80060548), UINT32_C(0x800601b8),
                 call_args(UINT32_C(0x80060548), UINT32_C(0x800601b8))));
  STEP(0x80060550);
  R(2) = shift(R(2), 16, 0, 0);
  /* 0x80060554: capture the branch predicate before its delay instruction. */
  STEP(0x80060554);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x80060558);
  R(4) = R(20);
  if (!decided) {
    stop(r, UINT32_C(0x80060554), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060698;
  STEP(0x8006055c);
  R(5) = R(17);
  /* 0x80060560: JAL 0x80060240; link precedes delay-slot work. */
  STEP(0x80060560);
  known(&R(31), UINT32_C(0x80060568));
  STEP(0x80060564);
  R(6) = bitorc(R(0), UINT32_C(0x00000002));
  OK(invoke_site(r, UINT32_C(0x80060560), UINT32_C(0x80060240),
                 call_args(UINT32_C(0x80060560), UINT32_C(0x80060240))));
  STEP(0x80060568);
  R(2) = shift(R(2), 16, 0, 0);
  /* 0x8006056C: capture the branch predicate before its delay instruction. */
  STEP(0x8006056c);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x80060570);
  R(4) = R(20);
  if (!decided) {
    stop(r, UINT32_C(0x8006056c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060698;
  STEP(0x80060574);
  OK(store(r, 19, 29, (int32_t)UINT32_C(0x00000010), 4, UINT32_C(0x80060574)));
  STEP(0x80060578);
  R(5) = R(17);
  STEP(0x8006057c);
  R(6) = shift(R(21), 16, 0, 0);
  STEP(0x80060580);
  R(6) = shift(R(6), 16, 1, 1);
  STEP(0x80060584);
  R(7) = shift(R(18), 16, 0, 0);
  /* 0x80060588: JAL 0x80060008; link precedes delay-slot work. */
  STEP(0x80060588);
  known(&R(31), UINT32_C(0x80060590));
  STEP(0x8006058c);
  R(7) = shift(R(7), 16, 1, 1);
  OK(invoke_site(r, UINT32_C(0x80060588), UINT32_C(0x80060008),
                 call_args(UINT32_C(0x80060588), UINT32_C(0x80060008))));
  STEP(0x80060590);
  R(2) = shift(R(2), 16, 0, 0);
  /* 0x80060594: capture the branch predicate before its delay instruction. */
  STEP(0x80060594);
  decided = eq(R(2), R(0), &branch);
  STEP(0x80060598);
  if (!decided) {
    stop(r, UINT32_C(0x80060594), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  /* 0x8006059C: unconditional transfer after its delay instruction. */
  STEP(0x8006059c);
  STEP(0x800605a0);
  goto L_80060698;
L_800605a4:
  /* 0x800605A4..0x8006062C: mode 1 probes retain signed low-half results. */
  STEP(0x800605a4);
  R(19) = bitorc(R(0), UINT32_C(0x00000001));
L_800605a8:
  STEP(0x800605a8);
  R(4) = R(20);
L_800605ac:
  STEP(0x800605ac);
  R(5) = R(17);
  /* 0x800605B0: JAL 0x800601B8; link precedes delay-slot work. */
  STEP(0x800605b0);
  known(&R(31), UINT32_C(0x800605b8));
  STEP(0x800605b4);
  R(6) = R(19);
  OK(invoke_site(r, UINT32_C(0x800605b0), UINT32_C(0x800601b8),
                 call_args(UINT32_C(0x800605b0), UINT32_C(0x800601b8))));
  STEP(0x800605b8);
  R(2) = shift(R(2), 16, 0, 0);
  STEP(0x800605bc);
  R(16) = shift(R(2), 16, 1, 1);
  /* 0x800605C0: capture the branch predicate before its delay instruction. */
  STEP(0x800605c0);
  decided = signcond(R(16), 0, &branch);
  STEP(0x800605c4);
  if (!decided) {
    stop(r, UINT32_C(0x800605c0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060a10;
  /* 0x800605C8: capture the branch predicate before its delay instruction. */
  STEP(0x800605c8);
  decided = eq(R(16), R(0), &branch);
  branch = !branch;
  STEP(0x800605cc);
  R(4) = R(20);
  if (!decided) {
    stop(r, UINT32_C(0x800605c8), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060698;
  STEP(0x800605d0);
  R(5) = R(17);
  /* 0x800605D4: JAL 0x80060240; link precedes delay-slot work. */
  STEP(0x800605d4);
  known(&R(31), UINT32_C(0x800605dc));
  STEP(0x800605d8);
  R(6) = R(19);
  OK(invoke_site(r, UINT32_C(0x800605d4), UINT32_C(0x80060240),
                 call_args(UINT32_C(0x800605d4), UINT32_C(0x80060240))));
  STEP(0x800605dc);
  R(2) = shift(R(2), 16, 0, 0);
  STEP(0x800605e0);
  R(16) = shift(R(2), 16, 1, 1);
  /* 0x800605E4: capture the branch predicate before its delay instruction. */
  STEP(0x800605e4);
  decided = signcond(R(16), 0, &branch);
  STEP(0x800605e8);
  if (!decided) {
    stop(r, UINT32_C(0x800605e4), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060a10;
  /* 0x800605EC: capture the branch predicate before its delay instruction. */
  STEP(0x800605ec);
  decided = eq(R(16), R(0), &branch);
  branch = !branch;
  STEP(0x800605f0);
  R(4) = R(20);
  if (!decided) {
    stop(r, UINT32_C(0x800605ec), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060698;
  STEP(0x800605f4);
  OK(store(r, 19, 29, (int32_t)UINT32_C(0x00000010), 4, UINT32_C(0x800605f4)));
  STEP(0x800605f8);
  R(5) = R(17);
  STEP(0x800605fc);
  R(6) = shift(R(21), 16, 0, 0);
  STEP(0x80060600);
  R(6) = shift(R(6), 16, 1, 1);
  STEP(0x80060604);
  R(7) = shift(R(18), 16, 0, 0);
  /* 0x80060608: JAL 0x80060008; link precedes delay-slot work. */
  STEP(0x80060608);
  known(&R(31), UINT32_C(0x80060610));
  STEP(0x8006060c);
  R(7) = shift(R(7), 16, 1, 1);
  OK(invoke_site(r, UINT32_C(0x80060608), UINT32_C(0x80060008),
                 call_args(UINT32_C(0x80060608), UINT32_C(0x80060008))));
  STEP(0x80060610);
  R(2) = shift(R(2), 16, 0, 0);
  STEP(0x80060614);
  R(16) = shift(R(2), 16, 1, 1);
  /* 0x80060618: capture the branch predicate before its delay instruction. */
  STEP(0x80060618);
  decided = signcond(R(16), 0, &branch);
  STEP(0x8006061c);
  if (!decided) {
    stop(r, UINT32_C(0x80060618), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060a10;
  /* 0x80060620: capture the branch predicate before its delay instruction. */
  STEP(0x80060620);
  decided = eq(R(16), R(0), &branch);
  STEP(0x80060624);
  if (!decided) {
    stop(r, UINT32_C(0x80060620), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  /* 0x80060628: unconditional transfer after its delay instruction. */
  STEP(0x80060628);
  STEP(0x8006062c);
  goto L_80060698;
L_80060630:
  /* 0x80060630..0x80060694: final alternate-hand and body probes. */
  STEP(0x80060630);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x000000e4), 2, 1, UINT32_C(0x80060630)));
  STEP(0x80060634);
  /* 0x80060638: capture the branch predicate before its delay instruction. */
  STEP(0x80060638);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x8006063c);
  R(2) = bitorc(R(0), UINT32_C(0x0000002a));
  if (!decided) {
    stop(r, UINT32_C(0x80060638), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  STEP(0x80060640);
  OK(load(r, 3, 17, (int32_t)UINT32_C(0x00000046), 2, 0, UINT32_C(0x80060640)));
  STEP(0x80060644);
  /* 0x80060648: capture the branch predicate before its delay instruction. */
  STEP(0x80060648);
  decided = eq(R(3), R(2), &branch);
  STEP(0x8006064c);
  R(4) = R(20);
  if (!decided) {
    stop(r, UINT32_C(0x80060648), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  STEP(0x80060650);
  R(5) = R(17);
  /* 0x80060654: JAL 0x800601B8; link precedes delay-slot work. */
  STEP(0x80060654);
  known(&R(31), UINT32_C(0x8006065c));
  STEP(0x80060658);
  known(&R(6), 0);
  OK(invoke_site(r, UINT32_C(0x80060654), UINT32_C(0x800601b8),
                 call_args(UINT32_C(0x80060654), UINT32_C(0x800601b8))));
  STEP(0x8006065c);
  R(2) = shift(R(2), 16, 0, 0);
  STEP(0x80060660);
  R(16) = shift(R(2), 16, 1, 1);
  /* 0x80060664: capture the branch predicate before its delay instruction. */
  STEP(0x80060664);
  decided = signcond(R(16), 0, &branch);
  STEP(0x80060668);
  if (!decided) {
    stop(r, UINT32_C(0x80060664), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060a10;
  /* 0x8006066C: capture the branch predicate before its delay instruction. */
  STEP(0x8006066c);
  decided = eq(R(16), R(0), &branch);
  branch = !branch;
  STEP(0x80060670);
  R(4) = R(20);
  if (!decided) {
    stop(r, UINT32_C(0x8006066c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060698;
  STEP(0x80060674);
  R(5) = R(17);
  /* 0x80060678: JAL 0x80060240; link precedes delay-slot work. */
  STEP(0x80060678);
  known(&R(31), UINT32_C(0x80060680));
  STEP(0x8006067c);
  known(&R(6), 0);
  OK(invoke_site(r, UINT32_C(0x80060678), UINT32_C(0x80060240),
                 call_args(UINT32_C(0x80060678), UINT32_C(0x80060240))));
  STEP(0x80060680);
  R(2) = shift(R(2), 16, 0, 0);
  STEP(0x80060684);
  R(16) = shift(R(2), 16, 1, 1);
  /* 0x80060688: capture the branch predicate before its delay instruction. */
  STEP(0x80060688);
  decided = signcond(R(16), 0, &branch);
  STEP(0x8006068c);
  if (!decided) {
    stop(r, UINT32_C(0x80060688), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060a10;
  /* 0x80060690: capture the branch predicate before its delay instruction. */
  STEP(0x80060690);
  decided = eq(R(16), R(0), &branch);
  STEP(0x80060694);
  if (!decided) {
    stop(r, UINT32_C(0x80060690), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
L_80060698:
  /* 0x80060698..0x80060898: accepted contact prelude and control transfer. */
  STEP(0x80060698);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x8006069c);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffe8cc), 2, 1, UINT32_C(0x8006069c)));
  STEP(0x800606a0);
  /* 0x800606A4: capture the branch predicate before its delay instruction. */
  STEP(0x800606a4);
  decided = eq(R(2), R(0), &branch);
  STEP(0x800606a8);
  if (!decided) {
    stop(r, UINT32_C(0x800606a4), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800606ec;
  STEP(0x800606ac);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x800606b0);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffe8c4), 2, 1, UINT32_C(0x800606b0)));
  STEP(0x800606b4);
  /* 0x800606B8: capture the branch predicate before its delay instruction. */
  STEP(0x800606b8);
  decided = eq(R(2), R(0), &branch);
  STEP(0x800606bc);
  if (!decided) {
    stop(r, UINT32_C(0x800606b8), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800606ec;
  STEP(0x800606c0);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x800606c4);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffe8ca), 2, 1, UINT32_C(0x800606c4)));
  STEP(0x800606c8);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x00000000), 4, 0, UINT32_C(0x800606c8)));
  STEP(0x800606cc);
  /* 0x800606D0: capture the branch predicate before its delay instruction. */
  STEP(0x800606d0);
  decided = eq(R(3), R(2), &branch);
  STEP(0x800606d4);
  if (!decided) {
    stop(r, UINT32_C(0x800606d0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800606ec;
  /* 0x800606D8: JAL 0x8002AB70; link precedes delay-slot work. */
  STEP(0x800606d8);
  known(&R(31), UINT32_C(0x800606e0));
  STEP(0x800606dc);
  OK(invoke_site(r, UINT32_C(0x800606d8), UINT32_C(0x8002ab70),
                 call_args(UINT32_C(0x800606d8), UINT32_C(0x8002ab70))));
  STEP(0x800606e0);
  R(2) = bitandc(R(2), UINT32_C(0x00000018));
  /* 0x800606E4: capture the branch predicate before its delay instruction. */
  STEP(0x800606e4);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x800606e8);
  if (!decided) {
    stop(r, UINT32_C(0x800606e4), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
L_800606ec:
  STEP(0x800606ec);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x800606f0);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdc30), 2, 0, UINT32_C(0x800606f0)));
  STEP(0x800606f4);
  /* 0x800606F8: capture the branch predicate before its delay instruction. */
  STEP(0x800606f8);
  decided = eq(R(2), R(0), &branch);
  STEP(0x800606fc);
  if (!decided) {
    stop(r, UINT32_C(0x800606f8), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060710;
  /* 0x80060700: JAL 0x800581C0; link precedes delay-slot work. */
  STEP(0x80060700);
  known(&R(31), UINT32_C(0x80060708));
  STEP(0x80060704);
  known(&R(4), 0);
  OK(invoke_site(r, UINT32_C(0x80060700), UINT32_C(0x800581c0),
                 call_args(UINT32_C(0x80060700), UINT32_C(0x800581c0))));
  /* 0x80060708: unconditional transfer after its delay instruction. */
  STEP(0x80060708);
  STEP(0x8006070c);
  goto L_80060718;
L_80060710:
  /* 0x80060710: JAL 0x80058120; link precedes delay-slot work. */
  STEP(0x80060710);
  known(&R(31), UINT32_C(0x80060718));
  STEP(0x80060714);
  known(&R(4), 0);
  OK(invoke_site(r, UINT32_C(0x80060710), UINT32_C(0x80058120),
                 call_args(UINT32_C(0x80060710), UINT32_C(0x80058120))));
L_80060718:
  STEP(0x80060718);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x8006071c);
  OK(store(r, 0, 1, (int32_t)UINT32_C(0xffffdb84), 2, UINT32_C(0x8006071c)));
  STEP(0x80060720);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x000000d9), 1, 0, UINT32_C(0x80060720)));
  STEP(0x80060724);
  known(&R(19), UINT32_C(0x80100000));
  STEP(0x80060728);
  OK(load(r, 19, 19, (int32_t)UINT32_C(0xffffdc40), 4, 0,
          UINT32_C(0x80060728)));
  /* 0x8006072C: capture the branch predicate before its delay instruction. */
  STEP(0x8006072c);
  decided = eq(R(2), R(0), &branch);
  STEP(0x80060730);
  if (!decided) {
    stop(r, UINT32_C(0x8006072c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060744;
  STEP(0x80060734);
  known(&R(2), UINT32_C(0x80020000));
  STEP(0x80060738);
  R(2) = add(R(2), imm(UINT32_C(0xffffeeb8)));
  /* 0x8006073C: unconditional transfer after its delay instruction. */
  STEP(0x8006073c);
  STEP(0x80060740);
  goto L_8006074c;
L_80060744:
  STEP(0x80060744);
  known(&R(2), UINT32_C(0x80020000));
  STEP(0x80060748);
  R(2) = add(R(2), imm(UINT32_C(0xffffedf4)));
L_8006074c:
  STEP(0x8006074c);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060750);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffdc40), 4, UINT32_C(0x80060750)));
  STEP(0x80060754);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x80060758);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdbd2), 2, 1, UINT32_C(0x80060758)));
  STEP(0x8006075c);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x00000000), 4, 0, UINT32_C(0x8006075c)));
  STEP(0x80060760);
  /* 0x80060764: capture the branch predicate before its delay instruction. */
  STEP(0x80060764);
  decided = eq(R(2), R(3), &branch);
  branch = !branch;
  STEP(0x80060768);
  if (!decided) {
    stop(r, UINT32_C(0x80060764), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8006087c;
  /* 0x8006076C: JAL 0x80029258; link precedes delay-slot work. */
  STEP(0x8006076c);
  known(&R(31), UINT32_C(0x80060774));
  STEP(0x80060770);
  R(4) = bitorc(R(0), UINT32_C(0x00000006));
  OK(invoke_site(r, UINT32_C(0x8006076c), UINT32_C(0x80029258),
                 call_args(UINT32_C(0x8006076c), UINT32_C(0x80029258))));
  STEP(0x80060774);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x80060778);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffe8cc), 2, 1, UINT32_C(0x80060778)));
  STEP(0x8006077c);
  /* 0x80060780: capture the branch predicate before its delay instruction. */
  STEP(0x80060780);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x80060784);
  if (!decided) {
    stop(r, UINT32_C(0x80060780), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800607f0;
  /* 0x80060788: JAL 0x800295C8; link precedes delay-slot work. */
  STEP(0x80060788);
  known(&R(31), UINT32_C(0x80060790));
  STEP(0x8006078c);
  R(4) = bitorc(R(0), UINT32_C(0x00007530));
  OK(invoke_site(r, UINT32_C(0x80060788), UINT32_C(0x800295c8),
                 call_args(UINT32_C(0x80060788), UINT32_C(0x800295c8))));
  STEP(0x80060790);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x000000d9), 1, 0, UINT32_C(0x80060790)));
  STEP(0x80060794);
  /* 0x80060798: capture the branch predicate before its delay instruction. */
  STEP(0x80060798);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x8006079c);
  if (!decided) {
    stop(r, UINT32_C(0x80060798), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800607bc;
  STEP(0x800607a0);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x000000ba), 2, 0, UINT32_C(0x800607a0)));
  STEP(0x800607a4);
  STEP(0x800607a8);
  R(2) = compare(R(2), UINT32_C(0x00000078), 0);
  /* 0x800607AC: capture the branch predicate before its delay instruction. */
  STEP(0x800607ac);
  decided = eq(R(2), R(0), &branch);
  STEP(0x800607b0);
  if (!decided) {
    stop(r, UINT32_C(0x800607ac), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800607e4;
  /* 0x800607B4: unconditional transfer after its delay instruction. */
  STEP(0x800607b4);
  STEP(0x800607b8);
  R(4) = bitorc(R(0), UINT32_C(0x0000000a));
  goto L_800607e8;
L_800607bc:
  STEP(0x800607bc);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x000000ba), 2, 0, UINT32_C(0x800607bc)));
  STEP(0x800607c0);
  STEP(0x800607c4);
  R(2) = compare(R(2), UINT32_C(0x00000078), 0);
  /* 0x800607C8: capture the branch predicate before its delay instruction. */
  STEP(0x800607c8);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x800607cc);
  R(4) = bitorc(R(0), UINT32_C(0x00000009));
  if (!decided) {
    stop(r, UINT32_C(0x800607c8), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800607e8;
  /* 0x800607D0: JAL 0x8002AB70; link precedes delay-slot work. */
  STEP(0x800607d0);
  known(&R(31), UINT32_C(0x800607d8));
  STEP(0x800607d4);
  OK(invoke_site(r, UINT32_C(0x800607d0), UINT32_C(0x8002ab70),
                 call_args(UINT32_C(0x800607d0), UINT32_C(0x8002ab70))));
  STEP(0x800607d8);
  R(2) = bitandc(R(2), UINT32_C(0x00000003));
  /* 0x800607DC: capture the branch predicate before its delay instruction. */
  STEP(0x800607dc);
  decided = eq(R(2), R(0), &branch);
  STEP(0x800607e0);
  if (!decided) {
    stop(r, UINT32_C(0x800607dc), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800607f0;
L_800607e4:
  STEP(0x800607e4);
  R(4) = bitorc(R(0), UINT32_C(0x00000009));
L_800607e8:
  /* 0x800607E8: JAL 0x80029590; link precedes delay-slot work. */
  STEP(0x800607e8);
  known(&R(31), UINT32_C(0x800607f0));
  STEP(0x800607ec);
  OK(invoke_site(r, UINT32_C(0x800607e8), UINT32_C(0x80029590),
                 call_args(UINT32_C(0x800607e8), UINT32_C(0x80029590))));
L_800607f0:
  STEP(0x800607f0);
  known(&R(6), UINT32_C(0x80100000));
  STEP(0x800607f4);
  R(6) = add(R(6), imm(UINT32_C(0xffffdbd0)));
  STEP(0x800607f8);
  OK(load(r, 5, 6, (int32_t)UINT32_C(0x00000000), 2, 1, UINT32_C(0x800607f8)));
  STEP(0x800607fc);
  /* 0x80060800: capture the branch predicate before its delay instruction. */
  STEP(0x80060800);
  decided = signcond(R(5), 0, &branch);
  STEP(0x80060804);
  R(4) = R(5);
  if (!decided) {
    stop(r, UINT32_C(0x80060800), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8006089c;
  STEP(0x80060808);
  R(2) = bitandc(R(4), UINT32_C(0x00000010));
  /* 0x8006080C: capture the branch predicate before its delay instruction. */
  STEP(0x8006080c);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x80060810);
  if (!decided) {
    stop(r, UINT32_C(0x8006080c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8006089c;
  STEP(0x80060814);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x00000004), 2, 1, UINT32_C(0x80060814)));
  STEP(0x80060818);
  /* 0x8006081C: capture the branch predicate before its delay instruction. */
  STEP(0x8006081c);
  decided = signcond(R(2), 1, &branch);
  STEP(0x80060820);
  R(3) = add(R(6), imm(UINT32_C(0x00000080)));
  if (!decided) {
    stop(r, UINT32_C(0x8006081c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8006089c;
  STEP(0x80060824);
  R(2) = shift(R(5), 2, 0, 0);
  STEP(0x80060828);
  R(2) = add(R(2), R(3));
  STEP(0x8006082c);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0x00000000), 4, 0, UINT32_C(0x8006082c)));
  STEP(0x80060830);
  STEP(0x80060834);
  OK(load(r, 16, 2, (int32_t)UINT32_C(0x00000026), 2, 1, UINT32_C(0x80060834)));
  STEP(0x80060838);
  OK(store(r, 4, 17, (int32_t)UINT32_C(0x00000004), 2, UINT32_C(0x80060838)));
  STEP(0x8006083c);
  OK(load(r, 2, 6, (int32_t)UINT32_C(0x00000000), 2, 1, UINT32_C(0x8006083c)));
  STEP(0x80060840);
  STEP(0x80060844);
  R(2) = shift(R(2), 2, 0, 0);
  STEP(0x80060848);
  R(2) = add(R(2), R(3));
  STEP(0x8006084c);
  OK(load(r, 3, 2, (int32_t)UINT32_C(0x00000000), 4, 0, UINT32_C(0x8006084c)));
  STEP(0x80060850);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x80060854);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdbd2), 2, 0, UINT32_C(0x80060854)));
  STEP(0x80060858);
  STEP(0x8006085c);
  OK(store(r, 2, 3, (int32_t)UINT32_C(0x00000026), 2, UINT32_C(0x8006085c)));
  STEP(0x80060860);
  R(2) = shift(R(16), 2, 0, 0);
  STEP(0x80060864);
  known(&R(1), UINT32_C(0x80020000));
  STEP(0x80060868);
  R(1) = add(R(1), R(2));
  STEP(0x8006086c);
  OK(load(r, 3, 1, (int32_t)UINT32_C(0x00000bec), 4, 0, UINT32_C(0x8006086c)));
  STEP(0x80060870);
  known(&R(2), UINT32_C(0xffffffff));
  /* 0x80060874: unconditional transfer after its delay instruction. */
  STEP(0x80060874);
  STEP(0x80060878);
  OK(store(r, 2, 3, (int32_t)UINT32_C(0x00000004), 2, UINT32_C(0x80060878)));
  goto L_8006089c;
L_8006087c:
  STEP(0x8006087c);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x00000004), 2, 1, UINT32_C(0x8006087c)));
  STEP(0x80060880);
  /* 0x80060884: capture the branch predicate before its delay instruction. */
  STEP(0x80060884);
  decided = signcond(R(2), 1, &branch);
  STEP(0x80060888);
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x80060884), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8006089c;
  STEP(0x8006088c);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060890);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffdb84), 2, UINT32_C(0x80060890)));
  /* 0x80060894: JAL 0x8007059C; link precedes delay-slot work. */
  STEP(0x80060894);
  known(&R(31), UINT32_C(0x8006089c));
  STEP(0x80060898);
  R(4) = R(17);
  OK(invoke_site(r, UINT32_C(0x80060894), UINT32_C(0x8007059c),
                 call_args(UINT32_C(0x80060894), UINT32_C(0x8007059c))));
L_8006089c:
  /* 0x8006089C..0x80060914: acquisition result and live phase selection. */
  /* 0x8006089C: JAL 0x8005D140; link precedes delay-slot work. */
  STEP(0x8006089c);
  known(&R(31), UINT32_C(0x800608a4));
  STEP(0x800608a0);
  R(4) = R(17);
  OK(invoke_site(r, UINT32_C(0x8006089c), UINT32_C(0x8005d140),
                 call_args(UINT32_C(0x8006089c), UINT32_C(0x8005d140))));
  STEP(0x800608a4);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x800608a8);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdbd2), 2, 1, UINT32_C(0x800608a8)));
  STEP(0x800608ac);
  R(16) = R(2);
  STEP(0x800608b0);
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  STEP(0x800608b4);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x800608b8);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffdb88), 2, UINT32_C(0x800608b8)));
  /* 0x800608BC: capture the branch predicate before its delay instruction. */
  STEP(0x800608bc);
  decided = signcond(R(3), 1, &branch);
  STEP(0x800608c0);
  R(2) = bitorc(R(0), UINT32_C(0x00000081));
  if (!decided) {
    stop(r, UINT32_C(0x800608bc), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800608f4;
  STEP(0x800608c4);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x800608c8);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdb90), 2, 1, UINT32_C(0x800608c8)));
  STEP(0x800608cc);
  /* 0x800608D0: capture the branch predicate before its delay instruction. */
  STEP(0x800608d0);
  decided = eq(R(3), R(2), &branch);
  STEP(0x800608d4);
  R(2) = bitorc(R(0), UINT32_C(0x00000082));
  if (!decided) {
    stop(r, UINT32_C(0x800608d0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060928;
  /* 0x800608D8: capture the branch predicate before its delay instruction. */
  STEP(0x800608d8);
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  STEP(0x800608dc);
  if (!decided) {
    stop(r, UINT32_C(0x800608d8), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060910;
  STEP(0x800608e0);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x800608e4);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffe884), 2, 1, UINT32_C(0x800608e4)));
  STEP(0x800608e8);
  /* 0x800608EC: capture the branch predicate before its delay instruction. */
  STEP(0x800608ec);
  decided = eq(R(2), R(0), &branch);
  STEP(0x800608f0);
  if (!decided) {
    stop(r, UINT32_C(0x800608ec), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060918;
L_800608f4:
  STEP(0x800608f4);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x800608f8);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdb90), 2, 1, UINT32_C(0x800608f8)));
  STEP(0x800608fc);
  R(2) = bitorc(R(0), UINT32_C(0x00000082));
  /* 0x80060900: capture the branch predicate before its delay instruction. */
  STEP(0x80060900);
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  STEP(0x80060904);
  R(2) = bitorc(R(0), UINT32_C(0x00000003));
  if (!decided) {
    stop(r, UINT32_C(0x80060900), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060910;
  STEP(0x80060908);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x8006090c);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffe86e), 2, UINT32_C(0x8006090c)));
L_80060910:
  STEP(0x80060910);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060914);
  OK(store(r, 0, 1, (int32_t)UINT32_C(0xffffdb90), 2, UINT32_C(0x80060914)));
L_80060918:
  /* 0x80060918: JAL 0x80058260; link precedes delay-slot work. */
  STEP(0x80060918);
  known(&R(31), UINT32_C(0x80060920));
  STEP(0x8006091c);
  OK(invoke_site(r, UINT32_C(0x80060918), UINT32_C(0x80058260),
                 call_args(UINT32_C(0x80060918), UINT32_C(0x80058260))));
  /* 0x80060920: unconditional transfer after its delay instruction. */
  STEP(0x80060920);
  STEP(0x80060924);
  OK(store(r, 0, 20, (int32_t)UINT32_C(0x00000018), 2, UINT32_C(0x80060924)));
  goto L_80060cb8;
L_80060928:
  /* 0x80060928..0x800609E4: phase-81 winner, duplicate timers, release, AF. */
  STEP(0x80060928);
  known(&R(4), UINT32_C(0x80020000));
  STEP(0x8006092c);
  OK(load(r, 4, 4, (int32_t)UINT32_C(0x00000bec), 4, 0, UINT32_C(0x8006092c)));
  STEP(0x80060930);
  known(&R(3), UINT32_C(0x80020000));
  STEP(0x80060934);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0x00000c00), 4, 0, UINT32_C(0x80060934)));
  STEP(0x80060938);
  R(2) = R(16);
  STEP(0x8006093c);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060940);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffe880), 2, UINT32_C(0x80060940)));
  STEP(0x80060944);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060948);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffdb96), 2, UINT32_C(0x80060948)));
  STEP(0x8006094c);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060950);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffdb72), 2, UINT32_C(0x80060950)));
  STEP(0x80060954);
  R(2) = bitorc(R(0), UINT32_C(0x0000001e));
  STEP(0x80060958);
  OK(store(r, 2, 3, (int32_t)UINT32_C(0x000000b4), 2, UINT32_C(0x80060958)));
  STEP(0x8006095c);
  OK(store(r, 2, 4, (int32_t)UINT32_C(0x000000b4), 2, UINT32_C(0x8006095c)));
  STEP(0x80060960);
  OK(store(r, 2, 3, (int32_t)UINT32_C(0x000000b4), 2, UINT32_C(0x80060960)));
  STEP(0x80060964);
  OK(store(r, 2, 4, (int32_t)UINT32_C(0x000000b4), 2, UINT32_C(0x80060964)));
  STEP(0x80060968);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x00000004), 2, 0, UINT32_C(0x80060968)));
  STEP(0x8006096c);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060970);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffdbd0), 2, UINT32_C(0x80060970)));
  /* 0x80060974: JAL 0x8005BC34; link precedes delay-slot work. */
  STEP(0x80060974);
  known(&R(31), UINT32_C(0x8006097c));
  STEP(0x80060978);
  R(4) = R(17);
  OK(invoke_site(r, UINT32_C(0x80060974), UINT32_C(0x8005bc34),
                 call_args(UINT32_C(0x80060974), UINT32_C(0x8005bc34))));
  /* 0x8006097C: capture the branch predicate before its delay instruction. */
  STEP(0x8006097c);
  decided = eq(R(16), R(0), &branch);
  branch = !branch;
  STEP(0x80060980);
  R(4) = bitorc(R(0), UINT32_C(0x0000000a));
  if (!decided) {
    stop(r, UINT32_C(0x8006097c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060988;
  STEP(0x80060984);
  R(4) = bitorc(R(0), UINT32_C(0x0000000c));
L_80060988:
  /* 0x80060988: JAL 0x80029590; link precedes delay-slot work. */
  STEP(0x80060988);
  known(&R(31), UINT32_C(0x80060990));
  STEP(0x8006098c);
  R(16) = bitorc(R(0), UINT32_C(0x00000027));
  OK(invoke_site(r, UINT32_C(0x80060988), UINT32_C(0x80029590),
                 call_args(UINT32_C(0x80060988), UINT32_C(0x80029590))));
  STEP(0x80060990);
  known(&R(2), UINT32_C(0x80020000));
  STEP(0x80060994);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0x00000bec), 4, 0, UINT32_C(0x80060994)));
  STEP(0x80060998);
  STEP(0x8006099c);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0x00000046), 2, 0, UINT32_C(0x8006099c)));
  STEP(0x800609a0);
  /* 0x800609A4: capture the branch predicate before its delay instruction. */
  STEP(0x800609a4);
  decided = eq(R(2), R(16), &branch);
  branch = !branch;
  STEP(0x800609a8);
  if (!decided) {
    stop(r, UINT32_C(0x800609a4), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800609bc;
  STEP(0x800609ac);
  known(&R(4), UINT32_C(0x80020000));
  STEP(0x800609b0);
  OK(load(r, 4, 4, (int32_t)UINT32_C(0x00000bec), 4, 0, UINT32_C(0x800609b0)));
  /* 0x800609B4: JAL 0x800582DC; link precedes delay-slot work. */
  STEP(0x800609b4);
  known(&R(31), UINT32_C(0x800609bc));
  STEP(0x800609b8);
  R(5) = bitorc(R(0), UINT32_C(0x00000001));
  OK(invoke_site(r, UINT32_C(0x800609b4), UINT32_C(0x800582dc),
                 call_args(UINT32_C(0x800609b4), UINT32_C(0x800582dc))));
L_800609bc:
  STEP(0x800609bc);
  known(&R(2), UINT32_C(0x80020000));
  STEP(0x800609c0);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0x00000c00), 4, 0, UINT32_C(0x800609c0)));
  STEP(0x800609c4);
  STEP(0x800609c8);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0x00000046), 2, 0, UINT32_C(0x800609c8)));
  STEP(0x800609cc);
  /* 0x800609D0: capture the branch predicate before its delay instruction. */
  STEP(0x800609d0);
  decided = eq(R(2), R(16), &branch);
  branch = !branch;
  STEP(0x800609d4);
  if (!decided) {
    stop(r, UINT32_C(0x800609d0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800609e8;
  STEP(0x800609d8);
  known(&R(4), UINT32_C(0x80020000));
  STEP(0x800609dc);
  OK(load(r, 4, 4, (int32_t)UINT32_C(0x00000c00), 4, 0, UINT32_C(0x800609dc)));
  /* 0x800609E0: JAL 0x800582DC; link precedes delay-slot work. */
  STEP(0x800609e0);
  known(&R(31), UINT32_C(0x800609e8));
  STEP(0x800609e4);
  R(5) = bitorc(R(0), UINT32_C(0x00000001));
  OK(invoke_site(r, UINT32_C(0x800609e0), UINT32_C(0x800582dc),
                 call_args(UINT32_C(0x800609e0), UINT32_C(0x800582dc))));
L_800609e8:
  STEP(0x800609e8);
  known(&R(19), UINT32_C(0x80100000));
  STEP(0x800609ec);
  OK(load(r, 19, 19, (int32_t)UINT32_C(0xffffdc40), 4, 0,
          UINT32_C(0x800609ec)));
  STEP(0x800609f0);
  R(2) = bitorc(R(0), UINT32_C(0x00000082));
  STEP(0x800609f4);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x800609f8);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffdb90), 2, UINT32_C(0x800609f8)));
  STEP(0x800609fc);
  R(2) = bitorc(R(0), UINT32_C(0x00000003));
  STEP(0x80060a00);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060a04);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffe884), 2, UINT32_C(0x80060a04)));
  /* 0x80060A08: unconditional transfer after its delay instruction. */
  STEP(0x80060a08);
  STEP(0x80060a0c);
  goto L_80060cb8;
L_80060a10:
  /* 0x80060A10..0x80060B44: negative contact releases prior possession. */
  STEP(0x80060a10);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x80060a14);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffe8cc), 2, 1, UINT32_C(0x80060a14)));
  STEP(0x80060a18);
  /* 0x80060A1C: capture the branch predicate before its delay instruction. */
  STEP(0x80060a1c);
  decided = eq(R(2), R(0), &branch);
  STEP(0x80060a20);
  if (!decided) {
    stop(r, UINT32_C(0x80060a1c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060a64;
  STEP(0x80060a24);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x80060a28);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffe8c4), 2, 1, UINT32_C(0x80060a28)));
  STEP(0x80060a2c);
  /* 0x80060A30: capture the branch predicate before its delay instruction. */
  STEP(0x80060a30);
  decided = eq(R(2), R(0), &branch);
  STEP(0x80060a34);
  if (!decided) {
    stop(r, UINT32_C(0x80060a30), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060a64;
  STEP(0x80060a38);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x80060a3c);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffe8ca), 2, 1, UINT32_C(0x80060a3c)));
  STEP(0x80060a40);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x00000000), 4, 0, UINT32_C(0x80060a40)));
  STEP(0x80060a44);
  /* 0x80060A48: capture the branch predicate before its delay instruction. */
  STEP(0x80060a48);
  decided = eq(R(3), R(2), &branch);
  STEP(0x80060a4c);
  if (!decided) {
    stop(r, UINT32_C(0x80060a48), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060a64;
  /* 0x80060A50: JAL 0x8002AB70; link precedes delay-slot work. */
  STEP(0x80060a50);
  known(&R(31), UINT32_C(0x80060a58));
  STEP(0x80060a54);
  OK(invoke_site(r, UINT32_C(0x80060a50), UINT32_C(0x8002ab70),
                 call_args(UINT32_C(0x80060a50), UINT32_C(0x8002ab70))));
  STEP(0x80060a58);
  R(2) = bitandc(R(2), UINT32_C(0x00000018));
  /* 0x80060A5C: capture the branch predicate before its delay instruction. */
  STEP(0x80060a5c);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x80060a60);
  if (!decided) {
    stop(r, UINT32_C(0x80060a5c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
L_80060a64:
  STEP(0x80060a64);
  known(&R(4), UINT32_C(0x80100000));
  STEP(0x80060a68);
  R(4) = add(R(4), imm(UINT32_C(0xffffdbcc)));
  STEP(0x80060a6c);
  OK(load(r, 2, 4, (int32_t)UINT32_C(0x00000000), 2, 1, UINT32_C(0x80060a6c)));
  STEP(0x80060a70);
  /* 0x80060A74: capture the branch predicate before its delay instruction. */
  STEP(0x80060a74);
  decided = signcond(R(2), 0, &branch);
  STEP(0x80060a78);
  known(&R(2), UINT32_C(0xffffffff));
  if (!decided) {
    stop(r, UINT32_C(0x80060a74), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060acc;
  STEP(0x80060a7c);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x80060a80);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdc34), 4, 0, UINT32_C(0x80060a80)));
  STEP(0x80060a84);
  OK(store(r, 2, 4, (int32_t)UINT32_C(0x00000000), 2, UINT32_C(0x80060a84)));
  STEP(0x80060a88);
  R(2) = bitorc(R(0), UINT32_C(0x0000001e));
  STEP(0x80060a8c);
  OK(store(r, 2, 3, (int32_t)UINT32_C(0x000000b4), 2, UINT32_C(0x80060a8c)));
  STEP(0x80060a90);
  known(&R(4), UINT32_C(0x80100000));
  STEP(0x80060a94);
  OK(load(r, 4, 4, (int32_t)UINT32_C(0xffffdc34), 4, 0, UINT32_C(0x80060a94)));
  STEP(0x80060a98);
  STEP(0x80060a9c);
  OK(load(r, 3, 4, (int32_t)UINT32_C(0x0000001a), 1, 0, UINT32_C(0x80060a9c)));
  STEP(0x80060aa0);
  R(2) = bitorc(R(0), UINT32_C(0x0000000c));
  /* 0x80060AA4: capture the branch predicate before its delay instruction. */
  STEP(0x80060aa4);
  decided = eq(R(3), R(2), &branch);
  STEP(0x80060aa8);
  R(2) = bitorc(R(0), UINT32_C(0x0000000e));
  if (!decided) {
    stop(r, UINT32_C(0x80060aa4), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060abc;
  /* 0x80060AAC: capture the branch predicate before its delay instruction. */
  STEP(0x80060aac);
  decided = eq(R(3), R(2), &branch);
  STEP(0x80060ab0);
  if (!decided) {
    stop(r, UINT32_C(0x80060aac), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060abc;
  /* 0x80060AB4: JAL 0x800582DC; link precedes delay-slot work. */
  STEP(0x80060ab4);
  known(&R(31), UINT32_C(0x80060abc));
  STEP(0x80060ab8);
  known(&R(5), 0);
  OK(invoke_site(r, UINT32_C(0x80060ab4), UINT32_C(0x800582dc),
                 call_args(UINT32_C(0x80060ab4), UINT32_C(0x800582dc))));
L_80060abc:
  STEP(0x80060abc);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x80060ac0);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdc48), 4, 0, UINT32_C(0x80060ac0)));
  STEP(0x80060ac4);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060ac8);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffdc34), 4, UINT32_C(0x80060ac8)));
L_80060acc:
  STEP(0x80060acc);
  R(2) = bitorc(R(0), UINT32_C(0x0000001e));
  STEP(0x80060ad0);
  OK(store(r, 2, 17, (int32_t)UINT32_C(0x000000b4), 2, UINT32_C(0x80060ad0)));
  STEP(0x80060ad4);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x80060ad8);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdc48), 4, 0, UINT32_C(0x80060ad8)));
  STEP(0x80060adc);
  R(2) = bitorc(R(0), UINT32_C(0x0000000f));
  STEP(0x80060ae0);
  OK(store(r, 2, 3, (int32_t)UINT32_C(0x000000b4), 2, UINT32_C(0x80060ae0)));
  STEP(0x80060ae4);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060ae8);
  OK(store(r, 0, 1, (int32_t)UINT32_C(0xffffdbe8), 2, UINT32_C(0x80060ae8)));
  STEP(0x80060aec);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060af0);
  OK(store(r, 0, 1, (int32_t)UINT32_C(0xffffdbb2), 2, UINT32_C(0x80060af0)));
  STEP(0x80060af4);
  OK(load(r, 4, 17, (int32_t)UINT32_C(0x000000d9), 1, 0, UINT32_C(0x80060af4)));
  STEP(0x80060af8);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x80060afc);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdb96), 2, 1, UINT32_C(0x80060afc)));
  STEP(0x80060b00);
  R(2) = bitandc(R(4), UINT32_C(0x000000ff));
  /* 0x80060B04: capture the branch predicate before its delay instruction. */
  STEP(0x80060b04);
  decided = eq(R(2), R(3), &branch);
  STEP(0x80060b08);
  if (!decided) {
    stop(r, UINT32_C(0x80060b04), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060d78;
  STEP(0x80060b0c);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060b10);
  OK(store(r, 4, 1, (int32_t)UINT32_C(0xffffdb96), 2, UINT32_C(0x80060b10)));
  STEP(0x80060b14);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x000000d9), 1, 0, UINT32_C(0x80060b14)));
  STEP(0x80060b18);
  known(&R(19), UINT32_C(0x80100000));
  STEP(0x80060b1c);
  OK(load(r, 19, 19, (int32_t)UINT32_C(0xffffdc40), 4, 0,
          UINT32_C(0x80060b1c)));
  /* 0x80060B20: capture the branch predicate before its delay instruction. */
  STEP(0x80060b20);
  decided = eq(R(2), R(0), &branch);
  STEP(0x80060b24);
  if (!decided) {
    stop(r, UINT32_C(0x80060b20), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060b48;
  STEP(0x80060b28);
  known(&R(2), UINT32_C(0x80020000));
  STEP(0x80060b2c);
  R(2) = add(R(2), imm(UINT32_C(0xffffeeb8)));
  STEP(0x80060b30);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060b34);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffdc40), 4, UINT32_C(0x80060b34)));
  /* 0x80060B38: JAL 0x800295C8; link precedes delay-slot work. */
  STEP(0x80060b38);
  known(&R(31), UINT32_C(0x80060b40));
  STEP(0x80060b3c);
  R(4) = bitorc(R(0), UINT32_C(0x00002710));
  OK(invoke_site(r, UINT32_C(0x80060b38), UINT32_C(0x800295c8),
                 call_args(UINT32_C(0x80060b38), UINT32_C(0x800295c8))));
  /* 0x80060B40: unconditional transfer after its delay instruction. */
  STEP(0x80060b40);
  STEP(0x80060b44);
  R(4) = bitorc(R(0), UINT32_C(0x00000005));
  goto L_80060b78;
L_80060b48:
  /* 0x80060B48..0x80060C48: team publication and exceptional rule response. */
  STEP(0x80060b48);
  known(&R(2), UINT32_C(0x80020000));
  STEP(0x80060b4c);
  R(2) = add(R(2), imm(UINT32_C(0xffffedf4)));
  STEP(0x80060b50);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060b54);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffdc40), 4, UINT32_C(0x80060b54)));
  /* 0x80060B58: JAL 0x8002AB70; link precedes delay-slot work. */
  STEP(0x80060b58);
  known(&R(31), UINT32_C(0x80060b60));
  STEP(0x80060b5c);
  OK(invoke_site(r, UINT32_C(0x80060b58), UINT32_C(0x8002ab70),
                 call_args(UINT32_C(0x80060b58), UINT32_C(0x8002ab70))));
  STEP(0x80060b60);
  R(2) = bitandc(R(2), UINT32_C(0x00000001));
  /* 0x80060B64: capture the branch predicate before its delay instruction. */
  STEP(0x80060b64);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x80060b68);
  if (!decided) {
    stop(r, UINT32_C(0x80060b64), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060b80;
  /* 0x80060B6C: JAL 0x800295C8; link precedes delay-slot work. */
  STEP(0x80060b6c);
  known(&R(31), UINT32_C(0x80060b74));
  STEP(0x80060b70);
  R(4) = bitorc(R(0), UINT32_C(0x00004e20));
  OK(invoke_site(r, UINT32_C(0x80060b6c), UINT32_C(0x800295c8),
                 call_args(UINT32_C(0x80060b6c), UINT32_C(0x800295c8))));
  STEP(0x80060b74);
  R(4) = bitorc(R(0), UINT32_C(0x00000006));
L_80060b78:
  /* 0x80060B78: JAL 0x80029590; link precedes delay-slot work. */
  STEP(0x80060b78);
  known(&R(31), UINT32_C(0x80060b80));
  STEP(0x80060b7c);
  OK(invoke_site(r, UINT32_C(0x80060b78), UINT32_C(0x80029590),
                 call_args(UINT32_C(0x80060b78), UINT32_C(0x80029590))));
L_80060b80:
  STEP(0x80060b80);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x80060b84);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdc40), 4, 0, UINT32_C(0x80060b84)));
  STEP(0x80060b88);
  OK(load(r, 3, 17, (int32_t)UINT32_C(0x00000000), 4, 0, UINT32_C(0x80060b88)));
  STEP(0x80060b8c);
  STEP(0x80060b90);
  OK(store(r, 3, 2, (int32_t)UINT32_C(0x00000052), 2, UINT32_C(0x80060b90)));
  STEP(0x80060b94);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x80060b98);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdbd4), 2, 1, UINT32_C(0x80060b98)));
  STEP(0x80060b9c);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060ba0);
  OK(store(r, 0, 1, (int32_t)UINT32_C(0xffffe8e0), 2, UINT32_C(0x80060ba0)));
  /* 0x80060BA4: capture the branch predicate before its delay instruction. */
  STEP(0x80060ba4);
  decided = eq(R(2), R(0), &branch);
  STEP(0x80060ba8);
  if (!decided) {
    stop(r, UINT32_C(0x80060ba4), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060d70;
  STEP(0x80060bac);
  OK(load(r, 3, 17, (int32_t)UINT32_C(0x000000d9), 1, 0, UINT32_C(0x80060bac)));
  STEP(0x80060bb0);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x80060bb4);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdb94), 2, 1, UINT32_C(0x80060bb4)));
  STEP(0x80060bb8);
  /* 0x80060BBC: capture the branch predicate before its delay instruction. */
  STEP(0x80060bbc);
  decided = eq(R(3), R(2), &branch);
  STEP(0x80060bc0);
  if (!decided) {
    stop(r, UINT32_C(0x80060bbc), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060d70;
  STEP(0x80060bc4);
  known(&R(2), UINT32_C(0x80020000));
  STEP(0x80060bc8);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0x00001d8d), 1, 0, UINT32_C(0x80060bc8)));
  STEP(0x80060bcc);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060bd0);
  OK(store(r, 0, 1, (int32_t)UINT32_C(0xffffdbca), 2, UINT32_C(0x80060bd0)));
  /* 0x80060BD4: capture the branch predicate before its delay instruction. */
  STEP(0x80060bd4);
  decided = eq(R(2), R(0), &branch);
  STEP(0x80060bd8);
  if (!decided) {
    stop(r, UINT32_C(0x80060bd4), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060cc8;
  STEP(0x80060bdc);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x80060be0);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdc48), 4, 0, UINT32_C(0x80060be0)));
  STEP(0x80060be4);
  STEP(0x80060be8);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0x00000018), 2, 1, UINT32_C(0x80060be8)));
  STEP(0x80060bec);
  /* 0x80060BF0: capture the branch predicate before its delay instruction. */
  STEP(0x80060bf0);
  decided = signcond(R(2), 1, &branch);
  STEP(0x80060bf4);
  if (!decided) {
    stop(r, UINT32_C(0x80060bf0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060cc8;
  STEP(0x80060bf8);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x80060bfc);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdbd8), 2, 1, UINT32_C(0x80060bfc)));
  STEP(0x80060c00);
  /* 0x80060C04: capture the branch predicate before its delay instruction. */
  STEP(0x80060c04);
  decided = eq(R(2), R(0), &branch);
  STEP(0x80060c08);
  if (!decided) {
    stop(r, UINT32_C(0x80060c04), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060cc8;
  STEP(0x80060c0c);
  known(&R(2), UINT32_C(0x80100000));
  STEP(0x80060c10);
  OK(load(r, 2, 2, (int32_t)UINT32_C(0xffffdbd6), 2, 1, UINT32_C(0x80060c10)));
  STEP(0x80060c14);
  STEP(0x80060c18);
  R(2) = compare(R(2), UINT32_C(0x00000005), 1);
  /* 0x80060C1C: capture the branch predicate before its delay instruction. */
  STEP(0x80060c1c);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x80060c20);
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x80060c1c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060cc8;
  STEP(0x80060c24);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060c28);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffdbda), 2, UINT32_C(0x80060c28)));
  /* 0x80060C2C: JAL 0x8006E7AC; link precedes delay-slot work. */
  STEP(0x80060c2c);
  known(&R(31), UINT32_C(0x80060c34));
  STEP(0x80060c30);
  R(16) = bitorc(R(0), UINT32_C(0x00000007));
  OK(invoke_site(r, UINT32_C(0x80060c2c), UINT32_C(0x8006e7ac),
                 call_args(UINT32_C(0x80060c2c), UINT32_C(0x8006e7ac))));
  STEP(0x80060c34);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x80060c38);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffdbd8), 2, 1, UINT32_C(0x80060c38)));
  STEP(0x80060c3c);
  R(2) = bitorc(R(0), UINT32_C(0x00000002));
  /* 0x80060C40: capture the branch predicate before its delay instruction. */
  STEP(0x80060c40);
  decided = eq(R(3), R(2), &branch);
  STEP(0x80060c44);
  if (!decided) {
    stop(r, UINT32_C(0x80060c40), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060c4c;
  STEP(0x80060c48);
  R(16) = bitorc(R(0), UINT32_C(0x00000008));
L_80060c4c:
  /* 0x80060C4C..0x80060D74: turnovers, capped player stat, controller wrap. */
  STEP(0x80060c4c);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x00000000), 4, 0, UINT32_C(0x80060c4c)));
  STEP(0x80060c50);
  STEP(0x80060c54);
  R(2) = compare(R(2), UINT32_C(0x00000005), 1);
  /* 0x80060C58: capture the branch predicate before its delay instruction. */
  STEP(0x80060c58);
  decided = eq(R(2), R(0), &branch);
  STEP(0x80060c5c);
  if (!decided) {
    stop(r, UINT32_C(0x80060c58), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060c70;
  /* 0x80060C60: JAL 0x80029590; link precedes delay-slot work. */
  STEP(0x80060c60);
  known(&R(31), UINT32_C(0x80060c68));
  STEP(0x80060c64);
  R(4) = bitorc(R(0), UINT32_C(0x0000000b));
  OK(invoke_site(r, UINT32_C(0x80060c60), UINT32_C(0x80029590),
                 call_args(UINT32_C(0x80060c60), UINT32_C(0x80029590))));
  /* 0x80060C68: unconditional transfer after its delay instruction. */
  STEP(0x80060c68);
  STEP(0x80060c6c);
  R(4) = bitorc(R(0), UINT32_C(0x00001388));
  goto L_80060c7c;
L_80060c70:
  /* 0x80060C70: JAL 0x80029590; link precedes delay-slot work. */
  STEP(0x80060c70);
  known(&R(31), UINT32_C(0x80060c78));
  STEP(0x80060c74);
  R(4) = bitorc(R(0), UINT32_C(0x0000000c));
  OK(invoke_site(r, UINT32_C(0x80060c70), UINT32_C(0x80029590),
                 call_args(UINT32_C(0x80060c70), UINT32_C(0x80029590))));
  STEP(0x80060c78);
  R(4) = bitorc(R(0), UINT32_C(0x00004e20));
L_80060c7c:
  /* 0x80060C7C: JAL 0x800295C8; link precedes delay-slot work. */
  STEP(0x80060c7c);
  known(&R(31), UINT32_C(0x80060c84));
  STEP(0x80060c80);
  OK(invoke_site(r, UINT32_C(0x80060c7c), UINT32_C(0x800295c8),
                 call_args(UINT32_C(0x80060c7c), UINT32_C(0x800295c8))));
  STEP(0x80060c84);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060c88);
  OK(store(r, 0, 1, (int32_t)UINT32_C(0xffffdbd8), 2, UINT32_C(0x80060c88)));
  STEP(0x80060c8c);
  OK(load(r, 5, 17, (int32_t)UINT32_C(0x00000000), 4, 0, UINT32_C(0x80060c8c)));
  /* 0x80060C90: JAL 0x8006229C; link precedes delay-slot work. */
  STEP(0x80060c90);
  known(&R(31), UINT32_C(0x80060c98));
  STEP(0x80060c94);
  R(4) = R(16);
  OK(invoke_site(r, UINT32_C(0x80060c90), UINT32_C(0x8006229c),
                 call_args(UINT32_C(0x80060c90), UINT32_C(0x8006229c))));
  /* 0x80060C98: JAL 0x80062660; link precedes delay-slot work. */
  STEP(0x80060c98);
  known(&R(31), UINT32_C(0x80060ca0));
  STEP(0x80060c9c);
  OK(invoke_site(r, UINT32_C(0x80060c98), UINT32_C(0x80062660),
                 call_args(UINT32_C(0x80060c98), UINT32_C(0x80062660))));
  STEP(0x80060ca0);
  R(4) = bitorc(R(0), UINT32_C(0x00000002));
  STEP(0x80060ca4);
  R(2) = bitorc(R(0), UINT32_C(0x00000007));
  STEP(0x80060ca8);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060cac);
  OK(store(r, 2, 1, (int32_t)UINT32_C(0xffffe882), 2, UINT32_C(0x80060cac)));
  /* 0x80060CB0: JAL 0x80035318; link precedes delay-slot work. */
  STEP(0x80060cb0);
  known(&R(31), UINT32_C(0x80060cb8));
  STEP(0x80060cb4);
  known(&R(5), UINT32_C(0xffffffff));
  OK(invoke_site(r, UINT32_C(0x80060cb0), UINT32_C(0x80035318),
                 call_args(UINT32_C(0x80060cb0), UINT32_C(0x80035318))));
L_80060cb8:
  STEP(0x80060cb8);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060cbc);
  OK(store(r, 19, 1, (int32_t)UINT32_C(0xffffdc40), 4, UINT32_C(0x80060cbc)));
  /* 0x80060CC0: unconditional transfer after its delay instruction. */
  STEP(0x80060cc0);
  STEP(0x80060cc4);
  goto L_80060e64;
L_80060cc8:
  STEP(0x80060cc8);
  known(&R(18), UINT32_C(0x80100000));
  STEP(0x80060ccc);
  R(18) = add(R(18), imm(UINT32_C(0xffffdbd8)));
  STEP(0x80060cd0);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060cd4);
  OK(store(r, 0, 1, (int32_t)UINT32_C(0xffffdbda), 2, UINT32_C(0x80060cd4)));
  STEP(0x80060cd8);
  OK(store(r, 0, 18, (int32_t)UINT32_C(0x00000000), 2, UINT32_C(0x80060cd8)));
  STEP(0x80060cdc);
  OK(load(r, 2, 17, (int32_t)UINT32_C(0x000000df), 1, 0, UINT32_C(0x80060cdc)));
  STEP(0x80060ce0);
  OK(load(r, 16, 17, (int32_t)UINT32_C(0x00000000), 4, 0,
          UINT32_C(0x80060ce0)));
  STEP(0x80060ce4);
  R(2) = add(R(2), imm(UINT32_C(0x00000001)));
  STEP(0x80060ce8);
  OK(store(r, 2, 17, (int32_t)UINT32_C(0x000000df), 1, UINT32_C(0x80060ce8)));
  STEP(0x80060cec);
  R(2) = shift(R(16), 2, 0, 0);
  STEP(0x80060cf0);
  R(2) = add(R(18), R(2));
  STEP(0x80060cf4);
  OK(load(r, 4, 2, (int32_t)UINT32_C(0x00000098), 4, 0, UINT32_C(0x80060cf4)));
  STEP(0x80060cf8);
  STEP(0x80060cfc);
  OK(load(r, 3, 4, (int32_t)UINT32_C(0x0000000c), 2, 0, UINT32_C(0x80060cfc)));
  STEP(0x80060d00);
  STEP(0x80060d04);
  R(2) = compare(R(3), UINT32_C(0x000003e7), 0);
  /* 0x80060D08: capture the branch predicate before its delay instruction. */
  STEP(0x80060d08);
  decided = eq(R(2), R(0), &branch);
  STEP(0x80060d0c);
  R(2) = add(R(3), imm(UINT32_C(0x00000001)));
  if (!decided) {
    stop(r, UINT32_C(0x80060d08), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060d20;
  STEP(0x80060d10);
  OK(store(r, 2, 4, (int32_t)UINT32_C(0x0000000c), 2, UINT32_C(0x80060d10)));
  STEP(0x80060d14);
  OK(load(r, 5, 17, (int32_t)UINT32_C(0x00000000), 2, 1, UINT32_C(0x80060d14)));
  /* 0x80060D18: JAL 0x80035318; link precedes delay-slot work. */
  STEP(0x80060d18);
  known(&R(31), UINT32_C(0x80060d20));
  STEP(0x80060d1c);
  R(4) = bitorc(R(0), UINT32_C(0x00000011));
  OK(invoke_site(r, UINT32_C(0x80060d18), UINT32_C(0x80035318),
                 call_args(UINT32_C(0x80060d18), UINT32_C(0x80035318))));
L_80060d20:
  STEP(0x80060d20);
  OK(load(r, 16, 17, (int32_t)UINT32_C(0x00000004), 2, 1,
          UINT32_C(0x80060d20)));
  STEP(0x80060d24);
  /* 0x80060D28: capture the branch predicate before its delay instruction. */
  STEP(0x80060d28);
  decided = signcond(R(16), 0, &branch);
  STEP(0x80060d2c);
  R(2) = shift(R(16), 2, 0, 0);
  if (!decided) {
    stop(r, UINT32_C(0x80060d28), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060d4c;
  STEP(0x80060d30);
  R(2) = add(R(18), R(2));
  STEP(0x80060d34);
  OK(load(r, 3, 2, (int32_t)UINT32_C(0x00000078), 4, 0, UINT32_C(0x80060d34)));
  STEP(0x80060d38);
  STEP(0x80060d3c);
  OK(load(r, 2, 3, (int32_t)UINT32_C(0x0000000c), 2, 0, UINT32_C(0x80060d3c)));
  STEP(0x80060d40);
  STEP(0x80060d44);
  R(2) = add(R(2), imm(UINT32_C(0x00000001)));
  STEP(0x80060d48);
  OK(store(r, 2, 3, (int32_t)UINT32_C(0x0000000c), 2, UINT32_C(0x80060d48)));
L_80060d4c:
  STEP(0x80060d4c);
  OK(load(r, 3, 17, (int32_t)UINT32_C(0x0000004a), 2, 0, UINT32_C(0x80060d4c)));
  STEP(0x80060d50);
  R(2) = bitorc(R(0), UINT32_C(0x00000045));
  /* 0x80060D54: capture the branch predicate before its delay instruction. */
  STEP(0x80060d54);
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  STEP(0x80060d58);
  R(5) = bitorc(R(0), UINT32_C(0x00000048));
  if (!decided) {
    stop(r, UINT32_C(0x80060d54), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060d70;
  STEP(0x80060d5c);
  R(4) = R(17);
  STEP(0x80060d60);
  known(&R(2), UINT32_C(0xffffffff));
  STEP(0x80060d64);
  OK(store(r, 0, 4, (int32_t)UINT32_C(0x00000060), 2, UINT32_C(0x80060d64)));
  /* 0x80060D68: JAL 0x8005699C; link precedes delay-slot work. */
  STEP(0x80060d68);
  known(&R(31), UINT32_C(0x80060d70));
  STEP(0x80060d6c);
  OK(store(r, 2, 4, (int32_t)UINT32_C(0x00000048), 2, UINT32_C(0x80060d6c)));
  OK(invoke_site(r, UINT32_C(0x80060d68), UINT32_C(0x8005699c),
                 call_args(UINT32_C(0x80060d68), UINT32_C(0x8005699c))));
L_80060d70:
  STEP(0x80060d70);
  known(&R(1), UINT32_C(0x80100000));
  STEP(0x80060d74);
  OK(store(r, 19, 1, (int32_t)UINT32_C(0xffffdc40), 4, UINT32_C(0x80060d74)));
L_80060d78:
  /* 0x80060D78..0x80060E60: clamped deflection velocity and phase cleanup. */
  STEP(0x80060d78);
  OK(load(r, 4, 20, (int32_t)UINT32_C(0x00000014), 2, 1, UINT32_C(0x80060d78)));
  STEP(0x80060d7c);
  OK(load(r, 5, 20, (int32_t)UINT32_C(0x00000016), 2, 1, UINT32_C(0x80060d7c)));
  /* 0x80060D80: JAL 0x8007066C; link precedes delay-slot work. */
  STEP(0x80060d80);
  known(&R(31), UINT32_C(0x80060d88));
  STEP(0x80060d84);
  OK(invoke_site(r, UINT32_C(0x80060d80), UINT32_C(0x8007066c),
                 call_args(UINT32_C(0x80060d80), UINT32_C(0x8007066c))));
  STEP(0x80060d88);
  OK(load(r, 5, 20, (int32_t)UINT32_C(0x00000018), 2, 1, UINT32_C(0x80060d88)));
  /* 0x80060D8C: JAL 0x8007066C; link precedes delay-slot work. */
  STEP(0x80060d8c);
  known(&R(31), UINT32_C(0x80060d94));
  STEP(0x80060d90);
  R(4) = R(2);
  OK(invoke_site(r, UINT32_C(0x80060d8c), UINT32_C(0x8007066c),
                 call_args(UINT32_C(0x80060d8c), UINT32_C(0x8007066c))));
  STEP(0x80060d94);
  R(18) = shift(R(2), 1, 1, 1);
  STEP(0x80060d98);
  R(2) = compare(R(18), UINT32_C(0x000002a1), 1);
  /* 0x80060D9C: capture the branch predicate before its delay instruction. */
  STEP(0x80060d9c);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  STEP(0x80060da0);
  R(2) = compare(R(18), UINT32_C(0x00000100), 1);
  if (!decided) {
    stop(r, UINT32_C(0x80060d9c), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060dac;
  STEP(0x80060da4);
  R(18) = bitorc(R(0), UINT32_C(0x000002a0));
  STEP(0x80060da8);
  R(2) = compare(R(18), UINT32_C(0x00000100), 1);
L_80060dac:
  /* 0x80060DAC: capture the branch predicate before its delay instruction. */
  STEP(0x80060dac);
  decided = eq(R(2), R(0), &branch);
  STEP(0x80060db0);
  if (!decided) {
    stop(r, UINT32_C(0x80060dac), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060db8;
  STEP(0x80060db4);
  R(18) = bitorc(R(0), UINT32_C(0x00000100));
L_80060db8:
  /* 0x80060DB8: JAL 0x8002AB70; link precedes delay-slot work. */
  STEP(0x80060db8);
  known(&R(31), UINT32_C(0x80060dc0));
  STEP(0x80060dbc);
  OK(invoke_site(r, UINT32_C(0x80060db8), UINT32_C(0x8002ab70),
                 call_args(UINT32_C(0x80060db8), UINT32_C(0x8002ab70))));
  STEP(0x80060dc0);
  R(2) = bitandc(R(2), UINT32_C(0x000000ff));
  STEP(0x80060dc4);
  R(16) = add(R(2), imm(UINT32_C(0xffffff80)));
  /* 0x80060DC8: JAL 0x800A5638; link precedes delay-slot work. */
  STEP(0x80060dc8);
  known(&R(31), UINT32_C(0x80060dd0));
  STEP(0x80060dcc);
  R(4) = R(16);
  OK(invoke_site(r, UINT32_C(0x80060dc8), UINT32_C(0x800a5638),
                 call_args(UINT32_C(0x80060dc8), UINT32_C(0x800a5638))));
  STEP(0x80060dd0);
  R(4) = R(2);
  /* 0x80060DD4: JAL 0x800AA788; link precedes delay-slot work. */
  STEP(0x80060dd4);
  known(&R(31), UINT32_C(0x80060ddc));
  STEP(0x80060dd8);
  R(5) = R(18);
  OK(invoke_site(r, UINT32_C(0x80060dd4), UINT32_C(0x800aa788),
                 call_args(UINT32_C(0x80060dd4), UINT32_C(0x800aa788))));
  STEP(0x80060ddc);
  R(4) = R(16);
  /* 0x80060DE0: JAL 0x800A5634; link precedes delay-slot work. */
  STEP(0x80060de0);
  known(&R(31), UINT32_C(0x80060de8));
  STEP(0x80060de4);
  OK(store(r, 2, 20, (int32_t)UINT32_C(0x00000018), 2, UINT32_C(0x80060de4)));
  OK(invoke_site(r, UINT32_C(0x80060de0), UINT32_C(0x800a5634),
                 call_args(UINT32_C(0x80060de0), UINT32_C(0x800a5634))));
  STEP(0x80060de8);
  R(4) = R(2);
  /* 0x80060DEC: JAL 0x800AA788; link precedes delay-slot work. */
  STEP(0x80060dec);
  known(&R(31), UINT32_C(0x80060df4));
  STEP(0x80060df0);
  R(5) = R(18);
  OK(invoke_site(r, UINT32_C(0x80060dec), UINT32_C(0x800aa788),
                 call_args(UINT32_C(0x80060dec), UINT32_C(0x800aa788))));
  /* 0x80060DF4: JAL 0x8002AB70; link precedes delay-slot work. */
  STEP(0x80060df4);
  known(&R(31), UINT32_C(0x80060dfc));
  STEP(0x80060df8);
  R(18) = R(2);
  OK(invoke_site(r, UINT32_C(0x80060df4), UINT32_C(0x8002ab70),
                 call_args(UINT32_C(0x80060df4), UINT32_C(0x8002ab70))));
  STEP(0x80060dfc);
  R(16) = bitandc(R(2), UINT32_C(0x0000ffff));
  /* 0x80060E00: JAL 0x800A5638; link precedes delay-slot work. */
  STEP(0x80060e00);
  known(&R(31), UINT32_C(0x80060e08));
  STEP(0x80060e04);
  R(4) = R(16);
  OK(invoke_site(r, UINT32_C(0x80060e00), UINT32_C(0x800a5638),
                 call_args(UINT32_C(0x80060e00), UINT32_C(0x800a5638))));
  STEP(0x80060e08);
  R(4) = R(2);
  /* 0x80060E0C: JAL 0x800AA788; link precedes delay-slot work. */
  STEP(0x80060e0c);
  known(&R(31), UINT32_C(0x80060e14));
  STEP(0x80060e10);
  R(5) = R(18);
  OK(invoke_site(r, UINT32_C(0x80060e0c), UINT32_C(0x800aa788),
                 call_args(UINT32_C(0x80060e0c), UINT32_C(0x800aa788))));
  STEP(0x80060e14);
  R(4) = R(16);
  /* 0x80060E18: JAL 0x800A5634; link precedes delay-slot work. */
  STEP(0x80060e18);
  known(&R(31), UINT32_C(0x80060e20));
  STEP(0x80060e1c);
  OK(store(r, 2, 20, (int32_t)UINT32_C(0x00000014), 2, UINT32_C(0x80060e1c)));
  OK(invoke_site(r, UINT32_C(0x80060e18), UINT32_C(0x800a5634),
                 call_args(UINT32_C(0x80060e18), UINT32_C(0x800a5634))));
  STEP(0x80060e20);
  R(4) = R(2);
  /* 0x80060E24: JAL 0x800AA788; link precedes delay-slot work. */
  STEP(0x80060e24);
  known(&R(31), UINT32_C(0x80060e2c));
  STEP(0x80060e28);
  R(5) = R(18);
  OK(invoke_site(r, UINT32_C(0x80060e24), UINT32_C(0x800aa788),
                 call_args(UINT32_C(0x80060e24), UINT32_C(0x800aa788))));
  /* 0x80060E2C: JAL 0x8005828C; link precedes delay-slot work. */
  STEP(0x80060e2c);
  known(&R(31), UINT32_C(0x80060e34));
  STEP(0x80060e30);
  OK(store(r, 2, 20, (int32_t)UINT32_C(0x00000016), 2, UINT32_C(0x80060e30)));
  OK(invoke_site(r, UINT32_C(0x80060e2c), UINT32_C(0x8005828c),
                 call_args(UINT32_C(0x80060e2c), UINT32_C(0x8005828c))));
  STEP(0x80060e34);
  known(&R(4), UINT32_C(0x80100000));
  STEP(0x80060e38);
  R(4) = add(R(4), imm(UINT32_C(0xffffdb90)));
  STEP(0x80060e3c);
  OK(load(r, 3, 4, (int32_t)UINT32_C(0x00000000), 2, 1, UINT32_C(0x80060e3c)));
  STEP(0x80060e40);
  R(2) = bitorc(R(0), UINT32_C(0x00000082));
  /* 0x80060E44: capture the branch predicate before its delay instruction. */
  STEP(0x80060e44);
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  STEP(0x80060e48);
  R(2) = bitorc(R(0), UINT32_C(0x00000003));
  if (!decided) {
    stop(r, UINT32_C(0x80060e44), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  STEP(0x80060e4c);
  known(&R(3), UINT32_C(0x80100000));
  STEP(0x80060e50);
  OK(load(r, 3, 3, (int32_t)UINT32_C(0xffffe884), 2, 1, UINT32_C(0x80060e50)));
  STEP(0x80060e54);
  /* 0x80060E58: capture the branch predicate before its delay instruction. */
  STEP(0x80060e58);
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  STEP(0x80060e5c);
  if (!decided) {
    stop(r, UINT32_C(0x80060e58), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80060e64;
  STEP(0x80060e60);
  OK(store(r, 0, 4, (int32_t)UINT32_C(0x00000000), 2, UINT32_C(0x80060e60)));
L_80060e64:
  /* 0x80060E64..0x80060E88: reload through live SP and return through live RA.
   */
  STEP(0x80060e64);
  OK(load(r, 31, 29, (int32_t)UINT32_C(0x00000038), 4, 0,
          UINT32_C(0x80060e64)));
  STEP(0x80060e68);
  OK(load(r, 21, 29, (int32_t)UINT32_C(0x00000034), 4, 0,
          UINT32_C(0x80060e68)));
  STEP(0x80060e6c);
  OK(load(r, 20, 29, (int32_t)UINT32_C(0x00000030), 4, 0,
          UINT32_C(0x80060e6c)));
  STEP(0x80060e70);
  OK(load(r, 19, 29, (int32_t)UINT32_C(0x0000002c), 4, 0,
          UINT32_C(0x80060e70)));
  STEP(0x80060e74);
  OK(load(r, 18, 29, (int32_t)UINT32_C(0x00000028), 4, 0,
          UINT32_C(0x80060e74)));
  STEP(0x80060e78);
  OK(load(r, 17, 29, (int32_t)UINT32_C(0x00000024), 4, 0,
          UINT32_C(0x80060e78)));
  STEP(0x80060e7c);
  OK(load(r, 16, 29, (int32_t)UINT32_C(0x00000020), 4, 0,
          UINT32_C(0x80060e7c)));
  STEP(0x80060e80);
  R(29) = add(R(29), imm(UINT32_C(0x00000040)));
  /* 0x80060E84: JR consumes the captured live target after the delay. */
  STEP(0x80060e84);
  jump_target = R(31);
  STEP(0x80060e88);
  if (jump_target.known_mask != 15) {
    stop(r, UINT32_C(0x80060e84), jump_target.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  p->restored_return_address = jump_target;
  p->completed = 1;
  stop(r, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
