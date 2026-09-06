#include "game_actor_input_adapter.h"

#include <cstdint>

int nba97_game_actor_input_from_match_tick(void *opaque,
                                           const Nba97MatchTickCall *call,
                                           Nba97GamePeriodValue *result) {
  auto *binding = static_cast<Nba97GameActorInputBinding *>(opaque);
  if (!binding || !call || result || call->pc != UINT32_C(0x80068e8c) ||
      call->entry != UINT32_C(0x800686b8) || call->count != 0 ||
      call->args[0] != 0 || call->args[1] != 0 ||
      binding->entry_machine_ready != 1 ||
      binding->entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
              .known_mask != 0x0f ||
      binding->entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          call->pc + 8u ||
      (!binding->memory.region && binding->memory.count) ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding)
      binding->result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  Nba97GameActorInputContext context{};
  context.memory = binding->memory;
  context.operation_budget = binding->operation_budget;
  context.machine = binding->entry_machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  ++binding->invocations;
  binding->result = nba97_game_actor_input(&context, &binding->progress);
  return binding->result == NBA97_TEXT_COMPLETE;
}
