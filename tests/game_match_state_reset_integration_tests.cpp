#include "game_match_state_reset_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
using U32 = std::uint32_t;
unsigned checks;
void check(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "match state reset integration check %u line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define CHECK(value) check((value), __LINE__)

struct Fixture {
  static constexpr U32 Base = 0x80000000u;
  static constexpr std::size_t Size = 0x200000u;
  static constexpr U32 Stack = 0x801ff000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameMatchInitializeAccess, 64> parent_journal{};
  std::array<Nba97GameMatchStateResetAccess, 64> reset_journal{};
  std::array<Nba97GameRosterBindingsAccess, 256> roster_journal{};
  Nba97GameMatchInitializeContext parent{};
  Nba97GameMatchInitializeProgress parent_progress{};
  Nba97GameMatchStateResetBinding binding{};
  Nba97GameMemoryZeroProgress parent_zero{};
  std::vector<Nba97GameMatchInitializeEvent> parent_calls;
  std::vector<Nba97GameMatchStateResetEvent> reset_calls;
  int parent_zero_result = NBA97_TEXT_ARGUMENT;
  int provider_mode = 0;
  int malformed_reset = 0;
  U32 refuse_reset_pc = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      parent.registers.gpr[i] = {0x22000000u + i, 15};
    parent.registers.gpr[0] = {0, 15};
    parent.registers.gpr[29] = {Stack, 15};
    parent.registers.gpr[31] = {0x81234568u, 15};
    parent.memory = {&region, 1};
    parent.operation_budget = 100;
    parent.io = parentIo;
    parent.user = this;
    parent.access_journal = parent_journal.data();
    parent.access_journal_capacity = parent_journal.size();
    binding.operation_budget = 100;
    for (auto &budget : binding.zero_operation_budget)
      budget = 2000;
    binding.roster_operation_budget = 1000;
    binding.io = resetIo;
    binding.user = this;
    binding.hi_lo_provider = hiLo;
    binding.hi_lo_user = this;
    binding.access_journal = reset_journal.data();
    binding.access_journal_capacity = reset_journal.size();
    binding.roster_journal = roster_journal.data();
    binding.roster_journal_capacity = roster_journal.size();
    put(0x80021d74u, 0);
    put(0x80021d78u, 1);
    put(0x80023aecu, 2, 1);
    put(0x80023b54u, 3, 1);
    put(0x8001edecu, 98, 2);
  }

  void put(U32 address, U32 value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i) {
      bytes[address - Base + i] = static_cast<std::uint8_t>(value >> (8 * i));
      known[address - Base + i] = 1;
    }
  }
  U32 get(U32 address, unsigned width = 4) const {
    U32 value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= U32(bytes[address - Base + i]) << (8 * i);
    return value;
  }

  static void mapZeroRegisters(const Nba97GameMemoryZeroProgress &progress,
                               U32 initial_a0,
                               Nba97GameMatchInitializeRegisters *registers) {
    auto incoming_v0 = registers->gpr[NBA97_MATCH_INITIALIZE_V0];
    registers->gpr[NBA97_MATCH_INITIALIZE_A0] = {progress.working_destination,
                                                 15};
    registers->gpr[NBA97_MATCH_INITIALIZE_A1] = {progress.working_count, 15};
    registers->gpr[NBA97_MATCH_INITIALIZE_A2] = {0, 15};
    registers->gpr[NBA97_MATCH_INITIALIZE_AT] = {0, 15};
    registers->gpr[NBA97_MATCH_INITIALIZE_T0 + 2] = {0, 15};
    registers->gpr[NBA97_MATCH_INITIALIZE_V0] = incoming_v0;
    if (progress.stores) {
      U32 alignment = initial_a0 & 3u;
      registers->gpr[NBA97_MATCH_INITIALIZE_T0] = {4u - alignment, 15};
      registers->gpr[NBA97_MATCH_INITIALIZE_T0 + 1] = {alignment, 15};
    }
  }

  static int parentIo(void *opaque, const Nba97GameTextMemory *memory,
                      const Nba97GameMatchInitializeEvent *event,
                      Nba97GameMatchInitializeRegisters *registers) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.parent_calls.push_back(*event);
    if (event->kind == NBA97_MATCH_INITIALIZE_MEMORY_ZERO) {
      U32 initial_a0 = registers->gpr[NBA97_MATCH_INITIALIZE_A0].word;
      auto incoming_v0 = registers->gpr[NBA97_MATCH_INITIALIZE_V0];
      Nba97GameMemoryZeroContext context{
          *memory,
          2000,
          initial_a0,
          registers->gpr[NBA97_MATCH_INITIALIZE_A1].word,
          incoming_v0.word,
          static_cast<std::uint8_t>(incoming_v0.known_mask == 15)};
      fixture.parent_zero_result =
          nba97_game_memory_zero(&context, &fixture.parent_zero);
      mapZeroRegisters(fixture.parent_zero, initial_a0, registers);
      return fixture.parent_zero_result == NBA97_TEXT_COMPLETE;
    }
    return 1;
  }

  static int resetIo(void *opaque, const Nba97GameTextMemory *,
                     const Nba97GameMatchStateResetEvent *event,
                     Nba97GameMatchStateResetMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.reset_calls.push_back(*event);
    if (event->pc == fixture.refuse_reset_pc)
      return 0;
    if (fixture.malformed_reset == 1) {
      machine->registers.gpr[9] = {0xfeed0009u, 16};
    } else if (fixture.malformed_reset == 2) {
      machine->registers.gpr[9] = {0xfeed0009u, 15};
      machine->hi = {0xfeed0010u, 16};
    } else if (fixture.malformed_reset == 3) {
      machine->lo = {0xfeed0011u, 16};
    } else if (fixture.malformed_reset == 4) {
      machine->registers.gpr[0].known_mask = 14;
    }
    return 1;
  }

  static int hiLo(void *opaque, const Nba97GameTextMemory *,
                  const Nba97GameMatchInitializeEvent *,
                  Nba97GameMatchStateResetWord *hi,
                  Nba97GameMatchStateResetWord *lo) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    if (fixture.provider_mode == 1)
      return 0;
    *hi = {0, 0};
    *lo = {0, 0};
    if (fixture.provider_mode == 2)
      hi->known_mask = 16;
    return 1;
  }

  int run() {
    return nba97_game_match_initialize_with_state_reset(&parent, &binding,
                                                        &parent_progress);
  }
};

void natural_success() {
  Fixture fixture;
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
  CHECK(fixture.parent_progress.completed &&
        fixture.binding.progress.completed);
  CHECK(fixture.binding.invocations == 1 && fixture.binding.completions == 1);
  CHECK(fixture.binding.event.pc == 0x8002dbf8u);
  CHECK(fixture.binding.event.delay_slot_pc == 0x8002dbfcu);
  CHECK(fixture.binding.event.entry == 0x800659f0u);
  CHECK(fixture.binding.event.argument_count == 0);
  CHECK(fixture.parent_zero_result == NBA97_TEXT_COMPLETE &&
        fixture.parent_zero.completed);
  CHECK(fixture.binding.zero_invocations == 4);
  for (unsigned i = 0; i < 4; ++i) {
    CHECK(fixture.binding.zero_result[i] == NBA97_TEXT_COMPLETE);
    CHECK(fixture.binding.zero_progress[i].completed);
  }
  CHECK(fixture.binding.roster_invocations == 1);
  CHECK(fixture.binding.roster_result == NBA97_TEXT_COMPLETE);
  CHECK(fixture.binding.roster_progress.completed);
  CHECK(fixture.binding.unresolved_callbacks_completed == 9);
  CHECK(fixture.binding.progress.machine.hi.known_mask == 0 &&
        fixture.binding.progress.machine.lo.known_mask == 0);
  CHECK(fixture.get(0x800fdb9cu, 2) == 0xffff);
  CHECK(fixture.get(0x800fdb54u, 2) == 0);
  CHECK(fixture.get(0x8001edf2u, 2) == 0);
  CHECK(fixture.get(0x8001eeccu, 2) == 5);
  for (U32 address = 0x8001f33cu; address < 0x8001f7ecu; ++address)
    CHECK(fixture.get(address, 1) == 0);
  CHECK(fixture.parent_progress.registers.gpr[29].word == Fixture::Stack);
  CHECK(fixture.parent_progress.registers.gpr[31].word == 0x81234568u);
}

void nested_failures() {
  Fixture zero;
  zero.binding.zero_operation_budget[0] = 0;
  CHECK(zero.run() == NBA97_TEXT_LIMIT);
  CHECK(zero.binding.result == NBA97_TEXT_LIMIT);
  CHECK(zero.parent_progress.stopped_pc == 0x8002dbf8u);
  CHECK(zero.binding.progress.stopped_pc == 0x80065a0cu);
  CHECK(zero.binding.zero_progress[0].stopped_pc == 0x800a3a94u);
  CHECK(zero.binding.progress.machine.registers.gpr[1].word == 0);
  CHECK(zero.binding.progress.machine.registers.gpr[6].word == 0);

  Fixture first_store;
  first_store.binding.zero_operation_budget[0] = 1;
  CHECK(first_store.run() == NBA97_TEXT_LIMIT);
  CHECK(first_store.binding.zero_progress[0].stores == 1);
  CHECK(first_store.binding.progress.machine.registers.gpr[8].word == 4);
  CHECK(first_store.binding.progress.machine.registers.gpr[9].word == 0);

  Fixture second_zero;
  second_zero.binding.zero_operation_budget[1] = 400;
  CHECK(second_zero.run() == NBA97_TEXT_LIMIT);
  CHECK(second_zero.binding.zero_invocations == 2);
  CHECK(second_zero.binding.zero_progress[0].completed);
  CHECK(second_zero.binding.zero_progress[1].operations == 400);

  Fixture roster;
  roster.binding.roster_operation_budget = 0;
  CHECK(roster.run() == NBA97_TEXT_LIMIT);
  CHECK(roster.binding.result == NBA97_TEXT_LIMIT);
  CHECK(roster.binding.progress.stopped_pc == 0x80065a54u);
  CHECK(roster.binding.roster_progress.operations == 0);

  Fixture refused;
  refused.refuse_reset_pc = 0x80065a9cu;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED);
  CHECK(refused.binding.progress.stopped_pc == 0x80065a9cu);
}

void zero_composition_prefixes() {
  for (unsigned alignment = 0; alignment < 4; ++alignment) {
    Fixture fixture;
    Nba97GameMatchStateResetMachine machine{};
    machine.registers = fixture.parent.registers;
    machine.registers.gpr[4] = {0x80030000u + alignment, 15};
    machine.registers.gpr[5] = {0x4b0u, 15};
    machine.registers.gpr[2] = {0x89abcdefu, 5};
    machine.registers.gpr[8] = {0x11110008u, 7};
    machine.registers.gpr[9] = {0x11110009u, 11};
    machine.registers.gpr[31] = {0x80065a14u, 15};
    machine.hi = {0x12345678u, 15};
    machine.lo = {0x87654321u, 15};
    Nba97GameMatchStateResetEvent event{0x80065a0cu,
                                        0x80065a10u,
                                        0x800a3a74u,
                                        1,
                                        1,
                                        NBA97_GAME_MATCH_STATE_RESET_ZERO,
                                        2};
    CHECK(nba97_game_match_state_reset_compose_zero(
              &fixture.binding, &fixture.parent.memory, &event, &machine) == 1);
    CHECK(fixture.binding.zero_progress[0].completed);
    CHECK(machine.registers.gpr[8].word == 4u - alignment &&
          machine.registers.gpr[8].known_mask == 15);
    CHECK(machine.registers.gpr[9].word == alignment &&
          machine.registers.gpr[9].known_mask == 15);
    CHECK(machine.registers.gpr[1].word == 0 &&
          machine.registers.gpr[6].word == 0 &&
          machine.registers.gpr[10].word == 0);
    CHECK(machine.registers.gpr[2].word == 0x89abcdefu &&
          machine.registers.gpr[2].known_mask == 5);
    CHECK(machine.hi.word == 0x12345678u && machine.lo.word == 0x87654321u);
  }

  for (std::size_t budget = 0; budget <= 10; ++budget) {
    Fixture fixture;
    fixture.binding.zero_operation_budget[0] = budget;
    Nba97GameMatchStateResetMachine machine{};
    machine.registers = fixture.parent.registers;
    machine.registers.gpr[4] = {0x80030003u, 15};
    machine.registers.gpr[5] = {0x4b0u, 15};
    machine.registers.gpr[2] = {0x76543210u, 10};
    machine.registers.gpr[8] = {0x11110008u, 7};
    machine.registers.gpr[9] = {0x11110009u, 11};
    machine.registers.gpr[31] = {0x80065a14u, 15};
    Nba97GameMatchStateResetEvent event{0x80065a0cu,
                                        0x80065a10u,
                                        0x800a3a74u,
                                        1,
                                        1,
                                        NBA97_GAME_MATCH_STATE_RESET_ZERO,
                                        2};
    CHECK(nba97_game_match_state_reset_compose_zero(
              &fixture.binding, &fixture.parent.memory, &event, &machine) == 0);
    CHECK(fixture.binding.nested_result == NBA97_TEXT_LIMIT);
    CHECK(fixture.binding.zero_progress[0].operations == budget);
    CHECK(fixture.binding.zero_progress[0].stores == budget);
    CHECK(machine.registers.gpr[2].word == 0x76543210u &&
          machine.registers.gpr[2].known_mask == 10);
    if (!budget) {
      CHECK(machine.registers.gpr[8].word == 0x11110008u &&
            machine.registers.gpr[8].known_mask == 7);
      CHECK(machine.registers.gpr[9].word == 0x11110009u &&
            machine.registers.gpr[9].known_mask == 11);
    } else {
      CHECK(machine.registers.gpr[8].word == 1 &&
            machine.registers.gpr[9].word == 3);
    }
  }

  Fixture middle;
  middle.binding.zero_operation_budget[1] = 17;
  Nba97GameMatchStateResetMachine machine{};
  machine.registers = middle.parent.registers;
  machine.registers.gpr[4] = {0x80031000u, 15};
  machine.registers.gpr[5] = {0x1320u, 15};
  machine.registers.gpr[31] = {0x80065a20u, 15};
  Nba97GameMatchStateResetEvent second{0x80065a18u,
                                       0x80065a1cu,
                                       0x800a3a74u,
                                       1,
                                       2,
                                       NBA97_GAME_MATCH_STATE_RESET_ZERO,
                                       2};
  CHECK(nba97_game_match_state_reset_compose_zero(
            &middle.binding, &middle.parent.memory, &second, &machine) == 0);
  CHECK(middle.binding.nested_result == NBA97_TEXT_LIMIT);
  CHECK(middle.binding.zero_progress[1].stores == 17);
  CHECK(middle.binding.zero_progress[1].stopped_pc == 0x800a3af8u);

  Fixture tail;
  tail.binding.zero_operation_budget[2] = 49;
  machine = {};
  machine.registers = tail.parent.registers;
  machine.registers.gpr[4] = {0x80032000u, 15};
  machine.registers.gpr[5] = {0xc4u, 15};
  machine.registers.gpr[31] = {0x80065a2cu, 15};
  Nba97GameMatchStateResetEvent third{0x80065a24u,
                                      0x80065a28u,
                                      0x800a3a74u,
                                      1,
                                      3,
                                      NBA97_GAME_MATCH_STATE_RESET_ZERO,
                                      2};
  CHECK(nba97_game_match_state_reset_compose_zero(
            &tail.binding, &tail.parent.memory, &third, &machine) == 0);
  CHECK(tail.binding.zero_progress[2].stores == 49);
  CHECK(tail.binding.zero_progress[2].stopped_pc == 0x800a3b8cu);
}

void metadata_and_prefixes() {
  Fixture provider_refused;
  provider_refused.provider_mode = 1;
  CHECK(provider_refused.run() == NBA97_TEXT_IO_REFUSED);
  CHECK(provider_refused.binding.invocations == 1 &&
        provider_refused.binding.completions == 0);

  Fixture provider_bad;
  provider_bad.provider_mode = 2;
  CHECK(provider_bad.run() == NBA97_TEXT_ARGUMENT);
  CHECK(provider_bad.binding.invocations == 1 &&
        provider_bad.binding.completions == 0);

  Fixture bad_hi;
  bad_hi.malformed_reset = 2;
  CHECK(bad_hi.run() == NBA97_TEXT_ARGUMENT);
  CHECK(bad_hi.binding.progress.machine.hi.word == 0xfeed0010u &&
        bad_hi.binding.progress.machine.hi.known_mask == 16);
  CHECK(bad_hi.parent_progress.registers.gpr[9].word == 0xfeed0009u &&
        bad_hi.parent_progress.registers.gpr[9].known_mask == 15);

  Fixture bad_gpr;
  bad_gpr.malformed_reset = 1;
  CHECK(bad_gpr.run() == NBA97_TEXT_ARGUMENT);
  CHECK(bad_gpr.binding.progress.machine.registers.gpr[9].known_mask == 16);
  CHECK(bad_gpr.parent_progress.registers.gpr[9].known_mask == 15);

  Fixture bad_zero;
  bad_zero.malformed_reset = 4;
  CHECK(bad_zero.run() == NBA97_TEXT_ARGUMENT);
  CHECK(bad_zero.binding.progress.machine.registers.gpr[0].known_mask == 14);
  CHECK(bad_zero.parent_progress.registers.gpr[0].known_mask == 15);

  Fixture reuse;
  auto registers = reuse.parent.registers;
  registers.gpr[31] = {0x8002dc00u, 15};
  Nba97GameMatchInitializeEvent event{0x8002dbf8u,
                                      0x8002dbfcu,
                                      0x800659f0u,
                                      1,
                                      NBA97_MATCH_INITIALIZE_CHILD_800659F0,
                                      0};
  CHECK(nba97_game_match_state_reset_from_match_initialize(
            &reuse.binding, &reuse.parent.memory, &event, &registers) == 1);
  CHECK(nba97_game_match_state_reset_from_match_initialize(
            &reuse.binding, &reuse.parent.memory, &event, &registers) == 1);
  CHECK(reuse.binding.invocations == 2 && reuse.binding.completions == 2);
  CHECK(reuse.binding.zero_invocations == 8);

  for (unsigned field = 0; field < 6; ++field) {
    Fixture guard;
    auto guarded_registers = guard.parent.registers;
    guarded_registers.gpr[31] = {0x8002dc00u, 15};
    auto malformed = event;
    if (field == 0)
      malformed.pc += 4;
    else if (field == 1)
      malformed.delay_slot_pc += 4;
    else if (field == 2)
      malformed.entry += 4;
    else if (field == 3)
      malformed.kind = NBA97_MATCH_INITIALIZE_CHILD_80065DB0;
    else if (field == 4)
      malformed.argument_count = 1;
    else
      guarded_registers.gpr[31].word += 4;
    auto before = guarded_registers;
    CHECK(nba97_game_match_state_reset_from_match_initialize(
              &guard.binding, &guard.parent.memory, &malformed,
              &guarded_registers) == 0);
    CHECK(guard.binding.invocations == 0);
    CHECK(std::memcmp(&guarded_registers, &before, sizeof before) == 0);
  }
}
} // namespace

int main() {
  natural_success();
  nested_failures();
  zero_composition_prefixes();
  metadata_and_prefixes();
  std::printf("game match state reset integration tests passed (%u checks)\n",
              checks);
}
