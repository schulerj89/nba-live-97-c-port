#include "game_team_tactics_update.h"

#include <string.h>

typedef Nba97GameTeamTacticsWord Word;
typedef struct Run {
  Nba97GameTeamTacticsContext *c;
  Nba97GameTeamTacticsProgress *p;
  Nba97GameTeamTacticsMachine m;
} Run;

#define R(n) (r->m.registers.gpr[(n)])
#define OK(x)                                                                  \
  do {                                                                         \
    int q_ = (x);                                                              \
    if (q_ != NBA97_TEXT_COMPLETE)                                             \
      return q_;                                                               \
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
static int valid_machine(const Nba97GameTeamTacticsMachine *m) {
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
    Nba97GameTeamTacticsAccess *e = &r->c->access_journal[n];
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
  journal(r, NBA97_GAME_TEAM_TACTICS_READ, pc, a, w, x);
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
  journal(r, NBA97_GAME_TEAM_TACTICS_STORE, pc, a, w, v);
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
static void bounds(Word v, uint32_t bias, uint32_t *lo, uint32_t *hi) {
  unsigned b;
  *lo = 0;
  *hi = 0;
  for (b = 0; b < 4; b++) {
    uint32_t x = (v.word >> (8 * b)) & 255u;
    if (b == 3)
      x ^= bias >> 24;
    if (v.known_mask & (1u << b))
      *lo |= x << (8 * b), *hi |= x << (8 * b);
    else
      *hi |= 255u << (8 * b);
  }
}
static Word compare_words(Word a, Word b, int sign) {
  Word z;
  uint32_t al, ah, bl, bh, bias = sign ? UINT32_C(0x80000000) : 0;
  z.word = ((a.word ^ bias) < (b.word ^ bias));
  z.known_mask = 14;
  bounds(a, bias, &al, &ah);
  bounds(b, bias, &bl, &bh);
  if (ah < bl || al >= bh)
    z.known_mask = 15;
  return z;
}
static int eq(Word a, Word b, int *v) {
  unsigned i;
  *v = a.word == b.word;
  for (i = 0; i < 4; i++)
    if ((a.known_mask & b.known_mask & (1u << i)) &&
        (((a.word >> (8 * i)) & 255u) != ((b.word >> (8 * i)) & 255u))) {
      *v = 0;
      return 1;
    }
  if (a.known_mask == 15 && b.known_mask == 15) {
    *v = a.word == b.word;
    return 1;
  }
  return 0;
}
static int bool_value(Word v, int *truth) {
  if (!(v.known_mask & 1u))
    return 0;
  *truth = (v.word & 1u) != 0;
  return 1;
}
static Word bitand_word(Word a, Word b) {
  Word z = {a.word & b.word, 0};
  unsigned i;
  for (i = 0; i < 4; i++) {
    uint32_t av = (a.word >> (8 * i)) & 255u, bv = (b.word >> (8 * i)) & 255u;
    uint8_t k = (uint8_t)(1u << i);
    if (((a.known_mask & k) && av == 0) || ((b.known_mask & k) && bv == 0) ||
        (a.known_mask & b.known_mask & k))
      z.known_mask = (uint8_t)(z.known_mask | k);
  }
  return z;
}
static Word bitxor_word(Word a, Word b) {
  Word z = {a.word ^ b.word, (uint8_t)(a.known_mask & b.known_mask)};
  return z;
}
static void multiply_unsigned(Run *r, Word a, Word b) {
  uint64_t p = (uint64_t)a.word * b.word;
  unsigned i;
  r->m.lo.word = (uint32_t)p;
  r->m.hi.word = (uint32_t)(p >> 32);
  r->m.lo.known_mask = 0;
  r->m.hi.known_mask = 0;
  if ((a.known_mask == 15 && b.known_mask == 15) ||
      (a.known_mask == 15 && a.word == 0) ||
      (b.known_mask == 15 && b.word == 0)) {
    r->m.lo.known_mask = 15;
    r->m.hi.known_mask = 15;
    return;
  }
  if (a.known_mask == 15 || b.known_mask == 15) {
    Word v = a.known_mask == 15 ? b : a;
    for (i = 0; i < 4; i++) {
      uint8_t need = (uint8_t)((1u << (i + 1)) - 1u);
      if ((v.known_mask & need) == need)
        r->m.lo.known_mask = (uint8_t)(r->m.lo.known_mask | (1u << i));
    }
  }
}
static uint8_t call_kind(uint32_t e) {
  switch (e) {
  case 0x8002ab70:
    return 1;
  case 0x800706e4:
    return 2;
  case 0x8007066c:
    return 3;
  case 0x800295c0:
    return 4;
  case 0x80073134:
    return 5;
  case 0x80072c40:
    return 6;
  case 0x8007308c:
    return 7;
  case 0x80072ab0:
    return 8;
  case 0x80072b70:
    return 9;
  case 0x80073054:
    return 10;
  case 0x800742c0:
    return 11;
  case 0x8007458c:
    return 12;
  case 0x80074374:
    return 13;
  case 0x800743c8:
    return 14;
  case 0x80074488:
    return 15;
  case 0x80074688:
    return 16;
  case 0x80074714:
    return 17;
  default:
    return 0;
  }
}
static uint8_t call_arguments(uint32_t e) {
  switch (e) {
  case 0x8002ab70:
  case 0x800295c0:
    return 0;
  case 0x80072c40:
  case 0x8007308c:
  case 0x80072ab0:
  case 0x80072b70:
  case 0x80073054:
  case 0x800742c0:
    return 1;
  case 0x8007066c:
  case 0x80074374:
  case 0x800743c8:
  case 0x80074488:
    return 2;
  case 0x800706e4:
  case 0x8007458c:
  case 0x80074688:
  case 0x80074714:
    return 3;
  case 0x80073134:
    return 4;
  default:
    return 0;
  }
}
static int invoke(Run *r, uint32_t pc, uint32_t entry) {
  Nba97GameTeamTacticsEvent e;
  uint8_t k = call_kind(entry);
  int q;
  stop(r, pc, 0, entry);
  OK(spend(r));
  memset(&e, 0, sizeof e);
  e.pc = pc;
  e.delay_slot_pc = pc + 4;
  e.entry = entry;
  e.operation = r->p->operations;
  e.invocation = r->p->call_count[k] + 1;
  e.kind = k;
  e.argument_count = call_arguments(entry);
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
int nba97_game_team_tactics_update(Nba97GameTeamTacticsContext *context,
                                   Nba97GameTeamTacticsProgress *progress) {
  Run state, *r = &state;
  size_t index, earlier;
  int branch = 0, decided = 0;
  Word predicate;
  if (!progress)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof *progress);
  if (!context ||
      (context->access_journal_capacity && !context->access_journal) ||
      !valid_machine(&context->machine) ||
      (context->memory.count && !context->memory.region))
    return NBA97_TEXT_ARGUMENT;
  for (index = 0; index < context->memory.count; index++) {
    const Nba97GameTextRegion *z = &context->memory.region[index];
    if (!z->data || !z->size || (uint64_t)z->size > UINT64_C(0x100000000) ||
        (uint64_t)z->base + z->size > UINT64_C(0x100000000))
      return NBA97_TEXT_ARGUMENT;
    for (earlier = 0; earlier < index; earlier++) {
      const Nba97GameTextRegion *q = &context->memory.region[earlier];
      if ((uint64_t)z->base < (uint64_t)q->base + q->size &&
          (uint64_t)q->base < (uint64_t)z->base + z->size)
        return NBA97_TEXT_ARGUMENT;
    }
  }
  memset(&state, 0, sizeof state);
  r->c = context;
  r->p = progress;
  r->m = context->machine;
  publish(r);
  /* 0x800747B0..0x80074894: live frame and global gates. */
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdb68), 2u, 1,
          UINT32_C(0x800747b4)));
  R(29) = add(R(29), imm(UINT32_C(0xffffff68)));
  progress->frame_stack_pointer = R(29).word;
  OK(store(r, 31u, 29u, (int32_t)UINT32_C(0x00000094), 4u,
           UINT32_C(0x800747bc)));
  OK(store(r, 30u, 29u, (int32_t)UINT32_C(0x00000090), 4u,
           UINT32_C(0x800747c0)));
  OK(store(r, 23u, 29u, (int32_t)UINT32_C(0x0000008c), 4u,
           UINT32_C(0x800747c4)));
  OK(store(r, 22u, 29u, (int32_t)UINT32_C(0x00000088), 4u,
           UINT32_C(0x800747c8)));
  OK(store(r, 21u, 29u, (int32_t)UINT32_C(0x00000084), 4u,
           UINT32_C(0x800747cc)));
  OK(store(r, 20u, 29u, (int32_t)UINT32_C(0x00000080), 4u,
           UINT32_C(0x800747d0)));
  OK(store(r, 19u, 29u, (int32_t)UINT32_C(0x0000007c), 4u,
           UINT32_C(0x800747d4)));
  OK(store(r, 18u, 29u, (int32_t)UINT32_C(0x00000078), 4u,
           UINT32_C(0x800747d8)));
  OK(store(r, 17u, 29u, (int32_t)UINT32_C(0x00000074), 4u,
           UINT32_C(0x800747dc)));
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000003)), 1);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x800747E8: sw s0,0x70(sp). */
  OK(store(r, 16u, 29u, (int32_t)UINT32_C(0x00000070), 4u,
           UINT32_C(0x800747e8)));
  if (!decided) {
    stop(r, UINT32_C(0x800747e4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074814;
  goto L_800747ec;
L_800747ec:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdb58), 4u, 0,
          UINT32_C(0x800747f0)));
  (void)0;
  R(2) = compare_words(R(2), imm(UINT32_C(0x00001c21)), 1);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074800: ori v0,zero,0x1. */
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x800747fc), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074814;
  goto L_80074804;
L_80074804:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffdbba), 2u, UINT32_C(0x80074808)));
  /* Delay 0x80074810: nop . */
  (void)0;
  goto L_8007481c;
L_80074814:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 0u, 1u, (int32_t)UINT32_C(0xffffdbba), 2u, UINT32_C(0x80074818)));
L_8007481c:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdbcc), 2u, 1,
          UINT32_C(0x80074820)));
  R(3) = imm(UINT32_C(0x80100000));
  R(3) = add(R(3), imm(UINT32_C(0xffffe8aa)));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 0u, 1u, (int32_t)UINT32_C(0xffffe8ac), 2u, UINT32_C(0x80074830)));
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  branch = !branch;
  /* Delay 0x80074838: sh zero,0x0(v1). */
  OK(store(r, 0u, 3u, (int32_t)UINT32_C(0x00000000), 2u, UINT32_C(0x80074838)));
  if (!decided) {
    stop(r, UINT32_C(0x80074834), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074854;
  goto L_8007483c;
L_8007483c:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdbd2), 2u, 1,
          UINT32_C(0x80074840)));
  (void)0;
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  branch = !branch;
  /* Delay 0x8007484C: ori v0,zero,0x1. */
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x80074848), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074854;
  goto L_80074850;
L_80074850:
  OK(store(r, 2u, 3u, (int32_t)UINT32_C(0x00000000), 2u, UINT32_C(0x80074850)));
L_80074854:
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffdb90), 2u, 1,
          UINT32_C(0x80074858)));
  R(18) = imm(UINT32_C(0x80020000));
  R(18) = add(R(18), imm(UINT32_C(0xffffedf4)));
  R(2) = bitorc(R(0), UINT32_C(0x00000082));
  decided = eq(R(3), R(2), &branch);
  /* Delay 0x8007486C: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074868), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8007488c;
  goto L_80074870;
L_80074870:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdb94), 2u, 1,
          UINT32_C(0x80074874)));
  (void)0;
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80074880: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x8007487c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075d0c;
  goto L_80074884;
L_80074884:
  /* Delay 0x80074888: nop . */
  (void)0;
  goto L_80074894;
L_8007488c:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffe880), 2u, 1,
          UINT32_C(0x80074890)));
L_80074894:
  /* Source block beginning at 0x80074894. */
  (void)0;
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x8007489C: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074898), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800748a4;
  goto L_800748a0;
L_800748a0:
  R(18) = add(R(18), imm(UINT32_C(0x000000c4)));
L_800748a4:
  OK(load(r, 21u, 18u, (int32_t)UINT32_C(0x00000004), 4u, 0,
          UINT32_C(0x800748a4)));
  R(5) = imm(UINT32_C(0x80100000));
  R(5) = add(R(5), imm(UINT32_C(0xffffe890)));
  OK(load(r, 2u, 5u, (int32_t)UINT32_C(0x00000000), 2u, 0,
          UINT32_C(0x800748b0)));
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffdb6c), 2u, 0,
          UINT32_C(0x800748b8)));
  R(4) = imm(UINT32_C(0x80100000));
  OK(load(r, 4u, 4u, (int32_t)UINT32_C(0xffffe872), 2u, 1,
          UINT32_C(0x800748c0)));
  R(2) = sub(R(2), R(3));
  decided = eq(R(4), R(0), &branch);
  branch = !branch;
  /* Delay 0x800748CC: sh v0,0x0(a1). */
  OK(store(r, 2u, 5u, (int32_t)UINT32_C(0x00000000), 2u, UINT32_C(0x800748cc)));
  if (!decided) {
    stop(r, UINT32_C(0x800748c8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074908;
  goto L_800748d0;
L_800748d0:
  OK(load(r, 3u, 18u, (int32_t)UINT32_C(0x00000074), 2u, 1,
          UINT32_C(0x800748d0)));
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdba4), 4u, 0,
          UINT32_C(0x800748d8)));
  (void)0;
  R(2) = compare_words(R(2), R(3), 1);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x800748E8: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x800748e4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074908;
  goto L_800748ec;
L_800748ec:
  known(&R(31), UINT32_C(0x800748f4));
  /* Call delay 0x800748F0: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x800748ec), UINT32_C(0x8002ab70)));
  goto L_800748f4;
L_800748f4:
  R(2) = bitandc(R(2), UINT32_C(0x0000001f));
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x800748FC: ori v0,zero,0x1. */
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x800748f8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074908;
  goto L_80074900;
L_80074900:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe872), 2u, UINT32_C(0x80074904)));
L_80074908:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdbcc), 2u, 1,
          UINT32_C(0x8007490c)));
  OK(load(r, 22u, 18u, (int32_t)UINT32_C(0x000000a4), 2u, 1,
          UINT32_C(0x80074910)));
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80074918: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074914), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074940;
  goto L_8007491c;
L_8007491c:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdc34), 4u, 0,
          UINT32_C(0x80074920)));
  (void)0;
  OK(load(r, 9u, 2u, (int32_t)UINT32_C(0x00000008), 4u, 0,
          UINT32_C(0x80074928)));
  (void)0;
  OK(store(r, 9u, 29u, (int32_t)UINT32_C(0x00000010), 4u,
           UINT32_C(0x80074930)));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0x0000000c), 4u, 0,
          UINT32_C(0x80074934)));
  /* Delay 0x8007493C: sw v0,0x18(sp). */
  OK(store(r, 2u, 29u, (int32_t)UINT32_C(0x00000018), 4u,
           UINT32_C(0x8007493c)));
  goto L_80074960;
L_80074940:
  R(9) = imm(UINT32_C(0x80100000));
  OK(load(r, 9u, 9u, (int32_t)UINT32_C(0xffffdbc0), 4u, 0,
          UINT32_C(0x80074944)));
  (void)0;
  OK(store(r, 9u, 29u, (int32_t)UINT32_C(0x00000010), 4u,
           UINT32_C(0x8007494c)));
  R(9) = imm(UINT32_C(0x80100000));
  OK(load(r, 9u, 9u, (int32_t)UINT32_C(0xffffdbc4), 4u, 0,
          UINT32_C(0x80074954)));
  (void)0;
  OK(store(r, 9u, 29u, (int32_t)UINT32_C(0x00000018), 4u,
           UINT32_C(0x8007495c)));
L_80074960:
  /* Source block beginning at 0x80074960. */
  OK(load(r, 9u, 18u, (int32_t)UINT32_C(0x00000010), 4u, 0,
          UINT32_C(0x80074960)));
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0x00000014), 2u, 0,
          UINT32_C(0x80074964)));
  OK(store(r, 9u, 29u, (int32_t)UINT32_C(0x00000020), 4u,
           UINT32_C(0x80074968)));
  R(9) = bitorc(R(0), UINT32_C(0x00000320));
  OK(store(r, 9u, 29u, (int32_t)UINT32_C(0x00000030), 4u,
           UINT32_C(0x80074970)));
  R(2) = shift(R(2), 2u, 0, 0);
  OK(store(r, 9u, 29u, (int32_t)UINT32_C(0x00000028), 4u,
           UINT32_C(0x80074978)));
  R(1) = imm(UINT32_C(0x80020000));
  R(1) = add(R(1), R(2));
  OK(load(r, 17u, 1u, (int32_t)UINT32_C(0x00000bec), 4u, 0,
          UINT32_C(0x80074984)));
  R(20) = bitorc(R(0), UINT32_C(0x00000005));
  R(19) = imm(UINT32_C(0x80100000));
  R(19) = add(R(19), imm(UINT32_C(0xffffdbcc)));
  R(23) = bitorc(R(0), UINT32_C(0x00000001));
  R(16) = R(17);
L_8007499c:
  ++progress->actor_iterations;
  OK(load(r, 2u, 16u, (int32_t)UINT32_C(0x0000001a), 1u, 0,
          UINT32_C(0x8007499c)));
  (void)0;
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000007)), 0);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x800749AC: ori v0,zero,0x1. */
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x800749a8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800749b4;
  goto L_800749b0;
L_800749b0:
  OK(store(r, 2u, 16u, (int32_t)UINT32_C(0x0000001a), 1u,
           UINT32_C(0x800749b0)));
L_800749b4:
  OK(load(r, 4u, 16u, (int32_t)UINT32_C(0x00000008), 4u, 0,
          UINT32_C(0x800749b4)));
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000010), 4u, 0,
          UINT32_C(0x800749b8)));
  OK(load(r, 5u, 16u, (int32_t)UINT32_C(0x0000000c), 4u, 0,
          UINT32_C(0x800749bc)));
  R(4) = sub(R(9), R(4));
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000018), 4u, 0,
          UINT32_C(0x800749c4)));
  R(6) = add(R(17), imm(UINT32_C(0x000000c0)));
  known(&R(31), UINT32_C(0x800749d4));
  /* Call delay 0x800749D0: subu a1,t1,a1. */
  R(5) = sub(R(9), R(5));
  OK(invoke(r, UINT32_C(0x800749cc), UINT32_C(0x800706e4)));
  goto L_800749d4;
L_800749d4:
  OK(load(r, 4u, 16u, (int32_t)UINT32_C(0x00000008), 4u, 0,
          UINT32_C(0x800749d4)));
  R(2) = shift(R(2), 8u, 1, 1);
  OK(store(r, 2u, 16u, (int32_t)UINT32_C(0x000000be), 2u,
           UINT32_C(0x800749dc)));
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000020), 4u, 0,
          UINT32_C(0x800749e0)));
  OK(load(r, 5u, 16u, (int32_t)UINT32_C(0x0000000c), 4u, 0,
          UINT32_C(0x800749e4)));
  R(6) = add(R(17), imm(UINT32_C(0x000000bc)));
  R(4) = sub(R(9), R(4));
  known(&R(31), UINT32_C(0x800749f8));
  /* Call delay 0x800749F4: subu a1,zero,a1. */
  R(5) = sub(R(0), R(5));
  OK(invoke(r, UINT32_C(0x800749f0), UINT32_C(0x800706e4)));
  goto L_800749f8;
L_800749f8:
  R(2) = shift(R(2), 8u, 1, 1);
  OK(store(r, 2u, 16u, (int32_t)UINT32_C(0x000000ba), 2u,
           UINT32_C(0x800749fc)));
  OK(load(r, 3u, 19u, (int32_t)UINT32_C(0x00000000), 2u, 1,
          UINT32_C(0x80074a00)));
  OK(load(r, 2u, 17u, (int32_t)UINT32_C(0x00000000), 4u, 0,
          UINT32_C(0x80074a04)));
  (void)0;
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  /* Delay 0x80074A10: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074a0c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074a5c;
  goto L_80074a14;
L_80074a14:
  OK(store(r, 0u, 19u, (int32_t)UINT32_C(0x00000cda), 2u,
           UINT32_C(0x80074a14)));
  OK(load(r, 2u, 16u, (int32_t)UINT32_C(0x000000ba), 2u, 0,
          UINT32_C(0x80074a18)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074A24: sltiu v0,v0,0x78. */
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000078)), 0);
  if (!decided) {
    stop(r, UINT32_C(0x80074a20), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074a8c;
  goto L_80074a28;
L_80074a28:
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074A2C: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074a28), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074a8c;
  goto L_80074a30;
L_80074a30:
  OK(load(r, 2u, 16u, (int32_t)UINT32_C(0x000000a2), 2u, 1,
          UINT32_C(0x80074a30)));
  OK(load(r, 3u, 16u, (int32_t)UINT32_C(0x000000bc), 2u, 1,
          UINT32_C(0x80074a34)));
  (void)0;
  R(2) = sub(R(2), R(3));
  R(2) = add(R(2), imm(UINT32_C(0x00000080)));
  R(2) = bitandc(R(2), UINT32_C(0x000003ff));
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000101)), 1);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074A50: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074a4c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074a8c;
  goto L_80074a54;
L_80074a54:
  /* Delay 0x80074A58: sh s7,0xcda(s3). */
  OK(store(r, 23u, 19u, (int32_t)UINT32_C(0x00000cda), 2u,
           UINT32_C(0x80074a58)));
  goto L_80074a8c;
L_80074a5c:
  OK(load(r, 3u, 16u, (int32_t)UINT32_C(0x000000be), 2u, 0,
          UINT32_C(0x80074a5c)));
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000030), 4u, 0,
          UINT32_C(0x80074a60)));
  (void)0;
  R(2) = compare_words(R(9), R(3), 1);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074A70: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074a6c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074a8c;
  goto L_80074a74;
L_80074a74:
  OK(load(r, 2u, 16u, (int32_t)UINT32_C(0x0000001a), 1u, 0,
          UINT32_C(0x80074a74)));
  (void)0;
  decided = eq(R(2), R(23), &branch);
  branch = !branch;
  /* Delay 0x80074A80: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074a7c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074a8c;
  goto L_80074a84;
L_80074a84:
  R(30) = R(16);
  OK(store(r, 3u, 29u, (int32_t)UINT32_C(0x00000030), 4u,
           UINT32_C(0x80074a88)));
L_80074a8c:
  OK(load(r, 4u, 16u, (int32_t)UINT32_C(0x000000ba), 2u, 0,
          UINT32_C(0x80074a8c)));
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000028), 4u, 0,
          UINT32_C(0x80074a90)));
  (void)0;
  R(2) = compare_words(R(9), R(4), 1);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074AA0: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074a9c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074ac4;
  goto L_80074aa4;
L_80074aa4:
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffdbcc), 2u, 1,
          UINT32_C(0x80074aa8)));
  OK(load(r, 2u, 17u, (int32_t)UINT32_C(0x00000000), 4u, 0,
          UINT32_C(0x80074aac)));
  (void)0;
  decided = eq(R(2), R(3), &branch);
  /* Delay 0x80074AB8: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074ab4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074ac4;
  goto L_80074abc;
L_80074abc:
  OK(store(r, 16u, 29u, (int32_t)UINT32_C(0x00000038), 4u,
           UINT32_C(0x80074abc)));
  OK(store(r, 4u, 29u, (int32_t)UINT32_C(0x00000028), 4u,
           UINT32_C(0x80074ac0)));
L_80074ac4:
  /* Source block beginning at 0x80074AC4. */
  R(20) = add(R(20), imm(UINT32_C(0xffffffff)));
  R(16) = add(R(16), imm(UINT32_C(0x000000f4)));
  predicate = compare_words(imm(0u), R(20), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80074AD0: addiu s1,s1,0xf4. */
  R(17) = add(R(17), imm(UINT32_C(0x000000f4)));
  if (!decided) {
    stop(r, UINT32_C(0x80074acc), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8007499c;
  goto L_80074ad4;
L_80074ad4:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdc48), 4u, 0,
          UINT32_C(0x80074ad8)));
  OK(load(r, 4u, 18u, (int32_t)UINT32_C(0x00000010), 4u, 0,
          UINT32_C(0x80074adc)));
  OK(load(r, 3u, 2u, (int32_t)UINT32_C(0x00000008), 4u, 0,
          UINT32_C(0x80074ae0)));
  OK(load(r, 5u, 2u, (int32_t)UINT32_C(0x0000000c), 4u, 0,
          UINT32_C(0x80074ae4)));
  known(&R(31), UINT32_C(0x80074af0));
  /* Call delay 0x80074AEC: subu a0,v1,a0. */
  R(4) = sub(R(3), R(4));
  OK(invoke(r, UINT32_C(0x80074ae8), UINT32_C(0x8007066c)));
  goto L_80074af0;
L_80074af0:
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffdc48), 4u, 0,
          UINT32_C(0x80074af4)));
  R(2) = shift(R(2), 8u, 1, 1);
  OK(store(r, 2u, 3u, (int32_t)UINT32_C(0x000000ba), 2u, UINT32_C(0x80074afc)));
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffdb90), 2u, 1,
          UINT32_C(0x80074b04)));
  R(2) = bitorc(R(0), UINT32_C(0x00000082));
  decided = eq(R(3), R(2), &branch);
  /* Delay 0x80074B10: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074b0c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074b28;
  goto L_80074b14;
L_80074b14:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffe8aa), 2u, 1,
          UINT32_C(0x80074b18)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074B24: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074b20), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074b3c;
  goto L_80074b28;
L_80074b28:
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000030), 4u, 0,
          UINT32_C(0x80074b28)));
  R(2) = bitorc(R(0), UINT32_C(0x00000320));
  decided = eq(R(9), R(2), &branch);
  /* Delay 0x80074B34: ori v0,zero,0x3. */
  R(2) = bitorc(R(0), UINT32_C(0x00000003));
  if (!decided) {
    stop(r, UINT32_C(0x80074b30), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074b3c;
  goto L_80074b38;
L_80074b38:
  OK(store(r, 2u, 30u, (int32_t)UINT32_C(0x0000001a), 1u,
           UINT32_C(0x80074b38)));
L_80074b3c:
  OK(load(r, 2u, 21u, (int32_t)UINT32_C(0x00000014), 2u, 0,
          UINT32_C(0x80074b3c)));
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffdbcc), 2u, 1,
          UINT32_C(0x80074b44)));
  R(9) = bitorc(R(0), UINT32_C(0x00000320));
  OK(store(r, 9u, 29u, (int32_t)UINT32_C(0x00000030), 4u,
           UINT32_C(0x80074b4c)));
  OK(store(r, 9u, 29u, (int32_t)UINT32_C(0x00000028), 4u,
           UINT32_C(0x80074b50)));
  R(2) = shift(R(2), 2u, 0, 0);
  R(1) = imm(UINT32_C(0x80020000));
  R(1) = add(R(1), R(2));
  OK(load(r, 19u, 1u, (int32_t)UINT32_C(0x00000bec), 4u, 0,
          UINT32_C(0x80074b60)));
  predicate = compare_words(R(3), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  branch = !branch;
  /* Delay 0x80074B68: ori v0,zero,0x1. */
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x80074b64), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074b7c;
  goto L_80074b6c;
L_80074b6c:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 0u, 1u, (int32_t)UINT32_C(0xffffe8a0), 2u, UINT32_C(0x80074b70)));
  /* Delay 0x80074B78: ori s4,zero,0x5. */
  R(20) = bitorc(R(0), UINT32_C(0x00000005));
  goto L_80074b88;
L_80074b7c:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe8a0), 2u, UINT32_C(0x80074b80)));
  R(20) = bitorc(R(0), UINT32_C(0x00000005));
L_80074b88:
  R(23) = imm(UINT32_C(0x80100000));
  R(23) = add(R(23), imm(UINT32_C(0xffffe8a0)));
  R(30) = add(R(23), imm(UINT32_C(0x0000000c)));
  R(16) = add(R(19), imm(UINT32_C(0x000000be)));
L_80074b98:
  ++progress->opposing_actor_iterations;
  OK(load(r, 4u, 16u, (int32_t)UINT32_C(0x00000016), 2u, 1,
          UINT32_C(0x80074b98)));
  (void)0;
  predicate = compare_words(R(4), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80074BA4: sll v0,a0,0x2. */
  R(2) = shift(R(4), 2u, 0, 0);
  if (!decided) {
    stop(r, UINT32_C(0x80074ba0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074c08;
  goto L_80074ba8;
L_80074ba8:
  R(1) = imm(UINT32_C(0x80020000));
  R(1) = add(R(1), R(2));
  OK(load(r, 17u, 1u, (int32_t)UINT32_C(0x00000bec), 4u, 0,
          UINT32_C(0x80074bb0)));
  (void)0;
  OK(load(r, 2u, 17u, (int32_t)UINT32_C(0x000000d4), 2u, 1,
          UINT32_C(0x80074bb8)));
  (void)0;
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  branch = !branch;
  /* Delay 0x80074BC4: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074bc0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074bd4;
  goto L_80074bc8;
L_80074bc8:
  OK(load(r, 2u, 19u, (int32_t)UINT32_C(0x00000000), 4u, 0,
          UINT32_C(0x80074bc8)));
  (void)0;
  OK(store(r, 2u, 17u, (int32_t)UINT32_C(0x000000d4), 2u,
           UINT32_C(0x80074bd0)));
L_80074bd4:
  OK(load(r, 3u, 17u, (int32_t)UINT32_C(0x00000008), 4u, 0,
          UINT32_C(0x80074bd4)));
  OK(load(r, 4u, 16u, (int32_t)UINT32_C(0xffffff4a), 4u, 0,
          UINT32_C(0x80074bd8)));
  OK(load(r, 2u, 17u, (int32_t)UINT32_C(0x0000000c), 4u, 0,
          UINT32_C(0x80074bdc)));
  OK(load(r, 5u, 16u, (int32_t)UINT32_C(0xffffff4e), 4u, 0,
          UINT32_C(0x80074be0)));
  R(6) = add(R(19), imm(UINT32_C(0x000000d2)));
  R(4) = sub(R(3), R(4));
  known(&R(31), UINT32_C(0x80074bf4));
  /* Call delay 0x80074BF0: subu a1,v0,a1. */
  R(5) = sub(R(2), R(5));
  OK(invoke(r, UINT32_C(0x80074bec), UINT32_C(0x800706e4)));
  goto L_80074bf4;
L_80074bf4:
  R(2) = shift(R(2), 8u, 1, 0);
  OK(store(r, 2u, 17u, (int32_t)UINT32_C(0x000000d0), 2u,
           UINT32_C(0x80074bf8)));
  OK(load(r, 3u, 16u, (int32_t)UINT32_C(0x00000014), 2u, 0,
          UINT32_C(0x80074bfc)));
  OK(store(r, 2u, 16u, (int32_t)UINT32_C(0x00000012), 2u,
           UINT32_C(0x80074c00)));
  OK(store(r, 3u, 17u, (int32_t)UINT32_C(0x000000d2), 2u,
           UINT32_C(0x80074c04)));
L_80074c08:
  OK(load(r, 4u, 16u, (int32_t)UINT32_C(0xffffff4a), 4u, 0,
          UINT32_C(0x80074c08)));
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000020), 4u, 0,
          UINT32_C(0x80074c0c)));
  OK(load(r, 5u, 16u, (int32_t)UINT32_C(0xffffff4e), 4u, 0,
          UINT32_C(0x80074c10)));
  R(6) = add(R(19), imm(UINT32_C(0x000000bc)));
  R(4) = sub(R(9), R(4));
  known(&R(31), UINT32_C(0x80074c24));
  /* Call delay 0x80074C20: subu a1,zero,a1. */
  R(5) = sub(R(0), R(5));
  OK(invoke(r, UINT32_C(0x80074c1c), UINT32_C(0x800706e4)));
  goto L_80074c24;
L_80074c24:
  OK(load(r, 4u, 16u, (int32_t)UINT32_C(0xffffff4a), 4u, 0,
          UINT32_C(0x80074c24)));
  R(2) = shift(R(2), 8u, 1, 1);
  OK(store(r, 2u, 16u, (int32_t)UINT32_C(0xfffffffc), 2u,
           UINT32_C(0x80074c2c)));
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000010), 4u, 0,
          UINT32_C(0x80074c30)));
  OK(load(r, 5u, 16u, (int32_t)UINT32_C(0xffffff4e), 4u, 0,
          UINT32_C(0x80074c34)));
  R(4) = sub(R(9), R(4));
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000018), 4u, 0,
          UINT32_C(0x80074c3c)));
  R(6) = add(R(19), imm(UINT32_C(0x000000c0)));
  known(&R(31), UINT32_C(0x80074c4c));
  /* Call delay 0x80074C48: subu a1,t1,a1. */
  R(5) = sub(R(9), R(5));
  OK(invoke(r, UINT32_C(0x80074c44), UINT32_C(0x800706e4)));
  goto L_80074c4c;
L_80074c4c:
  OK(load(r, 3u, 16u, (int32_t)UINT32_C(0xffffff5c), 1u, 0,
          UINT32_C(0x80074c4c)));
  R(4) = shift(R(2), 8u, 1, 1);
  R(3) = compare_words(R(3), imm(UINT32_C(0x00000007)), 0);
  decided = eq(R(3), R(0), &branch);
  /* Delay 0x80074C5C: sh a0,0x0(s0). */
  OK(store(r, 4u, 16u, (int32_t)UINT32_C(0x00000000), 2u,
           UINT32_C(0x80074c5c)));
  if (!decided) {
    stop(r, UINT32_C(0x80074c58), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074c9c;
  goto L_80074c60;
L_80074c60:
  OK(load(r, 3u, 16u, (int32_t)UINT32_C(0xfffffffc), 2u, 0,
          UINT32_C(0x80074c60)));
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000030), 4u, 0,
          UINT32_C(0x80074c64)));
  (void)0;
  R(2) = compare_words(R(9), R(3), 1);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074C74: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074c70), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074c80;
  goto L_80074c78;
L_80074c78:
  OK(store(r, 19u, 29u, (int32_t)UINT32_C(0x00000048), 4u,
           UINT32_C(0x80074c78)));
  OK(store(r, 3u, 29u, (int32_t)UINT32_C(0x00000030), 4u,
           UINT32_C(0x80074c7c)));
L_80074c80:
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000028), 4u, 0,
          UINT32_C(0x80074c80)));
  R(3) = bitandc(R(4), UINT32_C(0x0000ffff));
  R(2) = compare_words(R(9), R(3), 1);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074C90: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074c8c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074c9c;
  goto L_80074c94;
L_80074c94:
  OK(store(r, 19u, 29u, (int32_t)UINT32_C(0x00000040), 4u,
           UINT32_C(0x80074c94)));
  OK(store(r, 3u, 29u, (int32_t)UINT32_C(0x00000028), 4u,
           UINT32_C(0x80074c98)));
L_80074c9c:
  OK(load(r, 2u, 23u, (int32_t)UINT32_C(0x00000000), 2u, 1,
          UINT32_C(0x80074c9c)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074CA8: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074ca4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074cd4;
  goto L_80074cac;
L_80074cac:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdc34), 4u, 0,
          UINT32_C(0x80074cb0)));
  (void)0;
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0x000000ba), 2u, 0,
          UINT32_C(0x80074cb8)));
  OK(load(r, 3u, 16u, (int32_t)UINT32_C(0xfffffffc), 2u, 0,
          UINT32_C(0x80074cbc)));
  R(2) = add(R(2), imm(UINT32_C(0x00000008)));
  R(3) = compare_words(R(3), R(2), 1);
  decided = eq(R(3), R(0), &branch);
  /* Delay 0x80074CCC: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074cc8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074cd4;
  goto L_80074cd0;
L_80074cd0:
  OK(store(r, 0u, 23u, (int32_t)UINT32_C(0x00000000), 2u,
           UINT32_C(0x80074cd0)));
L_80074cd4:
  OK(load(r, 2u, 16u, (int32_t)UINT32_C(0x00000000), 2u, 0,
          UINT32_C(0x80074cd4)));
  (void)0;
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000021)), 0);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074CE4: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074ce0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074cf8;
  goto L_80074ce8;
L_80074ce8:
  OK(load(r, 2u, 30u, (int32_t)UINT32_C(0x00000000), 2u, 0,
          UINT32_C(0x80074ce8)));
  (void)0;
  R(2) = add(R(2), imm(UINT32_C(0x00000001)));
  OK(store(r, 2u, 30u, (int32_t)UINT32_C(0x00000000), 2u,
           UINT32_C(0x80074cf4)));
L_80074cf8:
  R(20) = add(R(20), imm(UINT32_C(0xffffffff)));
  R(16) = add(R(16), imm(UINT32_C(0x000000f4)));
  predicate = compare_words(imm(0u), R(20), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80074D04: addiu s3,s3,0xf4. */
  R(19) = add(R(19), imm(UINT32_C(0x000000f4)));
  if (!decided) {
    stop(r, UINT32_C(0x80074d00), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074b98;
  goto L_80074d08;
L_80074d08:
  /* Source block beginning at 0x80074D08. */
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffe8ac), 2u, 1,
          UINT32_C(0x80074d0c)));
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000028), 4u, 0,
          UINT32_C(0x80074d10)));
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 9u, 1u, (int32_t)UINT32_C(0xffffdbe0), 4u, UINT32_C(0x80074d1c)));
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  /* Delay 0x80074D24: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074d20), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074d30;
  goto L_80074d28;
L_80074d28:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 0u, 1u, (int32_t)UINT32_C(0xffffe8ac), 2u, UINT32_C(0x80074d2c)));
L_80074d30:
  known(&R(31), UINT32_C(0x80074d38));
  /* Call delay 0x80074D34: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x80074d30), UINT32_C(0x800295c0)));
  goto L_80074d38;
L_80074d38:
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffe86e), 2u, 0,
          UINT32_C(0x80074d3c)));
  (void)0;
  R(2) = bitandc(R(3), UINT32_C(0x00000002));
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074D4C: andi v0,v1,0xfffd. */
  R(2) = bitandc(R(3), UINT32_C(0x0000fffd));
  if (!decided) {
    stop(r, UINT32_C(0x80074d48), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074fb8;
  goto L_80074d50;
L_80074d50:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe86e), 2u, UINT32_C(0x80074d54)));
  OK(load(r, 2u, 21u, (int32_t)UINT32_C(0x000000ba), 1u, 0,
          UINT32_C(0x80074d58)));
  (void)0;
  R(2) = bitandc(R(2), UINT32_C(0x00000001));
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074D68: ori v0,zero,0x2. */
  R(2) = bitorc(R(0), UINT32_C(0x00000002));
  if (!decided) {
    stop(r, UINT32_C(0x80074d64), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074da8;
  goto L_80074d6c;
L_80074d6c:
  OK(load(r, 2u, 21u, (int32_t)UINT32_C(0x00000037), 1u, 0,
          UINT32_C(0x80074d6c)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074D78: addiu v0,v0,-0x1. */
  R(2) = add(R(2), imm(UINT32_C(0xffffffff)));
  if (!decided) {
    stop(r, UINT32_C(0x80074d74), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074da8;
  goto L_80074d7c;
L_80074d7c:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdbba), 2u, 1,
          UINT32_C(0x80074d80)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074D8C: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074d88), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074d98;
  goto L_80074d90;
L_80074d90:
  predicate = compare_words(R(22), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  branch = !branch;
  /* Delay 0x80074D94: ori v0,zero,0x2. */
  R(2) = bitorc(R(0), UINT32_C(0x00000002));
  if (!decided) {
    stop(r, UINT32_C(0x80074d90), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074da8;
  goto L_80074d98;
L_80074d98:
  R(2) = compare_words(R(22), imm(UINT32_C(0x00000010)), 1);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074DA0: ori v0,zero,0x1. */
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x80074d9c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074da8;
  goto L_80074da4;
L_80074da4:
  R(2) = bitorc(R(0), UINT32_C(0x00000002));
L_80074da8:
  OK(store(r, 2u, 21u, (int32_t)UINT32_C(0x00000040), 1u,
           UINT32_C(0x80074da8)));
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffe894), 2u, 0,
          UINT32_C(0x80074db0)));
  (void)0;
  R(2) = bitandc(R(2), UINT32_C(0x00000002));
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074DC0: ori v0,zero,0x5. */
  R(2) = bitorc(R(0), UINT32_C(0x00000005));
  if (!decided) {
    stop(r, UINT32_C(0x80074dbc), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074ebc;
  goto L_80074dc4;
L_80074dc4:
  OK(store(r, 0u, 21u, (int32_t)UINT32_C(0x000000ba), 1u,
           UINT32_C(0x80074dc4)));
  OK(store(r, 0u, 18u, (int32_t)UINT32_C(0x000000ba), 1u,
           UINT32_C(0x80074dc8)));
  OK(load(r, 3u, 21u, (int32_t)UINT32_C(0x00000039), 1u, 0,
          UINT32_C(0x80074dcc)));
  (void)0;
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  /* Delay 0x80074DD8: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074dd4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074e70;
  goto L_80074ddc;
L_80074ddc:
  OK(load(r, 2u, 21u, (int32_t)UINT32_C(0x00000010), 4u, 0,
          UINT32_C(0x80074ddc)));
  (void)0;
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  branch = !branch;
  /* Delay 0x80074DE8: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074de4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074e18;
  goto L_80074dec;
L_80074dec:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdc48), 4u, 0,
          UINT32_C(0x80074df0)));
  (void)0;
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0x00000008), 4u, 0,
          UINT32_C(0x80074df8)));
  (void)0;
  R(2) = shift(R(2), 8u, 1, 1);
  R(2) = compare_words(R(2), imm(UINT32_C(0xffffff89)), 1);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074E0C: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074e08), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074e70;
  goto L_80074e10;
L_80074e10:
  /* Delay 0x80074E14: nop . */
  (void)0;
  goto L_80074e3c;
L_80074e18:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdc48), 4u, 0,
          UINT32_C(0x80074e1c)));
  (void)0;
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0x00000008), 4u, 0,
          UINT32_C(0x80074e24)));
  (void)0;
  R(2) = shift(R(2), 8u, 1, 1);
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000078)), 1);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074E38: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074e34), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074e70;
  goto L_80074e3c;
L_80074e3c:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdbba), 2u, 1,
          UINT32_C(0x80074e40)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074E4C: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074e48), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074e70;
  goto L_80074e50;
L_80074e50:
  predicate = compare_words(R(22), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80074E54: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074e50), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074e70;
  goto L_80074e58;
L_80074e58:
  OK(load(r, 2u, 21u, (int32_t)UINT32_C(0x00000037), 1u, 0,
          UINT32_C(0x80074e58)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074E64: ori v0,zero,0x2. */
  R(2) = bitorc(R(0), UINT32_C(0x00000002));
  if (!decided) {
    stop(r, UINT32_C(0x80074e60), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074f5c;
  goto L_80074e68;
L_80074e68:
  /* Delay 0x80074E6C: sb v0,0x40(s5). */
  OK(store(r, 2u, 21u, (int32_t)UINT32_C(0x00000040), 1u,
           UINT32_C(0x80074e6c)));
  goto L_80074f5c;
L_80074e70:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffe894), 2u, 0,
          UINT32_C(0x80074e74)));
  (void)0;
  R(2) = bitandc(R(2), UINT32_C(0x00000002));
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074E84: ori v0,zero,0x5. */
  R(2) = bitorc(R(0), UINT32_C(0x00000005));
  if (!decided) {
    stop(r, UINT32_C(0x80074e80), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074ebc;
  goto L_80074e88;
L_80074e88:
  OK(load(r, 3u, 21u, (int32_t)UINT32_C(0x00000039), 1u, 0,
          UINT32_C(0x80074e88)));
  (void)0;
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  /* Delay 0x80074E94: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074e90), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074ecc;
  goto L_80074e98;
L_80074e98:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdbba), 2u, 1,
          UINT32_C(0x80074e9c)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074EA8: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074ea4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074ed8;
  goto L_80074eac;
L_80074eac:
  predicate = compare_words(imm(0u), R(22), 1);
  decided = bool_value(predicate, &branch);
  branch = !branch;
  /* Delay 0x80074EB0: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074eac), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074ed8;
  goto L_80074eb4;
L_80074eb4:
  /* Delay 0x80074EB8: nop . */
  (void)0;
  goto L_80074f5c;
L_80074ebc:
  OK(load(r, 3u, 21u, (int32_t)UINT32_C(0x00000039), 1u, 0,
          UINT32_C(0x80074ebc)));
  R(2) = bitorc(R(0), UINT32_C(0x00000005));
  decided = eq(R(3), R(2), &branch);
  /* Delay 0x80074EC8: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074ec4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074ed8;
  goto L_80074ecc;
L_80074ecc:
  OK(load(r, 2u, 21u, (int32_t)UINT32_C(0x00000039), 1u, 0,
          UINT32_C(0x80074ecc)));
  /* Delay 0x80074ED4: nop . */
  (void)0;
  goto L_80074f84;
L_80074ed8:
  predicate = compare_words(R(22), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80074EDC: slti v0,s6,0xa. */
  R(2) = compare_words(R(22), imm(UINT32_C(0x0000000a)), 1);
  if (!decided) {
    stop(r, UINT32_C(0x80074ed8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074f34;
  goto L_80074ee0;
L_80074ee0:
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074EE4: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074ee0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074f04;
  goto L_80074ee8;
L_80074ee8:
  known(&R(31), UINT32_C(0x80074ef0));
  /* Call delay 0x80074EEC: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x80074ee8), UINT32_C(0x8002ab70)));
  goto L_80074ef0;
L_80074ef0:
  R(2) = bitandc(R(2), UINT32_C(0x0000000c));
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074EF8: ori v0,zero,0x2. */
  R(2) = bitorc(R(0), UINT32_C(0x00000002));
  if (!decided) {
    stop(r, UINT32_C(0x80074ef4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074f84;
  goto L_80074efc;
L_80074efc:
  /* Delay 0x80074F00: ori v0,zero,0x3. */
  R(2) = bitorc(R(0), UINT32_C(0x00000003));
  goto L_80074f84;
L_80074f04:
  known(&R(31), UINT32_C(0x80074f0c));
  /* Call delay 0x80074F08: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x80074f04), UINT32_C(0x8002ab70)));
  goto L_80074f0c;
L_80074f0c:
  R(2) = bitandc(R(2), UINT32_C(0x0000000c));
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074F14: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074f10), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074f5c;
  goto L_80074f18;
L_80074f18:
  known(&R(31), UINT32_C(0x80074f20));
  /* Call delay 0x80074F1C: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x80074f18), UINT32_C(0x8002ab70)));
  goto L_80074f20;
L_80074f20:
  R(2) = bitandc(R(2), UINT32_C(0x00000008));
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074F28: ori v0,zero,0x1. */
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x80074f24), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074f84;
  goto L_80074f2c;
L_80074f2c:
  /* Delay 0x80074F30: ori v0,zero,0x2. */
  R(2) = bitorc(R(0), UINT32_C(0x00000002));
  goto L_80074f84;
L_80074f34:
  R(2) = compare_words(R(22), imm(UINT32_C(0xfffffffa)), 1);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074F3C: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074f38), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074f6c;
  goto L_80074f40;
L_80074f40:
  known(&R(31), UINT32_C(0x80074f48));
  /* Call delay 0x80074F44: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x80074f40), UINT32_C(0x8002ab70)));
  goto L_80074f48;
L_80074f48:
  R(2) = bitandc(R(2), UINT32_C(0x00000010));
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074F50: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80074f4c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074f6c;
  goto L_80074f54;
L_80074f54:
  /* Delay 0x80074F58: ori v0,zero,0x3. */
  R(2) = bitorc(R(0), UINT32_C(0x00000003));
  goto L_80074f84;
L_80074f5c:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 0u, 1u, (int32_t)UINT32_C(0xffffe8b2), 2u, UINT32_C(0x80074f60)));
  /* Delay 0x80074F68: nop . */
  (void)0;
  goto L_80074f8c;
L_80074f6c:
  R(2) = imm(UINT32_C(0x80020000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0x00001d72), 1u, 0,
          UINT32_C(0x80074f70)));
  (void)0;
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000002)), 0);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80074F80: ori v0,zero,0x4. */
  R(2) = bitorc(R(0), UINT32_C(0x00000004));
  if (!decided) {
    stop(r, UINT32_C(0x80074f7c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074f54;
  goto L_80074f84;
L_80074f84:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe8b2), 2u, UINT32_C(0x80074f88)));
L_80074f8c:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffe894), 2u, 0,
          UINT32_C(0x80074f90)));
  (void)0;
  R(2) = bitandc(R(2), UINT32_C(0x00000002));
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80074FA0: move a0,s5. */
  R(4) = R(21);
  if (!decided) {
    stop(r, UINT32_C(0x80074f9c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80074fb8;
  goto L_80074fa4;
L_80074fa4:
  R(5) = imm(UINT32_C(0x80100000));
  OK(load(r, 5u, 5u, (int32_t)UINT32_C(0xffffe8b2), 2u, 1,
          UINT32_C(0x80074fa8)));
  R(6) = imm(0u);
  known(&R(31), UINT32_C(0x80074fb8));
  /* Call delay 0x80074FB4: clear a3. */
  R(7) = imm(0u);
  OK(invoke(r, UINT32_C(0x80074fb0), UINT32_C(0x80073134)));
  goto L_80074fb8;
L_80074fb8:
  /* Source block beginning at 0x80074FB8. */
  R(2) = imm(UINT32_C(0x80100000));
  R(2) = add(R(2), imm(UINT32_C(0xffffe894)));
  OK(load(r, 3u, 2u, (int32_t)UINT32_C(0x00000000), 2u, 0,
          UINT32_C(0x80074fc0)));
  R(4) = imm(UINT32_C(0x80100000));
  OK(load(r, 4u, 4u, (int32_t)UINT32_C(0xffffe86e), 2u, 1,
          UINT32_C(0x80074fc8)));
  R(3) = bitandc(R(3), UINT32_C(0x0000fffd));
  decided = eq(R(4), R(0), &branch);
  /* Delay 0x80074FD4: sh v1,0x0(v0). */
  OK(store(r, 3u, 2u, (int32_t)UINT32_C(0x00000000), 2u, UINT32_C(0x80074fd4)));
  if (!decided) {
    stop(r, UINT32_C(0x80074fd0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8007542c;
  goto L_80074fd8;
L_80074fd8:
  known(&R(31), UINT32_C(0x80074fe0));
  /* Call delay 0x80074FDC: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x80074fd8), UINT32_C(0x8002ab70)));
  goto L_80074fe0;
L_80074fe0:
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffe86e), 2u, 0,
          UINT32_C(0x80074fe4)));
  R(2) = bitandc(R(2), UINT32_C(0x00000008));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe89e), 2u, UINT32_C(0x80074ff0)));
  R(3) = bitandc(R(3), UINT32_C(0x0000fffe));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 3u, 1u, (int32_t)UINT32_C(0xffffe86e), 2u, UINT32_C(0x80074ffc)));
  known(&R(31), UINT32_C(0x80075008));
  /* Call delay 0x80075004: move a0,s2. */
  R(4) = R(18);
  OK(invoke(r, UINT32_C(0x80075000), UINT32_C(0x80072c40)));
  goto L_80075008;
L_80075008:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdbba), 2u, 1,
          UINT32_C(0x8007500c)));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 0u, 1u, (int32_t)UINT32_C(0xffffe8a2), 2u, UINT32_C(0x80075014)));
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x8007501C: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075018), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075068;
  goto L_80075020;
L_80075020:
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0x000000a4), 2u, 1,
          UINT32_C(0x80075020)));
  (void)0;
  R(2) = compare_words(R(2), imm(UINT32_C(0xfffffffd)), 1);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075030: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x8007502c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075068;
  goto L_80075034;
L_80075034:
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffdb58), 4u, 0,
          UINT32_C(0x80075038)));
  (void)0;
  R(2) = compare_words(R(3), imm(UINT32_C(0x00000e11)), 1);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075048: slti v0,v1,0x709. */
  R(2) = compare_words(R(3), imm(UINT32_C(0x00000709)), 1);
  if (!decided) {
    stop(r, UINT32_C(0x80075044), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075054;
  goto L_8007504c;
L_8007504c:
  /* Delay 0x80075050: ori v0,zero,0x1e0. */
  R(2) = bitorc(R(0), UINT32_C(0x000001e0));
  goto L_80075060;
L_80075054:
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075058: ori v0,zero,0x4b0. */
  R(2) = bitorc(R(0), UINT32_C(0x000004b0));
  if (!decided) {
    stop(r, UINT32_C(0x80075054), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075060;
  goto L_8007505c;
L_8007505c:
  R(2) = bitorc(R(0), UINT32_C(0x000002d0));
L_80075060:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe8a2), 2u, UINT32_C(0x80075064)));
L_80075068:
  OK(load(r, 3u, 18u, (int32_t)UINT32_C(0x00000036), 1u, 0,
          UINT32_C(0x80075068)));
  R(2) = bitorc(R(0), UINT32_C(0x00000002));
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  /* Delay 0x80075074: sb zero,0x3f(s2). */
  OK(store(r, 0u, 18u, (int32_t)UINT32_C(0x0000003f), 1u,
           UINT32_C(0x80075074)));
  if (!decided) {
    stop(r, UINT32_C(0x80075070), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075080;
  goto L_80075078;
L_80075078:
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  OK(store(r, 2u, 18u, (int32_t)UINT32_C(0x0000003f), 1u,
           UINT32_C(0x8007507c)));
L_80075080:
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffdb90), 2u, 1,
          UINT32_C(0x80075084)));
  R(2) = bitorc(R(0), UINT32_C(0x00000082));
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  /* Delay 0x80075090: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x8007508c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800750b8;
  goto L_80075094;
L_80075094:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 0u, 1u, (int32_t)UINT32_C(0xffffe86c), 2u, UINT32_C(0x80075098)));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 0u, 1u, (int32_t)UINT32_C(0xffffe866), 2u, UINT32_C(0x800750a0)));
  OK(load(r, 4u, 18u, (int32_t)UINT32_C(0x00000014), 2u, 0,
          UINT32_C(0x800750a4)));
  known(&R(31), UINT32_C(0x800750b0));
  /* Call delay 0x800750AC: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x800750a8), UINT32_C(0x8007308c)));
  goto L_800750b0;
L_800750b0:
  /* Delay 0x800750B4: nop . */
  (void)0;
  goto L_800754e0;
L_800750b8:
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0x000000b8), 1u, 0,
          UINT32_C(0x800750b8)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x800750C4: ori v0,zero,0x1. */
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x800750c0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800750d8;
  goto L_800750c8;
L_800750c8:
  known(&R(31), UINT32_C(0x800750d0));
  /* Call delay 0x800750CC: move a0,s2. */
  R(4) = R(18);
  OK(invoke(r, UINT32_C(0x800750c8), UINT32_C(0x80072ab0)));
  goto L_800750d0;
L_800750d0:
  /* Delay 0x800750D4: nop . */
  (void)0;
  goto L_8007542c;
L_800750d8:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 0u, 1u, (int32_t)UINT32_C(0xffffe86c), 2u, UINT32_C(0x800750dc)));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffdbb4), 2u, UINT32_C(0x800750e4)));
  OK(load(r, 17u, 18u, (int32_t)UINT32_C(0x00000038), 1u, 0,
          UINT32_C(0x800750e8)));
  R(2) = bitorc(R(0), UINT32_C(0x00000007));
  decided = eq(R(17), R(2), &branch);
  branch = !branch;
  /* Delay 0x800750F4: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x800750f0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075280;
  goto L_800750f8;
L_800750f8:
  predicate = compare_words(R(22), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  branch = !branch;
  /* Delay 0x800750FC: slti v0,s6,-0x5. */
  R(2) = compare_words(R(22), imm(UINT32_C(0xfffffffb)), 1);
  if (!decided) {
    stop(r, UINT32_C(0x800750f8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800751d0;
  goto L_80075100;
L_80075100:
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075104: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075100), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8007511c;
  goto L_80075108;
L_80075108:
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0x00000036), 1u, 0,
          UINT32_C(0x80075108)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075114: ori v0,zero,0x1. */
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x80075110), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8007511c;
  goto L_80075118;
L_80075118:
  OK(store(r, 2u, 18u, (int32_t)UINT32_C(0x0000003f), 1u,
           UINT32_C(0x80075118)));
L_8007511c:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdbba), 2u, 1,
          UINT32_C(0x80075120)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x8007512C: slti v0,s6,-0x7. */
  R(2) = compare_words(R(22), imm(UINT32_C(0xfffffff9)), 1);
  if (!decided) {
    stop(r, UINT32_C(0x80075128), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075140;
  goto L_80075130;
L_80075130:
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075134: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075130), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800751d0;
  goto L_80075138;
L_80075138:
  /* Delay 0x8007513C: nop . */
  (void)0;
  goto L_800751b8;
L_80075140:
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffdb58), 4u, 0,
          UINT32_C(0x80075144)));
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdba4), 4u, 0,
          UINT32_C(0x8007514c)));
  (void)0;
  R(2) = compare_words(R(3), R(2), 1);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x8007515C: slti v0,s6,-0x3. */
  R(2) = compare_words(R(22), imm(UINT32_C(0xfffffffd)), 1);
  if (!decided) {
    stop(r, UINT32_C(0x80075158), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075190;
  goto L_80075160;
L_80075160:
  R(2) = compare_words(R(3), imm(UINT32_C(0x00000e10)), 1);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075168: slti v0,s6,-0x2. */
  R(2) = compare_words(R(22), imm(UINT32_C(0xfffffffe)), 1);
  if (!decided) {
    stop(r, UINT32_C(0x80075164), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800751c8;
  goto L_8007516c;
L_8007516c:
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075170: slti v0,s6,-0x4. */
  R(2) = compare_words(R(22), imm(UINT32_C(0xfffffffc)), 1);
  if (!decided) {
    stop(r, UINT32_C(0x8007516c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800751d0;
  goto L_80075174;
L_80075174:
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075178: ori s1,zero,0x5. */
  R(17) = bitorc(R(0), UINT32_C(0x00000005));
  if (!decided) {
    stop(r, UINT32_C(0x80075174), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075280;
  goto L_8007517c;
L_8007517c:
  known(&R(31), UINT32_C(0x80075184));
  /* Call delay 0x80075180: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x8007517c), UINT32_C(0x8002ab70)));
  goto L_80075184;
L_80075184:
  R(2) = bitandc(R(2), UINT32_C(0x00000fff));
  /* Delay 0x8007518C: sltiu v0,v0,0x4cd. */
  R(2) = compare_words(R(2), imm(UINT32_C(0x000004cd)), 0);
  goto L_800751c8;
L_80075190:
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075194: ori s1,zero,0x5. */
  R(17) = bitorc(R(0), UINT32_C(0x00000005));
  if (!decided) {
    stop(r, UINT32_C(0x80075190), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075280;
  goto L_80075198;
L_80075198:
  R(2) = compare_words(R(22), imm(UINT32_C(0xfffffffe)), 1);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x800751A0: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x8007519c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800751d0;
  goto L_800751a4;
L_800751a4:
  known(&R(31), UINT32_C(0x800751ac));
  /* Call delay 0x800751A8: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x800751a4), UINT32_C(0x8002ab70)));
  goto L_800751ac;
L_800751ac:
  R(2) = bitandc(R(2), UINT32_C(0x00000fff));
  /* Delay 0x800751B4: sltiu v0,v0,0x801. */
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000801)), 0);
  goto L_800751c8;
L_800751b8:
  known(&R(31), UINT32_C(0x800751c0));
  /* Call delay 0x800751BC: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x800751b8), UINT32_C(0x8002ab70)));
  goto L_800751c0;
L_800751c0:
  R(2) = bitandc(R(2), UINT32_C(0x00000fff));
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000667)), 0);
L_800751c8:
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x800751CC: ori s1,zero,0x5. */
  R(17) = bitorc(R(0), UINT32_C(0x00000005));
  if (!decided) {
    stop(r, UINT32_C(0x800751c8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075280;
  goto L_800751d0;
L_800751d0:
  known(&R(31), UINT32_C(0x800751d8));
  /* Call delay 0x800751D4: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x800751d0), UINT32_C(0x8002ab70)));
  goto L_800751d8;
L_800751d8:
  R(2) = bitandc(R(2), UINT32_C(0x00000007));
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x800751E0: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x800751dc), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075208;
  goto L_800751e4;
L_800751e4:
  known(&R(31), UINT32_C(0x800751ec));
  /* Call delay 0x800751E8: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x800751e4), UINT32_C(0x8002ab70)));
  goto L_800751ec;
L_800751ec:
  R(2) = bitandc(R(2), UINT32_C(0x00000078));
  R(17) = shift(R(2), 3u, 1, 0);
  R(2) = compare_words(R(17), imm(UINT32_C(0x00000007)), 1);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x800751FC: slti v0,s6,-0x3. */
  R(2) = compare_words(R(22), imm(UINT32_C(0xfffffffd)), 1);
  if (!decided) {
    stop(r, UINT32_C(0x800751f8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800751e4;
  goto L_80075200;
L_80075200:
  /* Delay 0x80075204: nop . */
  (void)0;
  goto L_80075284;
L_80075208:
  known(&R(31), UINT32_C(0x80075210));
  /* Call delay 0x8007520C: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x80075208), UINT32_C(0x8002ab70)));
  goto L_80075210;
L_80075210:
  R(2) = bitandc(R(2), UINT32_C(0x0000ffff));
  R(3) = imm(UINT32_C(0xaaaa0000));
  R(3) = bitorc(R(3), UINT32_C(0x0000aaab));
  multiply_unsigned(r, R(2), R(3));
  OK(load(r, 5u, 18u, (int32_t)UINT32_C(0x0000006c), 4u, 0,
          UINT32_C(0x80075220)));
  R(9) = r->m.hi;
  R(4) = shift(R(9), 1u, 1, 0);
  R(3) = shift(R(4), 1u, 0, 0);
  R(3) = add(R(3), R(4));
  R(2) = sub(R(2), R(3));
  R(2) = bitandc(R(2), UINT32_C(0x0000ffff));
  R(5) = add(R(5), R(2));
  OK(load(r, 17u, 5u, (int32_t)UINT32_C(0x0000005f), 1u, 0,
          UINT32_C(0x80075240)));
  R(2) = bitorc(R(0), UINT32_C(0x00000005));
  decided = eq(R(17), R(2), &branch);
  branch = !branch;
  /* Delay 0x8007524C: slti v0,s6,-0x3. */
  R(2) = compare_words(R(22), imm(UINT32_C(0xfffffffd)), 1);
  if (!decided) {
    stop(r, UINT32_C(0x80075248), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075284;
  goto L_80075250;
L_80075250:
  known(&R(31), UINT32_C(0x80075258));
  /* Call delay 0x80075254: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x80075250), UINT32_C(0x8002ab70)));
  goto L_80075258;
L_80075258:
  R(2) = bitandc(R(2), UINT32_C(0x00000008));
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075260: slti v0,s6,-0x3. */
  R(2) = compare_words(R(22), imm(UINT32_C(0xfffffffd)), 1);
  if (!decided) {
    stop(r, UINT32_C(0x8007525c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075284;
  goto L_80075264;
L_80075264:
  known(&R(31), UINT32_C(0x8007526c));
  /* Call delay 0x80075268: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x80075264), UINT32_C(0x8002ab70)));
  goto L_8007526c;
L_8007526c:
  OK(load(r, 3u, 18u, (int32_t)UINT32_C(0x0000006c), 4u, 0,
          UINT32_C(0x8007526c)));
  R(2) = bitandc(R(2), UINT32_C(0x00000008));
  R(2) = compare_words(R(0), R(2), 0);
  R(3) = add(R(3), R(2));
  OK(load(r, 17u, 3u, (int32_t)UINT32_C(0x0000005f), 1u, 0,
          UINT32_C(0x8007527c)));
L_80075280:
  R(2) = compare_words(R(22), imm(UINT32_C(0xfffffffd)), 1);
L_80075284:
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075288: ori v0,zero,0x5. */
  R(2) = bitorc(R(0), UINT32_C(0x00000005));
  if (!decided) {
    stop(r, UINT32_C(0x80075284), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800752b4;
  goto L_8007528c;
L_8007528c:
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffdb58), 4u, 0,
          UINT32_C(0x80075290)));
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdba4), 4u, 0,
          UINT32_C(0x80075298)));
  (void)0;
  R(2) = compare_words(R(2), R(3), 1);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x800752A8: ori v0,zero,0x5. */
  R(2) = bitorc(R(0), UINT32_C(0x00000005));
  if (!decided) {
    stop(r, UINT32_C(0x800752a4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800752b4;
  goto L_800752ac;
L_800752ac:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 0u, 1u, (int32_t)UINT32_C(0xffffdbb4), 2u, UINT32_C(0x800752b0)));
L_800752b4:
  decided = eq(R(17), R(2), &branch);
  branch = !branch;
  /* Delay 0x800752B8: sll v1,s1,0x2. */
  R(3) = shift(R(17), 2u, 0, 0);
  if (!decided) {
    stop(r, UINT32_C(0x800752b4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075314;
  goto L_800752bc;
L_800752bc:
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0x000000a6), 2u, 0,
          UINT32_C(0x800752bc)));
  R(16) = imm(UINT32_C(0x80100000));
  R(16) = add(R(16), imm(UINT32_C(0xffffdbbc)));
  OK(store(r, 2u, 16u, (int32_t)UINT32_C(0x00000000), 2u,
           UINT32_C(0x800752c8)));
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0x000000a8), 2u, 1,
          UINT32_C(0x800752cc)));
  (void)0;
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x800752D8: ori v0,zero,0x1. */
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x800752d4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075304;
  goto L_800752dc;
L_800752dc:
  known(&R(31), UINT32_C(0x800752e4));
  /* Call delay 0x800752E0: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x800752dc), UINT32_C(0x8002ab70)));
  goto L_800752e4;
L_800752e4:
  R(2) = bitandc(R(2), UINT32_C(0x00000fff));
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000999)), 0);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x800752F0: ori v0,zero,0x1. */
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x800752ec), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075304;
  goto L_800752f4;
L_800752f4:
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0x000000a8), 2u, 0,
          UINT32_C(0x800752f4)));
  (void)0;
  OK(store(r, 2u, 16u, (int32_t)UINT32_C(0x00000000), 2u,
           UINT32_C(0x800752fc)));
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
L_80075304:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe89c), 2u, UINT32_C(0x80075308)));
  /* Delay 0x80075310: sll v1,s1,0x2. */
  R(3) = shift(R(17), 2u, 0, 0);
  goto L_8007531c;
L_80075314:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 0u, 1u, (int32_t)UINT32_C(0xffffe89c), 2u, UINT32_C(0x80075318)));
L_8007531c:
  OK(load(r, 4u, 18u, (int32_t)UINT32_C(0x00000038), 1u, 0,
          UINT32_C(0x8007531c)));
  R(2) = imm(UINT32_C(0x800c0000));
  R(2) = add(R(2), imm(UINT32_C(0xffffb68e)));
  R(19) = add(R(3), R(2));
  R(2) = bitorc(R(0), UINT32_C(0x00000007));
  decided = eq(R(4), R(2), &branch);
  /* Delay 0x80075334: ori v0,zero,0x6. */
  R(2) = bitorc(R(0), UINT32_C(0x00000006));
  if (!decided) {
    stop(r, UINT32_C(0x80075330), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8007534c;
  goto L_80075338;
L_80075338:
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0x000000aa), 2u, 1,
          UINT32_C(0x80075338)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075344: addiu s0,v0,-0x1. */
  R(16) = add(R(2), imm(UINT32_C(0xffffffff)));
  if (!decided) {
    stop(r, UINT32_C(0x80075340), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800753b4;
  goto L_80075348;
L_80075348:
  R(2) = bitorc(R(0), UINT32_C(0x00000006));
L_8007534c:
  decided = eq(R(17), R(2), &branch);
  branch = !branch;
  /* Delay 0x80075350: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x8007534c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075394;
  goto L_80075354;
L_80075354:
  known(&R(31), UINT32_C(0x8007535c));
  /* Call delay 0x80075358: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x80075354), UINT32_C(0x8002ab70)));
  goto L_8007535c;
L_8007535c:
  R(2) = bitandc(R(2), UINT32_C(0x00000008));
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075364: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075360), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075374;
  goto L_80075368;
L_80075368:
  OK(load(r, 16u, 18u, (int32_t)UINT32_C(0x000000bb), 1u, 0,
          UINT32_C(0x80075368)));
  /* Delay 0x80075370: nop . */
  (void)0;
  goto L_80075378;
L_80075374:
  OK(load(r, 16u, 18u, (int32_t)UINT32_C(0x000000bc), 1u, 0,
          UINT32_C(0x80075374)));
L_80075378:
  known(&R(31), UINT32_C(0x80075380));
  /* Call delay 0x8007537C: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x80075378), UINT32_C(0x8002ab70)));
  goto L_80075380;
L_80075380:
  R(2) = bitandc(R(2), UINT32_C(0x00000004));
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075388: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075384), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800753b4;
  goto L_8007538c;
L_8007538c:
  /* Delay 0x80075390: addiu s0,s0,0x5. */
  R(16) = add(R(16), imm(UINT32_C(0x00000005)));
  goto L_800753b4;
L_80075394:
  known(&R(31), UINT32_C(0x8007539c));
  /* Call delay 0x80075398: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x80075394), UINT32_C(0x8002ab70)));
  goto L_8007539c;
L_8007539c:
  OK(load(r, 3u, 19u, (int32_t)UINT32_C(0x00000000), 2u, 1,
          UINT32_C(0x8007539c)));
  R(2) = bitandc(R(2), UINT32_C(0x00000038));
  R(16) = shift(R(2), 3u, 1, 0);
  R(3) = compare_words(R(16), R(3), 1);
  decided = eq(R(3), R(0), &branch);
  /* Delay 0x800753B0: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x800753ac), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075394;
  goto L_800753b4;
L_800753b4:
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0x00000078), 1u, 0,
          UINT32_C(0x800753b4)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x800753C0: move a0,s2. */
  R(4) = R(18);
  if (!decided) {
    stop(r, UINT32_C(0x800753bc), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800753d4;
  goto L_800753c4;
L_800753c4:
  R(5) = R(17);
  R(6) = R(16);
  known(&R(31), UINT32_C(0x800753d4));
  /* Call delay 0x800753D0: ori a3,zero,0x1. */
  R(7) = bitorc(R(0), UINT32_C(0x00000001));
  OK(invoke(r, UINT32_C(0x800753cc), UINT32_C(0x80073134)));
  goto L_800753d4;
L_800753d4:
  OK(load(r, 2u, 19u, (int32_t)UINT32_C(0xfffffffe), 2u, 0,
          UINT32_C(0x800753d4)));
  (void)0;
  R(2) = add(R(2), R(16));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe866), 2u, UINT32_C(0x800753e4)));
  OK(load(r, 4u, 18u, (int32_t)UINT32_C(0x00000014), 2u, 0,
          UINT32_C(0x800753e8)));
  known(&R(31), UINT32_C(0x800753f4));
  /* Call delay 0x800753F0: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x800753ec), UINT32_C(0x8007308c)));
  goto L_800753f4;
L_800753f4:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffe894), 2u, 0,
          UINT32_C(0x800753f8)));
  R(3) = bitorc(R(0), UINT32_C(0x0000012c));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 3u, 1u, (int32_t)UINT32_C(0xffffe86a), 2u, UINT32_C(0x80075404)));
  R(2) = bitandc(R(2), UINT32_C(0x0000fffe));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe894), 2u, UINT32_C(0x80075410)));
  /* Delay 0x80075418: nop . */
  (void)0;
  goto L_800754e0;
L_8007541c:
  known(&R(31), UINT32_C(0x80075424));
  /* Call delay 0x80075420: move a0,s2. */
  R(4) = R(18);
  OK(invoke(r, UINT32_C(0x8007541c), UINT32_C(0x80072b70)));
  goto L_80075424;
L_80075424:
  /* Delay 0x80075428: nop . */
  (void)0;
  goto L_8007571c;
L_8007542c:
  R(2) = imm(UINT32_C(0x80100000));
  R(2) = add(R(2), imm(UINT32_C(0xffffe86a)));
  OK(load(r, 3u, 2u, (int32_t)UINT32_C(0x00000000), 2u, 0,
          UINT32_C(0x80075434)));
  R(4) = imm(UINT32_C(0x80100000));
  OK(load(r, 4u, 4u, (int32_t)UINT32_C(0xffffdb6c), 2u, 0,
          UINT32_C(0x8007543c)));
  (void)0;
  R(3) = sub(R(3), R(4));
  OK(store(r, 3u, 2u, (int32_t)UINT32_C(0x00000000), 2u, UINT32_C(0x80075448)));
  R(3) = shift(R(3), 16u, 0, 0);
  predicate = compare_words(R(3), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80075454: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075450), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800754e0;
  goto L_80075458;
L_80075458:
  known(&R(31), UINT32_C(0x80075460));
  /* Call delay 0x8007545C: move a0,s2. */
  R(4) = R(18);
  OK(invoke(r, UINT32_C(0x80075458), UINT32_C(0x80072b70)));
  goto L_80075460;
L_80075460:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffe86c), 2u, 1,
          UINT32_C(0x80075464)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075470: ori s4,zero,0x5. */
  R(20) = bitorc(R(0), UINT32_C(0x00000005));
  if (!decided) {
    stop(r, UINT32_C(0x8007546c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800754e0;
  goto L_80075474;
L_80075474:
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0x00000014), 2u, 0,
          UINT32_C(0x80075474)));
  (void)0;
  R(2) = shift(R(2), 2u, 0, 0);
  R(1) = imm(UINT32_C(0x80020000));
  R(1) = add(R(1), R(2));
  OK(load(r, 17u, 1u, (int32_t)UINT32_C(0x00000bec), 4u, 0,
          UINT32_C(0x80075488)));
  (void)0;
  R(4) = add(R(17), imm(UINT32_C(0x000000da)));
L_80075494:
  OK(load(r, 2u, 4u, (int32_t)UINT32_C(0xffffff2a), 2u, 1,
          UINT32_C(0x80075494)));
  (void)0;
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  branch = !branch;
  /* Delay 0x800754A0: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x8007549c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800754cc;
  goto L_800754a4;
L_800754a4:
  OK(load(r, 2u, 4u, (int32_t)UINT32_C(0xfffffff4), 1u, 0,
          UINT32_C(0x800754a4)));
  (void)0;
  R(2) = bitandc(R(2), UINT32_C(0x00000001));
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x800754B4: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x800754b0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800754cc;
  goto L_800754b8;
L_800754b8:
  OK(load(r, 2u, 4u, (int32_t)UINT32_C(0x00000000), 1u, 0,
          UINT32_C(0x800754b8)));
  (void)0;
  R(2) = bitandc(R(2), UINT32_C(0x00000040));
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x800754C8: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x800754c4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8007541c;
  goto L_800754cc;
L_800754cc:
  R(20) = add(R(20), imm(UINT32_C(0xffffffff)));
  predicate = compare_words(imm(0u), R(20), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x800754D4: addiu a0,a0,0xf4. */
  R(4) = add(R(4), imm(UINT32_C(0x000000f4)));
  if (!decided) {
    stop(r, UINT32_C(0x800754d0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075494;
  goto L_800754d8;
L_800754d8:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 0u, 1u, (int32_t)UINT32_C(0xffffe86c), 2u, UINT32_C(0x800754dc)));
L_800754e0:
  /* Source block beginning at 0x800754E0. */
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0x00000014), 2u, 0,
          UINT32_C(0x800754e0)));
  (void)0;
  R(2) = shift(R(2), 2u, 0, 0);
  R(1) = imm(UINT32_C(0x80020000));
  R(1) = add(R(1), R(2));
  OK(load(r, 17u, 1u, (int32_t)UINT32_C(0x00000bec), 4u, 0,
          UINT32_C(0x800754f4)));
  R(20) = bitorc(R(0), UINT32_C(0x00000005));
  R(4) = add(R(17), imm(UINT32_C(0x000000da)));
L_80075500:
  OK(load(r, 2u, 4u, (int32_t)UINT32_C(0xfffffff4), 1u, 0,
          UINT32_C(0x80075500)));
  (void)0;
  R(2) = bitandc(R(2), UINT32_C(0x00000001));
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075510: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x8007550c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075524;
  goto L_80075514;
L_80075514:
  OK(load(r, 2u, 4u, (int32_t)UINT32_C(0x00000000), 1u, 0,
          UINT32_C(0x80075514)));
  (void)0;
  R(2) = bitandc(R(2), UINT32_C(0x000000b7));
  OK(store(r, 2u, 4u, (int32_t)UINT32_C(0x00000000), 1u, UINT32_C(0x80075520)));
L_80075524:
  R(20) = add(R(20), imm(UINT32_C(0xffffffff)));
  predicate = compare_words(imm(0u), R(20), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x8007552C: addiu a0,a0,0xf4. */
  R(4) = add(R(4), imm(UINT32_C(0x000000f4)));
  if (!decided) {
    stop(r, UINT32_C(0x80075528), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075500;
  goto L_80075530;
L_80075530:
  R(4) = imm(UINT32_C(0x80100000));
  R(4) = add(R(4), imm(UINT32_C(0xffffe878)));
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffe866), 2u, 0,
          UINT32_C(0x8007553c)));
  R(16) = add(R(4), imm(UINT32_C(0xfffffff0)));
  R(17) = imm(UINT32_C(0xffffffff));
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  OK(store(r, 2u, 4u, (int32_t)UINT32_C(0x00000000), 2u, UINT32_C(0x8007554c)));
  R(3) = bitandc(R(3), UINT32_C(0x00007fff));
  R(3) = shift(R(3), 2u, 0, 0);
  R(1) = imm(UINT32_C(0x800c0000));
  R(1) = add(R(1), R(3));
  OK(load(r, 19u, 1u, (int32_t)UINT32_C(0xffffb7f8), 4u, 0,
          UINT32_C(0x80075560)));
L_80075564:
  OK(load(r, 2u, 16u, (int32_t)UINT32_C(0x00000000), 2u, 0,
          UINT32_C(0x80075564)));
  (void)0;
  R(2) = add(R(2), imm(UINT32_C(0x00000001)));
  OK(store(r, 2u, 16u, (int32_t)UINT32_C(0x00000000), 2u,
           UINT32_C(0x80075570)));
  R(2) = shift(R(2), 16u, 0, 0);
  R(2) = shift(R(2), 13u, 1, 1);
  R(2) = add(R(2), R(19));
  OK(load(r, 3u, 2u, (int32_t)UINT32_C(0x00000000), 2u, 1,
          UINT32_C(0x80075580)));
  R(2) = bitorc(R(0), UINT32_C(0x000023ba));
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  /* Delay 0x8007558C: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075588), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800755d0;
  goto L_80075590;
L_80075590:
  OK(load(r, 2u, 16u, (int32_t)UINT32_C(0xfffffffe), 2u, 0,
          UINT32_C(0x80075590)));
  (void)0;
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000004)), 0);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x800755A0: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x8007559c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800755c8;
  goto L_800755a4;
L_800755a4:
  OK(load(r, 4u, 18u, (int32_t)UINT32_C(0x00000014), 2u, 0,
          UINT32_C(0x800755a4)));
  known(&R(31), UINT32_C(0x800755b0));
  /* Call delay 0x800755AC: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x800755a8), UINT32_C(0x80073054)));
  goto L_800755b0;
L_800755b0:
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  OK(store(r, 2u, 16u, (int32_t)UINT32_C(0x00000008), 2u,
           UINT32_C(0x800755b4)));
  OK(store(r, 17u, 16u, (int32_t)UINT32_C(0x00000016), 2u,
           UINT32_C(0x800755b8)));
  OK(store(r, 17u, 16u, (int32_t)UINT32_C(0x00000014), 2u,
           UINT32_C(0x800755bc)));
  OK(store(r, 17u, 16u, (int32_t)UINT32_C(0x00000012), 2u,
           UINT32_C(0x800755c0)));
  OK(store(r, 0u, 16u, (int32_t)UINT32_C(0x00000028), 2u,
           UINT32_C(0x800755c4)));
L_800755c8:
  /* Delay 0x800755CC: sh s1,0x0(s0). */
  OK(store(r, 17u, 16u, (int32_t)UINT32_C(0x00000000), 2u,
           UINT32_C(0x800755cc)));
  goto L_80075564;
L_800755d0:
  R(4) = imm(UINT32_C(0x80100000));
  R(4) = add(R(4), imm(UINT32_C(0xffffe870)));
  OK(load(r, 2u, 4u, (int32_t)UINT32_C(0x00000000), 2u, 1,
          UINT32_C(0x800755d8)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x800755E4: ori v0,zero,0x1. */
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x800755e0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075654;
  goto L_800755e8;
L_800755e8:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdbba), 2u, 1,
          UINT32_C(0x800755ec)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x800755F8: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x800755f4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8007560c;
  goto L_800755fc;
L_800755fc:
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0x000000a4), 2u, 1,
          UINT32_C(0x800755fc)));
  (void)0;
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80075608: ori v0,zero,0x1. */
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x80075604), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075654;
  goto L_8007560c;
L_8007560c:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdba4), 4u, 0,
          UINT32_C(0x80075610)));
  (void)0;
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000169)), 1);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075620: ori v0,zero,0x1. */
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  if (!decided) {
    stop(r, UINT32_C(0x8007561c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075650;
  goto L_80075624;
L_80075624:
  R(3) = imm(UINT32_C(0xffffffff));
  OK(store(r, 2u, 4u, (int32_t)UINT32_C(0x00000000), 2u, UINT32_C(0x80075628)));
  R(2) = imm(UINT32_C(0xffffffff));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe87e), 2u, UINT32_C(0x80075634)));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe87c), 2u, UINT32_C(0x8007563c)));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe87a), 2u, UINT32_C(0x80075644)));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 0u, 1u, (int32_t)UINT32_C(0xffffe890), 2u, UINT32_C(0x8007564c)));
L_80075650:
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
L_80075654:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe86c), 2u, UINT32_C(0x80075658)));
  R(2) = imm(UINT32_C(0xffffffff));
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  /* Delay 0x80075664: ori v0,zero,0x12c. */
  R(2) = bitorc(R(0), UINT32_C(0x0000012c));
  if (!decided) {
    stop(r, UINT32_C(0x80075660), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8007567c;
  goto L_80075668;
L_80075668:
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffe86a), 2u, 1,
          UINT32_C(0x8007566c)));
  (void)0;
  decided = eq(R(3), R(2), &branch);
  /* Delay 0x80075678: ori v1,zero,0x78. */
  R(3) = bitorc(R(0), UINT32_C(0x00000078));
  if (!decided) {
    stop(r, UINT32_C(0x80075674), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075684;
  goto L_8007567c;
L_8007567c:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 3u, 1u, (int32_t)UINT32_C(0xffffe86a), 2u, UINT32_C(0x80075680)));
L_80075684:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffe870), 2u, 1,
          UINT32_C(0x80075688)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075694: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075690), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8007571c;
  goto L_80075698;
L_80075698:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffe868), 2u, 1,
          UINT32_C(0x8007569c)));
  (void)0;
  R(2) = shift(R(2), 3u, 0, 0);
  R(5) = add(R(2), R(19));
  OK(load(r, 4u, 5u, (int32_t)UINT32_C(0x00000002), 2u, 1,
          UINT32_C(0x800756ac)));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 4u, 1u, (int32_t)UINT32_C(0xffffe8fa), 2u, UINT32_C(0x800756b4)));
  predicate = compare_words(R(4), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x800756BC: li v0,-0x1c. */
  R(2) = imm(UINT32_C(0xffffffe4));
  if (!decided) {
    stop(r, UINT32_C(0x800756b8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800756cc;
  goto L_800756c0;
L_800756c0:
  OK(load(r, 3u, 18u, (int32_t)UINT32_C(0x00000014), 2u, 0,
          UINT32_C(0x800756c0)));
  R(2) = bitand_word(R(4), R(2));
  R(4) = add(R(2), R(3));
L_800756cc:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 4u, 1u, (int32_t)UINT32_C(0xffffe87a), 2u, UINT32_C(0x800756d0)));
  OK(load(r, 4u, 5u, (int32_t)UINT32_C(0x00000004), 2u, 1,
          UINT32_C(0x800756d4)));
  (void)0;
  predicate = compare_words(R(4), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x800756E0: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x800756dc), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800756f0;
  goto L_800756e4;
L_800756e4:
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0x00000014), 2u, 0,
          UINT32_C(0x800756e4)));
  (void)0;
  R(4) = add(R(4), R(2));
L_800756f0:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 4u, 1u, (int32_t)UINT32_C(0xffffe87c), 2u, UINT32_C(0x800756f4)));
  OK(load(r, 4u, 5u, (int32_t)UINT32_C(0x00000006), 2u, 1,
          UINT32_C(0x800756f8)));
  (void)0;
  predicate = compare_words(R(4), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80075704: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075700), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075714;
  goto L_80075708;
L_80075708:
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0x00000014), 2u, 0,
          UINT32_C(0x80075708)));
  (void)0;
  R(4) = add(R(4), R(2));
L_80075714:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 4u, 1u, (int32_t)UINT32_C(0xffffe87e), 2u, UINT32_C(0x80075718)));
L_8007571c:
  /* Source block beginning at 0x8007571C. */
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffe8a8), 2u, 1,
          UINT32_C(0x80075720)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x8007572C: ori v0,zero,0x1e. */
  R(2) = bitorc(R(0), UINT32_C(0x0000001e));
  if (!decided) {
    stop(r, UINT32_C(0x80075728), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075784;
  goto L_80075730;
L_80075730:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffe8a4), 2u, 0,
          UINT32_C(0x80075734)));
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffdb6c), 2u, 0,
          UINT32_C(0x8007573c)));
  (void)0;
  R(4) = sub(R(2), R(3));
  R(2) = shift(R(4), 16u, 0, 0);
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 4u, 1u, (int32_t)UINT32_C(0xffffe8a4), 2u, UINT32_C(0x80075750)));
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  branch = !branch;
  /* Delay 0x80075758: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075754), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075d0c;
  goto L_8007575c;
L_8007575c:
  OK(load(r, 2u, 21u, (int32_t)UINT32_C(0x00000072), 2u, 0,
          UINT32_C(0x8007575c)));
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffdb94), 2u, 1,
          UINT32_C(0x80075764)));
  R(2) = add(R(4), R(2));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe8a4), 2u, UINT32_C(0x80075770)));
  predicate = compare_words(R(3), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80075778: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075774), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075d0c;
  goto L_8007577c;
L_8007577c:
  /* Delay 0x80075780: nop . */
  (void)0;
  goto L_80075848;
L_80075784:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe8a4), 2u, UINT32_C(0x80075788)));
  OK(load(r, 2u, 21u, (int32_t)UINT32_C(0x00000014), 2u, 0,
          UINT32_C(0x8007578c)));
  OK(load(r, 3u, 18u, (int32_t)UINT32_C(0x00000014), 2u, 0,
          UINT32_C(0x80075790)));
  R(2) = shift(R(2), 2u, 0, 0);
  R(1) = imm(UINT32_C(0x80020000));
  R(1) = add(R(1), R(2));
  OK(load(r, 19u, 1u, (int32_t)UINT32_C(0x00000bec), 4u, 0,
          UINT32_C(0x800757a0)));
  R(3) = shift(R(3), 2u, 0, 0);
  R(1) = imm(UINT32_C(0x80020000));
  R(1) = add(R(1), R(3));
  OK(load(r, 17u, 1u, (int32_t)UINT32_C(0x00000bec), 4u, 0,
          UINT32_C(0x800757b0)));
  R(20) = bitorc(R(0), UINT32_C(0x00000005));
  R(16) = add(R(19), imm(UINT32_C(0x000000d4)));
  R(18) = add(R(17), imm(UINT32_C(0x000000d4)));
L_800757c0:
  R(3) = imm(UINT32_C(0x80100000));
  OK(load(r, 3u, 3u, (int32_t)UINT32_C(0xffffdbcc), 2u, 1,
          UINT32_C(0x800757c4)));
  OK(load(r, 2u, 17u, (int32_t)UINT32_C(0x00000000), 4u, 0,
          UINT32_C(0x800757c8)));
  (void)0;
  decided = eq(R(2), R(3), &branch);
  /* Delay 0x800757D4: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x800757d0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800757f8;
  goto L_800757d8;
L_800757d8:
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0xffffff46), 1u, 0,
          UINT32_C(0x800757d8)));
  (void)0;
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000007)), 0);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x800757E8: ori v0,zero,0x2f. */
  R(2) = bitorc(R(0), UINT32_C(0x0000002f));
  if (!decided) {
    stop(r, UINT32_C(0x800757e4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800757f8;
  goto L_800757ec;
L_800757ec:
  OK(store(r, 2u, 18u, (int32_t)UINT32_C(0xffffffe4), 2u,
           UINT32_C(0x800757ec)));
  known(&R(31), UINT32_C(0x800757f8));
  /* Call delay 0x800757F4: move a0,s1. */
  R(4) = R(17);
  OK(invoke(r, UINT32_C(0x800757f0), UINT32_C(0x800742c0)));
  goto L_800757f8;
L_800757f8:
  OK(load(r, 2u, 18u, (int32_t)UINT32_C(0xfffffff8), 2u, 0,
          UINT32_C(0x800757f8)));
  (void)0;
  OK(store(r, 2u, 18u, (int32_t)UINT32_C(0x00000000), 2u,
           UINT32_C(0x80075800)));
  OK(load(r, 2u, 16u, (int32_t)UINT32_C(0xffffff46), 1u, 0,
          UINT32_C(0x80075804)));
  (void)0;
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000007)), 0);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075814: move a0,s3. */
  R(4) = R(19);
  if (!decided) {
    stop(r, UINT32_C(0x80075810), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075828;
  goto L_80075818;
L_80075818:
  R(2) = bitorc(R(0), UINT32_C(0x00000002));
  OK(store(r, 2u, 16u, (int32_t)UINT32_C(0xffffff46), 1u,
           UINT32_C(0x8007581c)));
  known(&R(31), UINT32_C(0x80075828));
  /* Call delay 0x80075824: sh zero,-0x1c(s0). */
  OK(store(r, 0u, 16u, (int32_t)UINT32_C(0xffffffe4), 2u,
           UINT32_C(0x80075824)));
  OK(invoke(r, UINT32_C(0x80075820), UINT32_C(0x800742c0)));
  goto L_80075828;
L_80075828:
  R(20) = add(R(20), imm(UINT32_C(0xffffffff)));
  OK(load(r, 2u, 16u, (int32_t)UINT32_C(0x00000002), 2u, 0,
          UINT32_C(0x8007582c)));
  R(18) = add(R(18), imm(UINT32_C(0x000000f4)));
  R(17) = add(R(17), imm(UINT32_C(0x000000f4)));
  R(19) = add(R(19), imm(UINT32_C(0x000000f4)));
  OK(store(r, 2u, 16u, (int32_t)UINT32_C(0x00000000), 2u,
           UINT32_C(0x8007583c)));
  predicate = compare_words(imm(0u), R(20), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80075844: addiu s0,s0,0xf4. */
  R(16) = add(R(16), imm(UINT32_C(0x000000f4)));
  if (!decided) {
    stop(r, UINT32_C(0x80075840), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800757c0;
  goto L_80075848;
L_80075848:
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 0u, 1u, (int32_t)UINT32_C(0xffffe8ae), 2u, UINT32_C(0x8007584c)));
  OK(load(r, 2u, 21u, (int32_t)UINT32_C(0x00000014), 2u, 0,
          UINT32_C(0x80075850)));
  R(20) = bitorc(R(0), UINT32_C(0x00000005));
  R(2) = shift(R(2), 2u, 0, 0);
  R(1) = imm(UINT32_C(0x80020000));
  R(1) = add(R(1), R(2));
  OK(load(r, 19u, 1u, (int32_t)UINT32_C(0x00000bec), 4u, 0,
          UINT32_C(0x80075864)));
  R(8) = bitorc(R(0), UINT32_C(0x00000002));
  R(6) = imm(UINT32_C(0xffffffff));
  R(7) = imm(UINT32_C(0x80020000));
  R(7) = add(R(7), imm(UINT32_C(0x00000bec)));
  R(5) = add(R(19), imm(UINT32_C(0x000000d4)));
L_8007587c:
  OK(load(r, 3u, 5u, (int32_t)UINT32_C(0xffffff46), 1u, 0,
          UINT32_C(0x8007587c)));
  R(2) = bitorc(R(0), UINT32_C(0x0000000a));
  decided = eq(R(3), R(2), &branch);
  /* Delay 0x80075888: sltiu v0,v1,0x7. */
  R(2) = compare_words(R(3), imm(UINT32_C(0x00000007)), 0);
  if (!decided) {
    stop(r, UINT32_C(0x80075884), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075988;
  goto L_8007588c;
L_8007588c:
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075890: ori v0,zero,0x6. */
  R(2) = bitorc(R(0), UINT32_C(0x00000006));
  if (!decided) {
    stop(r, UINT32_C(0x8007588c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075950;
  goto L_80075894;
L_80075894:
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  /* Delay 0x80075898: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075894), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_800758a8;
  goto L_8007589c;
L_8007589c:
  OK(store(r, 8u, 5u, (int32_t)UINT32_C(0xffffff46), 1u, UINT32_C(0x8007589c)));
  /* Delay 0x800758A4: sh a2,0x0(a1). */
  OK(store(r, 6u, 5u, (int32_t)UINT32_C(0x00000000), 2u, UINT32_C(0x800758a4)));
  goto L_80075988;
L_800758a8:
  OK(store(r, 8u, 5u, (int32_t)UINT32_C(0xffffff46), 1u, UINT32_C(0x800758a8)));
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdb90), 2u, 1,
          UINT32_C(0x800758b0)));
  (void)0;
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000080)), 1);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x800758C0: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x800758bc), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075988;
  goto L_800758c4;
L_800758c4:
  OK(load(r, 4u, 5u, (int32_t)UINT32_C(0x00000000), 2u, 1,
          UINT32_C(0x800758c4)));
  (void)0;
  predicate = compare_words(R(4), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x800758D0: sll v0,a0,0x2. */
  R(2) = shift(R(4), 2u, 0, 0);
  if (!decided) {
    stop(r, UINT32_C(0x800758cc), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075988;
  goto L_800758d4;
L_800758d4:
  R(2) = add(R(2), R(7));
  OK(load(r, 17u, 2u, (int32_t)UINT32_C(0x00000000), 4u, 0,
          UINT32_C(0x800758d8)));
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000020), 4u, 0,
          UINT32_C(0x800758dc)));
  OK(load(r, 2u, 17u, (int32_t)UINT32_C(0x00000008), 4u, 0,
          UINT32_C(0x800758e0)));
  (void)0;
  R(2) = bitxor_word(R(2), R(9));
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x800758F0: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x800758ec), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075988;
  goto L_800758f4;
L_800758f4:
  OK(load(r, 4u, 5u, (int32_t)UINT32_C(0xffffffe6), 2u, 0,
          UINT32_C(0x800758f4)));
  (void)0;
  R(2) = compare_words(R(4), imm(UINT32_C(0x00000030)), 1);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075904: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075900), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075988;
  goto L_80075908;
L_80075908:
  OK(load(r, 2u, 17u, (int32_t)UINT32_C(0x000000ba), 2u, 0,
          UINT32_C(0x80075908)));
  (void)0;
  R(2) = compare_words(R(2), R(4), 1);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075918: slti v0,a0,0xa1. */
  R(2) = compare_words(R(4), imm(UINT32_C(0x000000a1)), 1);
  if (!decided) {
    stop(r, UINT32_C(0x80075914), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075968;
  goto L_8007591c;
L_8007591c:
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075920: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x8007591c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075988;
  goto L_80075924;
L_80075924:
  OK(load(r, 2u, 5u, (int32_t)UINT32_C(0xfffffffe), 2u, 1,
          UINT32_C(0x80075924)));
  OK(load(r, 3u, 17u, (int32_t)UINT32_C(0x000000bc), 2u, 1,
          UINT32_C(0x80075928)));
  (void)0;
  R(2) = sub(R(2), R(3));
  R(2) = add(R(2), imm(UINT32_C(0xfffffe80)));
  R(2) = bitandc(R(2), UINT32_C(0x000003ff));
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000101)), 1);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075944: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075940), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075968;
  goto L_80075948;
L_80075948:
  /* Delay 0x8007594C: addiu s4,s4,-0x1. */
  R(20) = add(R(20), imm(UINT32_C(0xffffffff)));
  goto L_8007598c;
L_80075950:
  OK(load(r, 4u, 5u, (int32_t)UINT32_C(0x00000000), 2u, 1,
          UINT32_C(0x80075950)));
  (void)0;
  predicate = compare_words(R(4), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x8007595C: sll v0,a0,0x2. */
  R(2) = shift(R(4), 2u, 0, 0);
  if (!decided) {
    stop(r, UINT32_C(0x80075958), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075988;
  goto L_80075960;
L_80075960:
  R(2) = add(R(2), R(7));
  OK(load(r, 17u, 2u, (int32_t)UINT32_C(0x00000000), 4u, 0,
          UINT32_C(0x80075964)));
L_80075968:
  OK(store(r, 6u, 5u, (int32_t)UINT32_C(0x00000000), 2u, UINT32_C(0x80075968)));
  OK(load(r, 3u, 17u, (int32_t)UINT32_C(0x000000d4), 2u, 1,
          UINT32_C(0x8007596c)));
  OK(load(r, 2u, 19u, (int32_t)UINT32_C(0x00000000), 4u, 0,
          UINT32_C(0x80075970)));
  (void)0;
  decided = eq(R(2), R(3), &branch);
  branch = !branch;
  /* Delay 0x8007597C: ori v0,zero,0x140. */
  R(2) = bitorc(R(0), UINT32_C(0x00000140));
  if (!decided) {
    stop(r, UINT32_C(0x80075978), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075988;
  goto L_80075980;
L_80075980:
  OK(store(r, 6u, 17u, (int32_t)UINT32_C(0x000000d4), 2u,
           UINT32_C(0x80075980)));
  OK(store(r, 2u, 17u, (int32_t)UINT32_C(0x000000d0), 2u,
           UINT32_C(0x80075984)));
L_80075988:
  R(20) = add(R(20), imm(UINT32_C(0xffffffff)));
L_8007598c:
  R(5) = add(R(5), imm(UINT32_C(0x000000f4)));
  predicate = compare_words(imm(0u), R(20), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80075994: addiu s3,s3,0xf4. */
  R(19) = add(R(19), imm(UINT32_C(0x000000f4)));
  if (!decided) {
    stop(r, UINT32_C(0x80075990), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_8007587c;
  goto L_80075998;
L_80075998:
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000038), 4u, 0,
          UINT32_C(0x80075998)));
  (void)0;
  OK(load(r, 2u, 9u, (int32_t)UINT32_C(0x000000d4), 2u, 1,
          UINT32_C(0x800759a0)));
  (void)0;
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  branch = !branch;
  /* Delay 0x800759AC: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x800759a8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075a1c;
  goto L_800759b0;
L_800759b0:
  OK(load(r, 6u, 9u, (int32_t)UINT32_C(0x000000cc), 2u, 1,
          UINT32_C(0x800759b0)));
  OK(load(r, 4u, 29u, (int32_t)UINT32_C(0x00000038), 4u, 0,
          UINT32_C(0x800759b4)));
  known(&R(31), UINT32_C(0x800759c0));
  /* Call delay 0x800759BC: move a1,s5. */
  R(5) = R(21);
  OK(invoke(r, UINT32_C(0x800759b8), UINT32_C(0x8007458c)));
  goto L_800759c0;
L_800759c0:
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x800759C4: ori v0,zero,0x320. */
  R(2) = bitorc(R(0), UINT32_C(0x00000320));
  if (!decided) {
    stop(r, UINT32_C(0x800759c0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075a1c;
  goto L_800759c8;
L_800759c8:
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000030), 4u, 0,
          UINT32_C(0x800759c8)));
  (void)0;
  decided = eq(R(9), R(2), &branch);
  /* Delay 0x800759D4: ori v0,zero,0x2. */
  R(2) = bitorc(R(0), UINT32_C(0x00000002));
  if (!decided) {
    stop(r, UINT32_C(0x800759d0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075a08;
  goto L_800759d8;
L_800759d8:
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000048), 4u, 0,
          UINT32_C(0x800759d8)));
  (void)0;
  OK(load(r, 3u, 9u, (int32_t)UINT32_C(0x0000001a), 1u, 0,
          UINT32_C(0x800759e0)));
  (void)0;
  decided = eq(R(3), R(2), &branch);
  branch = !branch;
  /* Delay 0x800759EC: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x800759e8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075a08;
  goto L_800759f0;
L_800759f0:
  OK(load(r, 4u, 29u, (int32_t)UINT32_C(0x00000038), 4u, 0,
          UINT32_C(0x800759f0)));
  OK(load(r, 5u, 29u, (int32_t)UINT32_C(0x00000048), 4u, 0,
          UINT32_C(0x800759f4)));
  known(&R(31), UINT32_C(0x80075a00));
  /* Call delay 0x800759FC: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x800759f8), UINT32_C(0x80074374)));
  goto L_80075a00;
L_80075a00:
  /* Delay 0x80075A04: nop . */
  (void)0;
  goto L_80075a1c;
L_80075a08:
  OK(load(r, 4u, 29u, (int32_t)UINT32_C(0x00000038), 4u, 0,
          UINT32_C(0x80075a08)));
  known(&R(31), UINT32_C(0x80075a14));
  /* Call delay 0x80075A10: move a1,s5. */
  R(5) = R(21);
  OK(invoke(r, UINT32_C(0x80075a0c), UINT32_C(0x800743c8)));
  goto L_80075a14;
L_80075a14:
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075A18: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075a14), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075d0c;
  goto L_80075a1c;
L_80075a1c:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdbcc), 2u, 1,
          UINT32_C(0x80075a20)));
  (void)0;
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80075A2C: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075a28), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075c70;
  goto L_80075a30;
L_80075a30:
  R(17) = imm(UINT32_C(0x80100000));
  OK(load(r, 17u, 17u, (int32_t)UINT32_C(0xffffdc34), 4u, 0,
          UINT32_C(0x80075a34)));
  (void)0;
  OK(load(r, 4u, 17u, (int32_t)UINT32_C(0x000000d4), 2u, 1,
          UINT32_C(0x80075a3c)));
  (void)0;
  predicate = compare_words(R(4), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80075A48: sll v0,a0,0x2. */
  R(2) = shift(R(4), 2u, 0, 0);
  if (!decided) {
    stop(r, UINT32_C(0x80075a44), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075a60;
  goto L_80075a4c;
L_80075a4c:
  R(1) = imm(UINT32_C(0x80020000));
  R(1) = add(R(1), R(2));
  OK(load(r, 19u, 1u, (int32_t)UINT32_C(0x00000bec), 4u, 0,
          UINT32_C(0x80075a54)));
  /* Delay 0x80075A5C: ori v0,zero,0x4. */
  R(2) = bitorc(R(0), UINT32_C(0x00000004));
  goto L_80075a94;
L_80075a60:
  OK(load(r, 6u, 17u, (int32_t)UINT32_C(0x000000cc), 2u, 1,
          UINT32_C(0x80075a60)));
  R(4) = R(17);
  known(&R(31), UINT32_C(0x80075a70));
  /* Call delay 0x80075A6C: move a1,s5. */
  R(5) = R(21);
  OK(invoke(r, UINT32_C(0x80075a68), UINT32_C(0x8007458c)));
  goto L_80075a70;
L_80075a70:
  R(19) = R(2);
  decided = eq(R(19), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075A78: ori v0,zero,0x4. */
  R(2) = bitorc(R(0), UINT32_C(0x00000004));
  if (!decided) {
    stop(r, UINT32_C(0x80075a74), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075a94;
  goto L_80075a7c;
L_80075a7c:
  R(4) = R(17);
  known(&R(31), UINT32_C(0x80075a88));
  /* Call delay 0x80075A84: move a1,s5. */
  R(5) = R(21);
  OK(invoke(r, UINT32_C(0x80075a80), UINT32_C(0x80074488)));
  goto L_80075a88;
L_80075a88:
  R(19) = R(2);
  decided = eq(R(19), R(0), &branch);
  /* Delay 0x80075A90: ori v0,zero,0x4. */
  R(2) = bitorc(R(0), UINT32_C(0x00000004));
  if (!decided) {
    stop(r, UINT32_C(0x80075a8c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075d0c;
  goto L_80075a94;
L_80075a94:
  OK(store(r, 2u, 19u, (int32_t)UINT32_C(0x0000001a), 1u,
           UINT32_C(0x80075a94)));
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdb90), 2u, 1,
          UINT32_C(0x80075a9c)));
  (void)0;
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000080)), 1);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075AAC: addiu s0,s5,0x5c. */
  R(16) = add(R(21), imm(UINT32_C(0x0000005c)));
  if (!decided) {
    stop(r, UINT32_C(0x80075aa8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075ca4;
  goto L_80075ab0;
L_80075ab0:
  OK(load(r, 2u, 21u, (int32_t)UINT32_C(0x000000ba), 1u, 0,
          UINT32_C(0x80075ab0)));
  (void)0;
  R(2) = bitandc(R(2), UINT32_C(0x00000002));
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075AC0: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075abc), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075c20;
  goto L_80075ac4;
L_80075ac4:
  OK(load(r, 2u, 17u, (int32_t)UINT32_C(0x0000001c), 4u, 0,
          UINT32_C(0x80075ac4)));
  (void)0;
  OK(load(r, 6u, 2u, (int32_t)UINT32_C(0x0000001e), 1u, 0,
          UINT32_C(0x80075acc)));
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  decided = eq(R(6), R(2), &branch);
  /* Delay 0x80075AD8: ori s4,zero,0x5. */
  R(20) = bitorc(R(0), UINT32_C(0x00000005));
  if (!decided) {
    stop(r, UINT32_C(0x80075ad4), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075ca8;
  goto L_80075adc;
L_80075adc:
  OK(load(r, 4u, 17u, (int32_t)UINT32_C(0x000000ba), 2u, 0,
          UINT32_C(0x80075adc)));
  (void)0;
  R(2) = compare_words(R(4), imm(UINT32_C(0x00000038)), 0);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075AEC: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075ae8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075c20;
  goto L_80075af0;
L_80075af0:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdbba), 2u, 1,
          UINT32_C(0x80075af4)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075B00: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075afc), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075b0c;
  goto L_80075b04;
L_80075b04:
  predicate = compare_words(imm(0u), R(22), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80075B08: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075b04), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075c20;
  goto L_80075b0c;
L_80075b0c:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffe8b2), 2u, 1,
          UINT32_C(0x80075b10)));
  R(5) = bitorc(R(0), UINT32_C(0x00000002));
  decided = eq(R(2), R(5), &branch);
  branch = !branch;
  /* Delay 0x80075B1C: ori v0,zero,0xeffe. */
  R(2) = bitorc(R(0), UINT32_C(0x0000effe));
  if (!decided) {
    stop(r, UINT32_C(0x80075b18), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075b38;
  goto L_80075b20;
L_80075b20:
  OK(load(r, 3u, 17u, (int32_t)UINT32_C(0x00000008), 4u, 0,
          UINT32_C(0x80075b20)));
  (void)0;
  R(3) = add(R(3), imm(UINT32_C(0x000077ff)));
  R(2) = compare_words(R(2), R(3), 0);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075B34: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075b30), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075c20;
  goto L_80075b38;
L_80075b38:
  decided = eq(R(6), R(5), &branch);
  /* Delay 0x80075B3C: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075b38), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075c20;
  goto L_80075b40;
L_80075b40:
  OK(load(r, 2u, 21u, (int32_t)UINT32_C(0x00000062), 2u, 1,
          UINT32_C(0x80075b40)));
  (void)0;
  R(2) = compare_words(R(2), R(4), 1);
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075B50: sltiu v0,a0,0xc8. */
  R(2) = compare_words(R(4), imm(UINT32_C(0x000000c8)), 0);
  if (!decided) {
    stop(r, UINT32_C(0x80075b4c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075b6c;
  goto L_80075b54;
L_80075b54:
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075B58: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075b54), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075b6c;
  goto L_80075b5c;
L_80075b5c:
  OK(load(r, 2u, 21u, (int32_t)UINT32_C(0x00000039), 1u, 0,
          UINT32_C(0x80075b5c)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075B68: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075b64), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075ca0;
  goto L_80075b6c;
L_80075b6c:
  R(2) = imm(UINT32_C(0x80020000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0x00001d72), 1u, 0,
          UINT32_C(0x80075b70)));
  (void)0;
  R(2) = compare_words(R(2), imm(UINT32_C(0x00000002)), 0);
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075B80: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075b7c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075c20;
  goto L_80075b84;
L_80075b84:
  R(16) = imm(UINT32_C(0x80100000));
  R(16) = add(R(16), imm(UINT32_C(0xffffe8b4)));
  OK(load(r, 2u, 16u, (int32_t)UINT32_C(0x00000000), 2u, 1,
          UINT32_C(0x80075b8c)));
  (void)0;
  predicate = compare_words(imm(0u), R(2), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80075B98: move v1,v0. */
  R(3) = R(2);
  if (!decided) {
    stop(r, UINT32_C(0x80075b94), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075be8;
  goto L_80075b9c;
L_80075b9c:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffe8b6), 2u, 1,
          UINT32_C(0x80075ba0)));
  (void)0;
  predicate = compare_words(imm(0u), R(2), 1);
  decided = bool_value(predicate, &branch);
  branch = !branch;
  /* Delay 0x80075BAC: move v1,v0. */
  R(3) = R(2);
  if (!decided) {
    stop(r, UINT32_C(0x80075ba8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075bd0;
  goto L_80075bb0;
L_80075bb0:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdb6c), 2u, 0,
          UINT32_C(0x80075bb4)));
  (void)0;
  R(2) = sub(R(3), R(2));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe8b6), 2u, UINT32_C(0x80075bc4)));
  /* Delay 0x80075BCC: addiu s0,s5,0x5c. */
  R(16) = add(R(21), imm(UINT32_C(0x0000005c)));
  goto L_80075ca4;
L_80075bd0:
  known(&R(31), UINT32_C(0x80075bd8));
  /* Call delay 0x80075BD4: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x80075bd0), UINT32_C(0x8002ab70)));
  goto L_80075bd8;
L_80075bd8:
  R(2) = bitandc(R(2), UINT32_C(0x0000003f));
  R(2) = add(R(2), imm(UINT32_C(0x0000001e)));
  /* Delay 0x80075BE4: sh v0,0x0(s0). */
  OK(store(r, 2u, 16u, (int32_t)UINT32_C(0x00000000), 2u,
           UINT32_C(0x80075be4)));
  goto L_80075ca0;
L_80075be8:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffdb6c), 2u, 0,
          UINT32_C(0x80075bec)));
  (void)0;
  R(2) = sub(R(3), R(2));
  OK(store(r, 2u, 16u, (int32_t)UINT32_C(0x00000000), 2u,
           UINT32_C(0x80075bf8)));
  R(2) = shift(R(2), 16u, 0, 0);
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  branch = !branch;
  /* Delay 0x80075C04: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075c00), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075c20;
  goto L_80075c08;
L_80075c08:
  known(&R(31), UINT32_C(0x80075c10));
  /* Call delay 0x80075C0C: nop . */
  (void)0;
  OK(invoke(r, UINT32_C(0x80075c08), UINT32_C(0x8002ab70)));
  goto L_80075c10;
L_80075c10:
  R(2) = bitandc(R(2), UINT32_C(0x0000007f));
  R(2) = add(R(2), imm(UINT32_C(0x0000003c)));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe8b6), 2u, UINT32_C(0x80075c1c)));
L_80075c20:
  OK(load(r, 6u, 17u, (int32_t)UINT32_C(0x000000cc), 2u, 1,
          UINT32_C(0x80075c20)));
  R(4) = R(17);
  known(&R(31), UINT32_C(0x80075c30));
  /* Call delay 0x80075C2C: move a1,s5. */
  R(5) = R(21);
  OK(invoke(r, UINT32_C(0x80075c28), UINT32_C(0x80074688)));
  goto L_80075c30;
L_80075c30:
  R(19) = R(2);
  decided = eq(R(19), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075C38: ori v0,zero,0x6. */
  R(2) = bitorc(R(0), UINT32_C(0x00000006));
  if (!decided) {
    stop(r, UINT32_C(0x80075c34), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075c58;
  goto L_80075c3c;
L_80075c3c:
  OK(load(r, 6u, 29u, (int32_t)UINT32_C(0x00000038), 4u, 0,
          UINT32_C(0x80075c3c)));
  R(4) = R(17);
  known(&R(31), UINT32_C(0x80075c4c));
  /* Call delay 0x80075C48: move a1,s5. */
  R(5) = R(21);
  OK(invoke(r, UINT32_C(0x80075c44), UINT32_C(0x80074714)));
  goto L_80075c4c;
L_80075c4c:
  R(19) = R(2);
  decided = eq(R(19), R(0), &branch);
  /* Delay 0x80075C54: ori v0,zero,0x6. */
  R(2) = bitorc(R(0), UINT32_C(0x00000006));
  if (!decided) {
    stop(r, UINT32_C(0x80075c50), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075d0c;
  goto L_80075c58;
L_80075c58:
  OK(store(r, 2u, 19u, (int32_t)UINT32_C(0x0000001a), 1u,
           UINT32_C(0x80075c58)));
  R(2) = bitorc(R(0), UINT32_C(0x00000001));
  R(1) = imm(UINT32_C(0x80100000));
  OK(store(r, 2u, 1u, (int32_t)UINT32_C(0xffffe8ae), 2u, UINT32_C(0x80075c64)));
  /* Delay 0x80075C6C: addiu s0,s5,0x5c. */
  R(16) = add(R(21), imm(UINT32_C(0x0000005c)));
  goto L_80075ca4;
L_80075c70:
  R(2) = imm(UINT32_C(0x80100000));
  OK(load(r, 2u, 2u, (int32_t)UINT32_C(0xffffe8aa), 2u, 1,
          UINT32_C(0x80075c74)));
  (void)0;
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075C80: ori v0,zero,0x320. */
  R(2) = bitorc(R(0), UINT32_C(0x00000320));
  if (!decided) {
    stop(r, UINT32_C(0x80075c7c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075ca0;
  goto L_80075c84;
L_80075c84:
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000028), 4u, 0,
          UINT32_C(0x80075c84)));
  (void)0;
  decided = eq(R(9), R(2), &branch);
  /* Delay 0x80075C90: ori v0,zero,0x4. */
  R(2) = bitorc(R(0), UINT32_C(0x00000004));
  if (!decided) {
    stop(r, UINT32_C(0x80075c8c), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075ca0;
  goto L_80075c94;
L_80075c94:
  OK(load(r, 9u, 29u, (int32_t)UINT32_C(0x00000040), 4u, 0,
          UINT32_C(0x80075c94)));
  (void)0;
  OK(store(r, 2u, 9u, (int32_t)UINT32_C(0x0000001a), 1u, UINT32_C(0x80075c9c)));
L_80075ca0:
  R(16) = add(R(21), imm(UINT32_C(0x0000005c)));
L_80075ca4:
  R(20) = bitorc(R(0), UINT32_C(0x00000005));
L_80075ca8:
  R(18) = imm(UINT32_C(0x80020000));
  R(18) = add(R(18), imm(UINT32_C(0x00000bec)));
L_80075cb0:
  OK(load(r, 2u, 16u, (int32_t)UINT32_C(0x00000000), 1u, 1,
          UINT32_C(0x80075cb0)));
  (void)0;
  R(2) = shift(R(2), 2u, 0, 0);
  R(2) = add(R(2), R(18));
  OK(load(r, 17u, 2u, (int32_t)UINT32_C(0x00000000), 4u, 0,
          UINT32_C(0x80075cc0)));
  (void)0;
  OK(load(r, 2u, 17u, (int32_t)UINT32_C(0x000000d4), 2u, 1,
          UINT32_C(0x80075cc8)));
  (void)0;
  predicate = compare_words(R(2), imm(0u), 1);
  decided = bool_value(predicate, &branch);
  branch = !branch;
  /* Delay 0x80075CD4: addiu s0,s0,0x1. */
  R(16) = add(R(16), imm(UINT32_C(0x00000001)));
  if (!decided) {
    stop(r, UINT32_C(0x80075cd0), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075d00;
  goto L_80075cd8;
L_80075cd8:
  OK(load(r, 6u, 17u, (int32_t)UINT32_C(0x000000cc), 2u, 1,
          UINT32_C(0x80075cd8)));
  R(4) = R(17);
  known(&R(31), UINT32_C(0x80075ce8));
  /* Call delay 0x80075CE4: move a1,s5. */
  R(5) = R(21);
  OK(invoke(r, UINT32_C(0x80075ce0), UINT32_C(0x8007458c)));
  goto L_80075ce8;
L_80075ce8:
  decided = eq(R(2), R(0), &branch);
  branch = !branch;
  /* Delay 0x80075CEC: move a0,s1. */
  R(4) = R(17);
  if (!decided) {
    stop(r, UINT32_C(0x80075ce8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075d00;
  goto L_80075cf0;
L_80075cf0:
  known(&R(31), UINT32_C(0x80075cf8));
  /* Call delay 0x80075CF4: move a1,s5. */
  R(5) = R(21);
  OK(invoke(r, UINT32_C(0x80075cf0), UINT32_C(0x800743c8)));
  goto L_80075cf8;
L_80075cf8:
  decided = eq(R(2), R(0), &branch);
  /* Delay 0x80075CFC: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075cf8), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075d0c;
  goto L_80075d00;
L_80075d00:
  R(20) = add(R(20), imm(UINT32_C(0xffffffff)));
  predicate = compare_words(imm(0u), R(20), 1);
  decided = bool_value(predicate, &branch);
  /* Delay 0x80075D08: nop . */
  (void)0;
  if (!decided) {
    stop(r, UINT32_C(0x80075d04), 0u, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (branch)
    goto L_80075cb0;
  goto L_80075d0c;
L_80075d0c:
  /* Source block beginning at 0x80075D0C. */
  OK(load(r, 31u, 29u, (int32_t)UINT32_C(0x00000094), 4u, 0,
          UINT32_C(0x80075d0c)));
  OK(load(r, 30u, 29u, (int32_t)UINT32_C(0x00000090), 4u, 0,
          UINT32_C(0x80075d10)));
  OK(load(r, 23u, 29u, (int32_t)UINT32_C(0x0000008c), 4u, 0,
          UINT32_C(0x80075d14)));
  OK(load(r, 22u, 29u, (int32_t)UINT32_C(0x00000088), 4u, 0,
          UINT32_C(0x80075d18)));
  OK(load(r, 21u, 29u, (int32_t)UINT32_C(0x00000084), 4u, 0,
          UINT32_C(0x80075d1c)));
  OK(load(r, 20u, 29u, (int32_t)UINT32_C(0x00000080), 4u, 0,
          UINT32_C(0x80075d20)));
  OK(load(r, 19u, 29u, (int32_t)UINT32_C(0x0000007c), 4u, 0,
          UINT32_C(0x80075d24)));
  OK(load(r, 18u, 29u, (int32_t)UINT32_C(0x00000078), 4u, 0,
          UINT32_C(0x80075d28)));
  OK(load(r, 17u, 29u, (int32_t)UINT32_C(0x00000074), 4u, 0,
          UINT32_C(0x80075d2c)));
  OK(load(r, 16u, 29u, (int32_t)UINT32_C(0x00000070), 4u, 0,
          UINT32_C(0x80075d30)));
  R(29) = add(R(29), imm(UINT32_C(0x00000098)));
  /* Return delay 0x80075D3C: nop . */
  (void)0;
  if (R(31).known_mask != 15u) {
    stop(r, UINT32_C(0x80075d38), R(31).word, 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if (R(31).word & 3u) {
    stop(r, UINT32_C(0x80075d38), R(31).word, 0u);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  progress->restored_return_address = R(31);
  progress->restored_fp = R(30);
  progress->restored_saved[0] = R(16);
  progress->restored_saved[1] = R(17);
  progress->restored_saved[2] = R(18);
  progress->restored_saved[3] = R(19);
  progress->restored_saved[4] = R(20);
  progress->restored_saved[5] = R(21);
  progress->restored_saved[6] = R(22);
  progress->restored_saved[7] = R(23);
  progress->stopped_pc = 0u;
  progress->stopped_address = 0u;
  progress->stopped_entry = 0u;
  progress->completed = 1u;
  publish(r);
  return NBA97_TEXT_COMPLETE;
}
