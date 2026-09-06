#include "game_rectangle_normalize_adapter.h"

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

struct Fixture {
  static constexpr std::uint32_t Base = 0x80000000u;
  static constexpr std::uint32_t Sp = 0x801ff000u;
  static constexpr std::uint32_t Rectangle = 0x80010000u;
  static constexpr std::uint32_t Payload = 0x80010100u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97GameRectangleUploadSubmitMachine entry{};
  Nba97GameRectangleUploadSubmitProgress parentProgress{};
  Nba97GameRectangleNormalizeBinding binding{};
  std::array<Nba97GameRectangleNormalizeAccess, 8> journal{};
  unsigned submitCalls = 0;
  std::uint32_t submitA0 = 0;
  std::uint32_t submitA1 = 0;
  bool refuseSubmit = false;

  Fixture(std::uint16_t width = 16, std::uint16_t height = 1) {
    for (unsigned i = 0; i < 32; ++i)
      entry.registers.gpr[i] = {0x33000000u + i * 0x01010101u, 15};
    entry.registers.gpr[0] = {0, 15};
    entry.registers.gpr[4] = {Rectangle, 15};
    entry.registers.gpr[5] = {Payload, 15};
    entry.registers.gpr[29] = {Sp, 15};
    entry.registers.gpr[31] = {0x80012340u, 15};
    entry.hi = {0x12345678u, 5};
    entry.lo = {0x87654321u, 10};
    put(Rectangle + 4u, width, 2);
    put(Rectangle + 6u, height, 2);
    put(0x800d7b14u, 0, 4);
    binding.operation_budget = 3;
    binding.access_journal = journal.data();
    binding.access_journal_capacity = journal.size();
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
  static int fallback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameRectangleUploadSubmitEvent *event,
                      Nba97GameRectangleUploadSubmitMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (event->kind != NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_8009971C)
      return 0;
    ++f.submitCalls;
    f.submitA0 = machine->registers.gpr[4].word;
    f.submitA1 = machine->registers.gpr[5].word;
    return f.refuseSubmit ? 0 : 1;
  }
  int run() {
    Nba97GameRectangleUploadSubmitContext parent{};
    parent.memory = {&region, 1};
    parent.operation_budget = 32;
    parent.machine = entry;
    parent.io = fallback;
    parent.user = this;
    return nba97_game_rectangle_upload_submit_with_rectangle_normalize(
        &parent, &binding, &parentProgress);
  }
};

void actualSubmitEvenAndOdd() {
  Fixture even;
  check(even.run() == NBA97_TEXT_COMPLETE && even.parentProgress.completed &&
        even.binding.progress.completed && even.binding.invocations == 1 &&
        even.binding.completions == 1 && even.submitCalls == 1);
  check(even.binding.event.kind ==
            NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_80094440 &&
        even.binding.event.pc == 0x80094508u &&
        even.binding.event.delay_slot_pc == 0x8009450cu &&
        even.binding.event.entry == 0x80094440u &&
        even.binding.event.invocation == 1 &&
        even.binding.event.argument_count == 1);
  check(even.binding.progress.operations == 1 &&
        even.get(Fixture::Rectangle + 6u, 2) == 1 &&
        even.submitA0 == Fixture::Rectangle && even.submitA1 == Fixture::Payload &&
        even.get(0x800d7b14u, 4) == 1);

  Fixture odd(17, 2);
  check(odd.run() == NBA97_TEXT_COMPLETE && odd.binding.progress.operations == 3 &&
        odd.get(Fixture::Rectangle + 6u, 2) == 3 && odd.submitCalls == 1 &&
        odd.get(0x800d7b14u, 4) == 1);

  even.put(0x800d7b14u, 0, 4);
  check(even.run() == NBA97_TEXT_COMPLETE && even.binding.invocations == 2 &&
        even.binding.completions == 2);
}

void childAndParentFailurePrefixes() {
  Fixture limited(17, 2);
  limited.binding.operation_budget = 1;
  check(limited.run() == NBA97_TEXT_LIMIT && limited.binding.invocations == 1 &&
        limited.binding.progress.stopped_pc == 0x80094454u &&
        limited.parentProgress.stopped_pc == 0x80094508u &&
        limited.submitCalls == 0 && limited.get(0x800d7b14u, 4) == 0);

  Fixture refused(17, 2);
  refused.refuseSubmit = true;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.binding.result == NBA97_TEXT_COMPLETE &&
        refused.get(Fixture::Rectangle + 6u, 2) == 3 &&
        refused.parentProgress.stopped_pc == 0x80094514u &&
        refused.get(0x800d7b14u, 4) == 0);
}

void exactAdapterGuards() {
  Fixture f;
  Nba97GameTextMemory memory{&f.region, 1};
  Nba97GameRectangleUploadSubmitEvent event{
      0x80094508u, 0x8009450cu, 0x80094440u, 1, 1,
      NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_80094440, 1};
  auto machine = f.entry;
  machine.registers.gpr[31] = {0x80094510u, 15};
  auto reject = [&](const Nba97GameRectangleUploadSubmitEvent &candidate,
                    const Nba97GameRectangleUploadSubmitMachine &candidateMachine) {
    auto copy = candidateMachine;
    check(nba97_game_rectangle_normalize_from_rectangle_upload_submit(
              &f.binding, &memory, &candidate, &copy) == 0 &&
          f.binding.result == NBA97_TEXT_ARGUMENT &&
          copy.registers.gpr[4].word == candidateMachine.registers.gpr[4].word);
  };
  auto malformed = event;
  malformed.pc ^= 4u;
  reject(malformed, machine);
  malformed = event;
  malformed.delay_slot_pc ^= 4u;
  reject(malformed, machine);
  malformed = event;
  malformed.entry ^= 4u;
  reject(malformed, machine);
  malformed = event;
  malformed.kind = NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_8009971C;
  reject(malformed, machine);
  malformed = event;
  malformed.invocation = 2;
  reject(malformed, machine);
  malformed = event;
  malformed.argument_count = 2;
  reject(malformed, machine);
  auto wrongRa = machine;
  wrongRa.registers.gpr[31].word ^= 4u;
  reject(event, wrongRa);
}
} // namespace

int main() {
  actualSubmitEvenAndOdd();
  childAndParentFailurePrefixes();
  exactAdapterGuards();
  std::cout << "game_rectangle_normalize_integration_tests: PASS\n";
}
