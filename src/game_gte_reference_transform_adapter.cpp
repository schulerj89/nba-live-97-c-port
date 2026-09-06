#include "game_gte_reference_transform_adapter.h"

#include "game_player_geometry.hpp"

#include <cstring>

namespace {

bool valid_machine(const Nba97GameGteReferenceTransformMachine &machine) {
  if (machine.registers.gpr[0].word != 0u ||
      machine.registers.gpr[0].known_mask != 15u ||
      machine.hi.known_mask > 15u || machine.lo.known_mask > 15u)
    return false;
  for (unsigned index = 0u; index != 32u; ++index)
    if (machine.registers.gpr[index].known_mask > 15u)
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

bool valid_state(const Nba97GameGteReferenceTransformState &state) {
  for (unsigned index = 0u; index != 32u; ++index)
    if (state.control[index].known_mask > 15u ||
        state.data[index].known_mask > 15u)
      return false;
  return true;
}

int32_t signed_half(uint32_t value) {
  value &= UINT32_C(0x0000ffff);
  return value < UINT32_C(0x00008000) ? static_cast<int32_t>(value)
                                      : static_cast<int32_t>(value) - 0x10000;
}

Nba97GamePeriodValue scalar(uint32_t word) { return {word, 1u}; }

} // namespace

extern "C" void nba97_game_gte_reference_transform_geometry_binding_init(
    Nba97GameGteReferenceTransformGeometryBinding *binding, void *geometry) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  binding->geometry = geometry;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_gte_reference_transform_geometry_hardware(
    void *user, const Nba97GameGteReferenceTransformHardwareEvent *event,
    Nba97GameGteReferenceTransformState *state) {
  auto *binding =
      static_cast<Nba97GameGteReferenceTransformGeometryBinding *>(user);
  if (binding == nullptr || event == nullptr || state == nullptr)
    return NBA97_TEXT_ARGUMENT;
  ++binding->invocations;
  binding->result = NBA97_TEXT_ARGUMENT;
  if (binding->geometry == nullptr || event->pc != UINT32_C(0x8005665c) ||
      event->command != UINT32_C(0x00480012) || !valid_state(*state))
    return binding->result;

  for (unsigned index = 0u; index != 4u; ++index)
    if (state->control[index].known_mask != 15u)
      return binding->result = NBA97_TEXT_UNKNOWN;
  if ((state->control[4].known_mask & 3u) != 3u)
    return binding->result = NBA97_TEXT_UNKNOWN;
  for (unsigned index = 5u; index != 8u; ++index)
    if (state->control[index].known_mask != 15u)
      return binding->result = NBA97_TEXT_UNKNOWN;
  if (state->data[0].known_mask != 15u ||
      (state->data[1].known_mask & 3u) != 3u)
    return binding->result = NBA97_TEXT_UNKNOWN;

  auto *geometry = static_cast<nba97::GamePlayerGeometry *>(binding->geometry);
  nba97::GamePlayerGeometry working = *geometry;
  for (unsigned index = 0u; index != 4u; ++index)
    working.rotation[index] = scalar(state->control[index].word);
  working.rotation[4] =
      scalar(static_cast<uint32_t>(signed_half(state->control[4].word)));
  for (unsigned index = 0u; index != 3u; ++index)
    working.translation[index] = scalar(state->control[index + 5u].word);
  working.vertex[0] = scalar(state->data[0].word);
  working.vertex[1] =
      scalar(static_cast<uint32_t>(signed_half(state->data[1].word)));
  for (auto &value : working.ir)
    value = {0u, 0u};
  for (auto &value : working.mac)
    value = {0u, 0u};
  working.flags = {0u, 0u};

  Nba97PlayerMathRequest request{};
  request.pc = UINT32_C(0x8005665c);
  request.kind = NBA97_PLAYER_TRANSFORM;
  Nba97GamePeriodValue ignored{};
  const int result = working.apply(request, ignored);
  if (result == NBA97_BODY_UNKNOWN)
    return binding->result = NBA97_TEXT_UNKNOWN;
  if (result != NBA97_BODY_OK)
    return binding->result = NBA97_TEXT_ARGUMENT;

  for (unsigned index = 0u; index != 3u; ++index) {
    state->data[index + 25u] = {
        working.mac[index].word,
        static_cast<uint8_t>(working.mac[index].known ? 15u : 0u)};
    state->data[index + 9u] = {
        working.ir[index].word,
        static_cast<uint8_t>(working.ir[index].known ? 15u : 0u)};
  }
  state->control[31] = {working.flags.word,
                        static_cast<uint8_t>(working.flags.known ? 15u : 0u)};
  *geometry = working;
  binding->result = NBA97_TEXT_COMPLETE;
  return binding->result;
}

extern "C" void nba97_game_gte_reference_transform_camera_binding_init(
    Nba97GameGteReferenceTransformCameraBinding *binding,
    const Nba97GameGteReferenceTransformState *state, size_t operation_budget,
    Nba97GameGteReferenceTransformHardware hardware, void *hardware_user,
    Nba97GameGteReferenceTransformAccess *access_journal,
    size_t access_journal_capacity, Nba97GameCameraFrameTransformIo fallback,
    void *fallback_user) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  if (state != nullptr)
    binding->state = *state;
  binding->operation_budget = operation_budget;
  binding->hardware = hardware;
  binding->hardware_user = hardware_user;
  binding->access_journal = access_journal;
  binding->access_journal_capacity = access_journal_capacity;
  binding->fallback = fallback;
  binding->fallback_user = fallback_user;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_gte_reference_transform_from_camera_frame(
    void *user, const Nba97GameTextMemory *memory,
    const Nba97GameCameraFrameTransformEvent *event,
    Nba97GameCameraFrameTransformMachine *machine) {
  auto *binding =
      static_cast<Nba97GameGteReferenceTransformCameraBinding *>(user);
  if (binding == nullptr || memory == nullptr || event == nullptr ||
      machine == nullptr)
    return 0;

  const bool reference_kind =
      event->kind == NBA97_GAME_CAMERA_FRAME_TRANSFORM_REFERENCE_80056650;
  const bool reference_entry = event->entry == UINT32_C(0x80056650);
  if (!reference_kind && !reference_entry) {
    ++binding->fallback_invocations;
    if (binding->fallback == nullptr) {
      binding->result = NBA97_TEXT_IO_REFUSED;
      return 0;
    }
    return binding->fallback(binding->fallback_user, memory, event, machine);
  }

  ++binding->invocations;
  std::memset(&binding->progress, 0, sizeof(binding->progress));
  binding->result = NBA97_TEXT_ARGUMENT;
  if (!reference_kind || !reference_entry ||
      event->pc != UINT32_C(0x80051228) ||
      event->delay_slot_pc != UINT32_C(0x8005122c) ||
      event->argument_count != 3u || !valid_machine(*machine) ||
      !valid_memory(*memory) || !valid_state(binding->state) ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask != 15u ||
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          UINT32_C(0x80051230) ||
      (binding->access_journal_capacity != 0u &&
       binding->access_journal == nullptr))
    return 0;

  Nba97GameGteReferenceTransformContext context{};
  context.memory = *memory;
  context.operation_budget = binding->operation_budget;
  context.machine = *machine;
  context.state = binding->state;
  context.hardware = binding->hardware;
  context.hardware_user = binding->hardware_user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  binding->result =
      nba97_game_gte_reference_transform(&context, &binding->progress);
  *machine = binding->progress.machine;
  binding->state = binding->progress.state;
  return binding->result == NBA97_TEXT_COMPLETE ? 1 : 0;
}
