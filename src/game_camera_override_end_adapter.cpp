#include "game_camera_override_end_adapter.h"

int nba97_game_camera_override_end_from_selection(
    Nba97GameCameraOverrideEndSelectionBinding *binding,
    Nba97GameSelectionInput *state) {
  if (!binding || !state)
    return NBA97_SELECTION_INVALID;

  binding->selection_result = nba97_game_controller_selection(
      &binding->selection_effects, state);
  if (binding->selection_result != NBA97_SELECTION_OK)
    return binding->selection_result;

  for (unsigned i = 0; i < binding->selection_effects.write_count; ++i) {
    const Nba97GameSelectionWrite &write =
        binding->selection_effects.writes[i];
    state->controller[write.controller_record].selected.word =
        write.selected_word;
    state->controller[write.controller_record].selected.known = 1;
    state->entity[write.entity_record].claim =
        static_cast<int16_t>(write.logical_controller);
  }

  binding->tail_result = NBA97_TEXT_COMPLETE;
  if (binding->selection_effects.call_7a36c) {
    if (binding->entry_machine_ready != 1 ||
        binding->entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                .known_mask != 15 ||
        binding->entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
            UINT32_C(0x80065578) ||
        (!binding->access_journal && binding->access_journal_capacity)) {
      binding->tail_result = NBA97_TEXT_ARGUMENT;
      return binding->tail_result;
    }
    Nba97GameCameraOverrideEndContext context{};
    context.memory = binding->memory;
    context.operation_budget = binding->operation_budget;
    context.machine = binding->entry_machine;
    context.io = binding->io;
    context.user = binding->user;
    context.access_journal = binding->access_journal;
    context.access_journal_capacity = binding->access_journal_capacity;
    ++binding->invocations;
    binding->tail_result =
        nba97_game_camera_override_end(&context, &binding->progress);
    if (binding->tail_result != NBA97_TEXT_COMPLETE)
      return binding->tail_result;
  }

  if (binding->selection_effects.tail_state_written)
    state->tail_state = binding->selection_effects.tail_state;
  return NBA97_SELECTION_OK;
}
