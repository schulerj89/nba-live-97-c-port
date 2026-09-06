#include "recovered/game_draw_packet.h"

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
    std::fprintf(stderr, "game_draw_packet_tests: %s\n", message);
    std::exit(1);
  }
}

uint32_t read32(const uint8_t *bytes, size_t offset) {
  return static_cast<uint32_t>(bytes[offset]) |
         (static_cast<uint32_t>(bytes[offset + 1]) << 8u) |
         (static_cast<uint32_t>(bytes[offset + 2]) << 16u) |
         (static_cast<uint32_t>(bytes[offset + 3]) << 24u);
}

void write16(uint8_t *bytes, size_t offset, uint16_t value) {
  bytes[offset] = static_cast<uint8_t>(value);
  bytes[offset + 1] = static_cast<uint8_t>(value >> 8u);
}

void write32(uint8_t *bytes, size_t offset, uint32_t value) {
  for (unsigned byte = 0; byte != 4; ++byte)
    bytes[offset + byte] = static_cast<uint8_t>(value >> (8u * byte));
}

struct Fixture {
  static constexpr uint32_t stack_base = UINT32_C(0x80100000);
  static constexpr uint32_t packet_base = UINT32_C(0x80200000);
  static constexpr uint32_t env_base = UINT32_C(0x80300000);
  std::array<uint8_t, 512> stack{};
  std::array<uint8_t, 512> packet{};
  std::array<uint8_t, 512> env{};
  std::array<uint8_t, 16> globals{};
  std::array<uint8_t, 512> stack_known{};
  std::array<uint8_t, 512> packet_known{};
  std::array<uint8_t, 512> env_known{};
  std::array<uint8_t, 16> globals_known{};
  std::array<Nba97GameTextRegion, 4> regions{};
  std::array<Nba97GameDrawPacketAccess, 128> journal{};
  Nba97GameDrawPacketContext context{};
  Nba97GameDrawPacketProgress progress{};
  std::array<Nba97GameDrawPacketEvent, 5> events{};
  std::array<std::array<Nba97GameDrawPacketWord, 4>, 5> arguments{};
  size_t event_count{};
  int reject_kind{};
  int malformed_kind{};
  bool mutate_last{};

  Fixture() {
    stack_known.fill(1u);
    packet_known.fill(1u);
    env_known.fill(1u);
    globals_known.fill(1u);
    regions = {{{stack_base, stack.data(), stack_known.data(), stack.size()},
                {packet_base, packet.data(), packet_known.data(), packet.size()},
                {env_base, env.data(), env_known.data(), env.size()},
                {UINT32_C(0x800c55c0), globals.data(), globals_known.data(),
                 globals.size()}}};
    context.memory = {regions.data(), regions.size()};
    context.operation_budget = 256u;
    for (unsigned index = 0u; index != 32u; ++index) {
      context.machine.registers.gpr[index].word =
          UINT32_C(0x51000000) + index * UINT32_C(0x01010101);
      context.machine.registers.gpr[index].known_mask = 15u;
    }
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
        {packet_base, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1] = {env_base, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
        {stack_base + 0x80u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {UINT32_C(0x80099b28), 15u};
    context.machine.hi = {UINT32_C(0x12345678), 15u};
    context.machine.lo = {UINT32_C(0x89abcdef), 15u};
    context.io = callback;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    write16(env.data(), 0u, 10u);
    write16(env.data(), 2u, 20u);
    write16(env.data(), 4u, 64u);
    write16(env.data(), 6u, 32u);
    write16(env.data(), 8u, 3u);
    write16(env.data(), 10u, 4u);
    write16(env.data(), 0x14u, UINT16_C(0x3456));
    env[0x16] = 0x22u;
    env[0x17] = 0x33u;
    env[0x19] = 0x11u;
    env[0x1a] = 0x22u;
    env[0x1b] = 0x33u;
    write16(globals.data(), 4u, 640u);
    write16(globals.data(), 6u, 480u);
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameDrawPacketEvent *event,
                      Nba97GameDrawPacketMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    check(fixture.event_count < fixture.events.size(), "too many callbacks");
    fixture.events[fixture.event_count] = *event;
    for (unsigned argument = 0u; argument != 4u; ++argument)
      fixture.arguments[fixture.event_count][argument] =
          machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0 + argument];
    ++fixture.event_count;
    check(event->delay_slot_pc == event->pc + 4u, "callback delay pc");
    check(machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 15u,
          "callback ra known");
    check(machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
              event->pc + 8u,
          "callback ra value");
    if (fixture.reject_kind == event->kind)
      return 0;
    machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
        {UINT32_C(0x11110000) + event->kind, 15u};
    if (fixture.malformed_kind == event->kind)
      machine->registers.gpr[7].known_mask = 16u;
    if (fixture.mutate_last &&
        event->kind == NBA97_GAME_DRAW_PACKET_CHILD_8009A824) {
      const uint32_t relocated = stack_base + 0x120u;
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {relocated, 15u};
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] =
          {Fixture::env_base, 15u};
      machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1] =
          {Fixture::packet_base + 0x100u, 15u};
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_T0] =
          {UINT32_C(0xdeadbeef), 7u};
      machine->hi = {UINT32_C(0xaabbccdd), 5u};
      machine->lo = {UINT32_C(0x10203040), 10u};
      write32(fixture.stack.data(), 0x120u + 0x20u,
              UINT32_C(0x80001234));
      write32(fixture.stack.data(), 0x120u + 0x1cu,
              UINT32_C(0x0badcafe));
      write32(fixture.stack.data(), 0x120u + 0x18u,
              UINT32_C(0x76543210));
    }
    return 1;
  }

  int run() { return nba97_game_draw_packet(&context, &progress); }
};

void check_calls(const Fixture &fixture) {
  const uint32_t pcs[5] = {UINT32_C(0x8009a364), UINT32_C(0x8009a39c),
                           UINT32_C(0x8009a3b0), UINT32_C(0x8009a3c8),
                           UINT32_C(0x8009a3d4)};
  const uint32_t entries[5] = {
      UINT32_C(0x8009a644), UINT32_C(0x8009a710),
      UINT32_C(0x8009a7dc), UINT32_C(0x8009a5e8),
      UINT32_C(0x8009a824)};
  const unsigned args[5] = {2u, 2u, 2u, 3u, 1u};
  check(fixture.event_count == 5u, "five callbacks");
  for (size_t index = 0; index != 5; ++index) {
    check(fixture.events[index].pc == pcs[index], "call pc order");
    check(fixture.events[index].entry == entries[index], "call entry order");
    check(fixture.events[index].argument_count == args[index], "call args");
  }
}

void no_background() {
  Fixture fixture;
  auto original = fixture.context.machine;
  check(fixture.run() == NBA97_TEXT_COMPLETE, "no background completes");
  check(fixture.progress.completed == 1u, "completion flag");
  check_calls(fixture);
  for (unsigned kind = 1u; kind != 6u; ++kind)
    check(read32(fixture.packet.data(), kind * 4u) ==
              UINT32_C(0x11110000) + kind,
          "six fixed packet words");
  check(read32(fixture.packet.data(), 0x18u) == UINT32_C(0xe6000000),
        "fixed e6 word");
  check(fixture.packet[3] == 6u, "six payload count");
  check(fixture.progress.machine.hi.word == original.hi.word,
        "hi passthrough");
  check(fixture.progress.machine.lo.word == original.lo.word,
        "lo passthrough");
  const unsigned untouched[] = {1u, 9u, 10u, 11u, 12u, 13u, 14u, 15u,
                                18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u,
                                26u, 27u, 28u, 30u};
  for (unsigned reg : untouched) {
    check(fixture.progress.machine.registers.gpr[reg].word ==
              original.registers.gpr[reg].word,
          "untouched gpr value");
    check(fixture.progress.machine.registers.gpr[reg].known_mask ==
              original.registers.gpr[reg].known_mask,
          "untouched gpr mask");
  }
  check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Fixture::stack_base + 0x80u,
        "sp restored by frame advance");
}

void callback_arguments_and_delay_store() {
  Fixture fixture;
  check(fixture.run() == NBA97_TEXT_COMPLETE, "argument run");
  check(fixture.events[0].kind == NBA97_GAME_DRAW_PACKET_CHILD_8009A644,
        "first kind");
  check(fixture.events[1].kind == NBA97_GAME_DRAW_PACKET_CHILD_8009A710,
        "second kind");
  check(fixture.events[4].kind == NBA97_GAME_DRAW_PACKET_CHILD_8009A824,
        "fifth kind");
  check(fixture.arguments[0][0].word == 10u &&
            fixture.arguments[0][1].word == 20u,
        "first signed coordinate arguments");
  check(fixture.arguments[1][0].word == 73u &&
            fixture.arguments[1][1].word == 51u,
        "second inclusive coordinate arguments");
  check(fixture.arguments[2][0].word == 3u &&
            fixture.arguments[2][1].word == 4u,
        "third signed offset arguments");
  check(fixture.arguments[3][0].word == 0x33u &&
            fixture.arguments[3][1].word == 0x22u &&
            fixture.arguments[3][2].word == UINT32_C(0x3456),
        "fourth byte and halfword arguments");
  check(fixture.arguments[4][0].word == Fixture::env_base + 12u,
        "fifth environment-tail argument");
  check(read32(fixture.packet.data(), 0x10u) == UINT32_C(0x11110004),
        "last jal delay stores prior v0");
}

void background_offset() {
  Fixture fixture;
  fixture.env[0x18] = 1u;
  write16(fixture.env.data(), 0u, 10u);
  write16(fixture.env.data(), 4u, 80u);
  check(fixture.run() == NBA97_TEXT_COMPLETE, "offset background completes");
  check(fixture.packet[3] == 9u, "offset payload count");
  check(read32(fixture.packet.data(), 0x1cu) == UINT32_C(0x60332211),
        "offset rgb order and opcode");
  check(read32(fixture.packet.data(), 0x20u) == UINT32_C(0x00100007),
        "offset coordinates");
  check(read32(fixture.packet.data(), 0x24u) == UINT32_C(0x00200050),
        "offset dimensions");
}

void background_aligned_and_clamps() {
  Fixture fixture;
  fixture.env[0x18] = 1u;
  write16(fixture.env.data(), 0u, 64u);
  write16(fixture.env.data(), 4u, 640u);
  write16(fixture.env.data(), 6u, 500u);
  write16(fixture.globals.data(), 4u, 1024u);
  check(fixture.run() == NBA97_TEXT_COMPLETE, "aligned background completes");
  check(fixture.packet[3] == 9u, "aligned payload count");
  check(read32(fixture.packet.data(), 0x1cu) == UINT32_C(0x02332211),
        "aligned rgb order and opcode");
  check(read32(fixture.packet.data(), 0x20u) == UINT32_C(0x00140040),
        "aligned original coordinates");
  check(read32(fixture.packet.data(), 0x24u) == UINT32_C(0x01df0280),
        "signed global clamps minus one");

  Fixture negatives;
  negatives.env[0x18] = 1u;
  write16(negatives.env.data(), 4u, UINT16_C(0xffff));
  write16(negatives.env.data(), 6u, UINT16_C(0x8000));
  check(negatives.run() == NBA97_TEXT_COMPLETE, "negative sizes complete");
  check((read32(negatives.packet.data(), 0x24u) & UINT32_C(0xffffffff)) == 0u,
        "negative sizes clear separately");

  Fixture wrapped_limit;
  wrapped_limit.env[0x18] = 1u;
  write16(wrapped_limit.env.data(), 4u, 1u);
  write16(wrapped_limit.env.data(), 6u, 1u);
  write16(wrapped_limit.globals.data(), 4u, UINT16_C(0x8000));
  write16(wrapped_limit.globals.data(), 6u, UINT16_C(0x8000));
  check(wrapped_limit.run() == NBA97_TEXT_COMPLETE,
        "signed global extrema complete");
  check(read32(wrapped_limit.packet.data(), 0x24u) ==
            UINT32_C(0x7fff7fff),
        "signed global minus-one low-half wrap retained");
}

void mutable_callback_machine() {
  Fixture fixture;
  fixture.mutate_last = true;
  check(fixture.run() == NBA97_TEXT_COMPLETE, "mutable callback completes");
  check(fixture.packet[0x100u + 3u] == 6u, "live s1 final count target");
  check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            UINT32_C(0x80001234),
        "live sp restored ra");
  check(fixture.progress.machine.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1].word ==
            UINT32_C(0x0badcafe),
        "live sp restored s1");
  check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
            UINT32_C(0x76543210),
        "live sp restored s0");
  check(fixture.progress.machine.hi.word == UINT32_C(0xaabbccdd) &&
            fixture.progress.machine.hi.known_mask == 5u,
        "callback hi retained");
  check(fixture.progress.machine.lo.word == UINT32_C(0x10203040) &&
            fixture.progress.machine.lo.known_mask == 10u,
        "callback lo retained");
}

void unknown_and_failure_prefixes() {
  Fixture branch;
  branch.env_known[0x18] = 0u;
  check(branch.run() == NBA97_TEXT_UNKNOWN, "unknown background predicate");
  check(branch.progress.stopped_pc == UINT32_C(0x8009a3f0),
        "unknown background stop pc");
  check(branch.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0].word ==
            7u,
        "unknown branch delay t0");
  check(branch.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 14u,
        "unknown lbu retains upper known bytes");

  Fixture predicate;
  predicate.env[0x18] = 1u;
  write16(predicate.env.data(), 4u, 300u);
  predicate.globals_known[4] = 0u;
  check(predicate.run() == NBA97_TEXT_UNKNOWN, "unknown signed clamp");
  check(predicate.progress.stopped_pc == UINT32_C(0x8009a454),
        "unknown slt branch pc");
  check(predicate.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 300u,
        "slt branch delay publishes width");

  Fixture width_sign;
  width_sign.env[0x18] = 1u;
  write16(width_sign.env.data(), 4u, 300u);
  width_sign.globals_known[5] = 0u;
  check(width_sign.run() == NBA97_TEXT_UNKNOWN,
        "unknown width-limit sign refuses");
  check(width_sign.progress.stopped_pc == UINT32_C(0x8009a454),
        "unknown width-limit sign stop pc");
  check(width_sign.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 300u,
        "unknown width-limit branch delay v0");

  Fixture height_sign;
  height_sign.env[0x18] = 1u;
  write16(height_sign.env.data(), 4u, UINT16_C(0xffff));
  write16(height_sign.env.data(), 6u, 300u);
  height_sign.globals_known[7] = 0u;
  check(height_sign.run() == NBA97_TEXT_UNKNOWN,
        "unknown height-limit sign refuses");
  check(height_sign.progress.stopped_pc == UINT32_C(0x8009a494),
        "unknown height-limit sign stop pc");
  check(height_sign.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .word == 300u,
        "unknown height-limit branch delay v0");

  for (int kind = 1; kind != 6; ++kind) {
    Fixture refused;
    refused.reject_kind = kind;
    check(refused.run() == NBA97_TEXT_IO_REFUSED, "child refusal status");
    check(refused.progress.stopped_entry ==
              UINT32_C(0x8009a644) +
                  (kind == 1 ? 0u : kind == 2 ? 0xccu
                                      : kind == 3 ? 0x198u
                                      : kind == 4 ? UINT32_C(0xffffffa4)
                                                  : 0x1e0u),
          "child refusal target");
  }

  Fixture malformed;
  malformed.malformed_kind = 3;
  check(malformed.run() == NBA97_TEXT_ARGUMENT, "malformed callback machine");
  check(malformed.progress.callbacks_completed == 2u,
        "malformed callback not completed");
}

void budgets_and_journal() {
  Fixture complete;
  check(complete.run() == NBA97_TEXT_COMPLETE, "budget baseline");
  const size_t operations = complete.progress.operations;
  check(operations == 30u, "no-background operation count");
  for (size_t budget = 0u; budget != operations; ++budget) {
    Fixture limited;
    limited.context.operation_budget = budget;
    check(limited.run() == NBA97_TEXT_LIMIT, "every budget prefix limits");
    check(limited.progress.operations == budget, "budget exact prefix");
  }

  Fixture offset_complete;
  offset_complete.env[0x18] = 1u;
  check(offset_complete.run() == NBA97_TEXT_COMPLETE,
        "offset budget baseline");
  for (size_t budget = 0u; budget != offset_complete.progress.operations;
       ++budget) {
    Fixture limited;
    limited.env[0x18] = 1u;
    limited.context.operation_budget = budget;
    check(limited.run() == NBA97_TEXT_LIMIT,
          "every offset budget prefix limits");
    check(limited.progress.operations == budget,
          "offset budget exact prefix");
  }

  Fixture aligned_complete;
  aligned_complete.env[0x18] = 1u;
  write16(aligned_complete.env.data(), 0u, 64u);
  write16(aligned_complete.env.data(), 4u, 64u);
  check(aligned_complete.run() == NBA97_TEXT_COMPLETE,
        "aligned budget baseline");
  for (size_t budget = 0u; budget != aligned_complete.progress.operations;
       ++budget) {
    Fixture limited;
    limited.env[0x18] = 1u;
    write16(limited.env.data(), 0u, 64u);
    write16(limited.env.data(), 4u, 64u);
    limited.context.operation_budget = budget;
    check(limited.run() == NBA97_TEXT_LIMIT,
          "every aligned budget prefix limits");
    check(limited.progress.operations == budget,
          "aligned budget exact prefix");
  }

  Fixture first;
  Fixture second;
  first.context.access_journal_capacity = 3u;
  second.context.access_journal_capacity = 3u;
  check(first.run() == NBA97_TEXT_COMPLETE, "truncated journal run one");
  check(second.run() == NBA97_TEXT_COMPLETE, "truncated journal run two");
  check(first.progress.access_events > 3u, "journal reports logical count");
  check(std::memcmp(first.journal.data(), second.journal.data(),
                    3u * sizeof(first.journal[0])) == 0,
        "deterministic truncated journal");
}

void malformed_memory_and_unknown_store() {
  Fixture malformed;
  malformed.env_known[3] = 2u;
  check(malformed.run() == NBA97_TEXT_ARGUMENT, "late load knownness malformed");
  check(malformed.progress.stopped_pc == UINT32_C(0x8009a360),
        "malformed late byte stop");

  Fixture late;
  late.env[0x18] = 1u;
  write16(late.env.data(), 4u, 300u);
  late.globals_known[5] = 2u;
  check(late.run() == NBA97_TEXT_ARGUMENT, "late global knownness malformed");
  check(late.progress.stopped_pc == UINT32_C(0x8009a43c),
        "late malformed global stop");
  check(late.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
            UINT32_C(0x800c55c4),
        "failed load leaves destination at source address");

  Fixture immutable;
  immutable.packet.fill(0x5au);
  immutable.regions[1].known = nullptr;
  immutable.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word += 1u;
  check(immutable.run() == NBA97_TEXT_ALIGNMENT_TRAP,
        "misaligned stack traps");
  check(immutable.packet[4] == 0x5au, "unreached packet immutable");

  Fixture unknown_store;
  unknown_store.regions[1].known = nullptr;
  unknown_store.malformed_kind = 0;
  struct UnknownReturn {
    static int callback(void *, const Nba97GameTextMemory *,
                        const Nba97GameDrawPacketEvent *event,
                        Nba97GameDrawPacketMachine *machine) {
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] =
          {UINT32_C(0xaabbccdd),
           static_cast<uint8_t>(event->kind == 1u ? 14u : 15u)};
      return 1;
    }
  };
  unknown_store.context.io = UnknownReturn::callback;
  unknown_store.context.user = nullptr;
  unknown_store.packet.fill(0x77u);
  check(unknown_store.run() == NBA97_TEXT_ARGUMENT,
        "unknown store to null-known rejects");
  check(read32(unknown_store.packet.data(), 4u) == UINT32_C(0x77777777),
        "rejected unknown store immutable");
}

void aliases_and_unmapped_inputs() {
  Fixture stack_alias;
  stack_alias.env[0x18] = 1u;
  stack_alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
      {Fixture::stack_base + 0x58u, 15u};
  check(stack_alias.run() == NBA97_TEXT_COMPLETE,
        "packet and live frame alias completes");
  check(stack_alias.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                .word == UINT32_C(0x00100007),
        "packet overwrite is observed by later ra reload");

  Fixture packet_env_alias;
  packet_env_alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
      {Fixture::env_base, 15u};
  int first = packet_env_alias.run();
  Fixture packet_env_repeat;
  packet_env_repeat.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
      {Fixture::env_base, 15u};
  int second = packet_env_repeat.run();
  check(first == second, "packet/environment alias deterministic status");
  check(std::memcmp(packet_env_alias.env.data(), packet_env_repeat.env.data(),
                    packet_env_alias.env.size()) == 0,
        "packet/environment alias deterministic memory");

  Fixture unknown;
  unknown.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask =
      14u;
  check(unknown.run() == NBA97_TEXT_UNKNOWN, "unknown packet pointer");
  check(unknown.progress.stopped_pc == UINT32_C(0x8009a36c),
        "unknown packet pointer stop after first child");

  Fixture unmapped;
  unmapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
      {UINT32_C(0x90000000), 15u};
  check(unmapped.run() == NBA97_TEXT_RESOURCE, "unmapped packet pointer");
  check(unmapped.progress.stopped_pc == UINT32_C(0x8009a36c),
        "unmapped packet pointer stop");
}

void final_ra_unknown() {
  Fixture fixture;
  fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask =
      14u;
  check(fixture.run() == NBA97_TEXT_UNKNOWN, "unknown restored ra");
  check(fixture.progress.stopped_pc == UINT32_C(0x8009a5e0),
        "unknown final jr stop");
  check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Fixture::stack_base + 0x80u,
        "unknown jr after full epilogue");
}

} // namespace

int main() {
  no_background();
  callback_arguments_and_delay_store();
  background_offset();
  background_aligned_and_clamps();
  mutable_callback_machine();
  unknown_and_failure_prefixes();
  budgets_and_journal();
  malformed_memory_and_unknown_store();
  aliases_and_unmapped_inputs();
  final_ra_unknown();
  std::printf("game_draw_packet_tests: %zu checks passed\n", checks);
  return 0;
}
