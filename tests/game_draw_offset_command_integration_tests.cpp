#include "game_draw_area_end_adapter.h"
#include "game_draw_area_start_adapter.h"
#include "game_draw_offset_command_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

size_t checks;
void check(bool condition, const char *message) {
  ++checks;
  if (condition)
    return;
  std::fprintf(stderr, "game_draw_offset_command_integration_tests: %s\n",
               message);
  std::exit(1);
}

void put16(uint8_t *bytes, size_t offset, uint16_t value) {
  bytes[offset] = static_cast<uint8_t>(value);
  bytes[offset + 1u] = static_cast<uint8_t>(value >> 8u);
}

uint32_t get32(const uint8_t *bytes, size_t offset) {
  uint32_t value = 0u;
  for (unsigned byte = 0u; byte != 4u; ++byte)
    value |= static_cast<uint32_t>(bytes[offset + byte]) << (8u * byte);
  return value;
}

struct Fixture {
  static constexpr uint32_t kStack = UINT32_C(0x80100000);
  static constexpr uint32_t kPacket = UINT32_C(0x80200000);
  static constexpr uint32_t kEnvironment = UINT32_C(0x80300000);
  std::array<uint8_t, 512> stack{};
  std::array<uint8_t, 512> packet{};
  std::array<uint8_t, 512> environment{};
  std::array<uint8_t, 16> globals{};
  std::array<uint8_t, 512> stack_known{};
  std::array<uint8_t, 512> packet_known{};
  std::array<uint8_t, 512> environment_known{};
  std::array<uint8_t, 16> globals_known{};
  std::array<Nba97GameTextRegion, 4> regions{};
  std::array<Nba97GameDrawPacketAccess, 128> parent_journal{};
  std::array<Nba97GameDrawAreaStartAccess, 4> start_journal{};
  std::array<Nba97GameDrawAreaEndAccess, 4> end_journal{};
  std::array<Nba97GameDrawOffsetCommandAccess, 2> offset_journal{};
  Nba97GameDrawPacketContext context{};
  Nba97GameDrawPacketProgress progress{};
  Nba97GameDrawAreaStartPacketBinding start_binding{};
  Nba97GameDrawAreaEndPacketBinding end_binding{};
  Nba97GameDrawOffsetCommandPacketBinding offset_binding{};
  size_t other_calls{};

  explicit Fixture(size_t offset_budget = 1u) {
    stack_known.fill(1u);
    packet_known.fill(1u);
    environment_known.fill(1u);
    globals_known.fill(1u);
    packet.fill(0x5au);
    regions = {{{kStack, stack.data(), stack_known.data(), stack.size()},
                {kPacket, packet.data(), packet_known.data(), packet.size()},
                {kEnvironment, environment.data(), environment_known.data(),
                 environment.size()},
                {UINT32_C(0x800c55c0), globals.data(), globals_known.data(),
                 globals.size()}}};
    context.memory = {regions.data(), regions.size()};
    context.operation_budget = 256u;
    context.io = callback;
    context.user = this;
    context.access_journal = parent_journal.data();
    context.access_journal_capacity = parent_journal.size();
    for (unsigned reg = 0u; reg != 32u; ++reg)
      context.machine.registers.gpr[reg] = {UINT32_C(0x51000000) +
                                                reg * UINT32_C(0x01010101),
                                            static_cast<uint8_t>(reg % 16u)};
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {kPacket, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {kEnvironment,
                                                                15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {kStack + 0x80u,
                                                                15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
        UINT32_C(0x80099b28), 15u};
    context.machine.hi = {UINT32_C(0x12345678), 5u};
    context.machine.lo = {UINT32_C(0x89abcdef), 10u};
    put16(environment.data(), 0u, 100u);
    put16(environment.data(), 2u, 200u);
    put16(environment.data(), 4u, 64u);
    put16(environment.data(), 6u, 32u);
    put16(environment.data(), 8u, 3u);
    put16(environment.data(), 10u, 4u);
    put16(environment.data(), 0x14u, UINT16_C(0x3456));
    environment[0x16] = 0x22u;
    environment[0x17] = 0x33u;
    environment[0x19] = 0x11u;
    environment[0x1a] = 0x22u;
    environment[0x1b] = 0x33u;
    globals[0] = 1u;
    put16(globals.data(), 4u, 640u);
    put16(globals.data(), 6u, 480u);
    nba97_game_draw_area_start_packet_binding_init(
        &start_binding, 3u, start_journal.data(), start_journal.size(), nullptr,
        nullptr);
    nba97_game_draw_area_end_packet_binding_init(
        &end_binding, 3u, end_journal.data(), end_journal.size(), nullptr,
        nullptr);
    nba97_game_draw_offset_command_packet_binding_init(
        &offset_binding, offset_budget, offset_journal.data(),
        offset_journal.size(), nullptr, nullptr);
  }

  static int callback(void *opaque, const Nba97GameTextMemory *memory,
                      const Nba97GameDrawPacketEvent *event,
                      Nba97GameDrawPacketMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    if (event->kind == NBA97_GAME_DRAW_PACKET_CHILD_8009A644 ||
        event->entry == UINT32_C(0x8009a644))
      return nba97_game_draw_area_start_from_packet(&fixture.start_binding,
                                                    memory, event, machine);
    if (event->kind == NBA97_GAME_DRAW_PACKET_CHILD_8009A710 ||
        event->entry == UINT32_C(0x8009a710))
      return nba97_game_draw_area_end_from_packet(&fixture.end_binding, memory,
                                                  event, machine);
    if (event->kind == NBA97_GAME_DRAW_PACKET_CHILD_8009A7DC ||
        event->entry == UINT32_C(0x8009a7dc))
      return nba97_game_draw_offset_command_from_packet(&fixture.offset_binding,
                                                        memory, event, machine);
    ++fixture.other_calls;
    machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {
        UINT32_C(0x91000000) + event->kind, 15u};
    return 1;
  }

  int run() { return nba97_game_draw_packet(&context, &progress); }
};

Nba97GameDrawPacketEvent exact_event() {
  Nba97GameDrawPacketEvent event{};
  event.pc = UINT32_C(0x8009a3b0);
  event.delay_slot_pc = UINT32_C(0x8009a3b4);
  event.entry = UINT32_C(0x8009a7dc);
  event.kind = NBA97_GAME_DRAW_PACKET_CHILD_8009A7DC;
  event.argument_count = 2u;
  return event;
}

void natural_packet_completion() {
  Fixture fixture;
  const auto entry_hi = fixture.context.machine.hi;
  const auto entry_lo = fixture.context.machine.lo;
  check(fixture.run() == NBA97_TEXT_COMPLETE, "natural packet completes");
  check(fixture.start_binding.completions == 1u, "start owner completes");
  check(fixture.end_binding.completions == 1u, "end owner completes");
  check(fixture.offset_binding.invocations == 1u, "one offset invocation");
  check(fixture.offset_binding.completions == 1u, "one offset completion");
  check(fixture.offset_binding.result == NBA97_TEXT_COMPLETE,
        "nested result retained");
  check(fixture.offset_binding.event.pc == UINT32_C(0x8009a3b0),
        "natural call pc");
  check(fixture.offset_binding.event.delay_slot_pc == UINT32_C(0x8009a3b4),
        "natural delay pc");
  check(fixture.offset_binding.event.entry == UINT32_C(0x8009a7dc),
        "natural entry");
  check(fixture.offset_binding.event.argument_count == 2u,
        "natural two arguments");
  check(get32(fixture.packet.data(), 4u) == UINT32_C(0xe30c8064),
        "real start command stored");
  check(get32(fixture.packet.data(), 8u) == UINT32_C(0xe40e70a3),
        "real end command stored");
  check(get32(fixture.packet.data(), 12u) == UINT32_C(0xe5004003),
        "real offset command stored at packet plus twelve");
  check(fixture.offset_binding.progress.return_v0.word == UINT32_C(0xe5004003),
        "offset result retained");
  check(fixture.offset_binding.progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_A1]
                .word == 4u,
        "raw y offset remains live");
  check(fixture.other_calls == 2u, "two later packet children remain typed");
  check(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
          Fixture::kStack + 0x80u,
      "parent stack restored");
  check(fixture.progress.restored_return_address.word == UINT32_C(0x80099b28),
        "parent return address restored");
  check(std::memcmp(&fixture.progress.machine.hi, &entry_hi,
                    sizeof(entry_hi)) == 0,
        "hi passes through all owners");
  check(std::memcmp(&fixture.progress.machine.lo, &entry_lo,
                    sizeof(entry_lo)) == 0,
        "lo passes through all owners");
}

void natural_limit_prefix() {
  Fixture fixture(0u);
  const uint32_t offset_before = get32(fixture.packet.data(), 12u);
  check(fixture.run() == NBA97_TEXT_IO_REFUSED,
        "parent exposes nested refusal");
  check(fixture.start_binding.result == NBA97_TEXT_COMPLETE,
        "preceding start completes");
  check(fixture.end_binding.result == NBA97_TEXT_COMPLETE,
        "preceding end completes");
  check(fixture.offset_binding.result == NBA97_TEXT_LIMIT,
        "offset limit remains explicit");
  check(fixture.offset_binding.progress.operations == 0u,
        "zero offset operation prefix");
  check(fixture.offset_binding.progress.stopped_pc == UINT32_C(0x8009a7e4),
        "offset stops at type read");
  check(get32(fixture.packet.data(), 4u) == UINT32_C(0xe30c8064),
        "prior start store remains");
  check(get32(fixture.packet.data(), 8u) == UINT32_C(0xe40e70a3),
        "prior end store remains");
  check(get32(fixture.packet.data(), 12u) == offset_before,
        "offset store remains blocked");
  check(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
          UINT32_C(0x8009a3b8),
      "offset jal return address remains live");
  check(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
          Fixture::kStack + 0x58u,
      "allocated packet frame remains live");
}

int accepting_fallback(void *opaque, const Nba97GameTextMemory *,
                       const Nba97GameDrawPacketEvent *,
                       Nba97GameDrawPacketMachine *) {
  ++*static_cast<size_t *>(opaque);
  return 1;
}

void adapter_guards_and_machine_copy() {
  Fixture fixture;
  auto machine = fixture.context.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {UINT32_C(0xabc), 15u};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {UINT32_C(0xdef), 15u};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {UINT32_C(0x8009a3b8),
                                                      15u};
  auto event = exact_event();
  check(nba97_game_draw_offset_command_from_packet(&fixture.offset_binding,
                                                   &fixture.context.memory,
                                                   &event, &machine) == 1,
        "exact adapter event accepted");
  check(fixture.offset_binding.result == NBA97_TEXT_COMPLETE,
        "direct result complete");
  check(machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            UINT32_C(0xe5defabc),
        "direct E5 command returned");
  check(machine.hi.word == fixture.context.machine.hi.word &&
            machine.hi.known_mask == fixture.context.machine.hi.known_mask,
        "direct hi retained");
  check(machine.lo.word == fixture.context.machine.lo.word &&
            machine.lo.known_mask == fixture.context.machine.lo.known_mask,
        "direct lo retained");

  size_t fallback_calls = 0u;
  nba97_game_draw_offset_command_packet_binding_init(
      &fixture.offset_binding, 1u, nullptr, 0u, accepting_fallback,
      &fallback_calls);
  machine = fixture.context.machine;
  const auto original = machine;
  event.kind = NBA97_GAME_DRAW_PACKET_CHILD_8009A824;
  event.entry = UINT32_C(0x8009a824);
  check(nba97_game_draw_offset_command_from_packet(&fixture.offset_binding,
                                                   &fixture.context.memory,
                                                   &event, &machine) == 1,
        "unrelated event falls back");
  check(fallback_calls == 1u, "fallback counted");

  event = exact_event();
  event.delay_slot_pc++;
  check(nba97_game_draw_offset_command_from_packet(&fixture.offset_binding,
                                                   &fixture.context.memory,
                                                   &event, &machine) == 0,
        "wrong delay refused");
  check(std::memcmp(&machine, &original, sizeof(machine)) == 0,
        "guard refusal leaves machine unchanged");
  event = exact_event();
  machine = original;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 14u;
  check(nba97_game_draw_offset_command_from_packet(&fixture.offset_binding,
                                                   &fixture.context.memory,
                                                   &event, &machine) == 0,
        "unknown jal ra refused");
  machine = original;
  machine.registers.gpr[19].known_mask = 16u;
  check(nba97_game_draw_offset_command_from_packet(&fixture.offset_binding,
                                                   &fixture.context.memory,
                                                   &event, &machine) == 0,
        "malformed machine refused");
  machine = original;
  Nba97GameTextMemory malformed_memory{nullptr, 1u};
  check(nba97_game_draw_offset_command_from_packet(
            &fixture.offset_binding, &malformed_memory, &event, &machine) == 0,
        "malformed memory refused");
  nba97_game_draw_offset_command_packet_binding_init(
      &fixture.offset_binding, 1u, nullptr, 1u, nullptr, nullptr);
  check(nba97_game_draw_offset_command_from_packet(&fixture.offset_binding,
                                                   &fixture.context.memory,
                                                   &event, &machine) == 0,
        "malformed journal refused");
  check(fixture.offset_binding.result == NBA97_TEXT_ARGUMENT,
        "guard status retained");
}

} // namespace

int main() {
  natural_packet_completion();
  natural_limit_prefix();
  adapter_guards_and_machine_copy();
  std::printf("game_draw_offset_command_integration_tests: %zu checks passed\n",
              checks);
  return 0;
}