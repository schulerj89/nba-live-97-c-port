#include "game_graphics_submit.h"
#include <string.h>
#define R(i) (r->m.registers.gpr[(i)])
#define TRY(x)                                                                 \
  do {                                                                         \
    int q_ = (x);                                                              \
    if (q_ != NBA97_TEXT_COMPLETE)                                             \
      return q_;                                                               \
  } while (0)
typedef struct Run {
  Nba97GameGraphicsSubmitContext *c;
  Nba97GameGraphicsSubmitProgress *o;
  Nba97GameGraphicsSubmitMachine m;
} Run;
static void pub(Run *r) { r->o->machine = r->m; }
static void stop(Run *r, uint32_t pc, uint32_t a, uint32_t e) {
  r->o->stopped_pc = pc;
  r->o->stopped_address = a;
  r->o->stopped_entry = e;
  pub(r);
}
static void kn(Nba97GameGraphicsSubmitWord *v, uint32_t x) {
  v->word = x;
  v->known_mask = 15;
}
static int vm(const Nba97GameGraphicsSubmitMachine *m) {
  unsigned i;
  if (m->registers.gpr[0].word || m->registers.gpr[0].known_mask != 15 ||
      m->hi.known_mask > 15 || m->lo.known_mask > 15)
    return 0;
  for (i = 0; i < 32; i++)
    if (m->registers.gpr[i].known_mask > 15)
      return 0;
  return 1;
}
static int init(Nba97GameGraphicsSubmitContext *c,
                Nba97GameGraphicsSubmitProgress *o, Run *r) {
  size_t i, j;
  if (!o)
    return NBA97_TEXT_ARGUMENT;
  memset(o, 0, sizeof *o);
  if (!c || (!c->memory.region && c->memory.count) ||
      (!c->access_journal && c->access_journal_capacity) || !vm(&c->machine))
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < c->memory.count; i++) {
    Nba97GameTextRegion *a = &c->memory.region[i];
    if (!a->data || !a->size || a->size > UINT64_C(0x100000000) ||
        (uint64_t)a->base + a->size > UINT64_C(0x100000000))
      return NBA97_TEXT_ARGUMENT;
    for (j = 0; j < i; j++) {
      Nba97GameTextRegion *b = &c->memory.region[j];
      if ((uint64_t)a->base < (uint64_t)b->base + b->size &&
          (uint64_t)b->base < (uint64_t)a->base + a->size)
        return NBA97_TEXT_ARGUMENT;
    }
  }
  r->c = c;
  r->o = o;
  r->m = c->machine;
  pub(r);
  return NBA97_TEXT_COMPLETE;
}
static Nba97GameGraphicsSubmitWord add(Nba97GameGraphicsSubmitWord a,
                                       Nba97GameGraphicsSubmitWord b) {
  Nba97GameGraphicsSubmitWord q;
  unsigned carry_mask = 1;
  unsigned byte;
  q.word = a.word + b.word;
  q.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned as =
        (a.known_mask & (1u << byte)) ? ((a.word >> (byte * 8u)) & 255u) : 0;
    unsigned ae = (a.known_mask & (1u << byte)) ? as : 255u;
    unsigned bs =
        (b.known_mask & (1u << byte)) ? ((b.word >> (byte * 8u)) & 255u) : 0;
    unsigned be = (b.known_mask & (1u << byte)) ? bs : 255u;
    unsigned carry;
    for (carry = 0; carry < 2; ++carry) {
      unsigned av;
      if (!(carry_mask & (1u << carry)))
        continue;
      for (av = as; av <= ae; ++av) {
        unsigned bv;
        for (bv = bs; bv <= be; ++bv) {
          unsigned sum = av + bv + carry;
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
      q.known_mask = (uint8_t)(q.known_mask | (1u << byte));
    carry_mask = next_mask;
  }
  return q;
}
static Nba97GameGraphicsSubmitWord addc(Nba97GameGraphicsSubmitWord a,
                                        uint32_t x) {
  Nba97GameGraphicsSubmitWord b;
  kn(&b, x);
  return add(a, b);
}
static Nba97GameGraphicsSubmitWord sub(Nba97GameGraphicsSubmitWord a,
                                       Nba97GameGraphicsSubmitWord b) {
  Nba97GameGraphicsSubmitWord q;
  unsigned borrow_mask = 1;
  unsigned byte;
  q.word = a.word - b.word;
  q.known_mask = 0;
  for (byte = 0; byte < 4; ++byte) {
    unsigned next_mask = 0;
    unsigned first_output = 0;
    int first = 1;
    int invariant = 1;
    unsigned as =
        (a.known_mask & (1u << byte)) ? ((a.word >> (byte * 8u)) & 255u) : 0;
    unsigned ae = (a.known_mask & (1u << byte)) ? as : 255u;
    unsigned bs =
        (b.known_mask & (1u << byte)) ? ((b.word >> (byte * 8u)) & 255u) : 0;
    unsigned be = (b.known_mask & (1u << byte)) ? bs : 255u;
    unsigned borrow;
    for (borrow = 0; borrow < 2; ++borrow) {
      unsigned av;
      if (!(borrow_mask & (1u << borrow)))
        continue;
      for (av = as; av <= ae; ++av) {
        unsigned bv;
        for (bv = bs; bv <= be; ++bv) {
          unsigned output = (av - bv - borrow) & 255u;
          next_mask |= 1u << (av < bv + borrow);
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
      q.known_mask = (uint8_t)(q.known_mask | (1u << byte));
    borrow_mask = next_mask;
  }
  return q;
}
static Nba97GameGraphicsSubmitWord shl(Nba97GameGraphicsSubmitWord a,
                                       unsigned n) {
  Nba97GameGraphicsSubmitWord q;
  uint32_t kb = 0, s;
  unsigned i;
  q.word = a.word << n;
  for (i = 0; i < 4; i++)
    if (a.known_mask & (1u << i))
      kb |= 255u << (8 * i);
  s = (kb << n) | ((1u << n) - 1);
  q.known_mask = 0;
  for (i = 0; i < 4; i++)
    if (((s >> (8 * i)) & 255) == 255)
      q.known_mask |= (uint8_t)(1u << i);
  return q;
}
static Nba97GameGraphicsSubmitWord sar2(Nba97GameGraphicsSubmitWord a) {
  Nba97GameGraphicsSubmitWord q;
  q.word = a.word >> 2;
  if (a.word & UINT32_C(0x80000000))
    q.word |= UINT32_C(0xc0000000);
  q.known_mask = 0;
  if ((a.known_mask & 3u) == 3u)
    q.known_mask |= 1u;
  if ((a.known_mask & 6u) == 6u)
    q.known_mask |= 2u;
  if ((a.known_mask & 12u) == 12u)
    q.known_mask |= 4u;
  if (a.known_mask & 8u)
    q.known_mask |= 8u;
  return q;
}
static Nba97GameGraphicsSubmitWord band(Nba97GameGraphicsSubmitWord a,
                                        Nba97GameGraphicsSubmitWord b) {
  Nba97GameGraphicsSubmitWord q;
  unsigned i;
  q.word = a.word & b.word;
  q.known_mask = 0;
  for (i = 0; i < 4; i++) {
    uint32_t x = (a.word >> (8 * i)) & 255, y = (b.word >> (8 * i)) & 255;
    int ak = a.known_mask & (1u << i), bk = b.known_mask & (1u << i);
    if ((ak && bk) || (ak && x == 0) || (bk && y == 0))
      q.known_mask |= (uint8_t)(1u << i);
  }
  return q;
}
static Nba97GameGraphicsSubmitWord andc(Nba97GameGraphicsSubmitWord a,
                                        uint32_t x) {
  Nba97GameGraphicsSubmitWord b;
  kn(&b, x);
  return band(a, b);
}
static void signed_bounds(Nba97GameGraphicsSubmitWord value, int64_t *minimum,
                          int64_t *maximum) {
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
static Nba97GameGraphicsSubmitWord signed_less(Nba97GameGraphicsSubmitWord a,
                                               Nba97GameGraphicsSubmitWord b) {
  Nba97GameGraphicsSubmitWord q;
  int64_t amin, amax, bmin, bmax;
  q.word = (uint32_t)((a.word ^ UINT32_C(0x80000000)) <
                      (b.word ^ UINT32_C(0x80000000)));
  signed_bounds(a, &amin, &amax);
  signed_bounds(b, &bmin, &bmax);
  q.known_mask = (amax < bmin || amin >= bmax) ? 15 : 14;
  return q;
}
static int spend(Run *r) {
  if (r->o->operations >= r->c->operation_budget)
    return NBA97_TEXT_LIMIT;
  r->o->operations++;
  return NBA97_TEXT_COMPLETE;
}
static void jrnl(Run *r, uint8_t k, uint32_t pc, uint32_t a, unsigned w,
                 Nba97GameGraphicsSubmitWord v) {
  size_t i = r->o->access_events++;
  if (i < r->c->access_journal_capacity) {
    Nba97GameGraphicsSubmitAccess *e = &r->c->access_journal[i];
    e->pc = pc;
    e->address = a;
    e->value = v.word;
    e->operation = r->o->operations;
    e->width = (uint8_t)w;
    e->known_mask = (uint8_t)(v.known_mask & ((1u << w) - 1));
    e->kind = k;
  }
}
static int loc(Run *r, uint32_t a, unsigned w, unsigned al, uint32_t pc,
               uint8_t **d, uint8_t **k) {
  size_t i, j;
  stop(r, pc, a, 0);
  TRY(spend(r));
  r->o->accesses++;
  if (a & (al - 1))
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0; i < r->c->memory.count; i++) {
    Nba97GameTextRegion *g = &r->c->memory.region[i];
    uint64_t off = (uint64_t)a - g->base;
    if (a < g->base || off > g->size || w > g->size - (size_t)off)
      continue;
    *d = g->data + (size_t)off;
    *k = g->known ? g->known + (size_t)off : 0;
    if (*k)
      for (j = 0; j < w; j++)
        if ((*k)[j] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}
static int rd(Run *r, uint32_t a, unsigned w, unsigned al, uint32_t pc,
              Nba97GameGraphicsSubmitWord *v) {
  uint8_t *d, *k;
  Nba97GameGraphicsSubmitWord q = {0, 0};
  unsigned i;
  TRY(loc(r, a, w, al, pc, &d, &k));
  for (i = 0; i < w; i++) {
    q.word |= (uint32_t)d[i] << (8 * i);
    if (!k || k[i])
      q.known_mask |= (uint8_t)(1u << i);
  }
  if (w == 1) {
    q.known_mask |= 14;
    q.word &= 255;
  }
  *v = q;
  r->o->reads++;
  jrnl(r, NBA97_GAME_MATCH_CLOCKS_READ, pc, a, w, q);
  pub(r);
  return NBA97_TEXT_COMPLETE;
}
static int wr(Run *r, uint32_t a, unsigned w, unsigned al, uint32_t pc,
              Nba97GameGraphicsSubmitWord v) {
  uint8_t *d, *k;
  unsigned i;
  uint8_t m = (uint8_t)((1u << w) - 1);
  TRY(loc(r, a, w, al, pc, &d, &k));
  if (!k && (v.known_mask & m) != m)
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < w; i++) {
    d[i] = (uint8_t)(v.word >> (8 * i));
    if (k)
      k[i] = (uint8_t)((v.known_mask >> i) & 1);
  }
  r->o->stores++;
  jrnl(r, NBA97_GAME_MATCH_CLOCKS_STORE, pc, a, w, v);
  pub(r);
  return NBA97_TEXT_COMPLETE;
}
static int adr(Run *r, Nba97GameGraphicsSubmitWord a, uint32_t pc,
               uint32_t *out) {
  if (a.known_mask != 15) {
    stop(r, pc, a.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *out = a.word;
  return NBA97_TEXT_COMPLETE;
}
static int dr(Run *r, Nba97GameGraphicsSubmitWord a, unsigned w, unsigned al,
              uint32_t pc, Nba97GameGraphicsSubmitWord *v) {
  uint32_t x;
  TRY(adr(r, a, pc, &x));
  return rd(r, x, w, al, pc, v);
}
static int dw(Run *r, Nba97GameGraphicsSubmitWord a, unsigned w, unsigned al,
              uint32_t pc, Nba97GameGraphicsSubmitWord v) {
  uint32_t x;
  TRY(adr(r, a, pc, &x));
  return wr(r, x, w, al, pc, v);
}
static int eq(Run *r, Nba97GameGraphicsSubmitWord a,
              Nba97GameGraphicsSubmitWord b, uint32_t pc, int *z) {
  unsigned i;
  for (i = 0; i < 4; i++)
    if ((a.known_mask & (1u << i)) && (b.known_mask & (1u << i)) &&
        (((a.word ^ b.word) >> (8 * i)) & 255)) {
      *z = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (a.known_mask == 15 && b.known_mask == 15) {
    *z = a.word == b.word;
    return NBA97_TEXT_COMPLETE;
  }
  stop(r, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}
static int zero(Run *r, Nba97GameGraphicsSubmitWord a, uint32_t pc, int *z) {
  Nba97GameGraphicsSubmitWord q;
  kn(&q, 0);
  return eq(r, a, q, pc, z);
}
static int call(Run *r, uint32_t pc, Nba97GameGraphicsSubmitWord entry,
                uint8_t kind, uint8_t argc, int delay, unsigned reg,
                Nba97GameGraphicsSubmitWord value) {
  Nba97GameGraphicsSubmitEvent e;
  int ok;
  kn(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8);
  if (delay)
    R(reg) = value;
  stop(r, pc, 0, entry.word);
  TRY(spend(r));
  r->o->call_attempts[kind]++;
  if (entry.known_mask != 15)
    return NBA97_TEXT_UNKNOWN;
  if (entry.word & 3)
    return NBA97_TEXT_ALIGNMENT_TRAP;
  memset(&e, 0, sizeof e);
  e.pc = pc;
  e.delay_slot_pc = pc + 4;
  e.entry = entry.word;
  e.operation = r->o->operations;
  e.invocation = r->o->call_attempts[kind];
  e.kind = kind;
  e.argument_count = argc;
  pub(r);
  if (!r->c->io)
    return NBA97_TEXT_IO_REFUSED;
  ok = r->c->io(r->c->user, &r->c->memory, &e, &r->m);
  pub(r);
  if (ok != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!vm(&r->m))
    return NBA97_TEXT_ARGUMENT;
  r->o->callbacks_completed++;
  r->o->call_count[kind]++;
  return NBA97_TEXT_COMPLETE;
}
static int fixed(Run *r, uint32_t pc, uint32_t entry, uint8_t kind,
                 uint8_t argc, int delay, unsigned reg,
                 Nba97GameGraphicsSubmitWord v) {
  Nba97GameGraphicsSubmitWord e;
  kn(&e, entry);
  return call(r, pc, e, kind, argc, delay, reg, v);
}
static int restore(Run *r, uint32_t pc, uint32_t off, unsigned reg,
                   Nba97GameGraphicsSubmitWord *out) {
  Nba97GameGraphicsSubmitWord v;
  TRY(dr(r, addc(R(NBA97_MATCH_INITIALIZE_SP), off), 4, 4, pc, &v));
  R(reg) = v;
  *out = v;
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_graphics_submit(Nba97GameGraphicsSubmitContext *c,
                               Nba97GameGraphicsSubmitProgress *o) {
  Run s, *r = &s;
  Nba97GameGraphicsSubmitWord v, branch, entry;
  int z;
  TRY(init(c, o, r));
  /* 9B298..2C0 save live arguments and enter scheduler. */
  R(NBA97_MATCH_INITIALIZE_SP) = addc(R(NBA97_MATCH_INITIALIZE_SP), 0xffffffd8);
  o->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  TRY(dw(r, addc(R(NBA97_MATCH_INITIALIZE_SP), 28), 4, 4, 0x8009b29c,
         R(NBA97_MATCH_INITIALIZE_S0 + 3)));
  R(NBA97_MATCH_INITIALIZE_S0 + 3) = R(NBA97_MATCH_INITIALIZE_A0);
  TRY(dw(r, addc(R(NBA97_MATCH_INITIALIZE_SP), 16), 4, 4, 0x8009b2a4,
         R(NBA97_MATCH_INITIALIZE_S0)));
  R(NBA97_MATCH_INITIALIZE_S0) = R(NBA97_MATCH_INITIALIZE_A1);
  TRY(dw(r, addc(R(NBA97_MATCH_INITIALIZE_SP), 20), 4, 4, 0x8009b2ac,
         R(NBA97_MATCH_INITIALIZE_S0 + 1)));
  R(NBA97_MATCH_INITIALIZE_S0 + 1) = R(NBA97_MATCH_INITIALIZE_A2);
  TRY(dw(r, addc(R(NBA97_MATCH_INITIALIZE_SP), 24), 4, 4, 0x8009b2b4,
         R(NBA97_MATCH_INITIALIZE_S0 + 2)));
  TRY(dw(r, addc(R(NBA97_MATCH_INITIALIZE_SP), 32), 4, 4, 0x8009b2b8,
         R(NBA97_MATCH_INITIALIZE_RA)));
  TRY(fixed(r, 0x8009b2bc, 0x8009bafc, NBA97_GAME_GRAPHICS_SUBMIT_START, 0, 1,
            NBA97_MATCH_INITIALIZE_S0 + 2, R(NBA97_MATCH_INITIALIZE_A3)));
  /* 9B2CC..300 wait and drain while the ring is full. */
  for (;;) {
    kn(&R(NBA97_MATCH_INITIALIZE_V0), 0x800c0000);
    TRY(rd(r, 0x800c56c4, 4, 4, 0x8009b2e8, &R(NBA97_MATCH_INITIALIZE_V0)));
    kn(&R(NBA97_MATCH_INITIALIZE_V1), 0x800c0000);
    TRY(rd(r, 0x800c56c8, 4, 4, 0x8009b2f0, &R(NBA97_MATCH_INITIALIZE_V1)));
    R(NBA97_MATCH_INITIALIZE_V0) =
        andc(addc(R(NBA97_MATCH_INITIALIZE_V0), 1), 63);
    TRY(eq(r, R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1),
           0x8009b2fc, &z));
    if (!z)
      break;
    o->full_queue_iterations++;
    TRY(fixed(r, 0x8009b2cc, 0x8009bb30, NBA97_GAME_GRAPHICS_SUBMIT_WAIT, 0, 0,
              0, R(0)));
    branch = R(NBA97_MATCH_INITIALIZE_V0);
    kn(&R(NBA97_MATCH_INITIALIZE_V0), 0xffffffff);
    TRY(zero(r, branch, 0x8009b2d4, &z));
    if (!z)
      goto done;
    TRY(fixed(r, 0x8009b2dc, 0x8009b57c, NBA97_GAME_GRAPHICS_SUBMIT_DRAIN, 0, 0,
              0, R(0)));
  }
  /* 9B304..380 enter critical state and test queue eligibility. */
  kn(&v, 0);
  TRY(fixed(r, 0x8009b304, 0x800986f8, NBA97_GAME_GRAPHICS_SUBMIT_CRITICAL, 1,
            1, NBA97_MATCH_INITIALIZE_A0, v));
  kn(&R(NBA97_MATCH_INITIALIZE_V1), 0x800c55c8);
  kn(&R(NBA97_MATCH_INITIALIZE_A0), 1);
  TRY(wr(r, 0x800c55c8, 4, 4, 0x8009b318, R(NBA97_MATCH_INITIALIZE_A0)));
  kn(&R(NBA97_MATCH_INITIALIZE_V1), 0x800c0000);
  TRY(rd(r, 0x800c55c1, 1, 1, 0x8009b320, &R(NBA97_MATCH_INITIALIZE_V1)));
  kn(&R(NBA97_MATCH_INITIALIZE_AT), 0x800c0000);
  TRY(wr(r, 0x800c56cc, 4, 4, 0x8009b328, R(NBA97_MATCH_INITIALIZE_V0)));
  o->saved_critical = R(NBA97_MATCH_INITIALIZE_V0);
  branch = R(NBA97_MATCH_INITIALIZE_V1);
  kn(&R(NBA97_MATCH_INITIALIZE_A0), 0x04000000);
  TRY(zero(r, branch, 0x8009b32c, &z));
  if (!z) {
    kn(&R(NBA97_MATCH_INITIALIZE_V1), 0x800c0000);
    TRY(rd(r, 0x800c56c4, 4, 4, 0x8009b338, &R(NBA97_MATCH_INITIALIZE_V1)));
    kn(&R(NBA97_MATCH_INITIALIZE_V0), 0x800c0000);
    TRY(rd(r, 0x800c56c8, 4, 4, 0x8009b340, &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(eq(r, R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0),
           0x8009b348, &z));
    if (z) {
      kn(&R(NBA97_MATCH_INITIALIZE_V0), 0x800c0000);
      TRY(rd(r, 0x800c56a0, 4, 4, 0x8009b354, &R(NBA97_MATCH_INITIALIZE_V0)));
      TRY(dr(r, R(NBA97_MATCH_INITIALIZE_V0), 4, 4, 0x8009b35c,
             &R(NBA97_MATCH_INITIALIZE_V0)));
      kn(&R(NBA97_MATCH_INITIALIZE_V1), 0x01000000);
      R(NBA97_MATCH_INITIALIZE_V0) =
          band(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
      TRY(zero(r, R(NBA97_MATCH_INITIALIZE_V0), 0x8009b368, &z));
      if (z) {
        kn(&R(NBA97_MATCH_INITIALIZE_V0), 0x800c0000);
        TRY(rd(r, 0x800c55cc, 4, 4, 0x8009b374, &R(NBA97_MATCH_INITIALIZE_V0)));
        TRY(zero(r, R(NBA97_MATCH_INITIALIZE_V0), 0x8009b37c, &z));
        if (z)
          goto direct;
      }
    }
  } else {
    goto direct;
  }
  goto queued;
direct:
  /* 9B384..3E0 poll ready, call live function, publish last call, restore
     mode. */
  kn(&R(NBA97_MATCH_INITIALIZE_V1), 0x800c0000);
  TRY(rd(r, 0x800c5694, 4, 4, 0x8009b388, &R(NBA97_MATCH_INITIALIZE_V1)));
  for (;;) {
    o->gpu_poll_iterations++;
    TRY(dr(r, R(NBA97_MATCH_INITIALIZE_V1), 4, 4, 0x8009b390,
           &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_V0) =
        band(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_A0));
    TRY(zero(r, R(NBA97_MATCH_INITIALIZE_V0), 0x8009b39c, &z));
    if (!z)
      break;
  }
  R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0);
  entry = R(NBA97_MATCH_INITIALIZE_S0 + 3);
  TRY(call(r, 0x8009b3a8, entry, NBA97_GAME_GRAPHICS_SUBMIT_INDIRECT, 2, 1,
           NBA97_MATCH_INITIALIZE_A1, R(NBA97_MATCH_INITIALIZE_S0 + 2)));
  kn(&R(NBA97_MATCH_INITIALIZE_A0), 0x800c0000);
  TRY(rd(r, 0x800c56cc, 4, 4, 0x8009b3b4, &R(NBA97_MATCH_INITIALIZE_A0)));
  kn(&R(NBA97_MATCH_INITIALIZE_V0), 0x800c56b4);
  TRY(wr(r, 0x800c56b4, 4, 4, 0x8009b3c0, R(NBA97_MATCH_INITIALIZE_S0 + 3)));
  kn(&R(NBA97_MATCH_INITIALIZE_AT), 0x800c0000);
  TRY(wr(r, 0x800c56b8, 4, 4, 0x8009b3c8, R(NBA97_MATCH_INITIALIZE_S0)));
  kn(&R(NBA97_MATCH_INITIALIZE_AT), 0x800c0000);
  TRY(wr(r, 0x800c56bc, 4, 4, 0x8009b3d0, R(NBA97_MATCH_INITIALIZE_S0 + 2)));
  TRY(fixed(r, 0x8009b3d4, 0x800986f8, NBA97_GAME_GRAPHICS_SUBMIT_CRITICAL, 1,
            0, 0, R(0)));
  kn(&R(NBA97_MATCH_INITIALIZE_V0), 0);
  goto done;
queued:
  /* 9B3E4..4C4 install queue mode and copy signed count/4 words. */
  kn(&R(NBA97_MATCH_INITIALIZE_A1), 0x8009b57c);
  kn(&v, 2);
  TRY(fixed(r, 0x8009b3ec, 0x8009863c, NBA97_GAME_GRAPHICS_SUBMIT_INSTALL, 2, 1,
            NBA97_MATCH_INITIALIZE_A0, v));
  branch = R(NBA97_MATCH_INITIALIZE_S0 + 1);
  kn(&R(NBA97_MATCH_INITIALIZE_A2), 0);
  TRY(zero(r, branch, 0x8009b3f4, &z));
  if (!z) {
    kn(&R(8), 0x80104754);
    R(NBA97_MATCH_INITIALIZE_A3) = R(NBA97_MATCH_INITIALIZE_S0);
    for (;;) {
      R(NBA97_MATCH_INITIALIZE_V0) = R(NBA97_MATCH_INITIALIZE_S0 + 1);
      if (!(R(NBA97_MATCH_INITIALIZE_V0).known_mask & 8)) {
        stop(r, 0x8009b40c, 0, 0);
        return NBA97_TEXT_UNKNOWN;
      }
      if (R(NBA97_MATCH_INITIALIZE_V0).word & UINT32_C(0x80000000))
        R(NBA97_MATCH_INITIALIZE_V0) = addc(R(NBA97_MATCH_INITIALIZE_V0), 3);
      R(NBA97_MATCH_INITIALIZE_V0) = sar2(R(NBA97_MATCH_INITIALIZE_V0));
      R(NBA97_MATCH_INITIALIZE_V0) = signed_less(R(NBA97_MATCH_INITIALIZE_A2),
                                                 R(NBA97_MATCH_INITIALIZE_V0));
      branch = R(NBA97_MATCH_INITIALIZE_V0);
      R(NBA97_MATCH_INITIALIZE_A0) = shl(R(NBA97_MATCH_INITIALIZE_A2), 2);
      TRY(zero(r, branch, 0x8009b420, &z));
      if (z)
        break;
      TRY(dr(r, R(NBA97_MATCH_INITIALIZE_A3), 4, 4, 0x8009b428,
             &R(NBA97_MATCH_INITIALIZE_A1)));
      R(NBA97_MATCH_INITIALIZE_A3) = addc(R(NBA97_MATCH_INITIALIZE_A3), 4);
      kn(&R(NBA97_MATCH_INITIALIZE_V1), 0x800c0000);
      TRY(rd(r, 0x800c56c4, 4, 4, 0x8009b434, &R(NBA97_MATCH_INITIALIZE_V1)));
      R(NBA97_MATCH_INITIALIZE_A2) = addc(R(NBA97_MATCH_INITIALIZE_A2), 1);
      R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V1), 1);
      R(NBA97_MATCH_INITIALIZE_V0) =
          add(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
      R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V0), 5);
      R(NBA97_MATCH_INITIALIZE_V0) = add(R(NBA97_MATCH_INITIALIZE_V0), R(8));
      R(NBA97_MATCH_INITIALIZE_A0) =
          add(R(NBA97_MATCH_INITIALIZE_A0), R(NBA97_MATCH_INITIALIZE_V0));
      TRY(dw(r, R(NBA97_MATCH_INITIALIZE_A0), 4, 4, 0x8009b450,
             R(NBA97_MATCH_INITIALIZE_A1)));
      o->copy_iterations++;
    }
    kn(&R(NBA97_MATCH_INITIALIZE_V0), 0x800c0000);
    TRY(rd(r, 0x800c56c4, 4, 4, 0x8009b460, &R(NBA97_MATCH_INITIALIZE_V0)));
    kn(&R(NBA97_MATCH_INITIALIZE_V1), 0x800c0000);
    TRY(rd(r, 0x800c56c4, 4, 4, 0x8009b468, &R(NBA97_MATCH_INITIALIZE_V1)));
    R(NBA97_MATCH_INITIALIZE_A0) = shl(R(NBA97_MATCH_INITIALIZE_V0), 1);
    R(NBA97_MATCH_INITIALIZE_A0) =
        add(R(NBA97_MATCH_INITIALIZE_A0), R(NBA97_MATCH_INITIALIZE_V0));
    R(NBA97_MATCH_INITIALIZE_A0) = shl(R(NBA97_MATCH_INITIALIZE_A0), 5);
    R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V1), 1);
    R(NBA97_MATCH_INITIALIZE_V0) =
        add(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
    R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V0), 5);
    kn(&R(NBA97_MATCH_INITIALIZE_V1), 0x80104754);
    R(NBA97_MATCH_INITIALIZE_V0) =
        add(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
    kn(&R(NBA97_MATCH_INITIALIZE_AT), 0x80100000);
    R(NBA97_MATCH_INITIALIZE_AT) =
        add(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_A0));
    TRY(dw(r, addc(R(NBA97_MATCH_INITIALIZE_AT), 0x474c), 4, 4, 0x8009b498,
           R(NBA97_MATCH_INITIALIZE_V0)));
  } else {
    kn(&R(NBA97_MATCH_INITIALIZE_V1), 0x800c0000);
    TRY(rd(r, 0x800c56c4, 4, 4, 0x8009b4a8, &R(NBA97_MATCH_INITIALIZE_V1)));
    R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V1), 1);
    R(NBA97_MATCH_INITIALIZE_V0) =
        add(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
    R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V0), 5);
    kn(&R(NBA97_MATCH_INITIALIZE_AT), 0x80100000);
    R(NBA97_MATCH_INITIALIZE_AT) =
        add(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_V0));
    TRY(dw(r, addc(R(NBA97_MATCH_INITIALIZE_AT), 0x474c), 4, 4, 0x8009b4c4,
           R(NBA97_MATCH_INITIALIZE_S0)));
  }
  /* 9B4C8..558 reload head for fields, advance ring, restore critical and
   * drain.
   */
  kn(&R(NBA97_MATCH_INITIALIZE_V1), 0x800c0000);
  TRY(rd(r, 0x800c56c4, 4, 4, 0x8009b4cc, &R(NBA97_MATCH_INITIALIZE_V1)));
  R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V1), 1);
  R(NBA97_MATCH_INITIALIZE_V0) =
      add(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
  R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V0), 5);
  kn(&R(NBA97_MATCH_INITIALIZE_AT), 0x80100000);
  R(NBA97_MATCH_INITIALIZE_AT) =
      add(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(dw(r, addc(R(NBA97_MATCH_INITIALIZE_AT), 0x4750), 4, 4, 0x8009b4e8,
         R(NBA97_MATCH_INITIALIZE_S0 + 2)));
  kn(&R(NBA97_MATCH_INITIALIZE_V1), 0x800c0000);
  TRY(rd(r, 0x800c56c4, 4, 4, 0x8009b4f0, &R(NBA97_MATCH_INITIALIZE_V1)));
  R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V1), 1);
  R(NBA97_MATCH_INITIALIZE_V0) =
      add(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
  R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V0), 5);
  kn(&R(NBA97_MATCH_INITIALIZE_AT), 0x80100000);
  R(NBA97_MATCH_INITIALIZE_AT) =
      add(R(NBA97_MATCH_INITIALIZE_AT), R(NBA97_MATCH_INITIALIZE_V0));
  TRY(dw(r, addc(R(NBA97_MATCH_INITIALIZE_AT), 0x4748), 4, 4, 0x8009b50c,
         R(NBA97_MATCH_INITIALIZE_S0 + 3)));
  kn(&R(NBA97_MATCH_INITIALIZE_V0), 0x800c0000);
  TRY(rd(r, 0x800c56c4, 4, 4, 0x8009b514, &R(NBA97_MATCH_INITIALIZE_V0)));
  kn(&R(NBA97_MATCH_INITIALIZE_A0), 0x800c0000);
  TRY(rd(r, 0x800c56cc, 4, 4, 0x8009b51c, &R(NBA97_MATCH_INITIALIZE_A0)));
  R(NBA97_MATCH_INITIALIZE_V0) =
      andc(addc(R(NBA97_MATCH_INITIALIZE_V0), 1), 63);
  kn(&R(NBA97_MATCH_INITIALIZE_AT), 0x800c0000);
  TRY(wr(r, 0x800c56c4, 4, 4, 0x8009b52c, R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(fixed(r, 0x8009b530, 0x800986f8, NBA97_GAME_GRAPHICS_SUBMIT_CRITICAL, 1,
            0, 0, R(0)));
  TRY(fixed(r, 0x8009b538, 0x8009b57c, NBA97_GAME_GRAPHICS_SUBMIT_DRAIN, 0, 0,
            0, R(0)));
  kn(&R(NBA97_MATCH_INITIALIZE_V0), 0x800c0000);
  TRY(rd(r, 0x800c56c4, 4, 4, 0x8009b544, &R(NBA97_MATCH_INITIALIZE_V0)));
  kn(&R(NBA97_MATCH_INITIALIZE_V1), 0x800c0000);
  TRY(rd(r, 0x800c56c8, 4, 4, 0x8009b54c, &R(NBA97_MATCH_INITIALIZE_V1)));
  R(NBA97_MATCH_INITIALIZE_V0) =
      andc(sub(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1)), 63);
  o->queued = 1;
done:
  o->return_v0 = R(NBA97_MATCH_INITIALIZE_V0);
  TRY(restore(r, 0x8009b55c, 32, NBA97_MATCH_INITIALIZE_RA,
              &o->restored_return_address));
  TRY(restore(r, 0x8009b560, 28, NBA97_MATCH_INITIALIZE_S0 + 3,
              &o->restored_s3));
  TRY(restore(r, 0x8009b564, 24, NBA97_MATCH_INITIALIZE_S0 + 2,
              &o->restored_s2));
  TRY(restore(r, 0x8009b568, 20, NBA97_MATCH_INITIALIZE_S0 + 1,
              &o->restored_s1));
  TRY(restore(r, 0x8009b56c, 16, NBA97_MATCH_INITIALIZE_S0, &o->restored_s0));
  R(NBA97_MATCH_INITIALIZE_SP) = addc(R(NBA97_MATCH_INITIALIZE_SP), 40);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15) {
    stop(r, 0x8009b574, 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  o->completed = 1;
  stop(r, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
