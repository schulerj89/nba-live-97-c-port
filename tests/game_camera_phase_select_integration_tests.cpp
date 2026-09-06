#include "game_camera_phase_select_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "camera phase integration check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define CHECK(value) checkAt((value), __LINE__)

struct Fixture {
  static constexpr std::uint32_t Ram = UINT32_C(0x80000000);
  static constexpr std::uint32_t Sp = UINT32_C(0x801ff000);
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::vector<Nba97GameCameraSelectAccess> parentJournal =
      std::vector<Nba97GameCameraSelectAccess>(1024);
  std::vector<Nba97GameCameraPhaseSelectAccess> ownerJournal =
      std::vector<Nba97GameCameraPhaseSelectAccess>(256);
  std::vector<Nba97GameCameraSelectAccess> nestedJournal =
      std::vector<Nba97GameCameraSelectAccess>(1024);
  Nba97GameCameraSelectContext parent{};
  Nba97GameCameraSelectProgress parentProgress{};
  Nba97GameCameraPhaseSelectBinding binding{};
  std::vector<Nba97GameCameraSelectEvent> cameraEvents;
  std::vector<Nba97GameCameraPhaseSelectEvent> phaseEvents;
  std::uint32_t refuseCameraPc = 0;
  std::uint32_t refusePhasePc = 0;
  unsigned invalidControl = 0;

  explicit Fixture(std::uint32_t busy = 0) {
    parent.memory = {&region, 1};
    parent.operation_budget = 1000;
    parent.io = cameraIo;
    parent.user = this;
    parent.access_journal = parentJournal.data();
    parent.access_journal_capacity = parentJournal.size();
    for (unsigned index = 0; index < 32; ++index)
      parent.registers.gpr[index] = {
          UINT32_C(0x41000000) + index,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    parent.registers.gpr[0] = {0, 15};
    parent.registers.gpr[4] = {0, 15};
    parent.registers.gpr[5] = {0, 15};
    parent.registers.gpr[29] = {Sp, 15};
    parent.registers.gpr[31] = {UINT32_C(0x81234568), 15};
    nba97_game_camera_phase_select_binding_init(&binding, 200, 500);
    binding.io = phaseIo;
    binding.user = this;
    binding.access_journal = ownerJournal.data();
    binding.access_journal_capacity = ownerJournal.size();
    binding.camera_access_journal = nestedJournal.data();
    binding.camera_access_journal_capacity = nestedJournal.size();
    put(0x800fc99c, busy, 4);
    put(0x800f9ffe, 0, 2);
    put(0x800fdb90, 0x81, 2);
    put(0x800fe884, 2, 2);
    put(0x800fdb68, 1, 2);
    put(0x800fdb58, 10, 4);
    put(0x800fdb60, 10, 4);
    put(0x800bc940, 1, 4);
    put(0x800bc944, 1, 4);
    put(0x80021ed8, 0x5a, 1);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    for (unsigned byte = 0; byte < width; ++byte) {
      bytes[address - Ram + byte] =
          static_cast<std::uint8_t>(value >> (byte * 8u));
      known[address - Ram + byte] = 1;
    }
  }

  std::uint32_t get(std::uint32_t address, unsigned width = 4) const {
    std::uint32_t result = 0;
    for (unsigned byte = 0; byte < width; ++byte)
      result |= std::uint32_t(bytes[address - Ram + byte]) << (byte * 8u);
    return result;
  }

  static int cameraIo(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameCameraSelectEvent *event,
                      Nba97GameCameraSelectRegisters *registers) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.cameraEvents.push_back(*event);
    registers->gpr[2] = {0, 15};
    return event->pc == fixture.refuseCameraPc ? 0 : 1;
  }

  static int phaseIo(void *opaque, const Nba97GameTextMemory *,
                     const Nba97GameCameraPhaseSelectEvent *event,
                     Nba97GameCameraPhaseSelectMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.phaseEvents.push_back(*event);
    if (fixture.invalidControl != 0) {
      machine->registers.gpr[8] = {UINT32_C(0x5a5a0008), 5};
      if (fixture.invalidControl == 1)
        machine->hi.known_mask = 16;
      else if (fixture.invalidControl == 2)
        machine->lo.known_mask = 16;
      else if (fixture.invalidControl == 3)
        machine->registers.gpr[0] = {1, 15};
      else
        machine->registers.gpr[8].known_mask = 16;
    }
    return event->pc == fixture.refusePhasePc ? 0 : 1;
  }

  int run() {
    return nba97_game_camera_select_with_phase_select(&parent, &binding,
                                                      &parentProgress);
  }
};

Nba97GameCameraSelectEvent exactEvent() {
  Nba97GameCameraSelectEvent event{};
  event.pc = 0x80079a0c;
  event.delay_slot_pc = 0x80079a10;
  event.entry = 0x8007e26c;
  event.kind = NBA97_GAME_CAMERA_SELECT_CHILD_8007E26C;
  event.argument_count = 1;
  return event;
}

Nba97GameCameraSelectRegisters exactRegisters(unsigned a0) {
  Nba97GameCameraSelectRegisters registers{};
  for (unsigned index = 0; index < 32; ++index)
    registers.gpr[index] = {UINT32_C(0x51000000) + index,
                            static_cast<std::uint8_t>((index % 15u) + 1u)};
  registers.gpr[0] = {0, 15};
  registers.gpr[4] = {a0, 15};
  registers.gpr[29] = {Fixture::Sp, 15};
  registers.gpr[31] = {0x80079a14, 15};
  return registers;
}

void NaturalModeZeroBothA0Paths() {
  Fixture zero;
  CHECK(zero.run() == NBA97_TEXT_COMPLETE && zero.parentProgress.completed &&
        zero.binding.invocations == 1 && zero.binding.completions == 1 &&
        zero.binding.event.pc == 0x80079a0c &&
        zero.binding.event.delay_slot_pc == 0x80079a10 &&
        zero.binding.event.entry == 0x8007e26c);
  CHECK(!zero.binding.progress.phase_changed &&
        zero.binding.camera_invocations == 0 &&
        zero.binding.typed_invocations == 0 && zero.get(0x800bc944) == 1 &&
        zero.get(0x800fc99c) == 0);

  Fixture one(7);
  CHECK(one.run() == NBA97_TEXT_COMPLETE && one.parentProgress.completed &&
        one.binding.invocations == 1 && one.binding.completions == 1 &&
        one.binding.progress.phase_changed);
  CHECK(one.binding.camera_invocations == 1 &&
        one.binding.camera_progress.completed &&
        one.binding.typed_invocations == 3 && one.phaseEvents.size() == 3 &&
        one.binding.progress.machine.hi.known_mask == 0 &&
        one.binding.progress.machine.lo.known_mask == 0 &&
        one.get(0x800bc940) == 1 && one.get(0x800bc944) == 1 &&
        one.get(0x800fc99c) == 0);
  CHECK(one.phaseEvents[0].pc == 0x8007e3a8 &&
        one.phaseEvents[1].pc == 0x8007e3b0 &&
        one.phaseEvents[2].pc == 0x8007e3b8);
}

void DirectBothArgumentsAndGuards() {
  for (unsigned a0 = 0; a0 <= 1; ++a0) {
    Fixture fixture;
    auto event = exactEvent();
    auto registers = exactRegisters(a0);
    fixture.binding.camera_io = Fixture::cameraIo;
    fixture.binding.camera_user = &fixture;
    CHECK(nba97_game_camera_phase_select_from_camera_select(
              &fixture.binding, &fixture.parent.memory, &event, &registers) ==
              1 &&
          fixture.binding.result == NBA97_TEXT_COMPLETE &&
          fixture.binding.invocations == 1);
  }

  Fixture reused;
  auto reusedEvent = exactEvent();
  auto firstRegisters = exactRegisters(0);
  auto secondRegisters = exactRegisters(1);
  reused.binding.camera_io = Fixture::cameraIo;
  reused.binding.camera_user = &reused;
  CHECK(nba97_game_camera_phase_select_from_camera_select(
            &reused.binding, &reused.parent.memory, &reusedEvent,
            &firstRegisters) == 1 &&
        nba97_game_camera_phase_select_from_camera_select(
            &reused.binding, &reused.parent.memory, &reusedEvent,
            &secondRegisters) == 1 &&
        reused.binding.invocations == 2 && reused.binding.completions == 2);

  for (unsigned field = 0; field < 9; ++field) {
    Fixture fixture;
    auto event = exactEvent();
    auto registers = exactRegisters(0);
    if (field == 0)
      event.pc ^= 4;
    else if (field == 1)
      event.delay_slot_pc ^= 4;
    else if (field == 2)
      event.entry ^= 4;
    else if (field == 3)
      event.kind = NBA97_GAME_CAMERA_SELECT_CHILD_8007C964;
    else if (field == 4)
      event.argument_count = 2;
    else if (field == 5)
      registers.gpr[31].known_mask = 7;
    else if (field == 6)
      registers.gpr[31].word ^= 4;
    else if (field == 7)
      registers.gpr[4] = {2, 15};
    else
      registers.gpr[8].known_mask = 16;
    CHECK(nba97_game_camera_phase_select_from_camera_select(
              &fixture.binding, &fixture.parent.memory, &event, &registers) ==
              0 &&
          fixture.binding.result == NBA97_TEXT_ARGUMENT &&
          fixture.binding.invocations == 0);
  }
}

void NestedFailuresControlPrefixesAndReuse() {
  Fixture typedRefusal(1);
  typedRefusal.refusePhasePc = 0x8007e3a8;
  CHECK(typedRefusal.run() == NBA97_TEXT_IO_REFUSED &&
        typedRefusal.parentProgress.stopped_pc == 0x80079a0c &&
        typedRefusal.binding.progress.stopped_pc == 0x8007e3a8 &&
        typedRefusal.binding.camera_invocations == 1);

  for (unsigned control = 1; control <= 4; ++control) {
    Fixture invalid(1);
    invalid.invalidControl = control;
    CHECK(invalid.run() == NBA97_TEXT_ARGUMENT &&
          invalid.binding.progress.stopped_pc == 0x8007e3a8);
    if (control <= 2)
      CHECK(invalid.parentProgress.registers.gpr[8].word == 0x5a5a0008 &&
            invalid.parentProgress.registers.gpr[8].known_mask == 5);
    else
      CHECK(invalid.parentProgress.registers.gpr[0].word == 0 &&
            invalid.parentProgress.registers.gpr[0].known_mask == 15 &&
            invalid.parentProgress.registers.gpr[8].word == 0x41000008 &&
            invalid.parentProgress.registers.gpr[8].known_mask == 9);
  }

  Fixture budget;
  budget.binding.operation_budget = 0;
  CHECK(budget.run() == NBA97_TEXT_LIMIT && budget.binding.invocations == 1 &&
        budget.binding.progress.stopped_pc == 0x8007e270 &&
        budget.parentProgress.stopped_pc == 0x80079a0c);

  Fixture reused;
  CHECK(reused.run() == NBA97_TEXT_COMPLETE);
  reused.cameraEvents.clear();
  reused.phaseEvents.clear();
  CHECK(reused.run() == NBA97_TEXT_COMPLETE &&
        reused.binding.invocations == 1 && reused.binding.completions == 1 &&
        reused.cameraEvents.empty() && reused.phaseEvents.empty());
}

void MappingAndNullFailures() {
  Fixture missing;
  const std::size_t gap = 0x800f9ffe - Fixture::Ram;
  std::array<Nba97GameTextRegion, 2> regions{{
      {Fixture::Ram, missing.bytes.data(), missing.known.data(), gap},
      {0x800fa000, missing.bytes.data() + gap + 2,
       missing.known.data() + gap + 2, missing.bytes.size() - gap - 2},
  }};
  missing.parent.memory = {regions.data(), regions.size()};
  CHECK(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.binding.result == NBA97_TEXT_RESOURCE &&
        missing.binding.progress.stopped_address == 0x800f9ffe);

  Fixture nulls;
  auto event = exactEvent();
  auto registers = exactRegisters(0);
  CHECK(nba97_game_camera_phase_select_from_camera_select(
            nullptr, &nulls.parent.memory, &event, &registers) == 0);
  CHECK(nba97_game_camera_phase_select_from_camera_select(
            &nulls.binding, nullptr, &event, &registers) == 0);
  CHECK(nba97_game_camera_phase_select_from_camera_select(
            &nulls.binding, &nulls.parent.memory, nullptr, &registers) == 0);
  CHECK(nba97_game_camera_phase_select_from_camera_select(
            &nulls.binding, &nulls.parent.memory, &event, nullptr) == 0);
  CHECK(nba97_game_camera_select_with_phase_select(nullptr, &nulls.binding,
                                                   &nulls.parentProgress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_select_with_phase_select(&nulls.parent, nullptr,
                                                   &nulls.parentProgress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_select_with_phase_select(
            &nulls.parent, &nulls.binding, nullptr) == NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  NaturalModeZeroBothA0Paths();
  DirectBothArgumentsAndGuards();
  NestedFailuresControlPrefixesAndReuse();
  MappingAndNullFailures();
  std::printf("game camera phase select integration: %u checks\n", checks);
}
