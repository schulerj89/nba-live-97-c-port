#include "recovered/game_team_strategy_apply.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

size_t checks;

void check(bool condition, const char *message) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "game_team_strategy_apply_tests: %s\n", message);
    std::exit(1);
  }
}

void write16(std::vector<uint8_t> &data, uint32_t address, uint16_t value) {
  const size_t offset = address - UINT32_C(0x80000000);
  data[offset] = static_cast<uint8_t>(value);
  data[offset + 1u] = static_cast<uint8_t>(value >> 8u);
}

uint16_t read16(const std::vector<uint8_t> &data, uint32_t address) {
  const size_t offset = address - UINT32_C(0x80000000);
  return static_cast<uint16_t>(data[offset] |
                               static_cast<uint16_t>(data[offset + 1u]) << 8u);
}

bool same_word(const Nba97GameTeamStrategyApplyWord &left,
               const Nba97GameTeamStrategyApplyWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool same_machine(const Nba97GameTeamStrategyApplyMachine &left,
                  const Nba97GameTeamStrategyApplyMachine &right) {
  for (unsigned index = 0u; index != 32u; ++index)
    if (!same_word(left.registers.gpr[index], right.registers.gpr[index]))
      return false;
  return same_word(left.hi, right.hi) && same_word(left.lo, right.lo);
}

struct Fixture {
  static constexpr uint32_t base = UINT32_C(0x80000000);
  static constexpr size_t size = 0x200000u;
  static constexpr uint32_t team = UINT32_C(0x80010000);
  static constexpr uint32_t stack = UINT32_C(0x801ff000);
  std::vector<uint8_t> data = std::vector<uint8_t>(size, 0u);
  std::vector<uint8_t> known = std::vector<uint8_t>(size, 1u);
  Nba97GameTextRegion region{base, data.data(), known.data(), data.size()};
  std::array<Nba97GameTeamStrategyApplyAccess, 64> journal{};
  Nba97GameTeamStrategyApplyContext context{};
  Nba97GameTeamStrategyApplyProgress progress{};
  std::array<Nba97GameTeamStrategyApplyEvent, 4> events{};
  std::array<Nba97GameTeamStrategyApplyMachine, 4> event_machines{};
  size_t event_count{};
  bool reject{};
  bool corrupt{};
  bool relocate{};
  uint32_t relocated_team = UINT32_C(0x80012000);
  uint32_t relocated_stack = UINT32_C(0x801fe000);

  Fixture() {
    context.memory = {&region, 1u};
    context.operation_budget = 128u;
    context.io = callback;
    context.user = this;
    context.access_journal = journal.data();
    context.access_journal_capacity = journal.size();
    for (unsigned index = 0u; index != 32u; ++index)
      context.machine.registers.gpr[index] =
          {UINT32_C(0x31000000) + index * UINT32_C(0x01010101), 15u};
    context.machine.registers.gpr[0] = {0u, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] = {team, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {stack, 15u};
    context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] =
        {UINT32_C(0x80065ac4), 15u};
    context.machine.hi = {UINT32_C(0x11223344), 5u};
    context.machine.lo = {UINT32_C(0x55667788), 10u};
    write16(data, team + 0x14u, 0u);
    write16(data, team + 0x42u, 1u);
    write16(data, team + 0x66u, 3u);
    write16(data, relocated_team + 0x66u, 9u);
    write16(data, UINT32_C(0x8001edec), 0u);
    data[UINT32_C(0x80021ed5) - base] = 0u;
    data[UINT32_C(0x80021ed6) - base] = 0u;
    const uint32_t settings[7] = {UINT32_C(0x80021dea),
                                  UINT32_C(0x80021de8),
                                  UINT32_C(0x80021de6),
                                  UINT32_C(0x80021dec),
                                  UINT32_C(0x80021dee),
                                  UINT32_C(0x80021df0),
                                  UINT32_C(0x80021df2)};
    for (unsigned index = 0u; index != 7u; ++index) {
      data[settings[index] - base] = static_cast<uint8_t>(0x20u + index);
      data[settings[index] - base + 1u] =
          static_cast<uint8_t>(0x40u + index);
    }
    for (unsigned slot = 0u; slot != 24u; ++slot) {
      write16(data, UINT32_C(0x8001f80c) + slot * 34u, UINT16_C(0xffff));
      write16(data, team + 0x16u + slot * 2u,
              static_cast<uint16_t>(0x100u + slot));
    }
  }

  static int callback(void *opaque, const Nba97GameTextMemory *,
                      const Nba97GameTeamStrategyApplyEvent *event,
                      Nba97GameTeamStrategyApplyMachine *machine) {
    auto &fixture = *static_cast<Fixture *>(opaque);
    fixture.events[fixture.event_count] = *event;
    fixture.event_machines[fixture.event_count] = *machine;
    ++fixture.event_count;
    if (fixture.relocate) {
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_S0] =
          {fixture.relocated_team, 15u};
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_SP] =
          {fixture.relocated_stack, 15u};
      machine->hi = {UINT32_C(0xaabbccdd), 3u};
      machine->lo = {UINT32_C(0x12345678), 12u};
      const size_t frame = fixture.relocated_stack - Fixture::base;
      const uint32_t restored_s0 = UINT32_C(0x76543210);
      const uint32_t restored_ra = UINT32_C(0x80001234);
      for (unsigned byte = 0u; byte != 4u; ++byte) {
        fixture.data[frame + 0x10u + byte] =
            static_cast<uint8_t>(restored_s0 >> (8u * byte));
        fixture.data[frame + 0x14u + byte] =
            static_cast<uint8_t>(restored_ra >> (8u * byte));
      }
    }
    if (fixture.corrupt)
      machine->hi.known_mask = 16u;
    return fixture.reject ? 0 : 1;
  }

  int run() { return nba97_game_team_strategy_apply(&context, &progress); }
};

void human_direct_and_launch_paths() {
  Fixture human;
  auto original = human.context.machine;
  check(human.run() == NBA97_TEXT_COMPLETE, "home human direct completes");
  check(human.event_count == 1u &&
            human.events[0].kind == NBA97_GAME_TEAM_STRATEGY_APPLY_80064DBC &&
            human.events[0].pc == UINT32_C(0x800659c4) &&
            human.events[0].delay_slot_pc == UINT32_C(0x800659c8) &&
            human.events[0].entry == UINT32_C(0x80064dbc) &&
            human.events[0].argument_count == 4u,
        "direct child metadata");
  check(human.event_machines[0].registers
                    .gpr[NBA97_MATCH_INITIALIZE_A0]
                    .word == Fixture::team &&
            human.event_machines[0].registers
                    .gpr[NBA97_MATCH_INITIALIZE_A1]
                    .word == 0u &&
            human.event_machines[0].registers
                    .gpr[NBA97_MATCH_INITIALIZE_A2]
                    .word == UINT32_C(0x8001f7ec) &&
            human.event_machines[0].registers
                    .gpr[NBA97_MATCH_INITIALIZE_A3]
                    .word == 0u &&
            human.event_machines[0].registers
                    .gpr[NBA97_MATCH_INITIALIZE_RA]
                    .word == UINT32_C(0x800659cc),
        "direct child exact argument machine");
  check(read16(human.data, Fixture::team + 0x66u) == 2u,
        "count decremented");
  const uint32_t destinations[7] = {0x78u, 0x77u, 0x76u, 0x38u,
                                    0x39u, 0x36u, 0x37u};
  for (unsigned index = 0u; index != 7u; ++index)
    check(human.data[Fixture::team - Fixture::base + destinations[index]] ==
              static_cast<uint8_t>(0x20u + index),
          "home strategy byte order");
  const uint32_t pair_pcs[14] = {
      UINT32_C(0x8006586c), UINT32_C(0x80065874),
      UINT32_C(0x80065880), UINT32_C(0x80065888),
      UINT32_C(0x80065894), UINT32_C(0x8006589c),
      UINT32_C(0x800658a8), UINT32_C(0x800658b0),
      UINT32_C(0x800658bc), UINT32_C(0x800658c4),
      UINT32_C(0x800658d0), UINT32_C(0x800658d8),
      UINT32_C(0x800658e4), UINT32_C(0x800658ec)};
  for (unsigned index = 0u; index != 14u; ++index)
    check(human.journal[5u + index].pc == pair_pcs[index],
          "strategy read/store source order");
  check(human.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            Fixture::stack,
        "SP restored");
  check(same_word(human.progress.machine.hi, original.hi) &&
            same_word(human.progress.machine.lo, original.lo),
        "HI LO preserved");
  for (unsigned index = 0u; index != 32u; ++index) {
    if (index >= NBA97_MATCH_INITIALIZE_AT &&
        index <= NBA97_MATCH_INITIALIZE_A3)
      continue;
    check(same_word(human.progress.machine.registers.gpr[index],
                    original.registers.gpr[index]),
          "all untouched and restored GPR fields preserved");
  }

  Fixture away;
  write16(away.data, Fixture::team + 0x14u, 1u);
  away.data[UINT32_C(0x80021ed6) - Fixture::base] = 4u;
  check(away.run() == NBA97_TEXT_COMPLETE, "away direct completes");
  check(away.data[Fixture::team - Fixture::base + 0x78u] == 0x40u,
        "away setting selected");
  for (unsigned index = 0u; index != 7u; ++index)
    check(away.data[Fixture::team - Fixture::base + destinations[index]] ==
              static_cast<uint8_t>(0x40u + index),
          "away strategy byte pair");
  check(away.events[0].kind == NBA97_GAME_TEAM_STRATEGY_APPLY_80064DBC,
        "away direct child");

  Fixture launch;
  write16(launch.data, UINT32_C(0x8001edec), 1u);
  launch.data[UINT32_C(0x80021ed5) - Fixture::base] = 12u;
  check(launch.run() == NBA97_TEXT_COMPLETE, "launch no-injury completes");
  check(launch.data[Fixture::team - Fixture::base + 0x76u] == 1u &&
            launch.data[Fixture::team - Fixture::base + 0x77u] == 0u &&
            launch.event_count == 0u,
        "launch changes only byte 76");

  Fixture cpu;
  write16(cpu.data, Fixture::team + 0x42u, 0u);
  cpu.data[UINT32_C(0x80021ed5) - Fixture::base] = 255u;
  check(cpu.run() == NBA97_TEXT_COMPLETE, "CPU no-injury completes");
  check(cpu.data[Fixture::team - Fixture::base + 0x76u] == 1u &&
            cpu.data[Fixture::team - Fixture::base + 0x77u] == 1u,
        "CPU default bytes");
}

void injury_boundaries_scan_and_swap() {
  for (uint8_t injury : {uint8_t{0u}, uint8_t{4u}, uint8_t{12u},
                         uint8_t{255u}}) {
    Fixture fixture;
    fixture.data[UINT32_C(0x80021ed5) - Fixture::base] = injury;
    check(fixture.run() == NBA97_TEXT_COMPLETE, "injury boundary completes");
    check(fixture.event_count == (injury < 5u ? 1u : 0u),
          "injury boundary child count");
    check(read16(fixture.data, Fixture::team + 0x66u) ==
              (injury < 12u ? 2u : 3u),
          "injury boundary decrement gate");
  }

  for (uint8_t injury : {uint8_t{5u}, uint8_t{10u}, uint8_t{11u}}) {
    Fixture equal;
    equal.data[UINT32_C(0x80021ed5) - Fixture::base] = injury;
    check(equal.run() == NBA97_TEXT_COMPLETE, "scan equal completes");
    check(equal.event_count == 1u &&
              equal.events[0].kind ==
                  NBA97_GAME_TEAM_STRATEGY_APPLY_800646A8 &&
              equal.events[0].pc == UINT32_C(0x80065998),
          "scan child metadata");
  }

  Fixture swap;
  swap.data[UINT32_C(0x80021ed5) - Fixture::base] = 5u;
  write16(swap.data, UINT32_C(0x8001f80c) + 11u * 34u, 0u);
  const uint16_t injured = read16(swap.data, Fixture::team + 0x16u + 10u);
  const uint16_t candidate = read16(swap.data, Fixture::team + 0x16u + 22u);
  check(swap.run() == NBA97_TEXT_COMPLETE, "available scan swap completes");
  check(read16(swap.data, Fixture::team + 0x16u + 10u) == candidate &&
            read16(swap.data, Fixture::team + 0x16u + 22u) == injured,
        "lineup halfwords swapped");
  check(swap.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word ==
            injured &&
            swap.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A1]
                    .word == candidate,
        "source a0 a1 swap values retained");
  check(swap.event_machines[0].registers
                    .gpr[NBA97_MATCH_INITIALIZE_RA]
                    .word == UINT32_C(0x800659a0),
        "scan child JAL return address");

  Fixture alias;
  const uint32_t alias_team = UINT32_C(0x80021d70);
  alias.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0] =
      {alias_team, 15u};
  write16(alias.data, alias_team + 0x14u, 0u);
  write16(alias.data, alias_team + 0x42u, 1u);
  alias.data[UINT32_C(0x80021dea) - Fixture::base] = 0x5au;
  alias.data[UINT32_C(0x80021de8) - Fixture::base] = 0x6bu;
  alias.data[UINT32_C(0x80021ed5) - Fixture::base] = 12u;
  check(alias.run() == NBA97_TEXT_COMPLETE, "strategy/team alias completes");
  check(alias.data[alias_team - Fixture::base + 0x77u] == 0x5au,
        "later source load observes earlier strategy store");
}

void callback_live_state_and_failures() {
  Fixture live;
  live.relocate = true;
  check(live.run() == NBA97_TEXT_COMPLETE, "callback relocation completes");
  check(read16(live.data, live.relocated_team + 0x66u) == 8u,
        "callback-live S0 count decremented");
  check(live.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word ==
            live.relocated_stack + 0x18u &&
            live.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0]
                    .word == UINT32_C(0x76543210) &&
            live.progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
                    .word == UINT32_C(0x80001234),
        "callback-live SP epilogue");
  check(live.progress.machine.hi.word == UINT32_C(0xaabbccdd) &&
            live.progress.machine.lo.word == UINT32_C(0x12345678),
        "callback HI LO mutations retained");

  Fixture refused;
  refused.reject = true;
  check(refused.run() == NBA97_TEXT_IO_REFUSED, "callback refusal reported");
  check(refused.progress.stopped_pc == UINT32_C(0x800659c4) &&
            refused.progress.machine.registers
                    .gpr[NBA97_MATCH_INITIALIZE_A3]
                    .word == 0u,
        "direct-call delay prefix retained");

  Fixture corrupt;
  corrupt.corrupt = true;
  check(corrupt.run() == NBA97_TEXT_ARGUMENT,
        "malformed callback machine rejected");

  Fixture underflow;
  write16(underflow.data, Fixture::team + 0x66u, 0u);
  check(underflow.run() == NBA97_TEXT_COMPLETE, "zero count path completes");
  check(read16(underflow.data, Fixture::team + 0x66u) == UINT16_C(0xffff),
        "count underflows exactly");
}

void budgets_unknowns_and_invalid_inputs() {
  Fixture baseline;
  check(baseline.run() == NBA97_TEXT_COMPLETE, "budget baseline completes");
  const size_t total = baseline.progress.operations;
  for (size_t budget = 0u; budget != total; ++budget) {
    Fixture limited;
    limited.context.operation_budget = budget;
    check(limited.run() == NBA97_TEXT_LIMIT, "every operation budget limits");
    check(limited.progress.operations == budget, "budget prefix exact");
  }

  for (unsigned path = 0u; path != 3u; ++path) {
    Fixture path_baseline;
    if (path == 0u) {
      write16(path_baseline.data, UINT32_C(0x8001edec), 1u);
      path_baseline.data[UINT32_C(0x80021ed5) - Fixture::base] = 12u;
    } else if (path == 1u) {
      write16(path_baseline.data, Fixture::team + 0x42u, 0u);
      path_baseline.data[UINT32_C(0x80021ed5) - Fixture::base] = 12u;
    } else {
      path_baseline.data[UINT32_C(0x80021ed5) - Fixture::base] = 5u;
    }
    check(path_baseline.run() == NBA97_TEXT_COMPLETE,
          "alternate budget baseline completes");
    for (size_t budget = 0u; budget != path_baseline.progress.operations;
         ++budget) {
      Fixture limited;
      if (path == 0u) {
        write16(limited.data, UINT32_C(0x8001edec), 1u);
        limited.data[UINT32_C(0x80021ed5) - Fixture::base] = 12u;
      } else if (path == 1u) {
        write16(limited.data, Fixture::team + 0x42u, 0u);
        limited.data[UINT32_C(0x80021ed5) - Fixture::base] = 12u;
      } else {
        limited.data[UINT32_C(0x80021ed5) - Fixture::base] = 5u;
      }
      limited.context.operation_budget = budget;
      check(limited.run() == NBA97_TEXT_LIMIT,
            "alternate path budget limits");
      check(limited.progress.operations == budget,
            "alternate budget prefix exact");
    }
  }

  Fixture unknown_mode;
  unknown_mode.known[UINT32_C(0x8001edec) - Fixture::base] = 0u;
  check(unknown_mode.run() == NBA97_TEXT_UNKNOWN,
        "unknown mode branch refuses");
  check(unknown_mode.progress.stopped_pc == UINT32_C(0x80065840) &&
            unknown_mode.progress.machine.registers
                    .gpr[NBA97_MATCH_INITIALIZE_A0]
                    .word == 0u,
        "mode branch delay predicate retained");

  Fixture unknown_side;
  write16(unknown_side.data, UINT32_C(0x8001edec), 1u);
  unknown_side.known[Fixture::team - Fixture::base + 0x14u] = 0u;
  unknown_side.data[UINT32_C(0x80021ed5) - Fixture::base] = 12u;
  check(unknown_side.run() == NBA97_TEXT_UNKNOWN,
        "unknown reread side refuses");
  check(unknown_side.progress.stopped_pc == UINT32_C(0x80065900) &&
            unknown_side.progress.machine.registers
                    .gpr[NBA97_MATCH_INITIALIZE_A2]
                    .word == 12u,
        "side branch delay a2 retained");

  Fixture unknown_injury;
  unknown_injury.known[UINT32_C(0x80021ed5) - Fixture::base] = 0u;
  check(unknown_injury.run() == NBA97_TEXT_UNKNOWN,
        "unknown injury predicate refuses");
  check(unknown_injury.progress.stopped_pc == UINT32_C(0x8006594c) &&
            unknown_injury.progress.machine.registers
                    .gpr[NBA97_MATCH_INITIALIZE_V0]
                    .known_mask == 14u,
        "injury delay SLTI knownness retained");

  Fixture unknown_status;
  unknown_status.data[UINT32_C(0x80021ed5) - Fixture::base] = 5u;
  const uint32_t status = UINT32_C(0x8001f80c) + 11u * 34u;
  unknown_status.known[status - Fixture::base + 1u] = 0u;
  check(unknown_status.run() == NBA97_TEXT_UNKNOWN,
        "unknown signed status refuses");
  check(unknown_status.progress.stopped_pc == UINT32_C(0x80065984),
        "unknown status branch PC");

  Fixture malformed;
  malformed.known[Fixture::team - Fixture::base + 0x15u] = 2u;
  auto initial_v0 =
      malformed.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_V0];
  check(malformed.run() == NBA97_TEXT_ARGUMENT,
        "malformed later load byte rejects");
  check(same_word(malformed.progress.machine.registers
                      .gpr[NBA97_MATCH_INITIALIZE_V0],
                  initial_v0),
        "malformed load destination atomic");

  Fixture unaligned;
  unaligned.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word++;
  check(unaligned.run() == NBA97_TEXT_ALIGNMENT_TRAP,
        "unaligned frame store traps");
  Fixture unmapped;
  unmapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_A0].word =
      UINT32_C(0x90000000);
  check(unmapped.run() == NBA97_TEXT_RESOURCE, "unmapped team refuses");
  Fixture bad_machine;
  bad_machine.context.machine.lo.known_mask = 16u;
  check(bad_machine.run() == NBA97_TEXT_ARGUMENT, "bad machine rejects");
  Fixture bad_journal;
  bad_journal.context.access_journal = nullptr;
  check(bad_journal.run() == NBA97_TEXT_ARGUMENT, "bad journal rejects");

  Fixture partial_store;
  const uint32_t team = Fixture::team;
  const uint32_t stack_frame = Fixture::stack - 0x18u;
  std::array<Nba97GameTextRegion, 5> split = {
      Nba97GameTextRegion{team, partial_store.data.data() + team - Fixture::base,
                          nullptr, 0x100u},
      Nba97GameTextRegion{UINT32_C(0x8001edec),
                          partial_store.data.data() +
                              UINT32_C(0x8001edec) - Fixture::base,
                          nullptr, 2u},
      Nba97GameTextRegion{UINT32_C(0x80021de6),
                          partial_store.data.data() +
                              UINT32_C(0x80021de6) - Fixture::base,
                          partial_store.known.data() +
                              UINT32_C(0x80021de6) - Fixture::base,
                          14u},
      Nba97GameTextRegion{UINT32_C(0x80021ed5),
                          partial_store.data.data() +
                              UINT32_C(0x80021ed5) - Fixture::base,
                          nullptr, 2u},
      Nba97GameTextRegion{stack_frame,
                          partial_store.data.data() + stack_frame - Fixture::base,
                          nullptr, 0x18u}};
  partial_store.known[UINT32_C(0x80021dea) - Fixture::base] = 0u;
  partial_store.data[UINT32_C(0x80021ed5) - Fixture::base] = 12u;
  partial_store.context.memory = {split.data(), split.size()};
  const uint8_t original_destination =
      partial_store.data[team - Fixture::base + 0x78u];
  check(partial_store.run() == NBA97_TEXT_ARGUMENT,
        "partial strategy byte rejects known-null destination");
  check(partial_store.progress.stopped_pc == UINT32_C(0x80065874) &&
            partial_store.data[team - Fixture::base + 0x78u] ==
                original_destination,
        "partial rejected store is atomic");
}

void epilogue_wrap_and_determinism() {
  Fixture unknown_ra;
  unknown_ra.data[UINT32_C(0x80021ed5) - Fixture::base] = 12u;
  unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA]
      .known_mask = 14u;
  const auto original_s0 =
      unknown_ra.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_S0];
  check(unknown_ra.run() == NBA97_TEXT_UNKNOWN,
        "unknown saved RA refuses at JR");
  check(unknown_ra.progress.stopped_pc == UINT32_C(0x800659e8) &&
            unknown_ra.progress.machine.registers
                    .gpr[NBA97_MATCH_INITIALIZE_SP]
                    .word == Fixture::stack &&
            same_word(unknown_ra.progress.machine.registers
                          .gpr[NBA97_MATCH_INITIALIZE_S0],
                      original_s0),
        "unknown RA refusal follows S0 reload and SP release");

  Fixture wrapped;
  wrapped.data[UINT32_C(0x80021ed5) - Fixture::base] = 12u;
  wrapped.context.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {8u, 15u};
  std::array<uint8_t, 16> high_data{};
  std::array<uint8_t, 16> high_known{};
  std::array<uint8_t, 8> low_data{};
  std::array<uint8_t, 8> low_known{};
  high_known.fill(1u);
  low_known.fill(1u);
  std::array<Nba97GameTextRegion, 3> regions = {
      wrapped.region,
      Nba97GameTextRegion{UINT32_C(0xfffffff0), high_data.data(),
                          high_known.data(), high_data.size()},
      Nba97GameTextRegion{0u, low_data.data(), low_known.data(),
                          low_data.size()}};
  wrapped.context.memory = {regions.data(), regions.size()};
  check(wrapped.run() == NBA97_TEXT_COMPLETE,
        "mapped high-low stack wrap completes");
  check(wrapped.progress.frame_stack_pointer == UINT32_C(0xfffffff0) &&
            wrapped.progress.machine.registers
                    .gpr[NBA97_MATCH_INITIALIZE_SP]
                    .word == 8u &&
            wrapped.journal[0].address == 0u &&
            wrapped.journal[1].address == 4u,
        "wrapped frame stores and restores exact addresses");

  Fixture first;
  Fixture second;
  check(first.run() == NBA97_TEXT_COMPLETE &&
            second.run() == NBA97_TEXT_COMPLETE,
        "repeat fixtures complete");
  check(same_machine(first.progress.machine, second.progress.machine) &&
            first.data == second.data &&
            first.progress.operations == second.progress.operations &&
            first.progress.access_events == second.progress.access_events,
        "deterministic semantic machine memory and counts");
}

} // namespace

int main() {
  human_direct_and_launch_paths();
  injury_boundaries_scan_and_swap();
  callback_live_state_and_failures();
  budgets_unknowns_and_invalid_inputs();
  epilogue_wrap_and_determinism();
  std::printf("game_team_strategy_apply_tests: %zu checks passed\n", checks);
  return 0;
}
