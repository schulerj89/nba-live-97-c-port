#include "game_draw_area_end_adapter.h"
#include "game_draw_area_start_adapter.h"

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
  std::fprintf(stderr, "game_draw_area_end_integration_tests: %s\n", message);
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
  Nba97GameDrawPacketContext context{};
  Nba97GameDrawPacketProgress progress{};
  Nba97GameDrawAreaStartPacketBinding start_binding{};
  Nba97GameDrawAreaEndPacketBinding end_binding{};
  size_t other_calls{};

  explicit Fixture(size_t child_budget = 3u) {
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
        &end_binding, child_budget, end_journal.data(), end_journal.size(),
        nullptr, nullptr);
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
    ++fixture.other_calls;
    machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {
        UINT32_C(0x91000000) + event->kind, 15u};
    return 1;
  }

  int run() { return nba97_game_draw_packet(&context, &progress); }
};

Nba97GameDrawPacketEvent exact_event() {
  Nba97GameDrawPacketEvent event{};
  event.pc = UINT32_C(0x8009a39c);
  event.delay_slot_pc = UINT32_C(0x8009a3a0);
  event.entry = UINT32_C(0x8009a710);
  event.kind = NBA97_GAME_DRAW_PACKET_CHILD_8009A710;
  event.argument_count = 2u;
  return event;
}

void natural_packet_completion() {
  Fixture fixture;
  const auto entry_hi = fixture.context.machine.hi;
  const auto entry_lo = fixture.context.machine.lo;
  check(fixture.run() == NBA97_TEXT_COMPLETE, "natural packet completes");
  check(fixture.start_binding.invocations == 1u, "one start invocation");
  check(fixture.start_binding.completions == 1u, "one start completion");
  check(fixture.end_binding.invocations == 1u, "one end invocation");
  check(fixture.end_binding.completions == 1u, "one end completion");
  check(fixture.end_binding.result == NBA97_TEXT_COMPLETE,
        "nested result retained");
  check(fixture.end_binding.event.pc == UINT32_C(0x8009a39c),
        "natural call pc");
  check(fixture.end_binding.event.delay_slot_pc == UINT32_C(0x8009a3a0),
        "natural delay pc");
  check(fixture.end_binding.event.entry == UINT32_C(0x8009a710),
        "natural entry");
  check(fixture.end_binding.event.argument_count == 2u, "natural two args");
  check(get32(fixture.packet.data(), 4u) == UINT32_C(0xe30c8064),
        "actual start command stored at packet plus four");
  check(get32(fixture.packet.data(), 8u) == UINT32_C(0xe40e70a3),
        "computed end command stored at packet plus eight");
  check(fixture.end_binding.progress.return_v0.word == UINT32_C(0xe40e70a3),
        "owner command retained");
  check(fixture.end_binding.progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_A1]
                .word == 231u,
        "packet computes inclusive y end before child");
  check(fixture.end_binding.progress.machine.registers
                .gpr[NBA97_MATCH_INITIALIZE_A2]
                .word == 639u,
        "end owner retains x limit minus one");
  check(fixture.other_calls == 3u, "three later children remain typed");
  check(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
          Fixture::kStack + 0x80u,
      "parent frame stack restored");
  check(fixture.progress.restored_return_address.word == UINT32_C(0x80099b28),
        "parent frame return address preserved");
  check(std::memcmp(&fixture.progress.machine.hi, &entry_hi,
                    sizeof(entry_hi)) == 0,
        "hi passes through owner and parent");
  check(std::memcmp(&fixture.progress.machine.lo, &entry_lo,
                    sizeof(entry_lo)) == 0,
        "lo passes through owner and parent");
}

void natural_limit_prefix() {
  Fixture fixture(0u);
  const uint32_t packet_before = get32(fixture.packet.data(), 8u);
  check(fixture.run() == NBA97_TEXT_IO_REFUSED,
        "parent exposes refused nested boundary");
  check(fixture.start_binding.result == NBA97_TEXT_COMPLETE,
        "preceding start child completes");
  check(fixture.end_binding.result == NBA97_TEXT_LIMIT,
        "nested limit remains explicit");
  check(fixture.end_binding.progress.operations == 0u,
        "zero owner operation prefix");
  check(fixture.end_binding.progress.stopped_pc == UINT32_C(0x8009a728),
        "owner stops at first read");
  check(get32(fixture.packet.data(), 4u) == UINT32_C(0xe30c8064),
        "preceding start store remains committed");
  check(get32(fixture.packet.data(), 8u) == packet_before,
        "end packet store after child is blocked");
  check(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
          UINT32_C(0x8009a3a4),
      "jal return address remains live on refusal");
  check(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
          Fixture::kStack + 0x58u,
      "allocated parent frame remains live");
}

int accepting_fallback(void *opaque, const Nba97GameTextMemory *,
                       const Nba97GameDrawPacketEvent *,
                       Nba97GameDrawPacketMachine *) {
  ++*static_cast<size_t *>(opaque);
  return 1;
}

void adapter_guards_and_full_machine_prefix() {
  Fixture fixture;
  auto machine = fixture.context.machine;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {1234u, 15u};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {2345u, 15u};
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {UINT32_C(0x8009a3a4),
                                                      15u};
  auto event = exact_event();
  check(nba97_game_draw_area_end_from_packet(&fixture.end_binding,
                                             &fixture.context.memory, &event,
                                             &machine) == 1,
        "direct exact adapter event accepted");
  check(fixture.end_binding.result == NBA97_TEXT_COMPLETE,
        "direct adapter result complete");
  check(machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            UINT32_C(0xe41df27f),
        "direct machine receives command");
  check(machine.hi.word == fixture.context.machine.hi.word &&
            machine.hi.known_mask == fixture.context.machine.hi.known_mask,
        "direct full-machine hi retained");
  check(machine.lo.word == fixture.context.machine.lo.word &&
            machine.lo.known_mask == fixture.context.machine.lo.known_mask,
        "direct full-machine lo retained");

  size_t fallback_calls = 0u;
  nba97_game_draw_area_end_packet_binding_init(&fixture.end_binding, 3u,
                                               nullptr, 0u, accepting_fallback,
                                               &fallback_calls);
  machine = fixture.context.machine;
  const auto original = machine;
  event.kind = NBA97_GAME_DRAW_PACKET_CHILD_8009A824;
  event.entry = UINT32_C(0x8009a824);
  check(nba97_game_draw_area_end_from_packet(&fixture.end_binding,
                                             &fixture.context.memory, &event,
                                             &machine) == 1,
        "unrelated child falls back");
  check(fallback_calls == 1u, "fallback invocation counted");

  event = exact_event();
  event.delay_slot_pc++;
  check(nba97_game_draw_area_end_from_packet(&fixture.end_binding,
                                             &fixture.context.memory, &event,
                                             &machine) == 0,
        "wrong delay refused");
  check(std::memcmp(&machine, &original, sizeof(machine)) == 0,
        "guard failure leaves machine unchanged");
  event = exact_event();
  machine = original;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 14u;
  check(nba97_game_draw_area_end_from_packet(&fixture.end_binding,
                                             &fixture.context.memory, &event,
                                             &machine) == 0,
        "unknown jal ra refused");
  machine = original;
  machine.registers.gpr[17].known_mask = 16u;
  check(nba97_game_draw_area_end_from_packet(&fixture.end_binding,
                                             &fixture.context.memory, &event,
                                             &machine) == 0,
        "malformed machine refused");
  machine = original;
  Nba97GameTextMemory malformed_memory{nullptr, 1u};
  check(nba97_game_draw_area_end_from_packet(
            &fixture.end_binding, &malformed_memory, &event, &machine) == 0,
        "malformed memory refused");

  nba97_game_draw_area_end_packet_binding_init(&fixture.end_binding, 3u,
                                               nullptr, 1u, nullptr, nullptr);
  check(nba97_game_draw_area_end_from_packet(&fixture.end_binding,
                                             &fixture.context.memory, &event,
                                             &machine) == 0,
        "malformed journal refused");
  check(fixture.end_binding.result == NBA97_TEXT_ARGUMENT,
        "guard result remains explicit argument");
}

} // namespace

int main() {
  natural_packet_completion();
  natural_limit_prefix();
  adapter_guards_and_full_machine_prefix();
  std::printf("game_draw_area_end_integration_tests: %zu checks passed\n",
              checks);
  return 0;
}
