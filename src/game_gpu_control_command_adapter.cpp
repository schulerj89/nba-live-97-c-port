#include "game_gpu_control_command_adapter.h"

#include <cstdint>
#include <cstring>

namespace {
struct Run {
  Nba97GameDisplayEnvironmentIo fallback;
  void *user;
  Nba97GameGpuControlCommandBinding *binding;
};

int indexOf(const Nba97GameDisplayEnvironmentEvent *event) {
  if (!event)
    return -1;
  switch (event->pc) {
  case 0x80099d6cu:
    return NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_99D6C;
  case 0x80099f78u:
    return NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_99F78;
  case 0x80099fa4u:
    return NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_99FA4;
  case 0x8009a114u:
    return NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_9A114;
  default:
    return -1;
  }
}

bool target(const Nba97GameDisplayEnvironmentEvent *event) {
  return event && (indexOf(event) >= 0 || event->entry == 0x8009b16cu);
}

bool machineValid(const Nba97GameDisplayEnvironmentMachine &machine) {
  if (machine.registers.gpr[0].word ||
      machine.registers.gpr[0].known_mask != 15 || machine.hi.known_mask > 15 ||
      machine.lo.known_mask > 15)
    return false;
  for (unsigned index = 0; index < 32; ++index)
    if (machine.registers.gpr[index].known_mask > 15)
      return false;
  return true;
}

bool memoryValid(const Nba97GameTextMemory &memory) {
  if (!memory.region && memory.count)
    return false;
  for (std::size_t i = 0; i < memory.count; ++i) {
    const auto &region = memory.region[i];
    if (!region.data || !region.size || region.size > UINT64_C(0x100000000) ||
        std::uint64_t(region.base) + region.size > UINT64_C(0x100000000))
      return false;
    for (std::size_t j = 0; j < i; ++j) {
      const auto &earlier = memory.region[j];
      if (std::uint64_t(region.base) <
              std::uint64_t(earlier.base) + earlier.size &&
          std::uint64_t(earlier.base) <
              std::uint64_t(region.base) + region.size)
        return false;
    }
  }
  return true;
}

int dispatch(void *opaque, const Nba97GameTextMemory *memory,
             const Nba97GameDisplayEnvironmentEvent *event,
             Nba97GameDisplayEnvironmentMachine *machine) {
  auto &run = *static_cast<Run *>(opaque);
  if (target(event))
    return nba97_game_gpu_control_command_from_display_environment(
        run.binding, memory, event, machine);
  if (!run.fallback)
    return 0;
  int accepted = run.fallback(run.user, memory, event, machine);
  if (accepted == 1)
    ++run.binding->fallback_callbacks_completed;
  return accepted;
}
} // namespace

int nba97_game_gpu_control_command_from_display_environment(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameDisplayEnvironmentEvent *event,
    Nba97GameDisplayEnvironmentMachine *machine) {
  auto *binding = static_cast<Nba97GameGpuControlCommandBinding *>(opaque);
  int index = indexOf(event);
  if (!binding || !memory || !machine || index < 0 ||
      event->delay_slot_pc != event->pc + 4u || event->entry != 0x8009b16cu ||
      event->kind != NBA97_GAME_DISPLAY_ENVIRONMENT_GPU_COMMAND ||
      event->argument_count != 1 || !machineValid(*machine) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15 ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          event->pc + 8u ||
      !memoryValid(*memory) ||
      (!binding->access_journal && binding->access_journal_capacity)) {
    if (binding && index >= 0)
      binding->result[index] = NBA97_TEXT_ARGUMENT;
    return 0;
  }

  ++binding->invocations;
  ++binding->call_count[index];
  binding->event[index] = *event;
  Nba97GameGpuControlCommandContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result[index] =
      nba97_game_gpu_control_command(&context, &binding->progress[index]);
  *machine = binding->progress[index].machine;
  if (binding->result[index] != NBA97_TEXT_COMPLETE)
    return 0;
  ++binding->completions;
  return 1;
}

int nba97_game_display_environment_with_gpu_control_command(
    const Nba97GameDisplayEnvironmentContext *parent,
    Nba97GameGpuControlCommandBinding *binding,
    Nba97GameDisplayEnvironmentProgress *progress) {
  if (!parent || !binding || !progress)
    return NBA97_TEXT_ARGUMENT;
  binding->invocations = 0;
  binding->completions = 0;
  binding->fallback_callbacks_completed = 0;
  std::memset(binding->call_count, 0, sizeof(binding->call_count));
  std::memset(binding->event, 0, sizeof(binding->event));
  std::memset(binding->progress, 0, sizeof(binding->progress));
  std::memset(binding->result, 0, sizeof(binding->result));
  Run run{parent->io, parent->user, binding};
  Nba97GameDisplayEnvironmentContext context = *parent;
  context.io = dispatch;
  context.user = &run;
  int result = nba97_game_display_environment(&context, progress);
  if (result == NBA97_TEXT_IO_REFUSED) {
    int index = -1;
    switch (progress->stopped_pc) {
    case 0x80099d6cu:
      index = NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_99D6C;
      break;
    case 0x80099f78u:
      index = NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_99F78;
      break;
    case 0x80099fa4u:
      index = NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_99FA4;
      break;
    case 0x8009a114u:
      index = NBA97_GAME_GPU_CONTROL_COMMAND_DISPLAY_9A114;
      break;
    default:
      break;
    }
    if (index >= 0 && binding->result[index] != NBA97_TEXT_COMPLETE)
      return binding->result[index];
  }
  return result;
}


int nba97_game_gpu_control_command_from_display_mask(
    const Nba97GameTextMemory *memory, const Nba97GameDisplayMaskSetEvent *event,
    size_t budget, Nba97GameGpuControlCommandProgress *progress,
    Nba97GameDisplayMaskSetValue *value) {
  if (!memory || !event || !progress || !value || !memoryValid(*memory) ||
      event->pc != 0x800994d4u || event->entry != 0x8009b16cu ||
      event->kind != NBA97_GAME_DISPLAY_MASK_GPU_CONTROL ||
      event->argument_count != 1 || event->return_address != 0x800994dcu)
    return NBA97_TEXT_ARGUMENT;
  Nba97GameGpuControlCommandContext c{};
  c.memory=*memory;c.operation_budget=budget;
  c.machine.registers.gpr[0]={0,15};
  c.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]={event->argument[0],15};
  c.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]={event->stack_pointer,15};
  c.machine.registers.gpr[16]={event->saved_register[0],15};
  c.machine.registers.gpr[17]={event->saved_register[1],15};
  c.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]={event->return_address,15};
  const int result=nba97_game_gpu_control_command(&c,progress);
  const auto& returned=progress->machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0];
  *value={returned.word,static_cast<uint8_t>(returned.known_mask==15)};
  return result;
}
