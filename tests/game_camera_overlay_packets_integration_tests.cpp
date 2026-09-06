#include "game_camera_overlay_packets_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void check(bool condition, const char *expression, int line) {
  if (condition)
    return;
  std::fprintf(stderr, "check failed at line %d: %s\n", line, expression);
  std::exit(1);
}
#define CHECK(expression) check((expression), #expression, __LINE__)

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

void test_actual_link_machine_and_aliases() {
  Natural natural;
  Nba97GameCameraOverlayPacketsEvent event{};
  event.pc = UINT32_C(0x80076070);
  event.delay_slot_pc = event.pc + 4u;
  event.entry = UINT32_C(0x80056914);
  event.kind = NBA97_GAME_CAMERA_OVERLAY_PACKETS_LINK_80056914;
  event.argument_count = 2u;
  Nba97GameCameraOverlayPacketsMachine machine = natural.machine;
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           UINT32_C(0x80104000));
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
           UINT32_C(0x800fa25c));
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA], event.pc + 8u);
  set_word(machine.registers.gpr[8], UINT32_C(0xdeadbeef), 1u);
  Nba97GameCameraOverlayPacketsMachine before = machine;
  CHECK(nba97_game_camera_overlay_packets_children_io(
            &natural.children, &natural.memory, &event, &machine) ==
        NBA97_TEXT_COMPLETE);
  CHECK(get24(natural.ram, UINT32_C(0x800fa25c)) == UINT32_C(0x123456));
  CHECK(get24(natural.ram, UINT32_C(0x80104000)) == UINT32_C(0x0fa25c));
  CHECK(machine.registers.gpr[8].word == UINT32_C(0x123456ef));
  CHECK(machine.registers.gpr[8].known_mask == 15u);
  CHECK(machine.registers.gpr[9].word == UINT32_C(0x0fa25c00));
  for (unsigned reg = 0u; reg != 32u; ++reg)
    if (reg != 8u && reg != 9u)
      CHECK(std::memcmp(&machine.registers.gpr[reg],
                        &before.registers.gpr[reg],
                        sizeof(machine.registers.gpr[reg])) == 0);
  CHECK(machine.hi.word == before.hi.word && machine.lo.word == before.lo.word);

  put32(natural.ram, UINT32_C(0x80104000), UINT32_C(0xab112233));
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           UINT32_C(0x80104000));
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
           UINT32_C(0x80104000));
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA], event.pc + 8u);
  CHECK(nba97_game_camera_overlay_packets_children_io(
            &natural.children, &natural.memory, &event, &machine) ==
        NBA97_TEXT_COMPLETE);
  CHECK(get24(natural.ram, UINT32_C(0x80104000)) == UINT32_C(0x104000));
}

void test_link_refusal_prefixes() {
  Natural natural;
  Nba97GameCameraOverlayPacketsEvent event{};
  event.pc = UINT32_C(0x80076070);
  event.delay_slot_pc = event.pc + 4u;
  event.entry = UINT32_C(0x80056914);
  event.kind = NBA97_GAME_CAMERA_OVERLAY_PACKETS_LINK_80056914;
  event.argument_count = 2u;
  Nba97GameCameraOverlayPacketsMachine machine = natural.machine;
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           UINT32_C(0x80104000));
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
           UINT32_C(0x800fa25c));
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA], event.pc + 8u);
  natural.known[UINT32_C(0x104000)] = 0u;
  CHECK(nba97_game_camera_overlay_packets_children_io(
            &natural.children, &natural.memory, &event, &machine) ==
        NBA97_TEXT_UNKNOWN);
  CHECK(machine.registers.gpr[8].word == natural.machine.registers.gpr[8].word);

  natural.known[UINT32_C(0x104000)] = 1u;
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
           UINT32_C(0x800fa25d));
  CHECK(nba97_game_camera_overlay_packets_children_io(
            &natural.children, &natural.memory, &event, &machine) ==
        NBA97_TEXT_ARGUMENT);

  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
           UINT32_C(0x800fa25c));
  machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7u;
  CHECK(nba97_game_camera_overlay_packets_children_io(
            &natural.children, &natural.memory, &event, &machine) ==
        NBA97_TEXT_ARGUMENT);

  Nba97GameCameraOverlayPacketsMachine unchanged = natural.machine;
  set_word(unchanged.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
           UINT32_C(0x80104000));
  set_word(unchanged.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
           UINT32_C(0x800fa25c));
  set_word(unchanged.registers.gpr[NBA97_MATCH_INITIALIZE_RA], event.pc + 8u);
  Nba97GameCameraOverlayPacketsMachine snapshot = unchanged;
  event.delay_slot_pc ^= 4u;
  CHECK(nba97_game_camera_overlay_packets_children_io(
            &natural.children, &natural.memory, &event, &unchanged) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(std::memcmp(&unchanged, &snapshot, sizeof(unchanged)) == 0);
  event.delay_slot_pc = event.pc + 4u;
  event.pc = UINT32_C(0x80076074);
  event.delay_slot_pc = event.pc + 4u;
  CHECK(nba97_game_camera_overlay_packets_children_io(
            &natural.children, &natural.memory, &event, &unchanged) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(std::memcmp(&unchanged, &snapshot, sizeof(unchanged)) == 0);
  event.pc = UINT32_C(0x80076070);
  event.delay_slot_pc = event.pc + 4u;

  for (size_t budget = 0u; budget != 3u; ++budget) {
    Natural bounded;
    bounded.children.link_operation_budget = budget;
    Nba97GameCameraOverlayPacketsMachine prefix = bounded.machine;
    set_word(prefix.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
             UINT32_C(0x80104000));
    set_word(prefix.registers.gpr[NBA97_MATCH_INITIALIZE_A1],
             UINT32_C(0x800fa25c));
    set_word(prefix.registers.gpr[NBA97_MATCH_INITIALIZE_RA], event.pc + 8u);
    set_word(prefix.registers.gpr[8], UINT32_C(0xdeadbeef), 1u);
    CHECK(nba97_game_camera_overlay_packets_children_io(
              &bounded.children, &bounded.memory, &event, &prefix) ==
          NBA97_TEXT_LIMIT);
    CHECK(bounded.children.link_progress.operations == budget);
    CHECK(prefix.registers.gpr[8].word ==
          (budget == 0u ? UINT32_C(0xdeadbeef)
                        : UINT32_C(0x123456ef)));
    CHECK(prefix.registers.gpr[9].word ==
          (budget == 0u ? bounded.machine.registers.gpr[9].word
                        : UINT32_C(0x0fa25c00)));
  }
}

void test_natural_match_frame_and_validation() {
  Natural natural;
  CHECK(nba97_game_match_frame(&natural.frame, &natural.frame_progress) ==
        NBA97_BODY_JOURNAL_LIMIT);
  CHECK(natural.binding.invocations == 1u);
  CHECK(natural.binding.result == NBA97_TEXT_COMPLETE);
  CHECK(natural.binding.progress.completed == 1u);
  CHECK(natural.children.links_composed == 2u);
  CHECK(natural.binding.progress.call_count
            [NBA97_GAME_CAMERA_OVERLAY_PACKETS_LINK_80056914] == 2u);
  CHECK(natural.frame_progress.stopped_pc == UINT32_C(0x800490e8));

  Nba97MatchFrameCall call{};
  call.pc = UINT32_C(0x800490c8);
  call.entry = UINT32_C(0x80075d40);
  Nba97GamePeriodValue value{};
  value.word = UINT32_C(0xfeedface);
  value.known = 1u;
  CHECK(natural.binding.result == NBA97_TEXT_COMPLETE);
  CHECK(natural.binding.progress.completed == 1u);
  Nba97GameCameraOverlayPacketsMatchFrameBinding direct = natural.binding;
  direct.entry_machine = natural.machine;
  CHECK(nba97_game_camera_overlay_packets_from_match_frame(&direct, &call,
                                                           &value) ==
        NBA97_BODY_OK);
  CHECK(value.word == 0u && value.known == 0u);
  natural.binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ^=
      4u;
  CHECK(nba97_game_camera_overlay_packets_from_match_frame(
            &natural.binding, &call, &value) ==
        NBA97_GAME_CAMERA_OVERLAY_PACKETS_MATCH_FRAME_CHILD_INCOMPLETE);
  CHECK(natural.binding.result == NBA97_TEXT_ARGUMENT);

  call.pc = UINT32_C(0x80049024);
  call.entry = UINT32_C(0x800530fc);
  CHECK(nba97_game_camera_overlay_packets_from_match_frame(
            &natural.binding, &call, &value) == NBA97_BODY_OK);
}

} // namespace

int main() {
  test_actual_link_machine_and_aliases();
  test_link_refusal_prefixes();
  test_natural_match_frame_and_validation();
  std::puts("game_camera_overlay_packets_integration_tests: 14 adapter and natural executions passed");
  return 0;
}
