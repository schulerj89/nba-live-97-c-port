#include "game_rotation_matrix_adapter.h"

#include <cstring>

namespace {
bool valid_machine(const Nba97GameRotationMatrixMachine &machine) {
  if (machine.registers.gpr[0].word != 0u ||
      machine.registers.gpr[0].known_mask != 15u ||
      machine.hi.known_mask > 15u || machine.lo.known_mask > 15u)
    return false;
  for (unsigned i = 0; i != 32u; ++i)
    if (machine.registers.gpr[i].known_mask > 15u)
      return false;
  return true;
}
bool valid_memory(const Nba97GameTextMemory &memory) {
  if (memory.count != 0u && memory.region == nullptr)
    return false;
  for (size_t i = 0; i != memory.count; ++i) {
    const Nba97GameTextRegion &a = memory.region[i];
    if (a.data == nullptr || a.size == 0u || a.size > UINT64_C(0x100000000) ||
        static_cast<uint64_t>(a.base) + a.size > UINT64_C(0x100000000))
      return false;
    for (size_t j = 0; j != i; ++j) {
      const Nba97GameTextRegion &b = memory.region[j];
      if (static_cast<uint64_t>(a.base) <
              static_cast<uint64_t>(b.base) + b.size &&
          static_cast<uint64_t>(b.base) <
              static_cast<uint64_t>(a.base) + a.size)
        return false;
    }
  }
  return true;
}
} // namespace

extern "C" void nba97_game_rotation_matrix_binding_init(
    Nba97GameRotationMatrixBinding *binding, size_t operation_budget,
    Nba97GameRotationMatrixAccess *access_journal,
    size_t access_journal_capacity) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->operation_budget = operation_budget;
  binding->access_journal = access_journal;
  binding->access_journal_capacity = access_journal_capacity;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_rotation_matrix_from_camera_frame_transform(
    void *user, const Nba97GameTextMemory *memory,
    const Nba97GameCameraFrameTransformEvent *event,
    Nba97GameCameraFrameTransformMachine *machine) {
  auto *binding = static_cast<Nba97GameRotationMatrixBinding *>(user);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      machine == nullptr)
    return 0;
  ++binding->invocations;
  std::memset(&binding->progress, 0, sizeof(binding->progress));
  binding->result = NBA97_TEXT_ARGUMENT;
  if (event->pc != UINT32_C(0x80051168) ||
      event->delay_slot_pc != UINT32_C(0x8005116c) ||
      event->entry != UINT32_C(0x80056080) ||
      event->kind != NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATRIX_80056080 ||
      event->argument_count != 2u || !valid_machine(*machine) ||
      !valid_memory(*memory) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15u ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          UINT32_C(0x80051170) ||
      (binding->access_journal_capacity != 0u &&
       binding->access_journal == nullptr))
    return 0;
  Nba97GameRotationMatrixContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result = nba97_game_rotation_matrix(&context, &binding->progress);
  *machine = binding->progress.machine;
  return binding->result == NBA97_TEXT_COMPLETE ? 1 : 0;
}
