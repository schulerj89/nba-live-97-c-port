#include "game_text_chain_clear_adapter.h"

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
    throw std::runtime_error("text-chain integration failed at " +
                             std::to_string(line));
}
#define check(value) checkAt((value), __LINE__)

bool machineEq(const Nba97GameCountdownUiUpdateMachine &a,
               const Nba97GameCountdownUiUpdateMachine &b) {
  for (unsigned i = 0; i < 32; ++i)
    if (a.registers.gpr[i].word != b.registers.gpr[i].word ||
        a.registers.gpr[i].known_mask != b.registers.gpr[i].known_mask)
      return false;
  return a.hi.word == b.hi.word && a.hi.known_mask == b.hi.known_mask &&
         a.lo.word == b.lo.word && a.lo.known_mask == b.lo.known_mask;
}

struct Fixture {
  static constexpr U Font = 0x80110000u;
  static constexpr U Table = 0x80120000u;
  static constexpr U Links = 0x80130000u;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{0x80000000u, bytes.data(), known.data(),
                             bytes.size()};
  Nba97GameCountdownUiUpdateMachine machine{};
  Nba97GameCountdownUiUpdateProgress parentProgress{};
  Nba97GameTextChainClearBinding binding{};
  unsigned fallbackCalls = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      machine.registers.gpr[i] = {0x11220000u + i * 0x101u, 15};
    machine.registers.gpr[0] = {0, 15};
    machine.registers.gpr[29] = {0x801ff000u, 15};
    machine.registers.gpr[31] = {0x80032b20u, 15};
    machine.hi = {0x12345678u, 3};
    machine.lo = {0xabcdef01u, 12};
    put(0x800fdba4u, 601, 4);
    put(0x800fea2eu, 7, 2);
    put(0x800b2048u, Font, 4);
    put(Font + 0x10u, Links, 4);
    put(Font + 0x14u, Table, 4);
    put(Table + 0xc9u * 2u, 3, 2);
    put(Links + 3u * 64u + 0x12u, 0x7777u, 2);
    put(Links + 3u * 64u + 0x18u, 0xffffu, 2);
    for (unsigned i = 0; i < 22; ++i)
      put(0x800249e4u + i, 0x20u + i, 1);
    binding.operation_budget = 32;
  }

  std::size_t at(U address, unsigned width = 1) const {
    if (address < 0x80000000u || std::uint64_t(address) + width > 0x80200000u)
      throw std::out_of_range("unmapped");
    return address - 0x80000000u;
  }
  void put(U address, U value, unsigned width, std::uint8_t mask = 15) {
    const auto offset = at(address, width);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = std::uint8_t(value >> (8u * i));
      known[offset + i] = std::uint8_t((mask >> i) & 1u);
    }
  }
  U get(U address, unsigned width) const {
    const auto offset = at(address, width);
    U value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= U(bytes[offset + i]) << (8u * i);
    return value;
  }

  static int fallback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameCountdownUiUpdateEvent *,
                      Nba97GameCountdownUiUpdateMachine *) {
    ++static_cast<Fixture *>(opaque)->fallbackCalls;
    return 0;
  }

  int run() {
    Nba97GameCountdownUiUpdateContext parent{};
    parent.memory = {&region, 1};
    parent.operation_budget = 128;
    parent.machine = machine;
    parent.io = fallback;
    parent.user = this;
    return nba97_game_countdown_ui_update_with_text_chain_clear(
        &parent, &binding, &parentProgress);
  }
};

void naturalAndReuse() {
  Fixture f;
  check(f.run() == NBA97_TEXT_COMPLETE && f.parentProgress.completed &&
        f.binding.result == NBA97_TEXT_COMPLETE && f.binding.invocations == 1 &&
        f.binding.completions == 1 && f.fallbackCalls == 0);
  check(f.binding.event.kind == NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_8003066C &&
        f.binding.event.pc == 0x8003295cu &&
        f.binding.event.delay_slot_pc == 0x80032960u &&
        f.binding.event.entry == 0x8003066cu &&
        f.binding.event.argument_count == 1 &&
        f.binding.event.invocation == 1 &&
        f.binding.progress.machine.registers.gpr[6].word == 0xc9u);
  check(f.binding.progress.machine.registers.gpr[31].word == 0x80032964u &&
        f.parentProgress.machine.registers.gpr[31].word == 0x80032b20u &&
        f.get(Fixture::Links + 3u * 64u + 0x12u, 2) == 0 &&
        f.get(Fixture::Table + 0xc9u * 2u, 2) == 0xffffu &&
        f.get(0x800fea2eu, 2) == 0xffffu);

  f.put(0x800fea2eu, 7, 2);
  f.put(Fixture::Table + 0xc9u * 2u, 3, 2);
  f.put(Fixture::Links + 3u * 64u + 0x12u, 0x7777u, 2);
  check(f.run() == NBA97_TEXT_COMPLETE && f.binding.invocations == 2 &&
        f.binding.completions == 2);
}

void guardsTypedFallbackAndFailurePrefixes() {
  Fixture f;
  Nba97GameCountdownUiUpdateEvent event{
      0x8003295cu,
      0x80032960u,
      0x8003066cu,
      1,
      1,
      NBA97_GAME_COUNTDOWN_UI_UPDATE_CHILD_8003066C,
      1};
  Nba97GameTextMemory memory{&f.region, 1};
  auto childMachine = f.machine;
  childMachine.registers.gpr[4] = {0xc9u, 15};
  childMachine.registers.gpr[31] = {0x80032964u, 15};
  for (unsigned field = 0; field < 7; ++field) {
    auto invalid = event;
    auto invalidMachine = childMachine;
    if (field == 0)
      invalid.pc = 0;
    else if (field == 1)
      invalid.delay_slot_pc = 0;
    else if (field == 2)
      invalid.entry = 0;
    else if (field == 3)
      invalid.kind = 0;
    else if (field == 4)
      invalid.argument_count = 0;
    else if (field == 5)
      invalid.invocation = 2;
    else
      invalidMachine.registers.gpr[31].word = 0;
    check(nba97_game_text_chain_clear_from_countdown_ui_update(
              &f.binding, &memory, &invalid, &invalidMachine) == 0 &&
          f.binding.result == NBA97_TEXT_ARGUMENT);
  }
  auto invalidArgument = childMachine;
  invalidArgument.registers.gpr[4].word = 0xc8u;
  const auto invalidValue = invalidArgument;
  check(nba97_game_text_chain_clear_from_countdown_ui_update(
            &f.binding, &memory, &event, &invalidArgument) == 0 &&
        f.binding.result == NBA97_TEXT_ARGUMENT && f.binding.invocations == 0 &&
        machineEq(invalidArgument, invalidValue));
  invalidArgument = childMachine;
  invalidArgument.registers.gpr[4].known_mask = 14;
  const auto partialArgument = invalidArgument;
  check(nba97_game_text_chain_clear_from_countdown_ui_update(
            &f.binding, &memory, &event, &invalidArgument) == 0 &&
        f.binding.result == NBA97_TEXT_ARGUMENT && f.binding.invocations == 0 &&
        machineEq(invalidArgument, partialArgument));

  Fixture active;
  active.put(0x800fdba4u, 120, 4);
  active.put(0x800fdb58u, 120, 4);
  active.put(0x800fe8ccu, 0, 2);
  active.put(0x80021d92u, 1, 1);
  active.put(0x800fea2eu, 0xffffu, 2);
  active.put(0x800b2048u, Fixture::Font, 4);
  check(active.run() == NBA97_TEXT_IO_REFUSED && active.fallbackCalls == 1 &&
        active.parentProgress.stopped_pc == 0x800329e8u &&
        active.binding.invocations == 0);

  Fixture budget;
  budget.binding.operation_budget = 1;
  check(budget.run() == NBA97_TEXT_LIMIT &&
        budget.parentProgress.stopped_pc == 0x8003295cu &&
        budget.binding.progress.stopped_pc == 0x80030688u &&
        budget.get(0x800fea2eu, 2) == 7u &&
        machineEq(budget.parentProgress.machine,
                  budget.binding.progress.machine));

  Fixture missing;
  missing.put(0x800b2048u, 0x90000000u, 4);
  check(missing.run() == NBA97_TEXT_RESOURCE &&
        missing.binding.progress.stopped_pc == 0x80030688u &&
        missing.get(0x800fea2eu, 2) == 7u);

  Fixture unknown;
  unknown.known[unknown.at(Fixture::Links + 3u * 64u + 0x19u)] = 0;
  check(unknown.run() == NBA97_TEXT_UNKNOWN &&
        unknown.binding.progress.stopped_pc == 0x800306c0u &&
        unknown.get(Fixture::Links + 3u * 64u + 0x12u, 2) == 0 &&
        unknown.get(0x800fea2eu, 2) == 7u &&
        machineEq(unknown.parentProgress.machine,
                  unknown.binding.progress.machine));
}
} // namespace

int main() {
  try {
    naturalAndReuse();
    guardsTypedFallbackAndFailurePrefixes();
    std::printf("game_text_chain_clear_integration_tests: PASS (%u checks)\n",
                checks);
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
