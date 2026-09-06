#include "game_countdown_ui_update_adapter.h"

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U = std::uint32_t;
unsigned checks;
void checkAt(bool value, int line) {
  ++checks;
  if (!value)
    throw std::runtime_error("countdown integration failed at " +
                             std::to_string(line));
}
#define check(value) checkAt((value), __LINE__)

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{0x80000000u, bytes.data(), known.data(),
                             bytes.size()};
  Nba97GameFrameUiServiceMachine parentMachine{};
  Nba97GameFrameUiServiceProgress parentProgress{};
  Nba97GameCountdownUiUpdateBinding binding{};
  unsigned childCalls = 0;
  bool invalidChildMachine = false;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      parentMachine.registers.gpr[i] = {0x10100000u + i, 15};
    parentMachine.registers.gpr[0] = {0, 15};
    parentMachine.registers.gpr[29] = {0x801ff000u, 15};
    parentMachine.registers.gpr[31] = {0x8002ddb4u, 15};
    parentMachine.hi = {0x11112222u, 7};
    parentMachine.lo = {0x33334444u, 11};
    put(0x800fdba4u, 601, 4);
    put(0x800fea2eu, 0xffffu, 2);
    put(0x800fa038u, 0, 2);
    put(0x800eb680u, 1, 1);
    for (unsigned i = 0; i < 22; ++i)
      put(0x800249e4u + i, 0x20u + i, 1);
    binding.operation_budget = 256;
    binding.io = child;
    binding.user = this;
  }

  std::size_t at(U address, unsigned width) const {
    if (address < 0x80000000u || std::uint64_t(address) + width > 0x80200000u)
      throw std::out_of_range("unmapped");
    return address - 0x80000000u;
  }
  void put(U address, U value, unsigned width) {
    const auto offset = at(address, width);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = std::uint8_t(value >> (8u * i));
      known[offset + i] = 1;
    }
  }

  static int child(void *opaque, const Nba97GameTextMemory *,
                   const Nba97GameCountdownUiUpdateEvent *event,
                   Nba97GameCountdownUiUpdateMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.childCalls;
    check(event->pc == 0x8003295cu && event->entry == 0x8003066cu &&
          event->delay_slot_pc == 0x80032960u && event->argument_count == 1 &&
          machine->registers.gpr[4].word == 0xc9u);
    if (f.invalidChildMachine)
      machine->registers.gpr[0].word = 1;
    return 1;
  }

  int run() {
    Nba97GameFrameUiServiceContext parent{};
    parent.memory = {&region, 1};
    parent.operation_budget = 64;
    parent.machine = parentMachine;
    parent.user = this;
    return nba97_game_frame_ui_service_with_countdown_ui_update(
        &parent, &binding, &parentProgress);
  }
};

void naturalAndReuse() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.parentProgress.completed &&
        f.binding.result == NBA97_TEXT_COMPLETE && f.binding.invocations == 1 &&
        f.binding.completions == 1 && f.binding.progress.completed);
  check(f.binding.event.kind == NBA97_GAME_FRAME_UI_SERVICE_CHILD_8003287C &&
        f.binding.event.pc == 0x80032b18u &&
        f.binding.event.delay_slot_pc == 0x80032b1cu &&
        f.binding.event.entry == 0x8003287cu &&
        f.binding.event.argument_count == 0 &&
        f.binding.progress.saved_return_address.word == 0x80032b20u);
  check(f.parentProgress.machine.registers.gpr[31].word == 0x8002ddb4u &&
        f.parentProgress.machine.registers.gpr[29].word == 0x801ff000u &&
        f.childCalls == 0);
  check(f.run() == NBA97_TEXT_COMPLETE && f.binding.invocations == 2 &&
        f.binding.completions == 2);
}

void guardsAndFailurePrefixes() {
  Fixture f;
  Nba97GameFrameUiServiceEvent event{0x80032b18u,
                                     0x80032b1cu,
                                     0x8003287cu,
                                     1,
                                     1,
                                     NBA97_GAME_FRAME_UI_SERVICE_CHILD_8003287C,
                                     0};
  auto machine = f.parentMachine;
  machine.registers.gpr[31] = {0x80032b20u, 15};
  Nba97GameTextMemory memory{&f.region, 1};
  auto invalid = event;
  invalid.entry = 0;
  check(nba97_game_countdown_ui_update_from_frame_ui_service(
            &f.binding, &memory, &invalid, &machine) == 0 &&
        f.binding.result == NBA97_TEXT_ARGUMENT);
  invalid = event;
  invalid.pc = 0;
  check(nba97_game_countdown_ui_update_from_frame_ui_service(
            &f.binding, &memory, &invalid, &machine) == 0);
  invalid = event;
  invalid.delay_slot_pc = 0;
  check(nba97_game_countdown_ui_update_from_frame_ui_service(
            &f.binding, &memory, &invalid, &machine) == 0);
  invalid = event;
  invalid.argument_count = 1;
  check(nba97_game_countdown_ui_update_from_frame_ui_service(
            &f.binding, &memory, &invalid, &machine) == 0);
  machine.registers.gpr[31].word = 0x80032b24u;
  check(nba97_game_countdown_ui_update_from_frame_ui_service(
            &f.binding, &memory, &event, &machine) == 0);

  Fixture missing;
  missing.put(0x800fea2eu, 7, 2);
  missing.binding.io = nullptr;
  check(missing.run() == NBA97_TEXT_IO_REFUSED &&
        missing.parentProgress.stopped_pc == 0x80032b18u &&
        missing.binding.progress.stopped_pc == 0x8003295cu &&
        missing.binding.progress.reads >= 12 && missing.childCalls == 0);

  Fixture malformed;
  malformed.put(0x800fea2eu, 7, 2);
  malformed.invalidChildMachine = true;
  check(malformed.run() == NBA97_TEXT_ARGUMENT && malformed.childCalls == 1 &&
        malformed.binding.progress.machine.registers.gpr[0].word == 1 &&
        malformed.parentProgress.machine.registers.gpr[0].word == 1);
}
} // namespace

int main() {
  try {
    naturalAndReuse();
    guardsAndFailurePrefixes();
    std::printf(
        "game_countdown_ui_update_integration_tests: PASS (%u checks)\n",
        checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
