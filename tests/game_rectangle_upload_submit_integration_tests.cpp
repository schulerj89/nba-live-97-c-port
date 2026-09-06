#include "game_rectangle_upload_submit_adapter.h"

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
  static constexpr std::uint32_t Record = 0x80010000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97GameImageRecordUploadMachine entry{};
  Nba97GameImageRecordUploadProgress parentProgress{};
  Nba97GameRectangleUploadSubmitBinding binding{};
  std::array<Nba97GameRectangleUploadSubmitAccess, 32> journal{};
  unsigned normalizeCalls = 0;
  unsigned submitCalls = 0;
  unsigned fallbackCalls = 0;
  bool refuseSubmit = false;
  bool invalidateSubmit = false;
  std::uint32_t firstA0 = 0;
  std::uint32_t secondA0 = 0;
  std::uint32_t secondA1 = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      entry.registers.gpr[i] = {0x22000000u + i * 0x01010101u, 15};
    entry.registers.gpr[0] = {0, 15};
    entry.registers.gpr[4] = {Record, 15};
    entry.registers.gpr[5] = {0, 15};
    entry.registers.gpr[6] = {0, 15};
    entry.registers.gpr[7] = {0x340u, 15};
    entry.registers.gpr[29] = {Sp, 15};
    entry.registers.gpr[31] = {0x80012340u, 15};
    entry.hi = {0x12345678u, 5};
    entry.lo = {0x87654321u, 10};
    put(Sp + 0x10u, 0xf0u, 4);
    put(Record, 0x23u, 1);
    put(Record + 4u, 0x10u, 2);
    put(0x800d7b14u, 0, 4);
    binding.operation_budget = 32;
    binding.io = child;
    binding.user = this;
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
                      const Nba97GameImageRecordUploadEvent *,
                      Nba97GameImageRecordUploadMachine *) {
    ++static_cast<Fixture *>(opaque)->fallbackCalls;
    return 1;
  }
  static int child(void *opaque, const Nba97GameTextMemory *,
                   const Nba97GameRectangleUploadSubmitEvent *event,
                   Nba97GameRectangleUploadSubmitMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (event->kind == NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_80094440) {
      ++f.normalizeCalls;
      f.firstA0 = machine->registers.gpr[4].word;
      return 1;
    }
    if (event->kind == NBA97_GAME_RECTANGLE_UPLOAD_SUBMIT_CHILD_8009971C) {
      ++f.submitCalls;
      f.secondA0 = machine->registers.gpr[4].word;
      f.secondA1 = machine->registers.gpr[5].word;
      if (f.invalidateSubmit)
        machine->hi.known_mask = 16;
      return f.refuseSubmit ? 0 : 1;
    }
    return 0;
  }
  int run() {
    Nba97GameImageRecordUploadContext parent{};
    parent.memory = {&region, 1};
    parent.operation_budget = 128;
    parent.machine = entry;
    parent.io = fallback;
    parent.user = this;
    return nba97_game_image_record_upload_with_rectangle_upload_submit(
        &parent, &binding, &parentProgress);
  }
};

void actualImageRecordComposition() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.parentProgress.completed &&
        f.binding.progress.completed && f.binding.result == NBA97_TEXT_COMPLETE);
  check(f.binding.invocations == 1 && f.binding.completions == 1 &&
        f.binding.event.kind == NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800944F4 &&
        f.binding.event.pc == 0x8009464cu &&
        f.binding.event.delay_slot_pc == 0x80094650u &&
        f.binding.event.entry == 0x800944f4u &&
        f.binding.event.argument_count == 2);
  const auto descriptor = Fixture::Sp - 0x30u + 0x10u;
  check(f.normalizeCalls == 1 && f.submitCalls == 1 && f.fallbackCalls == 0 &&
        f.firstA0 == descriptor && f.secondA0 == descriptor &&
        f.secondA1 == Fixture::Record + 0x10u);
  check(f.get(descriptor, 2) == 0x340u &&
        f.get(descriptor + 2u, 2) == 0xf0u &&
        f.get(descriptor + 4u, 2) == 0x10u &&
        f.get(descriptor + 6u, 2) == 1u &&
        f.get(0x800d7b14u, 4) == 1u);
  check(f.parentProgress.machine.registers.gpr[29].word == Fixture::Sp &&
        f.parentProgress.machine.registers.gpr[31].word == 0x80012340u);
  f.put(0x800d7b14u, 0, 4);
  check(f.run() == NBA97_TEXT_COMPLETE && f.binding.invocations == 2 &&
        f.binding.completions == 2);
}

void naturalFailurePrefixes() {
  Fixture refused;
  refused.refuseSubmit = true;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        refused.binding.result == NBA97_TEXT_IO_REFUSED &&
        refused.binding.progress.stopped_pc == 0x80094514u &&
        refused.parentProgress.stopped_pc == 0x8009464cu &&
        refused.get(0x800d7b14u, 4) == 0);

  Fixture invalid;
  invalid.invalidateSubmit = true;
  check(invalid.run() == NBA97_TEXT_ARGUMENT &&
        invalid.binding.progress.machine.hi.known_mask == 16 &&
        invalid.parentProgress.machine.hi.known_mask == 16 &&
        invalid.get(0x800d7b14u, 4) == 0);

  Fixture limited;
  limited.binding.operation_budget = 0;
  check(limited.run() == NBA97_TEXT_LIMIT && limited.binding.invocations == 1 &&
        limited.parentProgress.stopped_pc == 0x8009464cu &&
        limited.get(0x800d7b14u, 4) == 0);
}

void adapterGuardsAndFallback() {
  Fixture f;
  Nba97GameTextMemory memory{&f.region, 1};
  Nba97GameImageRecordUploadEvent event{0x8009464cu, 0x80094650u,
                                        0x800944f4u, 1, 1,
                                        NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800944F4,
                                        2};
  Nba97GameImageRecordUploadMachine machine = f.entry;
  machine.registers.gpr[31] = {0x80094654u, 15};
  auto reject = [&](const Nba97GameImageRecordUploadEvent &candidate,
                    const Nba97GameImageRecordUploadMachine &candidateMachine) {
    auto copy = candidateMachine;
    const auto before = copy;
    check(nba97_game_rectangle_upload_submit_from_image_record_upload(
              &f.binding, &memory, &candidate, &copy) == 0 &&
          f.binding.result == NBA97_TEXT_ARGUMENT &&
          copy.registers.gpr[29].word == before.registers.gpr[29].word);
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
  malformed.kind = NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800A3BF8;
  reject(malformed, machine);
  malformed = event;
  malformed.argument_count = 1;
  reject(malformed, machine);
  malformed = event;
  malformed.invocation = 0;
  reject(malformed, machine);
  auto wrongRa = machine;
  wrongRa.registers.gpr[31].word ^= 4u;
  reject(event, wrongRa);

  Fixture fallback;
  fallback.put(Fixture::Record, 0x40, 1);
  fallback.put(Fixture::Record + 4, 1, 2);
  fallback.put(Fixture::Record + 6, 1, 2);
  check(fallback.run() == NBA97_TEXT_COMPLETE && fallback.fallbackCalls == 1 &&
        fallback.binding.invocations == 1);
}
} // namespace

int main() {
  actualImageRecordComposition();
  naturalFailurePrefixes();
  adapterGuardsAndFallback();
  std::cout << "game_rectangle_upload_submit_integration_tests: PASS\n";
}
