#include "recovered/game_gpu_packet_dma.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

size_t checks;

void check(bool condition, const char *message) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "game_gpu_packet_dma_tests: %s\n", message);
    std::exit(1);
  }
}

uint32_t read32(const uint8_t *bytes, size_t offset) {
  return static_cast<uint32_t>(bytes[offset]) |
         (static_cast<uint32_t>(bytes[offset + 1]) << 8u) |
         (static_cast<uint32_t>(bytes[offset + 2]) << 16u) |
         (static_cast<uint32_t>(bytes[offset + 3]) << 24u);
}

void write32(uint8_t *bytes, size_t offset, uint32_t value) {
  for (unsigned byte = 0u; byte != 4u; ++byte)
    bytes[offset + byte] = static_cast<uint8_t>(value >> (8u * byte));
}

struct Fixture {
  static constexpr uint32_t base = UINT32_C(0x800c0000);
  static constexpr uint32_t port0 = UINT32_C(0x800c6000);
  static constexpr uint32_t port1 = UINT32_C(0x800c6010);
  static constexpr uint32_t port2 = UINT32_C(0x800c6020);
  static constexpr uint32_t port3 = UINT32_C(0x800c6030);
  std::array<uint8_t, 0x10000> data{};
  std::array<uint8_t, 0x10000> known{};
  Nba97GameTextRegion region{base, data.data(), known.data(), data.size()};
  std::array<Nba97GameGpuPacketDmaAccess, 16> journal{};
  Nba97GameGpuPacketDmaContext context{};
  Nba97GameGpuPacketDmaProgress progress{};

  Fixture() {
    known.fill(1u);
    write32(data.data(), 0x5694u, port0);
    write32(data.data(), 0x5698u, port1);
    write32(data.data(), 0x569cu, port2);
    write32(data.data(), 0x56a0u, port3);
    context.memory = {&region, 1u};
    context.operation_budget = 8u;
    for (unsigned index = 0u; index != 32u; ++index) {
      context.machine.registers.gpr[index].word =
          UINT32_C(0x19000000) + index * UINT32_C(0x01010101);
      context.machine.registers.gpr[index].known_mask =
          static_cast<uint8_t>((index * 7u) & 15u);
    }
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        {UINT32_C(0x80123456), 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {UINT32_C(0x8009b3b0), 15u};
    context.machine.hi = {UINT32_C(0x12345678), 5u};
    context.machine.lo = {UINT32_C(0x89abcdef), 10u};
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
  }

  size_t offset(uint32_t address) const {
    return static_cast<size_t>(address - base);
  }

  int run() { return nba97_game_gpu_packet_dma(&context, &progress); }
};

void normal_and_journal() {
  Fixture fixture;
  auto original = fixture.context.machine;
  check(fixture.run() == NBA97_TEXT_COMPLETE, "normal complete");
  check(fixture.progress.completed == 1u, "completion flag");
  check(fixture.progress.operations == 8u && fixture.progress.reads == 4u &&
            fixture.progress.stores == 4u,
        "four reads and stores");
  check(read32(fixture.data.data(), fixture.offset(Fixture::port0)) ==
            UINT32_C(0x04000002),
        "first control word");
  check(read32(fixture.data.data(), fixture.offset(Fixture::port1)) ==
            UINT32_C(0x80123456),
        "raw packet word");
  check(read32(fixture.data.data(), fixture.offset(Fixture::port2)) == 0u,
        "third zero word");
  check(read32(fixture.data.data(), fixture.offset(Fixture::port3)) ==
            UINT32_C(0x01000401),
        "fourth start word");
  check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            Fixture::port3,
        "final v0 is fourth port");
  check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V1].word ==
            UINT32_C(0x01000401),
        "final v1 start word");
  for (unsigned index = 0u; index != 32u; ++index) {
    if (index == NBA97_MATCH_INITIALIZE_V0 ||
        index == NBA97_MATCH_INITIALIZE_V1)
      continue;
    check(fixture.progress.machine.registers.gpr[index].word ==
              original.registers.gpr[index].word,
          "untouched gpr word");
    check(fixture.progress.machine.registers.gpr[index].known_mask ==
              original.registers.gpr[index].known_mask,
          "untouched gpr mask");
  }
  check(std::memcmp(&fixture.progress.machine.hi, &original.hi,
                    sizeof(original.hi)) == 0,
        "hi passthrough");
  check(std::memcmp(&fixture.progress.machine.lo, &original.lo,
                    sizeof(original.lo)) == 0,
        "lo passthrough");

  const uint32_t pcs[8] = {
      UINT32_C(0x8009b200), UINT32_C(0x8009b208),
      UINT32_C(0x8009b210), UINT32_C(0x8009b218),
      UINT32_C(0x8009b220), UINT32_C(0x8009b228),
      UINT32_C(0x8009b230), UINT32_C(0x8009b238)};
  const uint32_t addresses[8] = {
      UINT32_C(0x800c5694), Fixture::port0, UINT32_C(0x800c5698),
      Fixture::port1, UINT32_C(0x800c569c), Fixture::port2,
      UINT32_C(0x800c56a0), Fixture::port3};
  const uint32_t values[8] = {
      Fixture::port0, UINT32_C(0x04000002), Fixture::port1,
      UINT32_C(0x80123456), Fixture::port2, 0u, Fixture::port3,
      UINT32_C(0x01000401)};
  for (size_t index = 0u; index != 8u; ++index) {
    check(fixture.journal[index].pc == pcs[index], "journal pc order");
    check(fixture.journal[index].address == addresses[index],
          "journal address order");
    check(fixture.journal[index].value == values[index],
          "journal value order");
    check(fixture.journal[index].width == 4u, "journal word width");
    check(fixture.journal[index].operation == index + 1u,
          "journal operation order");
    check(fixture.journal[index].kind ==
              (index % 2u == 0u ? NBA97_GAME_MATCH_CLOCKS_READ
                                : NBA97_GAME_MATCH_CLOCKS_STORE),
          "journal alternating kind");
  }
}

void raw_a0_and_partial_knownness() {
  const uint32_t values[] = {0u, 1u, UINT32_C(0xffffffff),
                             UINT32_C(0x80000001)};
  for (uint32_t value : values) {
    Fixture fixture;
    fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        {value, 15u};
    check(fixture.run() == NBA97_TEXT_COMPLETE, "raw a0 completes");
    check(read32(fixture.data.data(), fixture.offset(Fixture::port1)) == value,
          "raw a0 stored without dereference");
  }

  Fixture partial;
  partial.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
      {UINT32_C(0xa1b2c3d4), 5u};
  check(partial.run() == NBA97_TEXT_COMPLETE, "partial a0 completes");
  check(read32(partial.data.data(), partial.offset(Fixture::port1)) ==
            UINT32_C(0xa1b2c3d4),
        "partial a0 raw bits");
  check(partial.known[partial.offset(Fixture::port1)] == 1u &&
            partial.known[partial.offset(Fixture::port1) + 1u] == 0u &&
            partial.known[partial.offset(Fixture::port1) + 2u] == 1u &&
            partial.known[partial.offset(Fixture::port1) + 3u] == 0u,
        "partial a0 byte knownness");
  check(partial.journal[3].known_mask == 5u,
        "partial a0 journal knownness");
}

void budgets_and_unknown_pointers() {
  for (size_t budget = 0u; budget != 8u; ++budget) {
    Fixture fixture;
    fixture.context.operation_budget = budget;
    check(fixture.run() == NBA97_TEXT_LIMIT, "every budget prefix limits");
    check(fixture.progress.operations == budget, "exact budget prefix");
    check(fixture.progress.access_events == budget,
          "successful journal prefix count");
  }

  const size_t pointer_offsets[4] = {0x5694u, 0x5698u, 0x569cu, 0x56a0u};
  const uint32_t stop_pcs[4] = {UINT32_C(0x8009b208),
                                UINT32_C(0x8009b218),
                                UINT32_C(0x8009b228),
                                UINT32_C(0x8009b238)};
  for (size_t pointer = 0u; pointer != 4u; ++pointer) {
    Fixture fixture;
    fixture.known[pointer_offsets[pointer] + 2u] = 0u;
    check(fixture.run() == NBA97_TEXT_UNKNOWN, "unknown port pointer");
    check(fixture.progress.stopped_pc == stop_pcs[pointer],
          "unknown pointer store pc");
    check(fixture.progress.stores == pointer,
          "unknown pointer preserves earlier stores");
  }
}

void port_and_table_failures() {
  Fixture unaligned;
  write32(unaligned.data.data(), 0x5694u, Fixture::port0 + 1u);
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP,
        "unaligned port traps");
  check(unaligned.progress.operations == 2u && unaligned.progress.stores == 0u,
        "unaligned port prefix");

  Fixture unmapped_port;
  write32(unmapped_port.data.data(), 0x5694u, UINT32_C(0x1f8010f0));
  check(unmapped_port.run() == NBA97_TEXT_RESOURCE, "unmapped port refuses");
  check(unmapped_port.progress.stopped_address == UINT32_C(0x1f8010f0),
        "unmapped port address");

  Fixture unmapped_globals;
  unmapped_globals.region.size = 0x5000u;
  check(unmapped_globals.run() == NBA97_TEXT_RESOURCE,
        "unmapped pointer table refuses");
  check(unmapped_globals.progress.stopped_pc == UINT32_C(0x8009b200),
        "unmapped pointer table pc");
  check(unmapped_globals.progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == UINT32_C(0x800c0000),
        "failed first load keeps LUI v0");

  Fixture alias;
  write32(alias.data.data(), 0x5694u, UINT32_C(0x800c5698));
  check(alias.run() == NBA97_TEXT_ALIGNMENT_TRAP,
        "pointer-table alias changes later pointer");
  check(read32(alias.data.data(), 0x5698u) == UINT32_C(0x04000002),
        "earlier port store overwrites later pointer");
  check(alias.progress.stopped_pc == UINT32_C(0x8009b218),
        "alias observed at second store");

  Fixture stack_like;
  write32(stack_like.data.data(), 0x5694u, UINT32_C(0x800cff00));
  check(stack_like.run() == NBA97_TEXT_COMPLETE,
        "arbitrary mapped stack-like port completes");
  check(read32(stack_like.data.data(), 0xff00u) == UINT32_C(0x04000002),
        "stack-like mapped target written");
}

void malformed_and_atomic_stores() {
  Fixture malformed_load;
  malformed_load.known[0x56a0u + 3u] = 2u;
  check(malformed_load.run() == NBA97_TEXT_ARGUMENT,
        "malformed late load knownness");
  check(malformed_load.progress.stopped_pc == UINT32_C(0x8009b230),
        "malformed late load pc");
  check(malformed_load.progress.stores == 3u,
        "malformed late load retains three stores");
  check(malformed_load.progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == UINT32_C(0x800c0000),
        "malformed load leaves LUI destination atomic");

  Fixture malformed_store;
  const size_t target = malformed_store.offset(Fixture::port3);
  malformed_store.data[target] = 0x5au;
  malformed_store.known[target + 3u] = 2u;
  check(malformed_store.run() == NBA97_TEXT_ARGUMENT,
        "malformed store knownness");
  check(malformed_store.data[target] == 0x5au,
        "malformed store leaves bytes immutable");

  Fixture missing_known;
  missing_known.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
      {UINT32_C(0xaabbccdd), 14u};
  missing_known.region.known = nullptr;
  missing_known.data[missing_known.offset(Fixture::port1)] = 0x77u;
  check(missing_known.run() == NBA97_TEXT_ARGUMENT,
        "partial store without known backing rejects");
  check(read32(missing_known.data.data(), missing_known.offset(Fixture::port1)) ==
            UINT32_C(0x00000077),
        "partial null-known store immutable");
}

void unknown_ra_and_invalid_contexts() {
  Fixture unknown_ra;
  unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 14u;
  check(unknown_ra.run() == NBA97_TEXT_UNKNOWN,
        "unknown ra after stores refuses");
  check(unknown_ra.progress.stopped_pc == UINT32_C(0x8009b23c),
        "unknown ra jr pc");
  check(unknown_ra.progress.stores == 4u,
        "unknown ra keeps every port store");

  Fixture bad_machine;
  bad_machine.context.machine.registers.gpr[0].word = 1u;
  check(bad_machine.run() == NBA97_TEXT_ARGUMENT, "invalid zero register");
  check(bad_machine.progress.operations == 0u, "invalid machine immutable");

  Fixture overlap;
  std::array<Nba97GameTextRegion, 2> regions = {
      overlap.region,
      Nba97GameTextRegion{Fixture::base + 4u, overlap.data.data() + 4u,
                          overlap.known.data() + 4u, 4u}};
  overlap.context.memory = {regions.data(), regions.size()};
  check(overlap.run() == NBA97_TEXT_ARGUMENT, "overlap map rejected");

  Fixture bad_journal;
  bad_journal.context.access_journal = nullptr;
  bad_journal.context.access_journal_capacity = 1u;
  check(bad_journal.run() == NBA97_TEXT_ARGUMENT,
        "missing journal rejected");
}

void journal_truncation_and_repeatability() {
  Fixture first;
  Fixture second;
  first.context.access_journal_capacity = 3u;
  second.context.access_journal_capacity = 3u;
  check(first.run() == NBA97_TEXT_COMPLETE, "truncated journal one");
  check(second.run() == NBA97_TEXT_COMPLETE, "truncated journal two");
  check(first.progress.access_events == 8u, "logical journal count retained");
  check(std::memcmp(first.journal.data(), second.journal.data(),
                    3u * sizeof(first.journal[0])) == 0,
        "truncated journal deterministic");
  check(std::memcmp(first.data.data(), second.data.data(), first.data.size()) ==
            0,
        "memory deterministic");
  check(std::memcmp(&first.progress.machine, &second.progress.machine,
                    sizeof(first.progress.machine)) == 0,
        "machine deterministic");
}

} // namespace

int main() {
  normal_and_journal();
  raw_a0_and_partial_knownness();
  budgets_and_unknown_pointers();
  port_and_table_failures();
  malformed_and_atomic_stores();
  unknown_ra_and_invalid_contexts();
  journal_truncation_and_repeatability();
  std::printf("game_gpu_packet_dma_tests: %zu checks passed\n", checks);
  return 0;
}
