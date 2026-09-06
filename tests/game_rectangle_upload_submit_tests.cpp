#include "recovered/game_rectangle_upload_submit.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
#define check(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      std::cerr << "check failed: " #condition " at line " << __LINE__ << '\n'; \
      std::abort();                                                             \
    }                                                                           \
  } while (0)

struct Call {
  Nba97GameRectangleUploadSubmitEvent event{};
  Nba97GameRectangleUploadSubmitMachine machine{};
};

struct Fixture {
  static constexpr std::uint32_t Base = 0x80000000u;
  static constexpr std::uint32_t Sp = 0x801ff000u;
  static constexpr std::uint32_t Rectangle = 0x80010000u;
  static constexpr std::uint32_t Payload = 0x80010100u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97GameRectangleUploadSubmitMachine machine{};
  Nba97GameRectangleUploadSubmitProgress progress{};
  std::array<Nba97GameRectangleUploadSubmitAccess, 32> journal{};
  std::vector<Call> calls;
  std::size_t budget = 32;
  int refuseKind = 0;
  int invalidKind = 0;
  bool mutateFirst = false;
  bool mutateSecond = false;
  std::uint32_t poisonAddress = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x10000000u + i * 0x01010101u, 15};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[4] = {Rectangle, 15};
    machine.registers.gpr[5] = {Payload, 15};
    machine.registers.gpr[16] = {0x16161616u, 15};
    machine.registers.gpr[17] = {0x17171717u, 15};
    machine.registers.gpr[29] = {Sp, 15};
    machine.registers.gpr[31] = {0x80012340u, 15};
    machine.hi = {0xabcdef01u, 5};
    machine.lo = {0x12345678u, 10};
    put(0x800d7b14u, 0, 4);
  }
  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i)
      bytes[address - Base + i] = std::uint8_t(value >> (i * 8u));
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[address - Base + i]) << (i * 8u);
    return value;
  }
  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameRectangleUploadSubmitEvent *event,
                      Nba97GameRectangleUploadSubmitMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.calls.push_back({*event, *machine});
    if (f.refuseKind == event->kind)
      return 0;
    if (f.mutateFirst &&
        event->kind == NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_80094440) {
      machine->registers.gpr[16] = {0x80010200u, 15};
      machine->registers.gpr[17] = {0x80010300u, 15};
      machine->registers.gpr[29] = {Sp - 0x100u, 15};
    }
    if (f.mutateSecond &&
        event->kind == NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_8009971C) {
      machine->registers.gpr[8] = {0x88888888u, 3};
      machine->registers.gpr[16] = {0xfeed0000u, 15};
      machine->registers.gpr[17] = {0xfeed0001u, 15};
      machine->hi = {0x87654321u, 6};
      machine->lo = {0x10203040u, 9};
      if (f.poisonAddress)
        f.known[f.poisonAddress - Base] = 2;
    }
    if (f.invalidKind == event->kind)
      machine->lo.known_mask = 16;
    return 1;
  }
  int run() {
    Nba97GameRectangleUploadSubmitContext context{};
    context.memory = {&region, 1};
    context.operation_budget = budget;
    context.machine = machine;
    context.io = callback;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    return nba97_game_rectangle_upload_submit(&context, &progress);
  }
};

bool sameWord(const Nba97GameRectangleUploadSubmitWord &a,
              const Nba97GameRectangleUploadSubmitWord &b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}
bool sameMachine(const Nba97GameRectangleUploadSubmitMachine &a,
                 const Nba97GameRectangleUploadSubmitMachine &b) {
  for (unsigned i = 0; i < 32; ++i)
    if (!sameWord(a.registers.gpr[i], b.registers.gpr[i]))
      return false;
  return sameWord(a.hi, b.hi) && sameWord(a.lo, b.lo);
}

void normalAllInstructions() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.progress.completed &&
        f.progress.instruction_count == 19 && f.progress.operations == 9 &&
        f.progress.stopped_pc == 0);
  check(f.calls.size() == 2);
  check(f.calls[0].event.kind == NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_80094440 &&
        f.calls[0].event.pc == 0x80094508u &&
        f.calls[0].event.delay_slot_pc == 0x8009450cu &&
        f.calls[0].event.entry == 0x80094440u &&
        f.calls[0].event.argument_count == 1 &&
        f.calls[0].machine.registers.gpr[4].word == Fixture::Rectangle &&
        f.calls[0].machine.registers.gpr[17].word == Fixture::Payload &&
        f.calls[0].machine.registers.gpr[31].word == 0x80094510u);
  check(f.calls[1].event.kind == NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_8009971C &&
        f.calls[1].event.pc == 0x80094514u &&
        f.calls[1].event.delay_slot_pc == 0x80094518u &&
        f.calls[1].event.entry == 0x8009971cu &&
        f.calls[1].event.argument_count == 2 &&
        f.calls[1].machine.registers.gpr[4].word == Fixture::Rectangle &&
        f.calls[1].machine.registers.gpr[5].word == Fixture::Payload &&
        f.calls[1].machine.registers.gpr[31].word == 0x8009451cu);
  check(f.get(0x800d7b14u, 4) == 1 &&
        f.progress.machine.registers.gpr[1].word == 0x800d0000u &&
        f.progress.machine.registers.gpr[2].word == 1 &&
        f.progress.machine.registers.gpr[16].word == 0x16161616u &&
        f.progress.machine.registers.gpr[17].word == 0x17171717u &&
        f.progress.machine.registers.gpr[29].word == Fixture::Sp &&
        f.progress.machine.registers.gpr[31].word == 0x80012340u);
  const std::array<std::uint32_t, 7> pcs{
      0x800944f8u, 0x80094500u, 0x80094504u, 0x80094524u,
      0x80094528u, 0x8009452cu, 0x80094530u};
  check(f.progress.access_events == pcs.size());
  for (std::size_t i = 0; i < pcs.size(); ++i)
    check(f.journal[i].pc == pcs[i]);
}

void callbackLiveFrameAndState() {
  Fixture f;
  f.mutateFirst = true;
  f.mutateSecond = true;
  const auto frame = Fixture::Sp - 0x100u;
  f.put(frame + 0x18u, 0x80056780u, 4);
  f.put(frame + 0x14u, 0x17170000u, 4);
  f.put(frame + 0x10u, 0x16160000u, 4);
  check(f.run() == NBA97_TEXT_COMPLETE);
  check(f.calls[1].machine.registers.gpr[4].word == 0x80010200u &&
        f.calls[1].machine.registers.gpr[5].word == 0x80010300u);
  check(f.progress.machine.registers.gpr[8].word == 0x88888888u &&
        f.progress.machine.registers.gpr[8].known_mask == 3 &&
        f.progress.machine.hi.word == 0x87654321u &&
        f.progress.machine.hi.known_mask == 6 &&
        f.progress.machine.lo.word == 0x10203040u &&
        f.progress.machine.lo.known_mask == 9);
  check(f.progress.machine.registers.gpr[16].word == 0x16160000u &&
        f.progress.machine.registers.gpr[17].word == 0x17170000u &&
        f.progress.machine.registers.gpr[31].word == 0x80056780u &&
        f.progress.machine.registers.gpr[29].word == Fixture::Sp - 0xe0u);
}

void callbackFailuresAndFlagPrefix() {
  for (const int kind : {NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_80094440,
                         NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_8009971C}) {
    Fixture refused;
    refused.refuseKind = kind;
    check(refused.run() == NBA97_TEXT_IO_REFUSED &&
          refused.get(0x800d7b14u, 4) == 0 &&
          refused.progress.stopped_entry ==
              (kind == NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_80094440
                   ? 0x80094440u
                   : 0x8009971cu));
    Fixture invalid;
    invalid.invalidKind = kind;
    check(invalid.run() == NBA97_TEXT_ARGUMENT &&
          invalid.get(0x800d7b14u, 4) == 0 &&
          invalid.progress.machine.lo.known_mask == 16);
  }
  Fixture absent;
  Nba97GameRectangleUploadSubmitContext context{};
  context.memory = {&absent.region, 1};
  context.operation_budget = 32;
  context.machine = absent.machine;
  check(nba97_game_rectangle_upload_submit(&context, &absent.progress) ==
            NBA97_TEXT_IO_REFUSED &&
        absent.progress.stopped_pc == 0x80094508u);
}

void budgetsUnknownnessAndLateLoad() {
  Fixture complete;
  check(complete.run() == NBA97_TEXT_COMPLETE);
  for (std::size_t budget = 0; budget < complete.progress.operations; ++budget) {
    Fixture limited;
    limited.budget = budget;
    check(limited.run() == NBA97_TEXT_LIMIT &&
          limited.progress.operations == budget &&
          limited.get(0x800d7b14u, 4) == (budget > 5 ? 1u : 0u));
  }

  for (unsigned mask = 0; mask < 16; ++mask) {
    Fixture ra;
    ra.machine.registers.gpr[31].known_mask = std::uint8_t(mask);
    const int result = ra.run();
    check((mask == 15 && result == NBA97_TEXT_COMPLETE) ||
          (mask != 15 && result == NBA97_TEXT_UNKNOWN &&
           ra.progress.stopped_pc == 0x80094538u));
  }

  Fixture unknownSp;
  unknownSp.machine.registers.gpr[29].known_mask = 7;
  check(unknownSp.run() == NBA97_TEXT_UNKNOWN &&
        unknownSp.progress.stopped_pc == 0x800944f8u);
  Fixture unaligned;
  unaligned.machine.registers.gpr[29].word += 2;
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        unaligned.progress.stopped_pc == 0x800944f8u);

  Fixture unknownSaved;
  unknownSaved.machine.registers.gpr[16].known_mask = 0;
  unknownSaved.machine.registers.gpr[17].known_mask = 5;
  check(unknownSaved.run() == NBA97_TEXT_COMPLETE);
  const auto saved = Fixture::Sp - 0x20u;
  check(unknownSaved.known[saved + 0x10u - Fixture::Base] == 0 &&
        unknownSaved.progress.machine.registers.gpr[16].known_mask == 0 &&
        unknownSaved.progress.machine.registers.gpr[17].known_mask == 5);

  Fixture noPlane;
  noPlane.region.known = nullptr;
  noPlane.machine.registers.gpr[16].known_mask = 14;
  check(noPlane.run() == NBA97_TEXT_ARGUMENT &&
        noPlane.progress.stopped_pc == 0x800944f8u &&
        noPlane.get(saved + 0x10u, 4) == 0);

  Fixture late;
  late.mutateSecond = true;
  late.poisonAddress = saved + 0x10u;
  const int lateResult = late.run();
  check(lateResult == NBA97_TEXT_ARGUMENT &&
        late.progress.stopped_pc == 0x80094530u &&
        late.progress.machine.registers.gpr[16].word == 0xfeed0000u &&
        late.progress.machine.registers.gpr[17].word == 0x17171717u);
}

void memoryValidationAndDeterminism() {
  Fixture f;
  Nba97GameRectangleUploadSubmitContext context{};
  Nba97GameRectangleUploadSubmitProgress progress{};
  context.machine = f.machine;
  std::uint8_t byte = 0;
  Nba97GameTextRegion bad{0xffffffffu, &byte, nullptr, 2};
  context.memory = {&bad, 1};
  check(nba97_game_rectangle_upload_submit(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);
  bad = {0, &byte, nullptr, SIZE_MAX};
  check(nba97_game_rectangle_upload_submit(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);
  std::array<std::uint8_t, 8> data{};
  std::array<Nba97GameTextRegion, 2> overlap{{
      {0x1000u, data.data(), nullptr, 8},
      {0x1004u, data.data(), nullptr, 4},
  }};
  context.memory = {overlap.data(), overlap.size()};
  check(nba97_game_rectangle_upload_submit(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);
  bad = {0, &byte, nullptr, 0};
  context.memory = {&bad, 1};
  check(nba97_game_rectangle_upload_submit(&context, &progress) ==
        NBA97_TEXT_ARGUMENT);

  bad = {0, &byte, nullptr, std::size_t(UINT64_C(0x100000000))};
  context.memory = {&bad, 1};
  context.operation_budget = 0;
  check(nba97_game_rectangle_upload_submit(&context, &progress) ==
        NBA97_TEXT_LIMIT);

  std::array<std::uint8_t, 4> holeData{};
  Nba97GameTextRegion hole{0x800d7b14u, holeData.data(), nullptr,
                           holeData.size()};
  context.memory = {&hole, 1};
  context.operation_budget = 32;
  check(nba97_game_rectangle_upload_submit(&context, &progress) ==
            NBA97_TEXT_RESOURCE &&
        progress.stopped_pc == 0x800944f8u);

  std::array<std::uint8_t, 12> low{};
  std::array<std::uint8_t, 12> lowKnown{};
  std::array<std::uint8_t, 4> flag{};
  std::array<std::uint8_t, 4> flagKnown{{1, 1, 1, 1}};
  lowKnown.fill(1);
  std::array<Nba97GameTextRegion, 2> wrapped{{
      {0, low.data(), lowKnown.data(), low.size()},
      {0x800d7b14u, flag.data(), flagKnown.data(), flag.size()},
  }};
  context.memory = {wrapped.data(), wrapped.size()};
  context.operation_budget = 32;
  context.machine = f.machine;
  context.machine.registers.gpr[29] = {0x10u, 15};
  context.io = Fixture::callback;
  context.user = &f;
  check(nba97_game_rectangle_upload_submit(&context, &progress) ==
            NBA97_TEXT_COMPLETE &&
        progress.frame_stack_pointer == 0xfffffff0u &&
        progress.machine.registers.gpr[29].word == 0x10u && flag[0] == 1);

  Fixture left;
  Fixture right;
  check(left.run() == NBA97_TEXT_COMPLETE && right.run() == NBA97_TEXT_COMPLETE &&
        left.bytes == right.bytes && left.known == right.known &&
        sameMachine(left.progress.machine, right.progress.machine));
}
} // namespace

int main() {
  normalAllInstructions();
  callbackLiveFrameAndState();
  callbackFailuresAndFlagPrefix();
  budgetsUnknownnessAndLateLoad();
  memoryValidationAndDeterminism();
  std::cout << "game_rectangle_upload_submit_tests: PASS\n";
}
