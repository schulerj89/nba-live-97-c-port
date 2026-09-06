#include "game_camera_state_lookup_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct ParentRun {
  Nba97GameCameraElapsedDispatchIo fallback;
  void *fallbackUser;
  Nba97GameCameraStateLookupBinding *binding;
};

bool validMachine(const Nba97GameCameraElapsedDispatchMachine &machine) {
  if (machine.registers.gpr[0].word != 0u ||
      machine.registers.gpr[0].known_mask != 15u ||
      machine.hi.known_mask > 15u || machine.lo.known_mask > 15u)
    return false;
  for (unsigned index = 0u; index != 32u; ++index)
    if (machine.registers.gpr[index].known_mask > 15u)
      return false;
  return true;
}

bool parentTarget(const Nba97GameCameraElapsedDispatchEvent &event,
                  const Nba97GameCameraElapsedDispatchMachine &machine) {
  const auto &ra = machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA];
  return event.kind == NBA97_GAME_CAMERA_ELAPSED_DISPATCH_REFRESH_8007A410 ||
         event.entry == UINT32_C(0x8007a410) ||
         event.pc == UINT32_C(0x8007999c) ||
         event.delay_slot_pc == UINT32_C(0x800799a0) ||
         (ra.known_mask == 15u && ra.word == UINT32_C(0x800799a4));
}

bool exactParent(const Nba97GameCameraElapsedDispatchEvent &event,
                 const Nba97GameCameraElapsedDispatchMachine &machine) {
  const auto &ra = machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA];
  return event.kind == NBA97_GAME_CAMERA_ELAPSED_DISPATCH_REFRESH_8007A410 &&
         event.entry == UINT32_C(0x8007a410) &&
         event.pc == UINT32_C(0x8007999c) &&
         event.delay_slot_pc == UINT32_C(0x800799a0) &&
         event.argument_count == 0u && event.invocation == 1u &&
         ra.known_mask == 15u && ra.word == UINT32_C(0x800799a4);
}

int dispatchParent(void *opaque, const Nba97GameTextMemory *memory,
                   const Nba97GameCameraElapsedDispatchEvent *event,
                   Nba97GameCameraElapsedDispatchMachine *machine) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (event != nullptr && machine != nullptr && parentTarget(*event, *machine))
    return nba97_game_camera_state_lookup_from_elapsed_dispatch(
        run.binding, memory, event, machine);
  if (run.fallback == nullptr)
    return 0;
  return run.fallback(run.fallbackUser, memory, event, machine);
}
} // namespace

extern "C" void nba97_game_camera_state_lookup_binding_init(
    Nba97GameCameraStateLookupBinding *binding, std::size_t operationBudget) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->operation_budget = operationBudget;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_camera_state_lookup_from_elapsed_dispatch(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameCameraElapsedDispatchEvent *event,
    Nba97GameCameraElapsedDispatchMachine *machine) {
  auto *binding = static_cast<Nba97GameCameraStateLookupBinding *>(opaque);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      machine == nullptr || !validMachine(*machine) ||
      !exactParent(*event, *machine) ||
      (!binding->access_journal && binding->access_journal_capacity != 0u)) {
    if (binding != nullptr)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  Nba97GameCameraStateLookupContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  ++binding->invocations;
  binding->event = *event;
  binding->result =
      nba97_game_camera_state_lookup(&context, &binding->progress);
  if (validMachine(binding->progress.machine))
    *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

extern "C" int nba97_game_camera_elapsed_dispatch_with_state_lookup(
    const Nba97GameCameraElapsedDispatchContext *parent,
    Nba97GameCameraStateLookupBinding *binding,
    Nba97GameCameraElapsedDispatchProgress *progress) {
  if (parent == nullptr || binding == nullptr || progress == nullptr)
    return NBA97_TEXT_ARGUMENT;
  const auto operationBudget = binding->operation_budget;
  auto *const journal = binding->access_journal;
  const auto journalCapacity = binding->access_journal_capacity;
  nba97_game_camera_state_lookup_binding_init(binding, operationBudget);
  binding->access_journal = journal;
  binding->access_journal_capacity = journalCapacity;
  ParentRun run{parent->io, parent->user, binding};
  Nba97GameCameraElapsedDispatchContext context = *parent;
  context.io = dispatchParent;
  context.user = &run;
  const int result = nba97_game_camera_elapsed_dispatch(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED && binding->invocations != 0u &&
      progress->stopped_pc == UINT32_C(0x8007999c))
    return binding->result;
  return result;
}
