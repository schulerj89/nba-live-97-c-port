#include "game_video_mode_adapter.h"

#include <cstring>

namespace {

int site_index(const Nba97GameDisplayEnvironmentEvent *event) {
  if (event == nullptr)
    return -1;
  if (event->pc == UINT32_C(0x80099de8))
    return NBA97_GAME_VIDEO_MODE_DISPLAY_RECTANGLE;
  if (event->pc == UINT32_C(0x8009a034))
    return NBA97_GAME_VIDEO_MODE_DISPLAY_MODE;
  return -1;
}

bool valid_word(const Nba97GameDisplayEnvironmentWord &word) {
  return word.known_mask <= 15u;
}

bool valid_machine(const Nba97GameDisplayEnvironmentMachine &machine) {
  if (machine.registers.gpr[0].word != 0u ||
      machine.registers.gpr[0].known_mask != 15u || !valid_word(machine.hi) ||
      !valid_word(machine.lo))
    return false;
  for (unsigned index = 0u; index != 32u; ++index)
    if (!valid_word(machine.registers.gpr[index]))
      return false;
  return true;
}

bool valid_memory(const Nba97GameTextMemory &memory) {
  if (memory.count != 0u && memory.region == nullptr)
    return false;
  for (size_t index = 0u; index != memory.count; ++index) {
    const Nba97GameTextRegion &region = memory.region[index];
    if (region.data == nullptr || region.size == 0u ||
        region.size > UINT64_C(0x100000000) ||
        static_cast<uint64_t>(region.base) + region.size >
            UINT64_C(0x100000000))
      return false;
    for (size_t earlier = 0u; earlier != index; ++earlier) {
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

void copy_to_owner(const Nba97GameDisplayEnvironmentMachine &source,
                   Nba97GameVideoModeMachine &destination) {
  for (unsigned index = 0u; index != 32u; ++index) {
    destination.registers.gpr[index].word = source.registers.gpr[index].word;
    destination.registers.gpr[index].known_mask =
        source.registers.gpr[index].known_mask;
  }
  destination.hi.word = source.hi.word;
  destination.hi.known_mask = source.hi.known_mask;
  destination.lo.word = source.lo.word;
  destination.lo.known_mask = source.lo.known_mask;
}

void copy_from_owner(const Nba97GameVideoModeMachine &source,
                     Nba97GameDisplayEnvironmentMachine &destination) {
  for (unsigned index = 0u; index != 32u; ++index) {
    destination.registers.gpr[index].word = source.registers.gpr[index].word;
    destination.registers.gpr[index].known_mask =
        source.registers.gpr[index].known_mask;
  }
  destination.hi.word = source.hi.word;
  destination.hi.known_mask = source.hi.known_mask;
  destination.lo.word = source.lo.word;
  destination.lo.known_mask = source.lo.known_mask;
}

} // namespace

extern "C" void nba97_game_video_mode_display_binding_init(
    Nba97GameVideoModeDisplayBinding *binding,
    const Nba97GameVideoModeCallConfig *config,
    Nba97GameDisplayEnvironmentIo fallback, void *fallback_user) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  if (config != nullptr)
    for (unsigned index = 0u; index != NBA97_GAME_VIDEO_MODE_DISPLAY_SITE_COUNT;
         ++index)
      binding->config[index] = config[index];
  binding->fallback = fallback;
  binding->fallback_user = fallback_user;
  for (unsigned index = 0u; index != NBA97_GAME_VIDEO_MODE_DISPLAY_SITE_COUNT;
       ++index)
    binding->result[index] = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_video_mode_from_display(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameDisplayEnvironmentEvent *event,
    Nba97GameDisplayEnvironmentMachine *machine) {
  Nba97GameVideoModeDisplayBinding *binding =
      static_cast<Nba97GameVideoModeDisplayBinding *>(opaque);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      machine == nullptr)
    return 0;
  const bool target_kind =
      event->kind == NBA97_GAME_DISPLAY_ENVIRONMENT_VIDEO_MODE;
  const bool target_entry = event->entry == UINT32_C(0x800985cc);
  if (!target_kind && !target_entry) {
    if (binding->fallback == nullptr)
      return 0;
    return binding->fallback(binding->fallback_user, memory, event, machine);
  }

  const int site = site_index(event);
  if (site < 0)
    return 0;
  Nba97GameVideoModeCallConfig &config = binding->config[site];
  binding->result[site] = NBA97_TEXT_ARGUMENT;
  if (!target_kind || !target_entry || event->delay_slot_pc != event->pc + 4u ||
      event->argument_count != 0u || !valid_machine(*machine) ||
      !valid_memory(*memory) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15u ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          event->pc + 8u ||
      (config.access_journal_capacity != 0u &&
       config.access_journal == nullptr))
    return 0;

  Nba97GameVideoModeContext context{};
  context.memory = *memory;
  context.operation_budget = config.operation_budget;
  copy_to_owner(*machine, context.machine);
  context.access_journal = config.access_journal;
  context.access_journal_capacity = config.access_journal_capacity;
  ++binding->invocations;
  ++binding->call_count[site];
  binding->event[site] = *event;
  binding->result[site] =
      nba97_game_video_mode(&context, &binding->progress[site]);
  copy_from_owner(binding->progress[site].machine, *machine);
  if (binding->result[site] == NBA97_TEXT_COMPLETE) {
    ++binding->completions;
    return 1;
  }
  return 0;
}
