#include "game_camera_override_end_capture.h"
#include "game_camera_override_end_adapter.h"
#include <sstream>
#include <stdexcept>
#include <vector>

namespace nba97 {
std::string captureGameCameraOverrideEnd() {
  std::vector<uint8_t> bytes(0x200000), known(0x200000, 1);
  Nba97GameTextRegion region{0x80000000, bytes.data(), known.data(), bytes.size()};
  Nba97GameSelectionInput state{};
  for (unsigned i = 0; i < 8; ++i) {
    state.controller_table[i] = static_cast<uint8_t>(i);
    state.controller[i].team_base = -1;
    state.controller[i].selected = {0xffff, 1};
  }
  for (unsigned i = 0; i < 11; ++i) {
    state.entity_table[i] = static_cast<uint8_t>(i);
    state.entity[i].claim = -1;
  }
  state.controller[0].team_base = 0;
  state.ball = state.tail_entity = 10;
  state.incoming_s6 = {7, 1};
  state.tail_state = 2;
  Nba97GameCameraOverrideEndSelectionBinding binding{};
  binding.memory = {&region, 1};
  binding.operation_budget = 100;
  binding.entry_machine_ready = 1;
  for (auto& word : binding.entry_machine.registers.gpr) word = {0, 15};
  binding.entry_machine.hi = binding.entry_machine.lo = {0, 15};
  binding.entry_machine.registers.gpr[29] = {0x801ff000, 15};
  binding.entry_machine.registers.gpr[31] = {0x80065578, 15};
  binding.user = &state;
  binding.io = [](void* user, const Nba97GameTextMemory*,
                  const Nba97GameCameraOverrideEndEvent* e,
                  Nba97GameCameraOverrideEndMachine* m) {
    const auto& s = *static_cast<Nba97GameSelectionInput*>(user);
    if (e->pc != 0x8007a380 || e->entry != 0x8007a114 ||
        m->registers.gpr[4].word != 0 || s.tail_state != 2 ||
        s.controller[0].selected.word != 4 || s.entity[4].claim != 0) return 0;
    // Explicit restore-service fixture. No camera pixels are fabricated.
    m->registers.gpr[2] = {0xcafebabe, 15};
    return 1;
  };
  bytes[0xbc1f0] = 1;
  const int rc = nba97_game_camera_override_end_from_selection(&binding, &state);
  const auto& p = binding.progress;
  if (rc != NBA97_SELECTION_OK || !p.completed || bytes[0xbc1f0] != 0 ||
      state.tail_state != 1 || p.returned_value.word != 0xcafebabe)
    throw std::runtime_error("camera override CPU composition failed");
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x8007A36C\",\"inclusive_end\":\"0x8007A39F\","
       "\"bytes\":52,\"instructions\":13,\"classification\":\"no direct visual effect\","
       "\"scope\":\"actual normalized selection; independent full machine; typed camera restore\","
       "\"completed\":true,\"flag_before\":1,\"flag_after\":0,\"tail_before\":2,\"tail_after\":" << state.tail_state
    << ",\"selection_writes\":" << unsigned(binding.selection_effects.write_count)
    << ",\"selected\":" << state.controller[0].selected.word << ",\"claim\":" << state.entity[4].claim
    << ",\"operations\":" << p.operations << ",\"reads\":" << p.reads << ",\"stores\":" << p.stores
    << ",\"callbacks\":" << p.callbacks_completed << ",\"returned_value\":" << p.returned_value.word
    << ",\"frame_stack_pointer\":" << p.frame_stack_pointer << ",\"returned_sp\":" << p.machine.registers.gpr[29].word
    << ",\"restored_ra\":" << p.restored_return_address.word << "}";
  return o.str();
}
}
