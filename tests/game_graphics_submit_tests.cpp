#include "recovered/game_graphics_submit.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
using U32 = std::uint32_t;
unsigned checks;
void ck(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "graphics submit check %u line %u\n", checks, line);
    std::exit(1);
  }
}
#define CHECK(value) ck((value), __LINE__)

struct Seen {
  Nba97GameGraphicsSubmitEvent event{};
  Nba97GameGraphicsSubmitMachine machine{};
};

struct Fixture {
  static constexpr U32 Base = 0x80000000u;
  static constexpr std::size_t Size = 0x110000u;
  static constexpr U32 Stack = 0x8010f000u;
  static constexpr U32 Data = 0x80020000u;
  static constexpr U32 Function = 0x80030000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameGraphicsSubmitAccess, 256> journal{};
  Nba97GameGraphicsSubmitContext context{};
  Nba97GameGraphicsSubmitProgress progress{};
  std::vector<Seen> seen;
  int wait_result = 1;
  bool drain_space = false;
  bool mutate_indirect = false;
  bool refuse = false;
  bool partial_final_difference = false;
  int invalidate_eligibility = 0;
  int malformed = 0;
  U32 critical_result = 0x1234u;

  explicit Fixture(int count = 8) {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x11000000u + i, 15};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[4] = {Function, 15};
    context.machine.registers.gpr[5] = {Data, 15};
    context.machine.registers.gpr[6] = {static_cast<U32>(count), 15};
    context.machine.registers.gpr[7] = {0xabcdef01u, 15};
    context.machine.registers.gpr[29] = {Stack, 15};
    context.machine.registers.gpr[31] = {0x81234568u, 15};
    context.machine.hi = {0x12345678u, 15};
    context.machine.lo = {0x87654321u, 15};
    context.memory = {&region, 1};
    context.operation_budget = 300;
    context.io = io;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    put(0x800c56c4, 0);
    put(0x800c56c8, 0);
    put(0x800c55c1, 0, 1);
    put(0x800c56a0, 0x80030008);
    put(0x80030008, 0);
    put(0x800c55cc, 0);
    put(0x800c5694, 0x80030004);
    put(0x80030004, 0x04000000);
    put(Data, 0x11112222);
    put(Data + 4, 0x33334444);
  }

  void put(U32 address, U32 value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i) {
      bytes[address - Base + i] = static_cast<std::uint8_t>(value >> (8 * i));
      known[address - Base + i] = 1;
    }
  }
  void mask(U32 address, std::uint8_t value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i)
      known[address - Base + i] = static_cast<std::uint8_t>((value >> i) & 1u);
  }
  U32 get(U32 address, unsigned width = 4) const {
    U32 value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= U32(bytes[address - Base + i]) << (8 * i);
    return value;
  }

  static int io(void *opaque, const Nba97GameTextMemory *,
                const Nba97GameGraphicsSubmitEvent *event,
                Nba97GameGraphicsSubmitMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.seen.push_back({*event, *machine});
    if (event->kind == NBA97_GAME_GRAPHICS_SUBMIT_WAIT)
      machine->registers.gpr[2] = {static_cast<U32>(fixture.wait_result), 15};
    if (event->kind == NBA97_GAME_GRAPHICS_SUBMIT_DRAIN && fixture.drain_space)
      fixture.put(0x800c56c8, 2);
    if (event->kind == NBA97_GAME_GRAPHICS_SUBMIT_CRITICAL &&
        event->pc == 0x8009b304) {
      machine->registers.gpr[2] = {fixture.critical_result, 15};
      if (fixture.invalidate_eligibility == 1)
        fixture.mask(0x800c56c4, 14);
      else if (fixture.invalidate_eligibility == 2)
        fixture.mask(0x80030008, 7);
      else if (fixture.invalidate_eligibility == 3)
        fixture.mask(0x800c55cc, 14);
    }
    if (event->kind == NBA97_GAME_GRAPHICS_SUBMIT_DRAIN &&
        event->pc == 0x8009b538 && fixture.partial_final_difference) {
      fixture.put(0x800c56c4, 0x100);
      fixture.mask(0x800c56c4, 14);
      fixture.put(0x800c56c8, 0);
    }
    if (event->kind == NBA97_GAME_GRAPHICS_SUBMIT_INDIRECT &&
        fixture.mutate_indirect) {
      constexpr U32 NewFrame = 0x8010e000u;
      machine->registers.gpr[16] = {0xaaa00010u, 15};
      machine->registers.gpr[17] = {0xaaa00011u, 15};
      machine->registers.gpr[18] = {0xaaa00012u, 15};
      machine->registers.gpr[19] = {0xaaa00013u, 15};
      machine->registers.gpr[29] = {NewFrame, 15};
      machine->hi = {0xface0001u, 15};
      machine->lo = {0xface0002u, 15};
      fixture.put(NewFrame + 16, 0xbbbb0010u);
      fixture.put(NewFrame + 20, 0xbbbb0011u);
      fixture.put(NewFrame + 24, 0xbbbb0012u);
      fixture.put(NewFrame + 28, 0xbbbb0013u);
      fixture.put(NewFrame + 32, 0x81230004u);
    }
    if (fixture.refuse)
      return 0;
    if (fixture.malformed == 1) {
      machine->registers.gpr[9].word = 0xfeed0009u;
      machine->registers.gpr[9].known_mask = 16;
    } else if (fixture.malformed == 2) {
      machine->hi.word = 0xfeed0010u;
      machine->hi.known_mask = 16;
    } else if (fixture.malformed == 3) {
      machine->lo.word = 0xfeed0011u;
      machine->lo.known_mask = 16;
    } else if (fixture.malformed == 4) {
      machine->registers.gpr[0].known_mask = 14;
    }
    return 1;
  }
  int run() { return nba97_game_graphics_submit(&context, &progress); }
};

void direct_path() {
  Fixture f;
  CHECK(f.run() == NBA97_TEXT_COMPLETE);
  CHECK(f.progress.completed && !f.progress.queued);
  CHECK(f.progress.return_v0.word == 0 &&
        f.progress.return_v0.known_mask == 15);
  const U32 pcs[] = {0x8009b2bc, 0x8009b304, 0x8009b3a8, 0x8009b3d4};
  CHECK(f.seen.size() == std::size(pcs));
  for (unsigned i = 0; i < std::size(pcs); ++i) {
    CHECK(f.seen[i].event.pc == pcs[i]);
    CHECK(f.seen[i].machine.registers.gpr[31].word == pcs[i] + 8);
  }
  CHECK(f.seen[0].machine.registers.gpr[18].word == 0xabcdef01u);
  for (unsigned i = 1; i < 32; ++i)
    if (i != 16 && i != 17 && i != 18 && i != 19 && i != 29 && i != 31)
      CHECK(f.seen[0].machine.registers.gpr[i].word ==
            f.context.machine.registers.gpr[i].word);
  CHECK(f.seen[0].machine.hi.word == 0x12345678u);
  CHECK(f.seen[0].machine.lo.word == 0x87654321u);
  CHECK(f.seen[2].event.entry == Fixture::Function);
  CHECK(f.seen[2].machine.registers.gpr[4].word == Fixture::Data);
  CHECK(f.seen[2].machine.registers.gpr[5].word == 0xabcdef01u);
  CHECK(f.get(0x800c55c8) == 1 && f.get(0x800c56cc) == 0x1234);
  CHECK(f.get(0x800c56b4) == Fixture::Function);
  CHECK(f.get(0x800c56b8) == Fixture::Data);
  CHECK(f.get(0x800c56bc) == 0xabcdef01u);
  CHECK(f.progress.machine.registers.gpr[29].word == Fixture::Stack);
  CHECK(f.progress.machine.registers.gpr[31].word == 0x81234568u);
}

void queued_path() {
  Fixture f;
  f.put(0x800c55c1, 1, 1);
  f.put(0x800c56c4, 1);
  f.put(0x800c56c8, 0);
  CHECK(f.run() == NBA97_TEXT_COMPLETE);
  CHECK(f.progress.queued && f.progress.return_v0.word == 2);
  CHECK(f.progress.copy_iterations == 2);
  U32 queue = 0x80104748u + 96;
  CHECK(f.get(queue) == Fixture::Function && f.get(queue + 4) == queue + 12);
  CHECK(f.get(queue + 8) == 0xabcdef01u);
  CHECK(f.get(queue + 12) == 0x11112222);
  CHECK(f.get(queue + 16) == 0x33334444);
  CHECK(f.get(0x800c56c4) == 2);
  const U32 pcs[] = {0x8009b2bc, 0x8009b304, 0x8009b3ec, 0x8009b530,
                     0x8009b538};
  CHECK(f.seen.size() == std::size(pcs));
  for (unsigned i = 0; i < std::size(pcs); ++i)
    CHECK(f.seen[i].event.pc == pcs[i]);
  CHECK(f.seen[2].machine.registers.gpr[4].word == 2);
  CHECK(f.seen[2].machine.registers.gpr[5].word == 0x8009b57cu);

  for (int count : {0, 1, 3, 4, 5, -1, static_cast<int>(0x80000000u)}) {
    Fixture item(count);
    item.put(0x800c55c1, 1, 1);
    item.put(0x800c56c4, 2);
    item.put(0x800c56c8, 0);
    CHECK(item.run() == NBA97_TEXT_COMPLETE);
    unsigned words = count > 0 ? static_cast<unsigned>(count) / 4 : 0;
    CHECK(item.progress.copy_iterations == words);
    CHECK(item.get(0x8010474cu + 2 * 96) ==
          (count ? 0x80104754u + 2 * 96 : Fixture::Data));
  }

  Fixture wrap(0);
  wrap.put(0x800c55c1, 1, 1);
  wrap.put(0x800c56c4, 63);
  wrap.put(0x800c56c8, 62);
  CHECK(wrap.run() == NBA97_TEXT_COMPLETE);
  CHECK(wrap.get(0x800c56c4) == 0);
  CHECK(wrap.progress.return_v0.word == 2);
}

void queue_conditions() {
  Fixture all_clear;
  all_clear.critical_result = 0;
  all_clear.put(0x800c55c1, 1, 1);
  CHECK(all_clear.run() == NBA97_TEXT_COMPLETE && !all_clear.progress.queued);
  Fixture dma_busy;
  dma_busy.critical_result = 0;
  dma_busy.put(0x800c55c1, 1, 1);
  dma_busy.put(0x80030008, 0x01000000);
  CHECK(dma_busy.run() == NBA97_TEXT_COMPLETE && dma_busy.progress.queued);
  Fixture critical_busy;
  critical_busy.put(0x800c55c1, 1, 1);
  critical_busy.put(0x800c55cc, 1);
  CHECK(critical_busy.run() == NBA97_TEXT_COMPLETE &&
        critical_busy.progress.queued);
}

void full_queue_and_gpu_budget() {
  Fixture failed;
  failed.put(0x800c56c4, 0);
  failed.put(0x800c56c8, 1);
  CHECK(failed.run() == NBA97_TEXT_COMPLETE);
  CHECK(failed.progress.return_v0.word == 0xffffffffu);
  CHECK(failed.progress.full_queue_iterations == 1);
  Fixture drained;
  drained.put(0x800c56c4, 0);
  drained.put(0x800c56c8, 1);
  drained.wait_result = 0;
  drained.drain_space = true;
  CHECK(drained.run() == NBA97_TEXT_COMPLETE);
  CHECK(drained.progress.full_queue_iterations == 1);
  CHECK(drained.seen[1].event.pc == 0x8009b2cc);
  CHECK(drained.seen[2].event.pc == 0x8009b2dc);
  CHECK(drained.seen[1].machine.registers.gpr[31].word == 0x8009b2d4u);
  CHECK(drained.seen[2].machine.registers.gpr[31].word == 0x8009b2e4u);

  Fixture busy;
  busy.put(0x80030004, 0);
  busy.context.operation_budget = 18;
  CHECK(busy.run() == NBA97_TEXT_LIMIT);
  CHECK(busy.progress.stopped_pc == 0x8009b390);
  CHECK(busy.progress.gpu_poll_iterations == 6);
  unsigned pointer_reads = 0, status_reads = 0;
  for (std::size_t i = 0; i < busy.progress.access_events; ++i) {
    pointer_reads += busy.journal[i].pc == 0x8009b388;
    status_reads += busy.journal[i].pc == 0x8009b390;
  }
  CHECK(pointer_reads == 1 && status_reads == 5);

  for (std::size_t budget = 0; budget <= 29; ++budget) {
    Fixture item;
    item.put(0x800c56c4, 0);
    item.put(0x800c56c8, 1);
    item.wait_result = 0;
    item.drain_space = true;
    item.context.operation_budget = budget;
    int result = item.run();
    CHECK(item.progress.operations == budget);
    CHECK(result == (budget < 29 ? NBA97_TEXT_LIMIT : NBA97_TEXT_COMPLETE));
  }
  for (std::size_t budget = 0; budget <= 40; ++budget) {
    Fixture item;
    item.put(0x800c55c1, 1, 1);
    item.put(0x800c56c4, 1);
    item.put(0x800c56c8, 0);
    item.context.operation_budget = budget;
    int result = item.run();
    CHECK(item.progress.operations == budget);
    CHECK(result == (budget < 40 ? NBA97_TEXT_LIMIT : NBA97_TEXT_COMPLETE));
  }
}

void partial_knownness() {
  Fixture stack;
  stack.context.machine.registers.gpr[29].known_mask = 14;
  CHECK(stack.run() == NBA97_TEXT_UNKNOWN);
  CHECK(stack.progress.stopped_pc == 0x8009b29c);
  CHECK(stack.progress.machine.registers.gpr[29].known_mask == 12);

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture item(4);
    item.put(0x800c55c1, 1, 1);
    item.put(0x800c56c4, 1);
    item.put(0x800c56c8, 0);
    item.context.machine.registers.gpr[6].known_mask =
        static_cast<std::uint8_t>(mask);
    int result = item.run();
    if (!(mask & 1)) {
      CHECK(result == NBA97_TEXT_UNKNOWN &&
            item.progress.stopped_pc == 0x8009b3f4);
      CHECK(item.progress.machine.registers.gpr[6].word == 0);
    } else if (!(mask & 8)) {
      CHECK(result == NBA97_TEXT_UNKNOWN &&
            item.progress.stopped_pc == 0x8009b40c);
      CHECK(item.progress.machine.registers.gpr[2].known_mask == mask);
    } else if (mask != 15) {
      CHECK(result == NBA97_TEXT_UNKNOWN &&
            item.progress.stopped_pc == 0x8009b420);
      CHECK(item.progress.copy_iterations == (mask == 11 ? 1u : 0u));
      CHECK(item.progress.machine.registers.gpr[2].known_mask == 14);
      CHECK(item.progress.machine.registers.gpr[4].word ==
            (mask == 11 ? 4u : 0u));
      CHECK(item.progress.machine.registers.gpr[4].known_mask == 15);
    } else {
      CHECK(result == NBA97_TEXT_COMPLETE &&
            item.progress.copy_iterations == 1);
    }
  }

  Fixture flag;
  flag.mask(0x800c55c1, 0, 1);
  CHECK(flag.run() == NBA97_TEXT_UNKNOWN);
  CHECK(flag.progress.stopped_pc == 0x8009b32c);
  CHECK(flag.progress.machine.registers.gpr[4].word == 0x04000000u);

  for (int predicate = 1; predicate <= 3; ++predicate) {
    Fixture item;
    item.critical_result = 0;
    item.put(0x800c55c1, 1, 1);
    item.invalidate_eligibility = predicate;
    CHECK(item.run() == NBA97_TEXT_UNKNOWN);
    const U32 expected[] = {0, 0x8009b348u, 0x8009b368u, 0x8009b37cu};
    CHECK(item.progress.stopped_pc == expected[predicate]);
  }

  Fixture difference(0);
  difference.put(0x800c55c1, 1, 1);
  difference.put(0x800c56c4, 1);
  difference.put(0x800c56c8, 0);
  difference.partial_final_difference = true;
  CHECK(difference.run() == NBA97_TEXT_COMPLETE);
  CHECK(difference.progress.return_v0.word == 0);
  CHECK(difference.progress.return_v0.known_mask == 14);
}

void callback_and_epilogue_prefixes() {
  Fixture live;
  live.mutate_indirect = true;
  CHECK(live.run() == NBA97_TEXT_COMPLETE);
  CHECK(live.get(0x800c56b4) == 0xaaa00013u);
  CHECK(live.get(0x800c56b8) == 0xaaa00010u);
  CHECK(live.get(0x800c56bc) == 0xaaa00012u);
  CHECK(live.progress.machine.registers.gpr[16].word == 0xbbbb0010u);
  CHECK(live.progress.machine.registers.gpr[19].word == 0xbbbb0013u);
  CHECK(live.progress.machine.registers.gpr[29].word == 0x8010e028u);
  CHECK(live.progress.machine.registers.gpr[31].word == 0x81230004u);
  CHECK(live.progress.machine.hi.word == 0xface0001u);
  CHECK(live.progress.machine.lo.word == 0xface0002u);

  for (int malformed = 1; malformed <= 4; ++malformed) {
    Fixture item;
    item.malformed = malformed;
    CHECK(item.run() == NBA97_TEXT_ARGUMENT);
    CHECK(item.progress.stopped_pc == 0x8009b2bc);
    if (malformed == 1) {
      CHECK(item.progress.machine.registers.gpr[9].word == 0xfeed0009u);
      CHECK(item.progress.machine.registers.gpr[9].known_mask == 16);
    } else if (malformed == 2) {
      CHECK(item.progress.machine.hi.word == 0xfeed0010u &&
            item.progress.machine.hi.known_mask == 16);
    } else if (malformed == 3) {
      CHECK(item.progress.machine.lo.word == 0xfeed0011u &&
            item.progress.machine.lo.known_mask == 16);
    } else {
      CHECK(item.progress.machine.registers.gpr[0].word == 0 &&
            item.progress.machine.registers.gpr[0].known_mask == 14);
    }
  }
  Fixture refused;
  refused.refuse = true;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED);
  CHECK(refused.progress.stopped_pc == 0x8009b2bc);
  Fixture no_io;
  no_io.context.io = nullptr;
  CHECK(no_io.run() == NBA97_TEXT_IO_REFUSED &&
        no_io.progress.stopped_pc == 0x8009b2bc);
  Fixture unknown_target;
  unknown_target.context.machine.registers.gpr[4].known_mask = 14;
  CHECK(unknown_target.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_target.progress.stopped_pc == 0x8009b3a8);
  CHECK(unknown_target.progress.machine.registers.gpr[5].word == 0xabcdef01u);
  Fixture misaligned_target;
  misaligned_target.context.machine.registers.gpr[4].word += 2;
  CHECK(misaligned_target.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned_target.progress.stopped_pc == 0x8009b3a8);
  Fixture unknown_ra;
  unknown_ra.context.machine.registers.gpr[31].known_mask = 14;
  CHECK(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
        unknown_ra.progress.stopped_pc == 0x8009b574);
}

void memory_failures_and_aliases() {
  Fixture alignment;
  ++alignment.context.machine.registers.gpr[29].word;
  CHECK(alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        alignment.progress.stopped_pc == 0x8009b29c);
  Fixture unmapped_stack;
  unmapped_stack.context.machine.registers.gpr[29] = {0x80200000u, 15};
  CHECK(unmapped_stack.run() == NBA97_TEXT_RESOURCE &&
        unmapped_stack.progress.stopped_pc == 0x8009b29c);
  Fixture unmapped_gpu;
  unmapped_gpu.put(0x800c5694, 0x80200000u);
  CHECK(unmapped_gpu.run() == NBA97_TEXT_RESOURCE &&
        unmapped_gpu.progress.stopped_pc == 0x8009b390);
  Fixture unmapped_source(4);
  unmapped_source.put(0x800c55c1, 1, 1);
  unmapped_source.put(0x800c56c4, 1);
  unmapped_source.put(0x800c56c8, 0);
  unmapped_source.context.machine.registers.gpr[5] = {0x80200000u, 15};
  CHECK(unmapped_source.run() == NBA97_TEXT_RESOURCE &&
        unmapped_source.progress.stopped_pc == 0x8009b428);
  Fixture unaligned_source(4);
  unaligned_source.put(0x800c55c1, 1, 1);
  unaligned_source.put(0x800c56c4, 1);
  unaligned_source.put(0x800c56c8, 0);
  unaligned_source.context.machine.registers.gpr[5].word++;
  CHECK(unaligned_source.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned_source.progress.stopped_pc == 0x8009b428);

  Fixture malformed_load(4);
  malformed_load.put(0x800c55c1, 1, 1);
  malformed_load.put(0x800c56c4, 1);
  malformed_load.put(0x800c56c8, 0);
  malformed_load.known[Fixture::Data - Fixture::Base + 3] = 2;
  CHECK(malformed_load.run() == NBA97_TEXT_ARGUMENT &&
        malformed_load.progress.stopped_pc == 0x8009b428);
  CHECK(malformed_load.progress.machine.registers.gpr[5].word == 0x8009b57cu);
  Fixture invalid_byte(0);
  invalid_byte.put(0x800c55c1, 1, 1);
  invalid_byte.put(0x800c56c4, 1);
  invalid_byte.put(0x800c56c8, 0);
  U32 pointer_slot = 0x8010474cu + 96;
  U32 before = invalid_byte.get(pointer_slot);
  invalid_byte.known[pointer_slot - Fixture::Base + 3] = 2;
  CHECK(invalid_byte.run() == NBA97_TEXT_ARGUMENT &&
        invalid_byte.progress.stopped_pc == 0x8009b4c4);
  CHECK(invalid_byte.get(pointer_slot) == before);
  Fixture immutable(0);
  immutable.region.known = nullptr;
  immutable.context.machine.registers.gpr[7].known_mask = 14;
  immutable.put(0x800c55c1, 1, 1);
  immutable.put(0x800c56c4, 1);
  immutable.put(0x800c56c8, 0);
  U32 argument_slot = 0x80104750u + 96;
  U32 immutable_before = immutable.get(argument_slot);
  CHECK(immutable.run() == NBA97_TEXT_ARGUMENT &&
        immutable.progress.stopped_pc == 0x8009b4e8);
  CHECK(immutable.get(argument_slot) == immutable_before);

  Fixture queue_alias(8);
  queue_alias.put(0x800c55c1, 1, 1);
  queue_alias.put(0x800c56c4, 1);
  queue_alias.put(0x800c56c8, 0);
  U32 copy_slot = 0x80104754u + 96;
  queue_alias.context.machine.registers.gpr[5] = {copy_slot, 15};
  queue_alias.put(copy_slot, 0xcafe0001u);
  queue_alias.put(copy_slot + 4, 0xcafe0002u);
  CHECK(queue_alias.run() == NBA97_TEXT_COMPLETE);
  CHECK(queue_alias.get(copy_slot) == 0xcafe0001u &&
        queue_alias.get(copy_slot + 4) == 0xcafe0002u);
  std::size_t first_copy = queue_alias.progress.access_events;
  for (std::size_t i = 0; i + 2 < queue_alias.progress.access_events; ++i)
    if (queue_alias.journal[i].pc == 0x8009b428) {
      first_copy = i;
      break;
    }
  CHECK(first_copy + 2 < queue_alias.progress.access_events);
  CHECK(queue_alias.journal[first_copy].pc == 0x8009b428);
  CHECK(queue_alias.journal[first_copy + 1].pc == 0x8009b434);
  CHECK(queue_alias.journal[first_copy + 2].pc == 0x8009b450);

  Fixture global_alias(8);
  global_alias.put(0x800c55c1, 1, 1);
  global_alias.put(0x800c56c4, 1);
  global_alias.put(0x800c56c8, 0);
  global_alias.context.machine.registers.gpr[5] = {0x800c56c4u, 15};
  CHECK(global_alias.run() == NBA97_TEXT_COMPLETE);
  CHECK(global_alias.get(copy_slot) == 1);
  CHECK(global_alias.get(copy_slot + 4) == 0);
  Fixture alias;
  alias.context.machine.registers.gpr[29] = {0x800c56d4u, 15};
  alias.context.machine.registers.gpr[19] = {0, 15};
  CHECK(alias.run() == NBA97_TEXT_COMPLETE);
  CHECK(alias.progress.machine.registers.gpr[31].word == 0x1234u);
  Fixture short_journal;
  short_journal.context.access_journal_capacity = 1;
  CHECK(short_journal.run() == NBA97_TEXT_COMPLETE);
  CHECK(short_journal.progress.access_events > 1);
  Fixture first;
  Fixture second;
  CHECK(first.run() == second.run() && first.bytes == second.bytes);
  CHECK(std::memcmp(&first.progress.machine, &second.progress.machine,
                    sizeof first.progress.machine) == 0);
}
} // namespace

int main() {
  direct_path();
  queued_path();
  queue_conditions();
  full_queue_and_gpu_budget();
  partial_knownness();
  callback_and_epilogue_prefixes();
  memory_failures_and_aliases();
  std::printf("game graphics submit tests passed (%u checks)\n", checks);
}
