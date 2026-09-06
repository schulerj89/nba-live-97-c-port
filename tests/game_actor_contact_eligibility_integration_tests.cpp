#include "game_actor_contact_eligibility_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void check(bool condition, const char *expression, int line) {
  if (condition)
    return;
  std::fprintf(stderr, "check failed at line %d: %s\n", line, expression);
  std::exit(1);
}
#define CHECK(expression) check((expression), #expression, __LINE__)

constexpr uint32_t kBase = UINT32_C(0x80000000);
constexpr uint32_t kFirst = UINT32_C(0x80010000);
constexpr uint32_t kSecond = UINT32_C(0x80010200);
constexpr uint32_t kStack = UINT32_C(0x801ff000);

void put8(std::vector<uint8_t> &ram, uint32_t address, uint8_t value) {
  ram[address - kBase] = value;
}
void put16(std::vector<uint8_t> &ram, uint32_t address, uint16_t value) {
  put8(ram, address, static_cast<uint8_t>(value));
  put8(ram, address + 1u, static_cast<uint8_t>(value >> 8));
}
void put32(std::vector<uint8_t> &ram, uint32_t address, uint32_t value) {
  for (unsigned i = 0; i != 4; ++i)
    put8(ram, address + i, static_cast<uint8_t>(value >> (8u * i)));
}
void set_word(Nba97GameMatchClocksWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

struct Leaf {
  size_t calls = 0;
  bool refuse = false;
};
int leaf(void *opaque, const Nba97GameTextMemory *,
         const Nba97GameActorContactEligibilityEvent *event,
         Nba97GameActorContactEligibilityMachine *machine) {
  Leaf &state = *static_cast<Leaf *>(opaque);
  ++state.calls;
  CHECK(event->argument_count == 2u);
  set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0],
           event->kind == NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_GEOMETRY_8007066C
               ? 0u
               : UINT32_C(0x123456cd));
  return state.refuse ? 0 : 1;
}

struct Fixture {
  std::vector<uint8_t> ram = std::vector<uint8_t>(UINT32_C(0x200000), 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(UINT32_C(0x200000), 1u);
  Nba97GameTextRegion region{};
  Nba97GameActorContactGateContext gate{};
  Nba97GameActorContactGateProgress gate_progress{};
  Nba97GameActorContactEligibilityBinding binding{};
  Nba97GameActorContactEligibilityGeometryBinding geometry{};
  Leaf leaf_state{};

  Fixture() {
    region.base = kBase;
    region.data = ram.data();
    region.known = known.data();
    region.size = ram.size();
    gate.memory.region = &region;
    gate.memory.count = 1u;
    gate.operation_budget = 1000u;
    for (unsigned i = 0; i != 32; ++i)
      set_word(gate.machine.registers.gpr[i], UINT32_C(0x20000000) + i);
    set_word(gate.machine.registers.gpr[0], 0u);
    set_word(gate.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0], kFirst);
    set_word(gate.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1], kSecond);
    set_word(gate.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP], kStack);
    set_word(gate.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x81234567));
    set_word(gate.machine.hi, UINT32_C(0x11112222));
    set_word(gate.machine.lo, UINT32_C(0x33334444));
    put32(ram, kFirst + 8u, 0u);
    put32(ram, kSecond + 8u, 0u);
    put16(ram, UINT32_C(0x800fe8cc), 0u);
    put16(ram, UINT32_C(0x800fe8ca), UINT16_C(0x7fff));
    put16(ram, UINT32_C(0x800fdb90), 0u);
    put16(ram, UINT32_C(0x800fdbcc), 0u);
    put32(ram, kFirst, 100u);
    put32(ram, kSecond, 200u);
    put8(ram, kFirst + 0xd9u, 1u);
    put8(ram, kSecond + 0xd9u, 2u);
    put32(ram, kFirst + 0xcu, 0u);
    put32(ram, kSecond + 0xcu, 0u);
    nba97_game_actor_contact_eligibility_binding_init(&binding, 1000u, leaf,
                                                      &leaf_state, nullptr, 0u);
    gate.io = nba97_game_actor_contact_eligibility_from_actor_contact_gate;
    gate.user = &binding;
  }

  void compose_geometry() {
    nba97_game_actor_contact_eligibility_geometry_binding_init(&geometry, leaf,
                                                               &leaf_state);
    binding.io = nba97_game_actor_contact_eligibility_geometry_child;
    binding.user = &geometry;
  }
};

void test_natural_success_and_gate_rejection() {
  Fixture f;
  CHECK(nba97_game_actor_contact_gate(&f.gate, &f.gate_progress) ==
        NBA97_TEXT_COMPLETE);
  CHECK(f.gate_progress.completed == 1u &&
        f.gate_progress.callbacks_completed == 1u);
  CHECK(f.binding.invocations == 1u && f.binding.result == NBA97_TEXT_COMPLETE);
  CHECK(f.binding.progress.completed == 1u && f.leaf_state.calls == 2u);
  CHECK(f.binding.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .word == UINT32_C(0xcd));
  CHECK(f.gate_progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
        1u);
  CHECK(f.gate_progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        UINT32_C(0x81234567));

  Fixture rejected;
  put32(rejected.ram, kSecond + 8u, 4097u);
  CHECK(nba97_game_actor_contact_gate(
            &rejected.gate, &rejected.gate_progress) == NBA97_TEXT_COMPLETE);
  CHECK(rejected.binding.invocations == 0u && rejected.leaf_state.calls == 0u);
  CHECK(rejected.gate_progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .word == 0u);
}

void test_nested_failure_prefix() {
  Fixture f;
  f.leaf_state.refuse = true;
  CHECK(nba97_game_actor_contact_gate(&f.gate, &f.gate_progress) ==
        NBA97_TEXT_IO_REFUSED);
  CHECK(f.binding.result == NBA97_TEXT_IO_REFUSED);
  CHECK(f.binding.progress.stopped_pc == UINT32_C(0x8005fa18));
  CHECK(f.binding.progress.operations > 0u &&
        f.binding.progress.callbacks_completed == 0u);
  CHECK(std::memcmp(&f.gate_progress.machine, &f.binding.progress.machine,
                    sizeof(f.binding.progress.machine)) == 0);
}

void test_adapter_rejects_malformed_calls_without_clobber() {
  Fixture f;
  Nba97GameActorContactGateEvent event{};
  event.pc = UINT32_C(0x8005facc);
  event.delay_slot_pc = UINT32_C(0x8005fad0);
  event.entry = UINT32_C(0x8005f948);
  event.kind = NBA97_GAME_ACTOR_CONTACT_GATE_CHILD_8005F948;
  event.argument_count = 3u;
  Nba97GameActorContactGateMachine machine = f.gate.machine;
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
           UINT32_C(0x8005fad4));
  Nba97GameActorContactGateMachine before = machine;

  event.entry ^= 4u;
  CHECK(nba97_game_actor_contact_eligibility_from_actor_contact_gate(
            &f.binding, &f.gate.memory, &event, &machine) == 0);
  CHECK(std::memcmp(&machine, &before, sizeof(machine)) == 0);
  event.entry ^= 4u;

  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ^= 4u;
  before = machine;
  CHECK(nba97_game_actor_contact_eligibility_from_actor_contact_gate(
            &f.binding, &f.gate.memory, &event, &machine) == 0);
  CHECK(std::memcmp(&machine, &before, sizeof(machine)) == 0);

  machine = f.gate.machine;
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
           UINT32_C(0x8005fad4), 7u);
  before = machine;
  CHECK(nba97_game_actor_contact_eligibility_from_actor_contact_gate(
            &f.binding, &f.gate.memory, &event, &machine) == 0);
  CHECK(std::memcmp(&machine, &before, sizeof(machine)) == 0);

  machine = f.gate.machine;
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
           UINT32_C(0x8005fad4));
  machine.registers.gpr[9].known_mask = 16u;
  before = machine;
  CHECK(nba97_game_actor_contact_eligibility_from_actor_contact_gate(
            &f.binding, &f.gate.memory, &event, &machine) == 0);
  CHECK(std::memcmp(&machine, &before, sizeof(machine)) == 0);

  Nba97GameTextMemory invalid{};
  invalid.count = 1u;
  machine = f.gate.machine;
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
           UINT32_C(0x8005fad4));
  before = machine;
  CHECK(nba97_game_actor_contact_eligibility_from_actor_contact_gate(
            &f.binding, &invalid, &event, &machine) == 0);
  CHECK(std::memcmp(&machine, &before, sizeof(machine)) == 0);
}

void test_existing_geometry_owner_mapping() {
  struct Case {
    uint32_t a0, a1, output_a0, output_a1, output_v0;
  } cases[] = {{3u, 4u, 3u, 4u, 5u},
               {UINT32_C(0xfffffffd), 4u, 3u, 4u, 5u},
               {UINT32_C(0x80000000), 1u, UINT32_C(0x80000000), 1u,
                UINT32_C(0xe0000001)}};
  for (const Case &item : cases) {
    Fixture f;
    f.compose_geometry();
    Nba97GameActorContactEligibilityEvent event{};
    event.pc = UINT32_C(0x8005fa18);
    event.delay_slot_pc = UINT32_C(0x8005fa1c);
    event.entry = UINT32_C(0x8007066c);
    event.kind = NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_GEOMETRY_8007066C;
    event.argument_count = 2u;
    Nba97GameActorContactEligibilityMachine machine = f.gate.machine;
    set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x8005fa20));
    set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0], item.a0);
    set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1], item.a1);
    const Nba97GameActorContactEligibilityMachine before = machine;
    CHECK(nba97_game_actor_contact_eligibility_geometry_child(
              &f.geometry, &f.gate.memory, &event, &machine) == 1);
    CHECK(f.geometry.result == NBA97_TEXT_COMPLETE);
    CHECK(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
          item.output_a0);
    CHECK(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
          item.output_a1);
    CHECK(machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
          item.output_v0);
    for (unsigned reg = 0; reg != 32; ++reg) {
      if (reg == NBA97_MATCH_INITIALIZE_A0 ||
          reg == NBA97_MATCH_INITIALIZE_A1 || reg == NBA97_MATCH_INITIALIZE_V0)
        continue;
      CHECK(std::memcmp(&machine.registers.gpr[reg], &before.registers.gpr[reg],
                        sizeof(machine.registers.gpr[reg])) == 0);
    }
    CHECK(std::memcmp(&machine.hi, &before.hi, sizeof(machine.hi)) == 0);
    CHECK(std::memcmp(&machine.lo, &before.lo, sizeof(machine.lo)) == 0);
  }

  Fixture unknown;
  unknown.compose_geometry();
  Nba97GameActorContactEligibilityEvent event{};
  event.pc = UINT32_C(0x8005fa70);
  event.delay_slot_pc = UINT32_C(0x8005fa74);
  event.entry = UINT32_C(0x8007066c);
  event.kind = NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_GEOMETRY_8007066C;
  event.argument_count = 2u;
  Nba97GameActorContactEligibilityMachine machine = unknown.gate.machine;
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
           UINT32_C(0x8005fa78));
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask = 14u;
  const Nba97GameActorContactEligibilityMachine before = machine;
  CHECK(nba97_game_actor_contact_eligibility_geometry_child(
            &unknown.geometry, &unknown.gate.memory, &event, &machine) == 0);
  CHECK(unknown.geometry.result == NBA97_TEXT_ARGUMENT);
  CHECK(std::memcmp(&machine, &before, sizeof(machine)) == 0);
}

void test_natural_composed_geometry() {
  Fixture f;
  f.compose_geometry();
  CHECK(nba97_game_actor_contact_gate(&f.gate, &f.gate_progress) ==
        NBA97_TEXT_COMPLETE);
  CHECK(f.binding.result == NBA97_TEXT_COMPLETE);
  CHECK(f.geometry.geometry_invocations == 1u);
  CHECK(f.geometry.fallback_invocations == 1u);
  CHECK(f.leaf_state.calls == 1u);
  CHECK(f.binding.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .word == UINT32_C(0xcd));

  Fixture same;
  same.compose_geometry();
  put8(same.ram, kSecond + 0xd9u, 1u);
  put32(same.ram, kSecond + 8u, 3u << 8);
  put32(same.ram, kSecond + 0xcu, 4u << 8);
  CHECK(nba97_game_actor_contact_gate(&same.gate, &same.gate_progress) ==
        NBA97_TEXT_COMPLETE);
  CHECK(same.geometry.geometry_invocations == 1u);
  CHECK(same.geometry.fallback_invocations == 1u);
  CHECK(same.binding.progress.call_count[
            NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_SAME_TEAM_8005F328] == 1u);
  CHECK(same.binding.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .word == UINT32_C(0xcd));
  CHECK(same.gate_progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .word == 1u);
}

} // namespace

int main() {
  test_natural_success_and_gate_rejection();
  test_nested_failure_prefix();
  test_adapter_rejects_malformed_calls_without_clobber();
  test_existing_geometry_owner_mapping();
  test_natural_composed_geometry();
  std::puts("game_actor_contact_eligibility_integration_tests: 14 natural and "
            "adapter cases passed");
  return 0;
}
