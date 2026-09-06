#include "game_gpu_packet_dma_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

size_t checks;

void check(bool condition, const char *message) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "game_gpu_packet_dma_integration_tests: %s\n",
                 message);
    std::exit(1);
  }
}

void write32(uint8_t *bytes, uint32_t address, uint32_t value) {
  const size_t offset = address - UINT32_C(0x80000000);
  for (unsigned byte = 0u; byte != 4u; ++byte)
    bytes[offset + byte] = static_cast<uint8_t>(value >> (8u * byte));
}

uint32_t read32(const uint8_t *bytes, uint32_t address) {
  const size_t offset = address - UINT32_C(0x80000000);
  uint32_t value = 0u;
  for (unsigned byte = 0u; byte != 4u; ++byte)
    value |= static_cast<uint32_t>(bytes[offset + byte]) << (8u * byte);
  return value;
}

struct Fixture {
  static constexpr uint32_t base = UINT32_C(0x80000000);
  static constexpr size_t size = 0x200000u;
  static constexpr uint32_t stack = UINT32_C(0x801ff000);
  static constexpr uint32_t packet = UINT32_C(0x80050001);
  static constexpr uint32_t port0 = UINT32_C(0x80030080);
  static constexpr uint32_t port1 = UINT32_C(0x80030084);
  static constexpr uint32_t port2 = UINT32_C(0x80030088);
  static constexpr uint32_t port3 = UINT32_C(0x8003008c);
  std::vector<uint8_t> data = std::vector<uint8_t>(size, 0xa5u);
  std::vector<uint8_t> known = std::vector<uint8_t>(size, 1u);
  Nba97GameTextRegion region{base, data.data(), known.data(), data.size()};
  std::array<Nba97GameGpuPacketDmaAccess, 16> dma_journal{};
  Nba97GameGpuPacketDmaGraphicsBinding binding{};
  Nba97GameGraphicsSubmitContext context{};
  Nba97GameGraphicsSubmitProgress progress{};
  size_t typed_callbacks{};

  explicit Fixture(size_t dma_budget = 8u) {
    write32(data.data(), UINT32_C(0x800c56c4), 1u);
    write32(data.data(), UINT32_C(0x800c56c8), 0u);
    data[UINT32_C(0x800c55c1) - base] = 0u;
    write32(data.data(), UINT32_C(0x800c56cc), 0u);
    write32(data.data(), UINT32_C(0x800c55cc), 0u);
    write32(data.data(), UINT32_C(0x800c5694), port0);
    write32(data.data(), UINT32_C(0x800c5698), port1);
    write32(data.data(), UINT32_C(0x800c569c), port2);
    write32(data.data(), UINT32_C(0x800c56a0), port3);
    write32(data.data(), port0, UINT32_C(0x04000000));
    nba97_game_gpu_packet_dma_graphics_binding_init(
        &binding, dma_budget, dma_journal.data(), dma_journal.size(),
        typed_service, this);
    context.memory = {&region, 1u};
    context.operation_budget = 256u;
    for (unsigned index = 0u; index != 32u; ++index)
      context.machine.registers.gpr[index] =
          {UINT32_C(0x31000000) + index, 15u};
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        {UINT32_C(0x8009b1f8), 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {packet, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A3] =
        {UINT32_C(0x12345678), 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {stack, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {UINT32_C(0x80099b60), 15u};
    context.machine.hi = {UINT32_C(0x11223344), 3u};
    context.machine.lo = {UINT32_C(0x55667788), 12u};
    context.io = dispatch;
    context.user = this;
  }

  static int typed_service(void *opaque, const Nba97GameTextMemory *,
                           const Nba97GameGraphicsSubmitEvent *event,
                           Nba97GameGraphicsSubmitMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    ++fixture.typed_callbacks;
    if (event->kind == NBA97_GAME_GRAPHICS_SUBMIT_CRITICAL &&
        event->pc == UINT32_C(0x8009b304))
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
          {UINT32_C(0x55), 15u};
    return 1;
  }

  static int dispatch(void *opaque, const Nba97GameTextMemory *memory,
                      const Nba97GameGraphicsSubmitEvent *event,
                      Nba97GameGraphicsSubmitMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    return nba97_game_gpu_packet_dma_from_graphics_submit(
        &fixture.binding, memory, event, machine);
  }

  int run() { return nba97_game_graphics_submit(&context, &progress); }
};

int accepting_fallback(void *opaque, const Nba97GameTextMemory *,
                       const Nba97GameGraphicsSubmitEvent *,
                       Nba97GameGraphicsSubmitMachine *) {
  ++*static_cast<size_t *>(opaque);
  return 1;
}

void natural_complete() {
  Fixture fixture;
  check(fixture.run() == NBA97_TEXT_COMPLETE, "natural BI direct completes");
  check(fixture.progress.completed == 1u && fixture.progress.queued == 0u,
        "natural BI direct path");
  check(fixture.binding.invocations == 1u &&
            fixture.binding.result == NBA97_TEXT_COMPLETE &&
            fixture.binding.progress.completed == 1u,
        "one completed nested DMA");
  check(fixture.binding.progress.operations == 8u,
        "nested DMA eight accesses");
  check(read32(fixture.data.data(), Fixture::port0) == UINT32_C(0x04000002),
        "natural first MMIO word");
  check(read32(fixture.data.data(), Fixture::port1) == Fixture::packet,
        "natural raw packet MMIO word");
  check(read32(fixture.data.data(), Fixture::port2) == 0u,
        "natural zero MMIO word");
  check(read32(fixture.data.data(), Fixture::port3) == UINT32_C(0x01000401),
        "natural DMA start MMIO word");
  check(read32(fixture.data.data(), UINT32_C(0x800c56b4)) ==
            UINT32_C(0x8009b1f8),
        "BI publishes last function after DMA");
  check(read32(fixture.data.data(), UINT32_C(0x800c56b8)) == Fixture::packet,
        "BI publishes last packet after DMA");
  check(read32(fixture.data.data(), UINT32_C(0x800c56bc)) ==
            UINT32_C(0x12345678),
        "BI publishes last callback argument");
  check(fixture.progress.machine.hi.word == UINT32_C(0x11223344) &&
            fixture.progress.machine.hi.known_mask == 3u,
        "natural hi preserved");
  check(fixture.progress.machine.lo.word == UINT32_C(0x55667788) &&
            fixture.progress.machine.lo.known_mask == 12u,
        "natural lo preserved");
}

void natural_failure_prefix() {
  Fixture fixture(3u);
  const uint32_t last_before =
      read32(fixture.data.data(), UINT32_C(0x800c56b4));
  check(fixture.run() == NBA97_TEXT_IO_REFUSED,
        "BI promotes nested limit to callback refusal");
  check(fixture.binding.result == NBA97_TEXT_LIMIT,
        "nested limit remains inspectable");
  check(fixture.binding.progress.operations == 3u &&
            fixture.binding.progress.stopped_pc == UINT32_C(0x8009b218),
        "nested exact failure prefix");
  check(fixture.progress.stopped_pc == UINT32_C(0x8009b3a8),
        "parent remains at indirect call");
  check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            UINT32_C(0x8009b3b0),
        "parent JAL ra remains live");
  check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Fixture::stack - 0x28u,
        "parent frame remains allocated");
  check(read32(fixture.data.data(), UINT32_C(0x800c56b4)) == last_before,
        "stopped child blocks parent last-call stores");
  check(read32(fixture.data.data(), Fixture::port0) == UINT32_C(0x04000002),
        "nested first store retained");
}

void adapter_guards() {
  Fixture fixture;
  auto machine = fixture.context.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
      {UINT32_C(0x8009b3b0), 15u};
  Nba97GameGraphicsSubmitEvent event{};
  event.pc = UINT32_C(0x8009b3a8);
  event.delay_slot_pc = UINT32_C(0x8009b3ac);
  event.entry = UINT32_C(0x8009b1f8);
  event.kind = NBA97_GAME_GRAPHICS_SUBMIT_INDIRECT;
  event.argument_count = 2u;
  size_t fallback_calls = 0u;
  nba97_game_gpu_packet_dma_graphics_binding_init(
      &fixture.binding, 8u, fixture.dma_journal.data(),
      fixture.dma_journal.size(), accepting_fallback, &fallback_calls);
  auto original = machine;

  event.kind = NBA97_GAME_GRAPHICS_SUBMIT_CRITICAL;
  check(nba97_game_gpu_packet_dma_from_graphics_submit(
            &fixture.binding, &fixture.context.memory, &event, &machine) == 0,
        "same entry wrong kind rejected");
  check(fallback_calls == 0u, "assigned entry never falls back");
  check(std::memcmp(&machine, &original, sizeof(machine)) == 0,
        "malformed assigned event immutable");

  event.kind = NBA97_GAME_GRAPHICS_SUBMIT_INDIRECT;
  event.entry = UINT32_C(0x8009b248);
  const size_t own_before_dynamic = fixture.binding.invocations;
  const uint32_t port_before_dynamic =
      read32(fixture.data.data(), Fixture::port0);
  check(nba97_game_gpu_packet_dma_from_graphics_submit(
            &fixture.binding, &fixture.context.memory, &event, &machine) == 1,
        "generic indirect with another target falls back");
  check(fallback_calls == 1u &&
            fixture.binding.invocations == own_before_dynamic,
        "other indirect does not invoke DMA owner");
  check(read32(fixture.data.data(), Fixture::port0) == port_before_dynamic,
        "other indirect produces no DMA effect");

  event.entry = UINT32_C(0x8009b1f8);
  event.delay_slot_pc++;
  check(nba97_game_gpu_packet_dma_from_graphics_submit(
            &fixture.binding, &fixture.context.memory, &event, &machine) == 0,
        "wrong delay rejected");
  event.delay_slot_pc--;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 14u;
  check(nba97_game_gpu_packet_dma_from_graphics_submit(
            &fixture.binding, &fixture.context.memory, &event, &machine) == 0,
        "partial ra rejected");

  machine = original;
  event.kind = NBA97_GAME_GRAPHICS_SUBMIT_CRITICAL;
  event.entry = UINT32_C(0x800986f8);
  check(nba97_game_gpu_packet_dma_from_graphics_submit(
            &fixture.binding, &fixture.context.memory, &event, &machine) == 1,
        "unrelated event falls back");
  check(fallback_calls == 2u && fixture.binding.fallback_invocations == 2u,
        "fallback accounting");
}

} // namespace

int main() {
  natural_complete();
  natural_failure_prefix();
  adapter_guards();
  std::printf("game_gpu_packet_dma_integration_tests: %zu checks passed\n",
              checks);
  return 0;
}
