#include "game_gte_translation_install_adapter.h"

#include <cstring>

extern "C" void nba97_game_gte_translation_install_camera_binding_init(
    Nba97GameGteTranslationInstallCameraBinding *binding,
    const Nba97GameGteTranslationInstallWord *initial_control,
    size_t operation_budget,
    Nba97GameGteTranslationInstallAccess *access_journal,
    size_t access_journal_capacity,
    Nba97GameGteTranslationInstallControlWrite *control_journal,
    size_t control_journal_capacity, Nba97GameCameraFrameTransformIo fallback,
    void *fallback_user) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  if (initial_control != nullptr)
    std::memcpy(binding->control, initial_control, sizeof(binding->control));
  binding->operation_budget = operation_budget;
  binding->access_journal = access_journal;
  binding->access_journal_capacity = access_journal_capacity;
  binding->control_journal = control_journal;
  binding->control_journal_capacity = control_journal_capacity;
  binding->fallback = fallback;
  binding->fallback_user = fallback_user;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_gte_translation_install_from_camera(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameCameraFrameTransformEvent *event,
    Nba97GameCameraFrameTransformMachine *machine) {
  Nba97GameGteTranslationInstallCameraBinding *binding =
      static_cast<Nba97GameGteTranslationInstallCameraBinding *>(opaque);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      machine == nullptr)
    return NBA97_TEXT_ARGUMENT;

  const bool target_kind =
      event->kind == NBA97_GAME_CAMERA_FRAME_TRANSFORM_SET_TRANSLATION_80055F44;
  const bool target_entry = event->entry == UINT32_C(0x80055f44);
  if (!target_kind && !target_entry) {
    if (binding->fallback == nullptr)
      return NBA97_TEXT_IO_REFUSED;
    return binding->fallback(binding->fallback_user, memory, event, machine);
  }

  std::memset(&binding->progress, 0, sizeof(binding->progress));
  binding->result = NBA97_TEXT_ARGUMENT;
  const Nba97GameGteTranslationInstallWord &ra =
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA];
  if (!target_kind || !target_entry || event->pc != UINT32_C(0x8005120c) ||
      event->delay_slot_pc != UINT32_C(0x80051210) ||
      event->argument_count != 1u || ra.known_mask != 15u ||
      ra.word != UINT32_C(0x80051214))
    return NBA97_TEXT_ARGUMENT;

  Nba97GameGteTranslationInstallContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.control = binding->control;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  context.control_journal = binding->control_journal;
  context.control_journal_capacity = binding->control_journal_capacity;
  ++binding->invocations;
  binding->result =
      nba97_game_gte_translation_install(&context, &binding->progress);
  *machine = binding->progress.machine;
  return binding->result;
}
