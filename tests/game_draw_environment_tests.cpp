#include "game_draw_environment_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

unsigned checks = 0u;
void check(bool condition, const char *expression, int line) {
  ++checks;
  if (condition)
    return;
  std::fprintf(stderr, "check failed at line %d: %s\n", line, expression);
  std::exit(1);
}
#define CHECK(expression) check((expression), #expression, __LINE__)

constexpr uint32_t kBase = UINT32_C(0x80000000);
constexpr uint32_t kStack = UINT32_C(0x801ff000);
constexpr uint32_t kEnvironment = UINT32_C(0x80010000);
constexpr uint32_t kDispatch = UINT32_C(0x80030000);
constexpr uint32_t kDebug = UINT32_C(0x80040000);
constexpr uint32_t kSubmit = UINT32_C(0x80050000);

void set_word(Nba97GameDrawEnvironmentWord &word, uint32_t value,
              uint8_t mask = 15u) {
  word.word = value;
  word.known_mask = mask;
}

struct Seen {
  Nba97GameDrawEnvironmentEvent event{};
  Nba97GameDrawEnvironmentMachine machine{};
};

struct Fixture {
  enum Mode {
    Normal,
    Refuse,
    MalformedGpr,
    MalformedHi,
    RelocateAtCopy,
    RelocateAtSubmit,
    MutateS0AtPacket
  } mode = Normal;
  std::vector<uint8_t> ram = std::vector<uint8_t>(0x200000u, 0xa5u);
  std::vector<uint8_t> known = std::vector<uint8_t>(0x200000u, 1u);
  Nba97GameTextRegion region{};
  std::array<Nba97GameDrawEnvironmentAccess, 32> journal{};
  Nba97GameDrawEnvironmentContext context{};
  Nba97GameDrawEnvironmentProgress progress{};
  std::vector<Seen> calls;
  uint8_t fail_kind = 0u;
  uint32_t relocated_sp = UINT32_C(0x801fe000);
  uint32_t relocated_s1 = UINT32_C(0x80011000);
  uint32_t relocated_s2 = UINT32_C(0x800c6000);

  Fixture() {
    region = {kBase, ram.data(), known.data(), ram.size()};
    context.memory = {&region, 1u};
    context.operation_budget = 100u;
    context.io = callback;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned index = 0u; index != 32u; ++index)
      set_word(context.machine.registers.gpr[index],
               UINT32_C(0x10000000) + index);
    set_word(context.machine.registers.gpr[0], 0u);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
             kEnvironment);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP], kStack);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x81234567));
    set_word(context.machine.hi, UINT32_C(0x11112222));
    set_word(context.machine.lo, UINT32_C(0x33334444));
    put8(UINT32_C(0x800c55c2), 0u);
    put32(UINT32_C(0x800c55bc), kDebug);
    put32(UINT32_C(0x800c55b8), kDispatch);
    put32(kDispatch + 0x18u, UINT32_C(0x87654321));
    put32(kDispatch + 8u, kSubmit);
    put32(kEnvironment + 0x1cu, UINT32_C(0x12000000));
  }

  size_t offset(uint32_t address) const { return address - kBase; }
  void put8(uint32_t address, uint8_t value) { ram[offset(address)] = value; }
  void put32(uint32_t address, uint32_t value) {
    for (unsigned byte = 0u; byte != 4u; ++byte)
      ram[offset(address) + byte] = static_cast<uint8_t>(value >> (byte * 8u));
  }
  uint32_t get32(uint32_t address) const {
    uint32_t value = 0u;
    for (unsigned byte = 0u; byte != 4u; ++byte)
      value |= static_cast<uint32_t>(ram[offset(address) + byte])
               << (byte * 8u);
    return value;
  }
  int run() { return nba97_game_draw_environment(&context, &progress); }

  static int callback(void *opaque, const Nba97GameTextMemory *memory,
                      const Nba97GameDrawEnvironmentEvent *event,
                      Nba97GameDrawEnvironmentMachine *machine) {
    Fixture &fixture = *static_cast<Fixture *>(opaque);
    fixture.calls.push_back({*event, *machine});
    if (fixture.mode == Refuse && event->kind == fixture.fail_kind)
      return 0;
    if (fixture.mode == MalformedGpr && event->kind == fixture.fail_kind)
      machine->registers.gpr[14].known_mask = 16u;
    if (fixture.mode == MalformedHi && event->kind == fixture.fail_kind)
      machine->hi.known_mask = 16u;
    if (fixture.mode == MutateS0AtPacket &&
        event->kind == NBA97_GAME_DRAW_ENVIRONMENT_PACKET_8009A344)
      set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0],
               UINT32_C(0x80012000));
    if (fixture.mode == RelocateAtSubmit &&
        event->kind == NBA97_GAME_DRAW_ENVIRONMENT_SUBMIT_INDIRECT) {
      set_word(machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S2],
               fixture.relocated_s2);
      set_word(machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1],
               fixture.relocated_s1);
      set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP],
               fixture.relocated_sp);
      set_word(machine->hi, UINT32_C(0xabcdef01));
      set_word(machine->lo, UINT32_C(0x23456789));
    }
    if (event->kind == NBA97_GAME_DRAW_ENVIRONMENT_COPY_8009CB0C) {
      const uint32_t destination =
          machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word;
      const uint32_t source =
          machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word;
      std::array<uint8_t, 0x5c> bytes{};
      std::array<uint8_t, 0x5c> masks{};
      const auto find_byte = [memory](uint32_t address,
                                      uint8_t **known_byte) -> uint8_t * {
        for (size_t region = 0u; region != memory->count; ++region) {
          const Nba97GameTextRegion &candidate = memory->region[region];
          if (address >= candidate.base &&
              static_cast<uint64_t>(address - candidate.base) <
                  candidate.size) {
            const size_t offset = address - candidate.base;
            *known_byte =
                candidate.known != nullptr ? candidate.known + offset : nullptr;
            return candidate.data + offset;
          }
        }
        return nullptr;
      };
      for (unsigned index = 0u; index != bytes.size(); ++index) {
        uint8_t *source_known = nullptr;
        uint8_t *source_byte = find_byte(source + index, &source_known);
        CHECK(source_byte != nullptr);
        bytes[index] = *source_byte;
        masks[index] = source_known != nullptr ? *source_known : 1u;
      }
      for (unsigned index = 0u; index != bytes.size(); ++index) {
        uint8_t *destination_known = nullptr;
        uint8_t *destination_byte =
            find_byte(destination + index, &destination_known);
        CHECK(destination_byte != nullptr);
        *destination_byte = bytes[index];
        if (destination_known != nullptr)
          *destination_known = masks[index];
      }
      set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0],
               UINT32_C(0xdeadbeef));
      if (fixture.mode == RelocateAtCopy) {
        set_word(machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1],
                 fixture.relocated_s1);
        set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP],
                 fixture.relocated_sp);
        set_word(machine->hi, UINT32_C(0xabcdef01));
        set_word(machine->lo, UINT32_C(0x23456789));
      }
    }
    return 1;
  }
};

void test_normal_and_exact_order() {
  Fixture fixture;
  const auto initial = fixture.context.machine;
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
  CHECK(fixture.progress.completed == 1u);
  CHECK(fixture.progress.operations == 17u);
  CHECK(fixture.progress.accesses == 14u && fixture.progress.reads == 9u &&
        fixture.progress.stores == 5u);
  CHECK(fixture.progress.callbacks_completed == 3u);
  CHECK(fixture.calls.size() == 3u);
  const uint32_t pcs[] = {UINT32_C(0x80099b20), UINT32_C(0x80099b58),
                          UINT32_C(0x80099b68)};
  const uint32_t entries[] = {UINT32_C(0x8009a344), kSubmit,
                              UINT32_C(0x8009cb0c)};
  const uint8_t kinds[] = {NBA97_GAME_DRAW_ENVIRONMENT_PACKET_8009A344,
                           NBA97_GAME_DRAW_ENVIRONMENT_SUBMIT_INDIRECT,
                           NBA97_GAME_DRAW_ENVIRONMENT_COPY_8009CB0C};
  const uint8_t args[] = {2u, 4u, 3u};
  for (unsigned index = 0u; index != 3u; ++index) {
    CHECK(fixture.calls[index].event.pc == pcs[index]);
    CHECK(fixture.calls[index].event.delay_slot_pc == pcs[index] + 4u);
    CHECK(fixture.calls[index].event.entry == entries[index]);
    CHECK(fixture.calls[index].event.kind == kinds[index]);
    CHECK(fixture.calls[index].event.argument_count == args[index]);
    CHECK(fixture.calls[index]
              .machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
              .word == pcs[index] + 8u);
  }
  CHECK(
      fixture.calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
      kEnvironment + 0x1cu);
  CHECK(
      fixture.calls[0].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
      kEnvironment);
  CHECK(
      fixture.calls[1].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
      UINT32_C(0x87654321));
  CHECK(
      fixture.calls[1].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
      kEnvironment + 0x1cu);
  CHECK(
      fixture.calls[1].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
      0x40u);
  CHECK(
      fixture.calls[1].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A3].word ==
      0u);
  CHECK(
      fixture.calls[2].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
      UINT32_C(0x800c55d0));
  CHECK(
      fixture.calls[2].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
      kEnvironment);
  CHECK(
      fixture.calls[2].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
      0x5cu);
  CHECK(fixture.get32(kEnvironment + 0x1cu) == UINT32_C(0x12ffffff));
  CHECK(fixture.get32(UINT32_C(0x800c55d0) + 0x1cu) == UINT32_C(0x12ffffff));
  CHECK(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
      kEnvironment);
  CHECK(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
      kStack);
  CHECK(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
      initial.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word);
  CHECK(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
      initial.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word);
  CHECK(
      fixture.progress.machine.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1].word ==
      initial.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1].word);
  CHECK(
      fixture.progress.machine.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S2].word ==
      initial.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S2].word);
  CHECK(std::memcmp(&fixture.progress.machine.hi, &initial.hi,
                    sizeof(initial.hi)) == 0);
  CHECK(std::memcmp(&fixture.progress.machine.lo, &initial.lo,
                    sizeof(initial.lo)) == 0);
  const uint32_t access_pcs[] = {
      UINT32_C(0x80099ad0), UINT32_C(0x80099adc), UINT32_C(0x80099ae0),
      UINT32_C(0x80099ae4), UINT32_C(0x80099ae8), UINT32_C(0x80099b38),
      UINT32_C(0x80099b40), UINT32_C(0x80099b48), UINT32_C(0x80099b4c),
      UINT32_C(0x80099b50), UINT32_C(0x80099b74), UINT32_C(0x80099b78),
      UINT32_C(0x80099b7c), UINT32_C(0x80099b80)};
  for (unsigned index = 0u; index != 14u; ++index)
    CHECK(fixture.journal[index].pc == access_pcs[index]);
}

void test_debug_paths() {
  for (uint8_t debug : {uint8_t{0}, uint8_t{1}, uint8_t{2}, uint8_t{255}}) {
    Fixture fixture;
    fixture.put8(UINT32_C(0x800c55c2), debug);
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    const bool called = debug >= 2u;
    CHECK(fixture.calls.size() == (called ? 4u : 3u));
    if (called) {
      CHECK(fixture.calls[0].event.pc == UINT32_C(0x80099b10));
      CHECK(fixture.calls[0].event.entry == kDebug);
      CHECK(fixture.calls[0].event.delay_slot_pc == UINT32_C(0x80099b14));
      CHECK(fixture.calls[0].event.argument_count == 2u);
      CHECK(fixture.calls[0]
                .machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                .word == UINT32_C(0x8002836c));
      CHECK(fixture.calls[0]
                .machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
                .word == kEnvironment);
      CHECK(fixture.progress.operations == 19u);
    }
  }
}

void test_all_tag_masks_and_alias() {
  for (uint8_t mask = 0u; mask != 16u; ++mask) {
    Fixture fixture;
    for (unsigned byte = 0u; byte != 4u; ++byte)
      fixture.known[fixture.offset(kEnvironment + 0x1cu) + byte] =
          static_cast<uint8_t>((mask >> byte) & 1u);
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
    CHECK(fixture.get32(kEnvironment + 0x1cu) == UINT32_C(0x12ffffff));
    CHECK(fixture.known[fixture.offset(kEnvironment + 0x1cu)] == 1u);
    CHECK(fixture.known[fixture.offset(kEnvironment + 0x1du)] == 1u);
    CHECK(fixture.known[fixture.offset(kEnvironment + 0x1eu)] == 1u);
    CHECK(fixture.known[fixture.offset(kEnvironment + 0x1fu)] ==
          ((mask >> 3u) & 1u));
  }

  Fixture alias;
  alias.put32(UINT32_C(0x800c55b8), kEnvironment + 4u);
  alias.put32(kEnvironment + 0x0cu, kSubmit);
  alias.put32(kEnvironment + 0x1cu, UINT32_C(0x34000000));
  CHECK(alias.run() == NBA97_TEXT_COMPLETE);
  CHECK(alias.calls[1].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        UINT32_C(0x34ffffff));
  CHECK(alias.calls[1].event.entry == kSubmit);

  Fixture table_word_alias;
  const uint32_t alias_environment = UINT32_C(0x800c559c);
  set_word(
      table_word_alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
      alias_environment);
  table_word_alias.put32(UINT32_C(0x800c55b8), kDispatch);
  CHECK(table_word_alias.run() == NBA97_TEXT_COMPLETE);
  CHECK(table_word_alias.get32(UINT32_C(0x800c55b8)) == UINT32_C(0x80ffffff));
  CHECK(table_word_alias.calls[1].event.entry == kSubmit);
  CHECK(table_word_alias.calls[1]
            .machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
            .word == UINT32_C(0x87654321));
}

void test_live_callbacks_and_failures() {
  Fixture live_s0;
  live_s0.mode = Fixture::MutateS0AtPacket;
  CHECK(live_s0.run() == NBA97_TEXT_COMPLETE);
  CHECK(
      live_s0.calls[1].machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
      UINT32_C(0x80012000));

  Fixture live_submit;
  live_submit.mode = Fixture::RelocateAtSubmit;
  live_submit.put32(live_submit.relocated_s1 + 0x1cu, UINT32_C(0x56000000));
  live_submit.put32(live_submit.relocated_sp + 0x1cu, UINT32_C(0x82345678));
  live_submit.put32(live_submit.relocated_sp + 0x18u, UINT32_C(0x22222222));
  live_submit.put32(live_submit.relocated_sp + 0x14u, UINT32_C(0x33333333));
  live_submit.put32(live_submit.relocated_sp + 0x10u, UINT32_C(0x44444444));
  CHECK(live_submit.run() == NBA97_TEXT_COMPLETE);
  CHECK(live_submit.calls[2]
            .machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
            .word == live_submit.relocated_s2 + 0x0eu);
  CHECK(live_submit.calls[2]
            .machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
            .word == live_submit.relocated_s1);
  CHECK(live_submit.progress.machine.hi.word == UINT32_C(0xabcdef01));
  CHECK(live_submit.progress.machine.lo.word == UINT32_C(0x23456789));

  Fixture relocated;
  relocated.mode = Fixture::RelocateAtCopy;
  relocated.put32(relocated.relocated_sp + 0x1cu, UINT32_C(0x82345678));
  relocated.put32(relocated.relocated_sp + 0x18u, UINT32_C(0x22222222));
  relocated.put32(relocated.relocated_sp + 0x14u, UINT32_C(0x33333333));
  relocated.put32(relocated.relocated_sp + 0x10u, UINT32_C(0x44444444));
  CHECK(relocated.run() == NBA97_TEXT_COMPLETE);
  CHECK(relocated.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .word == relocated.relocated_s1);
  CHECK(relocated.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
            .word == relocated.relocated_sp + 0x20u);
  CHECK(relocated.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
            .word == UINT32_C(0x82345678));
  CHECK(relocated.progress.machine.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S2]
            .word == UINT32_C(0x22222222));
  CHECK(relocated.progress.machine.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1]
            .word == UINT32_C(0x33333333));
  CHECK(relocated.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0]
            .word == UINT32_C(0x44444444));
  CHECK(relocated.progress.machine.hi.word == UINT32_C(0xabcdef01));
  CHECK(relocated.progress.machine.lo.word == UINT32_C(0x23456789));

  const uint8_t kinds[] = {NBA97_GAME_DRAW_ENVIRONMENT_DEBUG_INDIRECT,
                           NBA97_GAME_DRAW_ENVIRONMENT_PACKET_8009A344,
                           NBA97_GAME_DRAW_ENVIRONMENT_SUBMIT_INDIRECT,
                           NBA97_GAME_DRAW_ENVIRONMENT_COPY_8009CB0C};
  for (uint8_t kind : kinds) {
    Fixture refused;
    refused.mode = Fixture::Refuse;
    refused.fail_kind = kind;
    if (kind == NBA97_GAME_DRAW_ENVIRONMENT_DEBUG_INDIRECT)
      refused.put8(UINT32_C(0x800c55c2), 2u);
    CHECK(refused.run() == NBA97_TEXT_IO_REFUSED);
    CHECK(refused.calls.back().event.kind == kind);

    Fixture malformed;
    malformed.mode = Fixture::MalformedGpr;
    malformed.fail_kind = kind;
    if (kind == NBA97_GAME_DRAW_ENVIRONMENT_DEBUG_INDIRECT)
      malformed.put8(UINT32_C(0x800c55c2), 2u);
    CHECK(malformed.run() == NBA97_TEXT_ARGUMENT);
    CHECK(malformed.progress.machine.registers.gpr[14].known_mask == 16u);
  }
  Fixture malformed_hi;
  malformed_hi.mode = Fixture::MalformedHi;
  malformed_hi.fail_kind = NBA97_GAME_DRAW_ENVIRONMENT_COPY_8009CB0C;
  CHECK(malformed_hi.run() == NBA97_TEXT_ARGUMENT);
  CHECK(malformed_hi.progress.machine.hi.known_mask == 16u);
}

void test_unknowns_mapping_and_atomic_store() {
  Fixture debug;
  debug.known[debug.offset(UINT32_C(0x800c55c2))] = 0u;
  CHECK(debug.run() == NBA97_TEXT_UNKNOWN);
  CHECK(debug.progress.stopped_pc == UINT32_C(0x80099af4));
  CHECK(debug.progress.machine.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1].word ==
        kEnvironment);
  CHECK(debug.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0].word ==
        1u);
  CHECK(debug.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .known_mask == 14u);
  CHECK(debug.progress.operations == 5u);

  Fixture a0;
  a0.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask = 7u;
  CHECK(a0.run() == NBA97_TEXT_UNKNOWN);
  CHECK(a0.progress.stopped_pc == UINT32_C(0x80099b38));
  CHECK(a0.progress.callbacks_completed == 1u);

  Fixture table;
  table.known[table.offset(UINT32_C(0x800c55b8))] = 0u;
  CHECK(table.run() == NBA97_TEXT_UNKNOWN);
  CHECK(table.progress.stopped_pc == UINT32_C(0x80099b4c));
  CHECK(table.get32(kEnvironment + 0x1cu) == UINT32_C(0x12ffffff));

  Fixture dispatch;
  dispatch.known[dispatch.offset(kDispatch + 8u)] = 0u;
  CHECK(dispatch.run() == NBA97_TEXT_UNKNOWN);
  CHECK(dispatch.progress.stopped_pc == UINT32_C(0x80099b58));
  CHECK(
      dispatch.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
      UINT32_C(0x80099b60));
  CHECK(
      dispatch.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A3].word ==
      0u);

  Fixture debug_target;
  debug_target.put8(UINT32_C(0x800c55c2), 2u);
  debug_target.known[debug_target.offset(UINT32_C(0x800c55bc))] = 0u;
  CHECK(debug_target.run() == NBA97_TEXT_UNKNOWN);
  CHECK(debug_target.progress.stopped_pc == UINT32_C(0x80099b10));
  CHECK(debug_target.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
            .word == UINT32_C(0x80099b18));
  CHECK(debug_target.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
            .word == kEnvironment);

  Fixture debug_alignment;
  debug_alignment.put8(UINT32_C(0x800c55c2), 2u);
  debug_alignment.put32(UINT32_C(0x800c55bc), kDebug + 2u);
  CHECK(debug_alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  CHECK(debug_alignment.progress.stopped_pc == UINT32_C(0x80099b10));

  Fixture submit_alignment;
  submit_alignment.put32(kDispatch + 8u, kSubmit + 2u);
  CHECK(submit_alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  CHECK(submit_alignment.progress.stopped_pc == UINT32_C(0x80099b58));

  Fixture ra;
  ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7u;
  CHECK(ra.run() == NBA97_TEXT_UNKNOWN);
  CHECK(ra.progress.stopped_pc == UINT32_C(0x80099b88));
  CHECK(ra.progress.callbacks_completed == 3u);
  CHECK(ra.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        kStack);

  Fixture sp;
  sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].known_mask = 7u;
  CHECK(sp.run() == NBA97_TEXT_UNKNOWN);
  CHECK(sp.progress.stopped_pc == UINT32_C(0x80099ad0));
  CHECK(sp.progress.operations == 0u);

  Fixture alignment;
  alignment.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word += 2u;
  CHECK(alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  CHECK(alignment.progress.operations == 1u);

  Fixture unmapped;
  unmapped.context.memory.count = 0u;
  CHECK(unmapped.run() == NBA97_TEXT_RESOURCE);

  Fixture late;
  late.known[late.offset(kDispatch + 11u)] = 2u;
  CHECK(late.run() == NBA97_TEXT_ARGUMENT);
  CHECK(late.progress.stopped_pc == UINT32_C(0x80099b50));
  CHECK(late.get32(kEnvironment + 0x1cu) == UINT32_C(0x12ffffff));

  Fixture overlap;
  Nba97GameTextRegion regions[2] = {overlap.region, overlap.region};
  overlap.context.memory = {regions, 2u};
  CHECK(overlap.run() == NBA97_TEXT_ARGUMENT);
  CHECK(overlap.progress.operations == 0u);

  Fixture atomic;
  atomic.context.machine.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S2].known_mask =
      7u;
  atomic.region.known = nullptr;
  const uint32_t before = atomic.get32(kStack - 8u);
  CHECK(atomic.run() == NBA97_TEXT_ARGUMENT);
  CHECK(atomic.get32(kStack - 8u) == before);
  CHECK(atomic.progress.stores == 0u);

  Fixture invalid;
  invalid.context.machine.registers.gpr[0].word = 1u;
  CHECK(invalid.run() == NBA97_TEXT_ARGUMENT);
  CHECK(invalid.progress.operations == 0u);
}

void test_wrap_budgets_journal_and_determinism() {
  std::array<uint8_t, 32> high{};
  std::array<uint8_t, 32> high_known{};
  std::array<uint8_t, 32> low{};
  std::array<uint8_t, 32> low_known{};
  high_known.fill(1u);
  low_known.fill(1u);
  Nba97GameTextRegion regions[3] = {
      {UINT32_C(0xffffffe0), high.data(), high_known.data(), high.size()},
      {0u, low.data(), low_known.data(), low.size()},
      {}};
  Fixture wrapping;
  regions[2] = wrapping.region;
  wrapping.context.memory = {regions, 3u};
  set_word(wrapping.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP],
           0u);
  CHECK(wrapping.run() == NBA97_TEXT_COMPLETE);
  CHECK(wrapping.progress.frame_stack_pointer == UINT32_C(0xffffffe0));
  CHECK(
      wrapping.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
      0u);

  Fixture complete;
  CHECK(complete.run() == NBA97_TEXT_COMPLETE);
  const size_t operations = complete.progress.operations;
  for (size_t budget = 0u; budget != operations; ++budget) {
    Fixture prefix;
    prefix.context.operation_budget = budget;
    CHECK(prefix.run() == NBA97_TEXT_LIMIT);
    CHECK(prefix.progress.operations == budget);
  }
  Fixture exact;
  exact.context.operation_budget = operations;
  CHECK(exact.run() == NBA97_TEXT_COMPLETE);

  Fixture short_journal;
  short_journal.context.access_journal_capacity = 2u;
  CHECK(short_journal.run() == NBA97_TEXT_COMPLETE);
  CHECK(short_journal.progress.access_events == 14u);

  Fixture first;
  Fixture second;
  CHECK(first.run() == NBA97_TEXT_COMPLETE);
  CHECK(second.run() == NBA97_TEXT_COMPLETE);
  CHECK(std::memcmp(&first.progress, &second.progress,
                    sizeof(first.progress)) == 0);
  CHECK(first.ram == second.ram && first.known == second.known);
}

int provide_hi_lo(void *, const Nba97GameSceneStartupEvent *,
                  Nba97GameDrawEnvironmentWord *hi,
                  Nba97GameDrawEnvironmentWord *lo) {
  set_word(*hi, UINT32_C(0x01020304));
  set_word(*lo, UINT32_C(0x05060708));
  return 1;
}

int malformed_hi_lo(void *, const Nba97GameSceneStartupEvent *,
                    Nba97GameDrawEnvironmentWord *hi,
                    Nba97GameDrawEnvironmentWord *lo) {
  set_word(*hi, UINT32_C(0x01020304), 16u);
  set_word(*lo, UINT32_C(0x05060708));
  return 1;
}

int accepting_scene_fallback(void *opaque, const Nba97GameTextMemory *,
                             const Nba97GameSceneStartupEvent *,
                             Nba97GameSceneStartupRegisters *) {
  ++*static_cast<size_t *>(opaque);
  return 1;
}

void test_scene_adapter_guards() {
  Fixture fixture;
  Nba97GameDrawEnvironmentSceneBinding binding{};
  nba97_game_draw_environment_scene_binding_init(
      &binding, 100u, Fixture::callback, &fixture, provide_hi_lo, nullptr,
      fixture.journal.data(), fixture.journal.size(), nullptr, nullptr);
  Nba97GameSceneStartupEvent event{};
  event.pc = UINT32_C(0x80048f4c);
  event.delay_slot_pc = UINT32_C(0x80048f50);
  event.entry = UINT32_C(0x80099acc);
  event.kind = NBA97_GAME_SCENE_STARTUP_DRAW_80099ACC;
  event.argument_count = 1u;
  Nba97GameSceneStartupRegisters registers = fixture.context.machine.registers;
  set_word(registers.gpr[NBA97_MATCH_INITIALIZE_A0], kEnvironment);
  set_word(registers.gpr[NBA97_MATCH_INITIALIZE_RA], UINT32_C(0x80048f54));
  CHECK(nba97_game_draw_environment_from_scene(
            &binding, &fixture.context.memory, &event, &registers) == 1);
  CHECK(binding.progress.machine.hi.word == UINT32_C(0x01020304));
  CHECK(binding.progress.machine.lo.word == UINT32_C(0x05060708));

  size_t fallback_calls = 0u;
  nba97_game_draw_environment_scene_binding_init(
      &binding, 100u, Fixture::callback, &fixture, nullptr, nullptr,
      fixture.journal.data(), fixture.journal.size(), accepting_scene_fallback,
      &fallback_calls);
  registers = fixture.context.machine.registers;
  set_word(registers.gpr[NBA97_MATCH_INITIALIZE_A0], kEnvironment);
  set_word(registers.gpr[NBA97_MATCH_INITIALIZE_RA], UINT32_C(0x80048fa8));
  event.pc = UINT32_C(0x80048fa0);
  event.delay_slot_pc = UINT32_C(0x80048fa4);
  CHECK(nba97_game_draw_environment_from_scene(
            &binding, &fixture.context.memory, &event, &registers) == 1);
  CHECK(binding.progress.machine.hi.known_mask == 0u);
  CHECK(binding.progress.machine.lo.known_mask == 0u);

  const auto immutable = registers;
  event.kind = NBA97_GAME_SCENE_STARTUP_DISPLAY_80099CA4;
  CHECK(nba97_game_draw_environment_from_scene(
            &binding, &fixture.context.memory, &event, &registers) == 0);
  CHECK(binding.result == NBA97_TEXT_ARGUMENT);
  CHECK(fallback_calls == 0u);
  CHECK(std::memcmp(&registers, &immutable, sizeof(registers)) == 0);

  event.entry = UINT32_C(0x80099ca4);
  CHECK(nba97_game_draw_environment_from_scene(
            &binding, &fixture.context.memory, &event, &registers) == 1);
  CHECK(fallback_calls == 1u);
  event.entry = UINT32_C(0x80099acc);
  event.kind = NBA97_GAME_SCENE_STARTUP_DRAW_80099ACC;

  registers = immutable;
  registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7u;
  const auto unknown_ra = registers;
  CHECK(nba97_game_draw_environment_from_scene(
            &binding, &fixture.context.memory, &event, &registers) == 0);
  CHECK(binding.result == NBA97_TEXT_ARGUMENT);
  CHECK(std::memcmp(&registers, &unknown_ra, sizeof(registers)) == 0);

  registers = immutable;
  registers.gpr[14].known_mask = 16u;
  const auto invalid_registers = registers;
  CHECK(nba97_game_draw_environment_from_scene(
            &binding, &fixture.context.memory, &event, &registers) == 0);
  CHECK(std::memcmp(&registers, &invalid_registers, sizeof(registers)) == 0);

  nba97_game_draw_environment_scene_binding_init(
      &binding, 100u, Fixture::callback, &fixture, malformed_hi_lo, nullptr,
      fixture.journal.data(), fixture.journal.size(), nullptr, nullptr);
  registers = immutable;
  set_word(registers.gpr[NBA97_MATCH_INITIALIZE_RA], UINT32_C(0x80048fa8));
  const auto before_provider = registers;
  CHECK(nba97_game_draw_environment_from_scene(
            &binding, &fixture.context.memory, &event, &registers) == 0);
  CHECK(binding.result == NBA97_TEXT_ARGUMENT);
  CHECK(std::memcmp(&registers, &before_provider, sizeof(registers)) == 0);
}

} // namespace

int main() {
  test_normal_and_exact_order();
  test_debug_paths();
  test_all_tag_masks_and_alias();
  test_live_callbacks_and_failures();
  test_unknowns_mapping_and_atomic_store();
  test_wrap_budgets_journal_and_determinism();
  test_scene_adapter_guards();
  std::printf("game_draw_environment_tests: %u checks passed\n", checks);
  return 0;
}
