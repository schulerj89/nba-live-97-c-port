#include "recovered/game_actor_resume.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "actor resume check %u failed at %u\n", checks, line);
    std::exit(1);
  }
}
#define check(v) check_at((v), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u, Actor = 0x80012000u;
constexpr std::uint32_t Actor2 = 0x80012200u, Actor3 = 0x80012400u;
constexpr std::uint32_t Nested = 0x80013000u, EntrySp = 0x800ff000u;

struct Call {
  Nba97GameActorResumeEvent event{};
  Nba97GameActorResumeMachine before{};
};
struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x110000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameActorResumeAccess, 64> journal{};
  Nba97GameActorResumeContext context{};
  Nba97GameActorResumeProgress progress{};
  std::vector<Call> calls;
  unsigned refuse = 0;
  bool mutate = false;
  Fixture() {
    context.memory = {&region, 1};
    context.operation_budget = 22;
    context.io = io;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {
          0x41000000u + i * 0x01010101u,
          static_cast<std::uint8_t>((i % 15) + 1)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {Actor, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {0x44556677u, 5};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x800676d4u,
                                                                15};
    context.machine.hi = {0x12345678u, 5};
    context.machine.lo = {0x9abcdef0u, 10};
    put(0x800fdb90u, 0x82, 2);
    put(0x800fe880u, 5, 2);
    put(0x800fdb94u, 5, 2);
    init_actor(Actor);
    init_actor(Actor2);
    init_actor(Actor3);
    put(Nested + 0xd, 1, 1);
  }
  void init_actor(std::uint32_t a) {
    put(a + 0xd9, 5, 1);
    put(a + 0x46, 37, 2);
    put(a + 0x4a, 37, 2);
    put(a + 0x4e, 0x7777, 2);
    put(a + 0x60, 0, 2);
    put(a + 0x64, 0, 2);
    put(a + 0x20, Nested, 4);
    put(a + 0x9a, 0x5555, 2);
    put(a + 0xa2, 0xbeef, 2);
  }
  void put(std::uint32_t a, std::uint32_t v, unsigned w,
           std::uint8_t mask = 15) {
    auto n = a - Ram;
    for (unsigned i = 0; i < w; ++i) {
      bytes[n + i] = static_cast<std::uint8_t>(v >> (8 * i));
      known[n + i] = static_cast<std::uint8_t>((mask >> i) & 1);
    }
  }
  std::uint32_t get(std::uint32_t a, unsigned w) const {
    std::uint32_t v = 0;
    auto n = a - Ram;
    for (unsigned i = 0; i < w; ++i)
      v |= std::uint32_t(bytes[n + i]) << (8 * i);
    return v;
  }
  std::uint8_t km(std::uint32_t a, unsigned w) const {
    std::uint8_t m = 0;
    auto n = a - Ram;
    for (unsigned i = 0; i < w; ++i)
      m |= known[n + i] << i;
    return m;
  }
  static int io(void *u, const Nba97GameTextMemory *,
                const Nba97GameActorResumeEvent *e,
                Nba97GameActorResumeMachine *m) {
    auto &f = *static_cast<Fixture *>(u);
    f.calls.push_back({*e, *m});
    unsigned n = f.calls.size();
    if (f.refuse == n)
      return 0;
    if (f.mutate) {
      if (n == 1) {
        m->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {Actor2, 15};
        m->registers.gpr[8] = {0x11111111, 3};
        m->hi = {0xaaaaaaaa, 6};
      }
      if (n == 2) {
        m->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {Actor3, 15};
        m->registers.gpr[9] = {0x22222222, 12};
        m->lo = {0xbbbbbbbb, 9};
      }
      if (n == 3) {
        constexpr std::uint32_t frame = 0x800ff080u;
        f.put(frame + 0x14, 0x81234567, 4);
        f.put(frame + 0x10, 0x89abcdef, 4, 6);
        m->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {frame, 15};
        m->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0x13572468, 7};
        m->registers.gpr[10] = {0x33333333, 5};
        m->hi = {0xcccccccc, 3};
        m->lo = {0xdddddddd, 12};
      }
    }
    if (n == 3 && !f.mutate)
      m->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0x13572468, 7};
    return 1;
  }
  int run() { return nba97_game_actor_resume(&context, &progress); }
};
bool same(Nba97GameActorResumeWord a, Nba97GameActorResumeWord b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}
bool saw_pc(const Fixture &f, std::uint32_t pc) {
  for (size_t i = 0; i < f.progress.access_events; ++i)
    if (f.journal[i].pc == pc)
      return true;
  return false;
}

void exact_normal_path() {
  Fixture f;
  auto entry = f.context.machine;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
  check(f.progress.operations == 22 && f.progress.accesses == 19 &&
        f.progress.reads == 12 && f.progress.stores == 7 &&
        f.progress.callbacks_completed == 3 && f.progress.access_events == 19);
  std::array<std::uint32_t, 19> pcs{
      {0x800582e0, 0x800582e8, 0x800582f8, 0x800582fc, 0x80058304, 0x80058338,
       0x8005833c, 0x8005834c, 0x80058350, 0x80058384, 0x80058398, 0x800583ac,
       0x800583b4, 0x800583c8, 0x800583d0, 0x800583dc, 0x800583e4, 0x800583e8,
       0x800583ec}};
  for (unsigned i = 0; i < pcs.size(); ++i)
    check(f.journal[i].pc == pcs[i]);
  check(f.calls.size() == 3 && f.calls[0].event.pc == 0x80058374 &&
        f.calls[0].event.entry == 0x80056ffc &&
        f.calls[0].event.operation == 10 &&
        f.calls[0].before.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            0x44556677 &&
        f.calls[0].before.registers.gpr[NBA97_MATCH_INITIALIZE_A1].known_mask ==
            5);
  check(f.calls[1].event.pc == 0x8005837c &&
        f.calls[1].before.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            Actor &&
        f.calls[2].event.pc == 0x800583e0 && f.calls[2].event.operation == 20 &&
        f.calls[2].before.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x800583e8);
  check(f.get(Actor + 0x1a, 1) == 1 && f.get(Actor + 0x4e, 2) == 0 &&
        f.get(Actor + 0x9a, 2) == 3 && f.get(Actor + 0xb8, 2) == 47 &&
        f.get(Actor + 0xa6, 2) == 0xbeef);
  check(
      same(f.progress.restored_return_address,
           entry.registers.gpr[NBA97_MATCH_INITIALIZE_RA]) &&
      same(f.progress.restored_s0,
           entry.registers.gpr[NBA97_MATCH_INITIALIZE_S0]) &&
      f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
          EntrySp &&
      f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
          0x13572468 &&
      f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask ==
          7 &&
      same(f.progress.machine.hi, entry.hi) &&
      same(f.progress.machine.lo, entry.lo));
}

void signed_team_selection() {
  struct Case {
    std::uint16_t phase, team;
    std::uint8_t actor, want;
  };
  std::array<Case, 9> c{{{0x82, 0xffff, 0xff, 2},
                         {0x82, 0x00ff, 0xff, 1},
                         {0, 0xffff, 0xff, 2},
                         {0, 0, 0, 1},
                         {0, 0x00ff, 0xff, 1},
                         {0x7fff, 5, 4, 2},
                         {0x8000, 5, 5, 1},
                         {0xffff, 0, 0, 1},
                         {1, 0, 0xff, 2}}};
  for (auto x : c) {
    Fixture f;
    f.put(0x800fdb90, x.phase, 2);
    f.put(x.phase == 0x82 ? 0x800fe880u : 0x800fdb94u, x.team, 2);
    f.put(Actor + 0xd9, x.actor, 1);
    check(f.run() == NBA97_TEXT_COMPLETE && f.get(Actor + 0x1a, 1) == x.want);
  }
}

void animation_thresholds_and_flags() {
  struct T {
    unsigned a, b;
    bool forced, second;
  };
  std::array<T, 4> t{{{36, 0, true, false},
                      {37, 36, true, true},
                      {37, 37, false, true},
                      {0xffff, 0xffff, false, true}}};
  for (auto x : t) {
    Fixture f;
    f.put(Actor + 0x46, x.a, 2);
    f.put(Actor + 0x4a, x.b, 2);
    check(f.run() == NBA97_TEXT_COMPLETE);
    check((f.calls[0].before.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
           1) == x.forced);
    check(saw_pc(f, 0x80058350) == x.second);
  }
  for (unsigned a : {1u, 2u, 3u}) {
    Fixture f;
    f.put(Actor + 0x60, a, 2);
    check(f.run() == NBA97_TEXT_COMPLETE && f.get(Actor + 0x9a, 2) == 0x5555 &&
          !saw_pc(f, 0x80058398));
  }
  for (unsigned a : {1u, 2u, 3u}) {
    Fixture f;
    f.put(Actor + 0x64, a, 2);
    check(f.run() == NBA97_TEXT_COMPLETE && f.get(Actor + 0x9a, 2) == 0x5555 &&
          !saw_pc(f, 0x800583ac));
  }
  Fixture four;
  four.put(Actor + 0x60, 4, 2);
  four.put(Actor + 0x64, 4, 2);
  check(four.run() == NBA97_TEXT_COMPLETE && four.get(Actor + 0x9a, 2) == 3);
  Fixture z;
  z.put(Nested + 0xd, 0, 1);
  check(z.run() == NBA97_TEXT_COMPLETE && z.get(Actor + 0x9a, 2) == 0);
  Fixture nz;
  check(nz.run() == NBA97_TEXT_COMPLETE && nz.get(Actor + 0x9a, 2) == 3);
}

void callback_mutation_and_refusal() {
  Fixture f;
  f.mutate = true;
  check(f.run() == NBA97_TEXT_COMPLETE);
  check(f.calls[1].before.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        Actor2);
  check(f.calls[2].before.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            Actor3 &&
        f.get(Actor3 + 0xb8, 2) == 47 && f.get(Actor3 + 0xa6, 2) == 0xbeef);
  check(f.progress.restored_return_address.word == 0x81234567 &&
        f.progress.restored_s0.word == 0x89abcdef &&
        f.progress.restored_s0.known_mask == 6 &&
        f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x800ff098 &&
        f.progress.machine.registers.gpr[8].word == 0x11111111 &&
        f.progress.machine.registers.gpr[9].word == 0x22222222 &&
        f.progress.machine.registers.gpr[10].word == 0x33333333 &&
        f.progress.machine.hi.word == 0xcccccccc &&
        f.progress.machine.lo.word == 0xdddddddd);
  for (unsigned n = 1; n <= 3; ++n) {
    Fixture r;
    r.refuse = n;
    check(r.run() == NBA97_TEXT_IO_REFUSED &&
          r.progress.callbacks_completed == n - 1 &&
          r.progress.stopped_pc == (n == 1   ? 0x80058374u
                                    : n == 2 ? 0x8005837cu
                                             : 0x800583e0u) &&
          r.progress.stopped_entry == (n == 1   ? 0x80056ffcu
                                       : n == 2 ? 0x8005703cu
                                                : 0x800582ccu));
  }
}

void every_budget_prefix() {
  for (unsigned b = 0; b <= 22; ++b) {
    Fixture f;
    f.context.operation_budget = b;
    int rc = f.run();
    check((b == 22 && rc == NBA97_TEXT_COMPLETE && f.progress.completed) ||
          (b < 22 && rc == NBA97_TEXT_LIMIT && !f.progress.completed));
    check(f.progress.operations == b);
  }
}

void unknown_alignment_mapping_alias_and_metadata() {
  Fixture phase;
  phase.put(0x800fdb90, 0x82, 2, 0);
  check(phase.run() == NBA97_TEXT_UNKNOWN &&
        phase.progress.stopped_pc == 0x800582f4 && phase.progress.stores == 2);
  Fixture threshold;
  threshold.put(Actor + 0x46, 36, 2, 2);
  check(threshold.run() == NBA97_TEXT_UNKNOWN &&
        threshold.progress.stopped_pc == 0x80058348 &&
        threshold.get(Actor + 0x4e, 2) == 0 &&
        threshold.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 1 &&
        threshold.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 0x0e);
  Fixture flags;
  flags.put(Actor + 0x60, 0, 2, 2);
  check(flags.run() == NBA97_TEXT_UNKNOWN &&
        flags.progress.stopped_pc == 0x80058390);
  Fixture pointer;
  pointer.put(Actor + 0x20, Nested, 4, 7);
  check(pointer.run() == NBA97_TEXT_UNKNOWN &&
        pointer.progress.stopped_pc == 0x800583b4);
  Fixture descriptor;
  descriptor.put(Nested + 0xd, 1, 1, 0);
  check(descriptor.run() == NBA97_TEXT_UNKNOWN &&
        descriptor.progress.stopped_pc == 0x800583bc &&
        descriptor.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 3 &&
        descriptor.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 0x0f &&
        descriptor.get(Actor + 0x9a, 2) == 0x5555 &&
        !saw_pc(descriptor, 0x800583c8) && !saw_pc(descriptor, 0x800583cc));
  Fixture partial;
  partial.put(Actor + 0xa2, 0xabcd, 2, 1);
  check(partial.run() == NBA97_TEXT_COMPLETE &&
        partial.get(Actor + 0xa6, 2) == 0xabcd &&
        partial.km(Actor + 0xa6, 2) == 1);
  Fixture raw;
  raw.put(Actor + 0xa2, 0xabcd, 2, 1);
  const std::size_t split = Actor + 0xa6 - Ram;
  Nba97GameTextRegion raw_regions[3]{
      {Ram, raw.bytes.data(), raw.known.data(), split},
      {Actor + 0xa6, raw.bytes.data() + split, nullptr, 2},
      {Actor + 0xa8, raw.bytes.data() + split + 2, raw.known.data() + split + 2,
       raw.bytes.size() - split - 2}};
  raw.context.memory = {raw_regions, 3};
  check(raw.run() == NBA97_TEXT_ARGUMENT &&
        raw.progress.stopped_pc == 0x800583e4);
  Fixture ra;
  ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7;
  check(ra.run() == NBA97_TEXT_UNKNOWN &&
        ra.progress.stopped_pc == 0x800583f4 && ra.progress.reads == 12);
  Fixture sp;
  sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 7;
  check(sp.run() == NBA97_TEXT_UNKNOWN && sp.progress.stopped_pc == 0x800582e8);
  Fixture unaligned;
  unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word =
      Actor + 1;
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x8005833c);
  Fixture missing;
  missing.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word =
      0x90000000;
  check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.progress.stopped_pc == 0x800582fc);
  Fixture overlap;
  Nba97GameTextRegion rs[2]{overlap.region, overlap.region};
  overlap.context.memory = {rs, 2};
  check(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture alias;
  alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
      Actor + 0x24, 15};
  check(alias.run() == NBA97_TEXT_COMPLETE &&
        alias.get(Actor + 0x20, 4) == 0x800676d4 &&
        alias.progress.restored_return_address.word == 0x800676d4 &&
        alias.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x800676d4);
  Fixture wrapping;
  std::array<std::uint8_t, 0x20> low{};
  std::array<std::uint8_t, 0x20> low_known{};
  low_known.fill(1);
  Nba97GameTextRegion wrapping_regions[2]{
      wrapping.region, {0, low.data(), low_known.data(), low.size()}};
  wrapping.context.memory = {wrapping_regions, 2};
  wrapping.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x10,
                                                                       15};
  const auto entry_s0 =
      wrapping.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0];
  const auto low_word = [&low](unsigned at) {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= std::uint32_t(low[at + i]) << (8u * i);
    return value;
  };
  check(
      wrapping.run() == NBA97_TEXT_COMPLETE &&
      wrapping.progress.frame_stack_pointer == 0xfffffff8 &&
      wrapping.journal[1].pc == 0x800582e8 &&
      wrapping.journal[1].address == 0x00000008 &&
      wrapping.journal[2].pc == 0x800582f8 &&
      wrapping.journal[2].address == 0x0000000c &&
      wrapping.journal[17].pc == 0x800583e8 &&
      wrapping.journal[17].address == 0x0000000c &&
      wrapping.journal[18].pc == 0x800583ec &&
      wrapping.journal[18].address == 0x00000008 &&
      low_word(8) == entry_s0.word && low_word(12) == 0x800676d4 &&
      wrapping.progress.restored_s0.word == entry_s0.word &&
      wrapping.progress.restored_s0.known_mask == entry_s0.known_mask &&
      wrapping.progress.restored_return_address.word == 0x800676d4 &&
      wrapping.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
          0x10);
}

void arguments() {
  Fixture f;
  Nba97GameActorResumeProgress p{};
  check(nba97_game_actor_resume(nullptr, &p) == NBA97_TEXT_ARGUMENT);
  check(nba97_game_actor_resume(&f.context, nullptr) == NBA97_TEXT_ARGUMENT);
  f.context.machine.registers.gpr[0] = {1, 15};
  check(f.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad;
  bad.known[0x12000 + 0x46] = 2;
  check(bad.run() == NBA97_TEXT_ARGUMENT &&
        bad.progress.stopped_pc == 0x8005833c);
}
} // namespace
int main() {
  exact_normal_path();
  signed_team_selection();
  animation_thresholds_and_flags();
  callback_mutation_and_refusal();
  every_budget_prefix();
  unknown_alignment_mapping_alias_and_metadata();
  arguments();
  std::printf("game actor resume: %u checks passed\n", checks);
}
