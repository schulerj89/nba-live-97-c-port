#include "game_gte_reference_transform_adapter.h"
#include "game_player_geometry.hpp"
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
  std::exit(1);
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

void test_actual_camera_owner_chain() {
  Natural natural;
  CHECK(nba97_game_camera_frame_transform(
            &natural.camera, &natural.camera_progress) == NBA97_TEXT_COMPLETE);
  CHECK(natural.camera_progress.completed == 1u);
  CHECK(natural.matrix_calls == 1u && natural.rotation_calls == 1u &&
        natural.translation_calls == 1u);
  CHECK(natural.reference.invocations == 1u);
  CHECK(natural.reference.result == NBA97_TEXT_COMPLETE);
  CHECK(natural.reference.progress.completed == 1u);
  CHECK(natural.hardware.invocations == 1u &&
        natural.hardware.result == NBA97_TEXT_COMPLETE);
  CHECK(natural.get32(UINT32_C(0x800fc61c)) == 1u);
  CHECK(natural.get32(UINT32_C(0x800fc620)) == 2u);
  CHECK(natural.get32(UINT32_C(0x800fc624)) == 3u);
  CHECK(natural.get32(kStack - 0x30u + 0x18u) == 0u);
  CHECK(natural.get32(UINT32_C(0x800f9fec)) == 5u);
  CHECK(natural.get32(UINT32_C(0x800f9ff0)) == 7u);
  CHECK(natural.get32(UINT32_C(0x800f9ff4)) == 9u);
}

void test_camera_adapter_metadata_and_prefix() {
  Natural natural;
  Nba97GameCameraFrameTransformEvent event{};
  event.pc = UINT32_C(0x80051228);
  event.delay_slot_pc = UINT32_C(0x8005122c);
  event.entry = UINT32_C(0x80056650);
  event.kind = NBA97_GAME_CAMERA_FRAME_TRANSFORM_REFERENCE_80056650;
  event.argument_count = 3u;
  Nba97GameCameraFrameTransformMachine machine = natural.camera.machine;
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           UINT32_C(0x800fab98));
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
           UINT32_C(0x800fc61c));
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A2], kStack + 0x18u);
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
           UINT32_C(0x80051230));
  const auto original = machine;

  event.delay_slot_pc += 4u;
  CHECK(nba97_game_gte_reference_transform_from_camera_frame(
            &natural.reference, &natural.camera.memory, &event, &machine) == 0);
  CHECK(std::memcmp(&machine, &original, sizeof(machine)) == 0);
  CHECK(natural.reference.result == NBA97_TEXT_ARGUMENT);
  event.delay_slot_pc -= 4u;

  machine = original;
  event.kind = NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATRIX_80056080;
  CHECK(nba97_game_gte_reference_transform_from_camera_frame(
            &natural.reference, &natural.camera.memory, &event, &machine) == 0);
  CHECK(natural.reference.result == NBA97_TEXT_ARGUMENT);
  CHECK(natural.fallback_calls == 0u);
  CHECK(std::memcmp(&machine, &original, sizeof(machine)) == 0);
  event.kind = NBA97_GAME_CAMERA_FRAME_TRANSFORM_REFERENCE_80056650;

  machine = original;
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7u;
  CHECK(nba97_game_gte_reference_transform_from_camera_frame(
            &natural.reference, &natural.camera.memory, &event, &machine) == 0);
  CHECK(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 7u);

  machine = original;
  natural.reference.state.data[18].known_mask = 16u;
  const auto invalid_state = natural.reference.state;
  CHECK(nba97_game_gte_reference_transform_from_camera_frame(
            &natural.reference, &natural.camera.memory, &event, &machine) == 0);
  CHECK(natural.reference.result == NBA97_TEXT_ARGUMENT);
  CHECK(std::memcmp(&machine, &original, sizeof(machine)) == 0);
  CHECK(std::memcmp(&natural.reference.state, &invalid_state,
                    sizeof(invalid_state)) == 0);
  natural.reference.state.data[18].known_mask = 15u;

  machine = original;
  natural.reference.operation_budget = 6u;
  CHECK(nba97_game_gte_reference_transform_from_camera_frame(
            &natural.reference, &natural.camera.memory, &event, &machine) == 0);
  CHECK(natural.reference.result == NBA97_TEXT_LIMIT);
  CHECK(natural.reference.progress.stores == 3u);
  CHECK(natural.get32(UINT32_C(0x800fc61c)) ==
        natural.reference.progress.state.data[25].word);
  CHECK(natural.get32(UINT32_C(0x800fc620)) ==
        natural.reference.progress.state.data[26].word);
  CHECK(natural.get32(UINT32_C(0x800fc624)) ==
        natural.reference.progress.state.data[27].word);
  CHECK(natural.get32(kStack + 0x18u) == 0u);
}


void test_composed_camera_geometry() {
  Composed c;
  CHECK(nba97_game_camera_frame_transform(&c.camera,&c.camera_progress)==NBA97_TEXT_COMPLETE);
  CHECK(c.matrix.progress.completed && c.rotation.progress.completed && c.translation.progress.completed && c.reference.progress.completed);
  CHECK(c.matrix.invocations==1 && c.rotation.invocations==1 && c.translation.invocations==1 && c.reference.invocations==1);
  const uint32_t controls[8]={0xe6671999u,0x20001999u,0xf0000000u,0x20000000u,0x1000u,0,0,0};
  for(unsigned i=0;i<8;++i) {CHECK(c.reference.state.control[i].word==controls[i]);CHECK(c.reference.state.control[i].known_mask==15);}
  const uint32_t mac[3]={3u,0xffffffffu,7u},tail[3]={7u,4u,13u};
  for(unsigned i=0;i<3;++i) {
    CHECK(c.reference.state.data[25+i].word==mac[i]);CHECK(c.reference.state.data[9+i].word==mac[i]);
    CHECK(c.get32(0x800fc61cu+4u*i)==mac[i]);CHECK(c.get32(0x800f9fecu+4u*i)==tail[i]);
  }
  CHECK(c.reference.state.control[31].word==0);
  CHECK(c.reference.progress.machine.registers.gpr[29].word==0x801fefd0u);
  CHECK(c.reference.progress.machine.registers.gpr[31].word==0x80051230u);
  for(unsigned i=8;i<31;++i) CHECK(c.reference.state.control[i].word==0x40000000u);
  for(unsigned i=2;i<32;++i) if(i<9 || (i>11 && i<25) || i>27) CHECK(c.reference.state.data[i].word==0x50000000u);
  Composed prefix;prefix.reference.operation_budget=6;
  CHECK(nba97_game_camera_frame_transform(&prefix.camera,&prefix.camera_progress)==NBA97_TEXT_IO_REFUSED);
  CHECK(prefix.reference.result==NBA97_TEXT_LIMIT && prefix.reference.progress.stores==3);
  CHECK(prefix.reference.progress.machine.registers.gpr[2].word==0);
  CHECK(prefix.get32(0x800f9fecu)==0);
  CHECK(prefix.rotation.progress.completed && prefix.translation.progress.completed);
}

} // namespace

int main() {
  test_actual_camera_owner_chain();
  test_composed_camera_geometry();
  test_camera_adapter_metadata_and_prefix();
  std::printf(
      "game_gte_reference_transform_integration_tests: %u checks passed\n",
      checks);
  return 0;
}
