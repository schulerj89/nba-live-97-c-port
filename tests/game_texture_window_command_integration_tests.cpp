#include "game_draw_area_end_adapter.h"
#include "game_draw_area_start_adapter.h"
#include "game_draw_mode_command_adapter.h"
#include "game_draw_offset_command_adapter.h"
#include "game_texture_window_command_adapter.h"

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
    std::fprintf(stderr,
                 "game_texture_window_command_integration_tests: %s\n",
                 message);
    std::exit(1);
  }
}

bool same_word(const Nba97GameDrawPacketWord &left,
               const Nba97GameDrawPacketWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool same_machine(const Nba97GameDrawPacketMachine &left,
                  const Nba97GameDrawPacketMachine &right) {
  for (unsigned index = 0u; index != 32u; ++index)
    if (!same_word(left.registers.gpr[index], right.registers.gpr[index]))
      return false;
  return same_word(left.hi, right.hi) && same_word(left.lo, right.lo);
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
  Nba97GameTextureWindowCommandPacketBinding texture{};
  Nba97GameDrawModeCommandPacketBinding mode{};
  Nba97GameDrawOffsetCommandPacketBinding offset{};
  Nba97GameDrawAreaEndPacketBinding area_end{};
  Nba97GameDrawAreaStartPacketBinding area_start{};
  Nba97GameDrawPacketContext context{};
  Nba97GameDrawPacketProgress progress{};

  explicit Fixture(size_t texture_budget = 8u) {
    write16(data.data(), environment, 10u);
    write16(data.data(), environment + 2u, 20u);
    write16(data.data(), environment + 4u, 64u);
    write16(data.data(), environment + 6u, 32u);
    write16(data.data(), environment + 8u, 3u);
    write16(data.data(), environment + 10u, 4u);
    data[environment - base + 12u] = 0x18u;
    data[environment - base + 14u] = 0x20u;
    write16(data.data(), environment + 16u, 8u);
    write16(data.data(), environment + 18u, 16u);
    write16(data.data(), environment + 0x14u, UINT16_C(0x3456));
    data[environment - base + 0x16u] = 0x22u;
    data[environment - base + 0x17u] = 0x33u;
    data[environment - base + 0x18u] = 0u;
    data[UINT32_C(0x800c55c0) - base] = 1u;
    write16(data.data(), UINT32_C(0x800c55c4), 640u);
    write16(data.data(), UINT32_C(0x800c55c6), 480u);

    nba97_game_texture_window_command_packet_binding_init(
        &texture, texture_budget, nullptr, 0u, nullptr, nullptr);
    nba97_game_draw_area_start_packet_binding_init(
        &area_start, 8u, nullptr, 0u,
        nba97_game_texture_window_command_from_packet, &texture);
    nba97_game_draw_area_end_packet_binding_init(
        &area_end, 8u, nullptr, 0u, nba97_game_draw_area_start_from_packet,
        &area_start);
    nba97_game_draw_offset_command_packet_binding_init(
        &offset, 4u, nullptr, 0u, nba97_game_draw_area_end_from_packet,
        &area_end);
    nba97_game_draw_mode_command_packet_binding_init(
        &mode, 1u, nullptr, 0u, nba97_game_draw_offset_command_from_packet,
        &offset);

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
            fixture.offset.completions == 1u && fixture.mode.completions == 1u &&
            fixture.texture.completions == 1u,
        "all five real helpers complete");
  check(fixture.texture.event.pc == UINT32_C(0x8009a3d4) &&
            fixture.texture.event.delay_slot_pc == UINT32_C(0x8009a3d8) &&
            fixture.texture.event.entry == UINT32_C(0x8009a824) &&
            fixture.texture.event.argument_count == 1u,
        "BO exact natural event");
  check(fixture.texture.progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_A0]
                .word == UINT32_C(0x000003c0),
        "BO final a0 preserves owner result");
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
            fixture.mode.progress.return_v0.word,
        "real mode word stored in BO JAL delay");
  check(read32(fixture.data.data(), Fixture::packet + 20u) ==
            fixture.texture.progress.return_v0.word,
        "real texture word stored");
  check(fixture.texture.progress.return_v0.word == UINT32_C(0xe2020fdf) &&
            fixture.texture.progress.return_v0.known_mask == 15u,
        "real texture command encoding");
  check(fixture.data[Fixture::packet - Fixture::base + 3u] == 6u,
        "six-word packet count");
  check(fixture.progress.machine.hi.word == UINT32_C(0x11223344) &&
            fixture.progress.machine.hi.known_mask == 5u &&
            fixture.progress.machine.lo.word == UINT32_C(0x55667788) &&
            fixture.progress.machine.lo.known_mask == 10u,
        "natural HI LO preserved");
}

void natural_failure_prefix() {
  Fixture fixture(0u);
  std::memset(fixture.data.data() + Fixture::packet - Fixture::base, 0x77, 64u);
  check(fixture.run() == NBA97_TEXT_IO_REFUSED,
        "BG reports nested BO refusal");
  check(fixture.texture.result == NBA97_TEXT_LIMIT,
        "nested BO budget result retained");
  check(fixture.texture.progress.stopped_pc == UINT32_C(0x8009a834) &&
            fixture.texture.progress.operations == 0u,
        "nested BO zero-budget first-read prefix");
  check(fixture.progress.stopped_pc == UINT32_C(0x8009a3d4),
        "BG stopped at BO call");
  check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            UINT32_C(0x8009a3dc),
        "BO JAL ra retained");
  check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Fixture::stack - 0x38u,
        "BG and BO frames remain allocated");
  check(read32(fixture.data.data(), Fixture::packet + 4u) !=
                UINT32_C(0x77777777) &&
            read32(fixture.data.data(), Fixture::packet + 8u) !=
                UINT32_C(0x77777777) &&
            read32(fixture.data.data(), Fixture::packet + 12u) !=
                UINT32_C(0x77777777) &&
            read32(fixture.data.data(), Fixture::packet + 16u) !=
                UINT32_C(0x77777777),
        "four prior helper words remain stored");
  check(read32(fixture.data.data(), Fixture::packet + 20u) ==
            UINT32_C(0x77777777),
        "BO failure blocks E2 store");
}

void adapter_guards_and_fallback() {
  Fixture fixture;
  auto machine = fixture.context.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
      {Fixture::environment + 12u, 15u};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
      {UINT32_C(0x8009a3dc), 15u};
  Nba97GameDrawPacketEvent event{};
  event.pc = UINT32_C(0x8009a3d4);
  event.delay_slot_pc = UINT32_C(0x8009a3d8);
  event.entry = UINT32_C(0x8009a824);
  event.kind = NBA97_GAME_DRAW_PACKET_CHILD_8009A824;
  event.argument_count = 1u;
  size_t fallback_calls = 0u;
  nba97_game_texture_window_command_packet_binding_init(
      &fixture.texture, 8u, nullptr, 0u, accepting_fallback, &fallback_calls);
  auto original = machine;

  event.kind = NBA97_GAME_DRAW_PACKET_CHILD_8009A5E8;
  check(nba97_game_texture_window_command_from_packet(
            &fixture.texture, &fixture.context.memory, &event, &machine) == 0,
        "assigned entry wrong kind rejects");
  check(fallback_calls == 0u && same_machine(machine, original),
        "malformed event immutable and not forwarded");

  event.kind = NBA97_GAME_DRAW_PACKET_CHILD_8009A824;
  event.entry = UINT32_C(0x8009a828);
  check(nba97_game_texture_window_command_from_packet(
            &fixture.texture, &fixture.context.memory, &event, &machine) == 0,
        "assigned kind wrong entry rejects");
  event.entry = UINT32_C(0x8009a824);
  event.delay_slot_pc++;
  check(nba97_game_texture_window_command_from_packet(
            &fixture.texture, &fixture.context.memory, &event, &machine) == 0,
        "wrong delay rejects");
  event.delay_slot_pc--;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 14u;
  check(nba97_game_texture_window_command_from_packet(
            &fixture.texture, &fixture.context.memory, &event, &machine) == 0,
        "partial RA rejects");

  machine = original;
  event.kind = NBA97_GAME_DRAW_PACKET_CHILD_8009A5E8;
  event.entry = UINT32_C(0x8009a5e8);
  check(nba97_game_texture_window_command_from_packet(
            &fixture.texture, &fixture.context.memory, &event, &machine) == 1,
        "unrelated child falls back");
  check(fallback_calls == 1u && fixture.texture.fallback_invocations == 1u,
        "fallback accounting");
}

} // namespace

int main() {
  natural_complete();
  natural_failure_prefix();
  adapter_guards_and_fallback();
  std::printf("game_texture_window_command_integration_tests: %zu checks passed\n",
              checks);
  return 0;
}
