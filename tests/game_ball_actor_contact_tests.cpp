#include "recovered/game_ball_actor_contact.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {
int failures, checks;
void check_at(bool value, int line) {
  ++checks;
  if (!value) {
    ++failures;
    std::cerr << "failed line " << line << "\n";
  }
}
#define check(value) check_at((value), __LINE__)
struct Fixture {
  static constexpr uint32_t ball = 0x80001000, actor = 0x80002000;
  std::vector<uint8_t> bytes = std::vector<uint8_t>(0x200000),
                       known = std::vector<uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{0x80000000, bytes.data(), known.data(),
                             bytes.size()};
  Nba97GameBallActorContactContext c{};
  Nba97GameBallActorContactProgress p{};
  std::vector<Nba97GameBallActorContactAccess> journal =
      std::vector<Nba97GameBallActorContactAccess>(512);
  std::vector<uint32_t> calls;
  int32_t contact = 1, distance = 0;
  uint8_t contact_mask = 15;
  uint32_t refuse = 0, mutate = 0;
  bool poison_target_after_acquire = false;
  int32_t velocity_first = 0, velocity_second = 0;
  std::vector<uint32_t> multiply_scales;
  Nba97GameBallActorContactWord first_zdelta{};
  Fixture() {
    c.memory.region = &region;
    c.memory.count = 1;
    c.operation_budget = 10000;
    c.io = io;
    c.user = this;
    c.access_journal = journal.data();
    c.access_journal_capacity = journal.size();
    for (auto &g : c.machine.registers.gpr) {
      g.word = 0;
      g.known_mask = 15;
    }
    c.machine.hi.known_mask = c.machine.lo.known_mask = 15;
    c.machine.registers.gpr[29].word = 0x801ff000;
    c.machine.registers.gpr[31].word = 0x80060edc;
    c.machine.registers.gpr[4].word = ball;
    c.machine.registers.gpr[5].word = actor;
    put16(0x800fdbcc, 0xffff);
    put32(0x800fdb58, 1);
    put16(0x800fe8c4, 0);
    put16(0x800fe8cc, 0);
    put16(0x800fdb90, 0);
    put16(0x800fdb94, 0);
    put16(0x800fdbd4, 0);
    put16(0x800fdbd2, 0);
    put16(0x800fdbd0, 0xffff);
    put32(0x800fdc40, 0x8001edf4);
    put32(0x800fdc48, actor);
    put8(actor + 0xd9, 0);
    put16(actor + 4, 0xffff);
    put32(actor + 0x20, 0x80003000);
    put8(0x8000300d, 0);
  }
  void put8(uint32_t a, uint8_t v) { bytes[a - region.base] = v; }
  void put16(uint32_t a, uint16_t v) {
    put8(a, uint8_t(v));
    put8(a + 1, uint8_t(v >> 8));
  }
  void put32(uint32_t a, uint32_t v) {
    put16(a, uint16_t(v));
    put16(a + 2, uint16_t(v >> 16));
  }
  uint8_t get8(uint32_t a) const { return bytes[a - region.base]; }
  uint16_t get16(uint32_t a) const {
    return uint16_t(get8(a) | (uint16_t(get8(a + 1)) << 8));
  }
  uint32_t get32(uint32_t a) const {
    return get16(a) | (uint32_t(get16(a + 2)) << 16);
  }
  int run() { return nba97_game_ball_actor_contact(&c, &p); }
  bool saw(uint32_t pc) const {
    return std::find(calls.begin(), calls.end(), pc) != calls.end();
  }
  static int io(void *u, const Nba97GameTextMemory *,
                const Nba97GameBallActorContactEvent *e,
                Nba97GameBallActorContactMachine *m) {
    auto &f = *static_cast<Fixture *>(u);
    f.calls.push_back(e->pc);
    if (e->pc == f.refuse)
      return 0;
    if (e->pc == 0x8006036c)
      f.first_zdelta = m->registers.gpr[5];
    if (e->entry == 0x8007066c) {
      int32_t value =
          e->pc == 0x8006036c
              ? f.distance
              : (e->pc == 0x80060d80 ? f.velocity_first : f.velocity_second);
      m->registers.gpr[2].word = uint32_t(value);
      m->registers.gpr[2].known_mask = 15;
    }
    if (e->entry == 0x800601b8 || e->entry == 0x80060240 ||
        e->entry == 0x80060008) {
      m->registers.gpr[2].word = uint32_t(f.contact);
      m->registers.gpr[2].known_mask = f.contact_mask;
    }
    if (e->entry == 0x800aa788)
      f.multiply_scales.push_back(m->registers.gpr[5].word);
    if (e->entry == 0x8005d140 || e->entry == 0x8002ab70 ||
        e->entry == 0x800a5638 || e->entry == 0x800a5634 ||
        e->entry == 0x800aa788) {
      m->registers.gpr[2].word = 0;
      m->registers.gpr[2].known_mask = 15;
    }
    if (e->entry == 0x8005d140 && f.poison_target_after_acquire) {
      f.known[0xfdbd2] = f.known[0xfdbd3] = 0;
    }
    if (e->pc == f.mutate) {
      m->registers.gpr[8].word = 0x12345678;
      m->registers.gpr[8].known_mask = 15;
      m->hi.word = 0x89abcdef;
      m->lo.word = 0xfedcba98;
      m->hi.known_mask = m->lo.known_mask = 15;
    }
    return 1;
  }
};
void expect_complete(Fixture &f) {
  check(f.run() == NBA97_TEXT_COMPLETE);
  check(f.p.completed);
  check(f.p.machine.registers.gpr[29].word == 0x801ff000);
  check(f.p.restored_return_address.word == 0x80060edc);
}
} // namespace
int main() {
  Fixture normal;
  expect_complete(normal);
  check(normal.calls.size() >= 5);
  check(normal.get16(Fixture::ball + 0x18) == 0);
  {
    const uint32_t addresses[] = {0x801fefe0, 0x801feff8, 0x801feff4,
                                  0x801feff0, 0x801fefec, 0x801fefe8,
                                  0x801fefe4, 0x800fdbcc};
    const uint32_t pcs[] = {0x800602d0, 0x800602dc, 0x800602e0, 0x800602e4,
                            0x800602e8, 0x800602ec, 0x800602f0, 0x800602f4};
    for (unsigned i = 0; i < 8; ++i) {
      check(normal.journal[i].address == addresses[i]);
      check(normal.journal[i].pc == pcs[i]);
      check(normal.journal[i].operation == i + 1);
    }
  }
  {
    Fixture f;
    f.put16(Fixture::actor + 0xb4, 1);
    expect_complete(f);
    check(f.calls.empty());
  }
  {
    Fixture f;
    f.put16(Fixture::ball + 0xb4, 1);
    expect_complete(f);
    check(f.calls.empty());
  }
  {
    Fixture f;
    f.put32(Fixture::actor + 0xc, uint32_t(33 << 8));
    expect_complete(f);
    check(f.calls.empty());
  }
  for (int x : {-33, -32, 32, 33}) {
    Fixture f;
    f.put32(Fixture::actor + 0xc, uint32_t(x * 256));
    expect_complete(f);
    check(f.saw(0x8006036c) == (x >= -32 && x <= 32));
  }
  {
    Fixture f;
    f.distance = 33;
    expect_complete(f);
    check(f.calls.size() == 1);
  }
  for (int d : {-1, 0, 32, 33}) {
    Fixture f;
    f.distance = d;
    expect_complete(f);
    check(f.saw(0x80060548) == (d <= 32));
  }
  {
    Fixture f;
    f.put32(Fixture::ball + 0x10, 81 << 8);
    expect_complete(f);
    check(f.calls.size() == 1);
  }
  for (int h : {-1, 0, 80, 81}) {
    Fixture f;
    f.put32(Fixture::ball + 0x10, uint32_t(h * 256));
    expect_complete(f);
    check(f.saw(0x80060548) == (h >= 0 && h <= 80));
  }
  {
    Fixture f;
    f.put32(0x800fdb58, 0);
    expect_complete(f);
    check(f.calls.size() == 1);
  }
  {
    Fixture f;
    f.put16(0x800fe8c4, 1);
    expect_complete(f);
    check(!f.saw(0x80060548));
  }
  {
    Fixture f;
    f.put16(0x800fdbcc, 0);
    f.put32(0x800fdc34, 0x80004000);
    f.put8(0x800040d9, 0);
    expect_complete(f);
    check(f.calls.empty());
  }
  {
    Fixture f;
    f.contact = 0;
    expect_complete(f);
    check(f.calls.size() >= 2);
  }
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f;
    f.contact_mask = uint8_t(mask);
    int result = f.run();
    check(result == ((mask & 1u) ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    if (mask & 1u)
      check(f.p.completed);
  }
  {
    Fixture f;
    f.put16(0x800fdb90, 0x82);
    f.put16(0x800fe880, 0);
    f.put16(0x800fe884, 1);
    expect_complete(f);
    check(f.saw(0x80060548));
  }
  {
    Fixture f;
    f.put16(0x800fe8cc, 1);
    f.put16(0x800fe8ca, 0);
    expect_complete(f);
    check(f.saw(0x80060548));
  }
  {
    Fixture f;
    f.contact = int32_t(0x12340001);
    expect_complete(f);
    check(f.get16(Fixture::ball + 0x18) == 0);
  }
  {
    Fixture f;
    f.contact = int32_t(0x1234ffff);
    f.put16(0x800fdb96, 0);
    expect_complete(f);
    check(f.get16(Fixture::actor + 0xb4) == 0);
    check(f.get16(Fixture::ball + 0x14) == 0);
  }
  Fixture deflect;
  deflect.contact = -1;
  deflect.put16(0x800fdb96, 1);
  deflect.put16(0x800fdb94, 1);
  deflect.put16(0x800fdbd4, 1);
  deflect.put32(0x800fdc70, 0x80004000);
  deflect.put16(0x8000400c, 998);
  deflect.put16(Fixture::actor + 4, 0);
  deflect.put32(0x800fdc50, 0x80005000);
  deflect.put16(0x8000500c, 0xffff);
  deflect.put8(Fixture::actor + 0xdf, 0xff);
  expect_complete(deflect);
  check(deflect.get8(Fixture::actor + 0xdf) == 0);
  check(deflect.get16(0x8000400c) == 999);
  check(deflect.get16(0x8000500c) == 0);
  check(deflect.get16(Fixture::actor + 0xb4) == 15);
  {
    Fixture capped;
    capped.contact = -1;
    capped.put16(0x800fdb96, 1);
    capped.put16(0x800fdb94, 1);
    capped.put16(0x800fdbd4, 1);
    capped.put32(0x800fdc70, 0x80004000);
    capped.put16(0x8000400c, 999);
    capped.put16(Fixture::actor + 4, 0);
    capped.put32(0x800fdc50, 0x80005000);
    capped.put16(0x8000500c, 0);
    capped.put8(Fixture::actor + 0xdf, 0);
    expect_complete(capped);
    check(capped.get8(Fixture::actor + 0xdf) == 1);
    check(capped.get16(0x8000400c) == 999);
    check(capped.get16(0x8000500c) == 1);
  }
  for (int distance_value : {100, 1000, 2000}) {
    Fixture f;
    f.contact = -1;
    f.put16(0x800fdb94, 1);
    f.put16(0x800fdb96, 0);
    f.velocity_second = distance_value;
    expect_complete(f);
    check(!f.multiply_scales.empty());
    check(f.multiply_scales[0] ==
          uint32_t(distance_value < 512
                       ? 256
                       : (distance_value > 1344 ? 672 : distance_value / 2)));
  }
  {
    Fixture f;
    f.mutate = 0x80060918;
    expect_complete(f);
    check(f.p.machine.registers.gpr[8].word == 0x12345678);
    check(f.p.machine.hi.word == 0x89abcdef);
  }
  {
    Fixture full;
    check(full.run() == NBA97_TEXT_COMPLETE);
    for (size_t b = 0; b < full.p.operations; b++) {
      Fixture f;
      f.c.operation_budget = b;
      check(f.run() == NBA97_TEXT_LIMIT);
      check(f.p.operations == b);
    }
  }
  {
    Fixture full;
    full.contact = -1;
    full.put16(0x800fdb96, 0);
    check(full.run() == NBA97_TEXT_COMPLETE);
    for (size_t b = 0; b < full.p.operations; b++) {
      Fixture f;
      f.contact = -1;
      f.put16(0x800fdb96, 0);
      f.c.operation_budget = b;
      check(f.run() == NBA97_TEXT_LIMIT);
      check(f.p.operations == b);
    }
  }
  {
    Fixture probe;
    check(probe.run() == NBA97_TEXT_COMPLETE);
    for (uint32_t pc : probe.calls) {
      Fixture f;
      f.refuse = pc;
      check(f.run() == NBA97_TEXT_IO_REFUSED);
      check(f.p.stopped_pc == pc);
    }
  }
  {
    Fixture probe;
    probe.contact = -1;
    probe.put16(0x800fdb94, 1);
    probe.put16(0x800fdb96, 0);
    check(probe.run() == NBA97_TEXT_COMPLETE);
    for (uint32_t pc : probe.calls) {
      Fixture f;
      f.contact = -1;
      f.put16(0x800fdb94, 1);
      f.put16(0x800fdb96, 0);
      f.refuse = pc;
      check(f.run() == NBA97_TEXT_IO_REFUSED);
      check(f.p.stopped_pc == pc);
    }
  }
  {
    Fixture full;
    full.put16(0x800fdb90, 0x81);
    full.put16(0x800fdbd2, 0xffff);
    full.put32(0x80020bec, Fixture::actor);
    full.put32(0x80020c00, Fixture::actor);
    full.put16(Fixture::actor + 0x46, 0x27);
    check(full.run() == NBA97_TEXT_COMPLETE);
    std::vector<uint32_t> duplicate_timer_pcs;
    for (size_t i = 0; i < full.p.access_events; ++i)
      if (full.journal[i].kind == NBA97_GAME_MATCH_CLOCKS_STORE &&
          full.journal[i].address == Fixture::actor + 0xb4 &&
          full.journal[i].pc >= 0x80060958 && full.journal[i].pc <= 0x80060964)
        duplicate_timer_pcs.push_back(full.journal[i].pc);
    check(duplicate_timer_pcs ==
          std::vector<uint32_t>(
              {0x80060958, 0x8006095c, 0x80060960, 0x80060964}));
    for (size_t budget = 0; budget < full.p.operations; ++budget) {
      Fixture f;
      f.put16(0x800fdb90, 0x81);
      f.put16(0x800fdbd2, 0xffff);
      f.put32(0x80020bec, Fixture::actor);
      f.put32(0x80020c00, Fixture::actor);
      f.put16(Fixture::actor + 0x46, 0x27);
      f.c.operation_budget = budget;
      check(f.run() == NBA97_TEXT_LIMIT);
      check(f.p.operations == budget);
    }
  }
  {
    Fixture f;
    f.known[0x800fdbcc - f.region.base + 1] = 0;
    check(f.run() == NBA97_TEXT_UNKNOWN);
    check(f.p.stopped_pc == 0x80060300);
    check(f.p.machine.registers.gpr[4].word ==
          f.c.machine.registers.gpr[6].word);
  }
  {
    Fixture f;
    f.put32(Fixture::actor + 0xc, 0x1ff);
    f.put32(Fixture::ball + 0xc, 0x100);
    f.known[Fixture::ball - f.region.base + 0xc] = 0;
    expect_complete(f);
    check(f.first_zdelta.word == 0);
    check(f.first_zdelta.known_mask == 15);
  }
  {
    Fixture f;
    f.c.machine.registers.gpr[29].known_mask = 14;
    check(f.run() == NBA97_TEXT_UNKNOWN);
    check(f.p.stopped_pc == 0x800602d0);
  }
  {
    Fixture f;
    std::vector<uint8_t> low(0x20), low_known(0x20, 1);
    Nba97GameTextRegion regions[2] = {
        f.region, {0, low.data(), low_known.data(), low.size()}};
    f.c.memory.region = regions;
    f.c.memory.count = 2;
    f.c.machine.registers.gpr[29].word = 0x20;
    check(f.run() == NBA97_TEXT_COMPLETE);
    check(f.p.completed);
    check(f.p.frame_stack_pointer == 0xffffffe0);
    check(f.p.machine.registers.gpr[29].word == 0x20);
    check(f.p.restored_return_address.word == 0x80060edc);
  }
  {
    Fixture f;
    f.c.machine.registers.gpr[29].word = 0x21;
    check(f.run() == NBA97_TEXT_ALIGNMENT_TRAP);
    check(f.p.stopped_pc == 0x800602d0);
  }
  {
    Fixture f;
    f.c.machine.registers.gpr[5].word = 0x70000000;
    check(f.run() == NBA97_TEXT_RESOURCE);
    check(f.p.stopped_pc == 0x80060328);
  }
  {
    Fixture f;
    f.c.machine.registers.gpr[31].known_mask = 14;
    check(f.run() == NBA97_TEXT_UNKNOWN);
    check(f.p.stopped_pc == 0x80060e84);
  }
  {
    Fixture f;
    f.c.machine.registers.gpr[0].word = 1;
    check(f.run() == NBA97_TEXT_ARGUMENT);
  }
  // Retired 608A4 fragment prefixes are reached through the complete owner.
  {
    Fixture f;
    f.poison_target_after_acquire = true;
    check(f.run() == NBA97_TEXT_UNKNOWN);
    check(f.p.stopped_pc == 0x800608bc);
    check(f.get16(0x800fdb88) == 1);
  }
  for (uint32_t slot : {0x80020becu, 0x80020c00u}) {
    Fixture f;
    f.put16(0x800fdb90, 0x81);
    f.put16(0x800fdbd2, 0xffff);
    f.put32(0x80020bec, Fixture::actor);
    f.put32(0x80020c00, Fixture::actor);
    for (unsigned i=0;i<4;++i) f.known[slot-f.region.base+i]=0;
    check(f.run() == NBA97_TEXT_UNKNOWN);
    check(f.p.stopped_pc == (slot==0x80020becu ? 0x8006095cu : 0x80060958u));
    check(f.get16(0x800fe880)==0 && f.get16(0x800fdb96)==0 && f.get16(0x800fdb72)==0);
    check(f.get16(Fixture::actor+0xb4)==(slot==0x80020becu ? 30 : 0));
  }
  if (failures) {
    std::cerr << failures << " failures in " << checks << " checks\n";
    return 1;
  }
  std::cout << "game_ball_actor_contact focused: " << checks << " checks\n";
}
