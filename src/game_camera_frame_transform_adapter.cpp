#include "game_camera_frame_transform_adapter.h"

#include <cstring>

namespace {

bool valid_machine(const Nba97GameCameraFrameTransformMachine &machine) {
  if (machine.registers.gpr[0].word != 0u ||
      machine.registers.gpr[0].known_mask != 15u ||
      machine.hi.known_mask > 15u || machine.lo.known_mask > 15u)
    return false;
  for (unsigned index = 0; index != 32u; ++index)
    if (machine.registers.gpr[index].known_mask > 15u)
      return false;
  return true;
}

bool valid_memory(const Nba97GameTextMemory &memory) {
  if (memory.count != 0u && memory.region == nullptr)
    return false;
  for (size_t index = 0; index != memory.count; ++index) {
    const Nba97GameTextRegion &region = memory.region[index];
    if (region.data == nullptr || region.size == 0u ||
        region.size > UINT64_C(0x100000000) ||
        static_cast<uint64_t>(region.base) + region.size >
            UINT64_C(0x100000000))
      return false;
    for (size_t earlier = 0; earlier != index; ++earlier) {
      const Nba97GameTextRegion &other = memory.region[earlier];
      if (static_cast<uint64_t>(region.base) <
              static_cast<uint64_t>(other.base) + other.size &&
          static_cast<uint64_t>(other.base) <
              static_cast<uint64_t>(region.base) + region.size)
        return false;
    }
  }
  return true;
}

} // namespace

extern "C" void nba97_game_camera_frame_transform_match_frame_binding_init(
    Nba97GameCameraFrameTransformMatchFrameBinding *binding,
    const Nba97GameTextMemory *memory,
    const Nba97GameCameraFrameTransformMachine *entry_machine,
    size_t operation_budget, Nba97GameCameraFrameTransformIo io, void *user,
    Nba97GameCameraFrameTransformAccess *access_journal,
    size_t access_journal_capacity, Nba97MatchFrameIo fallback,
    void *fallback_user) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  if (memory != nullptr)
    binding->memory = *memory;
  if (entry_machine != nullptr)
    binding->entry_machine = *entry_machine;
  binding->operation_budget = operation_budget;
  binding->io = io;
  binding->user = user;
  binding->access_journal = access_journal;
  binding->access_journal_capacity = access_journal_capacity;
  binding->fallback = fallback;
  binding->fallback_user = fallback_user;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_camera_frame_transform_from_match_frame(
    void *user, const Nba97MatchFrameCall *call, Nba97GamePeriodValue *value) {
  Nba97GameCameraFrameTransformMatchFrameBinding *binding =
      static_cast<Nba97GameCameraFrameTransformMatchFrameBinding *>(user);
  if (binding == nullptr || call == nullptr)
    return NBA97_BODY_ARGUMENT;

  const bool camera_pc = call->pc == UINT32_C(0x800490b4);
  const bool camera_entry = call->entry == UINT32_C(0x80051098);
  if (!camera_pc && !camera_entry) {
    if (binding->fallback == nullptr)
      return NBA97_MATCH_FRAME_IO_REQUIRED;
    return binding->fallback(binding->fallback_user, call, value);
  }

  std::memset(&binding->progress, 0, sizeof(binding->progress));
  binding->result = NBA97_TEXT_ARGUMENT;
  if (!camera_pc || !camera_entry || call->args[0] != 0u ||
      call->args[1] != 0u || !valid_machine(binding->entry_machine) ||
      binding->entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
              .known_mask != 15u ||
      binding->entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          call->pc + 8u ||
      !valid_memory(binding->memory) ||
      (binding->access_journal_capacity != 0u &&
       binding->access_journal == nullptr))
    return NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATCH_FRAME_CHILD_INCOMPLETE;

  Nba97GameCameraFrameTransformContext context{};
  context.memory = binding->memory;
  context.operation_budget = binding->operation_budget;
  context.machine = binding->entry_machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  ++binding->invocations;
  binding->result =
      nba97_game_camera_frame_transform(&context, &binding->progress);
  return binding->result == NBA97_TEXT_COMPLETE
             ? NBA97_BODY_OK
             : NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATCH_FRAME_CHILD_INCOMPLETE;
}
