#include "game_camera_frame_transform_adapter.h"

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
  for (unsigned byte = 0; byte != 4u; ++byte)
    put8(ram, address + byte, static_cast<uint8_t>(value >> (8u * byte)));
}
void set_word(Nba97GameCameraFrameTransformWord &word, uint32_t value,
              uint8_t known = 15u) {
  word.word = value;
  word.known_mask = known;
}

struct Natural {
  std::vector<uint8_t> ram = std::vector<uint8_t>(UINT32_C(0x200000), 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(UINT32_C(0x200000), 1u);
  Nba97GameTextRegion region{};
  Nba97GameTextMemory memory{};
  Nba97GameCameraFrameTransformMachine machine{};
  Nba97GameCameraFrameTransformMatchFrameBinding binding{};
  Nba97MatchFrameContext frame{};
  Nba97MatchFrameProgress frame_progress{};
  size_t children = 0u;
  size_t fallback_calls = 0u;

  Natural();
};

int transform_child(void *opaque, const Nba97GameTextMemory *,
                    const Nba97GameCameraFrameTransformEvent *event,
                    Nba97GameCameraFrameTransformMachine *machine) {
  Natural &natural = *static_cast<Natural *>(opaque);
  ++natural.children;
  CHECK(machine->registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
        event->pc + 8u);
  return 1;
}

int fallback(void *opaque, const Nba97MatchFrameCall *call,
             Nba97GamePeriodValue *value) {
  Natural &natural = *static_cast<Natural *>(opaque);
  ++natural.fallback_calls;
  if (call->entry == UINT32_C(0x80048ff4)) {
    value->word = 0u;
    value->known = 1u;
  }
  return NBA97_BODY_OK;
}

int match_io(void *opaque, const Nba97MatchFrameCall *call,
             Nba97GamePeriodValue *value) {
  Natural &natural = *static_cast<Natural *>(opaque);
  return nba97_game_camera_frame_transform_from_match_frame(&natural.binding,
                                                            call, value);
}

int match_access(void *opaque, uint32_t, uint32_t address, unsigned width,
                 unsigned kind, Nba97PlayerFrameValue *value) {
  Natural &natural = *static_cast<Natural *>(opaque);
  if (address < kBase ||
      static_cast<uint64_t>(address - kBase) + width > natural.ram.size())
    return NBA97_BODY_BOUNDS;
  const size_t offset = address - kBase;
  if (kind == NBA97_FRAME_READ) {
    std::memset(value, 0, sizeof(*value));
    for (unsigned byte = 0; byte != width; ++byte) {
      value->word |= static_cast<uint32_t>(natural.ram[offset + byte])
                     << (8u * byte);
      if (natural.known[offset + byte] != 0u)
        value->known_mask =
            static_cast<uint8_t>(value->known_mask | (1u << byte));
    }
  } else {
    for (unsigned byte = 0; byte != width; ++byte) {
      natural.ram[offset + byte] =
          static_cast<uint8_t>(value->word >> (8u * byte));
      natural.known[offset + byte] =
          static_cast<uint8_t>((value->known_mask >> byte) & 1u);
    }
  }
  return NBA97_BODY_OK;
}

Natural::Natural() {
  region.base = kBase;
  region.data = ram.data();
  region.known = known.data();
  region.size = ram.size();
  memory.region = &region;
  memory.count = 1u;
  for (unsigned reg = 0; reg != 32u; ++reg)
    set_word(machine.registers.gpr[reg], UINT32_C(0x30000000) + reg);
  set_word(machine.registers.gpr[0], 0u);
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP], kStack);
  set_word(machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA],
           UINT32_C(0x800490bc));
  set_word(machine.hi, UINT32_C(0x11112222));
  set_word(machine.lo, UINT32_C(0x33334444));
  put32(ram, UINT32_C(0x800eb678), 1u);
  put16(ram, UINT32_C(0x800fa638), 1u);
  put16(ram, UINT32_C(0x800fa63a), 2u);
  put16(ram, UINT32_C(0x800fa63c), 3u);
  put16(ram, UINT32_C(0x800fa630), 4u);
  put16(ram, UINT32_C(0x800fa632), 5u);
  put16(ram, UINT32_C(0x800fa634), 6u);
  put16(ram, UINT32_C(0x800f9fd8), 10u);
  put16(ram, UINT32_C(0x800f9fda), 20u);
  put16(ram, UINT32_C(0x800f9fdc), 30u);
  put32(ram, UINT32_C(0x800fc61c), 100u);
  put32(ram, UINT32_C(0x800fc620), 200u);
  put32(ram, UINT32_C(0x800fc624), 300u);
  put32(ram, UINT32_C(0x8001ede8), 0u);
  put32(ram, UINT32_C(0x800b729c), 320u);
  nba97_game_camera_frame_transform_match_frame_binding_init(
      &binding, &memory, &machine, 1000u, transform_child, this, nullptr, 0u,
      fallback, this);
  frame.access = match_access;
  frame.io = match_io;
  frame.user = this;
  frame.operation_budget = 14u;
}

void test_natural_match_frame_call() {
  Natural natural;
  CHECK(nba97_game_match_frame(&natural.frame, &natural.frame_progress) ==
        NBA97_BODY_JOURNAL_LIMIT);
  CHECK(natural.binding.invocations == 1u);
  CHECK(natural.binding.result == NBA97_TEXT_COMPLETE);
  CHECK(natural.binding.progress.completed == 1u);
  CHECK(natural.children == 4u);
  CHECK(natural.frame_progress.calls == 7u);
  CHECK(natural.frame_progress.stopped_pc == UINT32_C(0x800490c0));
  CHECK(
      natural.binding.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
          .word == UINT32_C(0x800490bc));
}

void test_adapter_validation_and_fallback() {
  Natural natural;
  Nba97MatchFrameCall call{};
  call.pc = UINT32_C(0x800490b4);
  call.entry = UINT32_C(0x80051098);
  Nba97GamePeriodValue value{};

  natural.binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ^=
      4u;
  CHECK(nba97_game_camera_frame_transform_from_match_frame(&natural.binding,
                                                           &call, &value) ==
        NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATCH_FRAME_CHILD_INCOMPLETE);
  CHECK(natural.binding.result == NBA97_TEXT_ARGUMENT);
  CHECK(natural.binding.invocations == 0u);

  natural.binding.entry_machine = natural.machine;
  natural.binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 7u;
  CHECK(nba97_game_camera_frame_transform_from_match_frame(&natural.binding,
                                                           &call, &value) ==
        NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATCH_FRAME_CHILD_INCOMPLETE);
  CHECK(natural.binding.invocations == 0u);

  natural.binding.entry_machine = natural.machine;
  call.entry = UINT32_C(0x8005109c);
  CHECK(nba97_game_camera_frame_transform_from_match_frame(&natural.binding,
                                                           &call, &value) ==
        NBA97_GAME_CAMERA_FRAME_TRANSFORM_MATCH_FRAME_CHILD_INCOMPLETE);
  CHECK(natural.binding.invocations == 0u);

  call.pc = UINT32_C(0x80049024);
  call.entry = UINT32_C(0x800530fc);
  CHECK(nba97_game_camera_frame_transform_from_match_frame(
            &natural.binding, &call, &value) == NBA97_BODY_OK);
  CHECK(natural.fallback_calls == 1u);
}

} // namespace

int main() {
  test_natural_match_frame_call();
  test_adapter_validation_and_fallback();
  std::puts("game_camera_frame_transform_integration_tests: 5 natural and "
            "adapter executions passed");
  return 0;
}
