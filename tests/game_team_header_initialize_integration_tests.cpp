#include "game_team_header_initialize_adapter.h"

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
    std::fprintf(stderr, "team header natural check %u line %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define CHECK(value) check((value), __LINE__)

bool sameMachine(const Nba97GameMatchStateResetMachine &left,
                 const Nba97GameMatchStateResetMachine &right) {
  for (unsigned i = 0; i < 32; ++i)
    if (left.registers.gpr[i].word != right.registers.gpr[i].word ||
        left.registers.gpr[i].known_mask != right.registers.gpr[i].known_mask)
      return false;
  return left.hi.word == right.hi.word &&
         left.hi.known_mask == right.hi.known_mask &&
         left.lo.word == right.lo.word &&
         left.lo.known_mask == right.lo.known_mask;
}

struct Fixture {
  static constexpr U32 Base = 0x80000000u;
  static constexpr std::size_t Size = 0x200000u;
  static constexpr U32 First = 0x8001edf4u;
  static constexpr U32 Second = 0x8001eeb8u;
  static constexpr U32 Metadata0 = 0x80040000u;
  static constexpr U32 Metadata1 = 0x80040100u;
  static constexpr U32 Stack = 0x801ff000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameMatchStateResetAccess, 128> parent_journal{};
  std::array<Nba97GameTeamHeaderInitializeAccess, 256> first_journal{};
  std::array<Nba97GameTeamHeaderInitializeAccess, 256> second_journal{};
  Nba97GameMatchStateResetContext parent{};
  Nba97GameMatchStateResetProgress parent_progress{};
  Nba97GameTeamHeaderInitializeBinding binding{};
  std::vector<Nba97GameMatchStateResetEvent> other_calls;
  U32 refuse_pc = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      parent.machine.registers.gpr[i] = {0x44000000u + i, 15};
    parent.machine.registers.gpr[0] = {0, 15};
    parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 15};
    parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x81234568u, 15};
    parent.machine.hi = {0x12345678u, 3};
    parent.machine.lo = {0x87654321u, 12};
    parent.memory = {&region, 1};
    parent.operation_budget = 200;
    parent.io = typedChild;
    parent.user = this;
    parent.access_journal = parent_journal.data();
    parent.access_journal_capacity = parent_journal.size();
    binding.operation_budget[0] = 1000;
    binding.operation_budget[1] = 1000;
    binding.access_journal[0] = first_journal.data();
    binding.access_journal[1] = second_journal.data();
    binding.access_journal_capacity[0] = first_journal.size();
    binding.access_journal_capacity[1] = second_journal.size();
    seed();
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
  void seed() {
    put(First, 2, 2);
    put(Second, 3, 2);
    put(First + 0x14, 0, 2);
    put(Second + 0x14, 0, 2); // BN's first JAL delay overwrites this with 5.
    for (unsigned i = 0; i < 5; ++i) {
      put(First + 0x16 + i * 2, 0x5100u + i, 2);
      put(Second + 0x16 + i * 2, 0x5200u + i, 2);
    }
    put(0x80020b0cu + 2 * 4, Metadata0);
    put(0x80020b0cu + 3 * 4, Metadata1);
    put(0x80023aecu + 2 * 104, 4, 1);
    put(0x80023aecu + 3 * 104, 2, 1);
    put(0x80021ed5u, 1, 1);
    put(0x80021ed6u, 0, 1);
    put(0x80021d72u, 2, 1);
    put(Metadata0 + 0x54, 3, 1);
    put(Metadata0 + 0x57, 10, 1);
    put(Metadata1 + 0x54, 5, 1);
    put(Metadata1 + 0x57, 12, 1);
    put(0x8001edecu, 98, 2);
    for (unsigned i = 0; i < 32; ++i)
      put(0x80020becu + 4 * i, 0xa0000000u + i);
  }
  static int typedChild(void *opaque, const Nba97GameTextMemory *,
                        const Nba97GameMatchStateResetEvent *event,
                        Nba97GameMatchStateResetMachine *) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.other_calls.push_back(*event);
    return event->pc != fixture.refuse_pc;
  }
  int run() {
    return nba97_game_match_state_reset_with_team_header_initialize(
        &parent, &binding, &parent_progress);
  }
};

void natural_two_calls() {
  Fixture fixture;
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
  CHECK(fixture.parent_progress.completed);
  CHECK(fixture.binding.invocations == 2 && fixture.binding.completions == 2);
  CHECK(fixture.binding.event[0].pc == 0x80065a88u &&
        fixture.binding.event[0].delay_slot_pc == 0x80065a8cu &&
        fixture.binding.event[0].entry == 0x800655b0u &&
        fixture.binding.event[0].invocation == 1 &&
        fixture.binding.event[0].argument_count == 2);
  CHECK(fixture.binding.event[1].pc == 0x80065a94u &&
        fixture.binding.event[1].delay_slot_pc == 0x80065a98u &&
        fixture.binding.event[1].entry == 0x800655b0u &&
        fixture.binding.event[1].invocation == 2 &&
        fixture.binding.event[1].argument_count == 2);
  CHECK(fixture.binding.progress[0].completed &&
        fixture.binding.progress[1].completed);
  CHECK(fixture.get(Fixture::First + 4) == Fixture::Second &&
        fixture.get(Fixture::Second + 4) == Fixture::First);
  CHECK(fixture.get(Fixture::First + 0x14, 2) == 0 &&
        fixture.get(Fixture::Second + 0x14, 2) == 5);
  CHECK(fixture.get(Fixture::First + 0x66, 2) == 4 &&
        fixture.get(Fixture::Second + 0x66, 2) == 2);
  CHECK(fixture.get(Fixture::First + 0x7c) == 0x80020b8cu &&
        fixture.get(Fixture::Second + 0x7c) == 0x80020bbcu);
  CHECK(fixture.binding.progress[0].machine.hi.word == 0x12345678u &&
        fixture.binding.progress[0].machine.hi.known_mask == 3);
  CHECK(fixture.binding.progress[1].machine.lo.word == 0x87654321u &&
        fixture.binding.progress[1].machine.lo.known_mask == 12);
  CHECK(fixture.other_calls.size() == 12);
  for (const auto &event : fixture.other_calls)
    CHECK(event.kind != NBA97_GAME_MATCH_STATE_RESET_800655B0);
}

void repeated_binding_and_nested_prefixes() {
  Fixture repeated;
  CHECK(repeated.run() == NBA97_TEXT_COMPLETE);
  repeated.seed();
  CHECK(repeated.run() == NBA97_TEXT_COMPLETE);
  CHECK(repeated.binding.invocations == 4 && repeated.binding.completions == 4);

  Fixture second_limit;
  second_limit.binding.operation_budget[1] = 0;
  CHECK(second_limit.run() == NBA97_TEXT_LIMIT);
  CHECK(second_limit.binding.invocations == 2 &&
        second_limit.binding.completions == 1);
  CHECK(second_limit.binding.progress[0].completed);
  CHECK(second_limit.binding.result[1] == NBA97_TEXT_LIMIT);
  CHECK(second_limit.binding.progress[1].operations == 0);
  CHECK(second_limit.binding.progress[1]
            .machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
            .word == Fixture::Second);
  CHECK(second_limit.binding.progress[1]
            .machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
            .word == Fixture::First);
  CHECK(second_limit.binding.progress[1]
            .machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
            .word == 0x80065a9cu);
  CHECK(second_limit.parent_progress.stopped_pc == 0x80065a94u);

  Fixture malformed_second;
  malformed_second.known[0x80020b0cu + 3 * 4 - Fixture::Base + 3] = 2;
  CHECK(malformed_second.run() == NBA97_TEXT_ARGUMENT);
  CHECK(malformed_second.binding.completions == 1);
  CHECK(malformed_second.binding.progress[1].stopped_pc == 0x8006561cu);
  CHECK(malformed_second.parent_progress.stopped_pc == 0x80065a94u);
}

void exact_event_guards() {
  Fixture fixture;
  Nba97GameMatchStateResetMachine machine = fixture.parent.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {Fixture::First, 15};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {Fixture::Second, 15};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80065a90u, 15};
  Nba97GameMatchStateResetEvent event{0x80065a88u,
                                      0x80065a8cu,
                                      0x800655b0u,
                                      1,
                                      1,
                                      NBA97_GAME_MATCH_STATE_RESET_800655B0,
                                      2};
  for (unsigned field = 0; field < 8; ++field) {
    auto malformed = event;
    auto candidate = machine;
    if (field == 0)
      malformed.pc += 4;
    else if (field == 1)
      malformed.delay_slot_pc += 4;
    else if (field == 2)
      malformed.entry += 4;
    else if (field == 3)
      malformed.invocation = 2;
    else if (field == 4)
      malformed.kind = NBA97_GAME_MATCH_STATE_RESET_80065328;
    else if (field == 5)
      malformed.argument_count = 1;
    else if (field == 6)
      candidate.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word += 4;
    else
      candidate.hi.known_mask = 16;
    auto before = candidate;
    std::size_t invocations = fixture.binding.invocations;
    CHECK(nba97_game_team_header_initialize_from_match_state_reset(
              &fixture.binding, &fixture.parent.memory, &malformed,
              &candidate) == 0);
    CHECK(sameMachine(candidate, before));
    CHECK(fixture.binding.invocations == invocations);
  }

  auto valid = machine;
  CHECK(nba97_game_team_header_initialize_from_match_state_reset(
            &fixture.binding, &fixture.parent.memory, &event, &valid) == 1);
  CHECK(fixture.binding.invocations == 1 && fixture.binding.completions == 1);
}
} // namespace

int main() {
  natural_two_calls();
  repeated_binding_and_nested_prefixes();
  exact_event_guards();
  std::printf(
      "game team header initialize integration tests passed (%u checks)\n",
      checks);
}
