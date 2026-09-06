#include "game_pregame_selection_screen_adapter.h"

#include <cstring>

namespace {
bool machineValid(const Nba97GamePeriodPresentationFinishMachine &machine) {
  if (machine.registers.gpr[0].word != 0u ||
      machine.registers.gpr[0].known_mask != 15u ||
      machine.hi.known_mask > 15u || machine.lo.known_mask > 15u)
    return false;
  for (unsigned index = 0u; index != 32u; ++index)
    if (machine.registers.gpr[index].known_mask > 15u)
      return false;
  return true;
}

bool memoryValid(const Nba97GameTextMemory &memory) {
  if (memory.count != 0u && memory.region == nullptr)
    return false;
  for (size_t index = 0u; index != memory.count; ++index) {
    const auto &region = memory.region[index];
    if (region.data == nullptr || region.size == 0u ||
        static_cast<uint64_t>(region.size) > UINT64_C(0x100000000) ||
        static_cast<uint64_t>(region.base) + region.size >
            UINT64_C(0x100000000))
      return false;
    for (size_t earlier = 0u; earlier != index; ++earlier) {
      const auto &other = memory.region[earlier];
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

extern "C" void nba97_game_pregame_selection_screen_presentation_binding_init(
    Nba97GamePregameSelectionScreenPresentationBinding *binding,
    size_t operation_budget, Nba97GamePregameSelectionScreenIo io, void *user,
    Nba97GamePregameSelectionScreenAccess *journal, size_t journal_capacity,
    Nba97GamePeriodPresentationFinishIo fallback, void *fallback_user) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->operation_budget = operation_budget;
  binding->io = io;
  binding->user = user;
  binding->access_journal = journal;
  binding->access_journal_capacity = journal_capacity;
  binding->fallback = fallback;
  binding->fallback_user = fallback_user;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_pregame_selection_screen_from_presentation_finish(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GamePeriodPresentationFinishEvent *event,
    Nba97GamePeriodPresentationFinishMachine *machine) {
  auto *binding =
      static_cast<Nba97GamePregameSelectionScreenPresentationBinding *>(opaque);
  if (event == nullptr)
    return 0;
  const bool kind =
      event->kind == NBA97_GAME_PERIOD_PRESENTATION_FINISH_CHILD_80046C2C;
  const bool entry = event->entry == UINT32_C(0x80046c2c);
  const bool pc = event->pc == UINT32_C(0x8002de14);
  const bool delay = event->delay_slot_pc == UINT32_C(0x8002de18);
  const bool returnAddress =
      machine != nullptr &&
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
          UINT32_C(0x8002de1c);
  if (!kind && !entry && !pc && !delay && !returnAddress) {
    if (binding != nullptr)
      ++binding->fallback_invocations;
    return binding != nullptr && binding->fallback != nullptr
               ? binding->fallback(binding->fallback_user, memory, event,
                                   machine)
               : 0;
  }

  if (binding != nullptr)
    binding->result = NBA97_TEXT_ARGUMENT;
  if (binding == nullptr || memory == nullptr || machine == nullptr || !kind ||
      !entry || !pc || !delay || event->argument_count != 0u ||
      event->invocation != 1u || !machineValid(*machine) ||
      !memoryValid(*memory) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15u ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          UINT32_C(0x8002de1c) ||
      (binding->access_journal_capacity != 0u &&
       binding->access_journal == nullptr))
    return 0;

  Nba97GamePregameSelectionScreenContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  ++binding->invocations;
  binding->event = *event;
  binding->result =
      nba97_game_pregame_selection_screen(&context, &binding->progress);
  *machine = binding->progress.machine;
  if (binding->result != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}
