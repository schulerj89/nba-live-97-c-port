#include "game_camera_elapsed_dispatch_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct OwnerRun {
  Nba97GameCameraElapsedDispatchBinding *binding;
};

struct ParentRun {
  Nba97GameCameraSelectIo fallback;
  void *fallbackUser;
  Nba97GameCameraElapsedDispatchBinding *binding;
};

bool validRegisters(const Nba97GameCameraSelectRegisters &registers) {
  if (registers.gpr[0].word != 0u || registers.gpr[0].known_mask != 15u)
    return false;
  for (unsigned index = 0u; index != 32u; ++index)
    if (registers.gpr[index].known_mask > 15u)
      return false;
  return true;
}

bool validMachine(const Nba97GameCameraElapsedDispatchMachine &machine) {
  return validRegisters(machine.registers) && machine.hi.known_mask <= 15u &&
         machine.lo.known_mask <= 15u;
}

bool parentTarget(const Nba97GameCameraSelectEvent &event,
                  const Nba97GameCameraSelectRegisters &registers) {
  const auto &ra = registers.gpr[NBA97_MATCH_INITIALIZE_RA];
  return event.kind == NBA97_GAME_CAMERA_SELECT_CHILD_800798B4 ||
         event.entry == UINT32_C(0x800798b4) ||
         event.pc == UINT32_C(0x80079c2c) ||
         event.delay_slot_pc == UINT32_C(0x80079c30) ||
         event.pc == UINT32_C(0x80079c8c) ||
         event.delay_slot_pc == UINT32_C(0x80079c90) ||
         (ra.known_mask == 15u &&
          (ra.word == UINT32_C(0x80079c34) || ra.word == UINT32_C(0x80079c94)));
}

bool exactParent(const Nba97GameCameraSelectEvent &event,
                 const Nba97GameCameraSelectRegisters &registers) {
  const auto &a0 = registers.gpr[NBA97_MATCH_INITIALIZE_A0];
  const auto &ra = registers.gpr[NBA97_MATCH_INITIALIZE_RA];
  if (event.kind != NBA97_GAME_CAMERA_SELECT_CHILD_800798B4 ||
      event.entry != UINT32_C(0x800798b4) || event.argument_count != 1u ||
      a0.known_mask != 15u || a0.word != UINT32_MAX || ra.known_mask != 15u)
    return false;
  if (event.pc == UINT32_C(0x80079c2c))
    return event.delay_slot_pc == UINT32_C(0x80079c30) &&
           ra.word == UINT32_C(0x80079c34);
  if (event.pc == UINT32_C(0x80079c8c))
    return event.delay_slot_pc == UINT32_C(0x80079c90) &&
           ra.word == UINT32_C(0x80079c94);
  return false;
}

bool exactChild(const Nba97GameCameraElapsedDispatchEvent &event,
                const Nba97GameCameraElapsedDispatchMachine &machine) {
  const auto &ra = machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA];
  if (event.argument_count != 0u || event.invocation != 1u ||
      event.delay_slot_pc != event.pc + 4u || ra.known_mask != 15u ||
      ra.word != event.pc + 8u)
    return false;
  if (event.kind == NBA97_GAME_CAMERA_ELAPSED_DISPATCH_INDIRECT_8007995C)
    return event.pc == UINT32_C(0x8007995c) && event.entry != 0u &&
           (event.entry & 3u) == 0u;
  if (event.kind == NBA97_GAME_CAMERA_ELAPSED_DISPATCH_PROBE_8007A468)
    return event.pc == UINT32_C(0x80079978) &&
           event.entry == UINT32_C(0x8007a468);
  return event.kind == NBA97_GAME_CAMERA_ELAPSED_DISPATCH_REFRESH_8007A410 &&
         event.pc == UINT32_C(0x8007999c) &&
         event.entry == UINT32_C(0x8007a410);
}

int dispatchOwner(void *opaque, const Nba97GameTextMemory *memory,
                  const Nba97GameCameraElapsedDispatchEvent *event,
                  Nba97GameCameraElapsedDispatchMachine *machine) {
  auto &run = *static_cast<OwnerRun *>(opaque);
  auto &binding = *run.binding;
  ++binding.typed_invocations;
  if (memory == nullptr || event == nullptr || machine == nullptr ||
      !validMachine(*machine) || !exactChild(*event, *machine)) {
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
    return nba97_game_camera_elapsed_dispatch_from_camera_select(
        run.binding, memory, event, registers);
  if (run.fallback == nullptr)
    return 0;
  return run.fallback(run.fallbackUser, memory, event, registers);
}
} // namespace

extern "C" void nba97_game_camera_elapsed_dispatch_binding_init(
    Nba97GameCameraElapsedDispatchBinding *binding,
    std::size_t operationBudget) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->operation_budget = operationBudget;
  binding->result = NBA97_TEXT_ARGUMENT;
  binding->nested_result = NBA97_TEXT_COMPLETE;
}

extern "C" int nba97_game_camera_elapsed_dispatch_from_camera_select(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameCameraSelectEvent *event,
    Nba97GameCameraSelectRegisters *registers) {
  auto *binding = static_cast<Nba97GameCameraElapsedDispatchBinding *>(opaque);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      registers == nullptr || !exactParent(*event, *registers) ||
      !validRegisters(*registers) ||
      (!binding->access_journal && binding->access_journal_capacity != 0u)) {
    if (binding != nullptr)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }
  Nba97GameCameraElapsedDispatchContext context{};
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
      nba97_game_camera_elapsed_dispatch(&context, &binding->progress);
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

extern "C" int nba97_game_camera_select_with_elapsed_dispatch(
    const Nba97GameCameraSelectContext *parent,
    Nba97GameCameraElapsedDispatchBinding *binding,
    Nba97GameCameraSelectProgress *progress) {
  if (parent == nullptr || binding == nullptr || progress == nullptr)
    return NBA97_TEXT_ARGUMENT;
  const auto operationBudget = binding->operation_budget;
  const auto io = binding->io;
  void *const user = binding->user;
  auto *const journal = binding->access_journal;
  const auto journalCapacity = binding->access_journal_capacity;
  nba97_game_camera_elapsed_dispatch_binding_init(binding, operationBudget);
  binding->io = io;
  binding->user = user;
  binding->access_journal = journal;
  binding->access_journal_capacity = journalCapacity;
  ParentRun run{parent->io, parent->user, binding};
  Nba97GameCameraSelectContext context = *parent;
  context.io = dispatchParent;
  context.user = &run;
  const int result = nba97_game_camera_select(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED && binding->invocations != 0u &&
      (progress->stopped_pc == UINT32_C(0x80079c2c) ||
       progress->stopped_pc == UINT32_C(0x80079c8c)))
    return binding->result;
  return result;
}
