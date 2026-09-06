#include "game_camera_overlay_packets_adapter.h"

#include <cstring>

namespace {

constexpr unsigned kT0 = 8u;
constexpr unsigned kT1 = 9u;

bool valid_machine(const Nba97GameCameraOverlayPacketsMachine &machine) {
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

bool link_call_pc(uint32_t pc) {
  return pc == UINT32_C(0x80075f14) || pc == UINT32_C(0x80076058) ||
         pc == UINT32_C(0x80076070) || pc == UINT32_C(0x80076080) ||
         pc == UINT32_C(0x800760dc) || pc == UINT32_C(0x80076108) ||
         pc == UINT32_C(0x8007624c);
}

struct LinkBridge {
  const Nba97GameTextMemory *memory;
  Nba97GameCameraOverlayPacketsMachine *machine;
  int status;
  bool t1_written;
};

int link_access(void *opaque, uint32_t, uint32_t address, unsigned width,
                int write, Nba97CourtValue *value) {
  LinkBridge &bridge = *static_cast<LinkBridge *>(opaque);
  if (width != 3u) {
    bridge.status = NBA97_TEXT_ARGUMENT;
    return NBA97_COURT_RESOURCE;
  }
  if (write && !bridge.t1_written) {
    const Nba97GameCameraOverlayPacketsWord &a1 =
        bridge.machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1];
    Nba97GameCameraOverlayPacketsWord &t1 = bridge.machine->registers.gpr[kT1];
    t1.word = a1.word << 8u;
    t1.known_mask = static_cast<uint8_t>(((a1.known_mask & 7u) << 1u) | 1u);
    bridge.t1_written = true;
  }
  for (size_t index = 0u; index != bridge.memory->count; ++index) {
    Nba97GameTextRegion &region = bridge.memory->region[index];
    const uint64_t offset = static_cast<uint64_t>(address) - region.base;
    if (address < region.base || offset > region.size ||
        width > region.size - static_cast<size_t>(offset))
      continue;
    uint8_t *data = region.data + static_cast<size_t>(offset);
    uint8_t *known_bytes =
        region.known == nullptr
            ? nullptr
            : region.known + static_cast<size_t>(offset);
    for (unsigned byte = 0u; byte != width; ++byte)
      if (known_bytes != nullptr && known_bytes[byte] > 1u) {
        bridge.status = NBA97_TEXT_ARGUMENT;
        return NBA97_COURT_RESOURCE;
      }
    if (!write) {
      uint32_t word = 0u;
      bool all_known = true;
      for (unsigned byte = 0u; byte != width; ++byte) {
        word |= static_cast<uint32_t>(data[byte]) << (8u * byte);
        if (known_bytes != nullptr && known_bytes[byte] == 0u)
          all_known = false;
      }
      if (!all_known) {
        value->word = 0u;
        value->known = 0u;
        return NBA97_COURT_COMPLETE;
      }
      value->word = word;
      value->known = 1u;
      Nba97GameCameraOverlayPacketsWord &t0 =
          bridge.machine->registers.gpr[kT0];
      t0.word = (t0.word & 0xffu) | (word << 8u);
      t0.known_mask = static_cast<uint8_t>((t0.known_mask & 1u) | 14u);
      const Nba97GameCameraOverlayPacketsWord &a1 =
          bridge.machine->registers.gpr[NBA97_MATCH_INITIALIZE_A1];
      Nba97GameCameraOverlayPacketsWord &t1 =
          bridge.machine->registers.gpr[kT1];
      t1.word = a1.word << 8u;
      t1.known_mask =
          static_cast<uint8_t>(((a1.known_mask & 7u) << 1u) | 1u);
      bridge.t1_written = true;
      return NBA97_COURT_COMPLETE;
    }
    for (unsigned byte = 0u; byte != width; ++byte) {
      data[byte] = static_cast<uint8_t>(value->word >> (8u * byte));
      if (known_bytes != nullptr)
        known_bytes[byte] = 1u;
    }
    return NBA97_COURT_COMPLETE;
  }
  bridge.status = NBA97_TEXT_RESOURCE;
  return NBA97_COURT_RESOURCE;
}

int compose_link(Nba97GameCameraOverlayPacketsChildren &children,
                 const Nba97GameTextMemory &memory,
                 const Nba97GameCameraOverlayPacketsEvent &event,
                 Nba97GameCameraOverlayPacketsMachine &machine) {
  const Nba97GameCameraOverlayPacketsWord &a0 =
      machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0];
  const Nba97GameCameraOverlayPacketsWord &a1 =
      machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1];
  const Nba97GameCameraOverlayPacketsWord &ra =
      machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA];
  children.link_result = NBA97_TEXT_ARGUMENT;
  std::memset(&children.link_progress, 0, sizeof(children.link_progress));
  if (!valid_memory(memory) || !valid_machine(machine) ||
      a0.known_mask != 15u || a1.known_mask != 15u ||
      ra.known_mask != 15u || ra.word != event.pc + 8u ||
      (a0.word & 3u) != 0u || (a1.word & 3u) != 0u)
    return NBA97_TEXT_ARGUMENT;

  LinkBridge bridge{&memory, &machine, NBA97_TEXT_COMPLETE, false};
  Nba97CourtContext context{};
  context.access = link_access;
  context.user = &bridge;
  context.operation_budget = children.link_operation_budget;
  const int result = nba97_game_court_link(&context, a0.word, a1.word,
                                            &children.link_progress);
  if (result != NBA97_COURT_COMPLETE) {
    if (bridge.status != NBA97_TEXT_COMPLETE)
      children.link_result = bridge.status;
    else if (result == NBA97_COURT_UNKNOWN)
      children.link_result = NBA97_TEXT_UNKNOWN;
    else if (result == NBA97_COURT_ALIGNMENT)
      children.link_result = NBA97_TEXT_ALIGNMENT_TRAP;
    else if (result == NBA97_COURT_LIMIT)
      children.link_result = NBA97_TEXT_LIMIT;
    else
      children.link_result = NBA97_TEXT_ARGUMENT;
    return children.link_result;
  }
  children.link_result = NBA97_TEXT_COMPLETE;
  ++children.links_composed;
  return NBA97_TEXT_COMPLETE;
}

} // namespace

extern "C" void nba97_game_camera_overlay_packets_children_init(
    Nba97GameCameraOverlayPacketsChildren *children,
    Nba97GameCameraOverlayPacketsIo fallback, void *fallback_user) {
  if (children == nullptr)
    return;
  std::memset(children, 0, sizeof(*children));
  children->fallback = fallback;
  children->fallback_user = fallback_user;
  children->link_result = NBA97_TEXT_ARGUMENT;
  children->link_operation_budget = 3u;
}

extern "C" int nba97_game_camera_overlay_packets_children_io(
    void *opaque, const Nba97GameTextMemory *memory,
    const Nba97GameCameraOverlayPacketsEvent *event,
    Nba97GameCameraOverlayPacketsMachine *machine) {
  Nba97GameCameraOverlayPacketsChildren *children =
      static_cast<Nba97GameCameraOverlayPacketsChildren *>(opaque);
  if (children == nullptr || memory == nullptr || event == nullptr ||
      machine == nullptr)
    return NBA97_TEXT_ARGUMENT;
  if (event->kind == NBA97_GAME_CAMERA_OVERLAY_PACKETS_LINK_80056914 &&
      event->entry == UINT32_C(0x80056914) && event->argument_count == 2u &&
      link_call_pc(event->pc) && event->delay_slot_pc == event->pc + 4u)
    return compose_link(*children, *memory, *event, *machine);
  if (event->kind == NBA97_GAME_CAMERA_OVERLAY_PACKETS_LINK_80056914 ||
      event->entry == UINT32_C(0x80056914))
    return NBA97_TEXT_ARGUMENT;
  if (children->fallback == nullptr)
    return NBA97_TEXT_IO_REFUSED;
  return children->fallback(children->fallback_user, memory, event, machine);
}

extern "C" void nba97_game_camera_overlay_packets_match_frame_binding_init(
    Nba97GameCameraOverlayPacketsMatchFrameBinding *binding,
    const Nba97GameTextMemory *memory,
    const Nba97GameCameraOverlayPacketsMachine *entry_machine,
    size_t operation_budget, Nba97GameCameraOverlayPacketsIo io, void *user,
    Nba97GameCameraOverlayPacketsAccess *access_journal,
    size_t access_journal_capacity, Nba97MatchFrameIo fallback,
    void *fallback_user) {
  if (binding == nullptr)
    return;
  std::memset(binding, 0, sizeof(*binding));
  if (memory != nullptr)
    binding->memory = *memory;
  if (entry_machine != nullptr)
    binding->entry_machine = *entry_machine;
  binding->operation_budget = operation_budget;
  binding->io = io;
  binding->user = user;
  binding->access_journal = access_journal;
  binding->access_journal_capacity = access_journal_capacity;
  binding->fallback = fallback;
  binding->fallback_user = fallback_user;
  binding->result = NBA97_TEXT_ARGUMENT;
}

extern "C" int nba97_game_camera_overlay_packets_from_match_frame(
    void *opaque, const Nba97MatchFrameCall *call, Nba97GamePeriodValue *value) {
  Nba97GameCameraOverlayPacketsMatchFrameBinding *binding =
      static_cast<Nba97GameCameraOverlayPacketsMatchFrameBinding *>(opaque);
  if (binding == nullptr || call == nullptr)
    return NBA97_BODY_ARGUMENT;
  const bool target_pc = call->pc == UINT32_C(0x800490c8);
  const bool target_entry = call->entry == UINT32_C(0x80075d40);
  if (!target_pc && !target_entry) {
    if (binding->fallback == nullptr)
      return NBA97_MATCH_FRAME_IO_REQUIRED;
    return binding->fallback(binding->fallback_user, call, value);
  }
  std::memset(&binding->progress, 0, sizeof(binding->progress));
  binding->result = NBA97_TEXT_ARGUMENT;
  if (!target_pc || !target_entry || call->args[0] != 0u ||
      call->args[1] != 0u || !valid_machine(binding->entry_machine) ||
      binding->entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
              .known_mask != 15u ||
      binding->entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word !=
          call->pc + 8u ||
      !valid_memory(binding->memory) ||
      (binding->access_journal_capacity != 0u &&
       binding->access_journal == nullptr))
    return NBA97_GAME_CAMERA_OVERLAY_PACKETS_MATCH_FRAME_CHILD_INCOMPLETE;

  Nba97GameCameraOverlayPacketsContext context{};
  context.memory = binding->memory;
  context.operation_budget = binding->operation_budget;
  context.machine = binding->entry_machine;
  context.io = binding->io;
  context.user = binding->user;
  context.access_journal = binding->access_journal;
  context.access_journal_capacity = binding->access_journal_capacity;
  ++binding->invocations;
  binding->result =
      nba97_game_camera_overlay_packets(&context, &binding->progress);
  if (binding->result == NBA97_TEXT_COMPLETE && value != nullptr) {
    value->word = 0u;
    value->known = 0u;
  }
  return binding->result == NBA97_TEXT_COMPLETE
             ? static_cast<int>(NBA97_BODY_OK)
             : NBA97_GAME_CAMERA_OVERLAY_PACKETS_MATCH_FRAME_CHILD_INCOMPLETE;
}
