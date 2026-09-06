#include "recovered/game_pregame_match_card.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "pregame match card check %u failed at %u\n", checks,
                 line);
    std::exit(1);
  }
}
#define check(value) checkAt((value), __LINE__)

struct Call {
  Nba97GamePregameMatchCardEvent event{};
  Nba97GamePregameMatchCardMachine machine{};
  std::array<std::uint32_t, 4> stack{};
};

struct Fixture {
  static constexpr std::uint32_t Ram = 0x80000000u;
  static constexpr std::uint32_t Sp = 0x801ff000u;
  static constexpr std::uint32_t Font = 0x80030000u;
  static constexpr std::uint32_t TeamA = 0x80040000u;
  static constexpr std::uint32_t TeamB = 0x80041000u;
  static constexpr std::uint32_t LocationText = 0x80050000u;

  std::vector<std::uint8_t> data = std::vector<std::uint8_t>(0x200000, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(data.size(), 1);
  Nba97GameTextRegion region{Ram, data.data(), known.data(), data.size()};
  std::vector<Nba97GamePregameMatchCardAccess> journal =
      std::vector<Nba97GamePregameMatchCardAccess>(512);
  Nba97GamePregameMatchCardContext context{};
  Nba97GamePregameMatchCardProgress progress{};
  std::vector<Call> calls;
  std::vector<std::uint32_t> times{100, 110};
  std::vector<std::uint32_t> inputs{0x180};
  std::vector<std::uint32_t> readiness{1};
  std::uint32_t locationResult = LocationText;
  unsigned refuseKind = 0;
  unsigned refuseInvocation = 1;
  bool invalidReturnedMachine = false;
  bool relocateFrame = false;
  bool corruptSavedRaKnown = false;
  bool misalignSavedRa = false;
  std::uint8_t inputMask = 15;
  std::uint8_t readinessMask = 15;

  Fixture() {
    context.memory = {&region, 1};
    context.operation_budget = std::numeric_limits<std::size_t>::max();
    context.machine.registers.gpr[0] = {0, 15};
    for (unsigned i = 1; i < 32; ++i)
      context.machine.registers.gpr[i] = {0x51000000u + i * 0x101u,
                                          static_cast<std::uint8_t>(i & 15u)};
    context.machine.registers.gpr[29] = {Sp, 15};
    context.machine.registers.gpr[31] = {0x81234568u, 15};
    context.machine.hi = {0x13579bdfu, 5};
    context.machine.lo = {0x2468ace0u, 10};
    context.io = callback;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    put(0x800b2048u, Font, 4);
    put(0x8001ef24u, TeamA, 4);
    put(0x8001ee60u, TeamB, 4);
    put(TeamA + 0x40u, 0x80060000u, 4);
    put(TeamB + 0x40u, 0x80061000u, 4);
    put(TeamB + 0x4cu, LocationText, 4);
    put(0x8001ec94u, 0, 4);
    put(0x8001edecu, 0, 2);
    put(0x800fdb78u, 0x5a, 1);
    put(0x800eb680u, 0x7c, 1);
  }

  std::size_t at(std::uint32_t address) const { return address - Ram; }
  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i) {
      data[at(address) + i] = static_cast<std::uint8_t>(value >> (i * 8u));
      known[at(address) + i] = 1;
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(data[at(address) + i]) << (i * 8u);
    return value;
  }
  void writeHalf(std::uint32_t address, std::uint16_t value) {
    put(address, value, 2);
  }
  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GamePregameMatchCardEvent *event,
                      Nba97GamePregameMatchCardMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    Call call{*event, *machine, {}};
    if (machine->registers.gpr[29].known_mask == 15)
      for (unsigned i = 0; i < 4; ++i)
        call.stack[i] =
            f.get(machine->registers.gpr[29].word + 0x10u + i * 4u, 4);
    f.calls.push_back(call);
    if (f.refuseKind == event->kind && f.refuseInvocation == event->invocation)
      return 0;
    auto &v0 = machine->registers.gpr[2];
    switch (event->kind) {
    case NBA97_GAME_PREGAME_MATCH_CARD_CALL_80081B50:
      v0 = {f.locationResult, 15};
      break;
    case NBA97_GAME_PREGAME_MATCH_CARD_CALL_800A5810: {
      const auto index = static_cast<std::size_t>(event->invocation - 1);
      v0 = {f.times[index < f.times.size() ? index : f.times.size() - 1], 15};
      break;
    }
    case NBA97_GAME_PREGAME_MATCH_CARD_CALL_800363DC:
      f.writeHalf(machine->registers.gpr[4].word, 7);
      f.writeHalf(machine->registers.gpr[5].word, 9);
      break;
    case NBA97_GAME_PREGAME_MATCH_CARD_CALL_80036478: {
      const auto index = static_cast<std::size_t>(event->invocation - 1);
      v0 = {f.inputs[index < f.inputs.size() ? index : f.inputs.size() - 1],
            f.inputMask};
      break;
    }
    case NBA97_GAME_PREGAME_MATCH_CARD_CALL_80088D0C: {
      const auto index = static_cast<std::size_t>(event->invocation - 1);
      v0 = {f.readiness[index < f.readiness.size() ? index
                                                   : f.readiness.size() - 1],
            f.readinessMask};
      break;
    }
    case NBA97_GAME_PREGAME_MATCH_CARD_CALL_8008048C:
      v0 = {0x89abcdefu, 15};
      if (f.invalidReturnedMachine)
        machine->hi.known_mask = 16;
      if (f.corruptSavedRaKnown)
        f.known[f.at(machine->registers.gpr[29].word + 0x64u)] = 0;
      if (f.misalignSavedRa)
        f.put(machine->registers.gpr[29].word + 0x64u, 0x81234569u, 4);
      break;
    default:
      break;
    }
    if (f.relocateFrame && event->pc == 0x80044568u) {
      const std::uint32_t oldSp = machine->registers.gpr[29].word;
      const std::uint32_t newSp = oldSp - 0x200u;
      for (unsigned i = 0; i < 0x68; ++i) {
        f.data[f.at(newSp) + i] = f.data[f.at(oldSp) + i];
        f.known[f.at(newSp) + i] = f.known[f.at(oldSp) + i];
      }
      machine->registers.gpr[29] = {newSp, 15};
    }
    return 1;
  }
  int run() { return nba97_game_pregame_match_card(&context, &progress); }
  const Call &call(unsigned kind, unsigned invocation = 1) const {
    for (const auto &value : calls)
      if (value.event.kind == kind && value.event.invocation == invocation)
        return value;
    std::abort();
  }
};

void normalCallOrderAndArguments() {
  Fixture f;
  const auto entry = f.context.machine;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed);
  check(f.calls.size() == 35 && f.progress.callbacks_completed == 35);
  const std::array<std::uint32_t, 35> entries{
      0x800810a4u, 0x8003081cu, 0x80031614u, 0x80031614u, 0x80031614u,
      0x80031614u, 0x80031614u, 0x80031614u, 0x80031614u, 0x80030d18u,
      0x80030d18u, 0x80030d18u, 0x80036688u, 0x80030d18u, 0x8009cb6cu,
      0x80030d18u, 0x80036688u, 0x80030d18u, 0x8009cb6cu, 0x80030d18u,
      0x8009cb7cu, 0x80030d18u, 0x800a5810u, 0x800363dcu, 0x80083eecu,
      0x80036478u, 0x800a5810u, 0x80088d0cu, 0x8002de34u, 0x80029880u,
      0x80049018u, 0x80029258u, 0x80036600u, 0x8003081cu, 0x8008048cu};
  for (std::size_t i = 0; i < entries.size(); ++i) {
    check(f.calls[i].event.entry == entries[i]);
    check(f.calls[i].event.delay_slot_pc == f.calls[i].event.pc + 4u);
    check(f.calls[i].machine.registers.gpr[31].word ==
          f.calls[i].event.pc + 8u);
  }
  const auto &firstLayout =
      f.call(NBA97_GAME_PREGAME_MATCH_CARD_CALL_80031614, 1);
  check(firstLayout.event.pc == 0x800445a8u &&
        firstLayout.event.argument_count == 8);
  check(firstLayout.machine.registers.gpr[4].word == UINT32_MAX &&
        firstLayout.machine.registers.gpr[5].word == 0x59u &&
        firstLayout.machine.registers.gpr[6].word == 0x82u &&
        firstLayout.machine.registers.gpr[7].word == 0x34u);
  check(firstLayout.stack[0] == 0 && firstLayout.stack[1] == 0x131u &&
        firstLayout.stack[2] == 8 && firstLayout.stack[3] == 0);
  const std::array<std::array<std::uint32_t, 7>, 7> layouts{{
      {0x59, 0x82, 0x34, 0, 0x131, 8, 0},
      {0x69, 0x82, 0x3c, 0, 0x131, 0x28, 1},
      {0x63, 0x55, 0x64, 0, 0x15e, 0x0c, 0},
      {0x59, 0x82, 0x70, 0, 0x131, 8, 0},
      {0x69, 0x82, 0x78, 0, 0x131, 0x28, 1},
      {0x59, 0x55, 0xa0, 0, 0x15e, 8, 0},
      {0x2f, 0x55, 0xa8, 0, 0x15e, 0x12, 0},
  }};
  for (unsigned i = 0; i < layouts.size(); ++i) {
    const auto &call =
        f.call(NBA97_GAME_PREGAME_MATCH_CARD_CALL_80031614, i + 1);
    check(call.machine.registers.gpr[4].word == UINT32_MAX);
    check(call.machine.registers.gpr[5].word == layouts[i][0] &&
          call.machine.registers.gpr[6].word == layouts[i][1] &&
          call.machine.registers.gpr[7].word == layouts[i][2]);
    for (unsigned slot = 0; slot < 4; ++slot)
      check(call.stack[slot] == layouts[i][slot + 3]);
  }

  const std::array<std::array<std::uint32_t, 4>, 8> texts{{
      {0x82, 0x34, 2, 1},
      {0x82, 0x70, 2, 1},
      {0x81, 0x62, 1, 0},
      {0x96, 0x3e, 0, 1},
      {0x96, 0x4a, 0, 1},
      {0x96, 0x7a, 0, 1},
      {0x96, 0x86, 0, 1},
      {0x96, 0xa8, 0, 1},
  }};
  for (unsigned i = 0; i < texts.size(); ++i) {
    const auto &call =
        f.call(NBA97_GAME_PREGAME_MATCH_CARD_CALL_80030D18, i + 1);
    check(call.event.argument_count == 5 &&
          call.machine.registers.gpr[4].word == UINT32_MAX &&
          call.machine.registers.gpr[6].word == texts[i][0] &&
          call.machine.registers.gpr[7].word == texts[i][1] &&
          call.stack[0] == texts[i][2]);
    if (texts[i][3])
      check(call.machine.registers.gpr[5].word ==
            call.machine.registers.gpr[29].word + 0x20u);
  }
  const auto &inputRead = f.call(NBA97_GAME_PREGAME_MATCH_CARD_CALL_80036478);
  check(inputRead.machine.registers.gpr[4].word == 7 &&
        inputRead.machine.registers.gpr[5].word ==
            inputRead.machine.registers.gpr[29].word + 0x4au);
  check(f.call(NBA97_GAME_PREGAME_MATCH_CARD_CALL_80029880)
            .machine.registers.gpr[4]
            .word == 0x5fu);
  check(f.call(NBA97_GAME_PREGAME_MATCH_CARD_CALL_80029258)
            .machine.registers.gpr[4]
            .word == 0x61u);
  check(f.call(NBA97_GAME_PREGAME_MATCH_CARD_CALL_80036600)
            .machine.registers.gpr[4]
            .word == 9u);
  check(f.get(Fixture::Font + 0x26u, 2) == 0);
  check(f.get(0x800eb680u, 1) == 0);
  check(f.get(0x800fdb78u, 1) == 0x5a);
  check(f.progress.exited_for_input && !f.progress.exited_for_timeout);
  check(f.progress.machine.registers.gpr[2].word == 0x89abcdefu);
  check(f.progress.machine.registers.gpr[29].word == Fixture::Sp);
  for (unsigned i : {16u, 17u, 18u, 19u, 20u, 31u}) {
    check(f.progress.machine.registers.gpr[i].word ==
          entry.registers.gpr[i].word);
    check(f.progress.machine.registers.gpr[i].known_mask ==
          entry.registers.gpr[i].known_mask);
  }
  check(f.progress.machine.hi.word == entry.hi.word &&
        f.progress.machine.hi.known_mask == entry.hi.known_mask &&
        f.progress.machine.lo.word == entry.lo.word &&
        f.progress.machine.lo.known_mask == entry.lo.known_mask);
  for (unsigned i = 0; i < 32; ++i) {
    if (i >= 1 && i <= 7)
      continue;
    check(f.progress.machine.registers.gpr[i].word ==
          entry.registers.gpr[i].word);
    check(f.progress.machine.registers.gpr[i].known_mask ==
          entry.registers.gpr[i].known_mask);
  }
}

void locationAndDemoPaths() {
  for (std::uint32_t location : {0x00010000u, 0x00008000u, 0xffff7fffu}) {
    Fixture f;
    f.put(0x8001ec94u, location, 4);
    check(f.run() == NBA97_TEXT_COMPLETE);
    const auto &lookup = f.call(NBA97_GAME_PREGAME_MATCH_CARD_CALL_80081B50);
    const std::uint32_t expected =
        location == 0x00008000u ? 0xffff8000u : location & 0xffffu;
    check(lookup.machine.registers.gpr[4].word == expected);
    check(f.call(NBA97_GAME_PREGAME_MATCH_CARD_CALL_8009CB7C)
              .machine.registers.gpr[6]
              .word == Fixture::LocationText);
  }
  Fixture zero;
  check(zero.run() == NBA97_TEXT_COMPLETE);
  check(zero.progress.call_count[NBA97_GAME_PREGAME_MATCH_CARD_CALL_80081B50] ==
        0);
  check(zero.call(NBA97_GAME_PREGAME_MATCH_CARD_CALL_8009CB7C)
            .machine.registers.gpr[6]
            .word == Fixture::LocationText);

  Fixture demo;
  demo.put(0x8001edecu, 2, 2);
  check(demo.run() == NBA97_TEXT_COMPLETE);
  check(demo.progress.call_count[NBA97_GAME_PREGAME_MATCH_CARD_CALL_80035678] ==
        1);
  check(demo.get(0x800fdb78u, 1) == 1 && demo.get(0x8001edecu, 2) == 99);
}

void pollingEdges() {
  Fixture timeout;
  timeout.inputs = {1};
  timeout.times = {0, 3600};
  timeout.readiness = {0};
  check(timeout.run() == NBA97_TEXT_COMPLETE);
  check(timeout.progress.exited_for_timeout &&
        !timeout.progress.exited_for_input);
  check(timeout.progress
            .call_count[NBA97_GAME_PREGAME_MATCH_CARD_CALL_80029258] == 1);

  Fixture unrelated;
  unrelated.inputs = {1};
  unrelated.times = {0, 3600};
  unrelated.readiness = {0};
  check(unrelated.run() == NBA97_TEXT_COMPLETE &&
        unrelated.progress.polling_iterations == 2);

  Fixture threshold;
  threshold.inputs = {0};
  threshold.times = {0, 100, 200, 300, 400, 500, 600, 700};
  threshold.readiness = {0, 1};
  check(threshold.run() == NBA97_TEXT_COMPLETE);
  check(threshold.progress.exited_for_timeout &&
        threshold.progress.polling_iterations == 7);

  Fixture negative;
  negative.inputs = {0x180};
  negative.times = {100, 90};
  check(negative.run() == NBA97_TEXT_COMPLETE);
  check(negative.progress
            .call_count[NBA97_GAME_PREGAME_MATCH_CARD_CALL_8002DE34] == 0);

  Fixture overflow;
  overflow.inputs = {0x180};
  overflow.times = {0xfffffff0u, 0x10u};
  check(overflow.run() == NBA97_TEXT_COMPLETE);
  check(overflow.progress
            .call_count[NBA97_GAME_PREGAME_MATCH_CARD_CALL_8002DE34] == 1);
}

void budgetsAndRefusals() {
  Fixture full;
  check(full.run() == NBA97_TEXT_COMPLETE);
  for (std::size_t budget = 0; budget < full.progress.operations; ++budget) {
    Fixture cut;
    cut.context.operation_budget = budget;
    check(cut.run() == NBA97_TEXT_LIMIT);
    check(cut.progress.operations == budget);
  }
  for (const auto &expected : full.calls) {
    Fixture refused;
    refused.refuseKind = expected.event.kind;
    refused.refuseInvocation = static_cast<unsigned>(expected.event.invocation);
    check(refused.run() == NBA97_TEXT_IO_REFUSED);
    check(refused.progress.stopped_entry == expected.event.entry);
    check(refused.progress.stopped_pc == expected.event.pc);
  }
  for (unsigned kind :
       {unsigned(NBA97_GAME_PREGAME_MATCH_CARD_CALL_80081B50),
        unsigned(NBA97_GAME_PREGAME_MATCH_CARD_CALL_80035678)}) {
    Fixture refused;
    refused.refuseKind = kind;
    if (kind == NBA97_GAME_PREGAME_MATCH_CARD_CALL_80081B50)
      refused.put(0x8001ec94u, 1, 4);
    else
      refused.put(0x8001edecu, 1, 2);
    check(refused.run() == NBA97_TEXT_IO_REFUSED);
    check(refused.progress.stopped_entry == refused.calls.back().event.entry);
  }
}

void unknownAndFailurePrefixes() {
  Fixture location;
  location.known[location.at(0x8001ec94u)] = 0;
  check(location.run() == NBA97_TEXT_UNKNOWN);
  check(location.progress.stopped_pc == 0x800447fcu);
  check(location.get(Fixture::Font + 0x26u, 2) == 0);

  Fixture demo;
  demo.known[demo.at(0x8001edecu)] = 0;
  check(demo.run() == NBA97_TEXT_UNKNOWN);
  check(demo.progress.stopped_pc == 0x80044860u);
  check(demo.progress.machine.registers.gpr[17].word == UINT32_MAX);

  Fixture readiness;
  readiness.readiness = {0};
  readiness.readinessMask = 14;
  /* Unknown Boolean E from the readiness callback stops after its branch NOP.
   */
  check(readiness.run() == NBA97_TEXT_UNKNOWN);
  check(readiness.progress.stopped_pc == 0x800448c0u);

  Fixture input;
  input.inputs = {0};
  input.inputMask = 14;
  check(input.run() == NBA97_TEXT_UNKNOWN);
  check(input.progress.stopped_pc == 0x80044910u);
  check(input.progress.machine.registers.gpr[2].known_mask == 15);

  Fixture malformed;
  malformed.invalidReturnedMachine = true;
  check(malformed.run() == NBA97_TEXT_ARGUMENT);
  check(malformed.progress.stopped_pc == 0x8004496cu);
  check(malformed.progress.machine.hi.known_mask == 16);
}

void booleanKnownnessMasks() {
  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture input;
    input.inputs = {0x180};
    input.inputMask = static_cast<std::uint8_t>(mask);
    const int inputResult = input.run();
    if (mask & 3u)
      check(inputResult == NBA97_TEXT_COMPLETE &&
            input.progress.exited_for_input);
    else
      check(inputResult == NBA97_TEXT_UNKNOWN &&
            input.progress.stopped_pc == 0x80044910u);

    Fixture ready;
    ready.readiness = {1};
    ready.readinessMask = static_cast<std::uint8_t>(mask);
    const int readyResult = ready.run();
    if (mask & 1u)
      check(readyResult == NBA97_TEXT_COMPLETE);
    else
      check(readyResult == NBA97_TEXT_UNKNOWN &&
            ready.progress.stopped_pc == 0x800448c0u);
  }
}

void relocatedFrameAndReturnFailures() {
  Fixture relocated;
  relocated.relocateFrame = true;
  check(relocated.run() == NBA97_TEXT_COMPLETE);
  check(relocated.progress.machine.registers.gpr[29].word ==
        Fixture::Sp - 0x200u);

  Fixture partial;
  partial.corruptSavedRaKnown = true;
  check(partial.run() == NBA97_TEXT_UNKNOWN);
  check(partial.progress.stopped_pc == 0x80044990u);
  check(partial.progress.machine.registers.gpr[29].word == Fixture::Sp);

  Fixture misaligned;
  misaligned.misalignSavedRa = true;
  check(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP);
  check(misaligned.progress.stopped_pc == 0x80044990u);
}

void stalledPollingAndStackGlobalAlias() {
  Fixture stalled;
  stalled.inputs = {0};
  stalled.times = {100, 100};
  stalled.readiness = {1};
  stalled.context.operation_budget = 150;
  check(stalled.run() == NBA97_TEXT_LIMIT);
  check(stalled.progress.operations == 150 &&
        stalled.progress.stopped_pc == 0x800448a0u &&
        stalled.progress.polling_iterations == 11 &&
        stalled.progress.callbacks_completed == 75 &&
        !stalled.progress.completed && !stalled.progress.exited_for_input &&
        !stalled.progress.exited_for_timeout);
  check(stalled.progress
                .call_count[NBA97_GAME_PREGAME_MATCH_CARD_CALL_80083EEC] ==
            11 &&
        stalled.progress
                .call_count[NBA97_GAME_PREGAME_MATCH_CARD_CALL_80036478] == 10);
  check(stalled.progress.machine.registers.gpr[18].word == 0 &&
        stalled.progress.machine.registers.gpr[17].word == UINT32_MAX &&
        stalled.get(0x800eb680u, 1) == 0x7c);

  Fixture alias;
  const std::uint32_t frame = Fixture::Sp - 0x68u;
  const std::uint32_t savedRa = frame + 0x64u;
  alias.put(0x800b2048u, frame + 0x3eu, 4);
  check(alias.run() == NBA97_TEXT_COMPLETE && alias.progress.completed);
  check(alias.progress.saved_return_address.word == 0x81234568u &&
        alias.progress.restored_return_address.word == 0x81230000u &&
        alias.progress.machine.registers.gpr[31].word == 0x81230000u);
  const std::array<std::uint32_t, 6> values{0x300u, 0u, 0x200u, 0u, 0x200u, 0u};
  unsigned found = 0;
  for (std::size_t i = 0; i < alias.progress.access_events; ++i) {
    const auto &event = alias.journal[i];
    if (event.address == savedRa &&
        event.kind == NBA97_GAME_PREGAME_MATCH_CARD_STORE && event.width == 2) {
      check(found < values.size() && event.value == values[found]);
      ++found;
    }
  }
  check(found == values.size());
}

void metadataMappingAndDeterminism() {
  Fixture base;
  Nba97GamePregameMatchCardProgress progress{};
  check(nba97_game_pregame_match_card(nullptr, &progress) ==
        NBA97_TEXT_ARGUMENT);
  check(nba97_game_pregame_match_card(&base.context, nullptr) ==
        NBA97_TEXT_ARGUMENT);

  Fixture invalidMachine;
  invalidMachine.context.machine.registers.gpr[8].known_mask = 16;
  check(invalidMachine.run() == NBA97_TEXT_ARGUMENT);
  Fixture nullRegion;
  nullRegion.context.memory.region = nullptr;
  check(nullRegion.run() == NBA97_TEXT_ARGUMENT);
  Fixture nullData;
  nullData.region.data = nullptr;
  check(nullData.run() == NBA97_TEXT_ARGUMENT);
  Fixture zeroSize;
  zeroSize.region.size = 0;
  check(zeroSize.run() == NBA97_TEXT_ARGUMENT);
  Fixture overflow;
  overflow.region.base = 0xfffffff0u;
  overflow.region.size = 32;
  check(overflow.run() == NBA97_TEXT_ARGUMENT);
  Fixture huge;
  huge.region.size = std::numeric_limits<std::size_t>::max();
  check(huge.run() == NBA97_TEXT_ARGUMENT);
  Fixture overlap;
  Nba97GameTextRegion duplicate[2]{overlap.region, overlap.region};
  overlap.context.memory = {duplicate, 2};
  check(overlap.run() == NBA97_TEXT_ARGUMENT);
  Fixture badJournal;
  badJournal.context.access_journal = nullptr;
  check(badJournal.run() == NBA97_TEXT_ARGUMENT);

  Fixture unknownSp;
  unknownSp.context.machine.registers.gpr[29].known_mask = 7;
  check(unknownSp.run() == NBA97_TEXT_UNKNOWN &&
        unknownSp.progress.stopped_pc == 0x80044554u);
  Fixture unalignedSp;
  unalignedSp.context.machine.registers.gpr[29].word += 1;
  check(unalignedSp.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unalignedSp.progress.stopped_pc == 0x80044554u);
  Fixture unmappedSp;
  unmappedSp.context.machine.registers.gpr[29] = {0x90000068u, 15};
  check(unmappedSp.run() == NBA97_TEXT_RESOURCE &&
        unmappedSp.progress.stopped_pc == 0x80044554u);

  Fixture malformedLoad;
  malformedLoad.known[malformedLoad.at(0x800b2048u) + 3] = 2;
  check(malformedLoad.run() == NBA97_TEXT_ARGUMENT &&
        malformedLoad.progress.stopped_pc == 0x80044584u &&
        malformedLoad.progress.machine.registers.gpr[3].word == 0x800b0000u);

  Fixture rawUnknownStore;
  rawUnknownStore.region.known = nullptr;
  rawUnknownStore.context.machine.registers.gpr[31].known_mask = 7;
  const auto savedByte =
      rawUnknownStore.data[rawUnknownStore.at(Fixture::Sp - 4u)];
  check(rawUnknownStore.run() == NBA97_TEXT_ARGUMENT &&
        rawUnknownStore.progress.stopped_pc == 0x80044554u &&
        rawUnknownStore.data[rawUnknownStore.at(Fixture::Sp - 4u)] ==
            savedByte);

  Fixture first;
  Fixture second;
  check(first.run() == NBA97_TEXT_COMPLETE &&
        second.run() == NBA97_TEXT_COMPLETE);
  for (unsigned i = 0; i < 32; ++i) {
    check(first.progress.machine.registers.gpr[i].word ==
          second.progress.machine.registers.gpr[i].word);
    check(first.progress.machine.registers.gpr[i].known_mask ==
          second.progress.machine.registers.gpr[i].known_mask);
  }
  check(first.progress.machine.hi.word == second.progress.machine.hi.word &&
        first.progress.machine.hi.known_mask ==
            second.progress.machine.hi.known_mask &&
        first.progress.machine.lo.word == second.progress.machine.lo.word &&
        first.progress.machine.lo.known_mask ==
            second.progress.machine.lo.known_mask);
  check(first.data == second.data && first.known == second.known);
}
} // namespace

int main() {
  normalCallOrderAndArguments();
  locationAndDemoPaths();
  pollingEdges();
  budgetsAndRefusals();
  unknownAndFailurePrefixes();
  booleanKnownnessMasks();
  relocatedFrameAndReturnFailures();
  stalledPollingAndStackGlobalAlias();
  metadataMappingAndDeterminism();
  std::printf("game_pregame_match_card_tests: PASS (%u checks)\n", checks);
  return EXIT_SUCCESS;
}
