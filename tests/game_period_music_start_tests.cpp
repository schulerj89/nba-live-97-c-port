#include "recovered/game_period_music_start.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "period music start check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Sp = 0x800ff000u;

bool same_word(const Nba97GamePeriodMusicStartWord &a,
               const Nba97GamePeriodMusicStartWord &b) {
  return a.word == b.word && a.known_mask == b.known_mask;
}

bool same_machine(const Nba97GamePeriodMusicStartMachine &a,
                  const Nba97GamePeriodMusicStartMachine &b) {
  for (unsigned index = 0u; index != 32u; ++index)
    if (!same_word(a.registers.gpr[index], b.registers.gpr[index]))
      return false;
  return same_word(a.hi, b.hi) && same_word(a.lo, b.lo);
}

struct Fixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x100000u, 0u);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x100000u, 1u);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97GamePeriodMusicStartContext context{};
  Nba97GamePeriodMusicStartProgress progress{};
  std::array<Nba97GamePeriodMusicStartAccess, 32> journal{};
  std::vector<Nba97GamePeriodMusicStartEvent> calls;
  std::vector<Nba97GamePeriodMusicStartMachine> entries;
  unsigned refuse_call{};
  unsigned corrupt_gpr_call{};
  unsigned corrupt_hi_call{};
  unsigned relocate_call{};
  std::uint32_t relocated_sp{0x800fe000u};
  std::uint32_t relocated_s0{0x80022000u};

  Fixture(std::uint8_t volume = 14u, std::uint8_t loaded = 1u) {
    for (unsigned index = 0u; index != 32u; ++index)
      context.machine.registers.gpr[index] = {
          0x12000000u + index * 0x010203u,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Sp, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80067400u,
                                                                15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {0x55667788u,
                                                                5u};
    context.machine.hi = {0x89abcdefu, 9u};
    context.machine.lo = {0x76543210u, 6u};
    context.memory = {&region, 1u};
    context.operation_budget = 100u;
    context.io = io;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    put(0x80021d7fu, volume, 1u);
    put(0x800b1f38u, loaded, 1u);
    put(0x800b1f39u, 0x5au, 1u);
    put(0x80021d6cu, 0x80035000u, 4u);
    put(0x800b1f34u, 0x80036000u, 4u);
    put(relocated_s0, 7u, 1u);
    put(relocated_sp + 0x10u, 0x13579bdfu, 4u);
    put(relocated_sp + 0x14u, 0x80067400u, 4u);
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
    std::uint32_t value = 0u;
    auto at = offset(address);
    for (unsigned byte = 0u; byte != width; ++byte)
      value |= std::uint32_t(bytes[at + byte]) << (8u * byte);
    return value;
  }
  static int io(void *user, const Nba97GameTextMemory *,
                const Nba97GamePeriodMusicStartEvent *event,
                Nba97GamePeriodMusicStartMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(user);
    fixture.calls.push_back(*event);
    fixture.entries.push_back(*machine);
    unsigned ordinal = static_cast<unsigned>(fixture.calls.size());
    if (ordinal == fixture.relocate_call) {
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {fixture.relocated_sp,
                                                           15u};
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {fixture.relocated_s0,
                                                           15u};
      machine->registers.gpr[12] = {0xdeadbeefu, 3u};
      machine->hi = {0x10203040u, 5u};
      machine->lo = {0x50607080u, 10u};
    }
    if (ordinal == fixture.corrupt_gpr_call)
      machine->registers.gpr[0] = {1u, 15u};
    if (ordinal == fixture.corrupt_hi_call)
      machine->hi.known_mask = 16u;
    return ordinal == fixture.refuse_call ? 0 : 1;
  }
  int run() { return nba97_game_period_music_start(&context, &progress); }
};

void every_volume_and_normal_calls() {
  for (unsigned volume = 0u; volume != 256u; ++volume) {
    Fixture fixture(static_cast<std::uint8_t>(volume), 1u);
    auto incoming = fixture.context.machine;
    check(fixture.run() == NBA97_TEXT_COMPLETE && fixture.progress.completed);
    check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
              .word == Sp);
    check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
              .word == 0x80067400u);
    check(fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0]
              .word == 0x55667788u);
    check(same_word(fixture.progress.machine.hi, incoming.hi) &&
          same_word(fixture.progress.machine.lo, incoming.lo));
    if (volume == 0u) {
      check(fixture.calls.empty() && !fixture.progress.playback_executed &&
            fixture.progress.operations == 5u &&
            fixture.get(0x800b1f39u, 1u) == 0x5au);
    } else {
      check(fixture.calls.size() == 4u &&
            fixture.progress.scaled_volume.word ==
                (volume < 15u ? volume * 9u : 127u) &&
            fixture.progress.scaled_volume.known_mask == 15u &&
            fixture.get(0x800b1f39u, 1u) == 1u);
    }
    const std::array<unsigned, 24> untouched = {
        0u,  6u,  7u,  8u,  9u,  10u, 11u, 12u, 13u, 14u, 15u, 17u,
        18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 30u};
    for (unsigned index : untouched)
      if (index != NBA97_MATCH_INITIALIZE_RA &&
          index != NBA97_MATCH_INITIALIZE_SP &&
          index != NBA97_MATCH_INITIALIZE_S0)
        check(same_word(fixture.progress.machine.registers.gpr[index],
                        incoming.registers.gpr[index]));
  }

  Fixture arguments(14u, 1u);
  check(arguments.run() == NBA97_TEXT_COMPLETE);
  const std::array<std::uint32_t, 4> pcs = {0x8002964cu, 0x80029654u,
                                            0x8002965cu, 0x80029664u};
  const std::array<std::uint32_t, 4> entries = {0x800aafa0u, 0x800ab224u,
                                                0x800ab388u, 0x800ab2c8u};
  const std::array<std::uint32_t, 4> a0 = {0u, 0u, 126u, 120u};
  for (unsigned index = 0u; index != 4u; ++index) {
    check(arguments.calls[index].pc == pcs[index] &&
          arguments.calls[index].delay_slot_pc == pcs[index] + 4u &&
          arguments.calls[index].entry == entries[index] &&
          arguments.calls[index].argument_count == 1u &&
          arguments.calls[index].invocation == 1u);
    check(arguments.entries[index]
                  .registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                  .word == pcs[index] + 8u &&
          arguments.entries[index]
                  .registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                  .word == a0[index]);
  }
}

void load_path_and_callback_live_state() {
  Fixture fixture(15u, 0u);
  fixture.relocate_call = 1u;
  check(fixture.run() == NBA97_TEXT_COMPLETE && fixture.progress.completed);
  check(fixture.calls.size() == 5u && fixture.progress.load_music_executed &&
        fixture.progress.callbacks_completed == 5u);
  check(fixture.calls[0].pc == 0x80029618u &&
        fixture.calls[0].delay_slot_pc == 0x8002961cu &&
        fixture.calls[0].entry == 0x800aae7cu &&
        fixture.calls[0].argument_count == 2u);
  check(fixture.entries[0].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            0x80035000u &&
        fixture.entries[0].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            0x80036000u);
  check(fixture.progress.scaled_volume.word == 63u &&
        fixture.entries[3].registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            63u);
  check(fixture.get(0x800b1f38u, 1u) == 1u &&
        fixture.get(0x800b1f39u, 1u) == 1u);
  check(
      fixture.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
          fixture.relocated_sp + 0x18u &&
      fixture.progress.restored_s0.word == 0x13579bdfu &&
      fixture.progress.restored_return_address.word == 0x80067400u);
  check(fixture.progress.machine.registers.gpr[12].word == 0xdeadbeefu &&
        fixture.progress.machine.registers.gpr[12].known_mask == 3u &&
        fixture.progress.machine.hi.word == 0x10203040u &&
        fixture.progress.machine.hi.known_mask == 5u &&
        fixture.progress.machine.lo.word == 0x50607080u &&
        fixture.progress.machine.lo.known_mask == 10u);

  const std::array<std::uint32_t, 11> addresses = {Sp - 8u,
                                                   Sp - 4u,
                                                   0x80021d7fu,
                                                   0x800b1f38u,
                                                   0x80021d6cu,
                                                   0x800b1f34u,
                                                   0x800b1f38u,
                                                   fixture.relocated_s0,
                                                   0x800b1f39u,
                                                   fixture.relocated_sp + 0x14u,
                                                   fixture.relocated_sp +
                                                       0x10u};
  const std::array<std::uint32_t, 11> pcs = {
      0x800295d4u, 0x800295e0u, 0x800295e4u, 0x800295f8u,
      0x8002960cu, 0x80029614u, 0x80029628u, 0x8002962cu,
      0x80029674u, 0x80029678u, 0x8002967cu};
  const std::array<size_t, 11> operations = {1u, 2u, 3u,  4u,  5u, 6u,
                                             8u, 9u, 14u, 15u, 16u};
  check(fixture.progress.access_events == addresses.size());
  for (size_t index = 0u; index != addresses.size(); ++index)
    check(fixture.journal[index].address == addresses[index] &&
          fixture.journal[index].pc == pcs[index] &&
          fixture.journal[index].operation == operations[index]);
}

void unknowns_atomicity_and_store_refusal() {
  Fixture enable;
  enable.known[enable.offset(0x80021d7fu)] = 0u;
  check(enable.run() == NBA97_TEXT_UNKNOWN &&
        enable.progress.stopped_pc == 0x800295ecu &&
        enable.progress.operations == 3u &&
        enable.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 14u);

  Fixture flag(1u, 0u);
  flag.known[flag.offset(0x800b1f38u)] = 0u;
  check(flag.run() == NBA97_TEXT_UNKNOWN &&
        flag.progress.stopped_pc == 0x80029600u &&
        flag.progress.operations == 4u && flag.calls.empty());

  /* Directly exercise the unknown correlated scale through the load child,
   * which may mutate knownness before the callback-live reread. */
  Fixture scaled(1u, 0u);
  scaled.context.io = [](void *user, const Nba97GameTextMemory *,
                         const Nba97GamePeriodMusicStartEvent *event,
                         Nba97GamePeriodMusicStartMachine *) -> int {
    auto &f = *static_cast<Fixture *>(user);
    f.calls.push_back(*event);
    if (event->pc == 0x80029618u)
      f.known[f.offset(0x80021d7fu)] = 0u;
    return 1;
  };
  check(scaled.run() == NBA97_TEXT_UNKNOWN &&
        scaled.progress.stopped_pc == 0x80029640u &&
        scaled.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0]
                .known_mask == 14u &&
        scaled.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V1]
                .known_mask == 12u &&
        scaled.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0]
                .known_mask == 12u);

  Fixture malformed(1u, 0u);
  auto before_a0 =
      malformed.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0];
  malformed.known[malformed.offset(0x80021d6cu) + 3u] = 2u;
  check(malformed.run() == NBA97_TEXT_ARGUMENT &&
        malformed.progress.stopped_pc == 0x8002960cu &&
        malformed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                .word == 0x80020000u &&
        malformed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0]
                .known_mask == 15u &&
        !same_word(
            malformed.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0],
            before_a0));

  std::array<std::uint8_t, 64> stack_bytes{};
  Nba97GameTextRegion stack{0x800feff0u, stack_bytes.data(), nullptr,
                            stack_bytes.size()};
  Nba97GamePeriodMusicStartContext context{};
  Nba97GamePeriodMusicStartProgress progress{};
  for (unsigned index = 0u; index != 32u; ++index)
    context.machine.registers.gpr[index] = {index, 15u};
  context.machine.registers.gpr[0] = {0u, 15u};
  context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {Sp, 15u};
  context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {0x11223344u, 7u};
  context.memory = {&stack, 1u};
  context.operation_budget = 10u;
  auto before_bytes = stack_bytes;
  check(nba97_game_period_music_start(&context, &progress) ==
            NBA97_TEXT_ARGUMENT &&
        progress.stopped_pc == 0x800295d4u && stack_bytes == before_bytes);
}

void budgets_failures_and_return_guards() {
  Fixture complete(1u, 0u);
  check(complete.run() == NBA97_TEXT_COMPLETE);
  const size_t operations = complete.progress.operations;
  check(operations == 16u);
  Fixture repeated_a(1u, 0u);
  Fixture repeated_b(1u, 0u);
  check(repeated_a.run() == NBA97_TEXT_COMPLETE &&
        repeated_b.run() == NBA97_TEXT_COMPLETE &&
        repeated_a.bytes == repeated_b.bytes &&
        repeated_a.known == repeated_b.known &&
        same_machine(repeated_a.progress.machine, repeated_b.progress.machine));
  for (size_t budget = 0u; budget != operations; ++budget) {
    Fixture a(1u, 0u);
    Fixture b(1u, 0u);
    a.context.operation_budget = budget;
    b.context.operation_budget = budget;
    check(a.run() == NBA97_TEXT_LIMIT && b.run() == NBA97_TEXT_LIMIT);
    check(a.progress.operations == budget && b.progress.operations == budget &&
          a.progress.stopped_pc == b.progress.stopped_pc &&
          a.progress.stopped_address == b.progress.stopped_address &&
          a.progress.stopped_entry == b.progress.stopped_entry &&
          a.bytes == b.bytes && a.known == b.known &&
          same_machine(a.progress.machine, b.progress.machine));
  }
  for (unsigned call = 1u; call != 6u; ++call) {
    Fixture refused(1u, 0u);
    refused.refuse_call = call;
    check(refused.run() == NBA97_TEXT_IO_REFUSED &&
          refused.progress.callbacks_completed == call - 1u &&
          refused.calls.size() == call);
  }

  Fixture bad_gpr(1u, 0u);
  bad_gpr.corrupt_gpr_call = 1u;
  check(bad_gpr.run() == NBA97_TEXT_ARGUMENT &&
        bad_gpr.progress.stopped_pc == 0x80029618u &&
        bad_gpr.progress.machine.registers.gpr[0].word == 1u);
  Fixture bad_hi(1u, 0u);
  bad_hi.corrupt_hi_call = 1u;
  check(bad_hi.run() == NBA97_TEXT_ARGUMENT &&
        bad_hi.progress.machine.hi.known_mask == 16u);

  for (unsigned mask = 0u; mask != 15u; ++mask) {
    Fixture unknown_ra(0u, 1u);
    unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
        .known_mask = static_cast<std::uint8_t>(mask);
    check(unknown_ra.run() == NBA97_TEXT_UNKNOWN &&
          unknown_ra.progress.stopped_pc == 0x80029684u &&
          unknown_ra.progress.operations == 5u &&
          unknown_ra.progress.restored_s0.word == 0x55667788u &&
          unknown_ra.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                  .word == Sp);
  }
  Fixture misaligned(0u, 1u);
  misaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word =
      0x80067402u;
  check(misaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        misaligned.progress.stopped_pc == 0x80029684u &&
        misaligned.progress.stopped_address == 0x80067402u &&
        misaligned.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP]
                .word == Sp);
}

void alignment_alias_and_wrapped_stack() {
  Fixture unmapped;
  unmapped.region.size = 0x100u;
  check(
      unmapped.run() == NBA97_TEXT_RESOURCE &&
      unmapped.progress.stopped_pc == 0x800295d4u &&
      unmapped.progress.stopped_address == Sp - 8u &&
      unmapped.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
          Sp - 0x18u);

  Fixture aligned;
  aligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
      Sp + 2u;
  check(aligned.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        aligned.progress.stopped_pc == 0x800295d4u);

  Fixture alias(1u, 0u);
  alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word =
      0x800b1f3cu;
  alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0] = {0x44332201u,
                                                                    15u};
  alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80067400u,
                                                                    15u};
  /* The first spill aliases descriptor 0x800B1F34; the later descriptor load
   * must observe those four bytes. */
  check(alias.run() == NBA97_TEXT_ALIGNMENT_TRAP &&
        alias.progress.stopped_pc == 0x80029684u && alias.calls.size() == 5u &&
        alias.entries[0].registers.gpr[NBA97_MATCH_INITIALIZE_A1].word ==
            0x44332201u);

  std::array<std::uint8_t, 8> high_data{};
  std::array<std::uint8_t, 8> high_known{};
  std::array<std::uint8_t, 32> low_data{};
  std::array<std::uint8_t, 32> low_known{};
  std::vector<std::uint8_t> ram_data(0x100000u, 0u);
  std::vector<std::uint8_t> ram_known(0x100000u, 1u);
  high_known.fill(1u);
  low_known.fill(1u);
  Nba97GameTextRegion regions[3] = {
      {0xfffffff8u, high_data.data(), high_known.data(), high_data.size()},
      {0u, low_data.data(), low_known.data(), low_data.size()},
      {Ram, ram_data.data(), ram_known.data(), ram_data.size()}};
  Nba97GamePeriodMusicStartContext context{};
  Nba97GamePeriodMusicStartProgress progress{};
  for (unsigned index = 0u; index != 32u; ++index)
    context.machine.registers.gpr[index] = {0x1000u + index, 15u};
  context.machine.registers.gpr[0] = {0u, 15u};
  context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x10u, 15u};
  context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80067400u, 15u};
  context.memory = {regions, 3u};
  context.operation_budget = 20u;
  ram_data[0x21d7fu] = 0u;
  check(nba97_game_period_music_start(&context, &progress) ==
            NBA97_TEXT_COMPLETE &&
        progress.frame_stack_pointer == 0xfffffff8u &&
        progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            0x10u);
}
} // namespace

int main() {
  every_volume_and_normal_calls();
  load_path_and_callback_live_state();
  unknowns_atomicity_and_store_refusal();
  budgets_failures_and_return_guards();
  alignment_alias_and_wrapped_stack();
  std::printf("game period music start: %u checks passed\n", checks);
}
