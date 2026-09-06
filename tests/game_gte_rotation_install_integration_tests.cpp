#include "game_gte_rotation_install_adapter.h"
#include "game_rotation_matrix_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

size_t checks = 0u;
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
uint32_t get32(const std::vector<uint8_t> &ram, uint32_t address) {
  uint32_t value = 0u;
  for (unsigned byte = 0u; byte != 4u; ++byte)
    value |= static_cast<uint32_t>(ram[address - kBase + byte])
             << (8u * byte);
  return value;
}
void set_word(Nba97GameGteRotationInstallWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

struct Natural {
  std::vector<uint8_t> ram = std::vector<uint8_t>(UINT32_C(0x200000), 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(UINT32_C(0x200000), 1u);
  Nba97GameTextRegion region{};
  Nba97GameTextMemory memory{};
  Nba97GameCameraFrameTransformContext camera{};
  Nba97GameCameraFrameTransformProgress camera_progress{};
  Nba97GameGteRotationInstallCameraBinding rotation{};
  Nba97GameRotationMatrixBinding matrix{};
  size_t fallback_calls = 0u;
  bool translation_observed = false;

  Natural() {
    region.base = kBase;
    region.data = ram.data();
    region.known = known.data();
    region.size = ram.size();
    memory.region = &region;
    memory.count = 1u;
    for (unsigned reg = 0u; reg != 32u; ++reg)
      set_word(camera.machine.registers.gpr[reg], UINT32_C(0x20000000) + reg);
    set_word(camera.machine.registers.gpr[0], 0u);
    set_word(camera.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP], kStack);
    set_word(camera.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x81234560));
    set_word(camera.machine.hi, UINT32_C(0x11112222));
    set_word(camera.machine.lo, UINT32_C(0x33334444));
    put32(ram, UINT32_C(0x800eb678), 1u);
    put16(ram, UINT32_C(0x800fa638), 1u);
    put16(ram, UINT32_C(0x800fa63a), 2u);
    put16(ram, UINT32_C(0x800fa63c), 3u);
    put16(ram, UINT32_C(0x800fa630), 4u);
    put16(ram, UINT32_C(0x800fa632), 5u);
    put16(ram, UINT32_C(0x800fa634), 6u);
    put32(ram, UINT32_C(0x800f9fd8), UINT32_C(0x11223344));
    put32(ram, UINT32_C(0x800f9fdc), UINT32_C(0x55667788));
    put32(ram, UINT32_C(0x800f9fe0), UINT32_C(0x99aabbcc));
    put32(ram, UINT32_C(0x800f9fe4), UINT32_C(0xddeeff00));
    put32(ram, UINT32_C(0x800f9fe8), UINT32_C(0xabcd8001));
    put32(ram, UINT32_C(0x800fc61c), 100u);
    put32(ram, UINT32_C(0x800fc620), 200u);
    put32(ram, UINT32_C(0x800fc624), 300u);
    for (unsigned i = 0; i < 4096; ++i)
      put32(ram, UINT32_C(0x800b3254) + 4u * i, UINT32_C(0x10001000));
    nba97_game_rotation_matrix_binding_init(&matrix, 1000u, nullptr, 0u);
    camera.memory = memory;
    camera.operation_budget = 1000u;
    camera.io = dispatch;
    camera.user = this;
    nba97_game_gte_rotation_install_camera_binding_init(
        &rotation, nullptr, 10u, nullptr, 0u, nullptr, 0u, fallback, this);
  }

  static int fallback(void *opaque, const Nba97GameTextMemory *memory,
                      const Nba97GameCameraFrameTransformEvent *event,
                      Nba97GameCameraFrameTransformMachine *machine) {
    Natural &self = *static_cast<Natural *>(opaque);
    ++self.fallback_calls;
    if (event->pc == UINT32_C(0x80051168)) {
      return nba97_game_rotation_matrix_from_camera_frame_transform(
          &self.matrix, memory, event, machine);
    }
    if (event->pc == UINT32_C(0x8005120c)) {
      self.translation_observed = true;
      for (unsigned index = 0u; index != 4u; ++index)
        CHECK(self.rotation.control[index].word ==
              get32(self.ram, UINT32_C(0x800f9fd8) + index * 4u));
      const uint32_t raw_rt33 = get32(self.ram, UINT32_C(0x800f9fe8));
      const uint32_t expected_rt33 =
          (raw_rt33 & UINT32_C(0x8000))
              ? (raw_rt33 & UINT32_C(0xffff)) | UINT32_C(0xffff0000)
              : raw_rt33 & UINT32_C(0xffff);
      CHECK(self.rotation.control[4].word == expected_rt33);
      for (unsigned index = 5u; index != 32u; ++index)
        CHECK(self.rotation.control[index].known_mask == 0u);
    }
    return NBA97_TEXT_COMPLETE;
  }

  static int dispatch(void *opaque, const Nba97GameTextMemory *memory,
                      const Nba97GameCameraFrameTransformEvent *event,
                      Nba97GameCameraFrameTransformMachine *machine) {
    Natural &self = *static_cast<Natural *>(opaque);
    return nba97_game_gte_rotation_install_from_camera(
        &self.rotation, memory, event, machine);
  }
};

Nba97GameCameraFrameTransformEvent rotation_event() {
  Nba97GameCameraFrameTransformEvent event{};
  event.pc = UINT32_C(0x80051204);
  event.delay_slot_pc = UINT32_C(0x80051208);
  event.entry = UINT32_C(0x80055f18);
  event.kind = NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_ROTATION_80055F18;
  event.argument_count = 1u;
  return event;
}

void test_natural_camera_owner() {
  Natural natural;
  CHECK(nba97_game_camera_frame_transform(&natural.camera,
                                           &natural.camera_progress) ==
        NBA97_TEXT_COMPLETE);
  CHECK(natural.camera_progress.completed == 1u);
  CHECK(natural.matrix.invocations == 1u);
  CHECK(natural.matrix.result == NBA97_TEXT_COMPLETE);
  CHECK(natural.matrix.progress.completed == 1u);
  CHECK(natural.rotation.invocations == 1u);
  CHECK(natural.rotation.result == NBA97_TEXT_COMPLETE);
  CHECK(natural.rotation.progress.completed == 1u);
  CHECK(natural.translation_observed);
  CHECK(natural.fallback_calls == 3u);
  CHECK(natural.rotation.progress.machine.registers
            .gpr[NBA97_MATCH_INITIALIZE_A0]
            .word == UINT32_C(0x800f9fd8));
  CHECK(natural.rotation.progress.machine.registers
            .gpr[NBA97_MATCH_INITIALIZE_RA]
            .word == UINT32_C(0x8005120c));
}

void test_adapter_prefix_and_metadata() {
  Natural natural;
  Nba97GameCameraFrameTransformEvent event = rotation_event();
  Nba97GameCameraFrameTransformMachine machine = natural.camera.machine;
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           UINT32_C(0x800f9fd8));
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
           UINT32_C(0x8005120c));
  natural.rotation.operation_budget = 7u;
  CHECK(nba97_game_gte_rotation_install_from_camera(
            &natural.rotation, &natural.memory, &event, &machine) ==
        NBA97_TEXT_LIMIT);
  CHECK(natural.rotation.progress.reads == 5u);
  CHECK(natural.rotation.progress.control_writes == 2u);
  CHECK(machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 4u].word ==
        UINT32_C(0xabcd8001));
  CHECK(natural.rotation.control[0].word == UINT32_C(0x11223344));
  CHECK(natural.rotation.control[1].word == UINT32_C(0x55667788));

  const uint32_t original_pc = event.pc;
  const uint32_t original_delay = event.delay_slot_pc;
  const uint32_t original_entry = event.entry;
  const uint8_t original_kind = event.kind;
  const uint8_t original_count = event.argument_count;
  for (unsigned case_index = 0u; case_index != 6u; ++case_index) {
    Natural rejected;
    Nba97GameCameraFrameTransformEvent malformed = rotation_event();
    Nba97GameCameraFrameTransformMachine incoming = rejected.camera.machine;
    set_word(incoming.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
             UINT32_C(0x800f9fd8));
    set_word(incoming.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x8005120c));
    if (case_index == 0u)
      malformed.pc = original_pc + 4u;
    else if (case_index == 1u)
      malformed.delay_slot_pc = original_delay + 4u;
    else if (case_index == 2u)
      malformed.entry = original_entry + 4u;
    else if (case_index == 3u)
      malformed.kind = original_kind - 1u;
    else if (case_index == 4u)
      malformed.argument_count = original_count + 1u;
    else
      incoming.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7u;
    Nba97GameCameraFrameTransformMachine snapshot = incoming;
    CHECK(nba97_game_gte_rotation_install_from_camera(
              &rejected.rotation, &rejected.memory, &malformed, &incoming) ==
          NBA97_TEXT_ARGUMENT);
    CHECK(std::memcmp(&incoming, &snapshot, sizeof(incoming)) == 0);
    CHECK(rejected.rotation.invocations == 0u);
  }

  Natural unknown;
  Nba97GameCameraFrameTransformMachine partial = unknown.camera.machine;
  set_word(partial.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           UINT32_C(0x800f9fd8), 14u);
  set_word(partial.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
           UINT32_C(0x8005120c));
  CHECK(nba97_game_gte_rotation_install_from_camera(
            &unknown.rotation, &unknown.memory, &event, &partial) ==
        NBA97_TEXT_UNKNOWN);
  CHECK(partial.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask == 14u);
}

void test_fallback() {
  Natural natural;
  Nba97GameCameraFrameTransformEvent event{};
  event.pc = UINT32_C(0x80051168);
  event.delay_slot_pc = UINT32_C(0x8005116c);
  event.entry = UINT32_C(0x80056080);
  event.kind = NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATRIX_80056080;
  event.argument_count = 2u;
  Nba97GameCameraFrameTransformMachine machine = natural.camera.machine;
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
           UINT32_C(0x800f9fd8));
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0], UINT32_C(0x800fa638));
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA], UINT32_C(0x80051170));
  CHECK(nba97_game_gte_rotation_install_from_camera(
            &natural.rotation, &natural.memory, &event, &machine) ==
        NBA97_TEXT_COMPLETE);
  CHECK(natural.fallback_calls == 1u);
  CHECK(natural.rotation.invocations == 0u);
}

} // namespace

int main() {
  test_natural_camera_owner();
  test_adapter_prefix_and_metadata();
  test_fallback();
  std::printf("game_gte_rotation_install_integration_tests: %zu checks passed\n",
              checks);
  return 0;
}
