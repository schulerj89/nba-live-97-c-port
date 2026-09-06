#include "game_actor_contact_eligibility.h"

#include <limits.h>
#include <string.h>

#define SPECIAL_MODE UINT32_C(0x800fe8cc)
#define EXCLUDED_ID UINT32_C(0x800fe8ca)
#define PHASE UINT32_C(0x800fdb90)
#define OWNER UINT32_C(0x800fdbcc)

typedef struct Run {
  Nba97GameActorContactEligibilityContext *c;
  Nba97GameActorContactEligibilityProgress *p;
  Nba97GameActorContactEligibilityMachine m;
} Run;
#define R(i) (run->m.registers.gpr[(i)])
#define TRY(x)                                                                 \
  do {                                                                         \
    int s_ = (x);                                                              \
    if (s_ != NBA97_TEXT_COMPLETE)                                             \
      return s_;                                                               \
  } while (0)

static void pub(Run *r) { r->p->machine = r->m; }
static void stop(Run *r, uint32_t pc, uint32_t a, uint32_t e) {
  r->p->stopped_pc = pc;
  r->p->stopped_address = a;
  r->p->stopped_entry = e;
  pub(r);
}
static void known(Nba97GameActorContactEligibilityWord *v, uint32_t w) {
  v->word = w;
  v->known_mask = 15;
}
static int machine_ok(const Nba97GameActorContactEligibilityMachine *m) {
  unsigned i;
  if (m->registers.gpr[0].word || m->registers.gpr[0].known_mask != 15 ||
      m->hi.known_mask > 15 || m->lo.known_mask > 15)
    return 0;
  for (i = 0; i < 32; ++i)
    if (m->registers.gpr[i].known_mask > 15)
      return 0;
  return 1;
}
static int init(Nba97GameActorContactEligibilityContext *c,
                Nba97GameActorContactEligibilityProgress *p, Run *r) {
  size_t i, j;
  if (!p)
    return NBA97_TEXT_ARGUMENT;
  memset(p, 0, sizeof *p);
  if (!c || (!c->memory.region && c->memory.count) ||
      (!c->access_journal && c->access_journal_capacity) ||
      !machine_ok(&c->machine))
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < c->memory.count; ++i) {
    const Nba97GameTextRegion *a = &c->memory.region[i];
    if (!a->data || !a->size || a->size > UINT64_C(0x100000000) ||
        (uint64_t)a->base + a->size > UINT64_C(0x100000000))
      return NBA97_TEXT_ARGUMENT;
    for (j = 0; j < i; ++j) {
      const Nba97GameTextRegion *b = &c->memory.region[j];
      if ((uint64_t)a->base < (uint64_t)b->base + b->size &&
          (uint64_t)b->base < (uint64_t)a->base + a->size)
        return NBA97_TEXT_ARGUMENT;
    }
  }
  r->c = c;
  r->p = p;
  r->m = c->machine;
  pub(r);
  return NBA97_TEXT_COMPLETE;
}

static Nba97GameActorContactEligibilityWord
add(Nba97GameActorContactEligibilityWord l,
    Nba97GameActorContactEligibilityWord q) {
  Nba97GameActorContactEligibilityWord z;
  unsigned cm = 1, b;
  z.word = l.word + q.word;
  z.known_mask = 0;
  if (l.known_mask == 15 && q.known_mask == 15) {
    z.known_mask = 15;
    return z;
  }
  for (b = 0; b < 4; ++b) {
    unsigned nm = 0, fo = 0, first = 1, inv = 1,
             ls = (l.known_mask & (1u << b)) ? ((l.word >> (8 * b)) & 255) : 0,
             le = (l.known_mask & (1u << b)) ? ls : 255,
             rs = (q.known_mask & (1u << b)) ? ((q.word >> (8 * b)) & 255) : 0,
             re = (q.known_mask & (1u << b)) ? rs : 255, c;
    for (c = 0; c < 2; ++c)
      if (cm & (1u << c)) {
        unsigned a;
        for (a = ls; a <= le; ++a) {
          unsigned d;
          for (d = rs; d <= re; ++d) {
            unsigned s = a + d + c, o = s & 255;
            nm |= 1u << (s >> 8);
            if (first) {
              fo = o;
              first = 0;
            } else if (o != fo)
              inv = 0;
          }
        }
      }
    if (inv)
      z.known_mask = (uint8_t)(z.known_mask | (1u << b));
    cm = nm;
  }
  return z;
}
static Nba97GameActorContactEligibilityWord
sub(Nba97GameActorContactEligibilityWord l,
    Nba97GameActorContactEligibilityWord q) {
  Nba97GameActorContactEligibilityWord z;
  unsigned bm = 1, b;
  z.word = l.word - q.word;
  z.known_mask = 0;
  if (l.known_mask == 15 && q.known_mask == 15) {
    z.known_mask = 15;
    return z;
  }
  for (b = 0; b < 4; ++b) {
    unsigned nm = 0, fo = 0, first = 1, inv = 1,
             ls = (l.known_mask & (1u << b)) ? ((l.word >> (8 * b)) & 255) : 0,
             le = (l.known_mask & (1u << b)) ? ls : 255,
             rs = (q.known_mask & (1u << b)) ? ((q.word >> (8 * b)) & 255) : 0,
             re = (q.known_mask & (1u << b)) ? rs : 255, c;
    for (c = 0; c < 2; ++c)
      if (bm & (1u << c)) {
        unsigned a;
        for (a = ls; a <= le; ++a) {
          unsigned d;
          for (d = rs; d <= re; ++d) {
            int s = (int)a - (int)d - (int)c;
            unsigned o = (unsigned)s & 255;
            nm |= 1u << (s < 0);
            if (first) {
              fo = o;
              first = 0;
            } else if (o != fo)
              inv = 0;
          }
        }
      }
    if (inv)
      z.known_mask = (uint8_t)(z.known_mask | (1u << b));
    bm = nm;
  }
  return z;
}
static Nba97GameActorContactEligibilityWord
addc(Nba97GameActorContactEligibilityWord v, uint32_t c) {
  Nba97GameActorContactEligibilityWord q;
  known(&q, c);
  return add(v, q);
}
static Nba97GameActorContactEligibilityWord
sra(Nba97GameActorContactEligibilityWord v, unsigned n) {
  Nba97GameActorContactEligibilityWord z;
  unsigned b;
  z.word = v.word >> n;
  if (v.word & UINT32_C(0x80000000))
    z.word |= ~(UINT32_MAX >> n);
  z.known_mask = 0;
  for (b = 0; b < 4; ++b) {
    unsigned lo = 8 * b + n, hi = lo + 7, f = lo / 8,
             last = hi < 32 ? hi / 8 : 3, s;
    int k = 1;
    for (s = f; s <= last; ++s)
      if (!(v.known_mask & (1u << s)))
        k = 0;
    if (hi >= 32 && !(v.known_mask & 8))
      k = 0;
    if (k)
      z.known_mask = (uint8_t)(z.known_mask | (1u << b));
  }
  return z;
}
static int64_t sw(uint32_t w) {
  return w < UINT32_C(0x80000000) ? (int64_t)w
                                  : (int64_t)w - INT64_C(0x100000000);
}
static void sbounds(const Nba97GameActorContactEligibilityWord *v, int64_t *mn,
                    int64_t *mx) {
  uint32_t l = 0, h = 0;
  unsigned i;
  if (!(v->known_mask & 8)) {
    *mn = INT32_MIN;
    *mx = INT32_MAX;
    return;
  }
  for (i = 0; i < 4; ++i) {
    uint32_t b = (v->word >> (8 * i)) & 255;
    l |= ((v->known_mask & (1u << i)) ? b : 0) << (8 * i);
    h |= ((v->known_mask & (1u << i)) ? b : 255) << (8 * i);
  }
  *mn = sw(l);
  *mx = sw(h);
}
static Nba97GameActorContactEligibilityWord
slti(const Nba97GameActorContactEligibilityWord *v, int32_t c) {
  Nba97GameActorContactEligibilityWord z;
  int64_t mn, mx;
  sbounds(v, &mn, &mx);
  z.word = sw(v->word) < c;
  z.known_mask = 14;
  if (mx < c)
    known(&z, 1);
  else if (mn >= c)
    known(&z, 0);
  return z;
}
static Nba97GameActorContactEligibilityWord
sltiu(const Nba97GameActorContactEligibilityWord *v, uint32_t c) {
  Nba97GameActorContactEligibilityWord z;
  uint32_t mn = 0, mx = 0;
  unsigned i;
  for (i = 0; i < 4; ++i) {
    uint32_t b = (v->word >> (8 * i)) & 255;
    mn |= ((v->known_mask & (1u << i)) ? b : 0) << (8 * i);
    mx |= ((v->known_mask & (1u << i)) ? b : 255) << (8 * i);
  }
  z.word = v->word < c;
  z.known_mask = 14;
  if (mx < c)
    known(&z, 1);
  else if (mn >= c)
    known(&z, 0);
  return z;
}
static Nba97GameActorContactEligibilityWord
lh(Nba97GameActorContactEligibilityWord v) {
  uint32_t w = v.word & 65535;
  v.word = (w & 32768) ? w | UINT32_C(0xffff0000) : w;
  v.known_mask = (uint8_t)((v.known_mask & 3) | ((v.known_mask & 2) ? 12 : 0));
  return v;
}
static Nba97GameActorContactEligibilityWord
lbu(Nba97GameActorContactEligibilityWord v) {
  v.word &= 255;
  v.known_mask = (uint8_t)((v.known_mask & 1) | 14);
  return v;
}
static int decide0(Run *r, const Nba97GameActorContactEligibilityWord *v,
                   uint32_t pc, int *z) {
  unsigned i;
  for (i = 0; i < 4; ++i)
    if ((v->known_mask & (1u << i)) && ((v->word >> (8 * i)) & 255)) {
      *z = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (v->known_mask == 15) {
    *z = 1;
    return NBA97_TEXT_COMPLETE;
  }
  stop(r, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}
static int eq(Run *r, const Nba97GameActorContactEligibilityWord *a,
              const Nba97GameActorContactEligibilityWord *b, uint32_t pc,
              int *e) {
  unsigned i;
  for (i = 0; i < 4; ++i) {
    uint8_t m = (uint8_t)(1u << i);
    if ((a->known_mask & b->known_mask & m) &&
        (((a->word >> (8 * i)) & 255) != ((b->word >> (8 * i)) & 255))) {
      *e = 0;
      return NBA97_TEXT_COMPLETE;
    }
  }
  if (a->known_mask == 15 && b->known_mask == 15) {
    *e = a->word == b->word;
    return NBA97_TEXT_COMPLETE;
  }
  stop(r, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}
static uint32_t wmask(unsigned w) {
  return w == 4 ? UINT32_MAX : (UINT32_C(1) << (8 * w)) - 1;
}
static int spend(Run *r) {
  if (r->p->operations >= r->c->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++r->p->operations;
  return NBA97_TEXT_COMPLETE;
}
static void journal(Run *r, uint8_t k, uint32_t pc, uint32_t a, unsigned w,
                    const Nba97GameActorContactEligibilityWord *v) {
  size_t i = r->p->access_events++;
  if (i < r->c->access_journal_capacity) {
    Nba97GameActorContactEligibilityAccess *e = &r->c->access_journal[i];
    e->pc = pc;
    e->address = a;
    e->value = v->word & wmask(w);
    e->operation = r->p->operations;
    e->width = (uint8_t)w;
    e->known_mask = (uint8_t)(v->known_mask & ((1u << w) - 1));
    e->kind = k;
  }
}
static int locate(Run *r, uint32_t a, unsigned w, uint32_t pc, uint8_t **d,
                  uint8_t **k) {
  size_t i, j;
  stop(r, pc, a, 0);
  TRY(spend(r));
  ++r->p->accesses;
  if (a & (w - 1))
    return NBA97_TEXT_ALIGNMENT_TRAP;
  for (i = 0; i < r->c->memory.count; ++i) {
    Nba97GameTextRegion *g = &r->c->memory.region[i];
    uint64_t o = (uint64_t)a - g->base;
    if (a < g->base || o > g->size || w > g->size - (size_t)o)
      continue;
    *d = g->data + (size_t)o;
    *k = g->known ? g->known + (size_t)o : 0;
    if (*k)
      for (j = 0; j < w; ++j)
        if ((*k)[j] > 1)
          return NBA97_TEXT_ARGUMENT;
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}
static int rd(Run *r, uint32_t a, unsigned w, uint32_t pc,
              Nba97GameActorContactEligibilityWord *v) {
  Nba97GameActorContactEligibilityWord q = {0, 0};
  uint8_t *d, *k;
  unsigned i;
  TRY(locate(r, a, w, pc, &d, &k));
  for (i = 0; i < w; ++i) {
    q.word |= (uint32_t)d[i] << (8 * i);
    if (!k || k[i])
      q.known_mask = (uint8_t)(q.known_mask | (1u << i));
  }
  *v = q;
  ++r->p->reads;
  journal(r, NBA97_GAME_MATCH_CLOCKS_READ, pc, a, w, v);
  pub(r);
  return NBA97_TEXT_COMPLETE;
}
static int wr(Run *r, uint32_t a, uint32_t pc,
              const Nba97GameActorContactEligibilityWord *v) {
  uint8_t *d, *k;
  unsigned i;
  TRY(locate(r, a, 4, pc, &d, &k));
  if (!k && v->known_mask != 15)
    return NBA97_TEXT_ARGUMENT;
  for (i = 0; i < 4; ++i) {
    d[i] = (uint8_t)(v->word >> (8 * i));
    if (k)
      k[i] = (uint8_t)((v->known_mask >> i) & 1);
  }
  ++r->p->stores;
  journal(r, NBA97_GAME_MATCH_CLOCKS_STORE, pc, a, 4, v);
  pub(r);
  return NBA97_TEXT_COMPLETE;
}
static int addr(Run *r, Nba97GameActorContactEligibilityWord b, uint32_t o,
                uint32_t pc, uint32_t *a) {
  Nba97GameActorContactEligibilityWord v = addc(b, o);
  if (v.known_mask != 15) {
    stop(r, pc, v.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *a = v.word;
  return NBA97_TEXT_COMPLETE;
}
static int load_at(Run *run, unsigned reg, uint32_t off, unsigned width,
                   uint32_t pc, Nba97GameActorContactEligibilityWord *v) {
  uint32_t a;
  TRY(addr(run, R(reg), off, pc, &a));
  TRY(rd(run, a, width, pc, v));
  return NBA97_TEXT_COMPLETE;
}
static int call(Run *run, uint32_t pc, uint32_t entry, uint8_t kind,
                unsigned delayreg, Nba97GameActorContactEligibilityWord delay,
                int hasdelay) {
  Nba97GameActorContactEligibilityEvent e;
  int ok;
  known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8);
  if (hasdelay)
    R(delayreg) = delay;
  stop(run, pc, 0, entry);
  TRY(spend(run));
  memset(&e, 0, sizeof e);
  e.pc = pc;
  e.delay_slot_pc = pc + 4;
  e.entry = entry;
  e.operation = run->p->operations;
  e.invocation = run->p->call_count[kind] + 1;
  e.kind = kind;
  e.argument_count = 2;
  pub(run);
  if (!run->c->io)
    return NBA97_TEXT_IO_REFUSED;
  ok = run->c->io(run->c->user, &run->c->memory, &e, &run->m);
  pub(run);
  if (ok != 1)
    return NBA97_TEXT_IO_REFUSED;
  if (!machine_ok(&run->m))
    return NBA97_TEXT_ARGUMENT;
  ++run->p->callbacks_completed;
  ++run->p->call_count[kind];
  return NBA97_TEXT_COMPLETE;
}
static int restore(Run *run, uint32_t pc, uint32_t off, unsigned reg,
                   Nba97GameActorContactEligibilityWord *out) {
  uint32_t a;
  TRY(addr(run, R(NBA97_MATCH_INITIALIZE_SP), off, pc, &a));
  TRY(rd(run, a, 4, pc, &R(reg)));
  *out = R(reg);
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_actor_contact_eligibility(
    Nba97GameActorContactEligibilityContext *c,
    Nba97GameActorContactEligibilityProgress *p) {
  Run s, *run = &s;
  Nba97GameActorContactEligibilityWord v, b, x;
  uint32_t a;
  int z;
  TRY(init(c, p, run));
  /* 0x8005F948..0x8005F974: preserve load order and the unconditional RA spill
   * delay. */
  R(29) = addc(R(29), UINT32_C(0xffffffe0));
  p->frame_stack_pointer = R(29).word;
  TRY(addr(run, R(29), 0x14, UINT32_C(0x8005f94c), &a));
  TRY(wr(run, a, UINT32_C(0x8005f94c), &R(17)));
  R(17) = R(5);
  known(&R(2), UINT32_C(0x80100000));
  TRY(rd(run, SPECIAL_MODE, 2, UINT32_C(0x8005f958), &v));
  R(2) = lh(v);
  known(&R(5), UINT32_C(0x80100000));
  TRY(rd(run, EXCLUDED_ID, 2, UINT32_C(0x8005f960), &v));
  R(5) = lh(v);
  TRY(addr(run, R(29), 0x10, UINT32_C(0x8005f964), &a));
  TRY(wr(run, a, UINT32_C(0x8005f964), &R(16)));
  R(16) = R(4);
  R(4) = R(6);
  b = R(2);
  TRY(addr(run, R(29), 0x18, UINT32_C(0x8005f974), &a));
  TRY(wr(run, a, UINT32_C(0x8005f974), &R(31)));
  TRY(decide0(run, &b, UINT32_C(0x8005f970), &z));
  if (!z)
    goto ids;
  /* 0x8005F978..0x8005F9BC: retain the asymmetric phase-82 negative-owner state
   * gate. */
  known(&R(3), UINT32_C(0x80100000));
  TRY(rd(run, PHASE, 2, UINT32_C(0x8005f97c), &v));
  R(3) = lh(v);
  known(&R(2), 0x82);
  TRY(eq(run, &R(3), &R(2), UINT32_C(0x8005f984), &z));
  if (!z)
    goto teams;
  known(&R(5), UINT32_C(0x80100000));
  TRY(rd(run, OWNER, 2, UINT32_C(0x8005f990), &v));
  R(5) = lh(v);
  b = slti(&R(5), 0);
  known(&R(3), 3);
  TRY(decide0(run, &b, UINT32_C(0x8005f998), &z));
  if (z)
    goto ids;
  TRY(load_at(run, 16, 0x1a, 1, UINT32_C(0x8005f9a0), &v));
  R(2) = lbu(v);
  b = R(2);
  known(&R(2), 0);
  TRY(eq(run, &b, &R(3), UINT32_C(0x8005f9a8), &z));
  if (z)
    goto done;
  TRY(load_at(run, 17, 0x1a, 1, UINT32_C(0x8005f9b0), &v));
  R(2) = lbu(v);
  TRY(eq(run, &R(2), &R(3), UINT32_C(0x8005f9b8), &z));
  if (!z)
    goto teams;
ids:
  /* 0x8005F9C0..0x8005F9DC: compare the live signed-half selector with full
   * IDs. */
  TRY(load_at(run, 16, 0, 4, UINT32_C(0x8005f9c0), &R(2)));
  b = R(2);
  known(&R(2), 0);
  TRY(eq(run, &R(5), &b, UINT32_C(0x8005f9c8), &z));
  if (z)
    goto done;
  TRY(load_at(run, 17, 0, 4, UINT32_C(0x8005f9d0), &R(2)));
  b = R(2);
  known(&R(2), 0);
  TRY(eq(run, &R(5), &b, UINT32_C(0x8005f9d8), &z));
  if (z)
    goto done;
teams:
  /* 0x8005F9E0..0x8005FA40: unequal-team Y/geometry gates and typed action. */
  TRY(load_at(run, 16, 0xd9, 1, UINT32_C(0x8005f9e0), &v));
  R(3) = lbu(v);
  TRY(load_at(run, 17, 0xd9, 1, UINT32_C(0x8005f9e4), &v));
  R(2) = lbu(v);
  TRY(eq(run, &R(3), &R(2), UINT32_C(0x8005f9ec), &z));
  if (z)
    goto same_team;
  TRY(load_at(run, 17, 0xc, 4, UINT32_C(0x8005f9f4), &R(2)));
  TRY(load_at(run, 16, 0xc, 4, UINT32_C(0x8005f9f8), &R(3)));
  R(2) = sub(R(2), R(3));
  R(5) = sra(R(2), 8);
  R(2) = addc(R(5), 16);
  R(2) = sltiu(&R(2), 33);
  b = R(2);
  known(&R(2), 0);
  TRY(decide0(run, &b, UINT32_C(0x8005fa10), &z));
  if (z)
    goto done;
  TRY(call(run, UINT32_C(0x8005fa18), UINT32_C(0x8007066c),
           NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_GEOMETRY_8007066C, 0, v, 0));
  R(2) = slti(&R(2), 17);
  b = R(2);
  R(4) = R(16);
  TRY(decide0(run, &b, UINT32_C(0x8005fa24), &z));
  if (z)
    goto reject;
  TRY(call(run, UINT32_C(0x8005fa2c), UINT32_C(0x8005f888),
           NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_OTHER_TEAM_8005F888, 5, R(17),
           1));
  R(2).word &= 255;
  R(2).known_mask = (uint8_t)((R(2).known_mask & 1) | 14);
  goto done;
reject:
  known(&R(2), 0);
  goto done;
same_team:
  /* 0x8005FA44..0x8005FA8C: symmetric Y window, original one-sided X gate,
   * geometry, and same-team action. */
  TRY(load_at(run, 17, 0xc, 4, UINT32_C(0x8005fa44), &R(2)));
  TRY(load_at(run, 16, 0xc, 4, UINT32_C(0x8005fa48), &R(3)));
  R(2) = sub(R(2), R(3));
  R(5) = sra(R(2), 8);
  R(2) = addc(R(5), 8);
  R(2) = sltiu(&R(2), 17);
  b = R(2);
  x = slti(&R(4), 9);
  R(2) = x;
  TRY(decide0(run, &b, UINT32_C(0x8005fa60), &z));
  if (z)
    goto reject;
  b = R(2);
  known(&R(2), 0);
  TRY(decide0(run, &b, UINT32_C(0x8005fa68), &z));
  if (z)
    goto done;
  TRY(call(run, UINT32_C(0x8005fa70), UINT32_C(0x8007066c),
           NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_GEOMETRY_8007066C, 0, v, 0));
  R(2) = slti(&R(2), 9);
  b = R(2);
  R(4) = R(16);
  TRY(decide0(run, &b, UINT32_C(0x8005fa7c), &z));
  if (z)
    goto reject;
  TRY(call(run, UINT32_C(0x8005fa84), UINT32_C(0x8005f328),
           NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_SAME_TEAM_8005F328, 5, R(17),
           1));
  R(2).word &= 255;
  R(2).known_mask = (uint8_t)((R(2).known_mask & 1) | 14);
done:
  /* 0x8005FA90..0x8005FAA4: restore through live SP and consume live RA after
   * +0x20. */
  TRY(restore(run, UINT32_C(0x8005fa90), 0x18, 31,
              &p->restored_return_address));
  TRY(restore(run, UINT32_C(0x8005fa94), 0x14, 17, &p->restored_s1));
  TRY(restore(run, UINT32_C(0x8005fa98), 0x10, 16, &p->restored_s0));
  R(29) = addc(R(29), 0x20);
  if (R(31).known_mask != 15) {
    stop(run, UINT32_C(0x8005faa0), 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  p->completed = 1;
  stop(run, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
