#include "game_draw_area_end_adapter.h"
#include "game_draw_area_start_adapter.h"
#include "game_draw_mode_command_adapter.h"
#include "game_draw_offset_command_adapter.h"

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
    std::fprintf(stderr, "game_draw_mode_command_integration_tests: %s\n",
                 message);
    std::exit(1);
  }
}

void write16(uint8_t *bytes, uint32_t address, uint16_t value) {
  const size_t offset = address - UINT32_C(0x80000000);
  bytes[offset] = static_cast<uint8_t>(value);
  bytes[offset + 1u] = static_cast<uint8_t>(value >> 8u);
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
  static constexpr uint32_t packet = UINT32_C(0x80090000);
  static constexpr uint32_t environment = UINT32_C(0x80080000);
  std::vector<uint8_t> data = std::vector<uint8_t>(size, 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(size, 1u);
  Nba97GameTextRegion region{base, data.data(), known.data(), data.size()};
  Nba97GameDrawModeCommandPacketBinding mode{};
  Nba97GameDrawOffsetCommandPacketBinding offset{};
  Nba97GameDrawAreaEndPacketBinding area_end{};
  Nba97GameDrawAreaStartPacketBinding area_start{};
  Nba97GameDrawPacketContext context{};
  Nba97GameDrawPacketProgress progress{};
  size_t texture_calls{};

  explicit Fixture(size_t mode_budget = 1u) {
    write16(data.data(), environment, 10u);
    write16(data.data(), environment + 2u, 20u);
    write16(data.data(), environment + 4u, 64u);
    write16(data.data(), environment + 6u, 32u);
    write16(data.data(), environment + 8u, 3u);
    write16(data.data(), environment + 10u, 4u);
    write16(data.data(), environment + 0x14u, UINT16_C(0x3456));
    data[environment - base + 0x16u] = 0x22u;
    data[environment - base + 0x17u] = 0x33u;
    data[environment - base + 0x18u] = 0u;
    data[UINT32_C(0x800c55c0) - base] = 1u;
    write16(data.data(), UINT32_C(0x800c55c4), 640u);
    write16(data.data(), UINT32_C(0x800c55c6), 480u);

    nba97_game_draw_area_start_packet_binding_init(
        &area_start, 8u, nullptr, 0u, texture_service, this);
    nba97_game_draw_area_end_packet_binding_init(
        &area_end, 8u, nullptr, 0u, nba97_game_draw_area_start_from_packet,
        &area_start);
    nba97_game_draw_offset_command_packet_binding_init(
        &offset, 4u, nullptr, 0u, nba97_game_draw_area_end_from_packet,
        &area_end);
    nba97_game_draw_mode_command_packet_binding_init(
        &mode, mode_budget, nullptr, 0u,
        nba97_game_draw_offset_command_from_packet, &offset);

    context.memory = {&region, 1u};
    context.operation_budget = 256u;
    for (unsigned index = 0u; index != 32u; ++index)
      context.machine.registers.gpr[index] =
          {UINT32_C(0x42000000) + index, 15u};
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {packet, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] =
        {environment, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {stack, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {UINT32_C(0x80099b28), 15u};
    context.machine.hi = {UINT32_C(0x11223344), 5u};
    context.machine.lo = {UINT32_C(0x55667788), 10u};
    context.io = nba97_game_draw_mode_command_from_packet;
    context.user = &mode;
  }

  static int texture_service(void *opaque, const Nba97GameTextMemory *,
                             const Nba97GameDrawPacketEvent *event,
                             Nba97GameDrawPacketMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    check(event->kind == NBA97_GAME_DRAW_PACKET_CHILD_8009A824,
          "only texture child remains typed");
    check(event->pc == UINT32_C(0x8009a3d4) &&
              event->delay_slot_pc == UINT32_C(0x8009a3d8) &&
              event->entry == UINT32_C(0x8009a824) &&
              event->argument_count == 1u,
          "texture child exact metadata");
    check(machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
              environment + 12u,
          "texture child argument");
    check(read32(fixture.data.data(), packet + 0x10u) == UINT32_C(0xe1003c56),
          "BM word stored in next JAL delay");
    ++fixture.texture_calls;
    machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
        {UINT32_C(0xe2001234), 15u};
    return 1;
  }

  int run() { return nba97_game_draw_packet(&context, &progress); }
};

int accepting_fallback(void *opaque, const Nba97GameTextMemory *,
                       const Nba97GameDrawPacketEvent *,
                       Nba97GameDrawPacketMachine *) {
  ++*static_cast<size_t *>(opaque);
  return 1;
}

void natural_complete() {
  Fixture fixture;
  check(fixture.run() == NBA97_TEXT_COMPLETE, "natural BG completes");
  check(fixture.progress.completed == 1u, "BG completion flag");
  check(fixture.area_start.completions == 1u &&
            fixture.area_end.completions == 1u &&
            fixture.offset.completions == 1u && fixture.mode.completions == 1u,
        "four real packet helpers complete");
  check(fixture.mode.event.pc == UINT32_C(0x8009a3c8) &&
            fixture.mode.event.delay_slot_pc == UINT32_C(0x8009a3cc) &&
            fixture.mode.event.entry == UINT32_C(0x8009a5e8) &&
            fixture.mode.event.argument_count == 3u,
        "BM exact natural event");
  check(fixture.texture_calls == 1u, "one typed texture child");
  check(read32(fixture.data.data(), Fixture::packet + 4u) ==
            fixture.area_start.progress.return_v0.word,
        "real area-start word stored");
  check(read32(fixture.data.data(), Fixture::packet + 8u) ==
            fixture.area_end.progress.return_v0.word,
        "real area-end word stored");
  check(read32(fixture.data.data(), Fixture::packet + 12u) ==
            fixture.offset.progress.return_v0.word,
        "real offset word stored");
  check(read32(fixture.data.data(), Fixture::packet + 16u) ==
            UINT32_C(0xe1003c56),
        "real mode word stored");
  check(read32(fixture.data.data(), Fixture::packet + 20u) ==
            UINT32_C(0xe2001234),
        "typed texture word stored");
  check(fixture.data[Fixture::packet - Fixture::base + 3u] == 6u,
        "six-word packet count");
  check(fixture.progress.machine.hi.word == UINT32_C(0x11223344) &&
            fixture.progress.machine.hi.known_mask == 5u,
        "natural hi preserved");
  check(fixture.progress.machine.lo.word == UINT32_C(0x55667788) &&
            fixture.progress.machine.lo.known_mask == 10u,
        "natural lo preserved");
}

void natural_failure_prefix() {
  Fixture fixture(0u);
  std::memset(fixture.data.data() + Fixture::packet - Fixture::base, 0x77, 64u);
  check(fixture.run() == NBA97_TEXT_IO_REFUSED,
        "BG reports nested BM refusal");
  check(fixture.mode.result == NBA97_TEXT_LIMIT,
        "nested BM budget result retained");
  check(fixture.mode.progress.stopped_pc == UINT32_C(0x8009a5f0) &&
            fixture.mode.progress.operations == 0u,
        "nested BM zero-budget prefix");
  check(fixture.progress.stopped_pc == UINT32_C(0x8009a3c8),
        "BG stopped at BM call");
  check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            UINT32_C(0x8009a3d0),
        "BG JAL ra retained");
  check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Fixture::stack - 0x28u,
        "BG frame remains live");
  check(read32(fixture.data.data(), Fixture::packet + 4u) !=
            UINT32_C(0x77777777) &&
            read32(fixture.data.data(), Fixture::packet + 8u) !=
                UINT32_C(0x77777777) &&
            read32(fixture.data.data(), Fixture::packet + 12u) !=
                UINT32_C(0x77777777),
        "three earlier real words remain stored");
  check(read32(fixture.data.data(), Fixture::packet + 16u) ==
            UINT32_C(0x77777777),
        "BM failure precedes next JAL delay store");
  check(fixture.texture_calls == 0u, "texture child not reached");
}

void adapter_guards() {
  Fixture fixture;
  auto machine = fixture.context.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
      {UINT32_C(0x8009a3d0), 15u};
  Nba97GameDrawPacketEvent event{};
  event.pc = UINT32_C(0x8009a3c8);
  event.delay_slot_pc = UINT32_C(0x8009a3cc);
  event.entry = UINT32_C(0x8009a5e8);
  event.kind = NBA97_GAME_DRAW_PACKET_CHILD_8009A5E8;
  event.argument_count = 3u;
  size_t fallback_calls = 0u;
  nba97_game_draw_mode_command_packet_binding_init(
      &fixture.mode, 1u, nullptr, 0u, accepting_fallback, &fallback_calls);
  auto original = machine;

  event.kind = NBA97_GAME_DRAW_PACKET_CHILD_8009A824;
  check(nba97_game_draw_mode_command_from_packet(
            &fixture.mode, &fixture.context.memory, &event, &machine) == 0,
        "assigned entry wrong kind rejects");
  check(fallback_calls == 0u, "assigned entry never falls back");
  check(std::memcmp(&machine, &original, sizeof(machine)) == 0,
        "malformed event immutable");

  event.kind = NBA97_GAME_DRAW_PACKET_CHILD_8009A5E8;
  event.entry = UINT32_C(0x8009a5ec);
  check(nba97_game_draw_mode_command_from_packet(
            &fixture.mode, &fixture.context.memory, &event, &machine) == 0,
        "assigned kind wrong entry rejects");
  check(fallback_calls == 0u, "assigned kind never falls back");

  event.entry = UINT32_C(0x8009a5e8);
  event.delay_slot_pc++;
  check(nba97_game_draw_mode_command_from_packet(
            &fixture.mode, &fixture.context.memory, &event, &machine) == 0,
        "wrong delay rejects");
  event.delay_slot_pc--;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 14u;
  check(nba97_game_draw_mode_command_from_packet(
            &fixture.mode, &fixture.context.memory, &event, &machine) == 0,
        "partial ra rejects");

  machine = original;
  event.kind = NBA97_GAME_DRAW_PACKET_CHILD_8009A824;
  event.entry = UINT32_C(0x8009a824);
  check(nba97_game_draw_mode_command_from_packet(
            &fixture.mode, &fixture.context.memory, &event, &machine) == 1,
        "unrelated event falls back");
  check(fallback_calls == 1u && fixture.mode.fallback_invocations == 1u,
        "fallback accounting");
}

} // namespace

int main() {
  natural_complete();
  natural_failure_prefix();
  adapter_guards();
  std::printf("game_draw_mode_command_integration_tests: %zu checks passed\n",
              checks);
  return 0;
}
