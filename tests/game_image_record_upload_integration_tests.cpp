#include "game_image_record_upload_adapter.h"

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
  static constexpr std::uint32_t Size = 0x200000u;
  static constexpr std::uint32_t Sp = 0x801ff000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(Size, 0xa5);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(Size, 1);
  Nba97GameTextRegion region{Base, bytes.data(), known.data(), bytes.size()};
  Nba97GameCountdownUiUpdateMachine entry{};
  Nba97GameCountdownUiUpdateProgress parentProgress{};
  Nba97GameImageRecordUploadBinding binding{};
  std::array<Nba97GameImageRecordUploadAccess, 128> journal{};
  unsigned textCalls = 0;
  unsigned uploadCalls = 0;
  unsigned fallbackCalls = 0;
  std::array<std::uint16_t, 4> rectangle{};
  bool refuseUpload = false;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      entry.registers.gpr[i] = {0x11110000u + i * 0x101u, 15};
    entry.registers.gpr[0] = {0, 15};
    entry.registers.gpr[29] = {Sp, 15};
    entry.registers.gpr[31] = {0x81234568u, 15};
    entry.hi = {0x12345678u, 5};
    entry.lo = {0x89abcdefu, 10};
    put(0x800fdba4u, 120, 4);
    put(0x800fe8ccu, 0, 2);
    put(0x800fdb58u, 120, 4);
    put(0x80021d92u, 1, 1);
    put(0x800fea2eu, 0xffffu, 2);
    put(0x800b2048u, 0x80110000u, 4);
    for (unsigned i = 0; i < 22; ++i)
      put(0x800249e4u + i, 0x40u + i, 1);
    put(0x800249e8u, 0x1555u, 2);
    binding.operation_budget = 128;
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
                      const Nba97GameCountdownUiUpdateEvent *event,
                      Nba97GameCountdownUiUpdateMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.fallbackCalls;
    if (event->kind == NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_80030D18) {
      ++f.textCalls;
      machine->registers.gpr[2] = {0, 15};
      return 1;
    }
    return 0;
  }
  static int child(void *opaque, const Nba97GameTextMemory *,
                   const Nba97GameImageRecordUploadEvent *event,
                   Nba97GameImageRecordUploadMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    if (event->kind == NBA97_GAME_IMAGE_RECORD_UPLOAD_CHILD_800944F4) {
      ++f.uploadCalls;
      const auto address = machine->registers.gpr[4].word;
      for (unsigned i = 0; i < 4; ++i)
        f.rectangle[i] = std::uint16_t(f.get(address + i * 2u, 2));
      return f.refuseUpload ? 0 : 1;
    }
    return 0;
  }
  int run(std::size_t parentBudget = 512) {
    Nba97GameCountdownUiUpdateContext parent{};
    parent.memory = {&region, 1};
    parent.operation_budget = parentBudget;
    parent.machine = entry;
    parent.io = fallback;
    parent.user = this;
    return nba97_game_countdown_ui_update_with_image_record_upload(
        &parent, &binding, &parentProgress);
  }
};

void actualCountdownComposition() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.parentProgress.completed &&
        f.parentProgress.record_uploaded && f.binding.progress.completed);
  check(f.binding.invocations == 1 && f.binding.completions == 1 &&
        f.binding.result == NBA97_TEXT_COMPLETE);
  check(f.binding.event.kind == NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_80094540 &&
        f.binding.event.pc == 0x80032ae4u &&
        f.binding.event.delay_slot_pc == 0x80032ae8u &&
        f.binding.event.entry == 0x80094540u &&
        f.binding.event.invocation == 1 && f.binding.event.argument_count == 5);
  check(f.textCalls == 1 && f.fallbackCalls == 1 && f.uploadCalls == 1);
  check(f.rectangle[0] == 0x340 && f.rectangle[1] == 0xf0 &&
        f.rectangle[2] == 0x10 && f.rectangle[3] == 1);
  check(f.get(0x800fb5c0u, 1) == 0x2bu &&
        f.get(0x800fea2eu, 2) == 2u);
  check(f.parentProgress.machine.registers.gpr[29].word == Fixture::Sp &&
        f.parentProgress.machine.registers.gpr[31].word == 0x81234568u);

  f.put(0x800fea2eu, 0xffffu, 2);
  check(f.run() == NBA97_TEXT_COMPLETE && f.binding.invocations == 2 &&
        f.binding.completions == 2);
}

void nestedFailureAndParentPrefix() {
  Fixture f;
  f.refuseUpload = true;
  check(f.run() == NBA97_TEXT_IO_REFUSED &&
        f.binding.result == NBA97_TEXT_IO_REFUSED &&
        f.binding.progress.stopped_pc == 0x8009464cu &&
        f.parentProgress.stopped_pc == 0x80032ae4u);
  check(f.get(0x800fea2eu, 2) == 0xffffu);

  Fixture limited;
  check(limited.run(0) == NBA97_TEXT_LIMIT && limited.binding.invocations == 0 &&
        limited.uploadCalls == 0);
}

void exactAdapterGuards() {
  Fixture f;
  Nba97GameCountdownUiUpdateEvent event{0x80032ae4u, 0x80032ae8u,
                                         0x80094540u, 1, 1,
                                         NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_80094540,
                                         5};
  Nba97GameCountdownUiUpdateMachine machine{};
  for (auto &word : machine.registers.gpr)
    word = {0, 15};
  machine.registers.gpr[4] = {0x800fb5c0u, 15};
  machine.registers.gpr[5] = {0, 15};
  machine.registers.gpr[6] = {0, 15};
  machine.registers.gpr[7] = {0x340u, 15};
  machine.registers.gpr[29] = {Fixture::Sp - 0x40u, 15};
  machine.registers.gpr[31] = {0x80032aecu, 15};
  f.put(machine.registers.gpr[29].word + 0x10u, 0xf0u, 4);

  auto malformed = event;
  malformed.invocation = 2;
  const auto before = machine;
  Nba97GameTextMemory memory{&f.region, 1};
  check(nba97_game_image_record_upload_from_countdown_ui_update(
            &f.binding, &memory, &malformed, &machine) == 0 &&
        f.binding.result == NBA97_TEXT_ARGUMENT &&
        machine.registers.gpr[29].word == before.registers.gpr[29].word);
  malformed = event;
  malformed.delay_slot_pc ^= 4u;
  check(nba97_game_image_record_upload_from_countdown_ui_update(
            &f.binding, &memory, &malformed, &machine) == 0);
  malformed = event;
  malformed.argument_count = 4;
  check(nba97_game_image_record_upload_from_countdown_ui_update(
            &f.binding, &memory, &malformed, &machine) == 0);
  malformed = event;
  malformed.kind = NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_80030D18;
  check(nba97_game_image_record_upload_from_countdown_ui_update(
            &f.binding, &memory, &malformed, &machine) == 0);
  malformed = event;
  malformed.pc ^= 4u;
  check(nba97_game_image_record_upload_from_countdown_ui_update(
            &f.binding, &memory, &malformed, &machine) == 0);
  malformed = event;
  malformed.entry ^= 4u;
  check(nba97_game_image_record_upload_from_countdown_ui_update(
            &f.binding, &memory, &malformed, &machine) == 0);
  machine.registers.gpr[31].word ^= 4u;
  check(nba97_game_image_record_upload_from_countdown_ui_update(
            &f.binding, &memory, &event, &machine) == 0);
  machine.registers.gpr[31].word ^= 4u;
  machine.registers.gpr[7].word = 0x341u;
  check(nba97_game_image_record_upload_from_countdown_ui_update(
            &f.binding, &memory, &event, &machine) == 0);
  machine.registers.gpr[7].word = 0x340u;
  f.put(machine.registers.gpr[29].word + 0x10u, 0xf1u, 4);
  check(nba97_game_image_record_upload_from_countdown_ui_update(
            &f.binding, &memory, &event, &machine) == 0);
}
} // namespace

int main() {
  actualCountdownComposition();
  nestedFailureAndParentPrefix();
  exactAdapterGuards();
  std::cout << "game_image_record_upload_integration_tests: PASS\n";
}
