#include "game_gte_translation_install_adapter.h"
#include "game_gte_translation_install_capture.h"
#include <sstream>
#include <stdexcept>
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
  throw std::runtime_error("GTE translation capture check failed");
}
#define CHECK(expression) check((expression), #expression, __LINE__)

constexpr uint32_t kBase = UINT32_C(0x80000000);
constexpr uint32_t kStack = UINT32_C(0x801ff000);
constexpr uint32_t kTransform = UINT32_C(0x800f9fd8);
constexpr uint32_t kTranslation = kTransform + UINT32_C(0x14);

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
void set_word(Nba97GameGteTranslationInstallWord &word, uint32_t value,
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
  Nba97GameGteTranslationInstallWord initial_control[32]{};
  Nba97GameGteTranslationInstallCameraBinding translation{};
  size_t fallback_calls = 0u;
  bool reference_observed = false;
  uint8_t refuse_kind = 0u;

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
    put32(ram, kTransform + 0u, UINT32_C(0x11223344));
    put32(ram, kTransform + 4u, UINT32_C(0x55667788));
    put32(ram, kTransform + 8u, UINT32_C(0x99aabbcc));
    put32(ram, kTransform + 12u, UINT32_C(0xddeeff00));
    put32(ram, kTransform + 16u, UINT32_C(0xabcd8001));
    put32(ram, UINT32_C(0x800fc61c), 100u);
    put32(ram, UINT32_C(0x800fc620), 200u);
    put32(ram, UINT32_C(0x800fc624), 300u);
    for (unsigned index = 0u; index != 32u; ++index)
      set_word(initial_control[index], UINT32_C(0xa5000000) + index,
               static_cast<uint8_t>(index % 16u));
    camera.memory = memory;
    camera.operation_budget = 1000u;
    camera.io = dispatch;
    camera.user = this;
    nba97_game_gte_translation_install_camera_binding_init(
        &translation, initial_control, 6u, nullptr, 0u, nullptr, 0u, fallback,
        this);
  }

  static int fallback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameCameraFrameTransformEvent *event,
                      Nba97GameCameraFrameTransformMachine *) {
    Natural &self = *static_cast<Natural *>(opaque);
    ++self.fallback_calls;
    if (self.refuse_kind == event->kind)
      return NBA97_TEXT_IO_REFUSED;
    if (event->pc == UINT32_C(0x80051168)) {
      put32(self.ram, kTransform + 0u, UINT32_C(0x11223344));
      put32(self.ram, kTransform + 4u, UINT32_C(0x55667788));
      put32(self.ram, kTransform + 8u, UINT32_C(0x99aabbcc));
      put32(self.ram, kTransform + 12u, UINT32_C(0xddeeff00));
      put32(self.ram, kTransform + 16u, UINT32_C(0xabcd8001));
    }
    if (event->pc == UINT32_C(0x80051204)) {
      put32(self.ram, kTranslation + 0u, UINT32_C(0x80000001));
      put32(self.ram, kTranslation + 4u, UINT32_C(0x7ffffffe));
      put32(self.ram, kTranslation + 8u, UINT32_C(0xffff8000));
    }
    if (event->pc == UINT32_C(0x80051228)) {
      self.reference_observed = true;
      CHECK(self.translation.control[5].word == UINT32_C(0x80000001));
      CHECK(self.translation.control[6].word == UINT32_C(0x7ffffffe));
      CHECK(self.translation.control[7].word == UINT32_C(0xffff8000));
      CHECK(self.translation.control[5].known_mask == 15u);
      CHECK(self.translation.control[6].known_mask == 15u);
      CHECK(self.translation.control[7].known_mask == 15u);
    }
    return NBA97_TEXT_COMPLETE;
  }

  static int dispatch(void *opaque, const Nba97GameTextMemory *memory,
                      const Nba97GameCameraFrameTransformEvent *event,
                      Nba97GameCameraFrameTransformMachine *machine) {
    Natural &self = *static_cast<Natural *>(opaque);
    return nba97_game_gte_translation_install_from_camera(
        &self.translation, memory, event, machine);
  }
};


// The production matrix builder and both installers share this retained bank.
struct Composed : Natural {
  Nba97GameRotationMatrixBinding matrix{};
  Nba97GameGteRotationInstallCameraBinding rotation{};

  Composed() {
    for(unsigned i=0;i<4096;++i)
      put32(ram, UINT32_C(0x800b3254)+4u*i, UINT32_C(0x10001000));
    nba97_game_rotation_matrix_binding_init(&matrix,1000u,nullptr,0u);
    nba97_game_gte_rotation_install_camera_binding_init(
      &rotation,initial_control,10u,nullptr,0u,nullptr,0u,nullptr,nullptr);
    translation.fallback=composedFallback;
    translation.fallback_user=this;
  }
  static int composedFallback(void* opaque,const Nba97GameTextMemory* memory,
      const Nba97GameCameraFrameTransformEvent* event,Nba97GameCameraFrameTransformMachine* machine) {
    auto& self=*static_cast<Composed*>(opaque);
    ++self.fallback_calls;
    if(event->pc==UINT32_C(0x80051168))
      return nba97_game_rotation_matrix_from_camera_frame_transform(&self.matrix,memory,event,machine);
    if(event->pc==UINT32_C(0x80051204)) {
      std::memcpy(self.rotation.control,self.translation.control,sizeof(self.rotation.control));
      const int result=nba97_game_gte_rotation_install_from_camera(&self.rotation,memory,event,machine);
      std::memcpy(self.translation.control,self.rotation.control,sizeof(self.translation.control));
      return result;
    }
    if(event->pc==UINT32_C(0x80051228)) {
      self.reference_observed=true;
      // AQ clears all translation words before either installer.
      for(unsigned i=5;i<8;++i) {
        CHECK(self.translation.control[i].word==0);
        CHECK(self.translation.control[i].known_mask==15);
      }
      return NBA97_TEXT_COMPLETE; // Explicit synthetic reference-service contract.
    }
    return NBA97_TEXT_IO_REFUSED;
  }
};


} // namespace
namespace nba97 {
std::string captureGameGteTranslationInstall() {
  Composed c;
  if(nba97_game_camera_frame_transform(&c.camera,&c.camera_progress)!=NBA97_TEXT_COMPLETE ||
     !c.matrix.progress.completed || !c.rotation.progress.completed || !c.translation.progress.completed ||
     !c.reference_observed) throw std::runtime_error("GTE translation native composition failed");
  const auto& q=c.translation.progress;
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x80055F44\",\"inclusive_end\":\"0x80055F5F\","
       "\"bytes\":28,\"instructions\":7,\"classification\":\"no direct visual effect\","
       "\"scope\":\"actual camera caller, matrix builder and both GTE installers; synthetic packed table; typed reference service\","
       "\"completed\":true,\"parent_completed\":true,\"matrix_completed\":true,\"rotation_completed\":true,"
       "\"operations\":" << q.operations << ",\"reads\":" << q.reads << ",\"control_writes\":" << q.control_writes
    << ",\"controls_before\":[";
  for(unsigned i=5;i<8;++i){if(i>5)o<<',';o<<c.initial_control[i].word;}
  o << "],\"controls_before_masks\":[5,6,7],\"controls_after\":[";
  for(unsigned i=0;i<8;++i){if(i)o<<',';o<<q.control[i].word;}
  o << "],\"controls_after_masks\":[";
  for(unsigned i=0;i<8;++i){if(i)o<<',';o<<unsigned(q.control[i].known_mask);}
  o << "],\"raw_loads\":[";
  for(unsigned i=0;i<3;++i){if(i)o<<',';o<<q.machine.registers.gpr[8+i].word;}
  bool unchanged=true;
  for(unsigned i=8;i<32;++i)unchanged=unchanged&&q.control[i].word==c.initial_control[i].word&&q.control[i].known_mask==c.initial_control[i].known_mask;
  o << "],\"untouched_controls_preserved\":" << (unchanged?"true":"false")
    << ",\"returned_sp\":" << q.machine.registers.gpr[29].word
    << ",\"return_address\":" << q.machine.registers.gpr[31].word << "}";
  return o.str();
}
}
