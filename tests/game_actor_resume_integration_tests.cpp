#include "game_actor_resume_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool v, unsigned l) {
  ++checks;
  if (!v) {
    std::fprintf(stderr, "actor resume integration check %u failed at %u\n",
                 checks, l);
    std::exit(1);
  }
}
#define check(v) check_at((v), __LINE__)
constexpr std::uint32_t Ram = 0x80000000u, Actor = 0x80012000u,
                        Nested = 0x80013000u, Ball = 0x80014000u;
struct Composition {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x110000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameActorResumeAccess, 32> journal{};
  Nba97GamePeriodExpiryContext expiry{};
  Nba97GamePeriodExpiryProgress expiry_progress{};
  Nba97GameActorResumeBinding actor{};
  std::vector<Nba97GameActorResumeEvent> children;
  unsigned refuse{};
  Composition() {
    expiry.memory = {&region, 1};
    expiry.operation_budget = 100;
    expiry.io = nba97_game_actor_resume_from_period_expiry;
    expiry.user = &actor;
    for (unsigned i = 0; i < 32; ++i)
      expiry.machine.registers.gpr[i] = {0x51000000u + i * 0x01010101u, 15};
    expiry.machine.registers.gpr[0] = {0, 15};
    expiry.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ff000, 15};
    expiry.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80068d74, 15};
    expiry.machine.hi = {0x12345678, 5};
    expiry.machine.lo = {0x9abcdef0, 10};
    actor.operation_budget = 22;
    actor.io = child;
    actor.user = this;
    actor.access_journal = journal.data();
    actor.access_journal_capacity = journal.size();
    put(0x800fdb58, 0, 4);
    put(0x800fdbcc, 0, 2);
    put(0x800fdc34, Actor, 4);
    put(Actor + 0x1a, 16, 1);
    put(0x800fdb90, 0x82, 2);
    put(0x800fe880, 5, 2);
    put(0x800fdb94, 5, 2);
    put(Actor + 0xd9, 5, 1);
    put(Actor + 0x46, 37, 2);
    put(Actor + 0x4a, 37, 2);
    put(Actor + 0x60, 0, 2);
    put(Actor + 0x64, 0, 2);
    put(Actor + 0x20, Nested, 4);
    put(Nested + 0xd, 1, 1);
    put(Actor + 0xa2, 0x2468, 2);
    put(0x800fdc48, Ball, 4);
    put(Ball + 0x10, 49u << 8, 4);
    put(Ball + 0x18, 0, 2);
  }
  void put(std::uint32_t a, std::uint32_t v, unsigned w) {
    auto n = a - Ram;
    for (unsigned i = 0; i < w; ++i)
      bytes[n + i] = static_cast<std::uint8_t>(v >> (8 * i));
  }
  std::uint32_t get(std::uint32_t a, unsigned w) const {
    auto n = a - Ram;
    std::uint32_t v = 0;
    for (unsigned i = 0; i < w; ++i)
      v |= std::uint32_t(bytes[n + i]) << (8 * i);
    return v;
  }
  static int child(void *u, const Nba97GameTextMemory *,
                   const Nba97GameActorResumeEvent *e,
                   Nba97GameActorResumeMachine *m) {
    auto &c = *static_cast<Composition *>(u);
    c.children.push_back(*e);
    if (c.refuse == c.children.size())
      return 0;
    if (e->kind == NBA97_GAME_ACTOR_RESUME_CHILD_800582CC)
      m->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0x13572468, 15};
    return 1;
  }
  int run() { return nba97_game_period_expiry(&expiry, &expiry_progress); }
};
void natural_z_child() {
  Composition c;
  check(c.run() == NBA97_TEXT_COMPLETE && c.expiry_progress.completed);
  check(c.actor.result == NBA97_TEXT_COMPLETE && c.actor.progress.completed &&
        c.actor.invocations == 1);
  check(c.actor.event.pc == 0x800676cc &&
        c.actor.event.delay_slot_pc == 0x800676d0 &&
        c.actor.event.entry == 0x800582dc && c.actor.event.argument_count == 2);
  check(c.actor.progress.frame_stack_pointer == 0x800fefc8 &&
        c.actor.progress.restored_return_address.word == 0x800676d4 &&
        c.actor.progress.machine.hi.word == 0x12345678 &&
        c.actor.progress.machine.lo.word == 0x9abcdef0);
  check(c.children.size() == 3 && c.children[0].entry == 0x80056ffc &&
        c.children[1].entry == 0x8005703c && c.children[2].entry == 0x800582cc);
  check(c.get(Actor + 0x1a, 1) == 1 && c.get(Actor + 0x4e, 2) == 0 &&
        c.get(Actor + 0x9a, 2) == 3 && c.get(Actor + 0xb8, 2) == 47 &&
        c.get(Actor + 0xa6, 2) == 0x2468);
  check(
      c.expiry_progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
      0x80068d74);
}
void failure_and_validation() {
  Composition refused;
  refused.refuse = 2;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.actor.result == NBA97_TEXT_IO_REFUSED &&
        refused.actor.progress.callbacks_completed == 1 &&
        refused.expiry_progress.stopped_pc == 0x800676cc);
  Composition limited;
  limited.actor.operation_budget = 0;
  check(limited.run() == NBA97_TEXT_IO_REFUSED &&
        limited.actor.result == NBA97_TEXT_LIMIT &&
        limited.actor.progress.stopped_pc == 0x800582e0);
  Composition c;
  Nba97GamePeriodExpiryEvent wrong{};
  check(!nba97_game_actor_resume_from_period_expiry(
            &c.actor, &c.expiry.memory, &wrong, &c.expiry.machine) &&
        c.actor.result == NBA97_TEXT_ARGUMENT);
}
} // namespace
int main() {
  natural_z_child();
  failure_and_validation();
  std::printf("game actor resume integration: %u checks passed\n", checks);
}
