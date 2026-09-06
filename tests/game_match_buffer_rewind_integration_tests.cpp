#include "game_match_buffer_initialize_adapter.h"
#include "game_match_buffer_rewind_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
using U32 = std::uint32_t;
unsigned checks;
void check(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "match buffer rewind integration check %u line %u\n",
                 checks, line);
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
  static constexpr U32 Stack = 0x801ff000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameMatchStateResetAccess, 128> parent_journal{};
  std::array<Nba97GameMatchBufferRewindAccess, 32> rewind_journal{};
  Nba97GameMatchStateResetContext parent{};
  Nba97GameMatchStateResetProgress parent_progress{};
  Nba97GameMatchBufferRewindBinding binding{};
  std::vector<Nba97GameMatchStateResetEvent> other_calls;

  Fixture(unsigned mode = 98) {
    for (unsigned i = 0; i < 32; ++i)
      parent.machine.registers.gpr[i] = {0x33000000u + i, 15};
    parent.machine.registers.gpr[0] = {0, 15};
    parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 15};
    parent.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x81234568u, 15};
    parent.machine.hi = {0x11223344u, 5};
    parent.machine.lo = {0x55667788u, 10};
    parent.memory = {&region, 1};
    parent.operation_budget = 200;
    parent.io = typedChild;
    parent.user = this;
    parent.access_journal = parent_journal.data();
    parent.access_journal_capacity = parent_journal.size();
    binding.operation_budget = 20;
    binding.zero_operation_budget = 2;
    binding.access_journal = rewind_journal.data();
    binding.access_journal_capacity = rewind_journal.size();
    put(0x8001edecu, mode, 2);
    put(0x800fa004u, 0x89abcdefu);
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
  static int typedChild(void *opaque, const Nba97GameTextMemory *,
                        const Nba97GameMatchStateResetEvent *event,
                        Nba97GameMatchStateResetMachine *) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.other_calls.push_back(*event);
    return 1;
  }
  int run() {
    return nba97_game_match_state_reset_with_match_buffer_rewind(
        &parent, &binding, &parent_progress);
  }
};

void natural_mode_paths() {
  Fixture mode98;
  CHECK(mode98.run() == NBA97_TEXT_COMPLETE);
  CHECK(mode98.parent_progress.completed && mode98.parent_progress.mode_98);
  CHECK(mode98.binding.invocations == 1 && mode98.binding.completions == 1);
  CHECK(mode98.binding.zero_invocations == 1 &&
        mode98.binding.zero_completions == 1);
  CHECK(mode98.binding.event.pc == 0x80065ae8u &&
        mode98.binding.event.delay_slot_pc == 0x80065aecu &&
        mode98.binding.event.entry == 0x80076ad0u &&
        mode98.binding.event.invocation == 1 &&
        mode98.binding.event.argument_count == 0);
  CHECK(mode98.binding.progress.completed &&
        mode98.binding.progress.operations == 9);
  CHECK(mode98.binding.zero_progress.completed &&
        mode98.binding.zero_progress.used_small_path == 0 &&
        mode98.binding.zero_progress.stores == 2 &&
        mode98.binding.zero_progress.bytes_stored == 8);
  CHECK(mode98.get(0x800f1918u) == 0);
  CHECK(mode98.get(0x800fa00cu) == 0x89abcdefu &&
        mode98.get(0x800fa010u) == 0x89abcdefu);
  CHECK(mode98.parent_progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
            .word == Fixture::Stack);
  CHECK(mode98.parent_progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
            .word == 0x81234568u);

  Fixture other(7);
  CHECK(other.run() == NBA97_TEXT_COMPLETE);
  CHECK(other.parent_progress.completed && !other.parent_progress.mode_98);
  CHECK(other.binding.invocations == 0 && other.binding.completions == 0 &&
        other.binding.zero_invocations == 0);
  bool saw_fallback = false;
  for (const auto &event : other.other_calls)
    if (event.pc == 0x80065af8u && event.entry == 0x8006432cu)
      saw_fallback = true;
  CHECK(saw_fallback);
}

void zero_budget_prefixes() {
  for (std::size_t budget = 0; budget <= 2; ++budget) {
    Fixture fixture;
    fixture.binding.zero_operation_budget = budget;
    int result = fixture.run();
    CHECK(result == (budget == 2 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_LIMIT));
    CHECK(fixture.binding.zero_progress.operations == budget);
    CHECK(fixture.binding.zero_progress.stores == budget);
    CHECK(fixture.binding.zero_progress.used_small_path == 0);
    if (budget == 0) {
      CHECK(fixture.binding.progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_A0]
                .word == 0x800f1918u);
      CHECK(fixture.binding.progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_A1]
                .word == 4);
    } else {
      CHECK(fixture.binding.progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_A0]
                .word == 0x800f1918u);
      CHECK(fixture.binding.progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_A1]
                .word == 0xfffffffcu);
      CHECK(fixture.binding.progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_T0]
                .word == 4);
      CHECK(fixture.binding.progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_T0 + 1]
                .word == 0);
    }
    CHECK(fixture.binding.progress.machine.registers
              .gpr[NBA97_MATCH_INITIALIZE_AT]
              .word == (budget == 2 ? 0x80100000u : 0u));
    CHECK(fixture.binding.progress.machine.registers
                  .gpr[NBA97_MATCH_INITIALIZE_A2]
                  .word == 0 &&
          fixture.binding.progress.machine.registers
                  .gpr[NBA97_MATCH_INITIALIZE_T0 + 2]
                  .word == 0);
    CHECK(fixture.get(0x800f1918u) == (budget ? 0u : UINT32_C(0xa5a5a5a5)));
  }

  Fixture late;
  late.binding.operation_budget = 8;
  CHECK(late.run() == NBA97_TEXT_LIMIT);
  CHECK(late.binding.progress.stores == 6 &&
        late.binding.progress.callbacks_completed == 1);
  CHECK(late.binding.progress.stopped_pc == 0x80076b18u);
  CHECK(late.binding.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
            .word == late.binding.progress.frame_stack_pointer);
  CHECK(late.parent_progress.stopped_pc == 0x80065ae8u);
}

void repeated_binding_and_event_guards() {
  Fixture repeated;
  CHECK(repeated.run() == NBA97_TEXT_COMPLETE);
  repeated.put(0x8001edecu, 98, 2);
  repeated.put(0x800f1918u, 0xa5a5a5a5u);
  CHECK(repeated.run() == NBA97_TEXT_COMPLETE);
  CHECK(repeated.binding.invocations == 2 &&
        repeated.binding.completions == 2 &&
        repeated.binding.zero_invocations == 2 &&
        repeated.binding.zero_completions == 2);

  Nba97GameMatchStateResetMachine machine = repeated.parent.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80065af0u, 15};
  Nba97GameMatchStateResetEvent event{0x80065ae8u,
                                      0x80065aecu,
                                      0x80076ad0u,
                                      1,
                                      1,
                                      NBA97_GAME_MATCH_STATE_RESET_80076AD0,
                                      0};
  for (unsigned field = 0; field < 8; ++field) {
    Fixture fixture;
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
      malformed.kind = NBA97_GAME_MATCH_STATE_RESET_8006432C;
    else if (field == 5)
      malformed.argument_count = 1;
    else if (field == 6)
      candidate.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word += 4;
    else
      candidate.lo.known_mask = 16;
    auto before = candidate;
    CHECK(nba97_game_match_buffer_rewind_from_match_state_reset(
              &fixture.binding, &fixture.parent.memory, &malformed,
              &candidate) == 0);
    CHECK(sameMachine(candidate, before));
    CHECK(fixture.binding.invocations == 0);
  }
}

void zero_bridge_guards_and_partial_v0() {
  Fixture fixture;
  Nba97GameMatchBufferRewindMachine machine = fixture.parent.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {0x800f1918u, 15};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {4, 15};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2] = {0x12345678u, 3};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0x76543210u, 5};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0] = {0x10101010u, 6};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 1] = {0x11111111u, 9};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80076b00u, 15};
  Nba97GameMatchBufferRewindEvent event{0x80076af8u,
                                        0x80076afcu,
                                        0x800a3a74u,
                                        5,
                                        1,
                                        NBA97_GAME_MATCH_BUFFER_REWIND_ZERO,
                                        2};
  CHECK(nba97_game_match_buffer_rewind_compose_zero(
            &fixture.binding, &fixture.parent.memory, &event, &machine) == 1);
  CHECK(machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word == 0x76543210u &&
        machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].known_mask == 5);
  CHECK(machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT].word == 0 &&
        machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word == 0 &&
        machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0].word == 4 &&
        machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 1].word == 0 &&
        machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 2].word == 0);

  for (unsigned field = 0; field < 7; ++field) {
    Fixture guarded;
    auto malformed = event;
    auto candidate = machine;
    candidate.registers.gpr[4] = {0x800f1918u, 15};
    candidate.registers.gpr[5] = {4, 15};
    candidate.registers.gpr[31] = {0x80076b00u, 15};
    if (field == 0)
      malformed.pc += 4;
    else if (field == 1)
      malformed.delay_slot_pc += 4;
    else if (field == 2)
      malformed.entry += 4;
    else if (field == 3)
      malformed.invocation = 2;
    else if (field == 4)
      malformed.argument_count = 1;
    else if (field == 5)
      candidate.registers.gpr[4].word += 4;
    else
      candidate.registers.gpr[31].word += 4;
    auto before = candidate;
    CHECK(nba97_game_match_buffer_rewind_compose_zero(
              &guarded.binding, &guarded.parent.memory, &malformed,
              &candidate) == 0);
    CHECK(sameMachine(candidate, before));
    CHECK(guarded.binding.zero_invocations == 0);
  }
}

struct BufferFixture : Fixture {
  Nba97GameMatchBufferInitializeBinding buffer{};
  BufferFixture() : Fixture(7) {
    parent.io = route;
    parent.user = this;
    buffer.operation_budget = 16;
    buffer.zero_operation_budget = 512;
    buffer.io = nba97_game_match_buffer_rewind_from_match_buffer_initialize;
    buffer.user = &binding;
  }
  static int route(void *opaque, const Nba97GameTextMemory *memory,
                   const Nba97GameMatchStateResetEvent *event,
                   Nba97GameMatchStateResetMachine *machine) {
    auto &f = *static_cast<BufferFixture *>(opaque);
    if (event->entry == 0x8006432cu)
      return nba97_game_match_buffer_initialize_from_match_state_reset(
          &f.buffer, memory, event, machine);
    return typedChild(&f, memory, event, machine);
  }
};
void natural_buffer_and_zero_chain() {
  for (unsigned budget = 0; budget <= 2; ++budget) {
    BufferFixture f;
    f.binding.zero_operation_budget = budget;
    CHECK(f.run() ==
          (budget == 2 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_IO_REFUSED));
    CHECK(f.buffer.zero_progress.completed &&
          f.buffer.zero_progress.stores == 223);
    CHECK(f.binding.zero_progress.stores == budget &&
          f.binding.invocations == 1);
    CHECK(f.get(0x800fa004u) == 0x800ccc00u &&
          f.get(0x800fa008u) == 0x800d5734u);
    CHECK(f.get(0x800fa00cu) == 0x800ccc00u &&
          f.get(0x800fa010u) == 0x800ccc00u);
    CHECK(f.binding.buffer_event.pc == 0x80064370u &&
          f.binding.buffer_event.delay_slot_pc == 0x80064374u);
    if (budget == 2) {
      CHECK(f.buffer.progress.completed && f.binding.progress.completed);
      CHECK(f.binding.progress.machine.registers.gpr[31].word == 0x80064378u);
      CHECK(f.binding.progress.machine.registers.gpr[29].word ==
            Fixture::Stack - 0x40);
      CHECK(f.parent_progress.machine.registers.gpr[31].word == 0x81234568u &&
            f.parent_progress.machine.registers.gpr[29].word == Fixture::Stack);
      CHECK(f.get(0x800f1918u) == 0 && f.get(0x800fe860u) == 0 &&
            f.get(0x8002148cu, 2) == 0 && f.get(0x800fe864u, 1) == 0);
      CHECK(f.run() == NBA97_TEXT_COMPLETE && f.binding.invocations == 2 &&
            f.binding.completions == 2);
    } else {
      CHECK(f.binding.result == NBA97_TEXT_LIMIT &&
            f.buffer.result == NBA97_TEXT_IO_REFUSED);
      CHECK(f.parent_progress.stopped_pc == 0x80065af8u &&
            f.buffer.progress.stopped_pc == 0x80064370u &&
            f.binding.progress.stopped_pc == 0x80076af8u);
      CHECK(f.get(0x800fe860u) == 0xa5a5a5a5u);
    }
  }
  for (unsigned field = 0; field < 8; ++field) {
    BufferFixture f;
    Nba97GameMatchBufferInitializeMachine machine{};
    machine.registers = f.parent.machine.registers;
    machine.hi = {f.parent.machine.hi.word, f.parent.machine.hi.known_mask};
    machine.lo = {f.parent.machine.lo.word, f.parent.machine.lo.known_mask};
    machine.registers.gpr[31] = {0x80064378u, 15};
    Nba97GameMatchBufferInitializeEvent event{
        0x80064370u,
        0x80064374u,
        0x80076ad0u,
        6,
        1,
        NBA97_GAME_MATCH_BUFFER_INITIALIZE_CHILD_80076AD0,
        0};
    if (field == 0)
      ++event.pc;
    else if (field == 1)
      ++event.delay_slot_pc;
    else if (field == 2)
      ++event.entry;
    else if (field == 3)
      ++event.invocation;
    else if (field == 4)
      event.kind = 1;
    else if (field == 5)
      event.argument_count = 1;
    else if (field == 6)
      machine.registers.gpr[31].known_mask = 14;
    else
      machine.lo.known_mask = 16;
    const auto before = machine;
    const auto bytes = f.bytes;
    CHECK(!nba97_game_match_buffer_rewind_from_match_buffer_initialize(
        &f.binding, &f.parent.memory, &event, &machine));
    CHECK(f.binding.result == NBA97_TEXT_ARGUMENT &&
          f.binding.invocations == 0 && f.bytes == bytes);
    for (unsigned i = 0; i < 32; ++i)
      CHECK(machine.registers.gpr[i].word == before.registers.gpr[i].word &&
            machine.registers.gpr[i].known_mask ==
                before.registers.gpr[i].known_mask);
    CHECK(machine.hi.word == before.hi.word &&
          machine.hi.known_mask == before.hi.known_mask &&
          machine.lo.word == before.lo.word &&
          machine.lo.known_mask == before.lo.known_mask);
  }
}
} // namespace

int main() {
  natural_buffer_and_zero_chain();
  natural_mode_paths();
  zero_budget_prefixes();
  repeated_binding_and_event_guards();
  zero_bridge_guards_and_partial_v0();
  std::printf("game match buffer rewind integration tests passed (%u checks)\n",
              checks);
}
