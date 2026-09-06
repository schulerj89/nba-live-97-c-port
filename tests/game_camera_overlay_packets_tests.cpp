#include "recovered/game_camera_overlay_packets.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void check(bool condition, const char *expression, int line) {
  if (condition)
    return;
  std::fprintf(stderr, "check failed at line %d: %s\n", line, expression);
  std::exit(1);
}
#define CHECK(expression) check((expression), #expression, __LINE__)

constexpr uint32_t kBase = UINT32_C(0x80000000);
constexpr uint32_t kStack = UINT32_C(0x801ff000);
size_t g_executions = 0u;

void put8(std::vector<uint8_t> &ram, uint32_t address, uint8_t value) {
  ram[address - kBase] = value;
}
void put16(std::vector<uint8_t> &ram, uint32_t address, uint16_t value) {
  put8(ram, address, static_cast<uint8_t>(value));
  put8(ram, address + 1u, static_cast<uint8_t>(value >> 8u));
}
void put32(std::vector<uint8_t> &ram, uint32_t address, uint32_t value) {
  for (unsigned byte = 0u; byte != 4u; ++byte)
    put8(ram, address + byte, static_cast<uint8_t>(value >> (8u * byte)));
}
uint16_t get16(const std::vector<uint8_t> &ram, uint32_t address) {
  return static_cast<uint16_t>(ram[address - kBase] |
                               (ram[address - kBase + 1u] << 8u));
}
uint32_t get32(const std::vector<uint8_t> &ram, uint32_t address) {
  uint32_t value = 0u;
  for (unsigned byte = 0u; byte != 4u; ++byte)
    value |= static_cast<uint32_t>(ram[address - kBase + byte]) << (8u * byte);
  return value;
}
void set_word(Nba97GameCameraOverlayPacketsWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

struct CallLog {
  std::vector<Nba97GameCameraOverlayPacketsEvent> event;
  std::vector<Nba97GameCameraOverlayPacketsMachine> machine;
  uint8_t refuse_kind = 0u;
  uint32_t mutate_pc = 0u;
  bool malformed = false;
  bool runaway = false;
  uint32_t replacement_sp = 0u;
};

int child(void *opaque, const Nba97GameTextMemory *,
          const Nba97GameCameraOverlayPacketsEvent *event,
          Nba97GameCameraOverlayPacketsMachine *machine) {
  CallLog &log = *static_cast<CallLog *>(opaque);
  log.event.push_back(*event);
  log.machine.push_back(*machine);
  if (event->kind == log.refuse_kind)
    return NBA97_TEXT_IO_REFUSED;
  if (event->pc == log.mutate_pc) {
    set_word(machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1],
             log.runaway ? UINT32_C(0x80000000) : 12u);
    set_word(machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S2], 1u);
    set_word(machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S2 + 1u],
             UINT32_C(0x800fa050));
    set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0],
             UINT32_C(0x800fa054));
    if (log.replacement_sp != 0u)
      set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP],
               log.replacement_sp);
    set_word(machine->hi, UINT32_C(0x13572468));
    set_word(machine->lo, UINT32_C(0x24681357));
  }
  if (log.malformed)
    machine->registers.gpr[0].word = 1u;
  return NBA97_TEXT_COMPLETE;
}

struct Fixture {
  std::vector<uint8_t> ram = std::vector<uint8_t>(UINT32_C(0x200000), 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(UINT32_C(0x200000), 1u);
  Nba97GameTextRegion region{};
  Nba97GameCameraOverlayPacketsAccess journal[256]{};
  Nba97GameCameraOverlayPacketsContext context{};
  Nba97GameCameraOverlayPacketsProgress progress{};
  CallLog calls;

  Fixture() {
    region.base = kBase;
    region.data = ram.data();
    region.known = known.data();
    region.size = ram.size();
    context.memory.region = &region;
    context.memory.count = 1u;
    context.operation_budget = 1000u;
    context.io = child;
    context.user = &calls;
    context.access_journal = journal;
    context.access_journal_capacity = 256u;
    for (unsigned reg = 0u; reg != 32u; ++reg)
      set_word(context.machine.registers.gpr[reg], UINT32_C(0x10000000) + reg);
    set_word(context.machine.registers.gpr[0], 0u);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP], kStack);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x81234560));
    set_word(context.machine.hi, UINT32_C(0x11112222));
    set_word(context.machine.lo, UINT32_C(0x33334444));
    put16(ram, UINT32_C(0x800fe8cc), 2u);
  }

  int run() {
    ++g_executions;
    return nba97_game_camera_overlay_packets(&context, &progress);
  }

  void selection(int16_t state, int16_t x, int16_t y, int16_t sx,
                 int16_t sy, uint16_t flag = 0u) {
    put16(ram, UINT32_C(0x800fe8cc), static_cast<uint16_t>(state));
    put8(ram, UINT32_C(0x800bc1f0), 1u);
    put16(ram, UINT32_C(0x800fe8ca), 0u);
    put32(ram, UINT32_C(0x80020bec), UINT32_C(0x80030000));
    put16(ram, UINT32_C(0x80030004), 0u);
    put16(ram, UINT32_C(0x800f9ffe), flag);
    put16(ram, UINT32_C(0x800fe8d8), static_cast<uint16_t>(x));
    put16(ram, UINT32_C(0x800fe8da), static_cast<uint16_t>(y));
    put16(ram, UINT32_C(0x800fe8dc), static_cast<uint16_t>(sx));
    put16(ram, UINT32_C(0x800fe8de), static_cast<uint16_t>(sy));
    put16(ram, UINT32_C(0x800fe8f8), 0u);
    put16(ram, UINT32_C(0x800bc160), 100u);
    put16(ram, UINT32_C(0x800bc162), 200u);
    put32(ram, UINT32_C(0x8001ede8), 1u);
    put32(ram, UINT32_C(0x80102924), UINT32_C(0x80104000));
  }
};

void expect_complete(Fixture &fixture) {
  const int result = fixture.run();
  if (result != NBA97_TEXT_COMPLETE)
    std::fprintf(stderr, "result %d stopped %08x address %08x count %zu base %08x size %zu z %u/%u hi %u lo %u\n", result,
                 fixture.progress.stopped_pc,
                 fixture.progress.stopped_address, fixture.context.memory.count,
                 fixture.region.base, fixture.region.size,
                 fixture.context.machine.registers.gpr[0].word,
                 fixture.context.machine.registers.gpr[0].known_mask,
                 fixture.context.machine.hi.known_mask,
                 fixture.context.machine.lo.known_mask);
  CHECK(result == NBA97_TEXT_COMPLETE);
  CHECK(fixture.progress.completed == 1u);
  CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        kStack);
  CHECK(fixture.progress.restored_return_address.word == UINT32_C(0x81234560));
}

void test_state_gates_and_epilogue() {
  const int16_t states[] = {-32768, 2, 3, 4, 5, 7};
  for (int16_t state : states) {
    Fixture fixture;
    fixture.selection(state, 56, 56, 56, 56);
    expect_complete(fixture);
    const size_t expected = state >= 3 && state != 7 ? 2u : 0u;
    CHECK(fixture.calls.event.size() == expected);
  }
  Fixture disabled;
  disabled.selection(3, 0, 0, 0, 0);
  put8(disabled.ram, UINT32_C(0x800bc1f0), 0u);
  expect_complete(disabled);
  CHECK(disabled.calls.event.empty());

  Fixture negative_owner;
  negative_owner.selection(3, 0, 0, 0, 0);
  put16(negative_owner.ram, UINT32_C(0x80030004), UINT16_C(0xffff));
  expect_complete(negative_owner);
  CHECK(negative_owner.calls.event.empty());
}

void test_rectangles_calls_and_wrapping() {
  Fixture fixture;
  fixture.selection(5, -1, 55, 55, -1);
  put16(fixture.ram, UINT32_C(0x800bc160), UINT16_C(0xfff8));
  put16(fixture.ram, UINT32_C(0x800bc162), UINT16_C(0xffff));
  expect_complete(fixture);
  CHECK(fixture.calls.event.size() == 4u);
  CHECK(fixture.calls.event[0].pc == UINT32_C(0x80075f14));
  CHECK(fixture.calls.event[1].pc == UINT32_C(0x80076058));
  CHECK(fixture.calls.event[2].pc == UINT32_C(0x80076070));
  CHECK(fixture.calls.event[3].pc == UINT32_C(0x80076080));
  CHECK(fixture.calls.machine[0].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
        UINT32_C(0x800f9cd0));
  CHECK(get16(fixture.ram, UINT32_C(0x800f9ce8)) == 106u);
  CHECK(get16(fixture.ram, UINT32_C(0x800f9c88)) == UINT16_C(0xfffa));
  CHECK(get16(fixture.ram, UINT32_C(0x800f9c8a)) == 57u);

  for (int16_t coordinate : {-1, 55, 56}) {
    Fixture edge;
    edge.selection(5, coordinate, coordinate, 56, 56);
    expect_complete(edge);
    CHECK(edge.calls.event.size() == (coordinate < 56 ? 3u : 2u));
  }

  Fixture ordered;
  ordered.selection(3, 1, 2, 56, 56);
  expect_complete(ordered);
  const uint32_t store_pc[] = {
      UINT32_C(0x80075f94), UINT32_C(0x80075fa0),
      UINT32_C(0x80075fcc), UINT32_C(0x80075fd8),
      UINT32_C(0x80076008), UINT32_C(0x80076014),
      UINT32_C(0x80076048), UINT32_C(0x80076054)};
  const uint32_t store_address[] = {
      UINT32_C(0x800f9c98), UINT32_C(0x800f9c88),
      UINT32_C(0x800f9ca0), UINT32_C(0x800f9c90),
      UINT32_C(0x800f9c92), UINT32_C(0x800f9c8a),
      UINT32_C(0x800f9ca2), UINT32_C(0x800f9c9a)};
  size_t rectangle_store = 0u;
  for (size_t index = 0u; index != ordered.progress.access_events; ++index) {
    const Nba97GameCameraOverlayPacketsAccess &access = ordered.journal[index];
    if (access.pc < UINT32_C(0x80075f94) ||
        access.pc > UINT32_C(0x80076054) ||
        access.kind != NBA97_GAME_MATCH_CLOCKS_STORE)
      continue;
    CHECK(rectangle_store < 8u);
    CHECK(access.pc == store_pc[rectangle_store]);
    CHECK(access.address == store_address[rectangle_store]);
    ++rectangle_store;
  }
  CHECK(rectangle_store == 8u);

  Fixture aliased;
  aliased.selection(3, 1, 2, 56, 56);
  put16(aliased.ram, UINT32_C(0x800f9c88), 1u);
  Nba97GameTextRegion aliased_regions[3] = {
      {kBase, aliased.ram.data(), aliased.known.data(), UINT32_C(0xfe8d8)},
      {UINT32_C(0x800fe8d8),
       aliased.ram.data() + UINT32_C(0xf9c88),
       aliased.known.data() + UINT32_C(0xf9c88), 2u},
      {UINT32_C(0x800fe8da),
       aliased.ram.data() + UINT32_C(0xfe8da),
       aliased.known.data() + UINT32_C(0xfe8da),
       UINT32_C(0x200000) - UINT32_C(0xfe8da)}};
  aliased.context.memory.region = aliased_regions;
  aliased.context.memory.count = 3u;
  expect_complete(aliased);
  CHECK(get16(aliased.ram, UINT32_C(0x800f9c88)) == 106u);
  CHECK(get16(aliased.ram, UINT32_C(0x800f9ca0)) == 327u);
}

void test_loop_masks_and_live_mutation() {
  for (uint32_t mask : {0u, 1u, UINT32_C(0x1000), UINT32_C(0xffffffff)}) {
    Fixture fixture;
    fixture.selection(2, 56, 56, 56, 56, 1u);
    put16(fixture.ram, UINT32_C(0x800fa038), 0u);
    put32(fixture.ram, UINT32_C(0x800fa050), mask);
    expect_complete(fixture);
    const size_t bits = mask == 0u ? 0u : mask == UINT32_C(0xffffffff) ? 13u : 1u;
    CHECK(fixture.calls.event.size() == bits + 1u);
    CHECK(fixture.calls.event.back().pc == UINT32_C(0x80076108));
  }
  Fixture live;
  live.selection(2, 56, 56, 56, 56, 1u);
  put32(live.ram, UINT32_C(0x800fa050), 1u);
  live.calls.mutate_pc = UINT32_C(0x800760dc);
  expect_complete(live);
  CHECK(live.progress.machine.hi.word == UINT32_C(0x13572468));
  CHECK(live.progress.machine.lo.word == UINT32_C(0x24681357));

  Fixture moved_stack;
  moved_stack.selection(2, 56, 56, 56, 56, 1u);
  put32(moved_stack.ram, UINT32_C(0x800fa050), 1u);
  moved_stack.calls.mutate_pc = UINT32_C(0x800760dc);
  moved_stack.calls.replacement_sp = UINT32_C(0x801fe000);
  put32(moved_stack.ram, UINT32_C(0x801fe040), UINT32_C(0x81230000));
  put32(moved_stack.ram, UINT32_C(0x801fe03c), UINT32_C(0x19191919));
  put32(moved_stack.ram, UINT32_C(0x801fe038), UINT32_C(0x18181818));
  put32(moved_stack.ram, UINT32_C(0x801fe034), UINT32_C(0x17171717));
  put32(moved_stack.ram, UINT32_C(0x801fe030), UINT32_C(0x16161616));
  CHECK(moved_stack.run() == NBA97_TEXT_COMPLETE);
  CHECK(moved_stack.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
            .word == UINT32_C(0x801fe048));
  CHECK(moved_stack.progress.restored_s3.word == UINT32_C(0x19191919));

  Fixture runaway;
  runaway.selection(2, 56, 56, 56, 56, 1u);
  put32(runaway.ram, UINT32_C(0x800fa050), UINT32_C(0xffffffff));
  runaway.calls.mutate_pc = UINT32_C(0x800760dc);
  runaway.calls.runaway = true;
  runaway.context.operation_budget = 40u;
  CHECK(runaway.run() == NBA97_TEXT_LIMIT);
}

void test_quad_and_ten_arguments() {
  Fixture fixture;
  fixture.selection(2, 56, 56, 56, 56, UINT16_C(0x81));
  put16(fixture.ram, UINT32_C(0x800fa038), 0u);
  put32(fixture.ram, UINT32_C(0x800fa050), 0u);
  put32(fixture.ram, UINT32_C(0x800fe770), UINT32_C(0x20000001));
  put32(fixture.ram, UINT32_C(0x800fe774), UINT32_C(0xe0000002));
  expect_complete(fixture);
  CHECK(fixture.calls.event.size() == 5u);
  CHECK(fixture.calls.event[1].pc == UINT32_C(0x800761b0));
  CHECK(fixture.calls.event[2].pc == UINT32_C(0x800761b8));
  CHECK(fixture.calls.event[3].pc == UINT32_C(0x80076220));
  CHECK(fixture.calls.event[3].argument_count == 10u);
  const auto &machine = fixture.calls.machine[3];
  CHECK(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
        UINT32_C(0x800d8ef4));
  CHECK(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A3].word ==
        UINT32_C(0x800d8f0c));
  CHECK(get32(fixture.ram, kStack - 0x48u + 0x10u) ==
        UINT32_C(0x800fa2dc));
  CHECK(get32(fixture.ram, kStack - 0x48u + 0x14u) ==
        UINT32_C(0x800fa2e4));
  CHECK(get32(fixture.ram, kStack - 0x48u + 0x18u) ==
        UINT32_C(0x800fa2ec));
  CHECK(get32(fixture.ram, kStack - 0x48u + 0x1cu) ==
        UINT32_C(0x800fa2f4));
  CHECK(get32(fixture.ram, kStack - 0x48u + 0x20u) ==
        kStack - 0x20u);
  CHECK(get32(fixture.ram, kStack - 0x48u + 0x24u) ==
        kStack - 0x1cu);
  CHECK(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        UINT32_C(0x80076228));
  CHECK(fixture.calls.event[3].delay_slot_pc == UINT32_C(0x80076224));
  CHECK(get16(fixture.ram, UINT32_C(0x800d8ef4)) == UINT16_C(0xff88));
  CHECK(get16(fixture.ram, UINT32_C(0x800d8ef8)) == UINT16_C(0x0090));
  CHECK(fixture.calls.event[4].pc == UINT32_C(0x8007624c));
}

void test_unknowns_failures_and_prefixes() {
  Fixture unknown_state;
  unknown_state.known[UINT32_C(0xfe8cc)] = 0u;
  CHECK(unknown_state.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_state.progress.stores == 5u);
  CHECK(unknown_state.progress.stopped_pc == UINT32_C(0x80075d60));

  Fixture unknown_alternate_gate;
  unknown_alternate_gate.selection(2, 56, 56, 56, 56, 1u);
  unknown_alternate_gate.known[UINT32_C(0xfa038)] = 0u;
  CHECK(unknown_alternate_gate.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_alternate_gate.progress.stopped_pc == UINT32_C(0x800760ac));
  CHECK(unknown_alternate_gate.progress.machine.registers
            .gpr[NBA97_GAME_MATCH_CLOCKS_S2]
            .word == 1u);

  Fixture unknown_pointer;
  unknown_pointer.selection(3, 56, 56, 56, 56);
  unknown_pointer.known[UINT32_C(0x20bec)] = 0u;
  CHECK(unknown_pointer.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_pointer.progress.stopped_pc == UINT32_C(0x80075da8));

  Fixture unknown_sp;
  unknown_sp.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
      .known_mask = 14u;
  CHECK(unknown_sp.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_sp.progress.stopped_pc == UINT32_C(0x80075d4c));

  Fixture unknown_ra;
  unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 7u;
  CHECK(unknown_ra.run() == NBA97_TEXT_UNKNOWN);
  CHECK(unknown_ra.progress.stopped_pc == UINT32_C(0x8007626c));

  Fixture malformed;
  malformed.selection(3, 56, 56, 56, 56);
  malformed.calls.malformed = true;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT);
  CHECK(malformed.progress.stopped_pc == UINT32_C(0x80076070));

  const uint8_t kinds[] = {
      NBA97_GAME_CAMERA_OVERLAY_PACKETS_LINK_80056914,
      NBA97_GAME_CAMERA_OVERLAY_PACKETS_SET_ROTATION_80055F18,
      NBA97_GAME_CAMERA_OVERLAY_PACKETS_SET_TRANSLATION_80055F44,
      NBA97_GAME_CAMERA_OVERLAY_PACKETS_PROJECT_QUAD_80055FE4};
  for (uint8_t kind : kinds) {
    Fixture refused;
    refused.selection(2, 56, 56, 56, 56, UINT16_C(0x81));
    put16(refused.ram, UINT32_C(0x800fa038), 0u);
    refused.calls.refuse_kind = kind;
    CHECK(refused.run() == NBA97_TEXT_IO_REFUSED);
  }

  Fixture no_known_array;
  no_known_array.region.known = nullptr;
  no_known_array.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0]
      .known_mask = 7u;
  put32(no_known_array.ram, kStack - 0x18u, UINT32_C(0xa5a5a5a5));
  CHECK(no_known_array.run() == NBA97_TEXT_ARGUMENT);
  CHECK(no_known_array.progress.stopped_pc == UINT32_C(0x80075d64));
  CHECK(get32(no_known_array.ram, kStack - 8u) == UINT32_C(0x81234560));
  CHECK(get32(no_known_array.ram, kStack - 0x18u) == UINT32_C(0xa5a5a5a5));

  Fixture alignment;
  set_word(alignment.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP],
           kStack + 2u);
  CHECK(alignment.run() == NBA97_TEXT_ALIGNMENT_TRAP);

  Fixture absent;
  absent.context.memory.count = 0u;
  CHECK(absent.run() == NBA97_TEXT_RESOURCE);
}

void test_every_operation_cutoff_and_stack_wrap() {
  Fixture full;
  full.selection(5, 0, 0, 0, 0);
  expect_complete(full);
  const size_t operations = full.progress.operations;
  CHECK(operations > 40u);
  for (size_t budget = 0u; budget != operations; ++budget) {
    Fixture fixture;
    fixture.selection(5, 0, 0, 0, 0);
    fixture.context.operation_budget = budget;
    CHECK(fixture.run() == NBA97_TEXT_LIMIT);
    CHECK(fixture.progress.operations == budget);
  }

  Fixture wrapped;
  uint8_t low[80] = {};
  uint8_t low_known[80];
  std::memset(low_known, 1, sizeof(low_known));
  Nba97GameTextRegion regions[2] = {
      wrapped.region, {UINT32_C(0xffffffb0), low, low_known, sizeof(low)}};
  wrapped.context.memory.region = regions;
  wrapped.context.memory.count = 2u;
  set_word(wrapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP],
           0u);
  CHECK(wrapped.run() == NBA97_TEXT_COMPLETE);
  CHECK(wrapped.progress.completed == 1u);
  CHECK(wrapped.progress.frame_stack_pointer == UINT32_C(0xffffffb8));
  CHECK(wrapped.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        0u);
}

} // namespace

int main() {
  test_state_gates_and_epilogue();
  test_rectangles_calls_and_wrapping();
  test_loop_masks_and_live_mutation();
  test_quad_and_ten_arguments();
  test_unknowns_failures_and_prefixes();
  test_every_operation_cutoff_and_stack_wrap();
  std::printf("game_camera_overlay_packets_tests: %zu focused executions passed\n",
              g_executions);
  return 0;
}
