#include "game_gte_translation_install_adapter.h"
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

Nba97GameCameraFrameTransformEvent translation_event() {
  Nba97GameCameraFrameTransformEvent event{};
  event.pc = UINT32_C(0x8005120c);
  event.delay_slot_pc = UINT32_C(0x80051210);
  event.entry = UINT32_C(0x80055f44);
  event.kind = NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_TRANSLATION_80055F44;
  event.argument_count = 1u;
  return event;
}

void prepare_direct_machine(Nba97GameCameraFrameTransformMachine &machine) {
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0], kTransform);
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
           UINT32_C(0x80051214));
}

void test_natural_camera_owner() {
  Natural natural;
  CHECK(nba97_game_camera_frame_transform(
            &natural.camera, &natural.camera_progress) == NBA97_TEXT_COMPLETE);
  CHECK(natural.camera_progress.completed == 1u);
  CHECK(natural.translation.invocations == 1u);
  CHECK(natural.translation.result == NBA97_TEXT_COMPLETE);
  CHECK(natural.translation.progress.completed == 1u);
  CHECK(natural.reference_observed);
  CHECK(natural.fallback_calls == 3u);
  CHECK(natural.translation.progress.machine.registers
            .gpr[NBA97_MATCH_INITIALIZE_A0]
            .word == kTransform);
  CHECK(natural.translation.progress.machine.registers
            .gpr[NBA97_MATCH_INITIALIZE_RA]
            .word == UINT32_C(0x80051214));
  for (unsigned index = 0u; index != 32u; ++index)
    if (index < 5u || index > 7u) {
      CHECK(natural.translation.control[index].word ==
            natural.initial_control[index].word);
      CHECK(natural.translation.control[index].known_mask ==
            natural.initial_control[index].known_mask);
    }
}

void test_adapter_prefix_and_guards() {
  Natural natural;
  Nba97GameCameraFrameTransformEvent event = translation_event();
  Nba97GameCameraFrameTransformMachine machine = natural.camera.machine;
  prepare_direct_machine(machine);
  put32(natural.ram, kTranslation + 0u, UINT32_C(0x01020304));
  put32(natural.ram, kTranslation + 4u, UINT32_C(0x11121314));
  put32(natural.ram, kTranslation + 8u, UINT32_C(0x21222324));
  natural.translation.operation_budget = 4u;
  CHECK(nba97_game_gte_translation_install_from_camera(
            &natural.translation, &natural.memory, &event, &machine) ==
        NBA97_TEXT_LIMIT);
  CHECK(natural.translation.progress.reads == 3u);
  CHECK(natural.translation.progress.control_writes == 1u);
  CHECK(machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0].word ==
        UINT32_C(0x01020304));
  CHECK(machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 1u].word ==
        UINT32_C(0x11121314));
  CHECK(machine.registers.gpr[NBA97_MATCH_INITIALIZE_T0 + 2u].word ==
        UINT32_C(0x21222324));
  CHECK(natural.translation.control[5].word == UINT32_C(0x01020304));

  for (unsigned case_index = 0u; case_index != 7u; ++case_index) {
    Natural rejected;
    Nba97GameCameraFrameTransformEvent malformed = translation_event();
    Nba97GameCameraFrameTransformMachine incoming = rejected.camera.machine;
    prepare_direct_machine(incoming);
    if (case_index == 0u)
      malformed.pc += 4u;
    else if (case_index == 1u)
      malformed.delay_slot_pc += 4u;
    else if (case_index == 2u)
      malformed.entry += 4u;
    else if (case_index == 3u)
      malformed.kind = NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_ROTATION_80055F18;
    else if (case_index == 4u)
      malformed.argument_count = 2u;
    else if (case_index == 5u)
      incoming.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7u;
    else
      incoming.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word += 4u;
    const Nba97GameCameraFrameTransformMachine snapshot = incoming;
    CHECK(nba97_game_gte_translation_install_from_camera(
              &rejected.translation, &rejected.memory, &malformed, &incoming) ==
          NBA97_TEXT_ARGUMENT);
    CHECK(std::memcmp(&incoming, &snapshot, sizeof(incoming)) == 0);
    CHECK(rejected.translation.invocations == 0u);
  }

  Natural malformed_machine;
  Nba97GameCameraFrameTransformMachine invalid =
      malformed_machine.camera.machine;
  prepare_direct_machine(invalid);
  invalid.hi.known_mask = 16u;
  const Nba97GameCameraFrameTransformMachine invalid_snapshot = invalid;
  CHECK(nba97_game_gte_translation_install_from_camera(
            &malformed_machine.translation, &malformed_machine.memory, &event,
            &invalid) == NBA97_TEXT_ARGUMENT);
  CHECK(std::memcmp(&invalid, &invalid_snapshot, sizeof(invalid)) == 0);
  CHECK(malformed_machine.translation.invocations == 1u);

  Natural unknown_pointer;
  Nba97GameCameraFrameTransformMachine partial = unknown_pointer.camera.machine;
  prepare_direct_machine(partial);
  partial.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask = 14u;
  CHECK(nba97_game_gte_translation_install_from_camera(
            &unknown_pointer.translation, &unknown_pointer.memory, &event,
            &partial) == NBA97_TEXT_UNKNOWN);
  CHECK(partial.registers.gpr[NBA97_MATCH_INITIALIZE_A0].known_mask == 14u);

  CHECK(nba97_game_gte_translation_install_from_camera(
            nullptr, &natural.memory, &event, &machine) == NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_gte_translation_install_from_camera(
            &natural.translation, nullptr, &event, &machine) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_gte_translation_install_from_camera(
            &natural.translation, &natural.memory, nullptr, &machine) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_gte_translation_install_from_camera(
            &natural.translation, &natural.memory, &event, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}

void test_fallback_and_natural_failures() {
  Natural fallback;
  Nba97GameCameraFrameTransformEvent event{};
  event.pc = UINT32_C(0x80051168);
  event.delay_slot_pc = UINT32_C(0x8005116c);
  event.entry = UINT32_C(0x80056080);
  event.kind = NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATRIX_80056080;
  event.argument_count = 2u;
  Nba97GameCameraFrameTransformMachine machine = fallback.camera.machine;
  CHECK(nba97_game_gte_translation_install_from_camera(
            &fallback.translation, &fallback.memory, &event, &machine) ==
        NBA97_TEXT_COMPLETE);
  CHECK(fallback.fallback_calls == 1u);
  CHECK(fallback.translation.invocations == 0u);

  fallback.translation.fallback = nullptr;
  CHECK(nba97_game_gte_translation_install_from_camera(
            &fallback.translation, &fallback.memory, &event, &machine) ==
        NBA97_TEXT_IO_REFUSED);

  Natural before_target;
  before_target.refuse_kind =
      NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_ROTATION_80055F18;
  CHECK(nba97_game_camera_frame_transform(&before_target.camera,
                                          &before_target.camera_progress) ==
        NBA97_TEXT_IO_REFUSED);
  CHECK(before_target.translation.invocations == 0u);

  Natural after_target;
  after_target.refuse_kind =
      NBA97_GAME_CAMERA_FRAME_TRANSFORM_REFERENCE_80056650;
  CHECK(nba97_game_camera_frame_transform(&after_target.camera,
                                          &after_target.camera_progress) ==
        NBA97_TEXT_IO_REFUSED);
  CHECK(after_target.translation.invocations == 1u);
  CHECK(after_target.translation.progress.completed == 1u);
  CHECK(after_target.translation.control[7].word == UINT32_C(0xffff8000));
}


void test_composed_camera_installers() {
  Composed c;
  CHECK(nba97_game_camera_frame_transform(&c.camera,&c.camera_progress)==NBA97_TEXT_COMPLETE);
  CHECK(c.camera_progress.completed && c.matrix.progress.completed && c.rotation.progress.completed);
  CHECK(c.translation.progress.completed && c.reference_observed);
  CHECK(c.matrix.invocations==1 && c.rotation.invocations==1 && c.translation.invocations==1);
  const uint32_t expected[8]={0xE6671999u,0x20001999u,0xF0000000u,0x20000000u,0x1000u,0,0,0};
  for(unsigned i=0;i<8;++i) {
    CHECK(c.translation.control[i].word==expected[i]);
    CHECK(c.translation.control[i].known_mask==15);
  }
  for(unsigned i=8;i<32;++i) {
    CHECK(c.translation.control[i].word==c.initial_control[i].word);
    CHECK(c.translation.control[i].known_mask==c.initial_control[i].known_mask);
  }
  CHECK(c.translation.progress.machine.registers.gpr[29].word==0x801fefd0u);
  CHECK(c.translation.progress.machine.registers.gpr[31].word==0x80051214u);

  Composed prefix;
  prefix.translation.operation_budget=4;
  CHECK(nba97_game_camera_frame_transform(&prefix.camera,&prefix.camera_progress)==NBA97_TEXT_IO_REFUSED);
  CHECK(prefix.translation.result==NBA97_TEXT_LIMIT);
  CHECK(prefix.camera_progress.stopped_pc==0x8005120cu);
  CHECK(prefix.matrix.progress.completed && prefix.rotation.progress.completed);
  CHECK(prefix.translation.progress.reads==3 && prefix.translation.progress.control_writes==1);
  CHECK(!prefix.reference_observed);
  CHECK(prefix.translation.control[5].word==0 && prefix.translation.control[5].known_mask==15);
  CHECK(prefix.translation.control[6].word==prefix.initial_control[6].word);
}

} // namespace

int main() {
  test_natural_camera_owner();
  test_composed_camera_installers();
  test_adapter_prefix_and_guards();
  test_fallback_and_natural_failures();
  std::printf(
      "game_gte_translation_install_integration_tests: %zu checks passed\n",
      checks);
  return 0;
}
