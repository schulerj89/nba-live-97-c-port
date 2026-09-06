#include "recovered/game_match_state_reset.h"

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
    std::fprintf(stderr, "match state reset check %u line %u\n", checks, line);
    std::exit(1);
  }
}
#define CHECK(value) check((value), __LINE__)

struct Seen {
  Nba97GameMatchStateResetEvent event{};
  Nba97GameMatchStateResetMachine machine{};
};

struct Fixture {
  static constexpr U32 Base = 0x80000000u;
  static constexpr std::size_t Size = 0x200000u;
  static constexpr U32 Stack = 0x801ff000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameMatchStateResetAccess, 64> journal{};
  Nba97GameMatchStateResetContext context{};
  Nba97GameMatchStateResetProgress progress{};
  std::vector<Seen> seen;
  U32 refuse_pc = 0;
  int malformed = 0;
  bool mutate_first_zero_s0 = false;
  bool relocate = false;
  bool mutate_second_65820_s0 = false;
  bool partial_second_65820_s0 = false;
  bool invalidate_epilogue = false;

  explicit Fixture(std::uint16_t mode = 98) {
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x11000000u + i, 15};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[29] = {Stack, 15};
    context.machine.registers.gpr[31] = {0x81234568u, 15};
    context.machine.hi = {0x12345678u, 15};
    context.machine.lo = {0x87654321u, 15};
    context.memory = {&region, 1};
    context.operation_budget = 100;
    context.io = io;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    put(0x8001edecu, mode, 2);
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
  void mask(U32 address, std::uint8_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i)
      known[address - Base + i] = static_cast<std::uint8_t>((value >> i) & 1u);
  }

  static int io(void *opaque, const Nba97GameTextMemory *,
                const Nba97GameMatchStateResetEvent *event,
                Nba97GameMatchStateResetMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.seen.push_back({*event, *machine});
    if (event->pc == fixture.refuse_pc)
      return 0;
    if (event->pc == 0x80065a0c && fixture.mutate_first_zero_s0)
      machine->registers.gpr[16] = {0xfffffff0u, 15};
    if (event->pc == 0x80065aa4 && fixture.relocate) {
      constexpr U32 NewFrame = 0x801fe000u;
      machine->registers.gpr[16] = {0x80024000u, 15};
      machine->registers.gpr[17] = {0x80025000u, 15};
      machine->registers.gpr[29] = {NewFrame, 15};
      machine->hi = {0xface0001u, 15};
      machine->lo = {0xface0002u, 15};
      fixture.put(NewFrame + 0x10, 0xbbbb0010u);
      fixture.put(NewFrame + 0x14, 0xbbbb0011u);
      fixture.put(NewFrame + 0x18, 0x81230004u);
    }
    if (event->pc == 0x80065abc && fixture.relocate)
      machine->registers.gpr[17] = {0x80026000u, 15};
    if (event->pc == 0x80065ac4 && fixture.mutate_second_65820_s0)
      machine->registers.gpr[16] = {
          0x80027000u,
          static_cast<std::uint8_t>(fixture.partial_second_65820_s0 ? 14 : 15)};
    if ((event->pc == 0x80065ae8 || event->pc == 0x80065af8) &&
        fixture.invalidate_epilogue) {
      U32 frame = machine->registers.gpr[29].word;
      fixture.known[frame + 0x18 - Base + 3] = 2;
    }
    if (fixture.malformed == 1) {
      machine->registers.gpr[9] = {0xfeed0009u, 16};
    } else if (fixture.malformed == 2) {
      machine->hi = {0xfeed0010u, 16};
    } else if (fixture.malformed == 3) {
      machine->lo = {0xfeed0011u, 16};
    } else if (fixture.malformed == 4) {
      machine->registers.gpr[0].known_mask = 14;
    }
    return 1;
  }
  int run() { return nba97_game_match_state_reset(&context, &progress); }
};

void normal_mode_98() {
  Fixture fixture;
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
  CHECK(fixture.progress.completed && fixture.progress.mode_98);
  CHECK(fixture.progress.operations == 26 && fixture.progress.accesses == 12);
  CHECK(fixture.progress.spin_iterations == 24);
  CHECK(fixture.get(0x8001edf2, 2) == 0);
  CHECK(fixture.get(0x800fdb9c, 2) == 0xffff);
  CHECK(fixture.get(0x8001eecc, 2) == 5);
  CHECK(fixture.get(0x800fdb54, 2) == 0);
  CHECK(fixture.progress.machine.registers.gpr[29].word == Fixture::Stack);
  CHECK(fixture.progress.machine.registers.gpr[31].word == 0x81234568u);
  CHECK(fixture.progress.machine.registers.gpr[16].word == 0x11000010u);
  CHECK(fixture.progress.machine.registers.gpr[17].word == 0x11000011u);
  CHECK(fixture.progress.machine.hi.word == 0x12345678u);
  CHECK(fixture.progress.machine.lo.word == 0x87654321u);

  const U32 pcs[] = {0x80065a0c, 0x80065a18, 0x80065a24, 0x80065a30, 0x80065a38,
                     0x80065a54, 0x80065a88, 0x80065a94, 0x80065a9c, 0x80065aa4,
                     0x80065abc, 0x80065ac4, 0x80065acc, 0x80065ae8};
  const U32 entries[] = {0x800a3a74, 0x800a3a74, 0x800a3a74, 0x800a3a74,
                         0x80083490, 0x80063d58, 0x800655b0, 0x800655b0,
                         0x80065328, 0x80065db0, 0x80065820, 0x80065820,
                         0x800646a8, 0x80076ad0};
  const unsigned argc[] = {2, 2, 2, 2, 1, 0, 2, 2, 0, 0, 1, 1, 0, 0};
  CHECK(fixture.seen.size() == std::size(pcs));
  for (unsigned i = 0; i < std::size(pcs); ++i) {
    CHECK(fixture.seen[i].event.pc == pcs[i]);
    CHECK(fixture.seen[i].event.delay_slot_pc == pcs[i] + 4);
    CHECK(fixture.seen[i].event.entry == entries[i]);
    CHECK(fixture.seen[i].event.argument_count == argc[i]);
    CHECK(fixture.seen[i].machine.registers.gpr[31].word == pcs[i] + 8);
  }
  CHECK(fixture.seen[0].machine.registers.gpr[4].word == 0x8001f33cu &&
        fixture.seen[0].machine.registers.gpr[5].word == 0x4b0);
  CHECK(fixture.seen[1].machine.registers.gpr[4].word == 0x8001f7ecu &&
        fixture.seen[1].machine.registers.gpr[5].word == 0x1320);
  CHECK(fixture.seen[2].machine.registers.gpr[4].word == 0x8001edf4u &&
        fixture.seen[2].machine.registers.gpr[5].word == 0xc4);
  CHECK(fixture.seen[3].machine.registers.gpr[4].word == 0x8001eeb8u &&
        fixture.seen[3].machine.registers.gpr[5].word == 0xc4);
  CHECK(fixture.seen[4].machine.registers.gpr[4].word == 0);
  CHECK(fixture.seen[6].machine.registers.gpr[4].word == 0x8001edf4u &&
        fixture.seen[6].machine.registers.gpr[5].word == 0x8001eeb8u &&
        fixture.get(0x8001eecc, 2) == 5);
  CHECK(fixture.seen[7].machine.registers.gpr[4].word == 0x8001eeb8u &&
        fixture.seen[7].machine.registers.gpr[5].word == 0x8001edf4u);
  CHECK(fixture.seen[10].machine.registers.gpr[4].word == 0x8001edf4u);
  CHECK(fixture.seen[11].machine.registers.gpr[4].word == 0x8001eeb8u);

  const U32 access_pcs[] = {0x800659f4, 0x80065a08, 0x80065a10, 0x80065a48,
                            0x80065a50, 0x80065a8c, 0x80065ac0, 0x80065ad0,
                            0x80065ad8, 0x80065b00, 0x80065b04, 0x80065b08};
  CHECK(fixture.progress.access_events == std::size(access_pcs));
  for (unsigned i = 0; i < std::size(access_pcs); ++i)
    CHECK(fixture.journal[i].pc == access_pcs[i]);
}

void alternate_mode_and_live_state() {
  Fixture other(7);
  CHECK(other.run() == NBA97_TEXT_COMPLETE && !other.progress.mode_98);
  CHECK(other.seen.back().event.pc == 0x80065af8 &&
        other.seen.back().event.entry == 0x8006432c);

  Fixture live;
  live.relocate = true;
  live.mutate_second_65820_s0 = true;
  CHECK(live.run() == NBA97_TEXT_COMPLETE);
  CHECK(live.seen[10].machine.registers.gpr[4].word == 0x80024000u);
  CHECK(live.seen[11].machine.registers.gpr[4].word == 0x80026000u);
  CHECK(live.get(0x80027000u, 2) == 0);
  CHECK(live.progress.machine.registers.gpr[29].word == 0x801fe020u);
  CHECK(live.progress.machine.registers.gpr[31].word == 0x81230004u);
  CHECK(live.progress.machine.registers.gpr[16].word == 0xbbbb0010u);
  CHECK(live.progress.machine.registers.gpr[17].word == 0xbbbb0011u);
  CHECK(live.progress.machine.hi.word == 0xface0001u);
  CHECK(live.progress.machine.lo.word == 0xface0002u);

  Fixture wrap;
  wrap.mutate_first_zero_s0 = true;
  CHECK(wrap.run() == NBA97_TEXT_COMPLETE);
  CHECK(wrap.seen[1].machine.registers.gpr[4].word == 0x000004a0u);
}

void budget_and_refusal_prefixes() {
  for (std::size_t budget = 0; budget <= 26; ++budget) {
    Fixture fixture;
    fixture.context.operation_budget = budget;
    int result = fixture.run();
    CHECK(fixture.progress.operations == budget);
    CHECK(result == (budget < 26 ? NBA97_TEXT_LIMIT : NBA97_TEXT_COMPLETE));
  }

  Fixture refused;
  refused.refuse_pc = 0x80065a94;
  CHECK(refused.run() == NBA97_TEXT_IO_REFUSED);
  CHECK(refused.progress.stopped_pc == 0x80065a94);
  CHECK(refused.progress.machine.registers.gpr[31].word == 0x80065a9cu);
  CHECK(refused.progress.machine.registers.gpr[4].word == 0x8001eeb8u);
  CHECK(refused.progress.machine.registers.gpr[5].word == 0x8001edf4u);

  Fixture missing;
  missing.context.io = nullptr;
  CHECK(missing.run() == NBA97_TEXT_IO_REFUSED);
  CHECK(missing.progress.stopped_pc == 0x80065a0c);
}

void knownness_and_malformed_prefixes() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture fixture;
    fixture.context.machine.registers.gpr[17].known_mask =
        static_cast<std::uint8_t>(mask);
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    CHECK(fixture.progress.machine.registers.gpr[17].known_mask == mask);
  }

  Fixture unknown_store;
  unknown_store.region.known = nullptr;
  unknown_store.context.machine.registers.gpr[17].known_mask = 14;
  CHECK(unknown_store.run() == NBA97_TEXT_ARGUMENT);
  CHECK(unknown_store.progress.stopped_pc == 0x80065a10);
  CHECK(unknown_store.progress.machine.registers.gpr[31].word == 0x80065a14u);

  Fixture unknown_mode;
  unknown_mode.mask(0x8001edec, 2, 2);
  CHECK(unknown_mode.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_mode.progress.stopped_pc == 0x80065ae0);
  CHECK(unknown_mode.progress.machine.registers.gpr[2].word == 98 &&
        unknown_mode.progress.machine.registers.gpr[2].known_mask == 15);

  Fixture malformed_mode;
  malformed_mode.known[0x8001eded - Fixture::Base] = 2;
  CHECK(malformed_mode.run() == NBA97_TEXT_ARGUMENT);
  CHECK(malformed_mode.progress.stopped_pc == 0x80065ad8);
  CHECK(malformed_mode.progress.machine.registers.gpr[3].word == 0x80020000u &&
        malformed_mode.progress.machine.registers.gpr[3].known_mask == 15);
  CHECK(malformed_mode.progress.machine.registers.gpr[2].word == 1);

  Fixture unknown_ra;
  unknown_ra.context.machine.registers.gpr[31].known_mask = 14;
  CHECK(unknown_ra.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_ra.progress.stopped_pc == 0x80065b10);
  CHECK(unknown_ra.progress.machine.registers.gpr[29].word == Fixture::Stack);
  CHECK(unknown_ra.progress.machine.registers.gpr[31].word == 0x81234568u &&
        unknown_ra.progress.machine.registers.gpr[31].known_mask == 14);
  CHECK(unknown_ra.progress.restored_return_address.known_mask == 14);

  Fixture unknown_delay_address;
  unknown_delay_address.mutate_second_65820_s0 = true;
  unknown_delay_address.partial_second_65820_s0 = true;
  CHECK(unknown_delay_address.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_delay_address.progress.stopped_pc == 0x80065ad0);
  CHECK(unknown_delay_address.progress.machine.registers.gpr[31].word ==
        0x80065ad4u);

  for (int malformed = 1; malformed <= 4; ++malformed) {
    Fixture fixture;
    fixture.malformed = malformed;
    CHECK(fixture.run() == NBA97_TEXT_ARGUMENT);
    CHECK(fixture.progress.stopped_pc == 0x80065a0c);
    if (malformed == 1)
      CHECK(fixture.progress.machine.registers.gpr[9].word == 0xfeed0009u &&
            fixture.progress.machine.registers.gpr[9].known_mask == 16);
    else if (malformed == 2)
      CHECK(fixture.progress.machine.hi.word == 0xfeed0010u &&
            fixture.progress.machine.hi.known_mask == 16);
    else if (malformed == 3)
      CHECK(fixture.progress.machine.lo.word == 0xfeed0011u &&
            fixture.progress.machine.lo.known_mask == 16);
    else
      CHECK(fixture.progress.machine.registers.gpr[0].word == 0 &&
            fixture.progress.machine.registers.gpr[0].known_mask == 14);
  }
}

void memory_failures_aliases_and_atomicity() {
  Fixture unaligned;
  ++unaligned.context.machine.registers.gpr[29].word;
  CHECK(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x800659f4);
  Fixture unmapped;
  unmapped.context.machine.registers.gpr[29] = {0x80210000u, 15};
  CHECK(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.stopped_pc == 0x800659f4);

  Fixture malformed_load;
  malformed_load.invalidate_epilogue = true;
  CHECK(malformed_load.run() == NBA97_TEXT_ARGUMENT);
  CHECK(malformed_load.progress.stopped_pc == 0x80065b00);
  CHECK(malformed_load.progress.machine.registers.gpr[31].word == 0x80065af0u);

  Fixture alias;
  alias.context.machine.registers.gpr[29] = {0x8001eedcu, 15};
  CHECK(alias.run() == NBA97_TEXT_COMPLETE);
  CHECK(alias.progress.machine.registers.gpr[16].word == 0x11000005u);
  CHECK(alias.get(0x8001eecc, 2) == 5);

  Fixture short_journal;
  short_journal.context.access_journal_capacity = 1;
  CHECK(short_journal.run() == NBA97_TEXT_COMPLETE);
  CHECK(short_journal.progress.access_events == 12);

  Fixture first;
  Fixture second;
  CHECK(first.run() == second.run());
  CHECK(first.bytes == second.bytes);
  CHECK(std::memcmp(&first.progress.machine, &second.progress.machine,
                    sizeof first.progress.machine) == 0);
}
} // namespace

int main() {
  normal_mode_98();
  alternate_mode_and_live_state();
  budget_and_refusal_prefixes();
  knownness_and_malformed_prefixes();
  memory_failures_aliases_and_atomicity();
  std::printf("game match state reset tests passed (%u checks)\n", checks);
}
