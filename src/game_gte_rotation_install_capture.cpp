#include "game_gte_rotation_install_adapter.h"
#include "game_gte_rotation_install_capture.h"
#include <sstream>
#include <stdexcept>
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
  throw std::runtime_error("GTE rotation capture check failed");
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

} // namespace
namespace nba97 {
std::string captureGameGteRotationInstall() {
  Natural n;
  if (nba97_game_camera_frame_transform(&n.camera, &n.camera_progress) != NBA97_TEXT_COMPLETE ||
      !n.rotation.progress.completed || !n.matrix.progress.completed || !n.translation_observed)
    throw std::runtime_error("GTE rotation native composition failed");
  const auto& q=n.rotation.progress;
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x80055F18\",\"inclusive_end\":\"0x80055F43\","
       "\"bytes\":44,\"instructions\":11,\"classification\":\"no direct visual effect\","
       "\"scope\":\"actual camera caller and recovered matrix builder; synthetic packed table; typed translation/reference services\","
       "\"completed\":true,\"parent_completed\":true,\"matrix_completed\":true,\"operations\":" << q.operations
    << ",\"reads\":" << q.reads << ",\"control_writes\":" << q.control_writes
    << ",\"controls_before\":[0,0,0,0,0],\"controls_before_masks\":[0,0,0,0,0],\"controls_after\":[";
  for(unsigned i=0;i<5;++i){if(i)o<<',';o<<q.control[i].word;}
  o << "],\"controls_after_masks\":[";
  for(unsigned i=0;i<5;++i){if(i)o<<',';o<<unsigned(q.control[i].known_mask);}
  o << "],\"raw_loads\":[";
  for(unsigned i=0;i<5;++i){if(i)o<<',';o<<q.machine.registers.gpr[8+i].word;}
  o << "],\"untouched_controls_unknown\":";
  bool unknown=true;for(unsigned i=5;i<32;++i)unknown=unknown&&q.control[i].known_mask==0;
  o << (unknown?"true":"false") << ",\"returned_sp\":" << q.machine.registers.gpr[29].word
    << ",\"return_address\":" << q.machine.registers.gpr[31].word << "}";
  return o.str();
}
}
