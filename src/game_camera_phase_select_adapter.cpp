#include "game_camera_phase_select_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct OwnerRun {
  Nba97GameCameraPhaseSelectBinding *binding;
};

struct ParentRun {
  Nba97GameCameraSelectIo fallback;
  void *fallbackUser;
  Nba97GameCameraPhaseSelectBinding *binding;
};

bool validRegisters(const Nba97GameCameraSelectRegisters &registers) {
  if (registers.gpr[0].word != 0u || registers.gpr[0].known_mask != 15u)
    return false;
  for (unsigned index = 0u; index != 32u; ++index)
    if (registers.gpr[index].known_mask > 15u)
      return false;
  return true;
}

bool validMachine(const Nba97GameCameraPhaseSelectMachine &machine) {
  return validRegisters(machine.registers) && machine.hi.known_mask <= 15u &&
         machine.lo.known_mask <= 15u;
}

bool parentTarget(const Nba97GameCameraSelectEvent &event,
                  const Nba97GameCameraSelectRegisters &registers) {
  return event.kind == NBA97_GAME_CAMERA_SELECT_CHILD_8007E26C ||
         event.entry == UINT32_C(0x8007e26c) ||
         event.pc == UINT32_C(0x80079a0c) ||
         event.delay_slot_pc == UINT32_C(0x80079a10) ||
         (registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 15u &&
          registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
              UINT32_C(0x80079a14));
}

bool exactParent(const Nba97GameCameraSelectEvent &event,
                 const Nba97GameCameraSelectRegisters &registers) {
  const auto &a0 = registers.gpr[NBA97_MATCH_INITIALIZE_A0];
  return event.kind == NBA97_GAME_CAMERA_SELECT_CHILD_8007E26C &&
         event.entry == UINT32_C(0x8007e26c) &&
         event.pc == UINT32_C(0x80079a0c) &&
         event.delay_slot_pc == UINT32_C(0x80079a10) &&
         event.argument_count == 1u &&
         registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask == 15u &&
         registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
             UINT32_C(0x80079a14) &&
         a0.known_mask == 15u && (a0.word == 0u || a0.word == 1u);
}

bool ownerCameraTarget(const Nba97GameCameraPhaseSelectEvent &event) {
  return event.kind == NBA97_GAME_CAMERA_PHASE_SELECT_CAMERA_800799CC ||
         event.entry == UINT32_C(0x800799cc) ||
         event.pc == UINT32_C(0x8007e3a0) || event.pc == UINT32_C(0x8007e3dc) ||
         event.pc == UINT32_C(0x8007e424);
}

bool exactOwnerCamera(const Nba97GameCameraPhaseSelectEvent &event,
                      const Nba97GameCameraPhaseSelectMachine &machine) {
  const auto &a0 = machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0];
  const auto &a1 = machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1];
  const auto &ra = machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA];
  if (event.kind != NBA97_GAME_CAMERA_PHASE_SELECT_CAMERA_800799CC ||
      event.entry != UINT32_C(0x800799cc) || event.argument_count != 2u ||
      event.invocation != 1u || event.delay_slot_pc != event.pc + 4u ||
      a0.known_mask != 15u || a1.known_mask != 15u || ra.known_mask != 15u ||
      ra.word != event.pc + 8u)
    return false;
  if (event.pc == UINT32_C(0x8007e3a0))
    return a0.word == 3u && a1.word == 1u;
  if (event.pc == UINT32_C(0x8007e3dc))
    return a0.word == 7u && a1.word == 0u;
  if (event.pc == UINT32_C(0x8007e424))
    return (a0.word == 1u || a0.word == 2u) && a1.word == 0u;
  return false;
}

bool exactTyped(const Nba97GameCameraPhaseSelectEvent &event,
                const Nba97GameCameraPhaseSelectMachine &machine) {
  const auto &a0 = machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0];
  const auto &ra = machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA];
  if (event.delay_slot_pc != event.pc + 4u || event.argument_count != 1u ||
      event.invocation == 0u || ra.known_mask != 15u ||
      ra.word != event.pc + 8u)
    return false;
  if (event.kind == NBA97_GAME_CAMERA_PHASE_SELECT_ADJUST_80079EBC &&
      event.entry == UINT32_C(0x80079ebc) && a0.known_mask == 15u) {
    if (event.pc == UINT32_C(0x8007e3a8))
      return event.invocation == 1u && a0.word == 15u;
    if (event.pc == UINT32_C(0x8007e3b0))
      return event.invocation == 2u && a0.word == 8u;
    if (event.pc == UINT32_C(0x8007e3b8))
      return event.invocation == 3u && a0.word == 8u;
    if (event.pc == UINT32_C(0x8007e3e4))
      return event.invocation == 1u && a0.word == 13u;
  }
  return event.kind == NBA97_GAME_CAMERA_PHASE_SELECT_FINALIZE_80079F78 &&
         event.entry == UINT32_C(0x80079f78) &&
         event.pc == UINT32_C(0x8007e434) && event.invocation == 1u;
}

int composeCamera(OwnerRun &run, const Nba97GameTextMemory *memory,
                  Nba97GameCameraPhaseSelectMachine *machine) {
  auto &binding = *run.binding;
  if (binding.camera_io == nullptr ||
      (!binding.camera_access_journal &&
       binding.camera_access_journal_capacity != 0u)) {
    binding.nested_result = NBA97_TEXT_IO_REFUSED;
    return 0;
  }
  Nba97GameCameraSelectContext context{};
  context.memory = *memory;
  context.operation_budget = binding.camera_operation_budget;
  context.registers = machine->registers;
  context.io = binding.camera_io;
  context.user = binding.camera_user;
  context.access_journal = binding.camera_access_journal;
  context.access_journal_capacity = binding.camera_access_journal_capacity;
  ++binding.camera_invocations;
  binding.nested_result =
      nba97_game_camera_select(&context, &binding.camera_progress);
  if (validRegisters(binding.camera_progress.registers))
    machine->registers = binding.camera_progress.registers;
  return binding.nested_result == NBA97_TEXT_COMPLETE ? 1 : 0;
}

int dispatchOwner(void *opaque, const Nba97GameTextMemory *memory,
                  const Nba97GameCameraPhaseSelectEvent *event,
                  Nba97GameCameraPhaseSelectMachine *machine) {
  auto &run = *static_cast<OwnerRun *>(opaque);
  auto &binding = *run.binding;
  if (memory == nullptr || event == nullptr || machine == nullptr ||
      !validMachine(*machine)) {
    binding.nested_result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  if (ownerCameraTarget(*event)) {
    if (!exactOwnerCamera(*event, *machine)) {
      binding.nested_result = NBA97_TEXT_ARGUMENT;
      return 0;
    }
    return composeCamera(run, memory, machine);
  }
  ++binding.typed_invocations;
  if (!exactTyped(*event, *machine)) {
    binding.nested_result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  if (binding.io == nullptr) {
    binding.nested_result = NBA97_TEXT_IO_REFUSED;
    return 0;
  }
  const int accepted = binding.io(binding.user, memory, event, machine);
  if (accepted != 1)
    binding.nested_result = NBA97_TEXT_IO_REFUSED;
  return accepted;
}

int dispatchParent(void *opaque, const Nba97GameTextMemory *memory,
                   const Nba97GameCameraSelectEvent *event,
                   Nba97GameCameraSelectRegisters *registers) {
  auto &run = *static_cast<ParentRun *>(opaque);
  if (event != nullptr && registers != nullptr &&
      parentTarget(*event, *registers))
    return nba97_game_camera_phase_select_from_camera_select(
        run.binding, memory, event, registers);
  if (run.fallback == nullptr)
    return 0;
  return run.fallback(run.fallbackUser, memory, event, registers);
}
} // namespace

extern "C" void nba97_game_camera_phase_select_binding_init(
    Nba97GameCameraPhaseSelectBinding *binding, std::size_t operationBudget,
    std::size_t cameraOperationBudget) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->operation_budget = operationBudget;
  binding->camera_operation_budget = cameraOperationBudget;
  binding->result = NBA97_TEXT_ARGUMENT;
  binding->nested_result = NBA97_TEXT_COMPLETE;
}

extern "C" int nba97_game_camera_phase_select_from_camera_select(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameCameraSelectEvent *event,
    Nba97GameCameraSelectRegisters *registers) {
  auto *binding = static_cast<Nba97GameCameraPhaseSelectBinding *>(opaque);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      registers == nullptr || !exactParent(*event, *registers) ||
      !validRegisters(*registers) ||
      (!binding->access_journal && binding->access_journal_capacity != 0u) ||
      (!binding->camera_access_journal &&
       binding->camera_access_journal_capacity != 0u)) {
    if (binding != nullptr)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  Nba97GameCameraPhaseSelectContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine.registers = *registers;
  context.machine.hi = {0u, 0u};
  context.machine.lo = {0u, 0u};
  OwnerRun run{binding};
  context.io = dispatchOwner;
  context.user = &run;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  ++binding->invocations;
  binding->event = *event;
  binding->nested_result = NBA97_TEXT_COMPLETE;
  binding->result =
      nba97_game_camera_phase_select(&context, &binding->progress);
  if (binding->result == NBA97_TEXT_IO_REFUSED &&
      binding->nested_result != NBA97_TEXT_COMPLETE)
    binding->result = binding->nested_result;
  if (validRegisters(binding->progress.machine.registers))
    *registers = binding->progress.machine.registers;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

extern "C" int nba97_game_camera_select_with_phase_select(
    const Nba97GameCameraSelectContext *parent,
    Nba97GameCameraPhaseSelectBinding *binding,
    Nba97GameCameraSelectProgress *progress) {
  if (parent == nullptr || binding == nullptr || progress == nullptr)
    return NBA97_TEXT_ARGUMENT;
  const auto operationBudget = binding->operation_budget;
  const auto cameraOperationBudget = binding->camera_operation_budget;
  const auto io = binding->io;
  void *const user = binding->user;
  auto *const journal = binding->access_journal;
  const auto journalCapacity = binding->access_journal_capacity;
  auto *const cameraJournal = binding->camera_access_journal;
  const auto cameraJournalCapacity = binding->camera_access_journal_capacity;
  nba97_game_camera_phase_select_binding_init(binding, operationBudget,
                                              cameraOperationBudget);
  binding->io = io;
  binding->user = user;
  binding->access_journal = journal;
  binding->access_journal_capacity = journalCapacity;
  binding->camera_io = parent->io;
  binding->camera_user = parent->user;
  binding->camera_access_journal = cameraJournal;
  binding->camera_access_journal_capacity = cameraJournalCapacity;
  ParentRun run{parent->io, parent->user, binding};
  Nba97GameCameraSelectContext context = *parent;
  context.io = dispatchParent;
  context.user = &run;
  const int result = nba97_game_camera_select(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED &&
      progress->stopped_pc == UINT32_C(0x80079a0c) &&
      binding->invocations != 0u)
    return binding->result;
  return result;
}
