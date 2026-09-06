#include "game_display_environment.h"
#include <string.h>
#define R(i) (r->m.registers.gpr[(i)])
#define TRY(x)                                                                 \
  do {                                                                         \
    int z_ = (x);                                                              \
    if (z_ != NBA97_TEXT_COMPLETE)                                             \
      return z_;                                                               \
  } while (0)
typedef struct Run {
  Nba97GameDisplayEnvironmentContext *c;
  Nba97GameDisplayEnvironmentProgress *o;
  Nba97GameDisplayEnvironmentMachine m;
} Run;
static void pub(Run *r) { r->o->machine = r->m; }
static void stop(Run *r, uint32_t pc, uint32_t a, uint32_t e) {
  r->o->stopped_pc = pc;
  r->o->stopped_address = a;
  r->o->stopped_entry = e;
  pub(r);
}
static void known(Nba97GameDisplayEnvironmentWord *v, uint32_t x) {
  v->word = x;
  v->known_mask = 15;
}
static int validm(const Nba97GameDisplayEnvironmentMachine *m) {
  unsigned i;
  if (m->registers.gpr[0].word || m->registers.gpr[0].known_mask != 15 ||
      m->hi.known_mask > 15 || m->lo.known_mask > 15)
    return 0;
  for (i = 0; i < 32; i++)
    if (m->registers.gpr[i].known_mask > 15)
      return 0;
  return 1;
}
static int init(Nba97GameDisplayEnvironmentContext *c,
                Nba97GameDisplayEnvironmentProgress *o, Run *r) {
  size_t i, j;
  if (!o)
    return NBA97_TEXT_ARGUMENT;
  memset(o, 0, sizeof *o);
  if (!c || (!c->memory.region && c->memory.count) ||
      (!c->access_journal && c->access_journal_capacity) ||
      !validm(&c->machine))
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
static Nba97GameDisplayEnvironmentWord add(Nba97GameDisplayEnvironmentWord a,
                                           Nba97GameDisplayEnvironmentWord b) {
  Nba97GameDisplayEnvironmentWord q;
  unsigned i;
  q.word = a.word + b.word;
  q.known_mask = 0;
  for (i = 0; i < 4; i++) {
    uint8_t bit = (uint8_t)(1u << i);
    if ((a.known_mask & ((1u << (i + 1)) - 1)) == ((1u << (i + 1)) - 1) &&
        (b.known_mask & ((1u << (i + 1)) - 1)) == ((1u << (i + 1)) - 1))
      q.known_mask = (uint8_t)(q.known_mask | bit);
  }
  return q;
}
static Nba97GameDisplayEnvironmentWord addc(Nba97GameDisplayEnvironmentWord a,
                                            uint32_t x) {
  Nba97GameDisplayEnvironmentWord b;
  known(&b, x);
  return add(a, b);
}
static Nba97GameDisplayEnvironmentWord shl(Nba97GameDisplayEnvironmentWord a,
                                           unsigned n) {
  Nba97GameDisplayEnvironmentWord q;
  uint32_t kb = 0, s;
  unsigned i;
  q.word = a.word << n;
  for (i = 0; i < 4; i++)
    if (a.known_mask & (1u << i))
      kb |= 0xffu << (8 * i);
  s = (kb << n) | ((1u << n) - 1);
  q.known_mask = 0;
  for (i = 0; i < 4; i++)
    if (((s >> (8 * i)) & 255) == 255)
      q.known_mask = (uint8_t)(q.known_mask | (1u << i));
  return q;
}
static Nba97GameDisplayEnvironmentWord
sar16(Nba97GameDisplayEnvironmentWord a) {
  Nba97GameDisplayEnvironmentWord q;
  q.word =
      (a.word & 0x80000000u) ? ((a.word >> 16) | 0xffff0000u) : (a.word >> 16);
  q.known_mask =
      (uint8_t)(((a.known_mask >> 2) & 3) | ((a.known_mask & 8) ? 12 : 0));
  return q;
}
static Nba97GameDisplayEnvironmentWord andi(Nba97GameDisplayEnvironmentWord a,
                                            uint32_t mask) {
  Nba97GameDisplayEnvironmentWord q;
  unsigned i;
  q.word = a.word & mask;
  q.known_mask = 0;
  for (i = 0; i < 4; i++) {
    uint32_t m = (mask >> (8 * i)) & 255;
    if (m == 0 || (a.known_mask & (1u << i)))
      q.known_mask = (uint8_t)(q.known_mask | (1u << i));
  }
  return q;
}
static Nba97GameDisplayEnvironmentWord bor(Nba97GameDisplayEnvironmentWord a,
                                           Nba97GameDisplayEnvironmentWord b) {
  Nba97GameDisplayEnvironmentWord q;
  unsigned i;
  q.word = a.word | b.word;
  q.known_mask = 0;
  for (i = 0; i < 4; i++) {
    uint32_t x = (a.word >> (8 * i)) & 255, y = (b.word >> (8 * i)) & 255;
    int ak = a.known_mask & (1u << i), bk = b.known_mask & (1u << i);
    if ((ak && bk) || (ak && x == 255) || (bk && y == 255))
      q.known_mask = (uint8_t)(q.known_mask | (1u << i));
  }
  return q;
}
static Nba97GameDisplayEnvironmentWord orc(Nba97GameDisplayEnvironmentWord a,
                                           uint32_t x) {
  Nba97GameDisplayEnvironmentWord b;
  known(&b, x);
  return bor(a, b);
}
static int32_t s32(uint32_t x) {
  return x < 0x80000000u ? (int32_t)x : -1 - (int32_t)~x;
}
static Nba97GameDisplayEnvironmentWord pred(int x,
                                            Nba97GameDisplayEnvironmentWord a) {
  Nba97GameDisplayEnvironmentWord q;
  q.word = (uint32_t)x;
  q.known_mask = a.known_mask == 15 ? 15 : 14;
  return q;
}
static Nba97GameDisplayEnvironmentWord
pred2(int x, Nba97GameDisplayEnvironmentWord a,
      Nba97GameDisplayEnvironmentWord b) {
  Nba97GameDisplayEnvironmentWord q;
  q.word = (uint32_t)x;
  q.known_mask = (a.known_mask == 15 && b.known_mask == 15) ? 15 : 14;
  return q;
}
static int spend(Run *r) {
  if (r->o->operations >= r->c->operation_budget)
    return NBA97_TEXT_LIMIT;
  r->o->operations++;
  return NBA97_TEXT_COMPLETE;
}
static void journal(Run *r, uint8_t k, uint32_t pc, uint32_t a, unsigned w,
                    Nba97GameDisplayEnvironmentWord v) {
  size_t i = r->o->access_events++;
  if (i < r->c->access_journal_capacity) {
    Nba97GameDisplayEnvironmentAccess *e = &r->c->access_journal[i];
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
              Nba97GameDisplayEnvironmentWord *v) {
  uint8_t *d, *k;
  Nba97GameDisplayEnvironmentWord q = {0, 0};
  unsigned i;
  TRY(loc(r, a, w, al, pc, &d, &k));
  for (i = 0; i < w; i++) {
    q.word |= (uint32_t)d[i] << (8 * i);
    if (!k || k[i])
      q.known_mask = (uint8_t)(q.known_mask | (1u << i));
  }
  *v = q;
  r->o->reads++;
  journal(r, NBA97_GAME_DISPLAY_ENVIRONMENT_READ, pc, a, w, q);
  pub(r);
  return NBA97_TEXT_COMPLETE;
}
static int wr(Run *r, uint32_t a, unsigned w, unsigned al, uint32_t pc,
              Nba97GameDisplayEnvironmentWord v) {
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
  journal(r, NBA97_GAME_DISPLAY_ENVIRONMENT_STORE, pc, a, w, v);
  pub(r);
  return NBA97_TEXT_COMPLETE;
}
static int addr(Run *r, Nba97GameDisplayEnvironmentWord a, uint32_t pc,
                uint32_t *out) {
  if (a.known_mask != 15) {
    stop(r, pc, a.word, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  *out = a.word;
  return NBA97_TEXT_COMPLETE;
}
static int drd(Run *r, Nba97GameDisplayEnvironmentWord a, unsigned w,
               unsigned al, uint32_t pc, Nba97GameDisplayEnvironmentWord *v) {
  uint32_t x;
  TRY(addr(r, a, pc, &x));
  return rd(r, x, w, al, pc, v);
}
static int dwr(Run *r, Nba97GameDisplayEnvironmentWord a, unsigned w,
               unsigned al, uint32_t pc, Nba97GameDisplayEnvironmentWord v) {
  uint32_t x;
  TRY(addr(r, a, pc, &x));
  return wr(r, x, w, al, pc, v);
}
static Nba97GameDisplayEnvironmentWord lbu(Nba97GameDisplayEnvironmentWord v) {
  v.word &= 255;
  v.known_mask = (uint8_t)((v.known_mask & 1) | 14);
  return v;
}
static Nba97GameDisplayEnvironmentWord lhu(Nba97GameDisplayEnvironmentWord v) {
  v.word &= 65535;
  v.known_mask = (uint8_t)((v.known_mask & 3) | 12);
  return v;
}
static Nba97GameDisplayEnvironmentWord lh(Nba97GameDisplayEnvironmentWord v) {
  v.word =
      (v.word & 0x8000) ? ((v.word & 65535) | 0xffff0000u) : (v.word & 65535);
  v.known_mask = (uint8_t)((v.known_mask & 1) | ((v.known_mask & 2) ? 14 : 0));
  return v;
}
static int eqdec(Run *r, Nba97GameDisplayEnvironmentWord a,
                 Nba97GameDisplayEnvironmentWord b, uint32_t pc, int *eq) {
  unsigned i;
  for (i = 0; i < 4; i++)
    if ((a.known_mask & (1u << i)) && (b.known_mask & (1u << i)) &&
        (((a.word ^ b.word) >> (8 * i)) & 255)) {
      *eq = 0;
      return NBA97_TEXT_COMPLETE;
    }
  if (a.known_mask == 15 && b.known_mask == 15) {
    *eq = a.word == b.word;
    return NBA97_TEXT_COMPLETE;
  }
  stop(r, pc, 0, 0);
  return NBA97_TEXT_UNKNOWN;
}
static int zdec(Run *r, Nba97GameDisplayEnvironmentWord a, uint32_t pc,
                int *z) {
  Nba97GameDisplayEnvironmentWord q;
  known(&q, 0);
  return eqdec(r, a, q, pc, z);
}
static int invoke(Run *r, uint32_t pc, Nba97GameDisplayEnvironmentWord entry,
                  uint8_t kind, uint8_t argc, int delay, unsigned reg,
                  Nba97GameDisplayEnvironmentWord dv) {
  Nba97GameDisplayEnvironmentEvent e;
  int ok;
  known(&R(NBA97_MATCH_INITIALIZE_RA), pc + 8);
  if (delay)
    R(reg) = dv;
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
  if (!validm(&r->m))
    return NBA97_TEXT_ARGUMENT;
  r->o->callbacks_completed++;
  r->o->call_count[kind]++;
  return NBA97_TEXT_COMPLETE;
}
static int fixedcall(Run *r, uint32_t pc, uint32_t e, uint8_t k, uint8_t n,
                     int delay, unsigned reg,
                     Nba97GameDisplayEnvironmentWord v) {
  Nba97GameDisplayEnvironmentWord x;
  known(&x, e);
  return invoke(r, pc, x, k, n, delay, reg, v);
}
static int cmp16(Run *r, uint32_t ca, uint32_t off, uint32_t pca, uint32_t pcb,
                 uint32_t branchpc, int *eq) {
  known(&R(NBA97_MATCH_INITIALIZE_V0),
        pca == 0x80099d7c ? 0x800c5634 : 0x800c0000);
  TRY(rd(r, ca, 2, 2, pca, &R(NBA97_MATCH_INITIALIZE_V0)));
  R(NBA97_MATCH_INITIALIZE_V0) = lhu(R(NBA97_MATCH_INITIALIZE_V0));
  TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), off), 2, 2, pcb,
          &R(NBA97_MATCH_INITIALIZE_V1)));
  R(NBA97_MATCH_INITIALIZE_V1) = lh(R(NBA97_MATCH_INITIALIZE_V1));
  R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V0), 16);
  R(NBA97_MATCH_INITIALIZE_V0) = sar16(R(NBA97_MATCH_INITIALIZE_V0));
  return eqdec(r, R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1),
               branchpc, eq);
}
static int restore(Run *r, uint32_t pc, uint32_t off, unsigned reg,
                   Nba97GameDisplayEnvironmentWord *out) {
  Nba97GameDisplayEnvironmentWord a = addc(R(NBA97_MATCH_INITIALIZE_SP), off),
                                  v;
  TRY(drd(r, a, 4, 4, pc, &v));
  R(reg) = v;
  *out = v;
  return NBA97_TEXT_COMPLETE;
}

int nba97_game_display_environment(Nba97GameDisplayEnvironmentContext *c,
                                   Nba97GameDisplayEnvironmentProgress *o) {
  Run s, *r = &s;
  Nba97GameDisplayEnvironmentWord v, cmd, branch;
  int e, z;
  TRY(init(c, o, r));
  /* 0x80099CA4..0x80099CD8: frame saves, debug gate, and its live S3 delay. */
  R(NBA97_MATCH_INITIALIZE_SP) = addc(R(NBA97_MATCH_INITIALIZE_SP), 0xffffffd8);
  o->frame_stack_pointer = R(NBA97_MATCH_INITIALIZE_SP).word;
  TRY(dwr(r, addc(R(NBA97_MATCH_INITIALIZE_SP), 16), 4, 4, 0x80099ca8,
          R(NBA97_MATCH_INITIALIZE_S0)));
  R(NBA97_MATCH_INITIALIZE_S0) = R(NBA97_MATCH_INITIALIZE_A0);
  known(&R(NBA97_MATCH_INITIALIZE_V0), 0x800c0000);
  R(NBA97_MATCH_INITIALIZE_V0) = addc(R(NBA97_MATCH_INITIALIZE_V0), 0x55c2);
  TRY(dwr(r, addc(R(NBA97_MATCH_INITIALIZE_SP), 32), 4, 4, 0x80099cb8,
          R(NBA97_MATCH_INITIALIZE_RA)));
  TRY(dwr(r, addc(R(NBA97_MATCH_INITIALIZE_SP), 28), 4, 4, 0x80099cbc,
          R(NBA97_MATCH_INITIALIZE_S0 + 3)));
  TRY(dwr(r, addc(R(NBA97_MATCH_INITIALIZE_SP), 24), 4, 4, 0x80099cc0,
          R(NBA97_MATCH_INITIALIZE_S0 + 2)));
  TRY(dwr(r, addc(R(NBA97_MATCH_INITIALIZE_SP), 20), 4, 4, 0x80099cc4,
          R(NBA97_MATCH_INITIALIZE_S0 + 1)));
  TRY(rd(r, 0x800c55c2, 1, 1, 0x80099cc8, &v));
  R(NBA97_MATCH_INITIALIZE_V0) = lbu(v);
  R(NBA97_MATCH_INITIALIZE_V0) =
      pred(R(NBA97_MATCH_INITIALIZE_V0).word < 2, R(NBA97_MATCH_INITIALIZE_V0));
  branch = R(NBA97_MATCH_INITIALIZE_V0);
  known(&R(NBA97_MATCH_INITIALIZE_S0 + 3), 0x08000000);
  TRY(zdec(r, branch, 0x80099cd4, &z));
  if (z) {
    known(&R(NBA97_MATCH_INITIALIZE_A0), 0x800283a0);
    known(&R(NBA97_MATCH_INITIALIZE_V0), 0x800c0000);
    TRY(rd(r, 0x800c55bc, 4, 4, 0x80099ce8, &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(invoke(r, 0x80099cf0, R(NBA97_MATCH_INITIALIZE_V0),
               NBA97_GAME_DISPLAY_ENVIRONMENT_DEBUG, 2, 1,
               NBA97_MATCH_INITIALIZE_A1, R(NBA97_MATCH_INITIALIZE_S0)));
  }
  /* 0x80099CF8..0x80099D70: hardware-dependent origin packing and GP1(05). */
  known(&R(NBA97_MATCH_INITIALIZE_V0), 0x800c0000);
  TRY(rd(r, 0x800c55c0, 1, 1, 0x80099cfc, &v));
  R(NBA97_MATCH_INITIALIZE_V0) = addc(lbu(v), 0xffffffff);
  R(NBA97_MATCH_INITIALIZE_V0) =
      pred(R(NBA97_MATCH_INITIALIZE_V0).word < 2, R(NBA97_MATCH_INITIALIZE_V0));
  TRY(zdec(r, R(NBA97_MATCH_INITIALIZE_V0), 0x80099d0c, &z));
  if (!z) {
    TRY(fixedcall(r, 0x80099d14, 0x8009a8a8,
                  NBA97_GAME_DISPLAY_ENVIRONMENT_ORIGIN_HELPER, 1, 1,
                  NBA97_MATCH_INITIALIZE_A0, R(NBA97_MATCH_INITIALIZE_S0)));
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 2), 2, 2, 0x80099d1c, &v));
    R(NBA97_MATCH_INITIALIZE_V1) = lhu(v);
    R(NBA97_MATCH_INITIALIZE_V0) = andi(R(NBA97_MATCH_INITIALIZE_V0), 0xfff);
    R(NBA97_MATCH_INITIALIZE_V1) =
        shl(andi(R(NBA97_MATCH_INITIALIZE_V1), 0xfff), 12);
    R(NBA97_MATCH_INITIALIZE_V1) =
        bor(R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0));
    known(&R(NBA97_MATCH_INITIALIZE_V0), 0x05000000);
    cmd = bor(R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0));
  } else {
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 2), 2, 2, 0x80099d38,
            &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_V0) = lhu(R(NBA97_MATCH_INITIALIZE_V0));
    TRY(drd(r, R(NBA97_MATCH_INITIALIZE_S0), 2, 2, 0x80099d3c,
            &R(NBA97_MATCH_INITIALIZE_V1)));
    R(NBA97_MATCH_INITIALIZE_V1) = lhu(R(NBA97_MATCH_INITIALIZE_V1));
    R(NBA97_MATCH_INITIALIZE_V0) = andi(R(NBA97_MATCH_INITIALIZE_V0), 0x3ff);
    R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V0), 10);
    R(NBA97_MATCH_INITIALIZE_V1) = andi(R(NBA97_MATCH_INITIALIZE_V1), 0x3ff);
    R(NBA97_MATCH_INITIALIZE_V0) =
        bor(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
    known(&R(NBA97_MATCH_INITIALIZE_V1), 0x05000000);
    cmd = bor(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
  }
  R(NBA97_MATCH_INITIALIZE_A0) = cmd;
  o->origin_command = cmd;
  known(&R(NBA97_MATCH_INITIALIZE_V0), 0x800c0000);
  TRY(rd(r, 0x800c55b8, 4, 4, 0x80099d5c, &R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_V0), 16), 4, 4, 0x80099d64,
          &R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(invoke(r, 0x80099d6c, R(NBA97_MATCH_INITIALIZE_V0),
             NBA97_GAME_DISPLAY_ENVIRONMENT_GPU_COMMAND, 1, 0, 0,
             R(NBA97_MATCH_INITIALIZE_ZERO)));
  /* Cache rectangle comparisons short-circuit in source order. */
  TRY(cmp16(r, 0x800c5634, 8, 0x80099d7c, 0x80099d80, 0x80099d8c, &e));
  if (e) {
    TRY(cmp16(r, 0x800c5636, 10, 0x80099d98, 0x80099d9c, 0x80099da8, &e));
    if (e) {
      TRY(cmp16(r, 0x800c5638, 12, 0x80099db4, 0x80099db8, 0x80099dc4, &e));
      if (e)
        TRY(cmp16(r, 0x800c563a, 14, 0x80099dd0, 0x80099dd4, 0x80099de0, &e));
    }
  }
  if (!e) {
    o->screen_rectangle_changed = 1;
    TRY(fixedcall(r, 0x80099de8, 0x800985cc,
                  NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE, 0, 0, 0,
                  R(NBA97_MATCH_INITIALIZE_ZERO)));
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 8), 2, 2, 0x80099df0, &v));
    R(NBA97_MATCH_INITIALIZE_V1) = lh(v);
    TRY(dwr(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 18), 1, 1, 0x80099df4,
            R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V1), 2);
    R(NBA97_MATCH_INITIALIZE_V0) =
        add(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
    R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V0), 1);
    R(NBA97_MATCH_INITIALIZE_A1) = addc(R(NBA97_MATCH_INITIALIZE_V0), 608);
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 18), 1, 1, 0x80099e08, &v));
    R(NBA97_MATCH_INITIALIZE_V0) = lbu(v);
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 10), 2, 2, 0x80099e0c, &v));
    R(NBA97_MATCH_INITIALIZE_A0) = lh(v);
    branch = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_S0 + 1) = addc(R(NBA97_MATCH_INITIALIZE_A0), 19);
    TRY(zdec(r, branch, 0x80099e10, &z));
    if (z)
      R(NBA97_MATCH_INITIALIZE_S0 + 1) = addc(R(NBA97_MATCH_INITIALIZE_A0), 16);
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 12), 2, 2, 0x80099e1c, &v));
    R(NBA97_MATCH_INITIALIZE_V1) = lh(v);
    branch = R(NBA97_MATCH_INITIALIZE_V1);
    R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V1), 2);
    TRY(zdec(r, branch, 0x80099e24, &z));
    if (!z) {
      R(NBA97_MATCH_INITIALIZE_V0) =
          add(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_V1));
      R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V0), 1);
      R(NBA97_MATCH_INITIALIZE_A2) =
          add(R(NBA97_MATCH_INITIALIZE_A1), R(NBA97_MATCH_INITIALIZE_V0));
    } else
      R(NBA97_MATCH_INITIALIZE_A2) = addc(R(NBA97_MATCH_INITIALIZE_A1), 2560);
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 14), 2, 2, 0x80099e40, &v));
    R(NBA97_MATCH_INITIALIZE_V0) = lh(v);
    branch = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_S0 + 2) =
        add(R(NBA97_MATCH_INITIALIZE_S0 + 1), R(NBA97_MATCH_INITIALIZE_V0));
    TRY(zdec(r, branch, 0x80099e48, &z));
    if (z)
      R(NBA97_MATCH_INITIALIZE_S0 + 2) =
          addc(R(NBA97_MATCH_INITIALIZE_S0 + 1), 240);
    /* Signed source clamps for horizontal origin/end. Every branch delay writes
     * its destination before a possibly unknown decision. */
    R(NBA97_MATCH_INITIALIZE_V0) =
        pred(s32(R(NBA97_MATCH_INITIALIZE_A1).word) < 500,
             R(NBA97_MATCH_INITIALIZE_A1));
    branch = R(NBA97_MATCH_INITIALIZE_V0);
    known(&R(NBA97_MATCH_INITIALIZE_A0), 500);
    TRY(zdec(r, branch, 0x80099e58, &z));
    if (z) {
      R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_A1);
      R(NBA97_MATCH_INITIALIZE_V0) =
          pred(s32(R(NBA97_MATCH_INITIALIZE_A0).word) < 3291,
               R(NBA97_MATCH_INITIALIZE_A0));
      branch = R(NBA97_MATCH_INITIALIZE_V0);
      R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_A0);
      TRY(zdec(r, branch, 0x80099e68, &z));
      if (z) {
        known(&R(NBA97_MATCH_INITIALIZE_A0), 3290);
        R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_A0);
      }
    } else
      R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_A0);
    R(NBA97_MATCH_INITIALIZE_V1) = addc(R(NBA97_MATCH_INITIALIZE_A1), 80);
    R(NBA97_MATCH_INITIALIZE_V0) =
        pred2(s32(R(NBA97_MATCH_INITIALIZE_A2).word) <
                  s32(R(NBA97_MATCH_INITIALIZE_V1).word),
              R(NBA97_MATCH_INITIALIZE_A2), R(NBA97_MATCH_INITIALIZE_V1));
    branch = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) =
        pred(s32(R(NBA97_MATCH_INITIALIZE_S0 + 1).word) < 16,
             R(NBA97_MATCH_INITIALIZE_S0 + 1));
    TRY(zdec(r, branch, 0x80099e80, &z));
    if (z) {
      R(NBA97_MATCH_INITIALIZE_V1) = R(NBA97_MATCH_INITIALIZE_A2);
      R(NBA97_MATCH_INITIALIZE_V0) =
          pred(s32(R(NBA97_MATCH_INITIALIZE_V1).word) < 3291,
               R(NBA97_MATCH_INITIALIZE_V1));
      branch = R(NBA97_MATCH_INITIALIZE_V0);
      R(NBA97_MATCH_INITIALIZE_V0) =
          pred(s32(R(NBA97_MATCH_INITIALIZE_S0 + 1).word) < 16,
               R(NBA97_MATCH_INITIALIZE_S0 + 1));
      TRY(zdec(r, branch, 0x80099e90, &z));
      if (z)
        known(&R(NBA97_MATCH_INITIALIZE_V1), 3290);
    }
    R(NBA97_MATCH_INITIALIZE_A2) = R(NBA97_MATCH_INITIALIZE_V1);
    /* Signed vertical clamps retain PAL-dependent maxima. */
    TRY(zdec(r, R(NBA97_MATCH_INITIALIZE_V0), 0x80099e9c, &z));
    if (!z)
      known(&R(NBA97_MATCH_INITIALIZE_A0), 16);
    else {
      TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 18), 1, 1, 0x80099ea4,
              &R(NBA97_MATCH_INITIALIZE_V0)));
      R(NBA97_MATCH_INITIALIZE_V0) = lbu(R(NBA97_MATCH_INITIALIZE_V0));
      branch = R(NBA97_MATCH_INITIALIZE_V0);
      R(NBA97_MATCH_INITIALIZE_V0) =
          pred(s32(R(NBA97_MATCH_INITIALIZE_S0 + 1).word) < 311,
               R(NBA97_MATCH_INITIALIZE_S0 + 1));
      TRY(zdec(r, branch, 0x80099eac, &z));
      if (!z) {
        TRY(zdec(r, R(NBA97_MATCH_INITIALIZE_V0), 0x80099eb4, &z));
        if (!z)
          R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0 + 1);
        else {
          TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 18), 1, 1, 0x80099ed0,
                  &R(NBA97_MATCH_INITIALIZE_V0)));
          R(NBA97_MATCH_INITIALIZE_V0) = lbu(R(NBA97_MATCH_INITIALIZE_V0));
          branch = R(NBA97_MATCH_INITIALIZE_V0);
          known(&R(NBA97_MATCH_INITIALIZE_A0), 256);
          TRY(zdec(r, branch, 0x80099ed8, &z));
          if (!z)
            known(&R(NBA97_MATCH_INITIALIZE_A0), 310);
        }
      } else {
        R(NBA97_MATCH_INITIALIZE_V0) =
            pred(s32(R(NBA97_MATCH_INITIALIZE_S0 + 1).word) < 257,
                 R(NBA97_MATCH_INITIALIZE_S0 + 1));
        TRY(zdec(r, R(NBA97_MATCH_INITIALIZE_V0), 0x80099ec8, &z));
        if (!z)
          R(NBA97_MATCH_INITIALIZE_A0) = R(NBA97_MATCH_INITIALIZE_S0 + 1);
        else {
          TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 18), 1, 1, 0x80099ed0,
                  &R(NBA97_MATCH_INITIALIZE_V0)));
          R(NBA97_MATCH_INITIALIZE_V0) = lbu(R(NBA97_MATCH_INITIALIZE_V0));
          branch = R(NBA97_MATCH_INITIALIZE_V0);
          known(&R(NBA97_MATCH_INITIALIZE_A0), 256);
          TRY(zdec(r, branch, 0x80099ed8, &z));
          if (!z)
            known(&R(NBA97_MATCH_INITIALIZE_A0), 310);
        }
      }
    }
    R(NBA97_MATCH_INITIALIZE_S0 + 1) = R(NBA97_MATCH_INITIALIZE_A0);
    R(NBA97_MATCH_INITIALIZE_V1) = addc(R(NBA97_MATCH_INITIALIZE_S0 + 1), 2);
    R(NBA97_MATCH_INITIALIZE_V0) =
        pred2(s32(R(NBA97_MATCH_INITIALIZE_S0 + 2).word) <
                  s32(R(NBA97_MATCH_INITIALIZE_V1).word),
              R(NBA97_MATCH_INITIALIZE_S0 + 2), R(NBA97_MATCH_INITIALIZE_V1));
    TRY(zdec(r, R(NBA97_MATCH_INITIALIZE_V0), 0x80099f00, &z));
    if (z) {
      TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 18), 1, 1, 0x80099f08,
              &R(NBA97_MATCH_INITIALIZE_V0)));
      R(NBA97_MATCH_INITIALIZE_V0) = lbu(R(NBA97_MATCH_INITIALIZE_V0));
      branch = R(NBA97_MATCH_INITIALIZE_V0);
      R(NBA97_MATCH_INITIALIZE_V0) =
          pred(s32(R(NBA97_MATCH_INITIALIZE_S0 + 2).word) < 313,
               R(NBA97_MATCH_INITIALIZE_S0 + 2));
      TRY(zdec(r, branch, 0x80099f10, &z));
      if (!z) {
        TRY(zdec(r, R(NBA97_MATCH_INITIALIZE_V0), 0x80099f18, &z));
        if (!z)
          R(NBA97_MATCH_INITIALIZE_V1) = R(NBA97_MATCH_INITIALIZE_S0 + 2);
        else {
          TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 18), 1, 1, 0x80099f34,
                  &R(NBA97_MATCH_INITIALIZE_V0)));
          R(NBA97_MATCH_INITIALIZE_V0) = lbu(R(NBA97_MATCH_INITIALIZE_V0));
          branch = R(NBA97_MATCH_INITIALIZE_V0);
          known(&R(NBA97_MATCH_INITIALIZE_V1), 258);
          TRY(zdec(r, branch, 0x80099f3c, &z));
          if (!z)
            known(&R(NBA97_MATCH_INITIALIZE_V1), 312);
        }
      } else {
        R(NBA97_MATCH_INITIALIZE_V0) =
            pred(s32(R(NBA97_MATCH_INITIALIZE_S0 + 2).word) < 259,
                 R(NBA97_MATCH_INITIALIZE_S0 + 2));
        TRY(zdec(r, R(NBA97_MATCH_INITIALIZE_V0), 0x80099f2c, &z));
        if (!z)
          R(NBA97_MATCH_INITIALIZE_V1) = R(NBA97_MATCH_INITIALIZE_S0 + 2);
        else {
          TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 18), 1, 1, 0x80099f34,
                  &R(NBA97_MATCH_INITIALIZE_V0)));
          R(NBA97_MATCH_INITIALIZE_V0) = lbu(R(NBA97_MATCH_INITIALIZE_V0));
          branch = R(NBA97_MATCH_INITIALIZE_V0);
          known(&R(NBA97_MATCH_INITIALIZE_V1), 258);
          TRY(zdec(r, branch, 0x80099f3c, &z));
          if (!z)
            known(&R(NBA97_MATCH_INITIALIZE_V1), 312);
        }
      }
    }
    R(NBA97_MATCH_INITIALIZE_S0 + 2) = R(NBA97_MATCH_INITIALIZE_V1);
    R(NBA97_MATCH_INITIALIZE_V0) = andi(R(NBA97_MATCH_INITIALIZE_A2), 0xfff);
    R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V0), 12);
    R(NBA97_MATCH_INITIALIZE_A0) = andi(R(NBA97_MATCH_INITIALIZE_A1), 0xfff);
    known(&R(NBA97_MATCH_INITIALIZE_V1), 0x06000000);
    known(&R(NBA97_MATCH_INITIALIZE_A1), 0x800c0000);
    TRY(rd(r, 0x800c55b8, 4, 4, 0x80099f68, &R(NBA97_MATCH_INITIALIZE_A1)));
    R(NBA97_MATCH_INITIALIZE_A0) =
        bor(R(NBA97_MATCH_INITIALIZE_A0), R(NBA97_MATCH_INITIALIZE_V1));
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_A1), 16), 4, 4, 0x80099f70,
            &R(NBA97_MATCH_INITIALIZE_V1)));
    cmd = bor(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_A0));
    o->horizontal_command = cmd;
    TRY(invoke(r, 0x80099f78, R(NBA97_MATCH_INITIALIZE_V1),
               NBA97_GAME_DISPLAY_ENVIRONMENT_GPU_COMMAND, 1, 1,
               NBA97_MATCH_INITIALIZE_A0, cmd));
    R(NBA97_MATCH_INITIALIZE_V0) =
        andi(R(NBA97_MATCH_INITIALIZE_S0 + 2), 0x3ff);
    R(NBA97_MATCH_INITIALIZE_V0) = shl(R(NBA97_MATCH_INITIALIZE_V0), 10);
    R(NBA97_MATCH_INITIALIZE_A0) =
        andi(R(NBA97_MATCH_INITIALIZE_S0 + 1), 0x3ff);
    known(&R(NBA97_MATCH_INITIALIZE_V1), 0x07000000);
    known(&R(NBA97_MATCH_INITIALIZE_A1), 0x800c0000);
    TRY(rd(r, 0x800c55b8, 4, 4, 0x80099f94, &R(NBA97_MATCH_INITIALIZE_A1)));
    R(NBA97_MATCH_INITIALIZE_A0) =
        bor(R(NBA97_MATCH_INITIALIZE_A0), R(NBA97_MATCH_INITIALIZE_V1));
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_A1), 16), 4, 4, 0x80099f9c,
            &R(NBA97_MATCH_INITIALIZE_V1)));
    cmd = bor(R(NBA97_MATCH_INITIALIZE_V0), R(NBA97_MATCH_INITIALIZE_A0));
    o->vertical_command = cmd;
    TRY(invoke(r, 0x80099fa4, R(NBA97_MATCH_INITIALIZE_V1),
               NBA97_GAME_DISPLAY_ENVIRONMENT_GPU_COMMAND, 1, 1,
               NBA97_MATCH_INITIALIZE_A0, cmd));
  }
  /* 0x80099FAC..0x8009A118: cached mode/rectangle comparison and GP1(08). */
  known(&R(NBA97_MATCH_INITIALIZE_V1), 0x800c0000);
  TRY(rd(r, 0x800c563c, 4, 4, 0x80099fb0, &R(NBA97_MATCH_INITIALIZE_V1)));
  TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 16), 4, 4, 0x80099fb4,
          &R(NBA97_MATCH_INITIALIZE_V0)));
  TRY(eqdec(r, R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0),
            0x80099fbc, &e));
  if (e) {
    TRY(cmp16(r, 0x800c562c, 0, 0x80099fc8, 0x80099fcc, 0x80099fd8, &e));
    if (e) {
      TRY(cmp16(r, 0x800c562e, 2, 0x80099fe4, 0x80099fe8, 0x80099ff4, &e));
      if (e) {
        TRY(cmp16(r, 0x800c5630, 4, 0x8009a000, 0x8009a004, 0x8009a010, &e));
        if (e)
          TRY(cmp16(r, 0x800c5632, 6, 0x8009a01c, 0x8009a020, 0x8009a02c, &e));
      }
    }
  }
  if (!e) {
    o->mode_changed = 1;
    TRY(fixedcall(r, 0x8009a034, 0x800985cc,
                  NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE, 0, 0, 0,
                  R(NBA97_MATCH_INITIALIZE_ZERO)));
    TRY(dwr(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 18), 1, 1, 0x8009a03c,
            R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 18), 1, 1, 0x8009a040, &v));
    R(NBA97_MATCH_INITIALIZE_V1) = lbu(v);
    known(&R(NBA97_MATCH_INITIALIZE_V0), 1);
    TRY(eqdec(r, R(NBA97_MATCH_INITIALIZE_V1), R(NBA97_MATCH_INITIALIZE_V0),
              0x8009a048, &e));
    if (e)
      R(NBA97_MATCH_INITIALIZE_S0 + 3) =
          orc(R(NBA97_MATCH_INITIALIZE_S0 + 3), 8);
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 17), 1, 1, 0x8009a054, &v));
    R(NBA97_MATCH_INITIALIZE_V0) = lbu(v);
    TRY(zdec(r, R(NBA97_MATCH_INITIALIZE_V0), 0x8009a05c, &z));
    if (!z)
      R(NBA97_MATCH_INITIALIZE_S0 + 3) =
          orc(R(NBA97_MATCH_INITIALIZE_S0 + 3), 16);
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 16), 1, 1, 0x8009a068, &v));
    R(NBA97_MATCH_INITIALIZE_V0) = lbu(v);
    TRY(zdec(r, R(NBA97_MATCH_INITIALIZE_V0), 0x8009a070, &z));
    if (!z)
      R(NBA97_MATCH_INITIALIZE_S0 + 3) =
          orc(R(NBA97_MATCH_INITIALIZE_S0 + 3), 32);
    known(&R(NBA97_MATCH_INITIALIZE_V0), 0x800c0000);
    R(NBA97_MATCH_INITIALIZE_V0) = addc(R(NBA97_MATCH_INITIALIZE_V0), 0x55c3);
    TRY(rd(r, 0x800c55c3, 1, 1, 0x8009a084, &R(NBA97_MATCH_INITIALIZE_V0)));
    R(NBA97_MATCH_INITIALIZE_V0) = lbu(R(NBA97_MATCH_INITIALIZE_V0));
    TRY(zdec(r, R(NBA97_MATCH_INITIALIZE_V0), 0x8009a08c, &z));
    if (!z)
      R(NBA97_MATCH_INITIALIZE_S0 + 3) =
          orc(R(NBA97_MATCH_INITIALIZE_S0 + 3), 128);
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 4), 2, 2, 0x8009a098, &v));
    R(NBA97_MATCH_INITIALIZE_V1) = lh(v);
    R(NBA97_MATCH_INITIALIZE_V0) =
        pred(s32(R(NBA97_MATCH_INITIALIZE_V1).word) < 281,
             R(NBA97_MATCH_INITIALIZE_V1));
    branch = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) =
        pred(s32(R(NBA97_MATCH_INITIALIZE_V1).word) < 353,
             R(NBA97_MATCH_INITIALIZE_V1));
    TRY(zdec(r, branch, 0x8009a0a4, &z));
    if (z) {
      branch = R(NBA97_MATCH_INITIALIZE_V0);
      R(NBA97_MATCH_INITIALIZE_V0) =
          pred(s32(R(NBA97_MATCH_INITIALIZE_V1).word) < 401,
               R(NBA97_MATCH_INITIALIZE_V1));
      TRY(zdec(r, branch, 0x8009a0ac, &z));
      if (!z)
        R(NBA97_MATCH_INITIALIZE_S0 + 3) =
            orc(R(NBA97_MATCH_INITIALIZE_S0 + 3), 1);
      else {
        branch = R(NBA97_MATCH_INITIALIZE_V0);
        R(NBA97_MATCH_INITIALIZE_V0) =
            pred(s32(R(NBA97_MATCH_INITIALIZE_V1).word) < 561,
                 R(NBA97_MATCH_INITIALIZE_V1));
        TRY(zdec(r, branch, 0x8009a0bc, &z));
        if (!z)
          R(NBA97_MATCH_INITIALIZE_S0 + 3) =
              orc(R(NBA97_MATCH_INITIALIZE_S0 + 3), 64);
        else {
          TRY(zdec(r, R(NBA97_MATCH_INITIALIZE_V0), 0x8009a0cc, &z));
          R(NBA97_MATCH_INITIALIZE_S0 + 3) =
              orc(R(NBA97_MATCH_INITIALIZE_S0 + 3), z ? 3 : 2);
        }
      }
    }
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 18), 1, 1, 0x8009a0e0, &v));
    R(NBA97_MATCH_INITIALIZE_V0) = lbu(v);
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_S0), 6), 2, 2, 0x8009a0e4, &v));
    R(NBA97_MATCH_INITIALIZE_V1) = lh(v);
    branch = R(NBA97_MATCH_INITIALIZE_V0);
    R(NBA97_MATCH_INITIALIZE_V0) =
        pred(s32(R(NBA97_MATCH_INITIALIZE_V1).word) < 289,
             R(NBA97_MATCH_INITIALIZE_V1));
    TRY(zdec(r, branch, 0x8009a0e8, &z));
    if (z)
      R(NBA97_MATCH_INITIALIZE_V0) =
          pred(s32(R(NBA97_MATCH_INITIALIZE_V1).word) < 257,
               R(NBA97_MATCH_INITIALIZE_V1));
    TRY(zdec(r, R(NBA97_MATCH_INITIALIZE_V0), 0x8009a0f4, &z));
    if (z)
      R(NBA97_MATCH_INITIALIZE_S0 + 3) =
          orc(R(NBA97_MATCH_INITIALIZE_S0 + 3), 36);
    cmd = R(NBA97_MATCH_INITIALIZE_S0 + 3);
    o->mode_command = cmd;
    known(&R(NBA97_MATCH_INITIALIZE_V0), 0x800c0000);
    TRY(rd(r, 0x800c55b8, 4, 4, 0x8009a104, &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(drd(r, addc(R(NBA97_MATCH_INITIALIZE_V0), 16), 4, 4, 0x8009a10c,
            &R(NBA97_MATCH_INITIALIZE_V0)));
    TRY(invoke(r, 0x8009a114, R(NBA97_MATCH_INITIALIZE_V0),
               NBA97_GAME_DISPLAY_ENVIRONMENT_GPU_COMMAND, 1, 1,
               NBA97_MATCH_INITIALIZE_A0, R(NBA97_MATCH_INITIALIZE_S0 + 3)));
  }
  /* Always delegate the exact 20-byte cache copy, then return live s0. */
  known(&R(NBA97_MATCH_INITIALIZE_A0), 0x800c562c);
  R(NBA97_MATCH_INITIALIZE_A1) = R(NBA97_MATCH_INITIALIZE_S0);
  known(&R(NBA97_MATCH_INITIALIZE_A2), 20);
  TRY(fixedcall(r, 0x8009a128, 0x8009cb0c, NBA97_GAME_DISPLAY_ENVIRONMENT_COPY,
                3, 1, NBA97_MATCH_INITIALIZE_A2, R(NBA97_MATCH_INITIALIZE_A2)));
  R(NBA97_MATCH_INITIALIZE_V0) = R(NBA97_MATCH_INITIALIZE_S0);
  o->return_v0 = R(NBA97_MATCH_INITIALIZE_V0);
  TRY(restore(r, 0x8009a134, 32, NBA97_MATCH_INITIALIZE_RA,
              &o->restored_return_address));
  TRY(restore(r, 0x8009a138, 28, NBA97_MATCH_INITIALIZE_S0 + 3,
              &o->restored_s3));
  TRY(restore(r, 0x8009a13c, 24, NBA97_MATCH_INITIALIZE_S0 + 2,
              &o->restored_s2));
  TRY(restore(r, 0x8009a140, 20, NBA97_MATCH_INITIALIZE_S0 + 1,
              &o->restored_s1));
  TRY(restore(r, 0x8009a144, 16, NBA97_MATCH_INITIALIZE_S0, &o->restored_s0));
  R(NBA97_MATCH_INITIALIZE_SP) = addc(R(NBA97_MATCH_INITIALIZE_SP), 40);
  if (R(NBA97_MATCH_INITIALIZE_RA).known_mask != 15) {
    stop(r, 0x8009a14c, 0, 0);
    return NBA97_TEXT_UNKNOWN;
  }
  o->completed = 1;
  stop(r, 0, 0, 0);
  return NBA97_TEXT_COMPLETE;
}
