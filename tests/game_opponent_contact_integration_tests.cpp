#include "game_actor_contact_eligibility_adapter.h"
#include "game_opponent_contact_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "opponent contact integration check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t First = 0x80010000u;
constexpr std::uint32_t Second = 0x80010200u;

struct Composition {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000u, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000u, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97GameActorContactEligibilityContext parent{};
  Nba97GameActorContactEligibilityProgress parent_progress{};
  Nba97GameActorContactEligibilityGeometryBinding geometry{};
  Nba97GameOpponentContactBinding opponent{};
  Nba97GameOpponentContactEvent leaf_event{};
  Nba97GameOpponentContactMachine leaf_machine{};
  unsigned leaf_calls{};
  bool refuse{};

  Composition() {
    parent.memory = {&region, 1};
    parent.operation_budget = 1000;
    for (unsigned i = 0; i < 32; ++i)
      parent.machine.registers.gpr[i] = {
          0x20000000u + i, static_cast<std::uint8_t>((i % 15u) + 1u)};
    parent.machine.registers.gpr[0] = {0, 15};
    parent.machine.registers.gpr[4] = {First, 15};
    parent.machine.registers.gpr[5] = {Second, 15};
    parent.machine.registers.gpr[6] = {0, 15};
    parent.machine.registers.gpr[29] = {0x801ff000u, 15};
    parent.machine.registers.gpr[31] = {0x81234568u, 15};
    parent.machine.hi = {0x11112222u, 7};
    parent.machine.lo = {0x33334444u, 11};
    put(0x800fe8ccu, 0, 2);
    put(0x800fe8cau, 0x7fff, 2);
    put(0x800fdb90u, 0, 2);
    put(0x800fdbccu, 0, 2);
    put(0x80021d8au, 0, 1);
    put(First, 100, 4);
    put(Second, 200, 4);
    put(First + 8u, 0, 4);
    put(Second + 8u, 0, 4);
    put(First + 0xcu, 0, 4);
    put(Second + 0xcu, 0, 4);
    put(First + 0xc2u, 0, 2);
    put(Second + 0xc2u, 0, 2);
    put(First + 0xd9u, 1, 1);
    put(Second + 0xd9u, 2, 1);
    put(First + 0xdau, 0, 1);
    put(Second + 0xdau, 0, 1);
    nba97_game_opponent_contact_binding_init(&opponent, 1000, leaf, this,
                                             nullptr, 0);
    nba97_game_actor_contact_eligibility_geometry_binding_init(
        &geometry, nba97_game_opponent_contact_from_actor_contact_eligibility,
        &opponent);
    parent.io = nba97_game_actor_contact_eligibility_geometry_child;
    parent.user = &geometry;
  }
  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }
  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    auto at = offset(address);
    for (unsigned i = 0; i < width; ++i) {
      bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
      known[at + i] = 1;
    }
  }
  static int leaf(void *opaque, const Nba97GameTextMemory *,
                  const Nba97GameOpponentContactEvent *event,
                  Nba97GameOpponentContactMachine *machine) {
    auto &c = *static_cast<Composition *>(opaque);
    ++c.leaf_calls;
    c.leaf_event = *event;
    c.leaf_machine = *machine;
    machine->registers.gpr[2] = {0x123456cdu, 15};
    return c.refuse ? 0 : 1;
  }
  int run() {
    return nba97_game_actor_contact_eligibility(&parent, &parent_progress);
  }
};

void actual_parent_call_and_ordering() {
  Composition c;
  check(c.run() == NBA97_TEXT_COMPLETE && c.parent_progress.completed);
  check(c.geometry.geometry_invocations == 1 &&
        c.geometry.fallback_invocations == 1 && c.opponent.invocations == 1 &&
        c.opponent.result == NBA97_TEXT_COMPLETE &&
        c.opponent.progress.completed && c.leaf_calls == 1);
  check(c.leaf_event.pc == 0x8005f92cu &&
        c.leaf_event.delay_slot_pc == 0x8005f930u &&
        c.leaf_event.entry == 0x8005f3bcu && c.leaf_event.argument_count == 2);
  check(c.leaf_machine.registers.gpr[4].word == First &&
        c.leaf_machine.registers.gpr[5].word == Second &&
        c.opponent.progress.restored_return_address.word == 0x8005fa34u);
  check(c.parent_progress.machine.registers.gpr[2].word == 0xcdu &&
        c.parent_progress.machine.hi.word == 0x11112222u &&
        c.parent_progress.machine.lo.known_mask == 11);

  Composition swapped;
  swapped.put(Second + 0xdau, 1, 1);
  swapped.put(First + 0xdau, 1, 1);
  swapped.put(0x800fdbccu, 7, 2);
  check(swapped.run() == NBA97_TEXT_COMPLETE && swapped.leaf_calls == 1 &&
        swapped.leaf_machine.registers.gpr[4].word == Second &&
        swapped.leaf_machine.registers.gpr[5].word == First);
}

void nested_failure_and_adapter_guards() {
  Composition refused;
  refused.refuse = true;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.opponent.result == NBA97_TEXT_IO_REFUSED &&
        refused.opponent.progress.stopped_pc == 0x8005f92cu &&
        refused.parent_progress.stopped_pc == 0x8005fa2cu);

  Composition guards;
  Nba97GameActorContactEligibilityEvent event{};
  event.pc = 0x8005fa2cu;
  event.delay_slot_pc = 0x8005fa30u;
  event.entry = 0x8005f888u;
  event.kind = NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_OTHER_TEAM_8005F888;
  event.argument_count = 2;
  auto machine = guards.parent.machine;
  machine.registers.gpr[31] = {0x8005fa34u, 15};
  const auto before = machine;
  event.pc ^= 4u;
  check(!nba97_game_opponent_contact_from_actor_contact_eligibility(
            &guards.opponent, &guards.parent.memory, &event, &machine) &&
        std::memcmp(&machine, &before, sizeof machine) == 0);
  event.pc ^= 4u;
  event.entry ^= 4u;
  check(!nba97_game_opponent_contact_from_actor_contact_eligibility(
      &guards.opponent, &guards.parent.memory, &event, &machine));
  event.entry ^= 4u;
  event.argument_count = 1;
  check(!nba97_game_opponent_contact_from_actor_contact_eligibility(
      &guards.opponent, &guards.parent.memory, &event, &machine));
  event.argument_count = 2;
  machine.registers.gpr[31].known_mask = 7;
  check(!nba97_game_opponent_contact_from_actor_contact_eligibility(
      &guards.opponent, &guards.parent.memory, &event, &machine));
  check(!nba97_game_opponent_contact_from_actor_contact_eligibility(
      nullptr, &guards.parent.memory, &event, &machine));
}
} // namespace

int main() {
  actual_parent_call_and_ordering();
  nested_failure_and_adapter_guards();
  std::printf("game opponent contact integration: %u checks\n", checks);
}
