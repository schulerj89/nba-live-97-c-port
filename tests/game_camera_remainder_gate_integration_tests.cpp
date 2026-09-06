#include "game_camera_remainder_gate_adapter.h"

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
    std::fprintf(stderr, "camera remainder integration check %u failed at %u\n",
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
  std::array<Nba97GameCameraRemainderGateAccess, 4> gateJournal{};
  std::array<Nba97GameCameraStateLookupAccess, 4> lookupJournal{};
  Nba97GameCameraElapsedDispatchContext parent{};
  Nba97GameCameraElapsedDispatchProgress parentProgress{};
  Nba97GameCameraRemainderGateBinding gate{};
  Nba97GameCameraStateLookupBinding lookup{};
  std::vector<Nba97GameCameraElapsedDispatchEvent> prerequisiteEvents;
  std::uint32_t refusePrerequisitePc = 0;
  unsigned unexpectedPrerequisites = 0;

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
    nba97_game_camera_remainder_gate_binding_init(&gate, 2);
    gate.access_journal = gateJournal.data();
    gate.access_journal_capacity = gateJournal.size();
    nba97_game_camera_state_lookup_binding_init(&lookup, 4);
    lookup.access_journal = lookupJournal.data();
    lookup.access_journal_capacity = lookupJournal.size();
    put(0x800bc1f4, 55);
    put(0x800bc1f8, 10);
    put(0x800bc1fc, 100);
    put(0x800bc200, 1);
    put(0x800fc9d0, 0x80010000);
    put(0x8001005c, 0x80020000);
    put(0x800fc9ac, 50);
    put(0x800bc204, 42);
    put(0x80106074, 20);
    put(0x800d8eec, 0);
  }

  void put(std::uint32_t address, std::uint32_t value, std::uint8_t mask = 15) {
    for (unsigned byte = 0; byte < 4; ++byte) {
      bytes[address - Ram + byte] =
          static_cast<std::uint8_t>(value >> (byte * 8u));
      known[address - Ram + byte] =
          static_cast<std::uint8_t>((mask >> byte) & 1u);
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
    const bool exact =
        event->kind == NBA97_GAME_CAMERA_ELAPSED_DISPATCH_INDIRECT_8007995C &&
        event->pc == 0x8007995c && event->delay_slot_pc == 0x80079960 &&
        event->entry == 0x80020000 && event->argument_count == 0 &&
        event->invocation == 1 && ra.known_mask == 15 && ra.word == 0x80079964;
    if (!exact) {
      ++fixture.unexpectedPrerequisites;
      return 0;
    }
    machine->registers.gpr[2] = {0, 15};
    return event->pc == fixture.refusePrerequisitePc ? 0 : 1;
  }

  int run() {
    return nba97_game_camera_elapsed_dispatch_with_remainder_gate(
        &parent, &gate, &lookup, &parentProgress);
  }
};

Nba97GameCameraElapsedDispatchEvent exactGateEvent() {
  Nba97GameCameraElapsedDispatchEvent event{};
  event.pc = 0x80079978;
  event.delay_slot_pc = 0x8007997c;
  event.entry = 0x8007a468;
  event.invocation = 1;
  event.kind = NBA97_GAME_CAMERA_ELAPSED_DISPATCH_PROBE_8007A468;
  event.argument_count = 0;
  return event;
}

Nba97GameCameraElapsedDispatchMachine exactGateMachine() {
  Nba97GameCameraElapsedDispatchMachine machine{};
  for (unsigned index = 0; index < 32; ++index)
    machine.registers.gpr[index] = {
        UINT32_C(0x51000000) + index,
        static_cast<std::uint8_t>((index % 15u) + 1u)};
  machine.registers.gpr[0] = {0, 15};
  machine.registers.gpr[29] = {Fixture::Sp, 15};
  machine.registers.gpr[31] = {0x80079980, 15};
  machine.hi = {0x10203040, 6};
  machine.lo = {0xa0b0c0d0, 9};
  return machine;
}

void GateOneKeepsCacheAndGateZeroRefreshes() {
  Fixture keep;
  CHECK(keep.run() == NBA97_TEXT_COMPLETE && keep.parentProgress.completed &&
        keep.gate.invocations == 1 && keep.gate.completions == 1 &&
        keep.gate.event.pc == 0x80079978 &&
        keep.gate.progress.returned_value.word == 1 &&
        keep.lookup.invocations == 0 && keep.get(0x800bc1f4) == 55 &&
        keep.get(0x800d8eec) == 55 && keep.get(0x80106074) == 0 &&
        keep.prerequisiteEvents.empty());

  Fixture refresh;
  refresh.put(0x800fc9ac, 51);
  CHECK(refresh.run() == NBA97_TEXT_COMPLETE && refresh.gate.invocations == 1 &&
        refresh.gate.completions == 1 &&
        refresh.gate.progress.returned_value.word == 0 &&
        refresh.lookup.invocations == 1 && refresh.lookup.completions == 1 &&
        refresh.gate.event.operation < refresh.lookup.event.operation &&
        refresh.lookup.progress.source_value.word == 51 &&
        refresh.lookup.progress.lookup_address.word == 0x800bc204 &&
        refresh.get(0x800bc1f4) == 42 && refresh.get(0x800d8eec) == 42);

  Fixture indirect;
  indirect.put(0x800bc200, 0);
  indirect.put(0x800fc9ac, 51);
  CHECK(indirect.run() == NBA97_TEXT_COMPLETE &&
        indirect.prerequisiteEvents.size() == 1 &&
        indirect.prerequisiteEvents[0].pc == 0x8007995c &&
        indirect.unexpectedPrerequisites == 0 &&
        indirect.prerequisiteEvents[0].operation <
            indirect.gate.event.operation &&
        indirect.gate.event.operation < indirect.lookup.event.operation &&
        indirect.get(0x800d8eec) == 42);
}

void ExactGuardsAndBindingReuse() {
  Fixture reused;
  auto event = exactGateEvent();
  auto first = exactGateMachine();
  auto second = exactGateMachine();
  CHECK(nba97_game_camera_remainder_gate_from_elapsed_dispatch(
            &reused.gate, &reused.parent.memory, &event, &first) == 1 &&
        nba97_game_camera_remainder_gate_from_elapsed_dispatch(
            &reused.gate, &reused.parent.memory, &event, &second) == 1 &&
        reused.gate.invocations == 2 && reused.gate.completions == 2);

  for (unsigned field = 0; field < 11; ++field) {
    Fixture fixture;
    auto malformed = exactGateEvent();
    auto machine = exactGateMachine();
    if (field == 0)
      malformed.pc ^= 4;
    else if (field == 1)
      malformed.delay_slot_pc ^= 4;
    else if (field == 2)
      malformed.entry ^= 4;
    else if (field == 3)
      malformed.kind = NBA97_GAME_CAMERA_ELAPSED_DISPATCH_REFRESH_8007A410;
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
    else if (field == 9)
      machine.hi.known_mask = 16;
    else
      machine.lo.known_mask = 16;
    const auto before = machine;
    CHECK(nba97_game_camera_remainder_gate_from_elapsed_dispatch(
              &fixture.gate, &fixture.parent.memory, &malformed, &machine) ==
              0 &&
          fixture.gate.result == NBA97_TEXT_ARGUMENT &&
          fixture.gate.invocations == 0 && sameMachine(machine, before));
  }
}

void FailurePrefixesAndWrapperReuse() {
  Fixture prerequisite;
  prerequisite.put(0x800bc200, 0);
  prerequisite.refusePrerequisitePc = 0x8007995c;
  CHECK(prerequisite.run() == NBA97_TEXT_IO_REFUSED &&
        prerequisite.parentProgress.stopped_pc == 0x8007995c &&
        prerequisite.gate.invocations == 0 &&
        prerequisite.lookup.invocations == 0 &&
        prerequisite.get(0x800d8eec) == 0);

  Fixture budget;
  budget.gate.operation_budget = 0;
  CHECK(budget.run() == NBA97_TEXT_LIMIT && budget.gate.invocations == 1 &&
        budget.gate.progress.stopped_pc == 0x8007a46c &&
        budget.parentProgress.stopped_pc == 0x80079978 &&
        budget.lookup.invocations == 0 && budget.get(0x800d8eec) == 0);

  Fixture unknown;
  unknown.put(0x800fc9ac, 50, 7);
  CHECK(unknown.run() == NBA97_TEXT_UNKNOWN &&
        unknown.gate.progress.stopped_pc == 0x8007a474 &&
        unknown.gate.progress.machine.registers.gpr[2].word == 50 &&
        unknown.gate.progress.machine.registers.gpr[2].known_mask == 7 &&
        unknown.parentProgress.stopped_pc == 0x80079978 &&
        unknown.get(0x800d8eec) == 0);

  Fixture unknownPredicate;
  unknownPredicate.put(0x800fc9ac, 0, 8);
  CHECK(unknownPredicate.run() == NBA97_TEXT_UNKNOWN &&
        unknownPredicate.gate.completions == 1 &&
        unknownPredicate.gate.progress.returned_value.word == 1 &&
        unknownPredicate.gate.progress.returned_value.known_mask == 14 &&
        unknownPredicate.parentProgress.stopped_pc == 0x80079984 &&
        unknownPredicate.parentProgress.machine.registers.gpr[2].word == 1 &&
        unknownPredicate.parentProgress.machine.registers.gpr[2].known_mask ==
            14 &&
        unknownPredicate.lookup.invocations == 0 &&
        unknownPredicate.get(0x800d8eec) == 0);

  Fixture malformed;
  malformed.known[0x800fc9ac - Fixture::Ram + 2] = 2;
  CHECK(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.gate.invocations == 1 &&
        malformed.gate.progress.stopped_pc == 0x8007a46c &&
        malformed.parentProgress.stopped_pc == 0x80079978 &&
        malformed.parentProgress.machine.registers.gpr[3].word == 0x80100000 &&
        malformed.lookup.invocations == 0 && malformed.get(0x800d8eec) == 0);

  Fixture missing;
  const std::size_t gap = 0x800fc9ac - Fixture::Ram;
  std::array<Nba97GameTextRegion, 2> regions{{
      {Fixture::Ram, missing.bytes.data(), missing.known.data(), gap},
      {0x800fc9b0, missing.bytes.data() + gap + 4,
       missing.known.data() + gap + 4, missing.bytes.size() - gap - 4},
  }};
  missing.parent.memory = {regions.data(), regions.size()};
  CHECK(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.gate.progress.stopped_address == 0x800fc9ac &&
        missing.parentProgress.stopped_pc == 0x80079978 &&
        missing.get(0x800d8eec) == 0);

  Fixture lookupBudget;
  lookupBudget.put(0x800fc9ac, 51);
  lookupBudget.lookup.operation_budget = 0;
  CHECK(lookupBudget.run() == NBA97_TEXT_LIMIT &&
        lookupBudget.gate.completions == 1 &&
        lookupBudget.lookup.invocations == 1 &&
        lookupBudget.lookup.progress.stopped_pc == 0x8007a414 &&
        lookupBudget.parentProgress.stopped_pc == 0x8007999c &&
        lookupBudget.get(0x800bc1f4) == 55 &&
        lookupBudget.get(0x800d8eec) == 0);

  Fixture reused;
  CHECK(reused.run() == NBA97_TEXT_COMPLETE);
  CHECK(reused.run() == NBA97_TEXT_COMPLETE && reused.gate.invocations == 1 &&
        reused.gate.completions == 1 && reused.lookup.invocations == 0);
}

void NullFailures() {
  Fixture fixture;
  auto event = exactGateEvent();
  auto machine = exactGateMachine();
  CHECK(nba97_game_camera_remainder_gate_from_elapsed_dispatch(
            nullptr, &fixture.parent.memory, &event, &machine) == 0);
  CHECK(nba97_game_camera_remainder_gate_from_elapsed_dispatch(
            &fixture.gate, nullptr, &event, &machine) == 0);
  CHECK(nba97_game_camera_remainder_gate_from_elapsed_dispatch(
            &fixture.gate, &fixture.parent.memory, nullptr, &machine) == 0);
  CHECK(nba97_game_camera_remainder_gate_from_elapsed_dispatch(
            &fixture.gate, &fixture.parent.memory, &event, nullptr) == 0);
  CHECK(nba97_game_camera_elapsed_dispatch_with_remainder_gate(
            nullptr, &fixture.gate, &fixture.lookup, &fixture.parentProgress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_elapsed_dispatch_with_remainder_gate(
            &fixture.parent, nullptr, &fixture.lookup,
            &fixture.parentProgress) == NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_elapsed_dispatch_with_remainder_gate(
            &fixture.parent, &fixture.gate, nullptr, &fixture.parentProgress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_camera_elapsed_dispatch_with_remainder_gate(
            &fixture.parent, &fixture.gate, &fixture.lookup, nullptr) ==
        NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  GateOneKeepsCacheAndGateZeroRefreshes();
  ExactGuardsAndBindingReuse();
  FailurePrefixesAndWrapperReuse();
  NullFailures();
  std::printf("game camera remainder integration: %u checks\n", checks);
}
