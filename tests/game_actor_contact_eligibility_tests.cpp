#include "recovered/game_actor_contact_eligibility.h"

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
void set_word(Nba97GameActorContactEligibilityWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

struct Calls {
  std::vector<Nba97GameActorContactEligibilityEvent> events;
  std::vector<Nba97GameActorContactEligibilityMachine> machines;
  uint32_t geometry_return = 0;
  uint8_t geometry_known = 15u;
  uint32_t action_return = UINT32_C(0x123456ab);
  bool refuse = false;
  bool malformed = false;
  bool mutate_geometry = false;
  bool mutate_action = false;
  bool mutate_memory = false;
  bool unknown_s0 = false;
  uint32_t alternate_sp = UINT32_C(0x801fe000);
};

int child(void *opaque, const Nba97GameTextMemory *memory,
          const Nba97GameActorContactEligibilityEvent *event,
          Nba97GameActorContactEligibilityMachine *machine) {
  Calls &calls = *static_cast<Calls *>(opaque);
  calls.events.push_back(*event);
  calls.machines.push_back(*machine);
  CHECK(event->argument_count == 2u);
  CHECK(machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 15u);
  CHECK(machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        event->pc + 8u);
  if (event->kind != NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_GEOMETRY_8007066C) {
    CHECK(machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
          machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0].word);
    CHECK(machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
          machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1].word);
  }
  if (calls.refuse)
    return 0;
  if (event->kind == NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_GEOMETRY_8007066C) {
    set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0],
             calls.geometry_return, calls.geometry_known);
    if (calls.mutate_geometry) {
      set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0],
               UINT32_C(0x80014000));
      set_word(machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1],
               UINT32_C(0x80014200));
      set_word(machine->hi, UINT32_C(0x11112222));
      set_word(machine->lo, UINT32_C(0x33334444));
    }
    if (calls.unknown_s0)
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0].known_mask = 0u;
    if (calls.mutate_memory)
      memory->region[0].data[UINT32_C(0x18000)] = UINT8_C(0x5a);
  } else {
    set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0],
             calls.action_return);
    if (calls.mutate_action) {
      set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP],
               calls.alternate_sp);
      set_word(machine->hi, UINT32_C(0xa1a2a3a4));
      set_word(machine->lo, UINT32_C(0xb1b2b3b4));
    }
  }
  if (calls.malformed)
    machine->registers.gpr[7].known_mask = 16u;
  return 1;
}

struct Fixture {
  std::vector<uint8_t> ram = std::vector<uint8_t>(UINT32_C(0x200000), 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(UINT32_C(0x200000), 1u);
  Nba97GameTextRegion region{};
  Nba97GameActorContactEligibilityContext context{};
  Nba97GameActorContactEligibilityProgress progress{};
  Nba97GameActorContactEligibilityAccess journal[64]{};
  Calls calls{};

  Fixture() {
    region.base = kBase;
    region.data = ram.data();
    region.known = known.data();
    region.size = ram.size();
    context.memory.region = &region;
    context.memory.count = 1u;
    context.operation_budget = 1000u;
    context.io = child;
    context.user = &calls;
    context.access_journal = journal;
    context.access_journal_capacity = 64u;
    for (unsigned i = 0; i != 32; ++i)
      set_word(context.machine.registers.gpr[i], UINT32_C(0x10000000) + i);
    set_word(context.machine.registers.gpr[0], 0u);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0], kFirst);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1], kSecond);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2], 0u);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP], kStack);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x81234567));
    set_word(context.machine.hi, UINT32_C(0x01020304));
    set_word(context.machine.lo, UINT32_C(0x05060708));
    put16(ram, UINT32_C(0x800fe8cc), 0u);
    put16(ram, UINT32_C(0x800fe8ca), UINT16_C(0x7fff));
    put16(ram, UINT32_C(0x800fdb90), 0u);
    put16(ram, UINT32_C(0x800fdbcc), 0u);
    put32(ram, kFirst, 100u);
    put32(ram, kSecond, 200u);
    put8(ram, kFirst + 0x1au, 0u);
    put8(ram, kSecond + 0x1au, 0u);
    put8(ram, kFirst + 0xd9u, 1u);
    put8(ram, kSecond + 0xd9u, 2u);
    put32(ram, kFirst + 0xcu, 0u);
    put32(ram, kSecond + 0xcu, 0u);
  }
  int run() {
    calls.events.clear();
    calls.machines.clear();
    return nba97_game_actor_contact_eligibility(&context, &progress);
  }
};

void expect_complete(Fixture &f, uint32_t result, size_t calls) {
  CHECK(f.run() == NBA97_TEXT_COMPLETE);
  CHECK(f.progress.completed == 1u);
  CHECK(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
        result);
  CHECK(f.calls.events.size() == calls);
}

void test_team_windows_and_calls() {
  const int unequal_y[] = {-17, -16, 16, 17};
  for (int y : unequal_y) {
    Fixture f;
    put32(f.ram, kSecond + 0xcu, static_cast<uint32_t>(y * 256));
    expect_complete(f, (y >= -16 && y <= 16) ? UINT32_C(0xab) : 0u,
                    (y >= -16 && y <= 16) ? 2u : 0u);
    if (!f.calls.events.empty()) {
      CHECK(f.calls.events[0].pc == UINT32_C(0x8005fa18));
      CHECK(f.calls.events[0].delay_slot_pc == UINT32_C(0x8005fa1c));
      CHECK(f.calls.events[1].pc == UINT32_C(0x8005fa2c));
      CHECK(f.calls.events[1].entry == UINT32_C(0x8005f888));
      CHECK(f.calls.machines[0].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            0u);
      CHECK(f.calls.machines[0].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            static_cast<uint32_t>(y));
    }
  }
  const int equal_y[] = {-9, -8, 8, 9};
  for (int y : equal_y) {
    Fixture f;
    put8(f.ram, kSecond + 0xd9u, 1u);
    put32(f.ram, kSecond + 0xcu, static_cast<uint32_t>(y * 256));
    expect_complete(f, (y >= -8 && y <= 8) ? UINT32_C(0xab) : 0u,
                    (y >= -8 && y <= 8) ? 2u : 0u);
    if (!f.calls.events.empty()) {
      CHECK(f.calls.events[0].pc == UINT32_C(0x8005fa70));
      CHECK(f.calls.events[1].pc == UINT32_C(0x8005fa84));
      CHECK(f.calls.events[1].entry == UINT32_C(0x8005f328));
      CHECK(f.calls.machines[0].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            static_cast<uint32_t>(y));
    }
  }
}

void test_same_team_x_and_geometry_signedness() {
  const uint32_t xs[] = {UINT32_C(0x80000000), 8u, 9u};
  const size_t expected_calls[] = {2u, 2u, 0u};
  for (unsigned i = 0; i != 3; ++i) {
    Fixture f;
    put8(f.ram, kSecond + 0xd9u, 1u);
    set_word(f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2], xs[i]);
    expect_complete(f, expected_calls[i] ? UINT32_C(0xab) : 0u,
                    expected_calls[i]);
    if (expected_calls[i])
      CHECK(f.calls.machines[0].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            xs[i]);
  }
  const uint32_t unequal_results[] = {UINT32_C(0xffffffff), 16u, 17u};
  const size_t unequal_calls[] = {2u, 2u, 1u};
  for (unsigned i = 0; i != 3; ++i) {
    Fixture f;
    f.calls.geometry_return = unequal_results[i];
    expect_complete(f, unequal_calls[i] == 2u ? UINT32_C(0xab) : 0u,
                    unequal_calls[i]);
  }
  const uint32_t equal_results[] = {UINT32_C(0xffffffff), 8u, 9u};
  const size_t equal_calls[] = {2u, 2u, 1u};
  for (unsigned i = 0; i != 3; ++i) {
    Fixture f;
    put8(f.ram, kSecond + 0xd9u, 1u);
    f.calls.geometry_return = equal_results[i];
    expect_complete(f, equal_calls[i] == 2u ? UINT32_C(0xab) : 0u,
                    equal_calls[i]);
  }
}

void test_identity_and_asymmetric_state_gates() {
  {
    Fixture f;
    put16(f.ram, UINT32_C(0x800fe8cc), 1u);
    put16(f.ram, UINT32_C(0x800fe8ca), 100u);
    expect_complete(f, 0u, 0u);
  }
  {
    Fixture f;
    put16(f.ram, UINT32_C(0x800fe8cc), 1u);
    put16(f.ram, UINT32_C(0x800fe8ca), UINT16_C(0xffff));
    put32(f.ram, kFirst, UINT32_C(0x0000ffff));
    expect_complete(f, UINT32_C(0xab), 2u);
  }
  {
    Fixture f;
    put16(f.ram, UINT32_C(0x800fe8cc), 1u);
    put16(f.ram, UINT32_C(0x800fe8ca), UINT16_C(0xffff));
    put32(f.ram, kFirst, UINT32_C(0xffffffff));
    expect_complete(f, 0u, 0u);
  }
  {
    Fixture f;
    put16(f.ram, UINT32_C(0x800fdb90), 0x82u);
    put16(f.ram, UINT32_C(0x800fdbcc), 100u);
    expect_complete(f, 0u, 0u);
  }
  {
    Fixture f;
    put16(f.ram, UINT32_C(0x800fdb90), 0x82u);
    put16(f.ram, UINT32_C(0x800fdbcc), 0xffffu);
    put8(f.ram, kFirst + 0x1au, 3u);
    expect_complete(f, 0u, 0u);
  }
  {
    Fixture f;
    put16(f.ram, UINT32_C(0x800fdb90), 0x82u);
    put16(f.ram, UINT32_C(0x800fdbcc), 0xffffu);
    put8(f.ram, kSecond + 0x1au, 2u);
    expect_complete(f, UINT32_C(0xab), 2u);
  }
  {
    Fixture f;
    put16(f.ram, UINT32_C(0x800fdb90), 0x82u);
    put16(f.ram, UINT32_C(0x800fdbcc), 0xffffu);
    put8(f.ram, kSecond + 0x1au, 3u);
    put32(f.ram, kSecond, UINT32_C(0xffffffff));
    expect_complete(f, 0u, 0u);
  }
}

void test_fractional_wrap_and_action_mask() {
  {
    Fixture f;
    put32(f.ram, kSecond + 0xcu, UINT32_C(0xffffffff));
    f.calls.action_return = UINT32_C(0xffffffff);
    expect_complete(f, 255u, 2u);
  }
  {
    Fixture f;
    f.calls.action_return = UINT32_C(0x100);
    expect_complete(f, 0u, 2u);
  }
  {
    Fixture f;
    put32(f.ram, kFirst + 0xcu, UINT32_C(0x7fffff00));
    put32(f.ram, kSecond + 0xcu, UINT32_C(0x80000000));
    expect_complete(f, UINT32_C(0xab), 2u);
  }
}

void test_live_machine_and_failures() {
  Fixture f;
  f.calls.mutate_geometry = true;
  f.calls.mutate_action = true;
  f.calls.mutate_memory = true;
  put32(f.ram, f.calls.alternate_sp + 0x18u, UINT32_C(0x87654321));
  put32(f.ram, f.calls.alternate_sp + 0x14u, UINT32_C(0x11111111));
  put32(f.ram, f.calls.alternate_sp + 0x10u, UINT32_C(0x22222222));
  expect_complete(f, UINT32_C(0xab), 2u);
  CHECK(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        UINT32_C(0x87654321));
  CHECK(f.progress.machine.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1].word ==
        UINT32_C(0x11111111));
  CHECK(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
        UINT32_C(0x22222222));
  CHECK(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        f.calls.alternate_sp + 0x20u);
  CHECK(f.progress.machine.hi.word == UINT32_C(0xa1a2a3a4));
  CHECK(f.progress.machine.lo.word == UINT32_C(0xb1b2b3b4));
  CHECK(f.ram[UINT32_C(0x18000)] == UINT8_C(0x5a));
  CHECK(f.calls.events[1].kind ==
        NBA97_GAME_ACTOR_CONTACT_ELIGIBILITY_OTHER_TEAM_8005F888);

  {
    Fixture x;
    x.calls.refuse = true;
    CHECK(x.run() == NBA97_TEXT_IO_REFUSED);
    CHECK(x.progress.callbacks_completed == 0u);
    CHECK(x.progress.stopped_pc == UINT32_C(0x8005fa18));
  }
  {
    Fixture x;
    x.calls.malformed = true;
    CHECK(x.run() == NBA97_TEXT_ARGUMENT);
    CHECK(x.progress.callbacks_completed == 0u);
    CHECK(x.progress.machine.registers.gpr[7].known_mask == 16u);
  }
  {
    Fixture x;
    x.context.machine.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1].known_mask =
        14u;
    x.region.known = nullptr;
    CHECK(x.run() == NBA97_TEXT_ARGUMENT);
    CHECK(x.progress.stopped_pc == UINT32_C(0x8005f94c));
  }
  {
    Fixture x;
    x.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word |= 1u;
    CHECK(x.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  }
  {
    Fixture x;
    x.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 7u;
    CHECK(x.run() == NBA97_TEXT_UNKNOWN);
  }
  {
    Fixture x;
    x.known[UINT32_C(0xfe8cc)] = 0u;
    CHECK(x.run() == NBA97_TEXT_UNKNOWN);
    CHECK(x.progress.stopped_pc == UINT32_C(0x8005f970));
  }
  {
    Fixture x;
    x.known[UINT32_C(0xfe8cc)] = 0u;
    const uint32_t frame = kStack - 0x20u;
    CHECK(x.run() == NBA97_TEXT_UNKNOWN);
    CHECK(x.ram[frame + 0x18u - kBase] == UINT8_C(0x67));
  }
  {
    Fixture x;
    put8(x.ram, kSecond + 0xd9u, 1u);
    x.known[kSecond + 0xcu + 1u - kBase] = 0u;
    set_word(x.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2], 9u);
    CHECK(x.run() == NBA97_TEXT_UNKNOWN);
    CHECK(x.progress.stopped_pc == UINT32_C(0x8005fa60));
    CHECK(x.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
          0u);
  }
  {
    Fixture x;
    x.calls.geometry_return = 17u;
    x.calls.mutate_geometry = true;
    x.calls.unknown_s0 = true;
    CHECK(x.run() == NBA97_TEXT_COMPLETE);
    CHECK(x.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
              .known_mask == 0u);
  }
  {
    Fixture x;
    x.calls.geometry_return = 0u;
    x.calls.geometry_known = 0u;
    CHECK(x.run() == NBA97_TEXT_UNKNOWN);
    CHECK(x.progress.stopped_pc == UINT32_C(0x8005fa24));
    CHECK(x.progress.callbacks_completed == 1u);
    CHECK(x.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
          kFirst);
  }
  {
    Fixture x;
    put8(x.ram, kSecond + 0xd9u, 1u);
    x.calls.geometry_return = 0u;
    x.calls.geometry_known = 0u;
    CHECK(x.run() == NBA97_TEXT_UNKNOWN);
    CHECK(x.progress.stopped_pc == UINT32_C(0x8005fa7c));
    CHECK(x.progress.callbacks_completed == 1u);
    CHECK(x.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
          kFirst);
  }
  {
    Fixture x;
    put8(x.ram, kSecond + 0xd9u, 1u);
    x.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2].known_mask = 0u;
    CHECK(x.run() == NBA97_TEXT_UNKNOWN);
    CHECK(x.progress.stopped_pc == UINT32_C(0x8005fa68));
    CHECK(x.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
          0u);
    CHECK(x.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
              .known_mask == 15u);
  }
  {
    Fixture x;
    x.context.memory.count = 0u;
    CHECK(x.run() == NBA97_TEXT_RESOURCE);
  }
  {
    Fixture x;
    set_word(x.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP], 0x10u);
    CHECK(x.run() == NBA97_TEXT_RESOURCE);
    CHECK(x.progress.stopped_address == UINT32_C(0x00000004));
  }
  {
    Fixture x;
    uint8_t low[16] = {};
    uint8_t low_known[16];
    std::memset(low_known, 1, sizeof(low_known));
    Nba97GameTextRegion regions[2] = {x.region, {0u, low, low_known, 16u}};
    x.context.memory.region = regions;
    x.context.memory.count = 2u;
    set_word(x.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP], 0x10u);
    expect_complete(x, UINT32_C(0xab), 2u);
    CHECK(x.progress.frame_stack_pointer == UINT32_C(0xfffffff0));
    CHECK(x.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
          0x10u);
    CHECK(x.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
          UINT32_C(0x81234567));
  }
  {
    Fixture x;
    set_word(x.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP],
             kFirst + 0x20u);
    expect_complete(x, UINT32_C(0xab), 2u);
    CHECK(x.progress.frame_stack_pointer == kFirst);
  }
  {
    Fixture x;
    x.calls.action_return = 0u;
    expect_complete(x, 0u, 2u);
  }
  {
    Fixture x;
    x.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7u;
    CHECK(x.run() == NBA97_TEXT_UNKNOWN);
    CHECK(x.progress.stopped_pc == UINT32_C(0x8005faa0));
  }
  {
    Fixture x;
    x.known[kFirst - kBase] = 0u;
    put16(x.ram, UINT32_C(0x800fe8cc), 1u);
    put16(x.ram, UINT32_C(0x800fe8ca), 0u);
    CHECK(x.run() == NBA97_TEXT_UNKNOWN);
    CHECK(x.progress.stopped_pc == UINT32_C(0x8005f9c8));
    CHECK(x.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
          0u);
  }
}

void test_access_and_lui_prefixes() {
  Fixture f;
  expect_complete(f, UINT32_C(0xab), 2u);
  CHECK(f.progress.access_events >= 5u);
  CHECK(f.journal[0].pc == UINT32_C(0x8005f94c) &&
        f.journal[0].address == kStack - 0xcu);
  CHECK(f.journal[1].pc == UINT32_C(0x8005f958) &&
        f.journal[1].address == UINT32_C(0x800fe8cc));
  CHECK(f.journal[2].pc == UINT32_C(0x8005f960) &&
        f.journal[2].address == UINT32_C(0x800fe8ca));
  CHECK(f.journal[3].pc == UINT32_C(0x8005f964) &&
        f.journal[3].address == kStack - 0x10u);
  CHECK(f.journal[4].pc == UINT32_C(0x8005f974) &&
        f.journal[4].address == kStack - 8u);

  Fixture v0_prefix;
  v0_prefix.context.operation_budget = 1u;
  CHECK(v0_prefix.run() == NBA97_TEXT_LIMIT);
  CHECK(v0_prefix.progress.stopped_pc == UINT32_C(0x8005f958));
  CHECK(v0_prefix.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .word == UINT32_C(0x80100000));
  Fixture a1_prefix;
  a1_prefix.context.operation_budget = 2u;
  CHECK(a1_prefix.run() == NBA97_TEXT_LIMIT);
  CHECK(a1_prefix.progress.stopped_pc == UINT32_C(0x8005f960));
  CHECK(a1_prefix.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
            .word == UINT32_C(0x80100000));
}

void test_every_budget_prefix() {
  Fixture full;
  expect_complete(full, UINT32_C(0xab), 2u);
  const size_t complete_operations = full.progress.operations;
  CHECK(complete_operations > 0u);
  for (size_t budget = 0; budget < complete_operations; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    CHECK(f.run() == NBA97_TEXT_LIMIT);
    CHECK(f.progress.operations == budget);
  }
  Fixture exact;
  exact.context.operation_budget = complete_operations;
  expect_complete(exact, UINT32_C(0xab), 2u);
}

} // namespace

int main() {
  test_team_windows_and_calls();
  test_same_team_x_and_geometry_signedness();
  test_identity_and_asymmetric_state_gates();
  test_fractional_wrap_and_action_mask();
  test_live_machine_and_failures();
  test_access_and_lui_prefixes();
  test_every_budget_prefix();
  std::puts("game_actor_contact_eligibility_tests: 67 focused executions, "
            "including all 15 operation-budget cutoffs, passed");
  return 0;
}
