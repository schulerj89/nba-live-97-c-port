#include "game_gte_reference_transform_adapter.h"
#include "game_player_geometry.hpp"
#include "game_gte_reference_transform_capture.h"
#include <sstream>
#include <stdexcept>
#include "game_rotation_matrix_adapter.h"
#include "game_gte_rotation_install_adapter.h"
#include "game_gte_translation_install_adapter.h"

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
  throw std::runtime_error("GTE reference capture check failed");
}
#define CHECK(expression) check((expression), #expression, __LINE__)

constexpr uint32_t kBase = UINT32_C(0x80000000);
constexpr uint32_t kStack = UINT32_C(0x801ff000);

void set_word(Nba97GameCameraFrameTransformWord &word, uint32_t value,
              uint8_t mask = 15u) {
  word.word = value;
  word.known_mask = mask;
}

struct Natural {
  std::vector<uint8_t> ram = std::vector<uint8_t>(0x200000u, 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(0x200000u, 1u);
  Nba97GameTextRegion region{};
  Nba97GameCameraFrameTransformContext camera{};
  Nba97GameCameraFrameTransformProgress camera_progress{};
  nba97::GamePlayerGeometry geometry{};
  Nba97GameGteReferenceTransformGeometryBinding hardware{};
  Nba97GameGteReferenceTransformCameraBinding reference{};
  std::array<Nba97GameGteReferenceTransformAccess, 8> journal{};
  size_t matrix_calls = 0u;
  size_t rotation_calls = 0u;
  size_t translation_calls = 0u;
  size_t fallback_calls = 0u;

  Natural();
  size_t offset(uint32_t address) const { return address - kBase; }
  void put16(uint32_t address, uint16_t value) {
    ram[offset(address)] = static_cast<uint8_t>(value);
    ram[offset(address) + 1u] = static_cast<uint8_t>(value >> 8u);
  }
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
  Nba97GameGteReferenceTransformWord load(uint32_t address) const {
    Nba97GameGteReferenceTransformWord value{};
    value.word = get32(address);
    for (unsigned byte = 0u; byte != 4u; ++byte)
      if (known[offset(address) + byte] != 0u)
        value.known_mask =
            static_cast<uint8_t>(value.known_mask | (1u << byte));
    return value;
  }
};

int accepting_fallback(void *opaque, const Nba97GameTextMemory *,
                       const Nba97GameCameraFrameTransformEvent *,
                       Nba97GameCameraFrameTransformMachine *) {
  Natural &natural = *static_cast<Natural *>(opaque);
  ++natural.fallback_calls;
  return 1;
}

int children(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97GameCameraFrameTransformEvent *event,
             Nba97GameCameraFrameTransformMachine *machine) {
  Natural &natural = *static_cast<Natural *>(opaque);
  CHECK(memory == &natural.camera.memory);
  CHECK(machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 15u);
  CHECK(machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        event->pc + 8u);

  if (event->kind == NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATRIX_80056080) {
    ++natural.matrix_calls;
    const uint32_t matrix =
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1].word;
    natural.put32(matrix + 0u, 2560u);
    natural.put32(matrix + 4u, 0u);
    natural.put32(matrix + 8u, 4096u);
    natural.put32(matrix + 12u, 0u);
    natural.put32(matrix + 16u, 4096u);
    return 1;
  }
  if (event->kind == NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_ROTATION_80055F18) {
    ++natural.rotation_calls;
    const uint32_t matrix =
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word;
    for (unsigned index = 0u; index != 5u; ++index)
      natural.reference.state.control[index] =
          natural.load(matrix + index * 4u);
    const uint32_t half = natural.reference.state.control[4].word & 0xffffu;
    natural.reference.state.control[4].word =
        (half & 0x8000u) != 0u ? half | UINT32_C(0xffff0000) : half;
    natural.reference.state.control[4].known_mask = 15u;
    return 1;
  }
  if (event->kind ==
      NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_TRANSLATION_80055F44) {
    ++natural.translation_calls;
    const uint32_t matrix =
        machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word;
    for (unsigned index = 0u; index != 3u; ++index)
      natural.reference.state.control[index + 5u] =
          natural.load(matrix + 20u + index * 4u);
    return 1;
  }
  if (event->kind == NBA97_GAME_CAMERA_FRAME_TRANSFORM_REFERENCE_80056650)
    return nba97_game_gte_reference_transform_from_camera_frame(
        &natural.reference, memory, event, machine);
  return 0;
}

Natural::Natural() {
  region = {kBase, ram.data(), known.data(), ram.size()};
  camera.memory = {&region, 1u};
  camera.operation_budget = 100u;
  camera.io = children;
  camera.user = this;
  for (unsigned index = 0u; index != 32u; ++index) {
    set_word(camera.machine.registers.gpr[index], UINT32_C(0x30000000) + index);
  }
  set_word(camera.machine.registers.gpr[0], 0u);
  set_word(camera.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP], kStack);
  set_word(camera.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
           UINT32_C(0x81234567));
  set_word(camera.machine.hi, UINT32_C(0x11112222));
  set_word(camera.machine.lo, UINT32_C(0x33334444));

  put32(UINT32_C(0x800eb678), 1u);
  put16(UINT32_C(0x800fa638), 1u);
  put16(UINT32_C(0x800fa63a), 2u);
  put16(UINT32_C(0x800fa63c), 3u);
  put16(UINT32_C(0x800fa630), 4u);
  put16(UINT32_C(0x800fa632), 5u);
  put16(UINT32_C(0x800fa634), 6u);
  put16(UINT32_C(0x800fb858), 0u);
  put16(UINT32_C(0x800fb85a), 0u);
  put16(UINT32_C(0x800fb85c), 0u);
  put32(UINT32_C(0x800fab98), UINT32_C(0x00020001));
  put32(UINT32_C(0x800fab9c), UINT32_C(0xdead0003));

  Nba97GameGteReferenceTransformState state{};
  for (auto &word : state.control)
    set_word(word, UINT32_C(0x40000000));
  for (auto &word : state.data)
    set_word(word, UINT32_C(0x50000000));
  nba97_game_gte_reference_transform_geometry_binding_init(&hardware,
                                                           &geometry);
  nba97_game_gte_reference_transform_camera_binding_init(
      &reference, &state, 7u,
      nba97_game_gte_reference_transform_geometry_hardware, &hardware,
      journal.data(), journal.size(), accepting_fallback, this);
}


// Share the full retained GTE bank across the existing production children.
struct Composed : Natural {
  Nba97GameRotationMatrixBinding matrix{};
  Nba97GameGteRotationInstallCameraBinding rotation{};
  Nba97GameGteTranslationInstallCameraBinding translation{};
  Composed() {
    for(unsigned i=0;i<4096;++i) put32(0x800b3254u+4u*i,0x10001000u);
    for(unsigned i=0;i<3;++i) put32(0x800fc61cu+4u*i,100u*(i+1u));
    nba97_game_rotation_matrix_binding_init(&matrix,1000u,nullptr,0u);
    nba97_game_gte_rotation_install_camera_binding_init(&rotation,reference.state.control,10u,nullptr,0u,nullptr,0u,nullptr,nullptr);
    nba97_game_gte_translation_install_camera_binding_init(&translation,reference.state.control,6u,nullptr,0u,nullptr,0u,nullptr,nullptr);
    camera.io=dispatch;camera.user=this;
  }
  static int dispatch(void* opaque,const Nba97GameTextMemory* memory,
      const Nba97GameCameraFrameTransformEvent* event,Nba97GameCameraFrameTransformMachine* machine) {
    auto& self=*static_cast<Composed*>(opaque);
    if(event->pc==0x80051168u) return nba97_game_rotation_matrix_from_camera_frame_transform(&self.matrix,memory,event,machine);
    if(event->pc==0x80051204u) {
      std::memcpy(self.rotation.control,self.reference.state.control,sizeof(self.rotation.control));
      const int result=nba97_game_gte_rotation_install_from_camera(&self.rotation,memory,event,machine);
      std::memcpy(self.reference.state.control,self.rotation.control,sizeof(self.rotation.control));return result;
    }
    if(event->pc==0x8005120cu) {
      std::memcpy(self.translation.control,self.reference.state.control,sizeof(self.translation.control));
      const int result=nba97_game_gte_translation_install_from_camera(&self.translation,memory,event,machine);
      std::memcpy(self.reference.state.control,self.translation.control,sizeof(self.translation.control));return result;
    }
    if(event->pc==0x80051228u) return nba97_game_gte_reference_transform_from_camera_frame(&self.reference,memory,event,machine);
    return 0;
  }
};


} // namespace
namespace nba97 {
std::string captureGameGteReferenceTransform() {
  Composed c;
  if(nba97_game_camera_frame_transform(&c.camera,&c.camera_progress)!=NBA97_TEXT_COMPLETE || !c.reference.progress.completed)
    throw std::runtime_error("GTE reference native composition failed");
  const auto& q=c.reference.progress;
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x80056650\",\"inclusive_end\":\"0x80056677\",\"bytes\":40,\"instructions\":10,"
    "\"classification\":\"no direct visual effect\",\"scope\":\"actual camera, matrix builder, rotation and translation installers, reference transform and production GTE hardware; synthetic packed table\","
    "\"completed\":true,\"parent_completed\":true,\"operations\":"<<q.operations<<",\"reads\":"<<q.reads<<",\"stores\":"<<q.stores<<",\"hardware_calls\":"<<q.hardware_completed
    <<",\"output_before\":[100,200,300],\"output_after\":[";
  for(unsigned i=0;i<3;++i){if(i)o<<',';o<<c.get32(0x800fc61cu+4u*i);}
  o<<"],\"mac\":[";for(unsigned i=0;i<3;++i){if(i)o<<',';o<<q.state.data[25+i].word;}
  o<<"],\"ir\":[";for(unsigned i=0;i<3;++i){if(i)o<<',';o<<q.state.data[9+i].word;}
  o<<"],\"camera_tail\":[";for(unsigned i=0;i<3;++i){if(i)o<<',';o<<c.get32(0x800f9fecu+4u*i);}
  o<<"],\"controls\":[";for(unsigned i=0;i<8;++i){if(i)o<<',';o<<q.state.control[i].word;}
  bool preserved=true;
  for(unsigned i=8;i<31;++i)preserved=preserved&&q.state.control[i].word==0x40000000u&&q.state.control[i].known_mask==15;
  for(unsigned i=2;i<32;++i)if(i<9||(i>11&&i<25)||i>27)preserved=preserved&&q.state.data[i].word==0x50000000u&&q.state.data[i].known_mask==15;
  o<<"],\"unrelated_gte_preserved\":"<<(preserved?"true":"false")<<",\"flag\":"<<q.state.control[31].word
   <<",\"returned_sp\":"<<q.machine.registers.gpr[29].word<<",\"return_address\":"<<q.machine.registers.gpr[31].word<<"}";
  return o.str();
}
}
