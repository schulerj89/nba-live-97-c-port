#include "game_rotation_matrix_adapter.h"
#include "game_rotation_matrix_capture.h"
#include <sstream>
#include <stdexcept>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
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
  std::uint32_t matrix_return[9]{};

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
      const int accepted=nba97_game_rotation_matrix_from_camera_frame_transform(
          &c.matrix, memory, event, machine);
      for(unsigned i=0;i<9;++i)c.matrix_return[i]=c.get(Matrix+2*i,2);
      return accepted;
    }
    return 1;
  }
  int run() {
    return nba97_game_camera_frame_transform(&parent, &parent_progress);
  }
};

} // namespace
namespace nba97 {
std::string captureGameRotationMatrix() {
  Composition c;
  std::uint32_t before[9];
  for(unsigned i=0;i<9;++i)before[i]=c.get(Matrix+2*i,2);
  if(c.run()!=NBA97_TEXT_COMPLETE || !c.parent_progress.completed || !c.matrix.progress.completed || c.matrix.invocations!=1)
    throw std::runtime_error("rotation matrix native caller failed");
  const auto& q=c.matrix.progress;
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x80056080\",\"inclusive_end\":\"0x800562CB\","
       "\"bytes\":588,\"instructions\":147,\"classification\":\"no direct visual effect\","
       "\"scope\":\"actual camera-frame-transform caller with synthetic packed table and typed GTE services\","
       "\"completed\":true,\"parent_completed\":true,\"operations\":" << q.operations
    << ",\"reads\":" << q.reads << ",\"stores\":" << q.stores << ",\"multiplies\":" << q.multiply_count
    << ",\"angles\":[" << q.angle_x.word << ',' << q.angle_y.word << ',' << q.angle_z.word
    << "],\"matrix_before\":[";
  for(unsigned i=0;i<9;++i){if(i)o<<',';o<<before[i];}
  o << "],\"matrix_after\":[";
  for(unsigned i=0;i<9;++i){if(i)o<<',';o<<c.get(Matrix+2*i,2);}
  o << "],\"matrix_return\":[";
  for(unsigned i=0;i<9;++i){if(i)o<<',';o<<c.matrix_return[i];}
  o << "],\"entry_pc\":" << c.matrix_event.pc << ",\"returned_value\":" << q.returned_value.word
    << ",\"returned_sp\":" << q.machine.registers.gpr[29].word << ",\"return_address\":" << q.machine.registers.gpr[31].word
    << ",\"hi\":" << q.machine.hi.word << ",\"lo\":" << q.machine.lo.word << "}";
  return o.str();
}
}
