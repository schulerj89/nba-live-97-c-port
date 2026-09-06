#include "game_period_audio_flag_clear.h"

#include <string.h>

#define FLAG_ADDRESS UINT32_C(0x800b1fd5)

static int valid_word(Nba97GamePeriodAudioFlagClearWord value) {
  return value.known_mask <= 15u;
}

static int valid_machine(const Nba97GamePeriodAudioFlagClearMachine *machine) {
  unsigned index;
  if (machine->registers.gpr[0].word != 0u ||
      machine->registers.gpr[0].known_mask != 15u || !valid_word(machine->hi) ||
      !valid_word(machine->lo))
    return 0;
  for (index = 0u; index != 32u; ++index)
    if (!valid_word(machine->registers.gpr[index]))
      return 0;
  return 1;
}

static int valid_memory(const Nba97GameTextMemory *memory) {
  size_t index;
  size_t earlier;
  if (memory->count != 0u && memory->region == NULL)
    return 0;
  for (index = 0u; index != memory->count; ++index) {
    const Nba97GameTextRegion *region = &memory->region[index];
    if (region->data == NULL || region->size == 0u ||
        (uint64_t)region->size > UINT64_C(0x100000000) ||
        (uint64_t)region->base + (uint64_t)region->size > UINT64_C(0x100000000))
      return 0;
    for (earlier = 0u; earlier != index; ++earlier) {
      const Nba97GameTextRegion *other = &memory->region[earlier];
      if ((uint64_t)region->base < (uint64_t)other->base + other->size &&
          (uint64_t)other->base < (uint64_t)region->base + region->size)
        return 0;
    }
  }
  return 1;
}

static void publish(Nba97GamePeriodAudioFlagClearProgress *progress,
                    const Nba97GamePeriodAudioFlagClearMachine *machine) {
  progress->machine = *machine;
}

static void stop(Nba97GamePeriodAudioFlagClearProgress *progress,
                 const Nba97GamePeriodAudioFlagClearMachine *machine,
                 uint32_t pc, uint32_t address) {
  progress->stopped_pc = pc;
  progress->stopped_address = address;
  publish(progress, machine);
}

static int store_flag(Nba97GamePeriodAudioFlagClearContext *context,
                      Nba97GamePeriodAudioFlagClearProgress *progress,
                      Nba97GamePeriodAudioFlagClearMachine *machine) {
  size_t index;
  stop(progress, machine, UINT32_C(0x8002a248), FLAG_ADDRESS);
  if (progress->operations >= context->operation_budget)
    return NBA97_TEXT_LIMIT;
  ++progress->operations;
  ++progress->accesses;
  for (index = 0u; index != context->memory.count; ++index) {
    Nba97GameTextRegion *region = &context->memory.region[index];
    uint64_t offset = (uint64_t)FLAG_ADDRESS - region->base;
    if (FLAG_ADDRESS < region->base || offset >= region->size)
      continue;
    if (region->known != NULL && region->known[(size_t)offset] > 1u)
      return NBA97_TEXT_ARGUMENT;
    region->data[(size_t)offset] = 0u;
    if (region->known != NULL)
      region->known[(size_t)offset] = 1u;
    ++progress->stores;
    if (progress->access_events < context->access_journal_capacity) {
      Nba97GamePeriodAudioFlagClearAccess *event =
          &context->access_journal[progress->access_events];
      event->pc = UINT32_C(0x8002a248);
      event->address = FLAG_ADDRESS;
      event->value = 0u;
      event->operation = progress->operations;
      event->width = 1u;
      event->known_mask = 1u;
      event->kind = NBA97_GAME_PERIOD_AUDIO_FLAG_CLEAR_STORE;
    }
    ++progress->access_events;
    publish(progress, machine);
    return NBA97_TEXT_COMPLETE;
  }
  return NBA97_TEXT_RESOURCE;
}

int nba97_game_period_audio_flag_clear(
    Nba97GamePeriodAudioFlagClearContext *context,
    Nba97GamePeriodAudioFlagClearProgress *progress) {
  Nba97GamePeriodAudioFlagClearMachine machine;
  int result;
  if (progress == NULL)
    return NBA97_TEXT_ARGUMENT;
  memset(progress, 0, sizeof(*progress));
  if (context == NULL)
    return NBA97_TEXT_ARGUMENT;
  machine = context->machine;
  publish(progress, &machine);
  if (!valid_machine(&machine) || !valid_memory(&context->memory) ||
      (context->access_journal_capacity != 0u &&
       context->access_journal == NULL))
    return NBA97_TEXT_ARGUMENT;

  /* 0x8002A244..0x8002A248: LUI establishes the exact AT prefix before the
   * single bounded store attempts to clear the retained flag. */
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT].word = UINT32_C(0x800b0000);
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_AT].known_mask = 15u;
  ++progress->instruction_count;
  ++progress->instruction_count;
  result = store_flag(context, progress, &machine);
  if (result != NBA97_TEXT_COMPLETE)
    return result;

  /* 0x8002A24C..0x8002A250: execute the JR and its NOP before requiring the
   * live return address to be fully known and aligned. */
  progress->instruction_count += 2u;
  if (machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15u) {
    stop(progress, &machine, UINT32_C(0x8002a24c), 0u);
    return NBA97_TEXT_UNKNOWN;
  }
  if ((machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word & 3u) != 0u) {
    stop(progress, &machine, UINT32_C(0x8002a24c),
         machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word);
    return NBA97_TEXT_ALIGNMENT_TRAP;
  }
  progress->completed = 1u;
  stop(progress, &machine, 0u, 0u);
  return NBA97_TEXT_COMPLETE;
}
