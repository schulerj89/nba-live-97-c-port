#include "recovered/game_gpu_control_command.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
using U32 = std::uint32_t;
unsigned checks;
void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "GPU control command check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

struct Block {
  U32 base;
  std::vector<std::uint8_t> data;
  std::vector<std::uint8_t> known;
  Nba97GameTextRegion region{};
  Block(U32 address, std::size_t size)
      : base(address), data(size), known(size, 1) {
    region = {base, data.data(), known.data(), data.size()};
  }
  void put(U32 address, U32 value, unsigned width = 4) {
    const std::size_t offset = address - base;
    for (unsigned i = 0; i < width; ++i) {
      data[offset + i] = static_cast<std::uint8_t>(value >> (8u * i));
      known[offset + i] = 1;
    }
  }
  U32 get(U32 address, unsigned width = 4) const {
    U32 value = 0;
    const std::size_t offset = address - base;
    for (unsigned i = 0; i < width; ++i)
      value |= U32(data[offset + i]) << (8u * i);
    return value;
  }
};

struct Fixture {
  static constexpr U32 Port = 0x1f801814u;
  Block globals{0x800c5694u, 4};
  Block cache{0x800d8d94u, 256};
  Block port{Port, 4};
  std::array<Nba97GameTextRegion, 3> regions{};
  std::array<Nba97GameGpuControlCommandAccess, 3> journal{};
  Nba97GameGpuControlCommandContext context{};
  Nba97GameGpuControlCommandProgress progress{};

  Fixture(U32 command = 0x03000001u) {
    globals.put(0x800c5694u, Port);
    regions = {globals.region, cache.region, port.region};
    for (unsigned i = 0; i < 32; ++i)
      context.machine.registers.gpr[i] = {
          0x11000000u + i * 0x010101u,
          static_cast<std::uint8_t>((i % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {command, 15};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x81234568u,
                                                                15};
    context.machine.hi = {0x89abcdefu, 5};
    context.machine.lo = {0x76543210u, 10};
    context.memory = {regions.data(), regions.size()};
    context.operation_budget = 3;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
  }
  int run() { return nba97_game_gpu_control_command(&context, &progress); }
};

void allCommandClassesAndMachineState() {
  const U32 lows[] = {0u, 1u, 0x7fu, 0x80u, 0xffu};
  for (U32 high = 0; high < 256; ++high) {
    for (U32 low : lows) {
      const U32 command = (high << 24u) | 0x0055aa00u | low;
      Fixture f(command);
      const auto initial = f.context.machine;
      check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
            f.progress.operations == 3 && f.progress.accesses == 3 &&
            f.progress.reads == 1 && f.progress.stores == 2);
      check(f.port.get(Fixture::Port) == command &&
            f.cache.get(0x800d8d94u + high, 1) == low &&
            f.progress.port_pointer.word == Fixture::Port &&
            f.progress.command_byte.word == high &&
            f.progress.cache_address.word == 0x800d8d94u + high);
      check(f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
                high &&
            f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                    .known_mask == 15 &&
            f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT].word ==
                0x800e0000u + high &&
            f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT]
                    .known_mask == 15 &&
            f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
                command);
      for (unsigned reg = 0; reg < 32; ++reg)
        if (reg != NBA97_MATCH_INITIALIZE_V0 &&
            reg != NBA97_MATCH_INITIALIZE_AT)
          check(std::memcmp(&f.progress.machine.registers.gpr[reg],
                            &initial.registers.gpr[reg],
                            sizeof(initial.registers.gpr[reg])) == 0);
      check(std::memcmp(&f.progress.machine.hi, &initial.hi,
                        sizeof initial.hi) == 0 &&
            std::memcmp(&f.progress.machine.lo, &initial.lo,
                        sizeof initial.lo) == 0);
      check(f.journal[0].pc == 0x8009b170u &&
            f.journal[0].kind == NBA97_GAME_GPU_CONTROL_COMMAND_READ &&
            f.journal[1].pc == 0x8009b178u &&
            f.journal[1].address == Fixture::Port &&
            f.journal[2].pc == 0x8009b188u &&
            f.journal[2].address == 0x800d8d94u + high);
    }
  }
}

void allArgumentMasksAndOrderedPrefixes() {
  const U32 command = 0xab1234cdu;
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture f(command);
    f.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask =
        static_cast<std::uint8_t>(mask);
    const int result = f.run();
    check(f.port.get(Fixture::Port) == command &&
          f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                  .known_mask == mask &&
          f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
              0xabu &&
          f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                  .known_mask ==
              static_cast<unsigned>(14u | ((mask >> 3u) & 1u)) &&
          f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT].word ==
              0x800e00abu &&
          f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT]
                  .known_mask ==
              static_cast<unsigned>(14u | ((mask >> 3u) & 1u)));
    if (mask & 8u) {
      check(result == NBA97_TEXT_COMPLETE && f.progress.completed &&
            f.cache.get(0x800d8e3fu, 1) == 0xcdu &&
            f.cache.known[0xab] == (mask & 1u));
    } else {
      check(result == NBA97_TEXT_UNKNOWN && !f.progress.completed &&
            f.progress.stopped_pc == 0x8009b188u &&
            f.progress.operations == 2 && f.progress.stores == 1);
    }
  }
}

void budgetsUnknownReturnAndJournals() {
  for (std::size_t budget = 0; budget <= 3; ++budget) {
    Fixture f;
    f.context.operation_budget = budget;
    const int result = f.run();
    check((budget == 3 ? result == NBA97_TEXT_COMPLETE
                       : result == NBA97_TEXT_LIMIT) &&
          f.progress.operations == budget &&
          f.progress.completed == (budget == 3));
    if (budget == 0)
      check(f.progress.stopped_pc == 0x8009b170u &&
            f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
                0x800c0000u);
    if (budget == 1)
      check(f.progress.stopped_pc == 0x8009b178u &&
            f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
                Fixture::Port);
    if (budget == 2)
      check(f.progress.stopped_pc == 0x8009b188u &&
            f.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT].word ==
                0x800e0003u);
  }

  Fixture unknownRa;
  unknownRa.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 14;
  check(unknownRa.run() == NBA97_TEXT_UNKNOWN &&
        unknownRa.progress.stopped_pc == 0x8009b18cu &&
        unknownRa.progress.stores == 2 &&
        unknownRa.port.get(Fixture::Port) == 0x03000001u);

  for (std::size_t capacity = 0; capacity <= 3; ++capacity) {
    Fixture f;
    f.context.access_journal_capacity = capacity;
    f.context.access_journal = capacity ? f.journal.data() : nullptr;
    check(f.run() == NBA97_TEXT_COMPLETE && f.progress.access_events == 3);
    for (std::size_t i = 0; i < capacity; ++i)
      check(f.journal[i].operation == i + 1);
  }
}

void nullKnownStoresAndAtomicLoads() {
  Fixture portRefusal;
  portRefusal.port.region.known = nullptr;
  portRefusal.regions[2] = portRefusal.port.region;
  portRefusal.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
      .known_mask = 14;
  const U32 oldPort = portRefusal.port.get(Fixture::Port);
  check(portRefusal.run() == NBA97_TEXT_ARGUMENT &&
        portRefusal.progress.stopped_pc == 0x8009b178u &&
        portRefusal.port.get(Fixture::Port) == oldPort &&
        portRefusal.progress.stores == 0);

  Fixture cacheRefusal(0xab1234cdu);
  cacheRefusal.cache.put(0x800d8e3fu, 0x77, 1);
  cacheRefusal.cache.region.known = nullptr;
  cacheRefusal.regions[1] = cacheRefusal.cache.region;
  cacheRefusal.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
      .known_mask = 14;
  check(cacheRefusal.run() == NBA97_TEXT_ARGUMENT &&
        cacheRefusal.progress.stopped_pc == 0x8009b188u &&
        cacheRefusal.port.get(Fixture::Port) == 0xab1234cdu &&
        cacheRefusal.cache.get(0x800d8e3fu, 1) == 0x77 &&
        cacheRefusal.progress.stores == 1);

  Fixture invalidByte;
  invalidByte.globals.known[3] = 2;
  check(invalidByte.run() == NBA97_TEXT_ARGUMENT &&
        invalidByte.progress.stopped_pc == 0x8009b170u &&
        invalidByte.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 0x800c0000u &&
        invalidByte.progress.reads == 0);
}

void aliasesReloadsAndInvalidMappings() {
  Fixture pointerAlias(0x11223344u);
  pointerAlias.globals.put(0x800c5694u, 0x800c5694u);
  check(pointerAlias.run() == NBA97_TEXT_COMPLETE &&
        pointerAlias.globals.get(0x800c5694u) == 0x11223344u &&
        pointerAlias.cache.get(0x800d8da5u, 1) == 0x44u);

  Fixture cacheAlias(0x7c123456u);
  cacheAlias.globals.put(0x800c5694u, 0x800d8e10u);
  check(cacheAlias.run() == NBA97_TEXT_COMPLETE);
  check(cacheAlias.cache.get(0x800d8e10u) == 0x7c123456u);
  check(cacheAlias.cache.get(0x800d8e10u, 1) == 0x56u);

  Fixture reloadA(0x03112233u);
  check(reloadA.run() == NBA97_TEXT_COMPLETE);
  reloadA.context.machine = reloadA.progress.machine;
  reloadA.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {
      0x04556677u, 15};
  reloadA.globals.put(0x800c5694u, 0x800d8d94u);
  check(reloadA.run() == NBA97_TEXT_COMPLETE &&
        reloadA.cache.get(0x800d8d94u) == 0x04556677u &&
        reloadA.cache.get(0x800d8d98u, 1) == 0x77u);

  Fixture misaligned;
  misaligned.globals.put(0x800c5694u, Fixture::Port + 1);
  check(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.stopped_pc == 0x8009b178u);
  Fixture unmapped;
  unmapped.globals.put(0x800c5694u, 0x1f900000u);
  check(unmapped.run() == NBA97_TEXT_RESOURCE &&
        unmapped.progress.stopped_pc == 0x8009b178u);
  Fixture badMachine;
  badMachine.context.machine.hi.known_mask = 16;
  check(badMachine.run() == NBA97_TEXT_ARGUMENT);
  Fixture empty;
  empty.regions[1].size = 0;
  check(empty.run() == NBA97_TEXT_ARGUMENT);
  Fixture overlap;
  overlap.regions[1] = overlap.regions[0];
  check(overlap.run() == NBA97_TEXT_ARGUMENT);

  Fixture deterministicA(0xff00aa55u);
  Fixture deterministicB(0xff00aa55u);
  check(deterministicA.run() == deterministicB.run() &&
        deterministicA.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT]
                .word == deterministicB.progress.machine.registers
                             .gpr[NBA97_MATCH_INITIALIZE_AT]
                             .word &&
        deterministicA.cache.data == deterministicB.cache.data &&
        deterministicA.port.data == deterministicB.port.data);
}
} // namespace

int main() {
  allCommandClassesAndMachineState();
  allArgumentMasksAndOrderedPrefixes();
  budgetsUnknownReturnAndJournals();
  nullKnownStoresAndAtomicLoads();
  aliasesReloadsAndInvalidMappings();
  std::printf("game GPU control command tests passed (%u checks)\n", checks);
  return 0;
}
