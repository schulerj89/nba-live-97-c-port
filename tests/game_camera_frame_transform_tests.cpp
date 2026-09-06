#include "recovered/game_camera_frame_transform.h"

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

void put8(std::vector<uint8_t> &ram, uint32_t address, uint8_t value) {
  ram[address - kBase] = value;
}
void put16(std::vector<uint8_t> &ram, uint32_t address, uint16_t value) {
  put8(ram, address, static_cast<uint8_t>(value));
  put8(ram, address + 1u, static_cast<uint8_t>(value >> 8u));
}
void put32(std::vector<uint8_t> &ram, uint32_t address, uint32_t value) {
  for (unsigned byte = 0; byte != 4u; ++byte)
    put8(ram, address + byte, static_cast<uint8_t>(value >> (8u * byte)));
}
uint16_t get16(const std::vector<uint8_t> &ram, uint32_t address) {
  return static_cast<uint16_t>(ram[address - kBase] |
                               (ram[address + 1u - kBase] << 8u));
}
uint32_t get32(const std::vector<uint8_t> &ram, uint32_t address) {
  uint32_t value = 0u;
  for (unsigned byte = 0; byte != 4u; ++byte)
    value |= static_cast<uint32_t>(ram[address + byte - kBase]) << (8u * byte);
  return value;
}
void set_word(Nba97GameCameraFrameTransformWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

struct Calls {
  std::vector<Nba97GameCameraFrameTransformEvent> events;
  std::vector<Nba97GameCameraFrameTransformMachine> machines;
  uint8_t refuse_kind = 0u;
  uint8_t malformed_kind = 0u;
  bool mutate_reference = false;
  bool mutate_memory = false;
  bool unknown_matrix_s0 = false;
  bool mutate_rotation_s0 = false;
  bool mutate_translation_sp = false;
  uint32_t alternate_sp = UINT32_C(0x801fe000);
  uint32_t alternate_s0 = UINT32_C(0x800fc700);
  uint32_t alternate_s1 = UINT32_C(0x800fb900);
};

int child(void *opaque, const Nba97GameTextMemory *memory,
          const Nba97GameCameraFrameTransformEvent *event,
          Nba97GameCameraFrameTransformMachine *machine) {
  Calls &calls = *static_cast<Calls *>(opaque);
  calls.events.push_back(*event);
  calls.machines.push_back(*machine);
  CHECK(machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 15u);
  CHECK(machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        event->pc + 8u);
  if (calls.refuse_kind == event->kind)
    return 0;
  if (calls.malformed_kind == event->kind)
    machine->registers.gpr[14].known_mask = 16u;
  if (calls.unknown_matrix_s0 &&
      event->kind == NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATRIX_80056080)
    machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0].known_mask = 0u;
  if (calls.mutate_rotation_s0 &&
      event->kind == NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_ROTATION_80055F18)
    set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0],
             UINT32_C(0x800f9f00));
  if (calls.mutate_translation_sp &&
      event->kind == NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_TRANSLATION_80055F44)
    set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP],
             calls.alternate_sp);
  if (calls.mutate_memory &&
      event->kind == NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_ROTATION_80055F18)
    memory->region[0].data[UINT32_C(0x18000)] = UINT8_C(0x5a);
  if (calls.mutate_reference &&
      event->kind == NBA97_GAME_CAMERA_FRAME_TRANSFORM_REFERENCE_80056650) {
    set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP],
             calls.alternate_sp);
    set_word(machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0],
             calls.alternate_s0);
    set_word(machine->registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1],
             calls.alternate_s1);
    set_word(machine->hi, UINT32_C(0x11223344));
    set_word(machine->lo, UINT32_C(0x55667788));
  }
  return 1;
}

struct Fixture {
  std::vector<uint8_t> ram = std::vector<uint8_t>(UINT32_C(0x200000), 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(UINT32_C(0x200000), 1u);
  Nba97GameTextRegion region{};
  Nba97GameCameraFrameTransformAccess journal[128]{};
  Nba97GameCameraFrameTransformContext context{};
  Nba97GameCameraFrameTransformProgress progress{};
  Calls calls{};

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
    context.access_journal_capacity = 128u;
    for (unsigned reg = 0; reg != 32u; ++reg)
      set_word(context.machine.registers.gpr[reg], UINT32_C(0x10000000) + reg);
    set_word(context.machine.registers.gpr[0], 0u);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP], kStack);
    set_word(context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x81234567));
    set_word(context.machine.hi, UINT32_C(0x01020304));
    set_word(context.machine.lo, UINT32_C(0x05060708));
    put32(ram, UINT32_C(0x800eb678), 1u);
    put16(ram, UINT32_C(0x800fa638), 0u);
    put16(ram, UINT32_C(0x800fa63a), 0u);
    put16(ram, UINT32_C(0x800fa63c), 0u);
    put16(ram, UINT32_C(0x800fa630), UINT16_C(0xffff));
    put16(ram, UINT32_C(0x800fa632), UINT16_C(0x8000));
    put16(ram, UINT32_C(0x800fa634), UINT16_C(0x7fff));
    put16(ram, UINT32_C(0x800fb858), 0u);
    put16(ram, UINT32_C(0x800fb85a), 0u);
    put16(ram, UINT32_C(0x800fb85c), 0u);
    put16(ram, UINT32_C(0x800f9fd8), 1u);
    put16(ram, UINT32_C(0x800f9fda), UINT16_C(0xffff));
    put16(ram, UINT32_C(0x800f9fdc), UINT16_C(0x7fff));
    put32(ram, UINT32_C(0x800fc61c), 1u);
    put32(ram, UINT32_C(0x800fc620), UINT32_C(0x80000000));
    put32(ram, UINT32_C(0x800fc624), UINT32_C(0xffffffff));
  }

  int run() {
    calls.events.clear();
    calls.machines.clear();
    return nba97_game_camera_frame_transform(&context, &progress);
  }
};

void expect_complete(Fixture &fixture, size_t calls) {
  CHECK(fixture.run() == NBA97_TEXT_COMPLETE);
  CHECK(fixture.progress.completed == 1u);
  CHECK(fixture.calls.events.size() == calls);
}

void test_flag_and_exact_calls() {
  Fixture skipped;
  expect_complete(skipped, 4u);
  const uint32_t pcs[] = {UINT32_C(0x80051168), UINT32_C(0x80051204),
                          UINT32_C(0x8005120c), UINT32_C(0x80051228)};
  const uint32_t entries[] = {UINT32_C(0x80056080), UINT32_C(0x80055f18),
                              UINT32_C(0x80055f44), UINT32_C(0x80056650)};
  const uint8_t arguments[] = {2u, 1u, 1u, 3u};
  for (unsigned index = 0; index != 4u; ++index) {
    CHECK(skipped.calls.events[index].pc == pcs[index]);
    CHECK(skipped.calls.events[index].delay_slot_pc == pcs[index] + 4u);
    CHECK(skipped.calls.events[index].entry == entries[index]);
    CHECK(skipped.calls.events[index].argument_count == arguments[index]);
  }
  const uint32_t frame = kStack - 0x30u;
  CHECK(
      skipped.calls.machines[0].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
      frame + 0x10u);
  CHECK(
      skipped.calls.machines[0].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
      UINT32_C(0x800f9fd8));
  CHECK(
      skipped.calls.machines[1].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
      UINT32_C(0x800f9fd8));
  CHECK(
      skipped.calls.machines[2].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
      UINT32_C(0x800f9fd8));
  CHECK(
      skipped.calls.machines[3].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
      UINT32_C(0x800fab98));
  CHECK(
      skipped.calls.machines[3].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
      UINT32_C(0x800fc61c));
  CHECK(
      skipped.calls.machines[3].registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
      frame + 0x18u);

  Fixture called;
  put32(called.ram, UINT32_C(0x800eb678), 0u);
  expect_complete(called, 5u);
  CHECK(called.calls.events[0].pc == UINT32_C(0x800510b4));
  CHECK(called.calls.events[0].entry == UINT32_C(0x8004ea88));
  CHECK(called.calls.events[0].argument_count == 0u);
}

void test_angles_copy_and_wrap() {
  struct Case {
    uint16_t angle;
    uint16_t offset;
    uint16_t masked;
    uint16_t sum;
  } cases[] = {
      {0u, 0u, 0u, 0u},
      {UINT16_C(0x0fff), 1u, UINT16_C(0x0fff), UINT16_C(0x1000)},
      {UINT16_C(0x1000), UINT16_C(0xffff), 0u, UINT16_C(0xffff)},
      {UINT16_C(0xffff), UINT16_C(0xffff), UINT16_C(0x0fff), UINT16_C(0x0ffe)}};
  for (const Case &item : cases) {
    Fixture fixture;
    put16(fixture.ram, UINT32_C(0x800fa638), item.angle);
    put16(fixture.ram, UINT32_C(0x800fa63a), item.angle);
    put16(fixture.ram, UINT32_C(0x800fa63c), item.angle);
    put16(fixture.ram, UINT32_C(0x800fb858), item.offset);
    put16(fixture.ram, UINT32_C(0x800fb85a), item.offset);
    put16(fixture.ram, UINT32_C(0x800fb85c), item.offset);
    expect_complete(fixture, 4u);
    CHECK(get16(fixture.ram, UINT32_C(0x800fa638)) == item.masked);
    CHECK(get16(fixture.ram, UINT32_C(0x800fa63a)) == item.masked);
    CHECK(get16(fixture.ram, UINT32_C(0x800fa63c)) == item.masked);
    const uint32_t frame = kStack - 0x30u;
    CHECK(get16(fixture.ram, frame + 0x10u) == item.sum);
    CHECK(get16(fixture.ram, frame + 0x12u) == item.sum);
    CHECK(get16(fixture.ram, frame + 0x14u) == item.sum);
    CHECK(get16(fixture.ram, UINT32_C(0x800fb828)) == UINT16_C(0xffff));
    CHECK(get16(fixture.ram, UINT32_C(0x800fb82a)) == UINT16_C(0x8000));
    CHECK(get16(fixture.ram, UINT32_C(0x800fb82c)) == UINT16_C(0x7fff));
  }
}

uint16_t scaled(int16_t value) {
  return static_cast<uint16_t>((static_cast<int32_t>(value) * 16) / 10);
}

struct ProductMasks {
  uint8_t lo;
  uint8_t hi;
};

ProductMasks enumerate_product_masks(uint16_t observed, uint8_t known_mask) {
  uint8_t first_byte[8] = {};
  bool invariant[8] = {true, true, true, true, true, true, true, true};
  bool first = true;
  for (uint32_t candidate = 0u; candidate != UINT32_C(0x10000); ++candidate) {
    if ((known_mask & 1u) != 0u && (candidate & 255u) != (observed & 255u))
      continue;
    if ((known_mask & 2u) != 0u &&
        (candidate >> 8u) != (static_cast<uint32_t>(observed) >> 8u))
      continue;
    const int32_t signed_half = candidate < UINT32_C(0x8000)
                                    ? static_cast<int32_t>(candidate)
                                    : static_cast<int32_t>(candidate) - 0x10000;
    const int64_t product =
        static_cast<int64_t>(signed_half * 16) * INT64_C(0x66666667);
    const uint64_t bits = static_cast<uint64_t>(product);
    for (unsigned byte = 0; byte != 8u; ++byte) {
      const uint8_t value = static_cast<uint8_t>(bits >> (8u * byte));
      if (first)
        first_byte[byte] = value;
      else if (first_byte[byte] != value)
        invariant[byte] = false;
    }
    first = false;
  }
  ProductMasks masks{};
  for (unsigned byte = 0; byte != 4u; ++byte) {
    if (invariant[byte])
      masks.lo = static_cast<uint8_t>(masks.lo | (1u << byte));
    if (invariant[byte + 4u])
      masks.hi = static_cast<uint8_t>(masks.hi | (1u << byte));
  }
  return masks;
}

void test_signed_scaling_and_mult_traces() {
  const int16_t values[] = {INT16_MIN, -1, 0, 1, INT16_MAX};
  for (int16_t value : values) {
    Fixture fixture;
    put16(fixture.ram, UINT32_C(0x800f9fd8), static_cast<uint16_t>(value));
    put16(fixture.ram, UINT32_C(0x800f9fda), static_cast<uint16_t>(value));
    put16(fixture.ram, UINT32_C(0x800f9fdc), static_cast<uint16_t>(value));
    expect_complete(fixture, 4u);
    CHECK(get16(fixture.ram, UINT32_C(0x800f9fd8)) == scaled(value));
    CHECK(get16(fixture.ram, UINT32_C(0x800f9fda)) == scaled(value));
    CHECK(get16(fixture.ram, UINT32_C(0x800f9fdc)) == scaled(value));
    CHECK(fixture.progress.multiply_count == 3u);
    CHECK(fixture.progress.multiply[0].pc == UINT32_C(0x80051180));
    CHECK(fixture.progress.multiply[0].mfhi_pc == UINT32_C(0x8005118c));
    CHECK(fixture.progress.multiply[1].pc == UINT32_C(0x80051198));
    CHECK(fixture.progress.multiply[1].mfhi_pc == UINT32_C(0x800511a4));
    CHECK(fixture.progress.multiply[2].pc == UINT32_C(0x800511b4));
    CHECK(fixture.progress.multiply[2].mfhi_pc == UINT32_C(0x800511f0));
  }

  const uint16_t observed_values[] = {0u, UINT16_C(0xffff), UINT16_C(0x8001)};
  for (uint16_t observed : observed_values) {
    for (uint8_t known_mask = 0u; known_mask != 4u; ++known_mask) {
      Fixture partial;
      put16(partial.ram, UINT32_C(0x800f9fd8), observed);
      partial.known[UINT32_C(0xf9fd8)] = known_mask & 1u;
      partial.known[UINT32_C(0xf9fd9)] = (known_mask >> 1u) & 1u;
      expect_complete(partial, 4u);
      const ProductMasks expected =
          enumerate_product_masks(observed, known_mask);
      CHECK(partial.progress.multiply[0].lo.known_mask == expected.lo);
      CHECK(partial.progress.multiply[0].hi.known_mask == expected.hi);
    }
  }
}

void test_final_translation_and_live_callback_state() {
  Fixture fixture;
  expect_complete(fixture, 4u);
  CHECK(get32(fixture.ram, UINT32_C(0x800f9fec)) == 0u);
  CHECK(get32(fixture.ram, UINT32_C(0x800f9ff0)) == UINT32_C(0x7fff8000));
  CHECK(get32(fixture.ram, UINT32_C(0x800f9ff4)) == UINT32_C(0x00007ffe));

  Fixture live;
  live.calls.mutate_reference = true;
  live.calls.mutate_memory = true;
  put16(live.ram, live.calls.alternate_s1, UINT16_C(0xfffe));
  put32(live.ram, live.calls.alternate_s0, UINT32_C(0xffffffff));
  put32(live.ram, live.calls.alternate_sp + 0x28u, UINT32_C(0x87654321));
  put32(live.ram, live.calls.alternate_sp + 0x24u, UINT32_C(0x11111111));
  put32(live.ram, live.calls.alternate_sp + 0x20u, UINT32_C(0x22222222));
  expect_complete(live, 4u);
  CHECK(get32(live.ram, UINT32_C(0x800f9fec)) == UINT32_C(0xfffffffd));
  CHECK(live.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        UINT32_C(0x87654321));
  CHECK(live.progress.machine.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1].word ==
        UINT32_C(0x11111111));
  CHECK(live.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0].word ==
        UINT32_C(0x22222222));
  CHECK(live.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
        live.calls.alternate_sp + 0x30u);
  CHECK(live.progress.machine.hi.word == UINT32_C(0x11223344));
  CHECK(live.progress.machine.lo.word == UINT32_C(0x55667788));
  CHECK(live.ram[UINT32_C(0x18000)] == UINT8_C(0x5a));

  Fixture delays;
  delays.calls.mutate_rotation_s0 = true;
  delays.calls.mutate_translation_sp = true;
  put32(delays.ram, delays.calls.alternate_sp + 0x28u, UINT32_C(0x81234567));
  put32(delays.ram, delays.calls.alternate_sp + 0x24u, UINT32_C(0x11111111));
  put32(delays.ram, delays.calls.alternate_sp + 0x20u, UINT32_C(0x22222222));
  expect_complete(delays, 4u);
  CHECK(
      delays.calls.machines[2].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
      UINT32_C(0x800f9f00));
  CHECK(
      delays.calls.machines[3].registers.gpr[NBA97_MATCH_INITIALIZE_A2].word ==
      delays.calls.alternate_sp + 0x18u);
}

void test_failures_unknowns_and_aliases() {
  {
    Fixture fixture;
    fixture.context.memory.region = nullptr;
    CHECK(fixture.run() == NBA97_TEXT_ARGUMENT);
  }
  {
    Fixture fixture;
    Nba97GameTextRegion regions[2] = {fixture.region, fixture.region};
    fixture.context.memory.region = regions;
    fixture.context.memory.count = 2u;
    CHECK(fixture.run() == NBA97_TEXT_ARGUMENT);
  }
  {
    Fixture fixture;
    fixture.known[UINT32_C(0xeb678)] = 2u;
    CHECK(fixture.run() == NBA97_TEXT_ARGUMENT);
    CHECK(fixture.progress.stopped_pc == UINT32_C(0x8005109c));
  }
  {
    Fixture fixture;
    fixture.context.memory.count = 0u;
    CHECK(fixture.run() == NBA97_TEXT_RESOURCE);
    CHECK(fixture.progress.stopped_pc == UINT32_C(0x8005109c));
    CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
              .word == kStack);
  }
  {
    Fixture fixture;
    fixture.known[UINT32_C(0xeb678)] = 0u;
    CHECK(fixture.run() == NBA97_TEXT_UNKNOWN);
    CHECK(fixture.progress.stopped_pc == UINT32_C(0x800510ac));
    CHECK(fixture.progress.stores == 3u);
  }
  {
    Fixture fixture;
    fixture.context.machine.registers.gpr[NBA97_GAME_MATCH_CLOCKS_S1]
        .known_mask = 14u;
    fixture.region.known = nullptr;
    CHECK(fixture.run() == NBA97_TEXT_ARGUMENT);
    CHECK(fixture.progress.stopped_pc == UINT32_C(0x800510a8));
  }
  {
    Fixture fixture;
    fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word |= 1u;
    CHECK(fixture.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  }
  {
    Fixture fixture;
    fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = 7u;
    CHECK(fixture.run() == NBA97_TEXT_UNKNOWN);
    CHECK(fixture.progress.stopped_pc == UINT32_C(0x8005128c));
  }
  {
    Fixture fixture;
    fixture.calls.malformed_kind =
        NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATRIX_80056080;
    CHECK(fixture.run() == NBA97_TEXT_ARGUMENT);
    CHECK(fixture.progress.stopped_pc == UINT32_C(0x80051168));
  }
  {
    Fixture fixture;
    fixture.calls.unknown_matrix_s0 = true;
    CHECK(fixture.run() == NBA97_TEXT_UNKNOWN);
    CHECK(fixture.progress.stopped_pc == UINT32_C(0x80051170));
    CHECK(fixture.progress.callbacks_completed == 1u);
  }
  const uint8_t kinds[] = {
      NBA97_GAME_CAMERA_FRAME_TRANSFORM_CONTROLLER_8004EA88,
      NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATRIX_80056080,
      NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_ROTATION_80055F18,
      NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_TRANSLATION_80055F44,
      NBA97_GAME_CAMERA_FRAME_TRANSFORM_REFERENCE_80056650};
  const uint32_t pcs[] = {UINT32_C(0x800510b4), UINT32_C(0x80051168),
                          UINT32_C(0x80051204), UINT32_C(0x8005120c),
                          UINT32_C(0x80051228)};
  for (unsigned index = 0; index != 5u; ++index) {
    Fixture fixture;
    if (index == 0u)
      put32(fixture.ram, UINT32_C(0x800eb678), 0u);
    fixture.calls.refuse_kind = kinds[index];
    CHECK(fixture.run() == NBA97_TEXT_IO_REFUSED);
    CHECK(fixture.progress.stopped_pc == pcs[index]);
  }
  {
    Fixture fixture;
    const uint32_t frame = UINT32_C(0x800fa628);
    set_word(fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP],
             frame + 0x30u);
    put16(fixture.ram, UINT32_C(0x800fa638), 1u);
    put16(fixture.ram, UINT32_C(0x800fb858), 2u);
    expect_complete(fixture, 4u);
    CHECK(get16(fixture.ram, UINT32_C(0x800fa638)) == 3u);
  }
  {
    Fixture fixture;
    uint8_t low[32] = {};
    uint8_t low_known[32];
    std::memset(low_known, 1, sizeof(low_known));
    Nba97GameTextRegion regions[2] = {fixture.region,
                                      {0u, low, low_known, sizeof(low)}};
    fixture.context.memory.region = regions;
    fixture.context.memory.count = 2u;
    set_word(fixture.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP],
             0x20u);
    expect_complete(fixture, 4u);
    CHECK(fixture.progress.frame_stack_pointer == UINT32_C(0xfffffff0));
    CHECK(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
              .word == 0x20u);
  }
}

void test_access_lui_delay_and_mult_prefixes() {
  Fixture full;
  expect_complete(full, 4u);
  CHECK(full.progress.access_events >= 4u);
  CHECK(full.journal[0].pc == UINT32_C(0x8005109c) &&
        full.journal[0].address == UINT32_C(0x800eb678));
  CHECK(full.journal[1].pc == UINT32_C(0x800510a4) &&
        full.journal[1].address == kStack - 8u);
  CHECK(full.journal[2].pc == UINT32_C(0x800510a8) &&
        full.journal[2].address == kStack - 12u);
  CHECK(full.journal[3].pc == UINT32_C(0x800510b0) &&
        full.journal[3].address == kStack - 16u);

  Fixture entry_lui;
  entry_lui.context.operation_budget = 0u;
  CHECK(entry_lui.run() == NBA97_TEXT_LIMIT);
  CHECK(entry_lui.progress.stopped_pc == UINT32_C(0x8005109c));
  CHECK(entry_lui.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
            .word == UINT32_C(0x800f0000));
  CHECK(entry_lui.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
            .word == kStack);

  Fixture angle_lui;
  angle_lui.context.operation_budget = 4u;
  CHECK(angle_lui.run() == NBA97_TEXT_LIMIT);
  CHECK(angle_lui.progress.stopped_pc == UINT32_C(0x800510c8));
  CHECK(angle_lui.progress.machine.registers.gpr[7].word ==
        UINT32_C(0x800fa638));

  Fixture matrix_delay;
  matrix_delay.context.operation_budget = 22u;
  CHECK(matrix_delay.run() == NBA97_TEXT_LIMIT);
  CHECK(matrix_delay.progress.stopped_pc == UINT32_C(0x80051168));
  CHECK(matrix_delay.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
            .word == UINT32_C(0x80051170));
  CHECK(get16(matrix_delay.ram, kStack - 0x30u + 0x14u) == 0u);

  Fixture first_mult;
  first_mult.context.operation_budget = 24u;
  CHECK(first_mult.run() == NBA97_TEXT_LIMIT);
  CHECK(first_mult.progress.stopped_pc == UINT32_C(0x80051188));
  CHECK(first_mult.progress.multiply_count == 1u);
  CHECK(first_mult.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V1]
            .word == UINT32_C(0x80100000));

  Fixture second_mult;
  second_mult.context.operation_budget = 25u;
  CHECK(second_mult.run() == NBA97_TEXT_LIMIT);
  CHECK(second_mult.progress.stopped_pc == UINT32_C(0x800511a0));
  CHECK(second_mult.progress.multiply_count == 2u);
  CHECK(second_mult.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2]
            .word == UINT32_C(0x80100000));

  Fixture third_mult;
  third_mult.context.operation_budget = 27u;
  CHECK(third_mult.run() == NBA97_TEXT_LIMIT);
  CHECK(third_mult.progress.stopped_pc == UINT32_C(0x800511bc));
  CHECK(third_mult.progress.multiply_count == 3u);
}

void test_every_budget_prefix() {
  Fixture full;
  expect_complete(full, 4u);
  const size_t operations = full.progress.operations;
  CHECK(operations == 47u);
  for (size_t budget = 0; budget != operations; ++budget) {
    Fixture fixture;
    fixture.context.operation_budget = budget;
    CHECK(fixture.run() == NBA97_TEXT_LIMIT);
    CHECK(fixture.progress.operations == budget);
  }
  Fixture exact;
  exact.context.operation_budget = operations;
  expect_complete(exact, 4u);
}

} // namespace

int main() {
  test_flag_and_exact_calls();
  test_angles_copy_and_wrap();
  test_signed_scaling_and_mult_traces();
  test_final_translation_and_live_callback_state();
  test_failures_unknowns_and_aliases();
  test_access_lui_delay_and_mult_prefixes();
  test_every_budget_prefix();
  std::puts("game_camera_frame_transform_tests: 99 focused executions, "
            "including every 47-operation budget cutoff, passed");
  return 0;
}
