#include "game_bios_memory_copy_adapter.h"

#include <cstring>

namespace {

bool valid_speech_registers(const Nba97GameSpeechInitializeRegisters &value) {
  if (value.gpr[0].word != 0u || value.gpr[0].known_mask != 15u)
    return false;
  for (unsigned index = 0u; index != 32u; ++index)
    if (value.gpr[index].known_mask > 15u)
      return false;
  return true;
}

} // namespace

extern "C" void nba97_game_bios_memory_copy_speech_binding_init(
    Nba97GameBiosMemoryCopySpeechBinding *binding, size_t operation_budget,
    Nba97GameBiosMemoryCopyIo bios_io, void *bios_user,
    Nba97GameBiosMemoryCopyHiLoProvider hilo_provider, void *hilo_user,
    Nba97GameSpeechInitializeIo fallback, void *fallback_user) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->operation_budget = operation_budget;
  binding->bios_io = bios_io;
  binding->bios_user = bios_user;
  binding->hilo_provider = hilo_provider;
  binding->hilo_user = hilo_user;
  binding->fallback = fallback;
  binding->fallback_user = fallback_user;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_bios_memory_copy_from_speech(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameSpeechInitializeEvent *event,
    Nba97GameSpeechInitializeRegisters *registers) {
  Nba97GameBiosMemoryCopySpeechBinding *binding =
      static_cast<Nba97GameBiosMemoryCopySpeechBinding *>(opaque);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      registers == nullptr)
    return 0;

  const bool target_kind =
      event->kind == NBA97_GAME_SPEECH_INITIALIZE_COPY_8009CB0C;
  const bool target_entry = event->entry == UINT32_C(0x8009cb0c);
  if (!target_kind && !target_entry) {
    if (binding->fallback == nullptr)
      return 0;
    return binding->fallback(binding->fallback_user, memory, event, registers);
  }

  std::memset(&binding->progress, 0, sizeof(binding->progress));
  binding->result = NBA97_TEXT_ARGUMENT;
  const Nba97GameSpeechInitializeWord &ra =
      registers->gpr[NBA97_MATCH_INITIALIZE_RA];
  if (!target_kind || !target_entry || event->pc != UINT32_C(0x8008008c) ||
      event->delay_slot_pc != UINT32_C(0x80080090) ||
      event->argument_count != 3u || ra.known_mask != 15u ||
      ra.word != UINT32_C(0x80080094))
    return 0;

  Nba97GameBiosMemoryCopyMachine machine{};
  machine.registers = *registers;
  if (binding->hilo_provider != nullptr) {
    ++binding->provider_invocations;
    if (binding->hilo_provider(binding->hilo_user, event, registers,
                               &machine.hi, &machine.lo) != 1) {
      binding->result = NBA97_TEXT_IO_REFUSED;
      return 0;
    }
  }

  Nba97GameBiosMemoryCopyContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = machine;
  context.io = binding->bios_io;
  context.user = binding->bios_user;
  ++binding->invocations;
  binding->result = nba97_game_bios_memory_copy(&context, &binding->progress);
  *registers = binding->progress.machine.registers;
  if (binding->result == NBA97_TEXT_COMPLETE)
    return 1;
  if (binding->result == NBA97_TEXT_ARGUMENT &&
      !valid_speech_registers(*registers))
    return 1;
  return 0;
}

extern "C" int nba97_game_bios_memory_copy_with_speech(
    const Nba97GameSpeechInitializeContext *context,
    Nba97GameBiosMemoryCopySpeechBinding *binding,
    Nba97GameSpeechInitializeProgress *progress) {
  if (context == nullptr || binding == nullptr || progress == nullptr)
    return NBA97_TEXT_ARGUMENT;
  Nba97GameSpeechInitializeContext composed = *context;
  const size_t invocations_before = binding->invocations;
  composed.io = nba97_game_bios_memory_copy_from_speech;
  composed.user = binding;
  const int result = nba97_game_speech_initialize(&composed, progress);
  if (result == NBA97_TEXT_IO_REFUSED &&
      binding->invocations != invocations_before &&
      binding->result == NBA97_TEXT_ARGUMENT)
    return NBA97_TEXT_ARGUMENT;
  return result;
}
