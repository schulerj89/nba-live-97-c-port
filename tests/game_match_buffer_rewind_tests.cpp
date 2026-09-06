#include "recovered/game_match_buffer_rewind.h"

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
    std::fprintf(stderr, "match buffer rewind check %u line %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define CHECK(value) check((value), __LINE__)

bool sameWord(const Nba97GameMatchBufferRewindWord &left,
              const Nba97GameMatchBufferRewindWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool sameMachine(const Nba97GameMatchBufferRewindMachine &left,
                 const Nba97GameMatchBufferRewindMachine &right) {
  for (unsigned i = 0; i < 32; ++i)
    if (!sameWord(left.registers.gpr[i], right.registers.gpr[i]))
      return false;
  return sameWord(left.hi, right.hi) && sameWord(left.lo, right.lo);
}

struct Fixture {
  static constexpr U32 Base = 0x80000000u;
  static constexpr std::size_t Size = 0x200000u;
  static constexpr U32 Stack = 0x801ff000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameMatchBufferRewindAccess, 32> journal{};
  Nba97GameMatchBufferRewindContext context{};
  Nba97GameMatchBufferRewindProgress progress{};
  std::vector<Nba97GameMatchBufferRewindEvent> calls;
  int callback_mode = 0;
  U32 callback_stack = 0x801fe000u;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x22000000u + i, 15};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Stack, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x81234568u,
                                                                15};
    context.machine.hi = {0x13579bdfu, 5};
    context.machine.lo = {0x2468ace0u, 10};
    context.memory = {&region, 1};
    context.operation_budget = 20;
    context.io = callback;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    put(0x800fa004u, 0x800f1918u);
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
  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameMatchBufferRewindEvent *event,
                      Nba97GameMatchBufferRewindMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.calls.push_back(*event);
    if (fixture.callback_mode == 1)
      return 0;
    if (fixture.callback_mode == 2) {
      machine->registers.gpr[9] = {0xfeed0009u, 16};
      return 1;
    }
    if (fixture.callback_mode == 3) {
      machine->hi = {0xfeed0010u, 16};
      return 1;
    }
    if (fixture.callback_mode == 4) {
      machine->lo = {0xfeed0011u, 16};
      return 1;
    }
    if (fixture.callback_mode == 5) {
      for (unsigned i = 1; i < 32; ++i)
        machine->registers.gpr[i] = {0x55000000u + i,
                                     static_cast<std::uint8_t>(i & 15)};
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {
          fixture.callback_stack, 15};
      machine->hi = {0x55550020u, 6};
      machine->lo = {0x55550021u, 9};
      fixture.put(fixture.callback_stack + 0x10, 0x82345678u);
    } else if (fixture.callback_mode == 6) {
      U32 address =
          machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP].word + 0x10u;
      fixture.known[address - Base + 3] = 2;
    } else {
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0x76543210u, 5};
    }
    for (unsigned i = 0; i < 4; ++i) {
      fixture.bytes[0x800f1918u - Base + i] = 0;
      fixture.known[0x800f1918u - Base + i] = 1;
    }
    return 1;
  }
  int run() { return nba97_game_match_buffer_rewind(&context, &progress); }
};

void normal_and_exact_order() {
  Fixture fixture;
  auto entry = fixture.context.machine;
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
  CHECK(fixture.progress.completed && fixture.progress.operations == 9);
  CHECK(fixture.progress.reads == 2 && fixture.progress.stores == 6 &&
        fixture.progress.accesses == 8);
  CHECK(fixture.progress.callbacks_completed == 1);
  CHECK(fixture.progress.call_attempts[NBA97_GAME_MATCH_BUFFER_REWIND_ZERO] ==
            1 &&
        fixture.progress.call_count[NBA97_GAME_MATCH_BUFFER_REWIND_ZERO] == 1);
  CHECK(fixture.calls.size() == 1);
  CHECK(fixture.calls[0].pc == 0x80076af8u &&
        fixture.calls[0].delay_slot_pc == 0x80076afcu &&
        fixture.calls[0].entry == 0x800a3a74u &&
        fixture.calls[0].operation == 5 && fixture.calls[0].invocation == 1 &&
        fixture.calls[0].kind == NBA97_GAME_MATCH_BUFFER_REWIND_ZERO &&
        fixture.calls[0].argument_count == 2);
  CHECK(fixture.journal[0].pc == 0x80076ad4u &&
        fixture.journal[0].address == 0x800fa004u &&
        fixture.journal[0].kind == NBA97_GAME_MATCH_BUFFER_REWIND_READ);
  const U32 pcs[8] = {0x80076ad4u, 0x80076ae4u, 0x80076aecu, 0x80076af4u,
                      0x80076b04u, 0x80076b0cu, 0x80076b14u, 0x80076b18u};
  const U32 addresses[8] = {0x800fa004u, Fixture::Stack - 8, 0x800fa00cu,
                            0x800fa010u, 0x800fe860u,        0x8002148cu,
                            0x800fe864u, Fixture::Stack - 8};
  const std::size_t operations[8] = {1, 2, 3, 4, 6, 7, 8, 9};
  for (unsigned i = 0; i < 8; ++i) {
    CHECK(fixture.journal[i].pc == pcs[i]);
    CHECK(fixture.journal[i].address == addresses[i]);
    CHECK(fixture.journal[i].operation == operations[i]);
  }
  CHECK(fixture.get(0x800fa00cu) == 0x800f1918u &&
        fixture.get(0x800fa010u) == 0x800f1918u);
  CHECK(fixture.get(0x800f1918u) == 0);
  CHECK(fixture.get(0x800fe860u) == 0 && fixture.get(0x8002148cu, 2) == 0 &&
        fixture.get(0x800fe864u, 1) == 0);
  CHECK(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
          0x76543210u &&
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
              .known_mask == 5);
  CHECK(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
      Fixture::Stack);
  CHECK(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
      0x81234568u);
  CHECK(fixture.progress.machine.hi.word == entry.hi.word &&
        fixture.progress.machine.hi.known_mask == entry.hi.known_mask &&
        fixture.progress.machine.lo.word == entry.lo.word &&
        fixture.progress.machine.lo.known_mask == entry.lo.known_mask);
  for (unsigned reg = 0; reg < 32; ++reg) {
    bool source_or_child_mutates =
        reg == NBA97_MATCH_INITIALIZE_AT || reg == NBA97_MATCH_INITIALIZE_V0 ||
        reg == NBA97_MATCH_INITIALIZE_A0 || reg == NBA97_MATCH_INITIALIZE_A1 ||
        reg == NBA97_MATCH_INITIALIZE_SP || reg == NBA97_MATCH_INITIALIZE_RA;
    if (!source_or_child_mutates)
      CHECK(sameWord(fixture.progress.machine.registers.gpr[reg],
                     entry.registers.gpr[reg]));
  }
}

void budgets_and_pointer_masks() {
  const U32 stopped[9] = {0x80076ad4u, 0x80076ae4u, 0x80076aecu,
                          0x80076af4u, 0x80076af8u, 0x80076b04u,
                          0x80076b0cu, 0x80076b14u, 0x80076b18u};
  for (std::size_t budget = 0; budget < 9; ++budget) {
    Fixture fixture;
    fixture.context.operation_budget = budget;
    CHECK(fixture.run() == NBA97_TEXT_LIMIT);
    CHECK(fixture.progress.operations == budget);
    CHECK(fixture.progress.stopped_pc == stopped[budget]);
    CHECK(fixture.progress.access_events == budget - (budget > 4 ? 1 : 0));
    CHECK(fixture.calls.size() == (budget >= 5 ? 1u : 0u));
    if (budget == 4) {
      CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                .word == 0x80076b00u);
      CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
                .word == 4);
    }
  }
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture fixture;
    for (unsigned byte = 0; byte < 4; ++byte)
      fixture.known[0x800fa004u - Fixture::Base + byte] = (mask >> byte) & 1u;
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    for (unsigned byte = 0; byte < 4; ++byte) {
      CHECK(fixture.known[0x800fa00cu - Fixture::Base + byte] ==
            ((mask >> byte) & 1u));
      CHECK(fixture.known[0x800fa010u - Fixture::Base + byte] ==
            ((mask >> byte) & 1u));
    }
  }
}

void callback_prefixes_and_live_machine() {
  Fixture refused;
  refused.callback_mode = 1;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED);
  CHECK(refused.progress.stopped_pc == 0x80076af8u &&
        refused.progress.stopped_entry == 0x800a3a74u);
  CHECK(refused.progress.operations == 5 && refused.progress.stores == 3);

  for (int mode = 2; mode <= 4; ++mode) {
    Fixture malformed;
    malformed.callback_mode = mode;
    CHECK(malformed.run() == NBA97_TEXT_ARGUMENT);
    CHECK(malformed.progress.stopped_pc == 0x80076af8u);
    if (mode == 2)
      CHECK(malformed.progress.machine.registers.gpr[9].known_mask == 16);
    else if (mode == 3)
      CHECK(malformed.progress.machine.hi.known_mask == 16);
    else
      CHECK(malformed.progress.machine.lo.known_mask == 16);
  }

  Fixture late_malformed;
  late_malformed.callback_mode = 6;
  CHECK(late_malformed.run() == NBA97_TEXT_ARGUMENT);
  CHECK(late_malformed.progress.stopped_pc == 0x80076b18u);
  CHECK(late_malformed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
            .word == 0x80076b00u);
  CHECK(late_malformed.get(0x800fe860u) == 0 &&
        late_malformed.get(0x8002148cu, 2) == 0 &&
        late_malformed.get(0x800fe864u, 1) == 0);

  Fixture live;
  live.callback_mode = 5;
  CHECK(live.run() == NBA97_TEXT_COMPLETE);
  CHECK(live.progress.frame_stack_pointer == Fixture::Stack - 0x18);
  CHECK(live.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        live.callback_stack + 0x18);
  CHECK(live.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        0x82345678u);
  CHECK(live.progress.machine.hi.word == 0x55550020u &&
        live.progress.machine.hi.known_mask == 6 &&
        live.progress.machine.lo.word == 0x55550021u &&
        live.progress.machine.lo.known_mask == 9);
  for (unsigned reg = 1; reg < 32; ++reg) {
    if (reg == NBA97_MATCH_INITIALIZE_AT || reg == NBA97_MATCH_INITIALIZE_SP ||
        reg == NBA97_MATCH_INITIALIZE_RA)
      continue;
    CHECK(live.progress.machine.registers.gpr[reg].word == 0x55000000u + reg);
    CHECK(live.progress.machine.registers.gpr[reg].known_mask == (reg & 15));
  }
}

void return_masks_failures_and_aliases() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture fixture;
    fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = static_cast<std::uint8_t>(mask);
    int result = fixture.run();
    CHECK(result == (mask == 15 ? NBA97_TEXT_COMPLETE : NBA97_TEXT_UNKNOWN));
    if (mask != 15)
      CHECK(fixture.progress.stopped_pc == 0x80076b20u);
    CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
              .word == Fixture::Stack);
  }

  Fixture stack_unknown;
  stack_unknown.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
      .known_mask = 14;
  CHECK(stack_unknown.run() == NBA97_TEXT_UNKNOWN);
  CHECK(stack_unknown.progress.stopped_pc == 0x80076ae4u);

  Fixture stack_unaligned;
  stack_unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
      .word += 1;
  CHECK(stack_unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  CHECK(stack_unaligned.progress.stopped_pc == 0x80076ae4u);

  Fixture return_unaligned;
  return_unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .word |= 1;
  CHECK(return_unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  CHECK(return_unaligned.progress.stopped_pc == 0x80076b20u);
  CHECK(
      return_unaligned.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
          .word == Fixture::Stack);

  Fixture unmapped;
  unmapped.region.size = 0x100;
  CHECK(unmapped.run() == NBA97_TEXT_RESOURCE);
  CHECK(unmapped.progress.stopped_pc == 0x80076ad4u);

  Fixture alias;
  alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800fa014u,
                                                                    15};
  alias.put(0x800fa004u, 0x81234000u);
  CHECK(alias.run() == NBA97_TEXT_COMPLETE);
  CHECK(alias.progress.restored_return_address.word == 0x81234000u);
  CHECK(alias.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        0x81234000u);

  std::array<std::uint8_t, 0x20> low{};
  std::array<std::uint8_t, 0x20> low_known{};
  low_known.fill(1);
  Nba97GameTextRegion low_region{0, low.data(), low_known.data(), low.size()};
  Fixture wrap;
  Nba97GameTextRegion regions[2] = {low_region, wrap.region};
  wrap.context.memory = {regions, 2};
  wrap.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {8, 15};
  wrap.context.operation_budget = 2;
  CHECK(wrap.run() == NBA97_TEXT_LIMIT);
  CHECK(wrap.journal[1].address == 0 && wrap.journal[1].pc == 0x80076ae4u);

  std::array<std::uint8_t, 0x20> complete_low{};
  std::array<std::uint8_t, 0x20> complete_low_known{};
  complete_low_known.fill(1);
  Nba97GameTextRegion complete_low_region{
      0, complete_low.data(), complete_low_known.data(), complete_low.size()};
  Fixture wrap_complete;
  Nba97GameTextRegion complete_regions[2] = {complete_low_region,
                                             wrap_complete.region};
  wrap_complete.context.memory = {complete_regions, 2};
  wrap_complete.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {8,
                                                                            15};
  CHECK(wrap_complete.run() == NBA97_TEXT_COMPLETE);
  CHECK(wrap_complete.progress.frame_stack_pointer == 0xfffffff0u);
  CHECK(wrap_complete.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
            .word == 8);
  CHECK(wrap_complete.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
            .word == 0x81234568u);
}

void deterministic_repeat() {
  Fixture first;
  Fixture second;
  CHECK(first.run() == NBA97_TEXT_COMPLETE);
  CHECK(second.run() == NBA97_TEXT_COMPLETE);
  CHECK(first.bytes == second.bytes);
  CHECK(first.known == second.known);
  CHECK(sameMachine(first.progress.machine, second.progress.machine));
  CHECK(first.progress.operations == second.progress.operations &&
        first.progress.reads == second.progress.reads &&
        first.progress.stores == second.progress.stores &&
        first.progress.access_events == second.progress.access_events);
}

void malformed_and_unknown_store_atomicity() {
  Fixture malformed;
  auto incoming_v0 = malformed.context.machine.registers.gpr[2];
  malformed.known[0x800fa004u - Fixture::Base + 3] = 2;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT);
  CHECK(malformed.progress.stopped_pc == 0x80076ad4u);
  CHECK(malformed.progress.machine.registers.gpr[2].word == 0x80100000u &&
        malformed.progress.machine.registers.gpr[2].known_mask == 15);
  CHECK(!sameWord(malformed.progress.machine.registers.gpr[2], incoming_v0));

  std::array<std::uint8_t, 4> source_data{0x18, 0x19, 0x0f, 0x80};
  std::array<std::uint8_t, 4> source_known{0, 1, 1, 1};
  std::array<std::uint8_t, 4> stack_data{};
  std::array<std::uint8_t, 8> destination_data{};
  Nba97GameTextRegion regions[3] = {
      {0x800fa004u, source_data.data(), source_known.data(),
       source_data.size()},
      {Fixture::Stack - 8, stack_data.data(), nullptr, stack_data.size()},
      {0x800fa00cu, destination_data.data(), nullptr, destination_data.size()}};
  Nba97GameMatchBufferRewindContext context{};
  for (unsigned i = 0; i < 32; ++i)
    context.machine.registers.gpr[i] = {0x66000000u + i, 15};
  context.machine.registers.gpr[0] = {0, 15};
  context.machine.registers.gpr[29] = {Fixture::Stack, 15};
  context.machine.registers.gpr[31] = {0x81234568u, 15};
  context.memory = {regions, 3};
  context.operation_budget = 10;
  Nba97GameMatchBufferRewindProgress progress{};
  CHECK(nba97_game_match_buffer_rewind(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(progress.stopped_pc == 0x80076aecu);
  for (auto byte : destination_data)
    CHECK(byte == 0);
}

void argument_guards() {
  Nba97GameMatchBufferRewindProgress progress{};
  CHECK(nba97_game_match_buffer_rewind(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
  Fixture fixture;
  CHECK(nba97_game_match_buffer_rewind(&fixture.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);
  fixture.context.machine.registers.gpr[0].word = 1;
  CHECK(fixture.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_hi;
  bad_hi.context.machine.hi.known_mask = 16;
  CHECK(bad_hi.run() == NBA97_TEXT_ARGUMENT);
  Fixture bad_journal;
  bad_journal.context.access_journal = nullptr;
  CHECK(bad_journal.run() == NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  normal_and_exact_order();
  budgets_and_pointer_masks();
  callback_prefixes_and_live_machine();
  return_masks_failures_and_aliases();
  deterministic_repeat();
  malformed_and_unknown_store_atomicity();
  argument_guards();
  std::printf("game match buffer rewind tests passed (%u checks)\n", checks);
}
