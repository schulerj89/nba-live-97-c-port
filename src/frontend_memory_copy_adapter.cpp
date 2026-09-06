#include "frontend_memory_copy_adapter.h"

#include <cstring>

namespace {
struct Composition {
  Nba97FrontendMainIo fallback;
  void *fallback_user;
  Nba97FrontendMemoryCopyBinding *binding;
};

int dispatch(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97FrontendMainEvent *event,
             Nba97FrontendMainMachine *machine,
             Nba97FrontendMainCalleeOutcome *outcome) {
  auto *composition = static_cast<Composition *>(opaque);
  if (!composition || !composition->binding || !memory || !event || !machine ||
      !outcome)
    return 0;
  if (event->site != NBA97_FRONTEND_MAIN_SITE_80028B54)
    return composition->fallback
               ? composition->fallback(composition->fallback_user, memory,
                                       event, machine, outcome)
               : 0;

  auto &binding = *composition->binding;
  binding.event = *event;
  binding.input_machine = *machine;
  ++binding.invocations;
  if (event->pc != 0x80028b54u || event->delay_slot_pc != 0x80028b58u ||
      event->entry != 0x800909a8u || event->argument_count != 3 ||
      event->target_program != NBA97_FRONTEND_MAIN_PROGRAM_FEONLY ||
      event->invocation != 1 ||
      machine->registers.gpr[NBA97_FRONTEND_MAIN_RA].word != 0x80028b5cu ||
      machine->registers.gpr[NBA97_FRONTEND_MAIN_RA].known_mask != 15) {
    binding.result = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  Nba97FrontendMemoryCopyContext context{};
  context.memory = *memory;
  context.operation_budget = binding.operation_budget;
  context.machine = *machine;
  context.access_journal = binding.access_journal;
  context.access_journal_capacity = binding.access_journal_capacity;
  context.instruction_journal = binding.instruction_journal;
  context.instruction_journal_capacity = binding.instruction_journal_capacity;
  binding.result = nba97_frontend_memory_copy(&context, &binding.progress);
  *machine = binding.progress.machine;
  if (binding.result != NBA97_TEXT_COMPLETE || !binding.progress.completed)
    return 0;
  ++binding.completions;
  *outcome = NBA97_FRONTEND_MAIN_CALLEE_RETURNED;
  return 1;
}
}  // namespace

int nba97_frontend_main_with_recovered_memory_copy(
    Nba97FrontendMainContext *context,
    Nba97FrontendMemoryCopyBinding *binding,
    Nba97FrontendMainProgress *progress) {
  if (!context || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  Nba97FrontendMainContext composed = *context;
  Composition composition{context->io, context->user, binding};
  composed.io = dispatch;
  composed.user = &composition;
  return nba97_frontend_main(&composed, progress);
}
