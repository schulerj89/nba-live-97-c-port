#include "game_graphics_submit_adapter.h"
#include "recovered/game_bios_memory_copy.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
namespace {
using U32 = std::uint32_t;
unsigned checks;
void ck(bool v, unsigned l) {
  ++checks;
  if (!v) {
    std::fprintf(stderr, "graphics integration check %u line %u\n", checks, l);
    std::exit(1);
  }
}
#define CHECK(x) ck((x), __LINE__)
struct F {
  static constexpr U32 Base = 0x80000000u, Stack = 0x801ff000u,
                       Env = 0x80010000u, Dispatch = 0x80030000u,
                       Fn = 0x80050000u;
  std::vector<std::uint8_t> b = std::vector<std::uint8_t>(0x200000, 0xa5);
  std::vector<std::uint8_t> k = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Base, b.data(), k.data(), b.size()};
  std::array<Nba97GameGraphicsSubmitAccess, 256> journal{};
  Nba97GameDrawEnvironmentContext parent{};
  Nba97GameDrawEnvironmentProgress pp{};
  Nba97GameGraphicsSubmitBinding bind{};
  std::vector<Nba97GameDrawEnvironmentEvent> fall;
  std::vector<Nba97GameGraphicsSubmitEvent> child;
  bool queue = false;
  Nba97GameBiosMemoryCopyProgress copyProgress{};
  bool fallback_refuses = false;
  int child_malformed = 0;
  F() {
    for (unsigned i = 0; i < 32; i++)
      parent.machine.registers.gpr[i] = {0x10000000u + i, 15};
    parent.machine.registers.gpr[0] = {0, 15};
    parent.machine.registers.gpr[4] = {Env, 15};
    parent.machine.registers.gpr[29] = {Stack, 15};
    parent.machine.registers.gpr[31] = {0x81234568, 15};
    parent.machine.hi = {0x11112222, 15};
    parent.machine.lo = {0x33334444, 15};
    parent.memory = {&region, 1};
    parent.operation_budget = 100;
    parent.io = fallback;
    parent.user = this;
    bind.operation_budget = 300;
    bind.io = service;
    bind.user = this;
    bind.access_journal = journal.data();
    bind.access_journal_capacity = journal.size();
    put(0x800c55c2, 0, 1);
    put(0x800c55b8, Dispatch);
    put(Dispatch + 0x18, Fn);
    put(Dispatch + 8, 0x8009b298);
    put(Env + 0x1c, 0x12000000);
    for (unsigned i = 0; i < 16; i++)
      put(Env + 0x1c + i * 4, 0xabc00000u + i);
    put(0x800c56c4, 1);
    put(0x800c56c8, 0);
    put(0x800c55c1, 0, 1);
    put(0x800c56a0, 0x80030080);
    put(0x80030080, 0);
    put(0x800c55cc, 0);
    put(0x800c5694, 0x80030084);
    put(0x80030084, 0x04000000);
  }
  void put(U32 a, U32 v, unsigned w = 4) {
    for (unsigned i = 0; i < w; i++) {
      b[a - Base + i] = (std::uint8_t)(v >> (8 * i));
      k[a - Base + i] = 1;
    }
  }
  U32 get(U32 a) const {
    U32 v = 0;
    for (unsigned i = 0; i < 4; i++)
      v |= U32(b[a - Base + i]) << (8 * i);
    return v;
  }
  static int bios(void* opaque,const Nba97GameTextMemory*,const Nba97GameBiosMemoryCopyEvent* e,Nba97GameBiosMemoryCopyMachine* m) {
    auto& f=*static_cast<F*>(opaque);
    CHECK(e->pc==0x8009cb10u&&e->delay_slot_pc==0x8009cb14u&&e->entry==0xa0u&&e->service==0x2a);
    const U32 d=m->registers.gpr[4].word,s=m->registers.gpr[5].word,n=m->registers.gpr[6].word;
    CHECK(n==0x5c);
    std::memmove(f.b.data()+d-Base,f.b.data()+s-Base,n);
    std::memmove(f.k.data()+d-Base,f.k.data()+s-Base,n);
    return 1;
  }
  static int fallback(void *o, const Nba97GameTextMemory *memory,
                      const Nba97GameDrawEnvironmentEvent *e,
                      Nba97GameDrawEnvironmentMachine *m) {
    auto &f = *static_cast<F *>(o);
    f.fall.push_back(*e);
    if (f.fallback_refuses)
      return 0;
    if (e->kind == NBA97_GAME_DRAW_ENVIRONMENT_COPY_8009CB0C) {
      Nba97GameBiosMemoryCopyContext c{};c.memory=*memory;c.operation_budget=1;c.machine=*m;c.io=bios;c.user=&f;
      const int result=nba97_game_bios_memory_copy(&c,&f.copyProgress);*m=f.copyProgress.machine;
      return result==NBA97_TEXT_COMPLETE;

    }
    return 1;
  }
  static int service(void *o, const Nba97GameTextMemory *,
                     const Nba97GameGraphicsSubmitEvent *e,
                     Nba97GameGraphicsSubmitMachine *m) {
    auto &f = *static_cast<F *>(o);
    f.child.push_back(*e);
    if (e->kind == NBA97_GAME_GRAPHICS_SUBMIT_CRITICAL && e->pc == 0x8009b304)
      m->registers.gpr[2] = {0x55, 15};
    if (f.child_malformed == 1) {
      m->registers.gpr[0].known_mask = 14;
    } else if (f.child_malformed == 2) {
      m->hi = {0xfeed0001u, 16};
    } else if (f.child_malformed == 3) {
      m->lo = {0xfeed0002u, 16};
    }
    return 1;
  }
  int run() {
    if (queue)
      put(0x800c55c1, 1, 1);
    return nba97_game_draw_environment_with_graphics_submit(&parent, &bind,
                                                            &pp);
  }
};
void natural(bool queue) {
  F f;
  f.queue = queue;
  CHECK(f.run() == NBA97_TEXT_COMPLETE && f.pp.completed &&
        f.bind.invocations == 1 && f.bind.completions == 1 &&
        f.bind.event.pc == 0x80099b58 &&
        f.bind.event.delay_slot_pc == 0x80099b5c &&
        f.bind.event.entry == 0x8009b298 && f.bind.event.argument_count == 4);
  CHECK(f.bind.progress.completed && f.bind.progress.queued == queue);
  CHECK(f.copyProgress.completed&&f.copyProgress.machine.registers.gpr[9].word==0x2a&&f.copyProgress.machine.registers.gpr[10].word==0xa0);
  CHECK(std::memcmp(f.b.data()+0xc55d0,f.b.data()+F::Env-F::Base,0x5c)==0);
  CHECK(f.fall.size() == 2 &&
        f.fall[0].kind == NBA97_GAME_DRAW_ENVIRONMENT_PACKET_8009A344 &&
        f.fall[1].kind == NBA97_GAME_DRAW_ENVIRONMENT_COPY_8009CB0C &&
        f.bind.fallback_callbacks_completed == 2);
  if (queue) {
    U32 q = 0x80104748 + 96;
    CHECK(f.get(q) == F::Fn);
    CHECK(f.get(q + 4) == q + 12);
    CHECK(f.get(q + 8) == 0);
    CHECK(f.get(q + 12) == 0xabffffff);
    CHECK(f.get(0x800c56c4) == 2);
  } else
    CHECK(f.child.size() == 4 &&
          f.child[2].kind == NBA97_GAME_GRAPHICS_SUBMIT_INDIRECT &&
          f.child[2].entry == F::Fn);
}
void failure() {
  F f;
  f.bind.operation_budget = 1;
  CHECK(f.run() == NBA97_TEXT_LIMIT && f.bind.result == NBA97_TEXT_LIMIT &&
        f.pp.stopped_pc == 0x80099b58 &&
        f.bind.progress.stopped_pc == 0x8009b2a4);
  auto machine = f.parent.machine;
  machine.registers.gpr[31] = {0x80099b60, 15};
  Nba97GameDrawEnvironmentEvent e{
      0x80099b58, 0x80099b5c, 0x8009b298,
      1,          1,          NBA97_GAME_DRAW_ENVIRONMENT_SUBMIT_INDIRECT,
      4};
  for (unsigned i = 0; i < 4; i++) {
    F x;
    auto bad = e;
    auto copy = machine;
    if (i == 0)
      bad.pc += 4;
    if (i == 1)
      bad.entry += 4;
    if (i == 2)
      bad.argument_count = 3;
    if (i == 3)
      copy.registers.gpr[31].word += 4;
    auto before = copy;
    CHECK(nba97_game_graphics_submit_from_draw_environment(
              &x.bind, &x.parent.memory, &bad, &copy) == 0 &&
          x.bind.invocations == 0 &&
          std::memcmp(&copy, &before, sizeof copy) == 0);
  }

  for (int malformed = 1; malformed <= 3; ++malformed) {
    F x;
    x.child_malformed = malformed;
    CHECK(x.run() == NBA97_TEXT_ARGUMENT);
    CHECK(x.bind.result == NBA97_TEXT_ARGUMENT && x.bind.invocations == 1 &&
          x.bind.completions == 0);
    CHECK(x.pp.stopped_pc == 0x80099b58 &&
          x.bind.progress.stopped_pc == 0x8009b2bc);
    if (malformed == 1)
      CHECK(x.pp.machine.registers.gpr[0].known_mask == 14);
    else if (malformed == 2)
      CHECK(x.pp.machine.hi.word == 0xfeed0001u &&
            x.pp.machine.hi.known_mask == 16);
    else
      CHECK(x.pp.machine.lo.word == 0xfeed0002u &&
            x.pp.machine.lo.known_mask == 16);
  }

  F fallback;
  fallback.fallback_refuses = true;
  CHECK(fallback.run() == NBA97_TEXT_IO_REFUSED);
  CHECK(fallback.bind.invocations == 0 && fallback.fall.size() == 1);
}
} // namespace
int main() {
  natural(false);
  natural(true);
  failure();
  std::printf("game graphics submit integration tests passed (%u checks)\n",
              checks);
}
