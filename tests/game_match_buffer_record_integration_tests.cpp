#include "game_match_buffer_record_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;

void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr,
                 "match buffer record integration check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

struct Fixture {
  static constexpr std::uint32_t Ram = 0x80000000u;
  static constexpr std::uint32_t Sp = 0x801ff000u;
  static constexpr std::uint32_t Controllers = 0x80030000u;
  static constexpr std::uint32_t Ball = 0x80040000u;
  static constexpr std::uint32_t Entities = 0x80050000u;
  static constexpr std::uint32_t Buffer = 0x800f3000u;

  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GamePeriodStartupAccess, 32> parentJournal{};
  std::array<Nba97GameMatchBufferRecordAccess, 4096> ownerJournal{};
  std::array<Nba97GameMatchBufferRewindAccess, 32> rewindJournal{};
  Nba97GamePeriodStartupContext parent{};
  Nba97GamePeriodStartupProgress parentProgress{};
  Nba97GameMatchBufferRecordBinding binding{};
  std::array<Nba97GamePeriodStartupEvent, 16> fallbackEvents{};
  std::array<Nba97GameMatchBufferRecordEvent, 2> compressionEvents{};
  unsigned fallbackCalls = 0;
  unsigned compressionCalls = 0;
  unsigned invalidCompressionControl = 0;

  Fixture() {
    parent.memory = {&region, 1};
    parent.operation_budget = 100;
    parent.io = fallback;
    parent.user = this;
    parent.access_journal = parentJournal.data();
    parent.access_journal_capacity = parentJournal.size();
    for (unsigned i = 0; i < 32; ++i)
      parent.registers.gpr[i] = {0x41000000u + i, 15};
    parent.registers.gpr[0] = {0, 15};
    parent.registers.gpr[29] = {Sp, 15};
    parent.registers.gpr[31] = {0x81234568u, 15};

    nba97_game_match_buffer_record_binding_init(&binding, 4000, 20, 100);
    binding.io = compression;
    binding.user = this;
    binding.access_journal = ownerJournal.data();
    binding.access_journal_capacity = ownerJournal.size();
    binding.rewind_journal = rewindJournal.data();
    binding.rewind_journal_capacity = rewindJournal.size();

    put(0x800fdb68u, 0, 2);
    put(0x80020c14u, Ball, 4);
    put(0x8001edecu, 0, 2);
    put(0x8002148cu, 1, 2);
    put(0x800f9ffcu, 0, 2);
    put(0x800fdb94u, 7, 2);
    for (unsigned i = 0; i < 8; ++i) {
      put(0x800fdc50u + i * 4u, Controllers + i * 0x40u, 4);
      put(Controllers + i * 0x40u + 0x26u, 0x120u + i, 2);
    }
    put(0x800fdbccu, 0x56, 2);
    put(0x800fdb58u, 0x12345678u, 4);
    put(0x800fdba4u, 0x87654321u, 4);
    put(0x800fdb90u, 0x89, 2);
    put(0x8001ee46u, 0x9a, 2);
    put(0x8001ef0au, 0xab, 2);
    put(0x800fdc38u, 0xdecafbadu, 4);
    for (unsigned i = 0; i < 4; ++i)
      put(0x800b7a00u + i * 4u, 0x40u + i, 4);
    put(0x801076e6u, 0xabc0u, 2);
    put(0x80108a0au, 0xdef0u, 2);
    put(Ball + 0x14u, 0x1111u, 2);
    put(Ball + 0x16u, 0x2222u, 2);
    put(Ball + 0x18u, 0x3333u, 2);
    put(0x800fe860u, 0x44556677u, 4);
    put(0x800fe864u, 0xee, 1);
    put(0x80020becu, Entities, 4);
    for (unsigned entity = 0; entity < 11; ++entity) {
      const auto base = Entities + entity * 0xf4u;
      put(base + 8u, 0x00012300u + entity * 0x100u, 4);
      put(base + 0x0cu, 0xfffffe00u - entity * 0x100u, 4);
      put(base + 0x10u, 0x00034500u + entity * 0x100u, 4);
      for (unsigned field = 0; field < 0x30; field += 2)
        put(base + 0x74u + field, 0x80u + entity + field, 2);
    }
    put(0x800fa004u, Buffer, 4);
    put(0x800fa008u, Buffer + 0x1000u, 4);
    put(0x800fa00cu, Buffer, 4);
    put(0x800fa010u, Buffer, 4);
    put(0x800fa014u, 0, 4);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    const auto offset = static_cast<std::size_t>(address - Ram);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = static_cast<std::uint8_t>(value >> (8u * i));
      known[offset + i] = 1;
    }
  }

  std::uint32_t get(std::uint32_t address, unsigned width = 1) const {
    const auto offset = static_cast<std::size_t>(address - Ram);
    std::uint32_t result = 0;
    for (unsigned i = 0; i < width; ++i)
      result |= std::uint32_t(bytes[offset + i]) << (8u * i);
    return result;
  }

  static int fallback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GamePeriodStartupEvent *event,
                      Nba97GamePeriodStartupRegisters *) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.fallbackEvents[fixture.fallbackCalls++] = *event;
    return 1;
  }

  static int compression(void *opaque, const Nba97GameTextMemory *,
                         const Nba97GameMatchBufferRecordEvent *event,
                         Nba97GameMatchBufferRecordMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.compressionEvents[fixture.compressionCalls] = *event;
    ++fixture.compressionCalls;
    machine->registers.gpr[2] = {Buffer + fixture.compressionCalls * 0x100u,
                                 15};
    if (fixture.invalidCompressionControl) {
      machine->registers.gpr[8] = {0x5a5a0008u, 5};
    }
    if (fixture.invalidCompressionControl == 1)
      machine->hi.known_mask = 16;
    if (fixture.invalidCompressionControl == 2)
      machine->lo.known_mask = 16;
    return 1;
  }

  int run() {
    return nba97_game_period_startup_with_match_buffer_record(&parent, &binding,
                                                              &parentProgress);
  }
};

void actualPeriodCallsOwnerTwiceAndComposesRewind() {
  Fixture fixture;
  check(fixture.run() == NBA97_TEXT_COMPLETE &&
        fixture.parentProgress.completed);
  check(fixture.binding.invocations == 2 && fixture.binding.completions == 2);
  check(fixture.binding.event[0].pc == 0x800674f8u &&
        fixture.binding.event[0].delay_slot_pc == 0x800674fcu &&
        fixture.binding.event[0].entry == 0x80076b3cu &&
        fixture.binding.event[0].argument_count == 0);
  check(fixture.binding.event[1].pc == 0x80067508u &&
        fixture.binding.event[1].delay_slot_pc == 0x8006750cu &&
        fixture.binding.event[1].entry == 0x80076b3cu &&
        fixture.binding.event[1].argument_count == 0);
  check(fixture.compressionCalls == 2 &&
        fixture.binding.compression_invocations == 2);
  for (const auto &event : fixture.compressionEvents)
    check(event.pc == 0x80076e58u && event.delay_slot_pc == 0x80076e5cu &&
          event.entry == 0x800767fcu && event.argument_count == 4);
  check(fixture.binding.rewind_invocations == 1 &&
        fixture.binding.rewind.zero_invocations == 1 &&
        fixture.binding.rewind.zero_completions == 1 &&
        fixture.binding.rewind.progress.completed);
  check(fixture.fallbackCalls == 11 &&
        fixture.parentProgress.callbacks_completed == 13);
  check(fixture.get(0x8002148cu, 2) == 0 &&
        fixture.get(0x800fa010u, 4) == Fixture::Buffer + 0x200u &&
        fixture.get(0x800fe864u) == 0);
  check(fixture.get(0x800f1814u + 9u) == 1u &&
        fixture.get(0x800f1814u + 0x18u, 2) == 0x1111u);
  check(fixture.parentProgress.registers.gpr[29].word == Fixture::Sp &&
        fixture.parentProgress.registers.gpr[31].word == 0x81234568u);
}

void ownerFailureStopsAtFirstNaturalCall() {
  Fixture fixture;
  fixture.binding.operation_budget = 0;
  check(fixture.run() == NBA97_TEXT_LIMIT);
  check(fixture.binding.invocations == 1 && fixture.binding.completions == 0 &&
        fixture.binding.result == NBA97_TEXT_LIMIT);
  check(fixture.parentProgress.stopped_pc == 0x800674f8u &&
        fixture.parentProgress.callbacks_completed == 8 &&
        fixture.fallbackCalls == 8 && fixture.compressionCalls == 0);
  check(fixture.get(0x800fe864u) == 0xeeu);
}

void malformedCompressionControlWordsPreserveGprPrefix() {
  for (unsigned control = 1; control <= 2; ++control) {
    Fixture fixture;
    fixture.put(0x8002148cu, 0, 2);
    fixture.invalidCompressionControl = control;
    check(fixture.run() == NBA97_TEXT_ARGUMENT);
    check(fixture.binding.invocations == 1 &&
          fixture.binding.completions == 0 &&
          fixture.binding.compression_invocations == 1 &&
          fixture.binding.progress.stopped_pc == 0x80076e58u);
    check(
        (control == 1 &&
         fixture.binding.progress.machine.hi.known_mask == 16) ||
        (control == 2 && fixture.binding.progress.machine.lo.known_mask == 16));
    check(fixture.parentProgress.stopped_pc == 0x800674f8u &&
          fixture.parentProgress.registers.gpr[8].word == 0x5a5a0008u &&
          fixture.parentProgress.registers.gpr[8].known_mask == 5);
  }
}

void wrapperBindingAndRamCanBeReused() {
  Fixture fixture;
  check(fixture.run() == NBA97_TEXT_COMPLETE);
  fixture.fallbackCalls = 0;
  fixture.compressionCalls = 0;
  fixture.fallbackEvents.fill({});
  fixture.compressionEvents.fill({});
  check(fixture.run() == NBA97_TEXT_COMPLETE &&
        fixture.parentProgress.completed);
  check(fixture.binding.invocations == 2 && fixture.binding.completions == 2 &&
        fixture.binding.compression_invocations == 2 &&
        fixture.binding.rewind_invocations == 0);
  check(fixture.fallbackCalls == 11 && fixture.compressionCalls == 2 &&
        fixture.get(0x800fa010u, 4) == Fixture::Buffer + 0x200u);
}
} // namespace

int main() {
  actualPeriodCallsOwnerTwiceAndComposesRewind();
  ownerFailureStopsAtFirstNaturalCall();
  malformedCompressionControlWordsPreserveGprPrefix();
  wrapperBindingAndRamCanBeReused();
  std::printf("game match buffer record integration: %u checks\n", checks);
}
