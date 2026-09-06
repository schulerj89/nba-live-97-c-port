#include "game_match_buffer_compress_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr,
                 "match buffer compress integration check %u failed at %u\n",
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

  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(bytes.size(), 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::vector<Nba97GameMatchBufferRecordAccess> parentJournal =
      std::vector<Nba97GameMatchBufferRecordAccess>(4096);
  std::vector<Nba97GameMatchBufferCompressAccess> childJournal =
      std::vector<Nba97GameMatchBufferCompressAccess>(1024);
  Nba97GameMatchBufferRecordContext parent{};
  Nba97GameMatchBufferRecordProgress parentProgress{};
  Nba97GameMatchBufferCompressBinding binding{};
  unsigned rewindCalls = 0;

  Fixture(std::uint16_t toggle = 0, bool rewind = false) {
    parent.memory = {&region, 1};
    parent.operation_budget = 10000;
    parent.io = fallback;
    parent.user = this;
    parent.access_journal = parentJournal.data();
    parent.access_journal_capacity = parentJournal.size();
    for (unsigned i = 0; i < 32; ++i)
      parent.machine.registers.gpr[i] = {0x41000000u + i, 15};
    parent.machine.registers.gpr[0] = {0, 15};
    parent.machine.registers.gpr[29] = {Sp, 15};
    parent.machine.registers.gpr[31] = {0x81234568u, 15};
    parent.machine.hi = {0x11112222u, 5};
    parent.machine.lo = {0x33334444u, 10};
    binding.operation_budget = 10000;
    binding.access_journal = childJournal.data();
    binding.access_journal_capacity = childJournal.size();

    put(0x8002148cu, rewind ? 1 : 0, 2);
    put(0x800f9ffcu, toggle, 2);
    put(0x800fdb6cu, 0x56, 2);
    put(0x800fdb94u, 7, 2);
    put(0x80020c14u, Ball, 4);
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
    for (unsigned i = 0; i < 0x82; ++i) {
      put(0x800f1814u + i * 2u, 0x100u + i, 2);
      put(0x800f1918u + i * 2u, 0x100u + i, 2);
    }
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    const auto offset = static_cast<std::size_t>(address - Ram);
    for (unsigned i = 0; i < width; ++i)
      bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
  }
  std::uint32_t get(std::uint32_t address, unsigned width = 1) const {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[address - Ram + i]) << (i * 8u);
    return value;
  }
  static int fallback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameMatchBufferRecordEvent *event,
                      Nba97GameMatchBufferRecordMachine *) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (event &&
        event->kind == NBA97_GAME_MATCH_BUFFER_RECORD_REWIND_80076AD0 &&
        event->pc == 0x80076b50u && event->entry == 0x80076ad0u) {
      ++f.rewindCalls;
      return 1;
    }
    return 0;
  }
  int run() {
    return nba97_game_match_buffer_record_with_compress(&parent, &binding,
                                                        &parentProgress);
  }
};

void actualCallerBothSelections() {
  for (std::uint16_t toggle : {std::uint16_t(0), std::uint16_t(1)}) {
    Fixture f(toggle);
    check(f.run() == NBA97_TEXT_COMPLETE && f.parentProgress.completed);
    check(f.binding.invocations == 1 && f.binding.completions == 1 &&
          f.binding.progress.completed);
    check(f.binding.event.pc == 0x80076e58u &&
          f.binding.event.delay_slot_pc == 0x80076e5cu &&
          f.binding.event.entry == 0x800767fcu &&
          f.binding.event.argument_count == 4);
    check(f.childJournal[0].address == (toggle ? 0x800f1918u : 0x800f1814u));
    check(f.childJournal[1].address == (toggle ? 0x800f1814u : 0x800f1918u));
    const std::uint32_t returned =
        f.binding.progress.machine.registers.gpr[2].word;
    check(f.get(0x800fa010u, 4) == returned && f.get(0x800fe864u) == 0);
    check(f.get(0x800f9ffcu, 2) == std::uint16_t(toggle ^ 1u));
  }
}

void typedRewindAndFailure() {
  Fixture rewind(0, true);
  check(rewind.run() == NBA97_TEXT_COMPLETE && rewind.rewindCalls == 1);

  Fixture limited;
  limited.binding.operation_budget = 0;
  check(limited.run() == NBA97_TEXT_LIMIT);
  check(limited.binding.invocations == 1 && limited.binding.completions == 0 &&
        limited.binding.result == NBA97_TEXT_LIMIT);
  check(limited.parentProgress.stopped_pc == 0x80076e58u);
  check(limited.binding.progress.stopped_pc == 0x80076820u);
}

void exactGuardsAndReuse() {
  Fixture f;
  Nba97GameMatchBufferRecordEvent event{
      0x80076e58u,
      0x80076e5cu,
      0x800767fcu,
      1,
      1,
      NBA97_GAME_MATCH_BUFFER_RECORD_COMPRESS_800767FC,
      4};
  auto machine = f.parent.machine;
  machine.registers.gpr[4] = {0x800f1814u, 15};
  machine.registers.gpr[5] = {0x800f1918u, 15};
  machine.registers.gpr[6] = {Fixture::Buffer, 15};
  machine.registers.gpr[7] = {0x82u, 15};
  machine.registers.gpr[31] = {0x80076e60u, 15};
  check(nba97_game_match_buffer_compress_from_record(
            &f.binding, &f.parent.memory, &event, &machine) == 1);
  check(f.binding.invocations == 1 && f.binding.completions == 1);
  machine = f.parent.machine;
  machine.registers.gpr[4] = {0x800f1814u, 15};
  machine.registers.gpr[5] = {0x800f1918u, 15};
  machine.registers.gpr[6] = {Fixture::Buffer + 0x400u, 15};
  machine.registers.gpr[7] = {0x82u, 15};
  machine.registers.gpr[31] = {0x80076e60u, 15};
  check(nba97_game_match_buffer_compress_from_record(
            &f.binding, &f.parent.memory, &event, &machine) == 1);
  check(f.binding.invocations == 2 && f.binding.completions == 2);

  const std::array<unsigned, 8> fields{0, 1, 2, 3, 4, 5, 6, 7};
  for (unsigned field : fields) {
    auto badEvent = event;
    auto badMachine = machine;
    switch (field) {
    case 0:
      badEvent.pc ^= 4;
      break;
    case 1:
      badEvent.delay_slot_pc ^= 4;
      break;
    case 2:
      badEvent.entry ^= 4;
      break;
    case 3:
      badEvent.kind = NBA97_GAME_MATCH_BUFFER_RECORD_REWIND_80076AD0;
      break;
    case 4:
      badEvent.argument_count = 3;
      break;
    case 5:
      badEvent.invocation = 2;
      break;
    case 6:
      badMachine.registers.gpr[31].word ^= 4;
      break;
    default:
      badMachine.registers.gpr[7].word = 0x81u;
      break;
    }
    const auto before = badMachine;
    check(nba97_game_match_buffer_compress_from_record(
              &f.binding, &f.parent.memory, &badEvent, &badMachine) == 0);
    for (unsigned i = 0; i < 32; ++i)
      check(badMachine.registers.gpr[i].word == before.registers.gpr[i].word &&
            badMachine.registers.gpr[i].known_mask ==
                before.registers.gpr[i].known_mask);
  }

  auto delayOnly = event;
  delayOnly.kind = NBA97_GAME_MATCH_BUFFER_RECORD_REWIND_80076AD0;
  delayOnly.pc = 0x80076b50u;
  delayOnly.entry = 0x80076ad0u;
  delayOnly.argument_count = 0;
  auto delayMachine = machine;
  delayMachine.registers.gpr[31] = {0x80076b58u, 15};
  check(nba97_game_match_buffer_compress_from_record(
            &f.binding, &f.parent.memory, &delayOnly, &delayMachine) == 0);
  check(f.binding.result == NBA97_TEXT_ARGUMENT);

  auto returnOnly = delayOnly;
  returnOnly.delay_slot_pc = 0x80076b54u;
  auto returnMachine = machine;
  returnMachine.registers.gpr[31] = {0x80076e60u, 15};
  check(nba97_game_match_buffer_compress_from_record(
            &f.binding, &f.parent.memory, &returnOnly, &returnMachine) == 0);
  check(f.binding.result == NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  actualCallerBothSelections();
  typedRewindAndFailure();
  exactGuardsAndReuse();
  std::printf("game match buffer compress integration: %u checks\n", checks);
  return EXIT_SUCCESS;
}
