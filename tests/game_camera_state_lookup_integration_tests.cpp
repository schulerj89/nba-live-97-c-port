#include "game_camera_state_lookup_adapter.h"

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
    std::fprintf(stderr, "camera state integration check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define CHECK(value) checkAt((value), __LINE__)

bool sameMachine(const Nba97GameCameraElapsedDispatchMachine &left,
                 const Nba97GameCameraElapsedDispatchMachine &right) {
  for (unsigned index = 0; index < 32; ++index)
    if (left.registers.gpr[index].word != right.registers.gpr[index].word ||
        left.registers.gpr[index].known_mask !=
            right.registers.gpr[index].known_mask)
      return false;
  return left.hi.word == right.hi.word &&
         left.hi.known_mask == right.hi.known_mask &&
         left.lo.word == right.lo.word &&
         left.lo.known_mask == right.lo.known_mask;
}

struct Fixture {
  static constexpr std::uint32_t Ram = UINT32_C(0x80000000);
  static constexpr std::uint32_t Sp = UINT32_C(0x801ff000);
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::vector<Nba97GameCameraElapsedDispatchAccess> parentJournal =
      std::vector<Nba97GameCameraElapsedDispatchAccess>(256);
  std::vector<Nba97GameCameraStateLookupAccess> lookupJournal =
      std::vector<Nba97GameCameraStateLookupAccess>(8);
  Nba97GameCameraElapsedDispatchContext parent{};
  Nba97GameCameraElapsedDispatchProgress parentProgress{};
  Nba97GameCameraStateLookupBinding binding{};
  std::vector<Nba97GameCameraElapsedDispatchEvent> prerequisiteEvents;
  std::uint32_t refusePrerequisitePc = 0;
  unsigned unexpectedPrerequisites = 0;
  std::uint32_t prerequisiteSource = 0;

  Fixture() {
    parent.memory = {&region, 1};
    parent.operation_budget = 100;
    parent.io = prerequisiteIo;
    parent.user = this;
    parent.access_journal = parentJournal.data();
    parent.access_journal_capacity = parentJournal.size();
    for (unsigned index = 0; index < 32; ++index)
      parent.machine.registers.gpr[index] = {
          UINT32_C(0x41000000) + index,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    parent.machine.registers.gpr[0] = {0, 15};
    parent.machine.registers.gpr[4] = {UINT32_MAX, 15};
    parent.machine.registers.gpr[29] = {Sp, 15};
    parent.machine.registers.gpr[31] = {UINT32_C(0x81234568), 15};
    parent.machine.hi = {0x11223344, 5};
    parent.machine.lo = {0x55667788, 10};
    nba97_game_camera_state_lookup_binding_init(&binding, 4);
    binding.access_journal = lookupJournal.data();
    binding.access_journal_capacity = lookupJournal.size();
    put(0x800bc1f4, UINT32_MAX);
    put(0x800bc1f8, 10);
    put(0x800bc1fc, 100);
    put(0x800bc200, 1);
    put(0x800fc9d0, 0x80010000);
    put(0x8001005c, 0x80020000);
    put(0x800fc9ac, 0);
    put(0x800bc204, 42);
    put(0x800bc208, 99);
    put(0x80106074, 20);
    put(0x800d8eec, 0);
  }

  void put(std::uint32_t address, std::uint32_t value, unsigned width = 4) {
    for (unsigned byte = 0; byte < width; ++byte) {
      bytes[address - Ram + byte] =
          static_cast<std::uint8_t>(value >> (byte * 8u));
      known[address - Ram + byte] = 1;
    }
  }

  std::uint32_t get(std::uint32_t address) const {
    std::uint32_t result = 0;
    for (unsigned byte = 0; byte < 4; ++byte)
      result |= std::uint32_t(bytes[address - Ram + byte]) << (byte * 8u);
    return result;
  }

  static int prerequisiteIo(void *opaque, const Nba97GameTextMemory *,
                            const Nba97GameCameraElapsedDispatchEvent *event,
                            Nba97GameCameraElapsedDispatchMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.prerequisiteEvents.push_back(*event);
    const auto &ra = machine->registers.gpr[31];
    const bool common = event->argument_count == 0 && event->invocation == 1 &&
                        event->delay_slot_pc == event->pc + 4u &&
                        ra.known_mask == 15 && ra.word == event->pc + 8u;
    bool exact = false;
    if (event->kind == NBA97_GAME_CAMERA_ELAPSED_DISPATCH_INDIRECT_8007995C)
      exact = common && event->pc == 0x8007995c && event->entry == 0x80020000;
    else if (event->kind == NBA97_GAME_CAMERA_ELAPSED_DISPATCH_PROBE_8007A468)
      exact = common && event->pc == 0x80079978 && event->entry == 0x8007a468;
    if (!exact) {
      ++fixture.unexpectedPrerequisites;
      return 0;
    }
    /* Synthetic prerequisite outputs are explicit; both unresolved services
     * return zero, and may publish the next runtime lookup source. */
    machine->registers.gpr[2] = {0, 15};
    fixture.put(0x800fc9ac, fixture.prerequisiteSource);
    return event->pc == fixture.refusePrerequisitePc ? 0 : 1;
  }

  int run() {
    return nba97_game_camera_elapsed_dispatch_with_state_lookup(
        &parent, &binding, &parentProgress);
  }
};

Nba97GameCameraElapsedDispatchEvent exactEvent() {
  Nba97GameCameraElapsedDispatchEvent event{};
  event.pc = 0x8007999c;
  event.delay_slot_pc = 0x800799a0;
  event.entry = 0x8007a410;
  event.invocation = 1;
  event.kind = NBA97_GAME_CAMERA_ELAPSED_DISPATCH_REFRESH_8007A410;
  event.argument_count = 0;
  return event;
}

Nba97GameCameraElapsedDispatchMachine exactMachine() {
  Nba97GameCameraElapsedDispatchMachine machine{};
  for (unsigned index = 0; index < 32; ++index)
    machine.registers.gpr[index] = {
        UINT32_C(0x51000000) + index,
        static_cast<std::uint8_t>((index % 15u) + 1u)};
  machine.registers.gpr[0] = {0, 15};
  machine.registers.gpr[29] = {Fixture::Sp, 15};
  machine.registers.gpr[31] = {0x800799a4, 15};
  machine.hi = {0x10203040, 6};
  machine.lo = {0xa0b0c0d0, 9};
  return machine;
}

void NegativeCacheAndProbeZeroComposition() {
  Fixture negative;
  CHECK(negative.run() == NBA97_TEXT_COMPLETE &&
        negative.parentProgress.completed &&
        negative.binding.invocations == 1 &&
        negative.binding.completions == 1 &&
        negative.binding.event.pc == 0x8007999c &&
        negative.binding.progress.lookup_address.word == 0x800bc204 &&
        negative.get(0x800bc1f4) == 42 && negative.get(0x800d8eec) == 42 &&
        negative.get(0x80106074) == 0 && negative.prerequisiteEvents.empty());

  Fixture probe;
  probe.put(0x800bc1f4, 55);
  probe.prerequisiteSource = 0x100;
  CHECK(probe.run() == NBA97_TEXT_COMPLETE && probe.binding.invocations == 1 &&
        probe.prerequisiteEvents.size() == 1 &&
        probe.prerequisiteEvents[0].pc == 0x80079978 &&
        probe.unexpectedPrerequisites == 0 &&
        probe.binding.progress.source_value.word == 0x100 &&
        probe.binding.progress.lookup_address.word == 0x800bc208 &&
        probe.get(0x800bc1f4) == 99 && probe.get(0x800d8eec) == 99);

  Fixture indirect;
  indirect.put(0x800bc200, 0);
  indirect.put(0x800bc1f4, 55);
  indirect.prerequisiteSource = 0x100;
  CHECK(indirect.run() == NBA97_TEXT_COMPLETE &&
        indirect.prerequisiteEvents.size() == 2 &&
        indirect.prerequisiteEvents[0].pc == 0x8007995c &&
        indirect.prerequisiteEvents[1].pc == 0x80079978 &&
        indirect.binding.progress.source_value.word == 0x100 &&
        indirect.get(0x800d8eec) == 99);
}

void ExactGuardsAndDirectReuse() {
  Fixture reused;
  auto event = exactEvent();
  auto first = exactMachine();
  auto second = exactMachine();
  CHECK(nba97_game_camera_state_lookup_from_elapsed_dispatch(
            &reused.binding, &reused.parent.memory, &event, &first) == 1 &&
        nba97_game_camera_state_lookup_from_elapsed_dispatch(
            &reused.binding, &reused.parent.memory, &event, &second) == 1 &&
        reused.binding.invocations == 2 && reused.binding.completions == 2);

  for (unsigned field = 0; field < 10; ++field) {
    Fixture fixture;
    auto malformed = exactEvent();
    auto machine = exactMachine();
    if (field == 0)
      malformed.pc ^= 4;
    else if (field == 1)
      malformed.delay_slot_pc ^= 4;
    else if (field == 2)
      malformed.entry ^= 4;
    else if (field == 3)
      malformed.kind = NBA97_GAME_CAMERA_ELAPSED_DISPATCH_PROBE_8007A468;
    else if (field == 4)
      malformed.argument_count = 1;
    else if (field == 5)
      malformed.invocation = 2;
    else if (field == 6)
      machine.registers.gpr[31].word ^= 4;
    else if (field == 7)
      machine.registers.gpr[31].known_mask = 7;
    else if (field == 8)
      machine.registers.gpr[0] = {1, 15};
    else
      machine.hi.known_mask = 16;
    const auto before = machine;
    CHECK(nba97_game_camera_state_lookup_from_elapsed_dispatch(
              &fixture.binding, &fixture.parent.memory, &malformed, &machine) ==
              0 &&
          fixture.binding.result == NBA97_TEXT_ARGUMENT &&
          fixture.binding.invocations == 0 && sameMachine(machine, before));
  }
}

void FailurePrefixesPrerequisiteAndReuse() {
  Fixture prerequisite;
  prerequisite.put(0x800bc1f4, 55);
  prerequisite.refusePrerequisitePc = 0x80079978;
  CHECK(prerequisite.run() == NBA97_TEXT_IO_REFUSED &&
        prerequisite.parentProgress.stopped_pc == 0x80079978 &&
        prerequisite.binding.invocations == 0 &&
        prerequisite.get(0x800d8eec) == 0 &&
        prerequisite.get(0x80106074) == 10);

  Fixture budget;
  budget.binding.operation_budget = 0;
  CHECK(budget.run() == NBA97_TEXT_LIMIT && budget.binding.invocations == 1 &&
        budget.binding.progress.stopped_pc == 0x8007a414 &&
        budget.parentProgress.stopped_pc == 0x8007999c &&
        budget.get(0x800d8eec) == 0);

  Fixture missing;
  const std::size_t gap = 0x800bc204 - Fixture::Ram;
  std::array<Nba97GameTextRegion, 2> regions{{
      {Fixture::Ram, missing.bytes.data(), missing.known.data(), gap},
      {0x800bc208, missing.bytes.data() + gap + 4,
       missing.known.data() + gap + 4, missing.bytes.size() - gap - 4},
  }};
  missing.parent.memory = {regions.data(), regions.size()};
  CHECK(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.binding.progress.stopped_address == 0x800bc204 &&
        missing.parentProgress.stopped_pc == 0x8007999c &&
        missing.get(0x800d8eec) == 0);

  Fixture malformedMemory;
  malformedMemory.known[0x800fc9ac - Fixture::Ram + 2] = 2;
  CHECK(malformedMemory.run() == NBA97_TEXT_ARGUMENT &&
        malformedMemory.binding.progress.stopped_pc == 0x8007a414 &&
        sameMachine(malformedMemory.parentProgress.machine,
                    malformedMemory.binding.progress.machine) &&
        malformedMemory.parentProgress.machine.registers.gpr[2].word ==
            0x80100000 &&
        malformedMemory.parentProgress.machine.registers.gpr[29].word ==
            Fixture::Sp - 0x18 &&
        malformedMemory.parentProgress.machine.registers.gpr[31].word ==
            0x800799a4);

  Fixture reused;
  CHECK(reused.run() == NBA97_TEXT_COMPLETE);
  reused.prerequisiteEvents.clear();
  CHECK(reused.run() == NBA97_TEXT_COMPLETE &&
        reused.binding.invocations == 1 && reused.binding.completions == 1);
}

void NullFailures() {
  Fixture fixture;
  auto event = exactEvent();
  auto machine = exactMachine();
  CHECK(nba97_game_camera_state_lookup_from_elapsed_dispatch(
            nullptr, &fixture.parent.memory, &event, &machine) == 0);
  CHECK(nba97_game_camera_state_lookup_from_elapsed_dispatch(
            &fixture.binding, nullptr, &event, &machine) == 0);
  CHECK(nba97_game_camera_state_lookup_from_elapsed_dispatch(
            &fixture.binding, &fixture.parent.memory, nullptr, &machine) == 0);
  CHECK(nba97_game_camera_state_lookup_from_elapsed_dispatch(
            &fixture.binding, &fixture.parent.memory, &event, nullptr) == 0);
  CHECK(nba97_game_camera_elapsed_dispatch_with_state_lookup(
            nullptr, &fixture.binding, &fixture.parentProgress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_elapsed_dispatch_with_state_lookup(
            &fixture.parent, nullptr, &fixture.parentProgress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_elapsed_dispatch_with_state_lookup(
            &fixture.parent, &fixture.binding, nullptr) == NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  NegativeCacheAndProbeZeroComposition();
  ExactGuardsAndDirectReuse();
  FailurePrefixesPrerequisiteAndReuse();
  NullFailures();
  std::printf("game camera state integration: %u checks\n", checks);
}
