#include "game_period_audio_flag_clear_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
std::size_t checks;

void checkAt(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr,
                 "period audio flag clear integration check %zu failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define CHECK(value) checkAt((value), __LINE__)

struct Fixture {
  static constexpr std::uint32_t Ram = UINT32_C(0x80000000);
  static constexpr std::uint32_t Sp = UINT32_C(0x800ff800);
  static constexpr std::uint32_t Presentation = UINT32_C(0x800eb680);
  static constexpr std::uint32_t AudioFlag = UINT32_C(0x800b1fd5);
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x100000u, 0u);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x100000u, 1u);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  std::array<Nba97GameFirstPeriodStartupAccess, 8> parentJournal{};
  std::array<Nba97GamePeriodAudioFlagClearAccess, 2> ownerJournal{};
  Nba97GameFirstPeriodStartupContext parent{};
  Nba97GameFirstPeriodStartupProgress parentProgress{};
  Nba97GamePeriodAudioFlagClearBinding binding{};
  std::vector<Nba97GameFirstPeriodStartupEvent> fallbackEvents;

  explicit Fixture(std::uint8_t presentation = 0u) {
    parent.memory = {&region, 1u};
    parent.operation_budget = 100u;
    parent.io = fallback;
    parent.user = this;
    parent.access_journal = parentJournal.data();
    parent.access_journal_capacity = parentJournal.size();
    for (unsigned index = 0u; index != 32u; ++index)
      parent.registers.gpr[index] = {
          UINT32_C(0x61000000) + index,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    parent.registers.gpr[0] = {0u, 15u};
    parent.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Sp, 15u};
    parent.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {UINT32_C(0x81234568),
                                                       15u};
    put(Presentation, presentation);
    put(AudioFlag, 0xd7u);
    nba97_game_period_audio_flag_clear_binding_init(
        &binding, 1u, ownerJournal.data(), ownerJournal.size(), nullptr,
        nullptr);
  }

  void put(std::uint32_t address, std::uint8_t value) {
    bytes[address - Ram] = value;
    known[address - Ram] = 1u;
  }

  static int fallback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameFirstPeriodStartupEvent *event,
                      Nba97GameFirstPeriodStartupRegisters *registers) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.fallbackEvents.push_back(*event);
    if (event->entry == UINT32_C(0x800295d0))
      registers->gpr[NBA97_MATCH_INITIALIZE_V0] = {UINT32_C(0x55667788), 5u};
    return 1;
  }

  int run() {
    return nba97_game_first_period_startup_with_audio_flag_clear(
        &parent, &binding, &parentProgress);
  }
};

Nba97GameFirstPeriodStartupEvent exactEvent() {
  Nba97GameFirstPeriodStartupEvent event{};
  event.pc = UINT32_C(0x80067400);
  event.delay_slot_pc = UINT32_C(0x80067404);
  event.entry = UINT32_C(0x8002a244);
  event.operation = 3u;
  event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_2A244;
  event.argument_count = 0u;
  return event;
}

Nba97GameFirstPeriodStartupRegisters exactRegisters() {
  Nba97GameFirstPeriodStartupRegisters registers{};
  for (unsigned index = 0u; index != 32u; ++index)
    registers.gpr[index] = {UINT32_C(0x71000000) + index,
                            static_cast<std::uint8_t>((index % 15u) + 1u)};
  registers.gpr[0] = {0u, 15u};
  registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {UINT32_C(0x80067408), 15u};
  return registers;
}

void BothPresentationPathsAndV0Forwarding() {
  for (std::uint8_t presentation : {std::uint8_t{0}, std::uint8_t{0xff}}) {
    Fixture fixture(presentation);
    CHECK(fixture.run() == NBA97_TEXT_COMPLETE &&
          fixture.parentProgress.completed);
    CHECK(fixture.bytes[Fixture::AudioFlag - Fixture::Ram] == 0u &&
          fixture.known[Fixture::AudioFlag - Fixture::Ram] == 1u);
    CHECK(fixture.binding.invocations == 1u &&
          fixture.binding.completions == 1u &&
          fixture.binding.result == NBA97_TEXT_COMPLETE);
    CHECK(fixture.binding.event.pc == UINT32_C(0x80067400) &&
          fixture.binding.event.delay_slot_pc == UINT32_C(0x80067404) &&
          fixture.binding.event.entry == UINT32_C(0x8002a244) &&
          fixture.binding.event.operation == 3u &&
          fixture.binding.event.argument_count == 0u);
    CHECK(fixture.binding.progress.machine.registers
                  .gpr[NBA97_MATCH_INITIALIZE_V0]
                  .word == UINT32_C(0x55667788) &&
          fixture.binding.progress.machine.registers
                  .gpr[NBA97_MATCH_INITIALIZE_V0]
                  .known_mask == 5u);
    CHECK(fixture.parentProgress.optional_presentation_executed ==
          (presentation != 0u));
    CHECK(fixture.fallbackEvents.size() == (presentation == 0u ? 4u : 6u));
  }
}

void BudgetFailureAndWrapperReuse() {
  Fixture limited;
  limited.binding.operation_budget = 0u;
  CHECK(limited.run() == NBA97_TEXT_LIMIT &&
        limited.binding.invocations == 1u &&
        limited.binding.completions == 0u &&
        limited.binding.progress.stopped_pc == UINT32_C(0x8002a248) &&
        limited.bytes[Fixture::AudioFlag - Fixture::Ram] == 0xd7u &&
        limited.parentProgress.stopped_pc == UINT32_C(0x80067400));

  Fixture reused(1u);
  CHECK(reused.run() == NBA97_TEXT_COMPLETE);
  reused.fallbackEvents.clear();
  reused.put(Fixture::AudioFlag, 0xe3u);
  CHECK(reused.run() == NBA97_TEXT_COMPLETE &&
        reused.binding.invocations == 1u && reused.binding.completions == 1u &&
        reused.bytes[Fixture::AudioFlag - Fixture::Ram] == 0u &&
        reused.fallbackEvents.size() == 6u);
}

void ExactGuardsAndFallback() {
  for (unsigned field = 0u; field != 9u; ++field) {
    Fixture fixture;
    auto event = exactEvent();
    auto registers = exactRegisters();
    if (field == 0u)
      event.pc ^= 4u;
    else if (field == 1u)
      event.delay_slot_pc ^= 4u;
    else if (field == 2u)
      event.entry ^= 4u;
    else if (field == 3u)
      event.operation = 4u;
    else if (field == 4u)
      event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_2A254;
    else if (field == 5u)
      event.argument_count = 1u;
    else if (field == 6u)
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].known_mask = 7u;
    else if (field == 7u)
      registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ^= 4u;
    else
      registers.gpr[9].known_mask = 16u;
    CHECK(nba97_game_period_audio_flag_clear_from_first_period(
              &fixture.binding, &fixture.parent.memory, &event, &registers) ==
              0 &&
          fixture.binding.result == NBA97_TEXT_ARGUMENT &&
          fixture.binding.invocations == 0u && fixture.fallbackEvents.empty());
  }

  Fixture missingJournal;
  missingJournal.binding.access_journal = nullptr;
  auto event = exactEvent();
  auto registers = exactRegisters();
  CHECK(nba97_game_period_audio_flag_clear_from_first_period(
            &missingJournal.binding, &missingJournal.parent.memory, &event,
            &registers) == 0 &&
        missingJournal.binding.result == NBA97_TEXT_ARGUMENT);

  Fixture fallback;
  Nba97GameFirstPeriodStartupEvent other{};
  other.pc = UINT32_C(0x800673f8);
  other.delay_slot_pc = UINT32_C(0x800673fc);
  other.entry = UINT32_C(0x800295d0);
  other.operation = 2u;
  other.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_295D0;
  auto otherRegisters = exactRegisters();
  otherRegisters.gpr[NBA97_MATCH_INITIALIZE_RA] = {UINT32_C(0x80067400), 15u};
  fallback.binding.fallback = Fixture::fallback;
  fallback.binding.fallback_user = &fallback;
  CHECK(nba97_game_period_audio_flag_clear_from_first_period(
            &fallback.binding, &fallback.parent.memory, &other,
            &otherRegisters) == 1 &&
        fallback.fallbackEvents.size() == 1u &&
        fallback.binding.invocations == 0u);

  CHECK(nba97_game_period_audio_flag_clear_from_first_period(
            nullptr, &fallback.parent.memory, &event, &registers) == 0);
  CHECK(nba97_game_period_audio_flag_clear_from_first_period(
            &fallback.binding, nullptr, &event, &registers) == 0);
  CHECK(nba97_game_period_audio_flag_clear_from_first_period(
            &fallback.binding, &fallback.parent.memory, nullptr, &registers) ==
        0);
  CHECK(nba97_game_period_audio_flag_clear_from_first_period(
            &fallback.binding, &fallback.parent.memory, &event, nullptr) == 0);
}

void NaturalMappingFailure() {
  Fixture fixture;
  const std::size_t gap = Fixture::AudioFlag - Fixture::Ram;
  std::array<Nba97GameTextRegion, 2> regions{{
      {Fixture::Ram, fixture.bytes.data(), fixture.known.data(), gap},
      {Fixture::AudioFlag + 1u, fixture.bytes.data() + gap + 1u,
       fixture.known.data() + gap + 1u, fixture.bytes.size() - gap - 1u},
  }};
  fixture.parent.memory = {regions.data(), regions.size()};
  CHECK(fixture.run() == NBA97_TEXT_RESOURCE &&
        fixture.binding.invocations == 1u &&
        fixture.binding.result == NBA97_TEXT_RESOURCE &&
        fixture.binding.progress.stopped_address == Fixture::AudioFlag &&
        fixture.parentProgress.stopped_pc == UINT32_C(0x80067400));

  CHECK(nba97_game_first_period_startup_with_audio_flag_clear(
            nullptr, &fixture.binding, &fixture.parentProgress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_first_period_startup_with_audio_flag_clear(
            &fixture.parent, nullptr, &fixture.parentProgress) ==
        NBA97_TEXT_ARGUMENT);
  CHECK(nba97_game_first_period_startup_with_audio_flag_clear(
            &fixture.parent, &fixture.binding, nullptr) == NBA97_TEXT_ARGUMENT);
}
} // namespace

int main() {
  BothPresentationPathsAndV0Forwarding();
  BudgetFailureAndWrapperReuse();
  ExactGuardsAndFallback();
  NaturalMappingFailure();
  std::printf("game period audio flag clear integration: %zu checks\n", checks);
}
