#include "game_camera_overlay_packets_adapter.h"
#include "game_camera_overlay_packets_capture.h"
#include <sstream>
#include <stdexcept>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr uint32_t kBase = UINT32_C(0x80000000);
constexpr uint32_t kStack = UINT32_C(0x801ff000);

void put8(std::vector<uint8_t> &ram, uint32_t address, uint8_t value) {
  ram[address - kBase] = value;
}
void put16(std::vector<uint8_t> &ram, uint32_t address, uint16_t value) {
  put8(ram, address, static_cast<uint8_t>(value));
  put8(ram, address + 1u, static_cast<uint8_t>(value >> 8u));
}
void put32(std::vector<uint8_t> &ram, uint32_t address, uint32_t value) {
  for (unsigned byte = 0u; byte != 4u; ++byte)
    put8(ram, address + byte, static_cast<uint8_t>(value >> (8u * byte)));
}
uint32_t get24(const std::vector<uint8_t> &ram, uint32_t address) {
  return static_cast<uint32_t>(ram[address - kBase]) |
         (static_cast<uint32_t>(ram[address - kBase + 1u]) << 8u) |
         (static_cast<uint32_t>(ram[address - kBase + 2u]) << 16u);
}
void set_word(Nba97GameCameraOverlayPacketsWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

struct Natural {
  std::vector<uint8_t> ram = std::vector<uint8_t>(UINT32_C(0x200000), 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(UINT32_C(0x200000), 1u);
  Nba97GameTextRegion region{};
  Nba97GameTextMemory memory{};
  Nba97GameCameraOverlayPacketsMachine machine{};
  Nba97GameCameraOverlayPacketsChildren children{};
  Nba97GameCameraOverlayPacketsMatchFrameBinding binding{};
  Nba97MatchFrameContext frame{};
  Nba97MatchFrameProgress frame_progress{};
  uint32_t table_address = 0u, table_before = 0u;
  size_t gte_calls = 0u;
  size_t fallback_calls = 0u;

  Natural() {
    region.base = kBase;
    region.data = ram.data();
    region.known = known.data();
    region.size = ram.size();
    memory.region = &region;
    memory.count = 1u;
    for (unsigned reg = 0u; reg != 32u; ++reg)
      set_word(machine.registers.gpr[reg], UINT32_C(0x50000000) + reg);
    set_word(machine.registers.gpr[0], 0u);
    set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP], kStack);
    set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
             UINT32_C(0x800490d0));
    set_word(machine.hi, UINT32_C(0x11112222));
    set_word(machine.lo, UINT32_C(0x33334444));
    put16(ram, UINT32_C(0x800fe8cc), 3u);
    put8(ram, UINT32_C(0x800bc1f0), 1u);
    put32(ram, UINT32_C(0x80020bec), UINT32_C(0x80030000));
    put16(ram, UINT32_C(0x80030004), 0u);
    put16(ram, UINT32_C(0x800fe8d8), 56u);
    put16(ram, UINT32_C(0x800fe8da), 56u);
    put16(ram, UINT32_C(0x800f9ffe), 0u);
    put32(ram, UINT32_C(0x80102924), UINT32_C(0x80104000));
    put32(ram, UINT32_C(0x80104000), UINT32_C(0xaa123456));
    put32(ram, UINT32_C(0x800fa25c), UINT32_C(0xbb654321));
    put32(ram, UINT32_C(0x800fa284), UINT32_C(0xccabcdef));
    put32(ram, UINT32_C(0x8001ede8), 0u);
    put32(ram, UINT32_C(0x800b729c), 320u);
    nba97_game_camera_overlay_packets_children_init(&children, gte, this);
    nba97_game_camera_overlay_packets_match_frame_binding_init(
        &binding, &memory, &machine, 1000u,
        nba97_game_camera_overlay_packets_children_io, &children, nullptr, 0u,
        fallback, this);
    frame.access = match_access;
    frame.io = match_io;
    frame.user = this;
    frame.operation_budget = 17u;
  }

  static int gte(void *opaque, const Nba97GameTextMemory *,
                 const Nba97GameCameraOverlayPacketsEvent *,
                 Nba97GameCameraOverlayPacketsMachine *) {
    ++static_cast<Natural *>(opaque)->gte_calls;
    return NBA97_TEXT_COMPLETE;
  }

  static int fallback(void *opaque, const Nba97MatchFrameCall *call,
                      Nba97GamePeriodValue *value) {
    Natural &self = *static_cast<Natural *>(opaque);
    ++self.fallback_calls;
    if (call->entry == UINT32_C(0x80048ff4)) {
      value->word = 0u;
      value->known = 1u;
    }
    return NBA97_BODY_OK;
  }

  static int match_io(void *opaque, const Nba97MatchFrameCall *call,
                      Nba97GamePeriodValue *value) {
    Natural &self = *static_cast<Natural *>(opaque);
    if(call->pc == 0x800490c8) {
      self.table_address=get24(self.ram,0x80102924) | (uint32_t(self.ram[0x102927])<<24);
      self.table_before=get24(self.ram,self.table_address);
    }
    return nba97_game_camera_overlay_packets_from_match_frame(&self.binding,
                                                              call, value);
  }

  static int match_access(void *opaque, uint32_t, uint32_t address,
                          unsigned width, unsigned kind,
                          Nba97PlayerFrameValue *value) {
    Natural &self = *static_cast<Natural *>(opaque);
    if (address < kBase ||
        static_cast<uint64_t>(address - kBase) + width > self.ram.size())
      return NBA97_BODY_BOUNDS;
    const size_t offset = address - kBase;
    if (kind == NBA97_FRAME_READ) {
      std::memset(value, 0, sizeof(*value));
      for (unsigned byte = 0u; byte != width; ++byte) {
        value->word |= static_cast<uint32_t>(self.ram[offset + byte])
                       << (8u * byte);
        if (self.known[offset + byte])
          value->known_mask =
              static_cast<uint8_t>(value->known_mask | (1u << byte));
      }
    } else {
      for (unsigned byte = 0u; byte != width; ++byte) {
        self.ram[offset + byte] =
            static_cast<uint8_t>(value->word >> (8u * byte));
        self.known[offset + byte] =
            static_cast<uint8_t>((value->known_mask >> byte) & 1u);
      }
    }
    return NBA97_BODY_OK;
  }
};

} // namespace
namespace nba97 {
std::string captureGameCameraOverlayPackets() {
  Natural n;
  const int result=nba97_game_match_frame(&n.frame,&n.frame_progress);
  if(result!=NBA97_BODY_JOURNAL_LIMIT || !n.binding.progress.completed ||
      n.children.links_composed!=2 || n.frame_progress.stopped_pc!=0x800490e8)
    throw std::runtime_error("camera overlay native caller prefix drifted");
  const auto& q=n.binding.progress;
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x80075D40\",\"inclusive_end\":\"0x80076273\","
       "\"bytes\":1332,\"instructions\":333,\"classification\":\"no direct visual effect\","
       "\"scope\":\"actual match-frame prefix with independent full machine and recovered packet linker; typed other services\","
       "\"completed\":true,\"frame_completed\":false,\"frame_stopped_pc\":" << n.frame_progress.stopped_pc
    << ",\"links\":" << n.children.links_composed << ",\"operations\":" << q.operations
    << ",\"reads\":" << q.reads << ",\"stores\":" << q.stores << ",\"callbacks\":" << q.callbacks_completed
    << ",\"table_address\":" << n.table_address << ",\"table_before\":" << n.table_before << ",\"packet_before\":[6636321,11259375],\"table_after\":" << get24(n.ram,n.table_address)
    << ",\"packet_after\":[" << get24(n.ram,0x800fa25c) << ',' << get24(n.ram,0x800fa284)
    << "],\"returned_sp\":" << q.machine.registers.gpr[29].word
    << ",\"restored_ra\":" << q.restored_return_address.word << "}";
  return o.str();
}
}
