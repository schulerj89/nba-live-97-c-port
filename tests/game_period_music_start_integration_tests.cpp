#include "game_period_music_start_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr,
                 "period music start integration check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t EntrySp = 0x800ff800u;

bool same_registers(const Nba97GameFirstPeriodStartupRegisters &a,
                    const Nba97GameFirstPeriodStartupRegisters &b) {
  for (unsigned index = 0u; index != 32u; ++index)
    if (a.gpr[index].word != b.gpr[index].word ||
        a.gpr[index].known_mask != b.gpr[index].known_mask)
      return false;
  return true;
}

struct Composition {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x100000u, 0u);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x100000u, 1u);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97GameFirstPeriodStartupContext parent{};
  Nba97GameFirstPeriodStartupProgress parent_progress{};
  Nba97GamePeriodMusicStartFirstPeriodBinding music{};
  std::vector<Nba97GameFirstPeriodStartupEvent> parent_calls;
  std::vector<Nba97GamePeriodMusicStartEvent> music_calls;
  Nba97GameFirstPeriodStartupEvent natural_event{};
  Nba97GameFirstPeriodStartupRegisters natural_entry{};
  unsigned refuse_music{};
  unsigned invalid_hi_music{};
  unsigned invalid_zero_music{};

  explicit Composition(std::uint8_t volume = 15u, std::uint8_t loaded = 1u) {
    for (unsigned index = 0u; index != 32u; ++index)
      parent.registers.gpr[index] = {0x33000000u + index * 0x010101u, 15u};
    parent.registers.gpr[0] = {0u, 15u};
    parent.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {EntrySp, 15u};
    parent.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80068c54u, 15u};
    parent.memory = {&region, 1u};
    parent.operation_budget = 100u;
    parent.io = route;
    parent.user = this;
    nba97_game_period_music_start_first_period_binding_init(
        &music, 100u, music_io, this, nullptr, 0u, fallback, this);

    put(0x80021d7fu, volume, 1u);
    put(0x80021d6cu, 0x80031000u, 4u);
    put(0x800b1f34u, 0x80032000u, 4u);
    put(0x800b1f38u, loaded, 1u);
    put(0x800b1f39u, 0u, 1u);
    put(0x800eb680u, 0u, 1u);
    put(0x800fdb4eu, 0xbeefu, 2u);
    put(0x800fdb94u, 0x1234u, 2u);
  }

  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }
  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    auto at = offset(address);
    for (unsigned byte = 0u; byte != width; ++byte) {
      bytes[at + byte] = static_cast<std::uint8_t>(value >> (8u * byte));
      known[at + byte] = 1u;
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    auto at = offset(address);
    std::uint32_t value = 0u;
    for (unsigned byte = 0u; byte != width; ++byte)
      value |= std::uint32_t(bytes[at + byte]) << (8u * byte);
    return value;
  }
  static int music_io(void *user, const Nba97GameTextMemory *,
                      const Nba97GamePeriodMusicStartEvent *event,
                      Nba97GamePeriodMusicStartMachine *machine) {
    auto &fixture = *static_cast<Composition *>(user);
    fixture.music_calls.push_back(*event);
    const unsigned ordinal = static_cast<unsigned>(fixture.music_calls.size());
    machine->registers.gpr[12] = {0x45670000u + ordinal,
                                  static_cast<std::uint8_t>(ordinal)};
    if (ordinal == fixture.invalid_hi_music)
      machine->hi.known_mask = 16u;
    if (ordinal == fixture.invalid_zero_music)
      machine->registers.gpr[0] = {1u, 15u};
    return ordinal == fixture.refuse_music ? 0 : 1;
  }
  static int fallback(void *user, const Nba97GameTextMemory *,
                      const Nba97GameFirstPeriodStartupEvent *,
                      Nba97GameFirstPeriodStartupRegisters *registers) {
    auto &fixture = *static_cast<Composition *>(user);
    registers->gpr[13] = {0xabc00000u + static_cast<std::uint32_t>(
                                            fixture.music.fallback_invocations),
                          7u};
    return 1;
  }
  static int route(void *user, const Nba97GameTextMemory *memory,
                   const Nba97GameFirstPeriodStartupEvent *event,
                   Nba97GameFirstPeriodStartupRegisters *registers) {
    auto &fixture = *static_cast<Composition *>(user);
    fixture.parent_calls.push_back(*event);
    if (event->pc == 0x800673f8u) {
      fixture.natural_event = *event;
      fixture.natural_entry = *registers;
    }
    return nba97_game_period_music_start_from_first_period(
        &fixture.music, memory, event, registers);
  }
  int run() {
    return nba97_game_first_period_startup(&parent, &parent_progress);
  }
};

void natural_normal_disabled_and_reuse() {
  Composition normal(15u, 1u);
  check(normal.run() == NBA97_TEXT_COMPLETE &&
        normal.parent_progress.completed);
  check(normal.natural_event.kind == NBA97_GAME_FIRST_PERIOD_STARTUP_295D0 &&
        normal.natural_event.pc == 0x800673f8u &&
        normal.natural_event.delay_slot_pc == 0x800673fcu &&
        normal.natural_event.entry == 0x800295d0u &&
        normal.natural_event.argument_count == 0u);
  check(normal.natural_entry.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80067400u &&
        normal.natural_entry.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp - 0x18u);
  check(normal.music.invocations == 1u && normal.music.completions == 1u &&
        normal.music.result == NBA97_TEXT_COMPLETE &&
        normal.music.progress.completed && normal.music_calls.size() == 4u &&
        normal.music.progress.machine.hi.known_mask == 0u &&
        normal.music.progress.machine.lo.known_mask == 0u);
  check(normal.music.progress.frame_stack_pointer == EntrySp - 0x30u &&
        normal.music.progress.restored_return_address.word == 0x80067400u &&
        normal.get(0x800b1f39u, 1u) == 1u);
  check(normal.parent_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp &&
        normal.parent_progress.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word ==
            0x80068c54u);
  check(normal.music.fallback_invocations == 4u &&
        normal.parent_calls.size() == 5u);

  Composition disabled(0u, 1u);
  check(disabled.run() == NBA97_TEXT_COMPLETE &&
        disabled.music.progress.completed && disabled.music_calls.empty() &&
        disabled.get(0x800b1f39u, 1u) == 0u);

  check(normal.run() == NBA97_TEXT_COMPLETE && normal.music.invocations == 2u &&
        normal.music.completions == 2u && normal.music_calls.size() == 8u &&
        normal.music.fallback_invocations == 8u);
}

void natural_failures_and_machine_transport() {
  Composition refused(1u, 0u);
  refused.refuse_music = 1u;
  check(refused.run() == NBA97_TEXT_IO_REFUSED &&
        !refused.parent_progress.completed &&
        refused.parent_progress.stopped_pc == 0x800673f8u &&
        refused.parent_progress.stopped_entry == 0x800295d0u &&
        refused.music.result == NBA97_TEXT_IO_REFUSED &&
        refused.music.progress.stopped_pc == 0x80029618u &&
        refused.get(0x800b1f38u, 1u) == 0u);

  Composition limited(1u, 1u);
  limited.music.operation_budget = 0u;
  check(limited.run() == NBA97_TEXT_IO_REFUSED &&
        limited.music.result == NBA97_TEXT_LIMIT &&
        limited.music.progress.stopped_pc == 0x800295d4u &&
        limited.parent_progress.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            EntrySp - 0x30u);

  Composition bad_hi(1u, 0u);
  bad_hi.invalid_hi_music = 1u;
  check(bad_hi.run() == NBA97_TEXT_IO_REFUSED &&
        bad_hi.music.result == NBA97_TEXT_ARGUMENT &&
        bad_hi.parent_progress.registers.gpr[12].word == 0x45670001u &&
        bad_hi.parent_progress.registers.gpr[12].known_mask == 1u);

  Composition bad_zero(1u, 0u);
  bad_zero.invalid_zero_music = 1u;
  auto entry = bad_zero.parent.registers;
  check(bad_zero.run() == NBA97_TEXT_IO_REFUSED &&
        bad_zero.music.result == NBA97_TEXT_ARGUMENT &&
        bad_zero.parent_progress.registers.gpr[0].word == 0u &&
        bad_zero.parent_progress.registers.gpr[12].word != 0x45670001u &&
        !same_registers(bad_zero.music.progress.machine.registers, entry));
}

void adapter_claim_and_guards() {
  Composition fixture;
  Nba97GameFirstPeriodStartupEvent event{};
  event.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_295D0;
  event.pc = 0x800673f8u;
  event.delay_slot_pc = 0x800673fcu;
  event.entry = 0x800295d0u;
  event.argument_count = 0u;
  auto registers = fixture.parent.registers;
  registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80067400u, 15u};

  const auto original = registers;
  const std::uint32_t mutations[5] = {0x800673fcu, 0x800673f8u, 0x800673fcu,
                                      0x800295d4u, 1u};
  for (unsigned which = 0u; which != 5u; ++which) {
    auto malformed = event;
    if (which == 0u)
      malformed.pc = mutations[which];
    else if (which == 1u)
      malformed.delay_slot_pc = mutations[which];
    else if (which == 2u)
      malformed.entry = mutations[which];
    else if (which == 3u)
      malformed.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_2A244;
    else
      malformed.argument_count = static_cast<std::uint8_t>(mutations[which]);
    registers = original;
    check(!nba97_game_period_music_start_from_first_period(
        &fixture.music, &fixture.parent.memory, &malformed, &registers));
    check(fixture.music.result == NBA97_TEXT_ARGUMENT &&
          fixture.music.invocations == 0u &&
          same_registers(registers, original));
  }

  registers = original;
  registers.gpr[NBA97_MATCH_INITIALIZE_RA].word = 0x80067404u;
  check(!nba97_game_period_music_start_from_first_period(
            &fixture.music, &fixture.parent.memory, &event, &registers) &&
        fixture.music.invocations == 0u);
  registers = original;
  registers.gpr[0] = {1u, 15u};
  check(!nba97_game_period_music_start_from_first_period(
            &fixture.music, &fixture.parent.memory, &event, &registers) &&
        fixture.music.invocations == 0u);

  auto delay_only = event;
  delay_only.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_2A244;
  delay_only.pc = 0x80067400u;
  delay_only.entry = 0x8002a244u;
  registers = original;
  registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80067408u, 15u};
  check(!nba97_game_period_music_start_from_first_period(
            &fixture.music, &fixture.parent.memory, &delay_only, &registers) &&
        fixture.music.fallback_invocations == 0u &&
        same_registers(registers, [&] {
          auto expected = original;
          expected.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80067408u, 15u};
          return expected;
        }()));

  auto ra_only = event;
  ra_only.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_2A244;
  ra_only.pc = 0x80067400u;
  ra_only.delay_slot_pc = 0x80067404u;
  ra_only.entry = 0x8002a244u;
  registers = original;
  check(!nba97_game_period_music_start_from_first_period(
            &fixture.music, &fixture.parent.memory, &ra_only, &registers) &&
        fixture.music.fallback_invocations == 0u &&
        same_registers(registers, original));

  auto unrelated = event;
  unrelated.kind = NBA97_GAME_FIRST_PERIOD_STARTUP_2A244;
  unrelated.pc = 0x80067400u;
  unrelated.delay_slot_pc = 0x80067404u;
  unrelated.entry = 0x8002a244u;
  registers = original;
  registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80067408u, 15u};
  check(nba97_game_period_music_start_from_first_period(
            &fixture.music, &fixture.parent.memory, &unrelated, &registers) &&
        fixture.music.fallback_invocations == 1u &&
        registers.gpr[13].word == 0xabc00001u);
}
} // namespace

int main() {
  natural_normal_disabled_and_reuse();
  natural_failures_and_machine_transport();
  adapter_claim_and_guards();
  std::printf("game period music start integration: %u checks passed\n",
              checks);
}
