#include "game_camera_elapsed_dispatch_adapter.h"

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
    std::fprintf(stderr, "camera elapsed integration check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define CHECK(value) checkAt((value), __LINE__)

struct Fixture {
  static constexpr std::uint32_t Ram = UINT32_C(0x80000000);
  static constexpr std::uint32_t Sp = UINT32_C(0x8010f000);
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::vector<Nba97GameCameraSelectAccess> parentJournal =
      std::vector<Nba97GameCameraSelectAccess>(1024);
  std::vector<Nba97GameCameraElapsedDispatchAccess> ownerJournal =
      std::vector<Nba97GameCameraElapsedDispatchAccess>(256);
  Nba97GameCameraSelectContext parent{};
  Nba97GameCameraSelectProgress parentProgress{};
  Nba97GameCameraElapsedDispatchBinding binding{};
  std::vector<Nba97GameCameraSelectEvent> parentEvents;
  std::vector<Nba97GameCameraElapsedDispatchEvent> childEvents;
  std::vector<std::uint32_t> serviceOrder;
  std::uint32_t refuseParentPc = 0;
  std::uint32_t refuseChildPc = 0;
  unsigned unexpectedParentCalls = 0;
  unsigned unexpectedChildCalls = 0;
  unsigned invalidControl = 0;

  Fixture() {
    parent.memory = {&region, 1};
    parent.operation_budget = 2000;
    parent.io = parentIo;
    parent.user = this;
    parent.access_journal = parentJournal.data();
    parent.access_journal_capacity = parentJournal.size();
    for (unsigned index = 0; index < 32; ++index)
      parent.registers.gpr[index] = {
          UINT32_C(0x41000000) + index,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    parent.registers.gpr[0] = {0, 15};
    parent.registers.gpr[29] = {Sp, 15};
    parent.registers.gpr[31] = {UINT32_C(0x81234568), 15};
    nba97_game_camera_elapsed_dispatch_binding_init(&binding, 500);
    binding.io = childIo;
    binding.user = this;
    binding.access_journal = ownerJournal.data();
    binding.access_journal_capacity = ownerJournal.size();
    put(0x80021ed7, 6, 1);
    put(0x80021ed8, 0x5a, 1);
    for (unsigned index = 0; index < 6; ++index)
      put(0x80109aa8 + index * 4u, 0xa1000000 + index, 4);
    put(0x800bc1f4, UINT32_MAX);
    put(0x800bc1f8, 10);
    put(0x800bc1fc, 100);
    put(0x800bc200, 1);
    put(0x80106074, 20);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width = 4) {
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

  static int parentIo(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameCameraSelectEvent *event,
                      Nba97GameCameraSelectRegisters *registers) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.parentEvents.push_back(*event);
    fixture.serviceOrder.push_back(event->pc);
    const auto &a0 = registers->gpr[4];
    const auto &a1 = registers->gpr[5];
    const auto &ra = registers->gpr[31];
    const bool common = event->delay_slot_pc == event->pc + 4u &&
                        ra.known_mask == 15 && ra.word == event->pc + 8u;
    const bool selector = a1.known_mask == 15 && a1.word <= 1u;
    bool exact = false;
    if (event->pc == 0x80079aa4)
      exact = common && selector && event->entry == 0x8007c964 &&
              event->kind == NBA97_GAME_CAMERA_SELECT_CHILD_8007C964 &&
              event->argument_count == 0 && a0.word == 8 && a0.known_mask == 15;
    else if (event->pc == 0x80079b7c)
      exact = common && selector && event->entry == 0x80079f78 &&
              event->kind == NBA97_GAME_CAMERA_SELECT_CHILD_80079F78 &&
              event->argument_count == 1 && a0.word == 0x5a &&
              a0.known_mask == 15;
    else if (event->pc == 0x80079d0c)
      exact = common && event->entry == 0x8007a3a0 &&
              event->kind == NBA97_GAME_CAMERA_SELECT_CHILD_8007A3A0 &&
              event->argument_count == 0 && a0.known_mask == 15 &&
              a1.known_mask == 15 &&
              ((a0.word == 0 && a1.word == 0) ||
               (a0.word == 0x800fc9a0 && a1.word == 0x80109aa8));
    if (!exact) {
      ++fixture.unexpectedParentCalls;
      return 0;
    }
    registers->gpr[2] = {0, 15};
    return event->pc == fixture.refuseParentPc ? 0 : 1;
  }

  static int childIo(void *opaque, const Nba97GameTextMemory *,
                     const Nba97GameCameraElapsedDispatchEvent *event,
                     Nba97GameCameraElapsedDispatchMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.childEvents.push_back(*event);
    fixture.serviceOrder.push_back(event->pc);
    const auto &ra = machine->registers.gpr[31];
    const bool common = event->delay_slot_pc == event->pc + 4u &&
                        event->argument_count == 0 && event->invocation == 1 &&
                        ra.word == event->pc + 8u && ra.known_mask == 15;
    bool exact = false;
    if (event->kind == NBA97_GAME_CAMERA_ELAPSED_DISPATCH_INDIRECT_8007995C)
      exact = common && event->pc == 0x8007995c && event->entry != 0 &&
              (event->entry & 3u) == 0;
    else if (event->kind == NBA97_GAME_CAMERA_ELAPSED_DISPATCH_PROBE_8007A468)
      exact = common && event->pc == 0x80079978 && event->entry == 0x8007a468;
    else if (event->kind == NBA97_GAME_CAMERA_ELAPSED_DISPATCH_REFRESH_8007A410)
      exact = common && event->pc == 0x8007999c && event->entry == 0x8007a410;
    if (!exact) {
      ++fixture.unexpectedChildCalls;
      return 0;
    }
    /* Synthetic prerequisite services: indirect/probe return zero; refresh
     * returns the explicit camera-state fixture value 42. */
    machine->registers.gpr[2] = {
        event->kind == NBA97_GAME_CAMERA_ELAPSED_DISPATCH_REFRESH_8007A410 ? 42u
                                                                           : 0u,
        15};
    if (fixture.invalidControl != 0) {
      machine->registers.gpr[8] = {0x5a5a0008, 5};
      if (fixture.invalidControl == 1)
        machine->hi.known_mask = 16;
      else if (fixture.invalidControl == 2)
        machine->lo.known_mask = 16;
      else if (fixture.invalidControl == 3)
        machine->registers.gpr[0] = {1, 15};
      else
        machine->registers.gpr[8].known_mask = 16;
    }
    return event->pc == fixture.refuseChildPc ? 0 : 1;
  }

  int run(unsigned selector) {
    parent.registers.gpr[4] = {8, 15};
    parent.registers.gpr[5] = {selector, 15};
    return nba97_game_camera_select_with_elapsed_dispatch(&parent, &binding,
                                                          &parentProgress);
  }
};

Nba97GameCameraSelectEvent exactEvent(std::uint32_t pc) {
  Nba97GameCameraSelectEvent event{};
  event.pc = pc;
  event.delay_slot_pc = pc + 4u;
  event.entry = 0x800798b4;
  event.kind = NBA97_GAME_CAMERA_SELECT_CHILD_800798B4;
  event.argument_count = 1;
  return event;
}

Nba97GameCameraSelectRegisters exactRegisters(std::uint32_t pc) {
  Nba97GameCameraSelectRegisters registers{};
  for (unsigned index = 0; index < 32; ++index)
    registers.gpr[index] = {UINT32_C(0x51000000) + index,
                            static_cast<std::uint8_t>((index % 15u) + 1u)};
  registers.gpr[0] = {0, 15};
  registers.gpr[4] = {UINT32_MAX, 15};
  registers.gpr[29] = {Fixture::Sp, 15};
  registers.gpr[31] = {pc + 8u, 15};
  return registers;
}

void BothNaturalCallerSites() {
  for (unsigned selector : {1u, 0u}) {
    Fixture fixture;
    CHECK(
        fixture.run(selector) == NBA97_TEXT_COMPLETE &&
        fixture.parentProgress.completed && fixture.binding.invocations == 1 &&
        fixture.binding.completions == 1 &&
        fixture.binding.event.pc == (selector == 1 ? 0x80079c2c : 0x80079c8c) &&
        fixture.binding.event.delay_slot_pc ==
            (selector == 1 ? 0x80079c30 : 0x80079c90));
    CHECK(fixture.binding.progress.requested_delta.word == UINT32_MAX &&
          fixture.binding.progress.machine.hi.known_mask == 0 &&
          fixture.binding.progress.machine.lo.known_mask == 0 &&
          fixture.get(0x800d8eec) == 42 && fixture.get(0x80106074) == 0 &&
          fixture.childEvents.size() == 1 &&
          fixture.childEvents[0].pc == 0x8007999c &&
          fixture.unexpectedParentCalls == 0 &&
          fixture.unexpectedChildCalls == 0);
    const std::vector<std::uint32_t> expectedOrder{0x80079aa4, 0x80079b7c,
                                                   0x8007999c, 0x80079d0c};
    CHECK(fixture.serviceOrder == expectedOrder);
  }
}

void ExactGuardsAndDirectReuse() {
  for (std::uint32_t pc : {UINT32_C(0x80079c2c), UINT32_C(0x80079c8c)}) {
    Fixture reused;
    auto event = exactEvent(pc);
    auto first = exactRegisters(pc);
    auto second = exactRegisters(pc);
    const int firstResult =
        nba97_game_camera_elapsed_dispatch_from_camera_select(
            &reused.binding, &reused.parent.memory, &event, &first);
    const int secondResult =
        nba97_game_camera_elapsed_dispatch_from_camera_select(
            &reused.binding, &reused.parent.memory, &event, &second);
    CHECK(firstResult == 1 && secondResult == 1 &&
          reused.binding.invocations == 2 && reused.binding.completions == 2);
  }

  for (unsigned field = 0; field < 9; ++field) {
    Fixture fixture;
    auto event = exactEvent(0x80079c2c);
    auto registers = exactRegisters(0x80079c2c);
    if (field == 0)
      event.pc ^= 4;
    else if (field == 1)
      event.delay_slot_pc ^= 4;
    else if (field == 2)
      event.entry ^= 4;
    else if (field == 3)
      event.kind = NBA97_GAME_CAMERA_SELECT_CHILD_80079EBC;
    else if (field == 4)
      event.argument_count = 0;
    else if (field == 5)
      registers.gpr[31].known_mask = 7;
    else if (field == 6)
      registers.gpr[31].word ^= 4;
    else if (field == 7)
      registers.gpr[4] = {0, 15};
    else
      registers.gpr[8].known_mask = 16;
    CHECK(nba97_game_camera_elapsed_dispatch_from_camera_select(
              &fixture.binding, &fixture.parent.memory, &event, &registers) ==
              0 &&
          fixture.binding.result == NBA97_TEXT_ARGUMENT &&
          fixture.binding.invocations == 0);
  }
}

void NestedFailuresPrefixesAndWrapperReuse() {
  Fixture refusal;
  refusal.refuseChildPc = 0x8007999c;
  CHECK(refusal.run(0) == NBA97_TEXT_IO_REFUSED &&
        refusal.binding.progress.stopped_pc == 0x8007999c &&
        refusal.parentProgress.stopped_pc == 0x80079c8c);

  Fixture prerequisiteRefusal;
  prerequisiteRefusal.refuseParentPc = 0x80079aa4;
  CHECK(prerequisiteRefusal.run(0) == NBA97_TEXT_IO_REFUSED &&
        prerequisiteRefusal.parentProgress.stopped_pc == 0x80079aa4 &&
        prerequisiteRefusal.binding.invocations == 0 &&
        prerequisiteRefusal.childEvents.empty());

  for (unsigned control = 1; control <= 4; ++control) {
    Fixture invalid;
    invalid.invalidControl = control;
    CHECK(invalid.run(0) == NBA97_TEXT_ARGUMENT &&
          invalid.binding.progress.stopped_pc == 0x8007999c);
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
  CHECK(budget.run(0) == NBA97_TEXT_LIMIT && budget.binding.invocations == 1 &&
        budget.binding.progress.stopped_pc == 0x800798c0);

  Fixture reused;
  CHECK(reused.run(0) == NBA97_TEXT_COMPLETE);
  reused.parentEvents.clear();
  reused.childEvents.clear();
  CHECK(reused.run(0) == NBA97_TEXT_COMPLETE &&
        reused.binding.invocations == 1 && reused.binding.completions == 1);
}

void MappingAndNullFailures() {
  Fixture missing;
  const std::size_t gap = 0x800bc1f8 - Fixture::Ram;
  std::array<Nba97GameTextRegion, 2> regions{{
      {Fixture::Ram, missing.bytes.data(), missing.known.data(), gap},
      {0x800bc1fc, missing.bytes.data() + gap + 4,
       missing.known.data() + gap + 4, missing.bytes.size() - gap - 4},
  }};
  missing.parent.memory = {regions.data(), regions.size()};
  CHECK(missing.run(0) == NBA97_TEXT_RESOURCE &&
        missing.binding.result == NBA97_TEXT_RESOURCE &&
        missing.binding.progress.stopped_address == 0x800bc1f8);

  Fixture nulls;
  auto event = exactEvent(0x80079c2c);
  auto registers = exactRegisters(0x80079c2c);
  CHECK(nba97_game_camera_elapsed_dispatch_from_camera_select(
            nullptr, &nulls.parent.memory, &event, &registers) == 0);
  CHECK(nba97_game_camera_elapsed_dispatch_from_camera_select(
            &nulls.binding, nullptr, &event, &registers) == 0);
  CHECK(nba97_game_camera_elapsed_dispatch_from_camera_select(
            &nulls.binding, &nulls.parent.memory, nullptr, &registers) == 0);
  CHECK(nba97_game_camera_elapsed_dispatch_from_camera_select(
            &nulls.binding, &nulls.parent.memory, &event, nullptr) == 0);
  CHECK(nba97_game_camera_select_with_elapsed_dispatch(nullptr, &nulls.binding,
                                                       &nulls.parentProgress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_select_with_elapsed_dispatch(&nulls.parent, nullptr,
                                                       &nulls.parentProgress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_select_with_elapsed_dispatch(
            &nulls.parent, &nulls.binding, nullptr) == NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  BothNaturalCallerSites();
  ExactGuardsAndDirectReuse();
  NestedFailuresPrefixesAndWrapperReuse();
  MappingAndNullFailures();
  std::printf("game camera elapsed integration: %u checks\n", checks);
}
