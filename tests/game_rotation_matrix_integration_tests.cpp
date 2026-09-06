#include "game_rotation_matrix_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "rotation matrix integration check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Stack = 0x801ff000u;
constexpr std::uint32_t Table = 0x800b3254u;
constexpr std::uint32_t Matrix = 0x800f9fd8u;

struct Composition {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000u, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000u, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97GameCameraFrameTransformContext parent{};
  Nba97GameCameraFrameTransformProgress parent_progress{};
  Nba97GameRotationMatrixBinding matrix{};
  Nba97GameCameraFrameTransformEvent matrix_event{};
  Nba97GameCameraFrameTransformMachine matrix_machine{};
  unsigned parent_calls{};

  Composition() {
    parent.memory = {&region, 1};
    parent.operation_budget = 1000;
    parent.io = child;
    parent.user = this;
    for (unsigned reg = 0; reg < 32; ++reg)
      parent.machine.registers.gpr[reg] = {0x10000000u + reg, 15};
    parent.machine.registers.gpr[0] = {0, 15};
    parent.machine.registers.gpr[29] = {Stack, 15};
    parent.machine.registers.gpr[31] = {0x81234568u, 15};
    parent.machine.hi = {0x01020304u, 15};
    parent.machine.lo = {0x05060708u, 15};
    put(0x800eb678u, 1, 4);
    put(0x800fa638u, 1, 2);
    put(0x800fa63au, 1, 2);
    put(0x800fa63cu, 1, 2);
    put(0x800fa630u, 0xffffu, 2);
    put(0x800fa632u, 0x8000u, 2);
    put(0x800fa634u, 0x7fffu, 2);
    put(0x800fb858u, 0, 2);
    put(0x800fb85au, 0, 2);
    put(0x800fb85cu, 0, 2);
    put(0x800f9fd8u, 1, 2);
    put(0x800f9fdau, 0xffffu, 2);
    put(0x800f9fdcu, 0x7fffu, 2);
    put(0x800fc61cu, 1, 4);
    put(0x800fc620u, 0x80000000u, 4);
    put(0x800fc624u, 0xffffffffu, 4);
    for (unsigned i = 0; i < 4096; ++i)
      put(Table + i * 4u, 0x10001000u, 4);
    nba97_game_rotation_matrix_binding_init(&matrix, 1000, nullptr, 0);
  }
  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }
  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    const auto at = offset(address);
    for (unsigned i = 0; i < width; ++i) {
      bytes[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
      known[at + i] = 1;
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0;
    const auto at = offset(address);
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[at + i]) << (8u * i);
    return value;
  }
  static int child(void *opaque, const Nba97GameTextMemory *memory,
                   const Nba97GameCameraFrameTransformEvent *event,
                   Nba97GameCameraFrameTransformMachine *machine) {
    auto &c = *static_cast<Composition *>(opaque);
    ++c.parent_calls;
    if (event->kind == NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATRIX_80056080) {
      c.matrix_event = *event;
      c.matrix_machine = *machine;
      return nba97_game_rotation_matrix_from_camera_frame_transform(
          &c.matrix, memory, event, machine);
    }
    return 1;
  }
  int run() {
    return nba97_game_camera_frame_transform(&parent, &parent_progress);
  }
};

void actual_aq_parent_composition() {
  Composition c;
  check(c.run() == NBA97_TEXT_COMPLETE && c.parent_progress.completed);
  check(c.matrix.invocations == 1 && c.matrix.result == NBA97_TEXT_COMPLETE &&
        c.matrix.progress.completed && c.parent_calls == 4);
  check(c.matrix_event.pc == 0x80051168u &&
        c.matrix_event.delay_slot_pc == 0x8005116cu &&
        c.matrix_event.entry == 0x80056080u &&
        c.matrix_event.kind ==
            NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATRIX_80056080 &&
        c.matrix_event.argument_count == 2);
  check(c.matrix_machine.registers.gpr[31].word == 0x80051170u &&
        c.matrix_machine.registers.gpr[4].word == Stack - 0x30u + 0x10u &&
        c.matrix_machine.registers.gpr[5].word == Matrix);
  check(c.matrix.progress.returned_value.word == Matrix &&
        c.matrix.progress.stores == 9 && c.get(Matrix + 4, 2) != 1);
}

void nested_prefix_and_adapter_guards() {
  Composition limited;
  limited.matrix.operation_budget = 0;
  check(limited.run() == NBA97_TEXT_IO_REFUSED &&
        limited.parent_progress.stopped_pc == 0x80051168u &&
        limited.matrix.result == NBA97_TEXT_LIMIT &&
        limited.matrix.progress.stopped_pc == 0x80056080u);

  Composition guard;
  Nba97GameCameraFrameTransformEvent event{};
  event.pc = 0x80051168u;
  event.delay_slot_pc = 0x8005116cu;
  event.entry = 0x80056080u;
  event.kind = NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATRIX_80056080;
  event.argument_count = 2;
  auto machine = guard.parent.machine;
  machine.registers.gpr[31] = {0x80051170u, 15};
  const auto before = machine;
  event.pc ^= 4u;
  check(!nba97_game_rotation_matrix_from_camera_frame_transform(
            &guard.matrix, &guard.parent.memory, &event, &machine) &&
        std::memcmp(&machine, &before, sizeof machine) == 0);
  event.pc ^= 4u;
  event.argument_count = 1;
  check(!nba97_game_rotation_matrix_from_camera_frame_transform(
      &guard.matrix, &guard.parent.memory, &event, &machine));
  event.argument_count = 2;
  machine.registers.gpr[31].known_mask = 7;
  check(!nba97_game_rotation_matrix_from_camera_frame_transform(
      &guard.matrix, &guard.parent.memory, &event, &machine));
  check(!nba97_game_rotation_matrix_from_camera_frame_transform(
      nullptr, &guard.parent.memory, &event, &machine));
}
} // namespace

int main() {
  actual_aq_parent_composition();
  nested_prefix_and_adapter_guards();
  std::printf("game rotation matrix integration: %u checks\n", checks);
}
