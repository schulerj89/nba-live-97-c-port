#include "game_team_tactics_update_adapter.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr,
                 "team tactics integration check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)
constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Ball = 0x80012000u;

bool same_word(const Nba97GameTeamTacticsWord &left,
               const Nba97GameTeamTacticsWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}
bool same_machine(const Nba97GameTeamTacticsMachine &left,
                  const Nba97GameTeamTacticsMachine &right) {
  for (unsigned index = 0u; index != NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
       ++index)
    if (!same_word(left.registers.gpr[index], right.registers.gpr[index]))
      return false;
  return same_word(left.hi, right.hi) && same_word(left.lo, right.lo);
}

struct ExpectedService {
  std::uint32_t pc;
  std::uint32_t entry;
  std::uint32_t a0;
  unsigned count;
};

struct ChildExpected {
  std::uint32_t pc;
  std::uint32_t entry;
  std::uint32_t a0;
  std::uint32_t a2;
  unsigned argc;
};
struct NaturalFixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000u, 0u);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000u, 1u);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97MatchTickContext tick{};
  Nba97MatchTickProgress tick_progress{};
  Nba97GameTeamTacticsBinding binding{};
  std::vector<Nba97MatchTickCall> services;
  std::size_t expected_index{};
  unsigned player_calls{};
  unsigned ball_calls{};
  unsigned child_calls{};
  Nba97GameTeamTacticsEvent child_event{};
  std::vector<Nba97GameTeamTacticsEvent> child_events;
  bool unexpected{};

  static constexpr std::array<ChildExpected, 23> ExpectedChildren{{
      {0x800749ccu, 0x800706e4u, 0u, 0x800300c0u, 3u},
      {0x800749f0u, 0x800706e4u, 0u, 0x800300bcu, 3u},
      {0x800749ccu, 0x800706e4u, 0u, 0x800301b4u, 3u},
      {0x800749f0u, 0x800706e4u, 0u, 0x800301b0u, 3u},
      {0x800749ccu, 0x800706e4u, 0u, 0x800302a8u, 3u},
      {0x800749f0u, 0x800706e4u, 0u, 0x800302a4u, 3u},
      {0x800749ccu, 0x800706e4u, 0u, 0x8003039cu, 3u},
      {0x800749f0u, 0x800706e4u, 0u, 0x80030398u, 3u},
      {0x800749ccu, 0x800706e4u, 0u, 0x80030490u, 3u},
      {0x800749f0u, 0x800706e4u, 0u, 0x8003048cu, 3u},
      {0x80074ae8u, 0x8007066cu, 0u, 0u, 2u},
      {0x80074c1cu, 0x800706e4u, 0u, 0x80030580u, 3u},
      {0x80074c44u, 0x800706e4u, 0u, 0x80030584u, 3u},
      {0x80074c1cu, 0x800706e4u, 0u, 0x80030674u, 3u},
      {0x80074c44u, 0x800706e4u, 0u, 0x80030678u, 3u},
      {0x80074c1cu, 0x800706e4u, 0u, 0x80030768u, 3u},
      {0x80074c44u, 0x800706e4u, 0u, 0x8003076cu, 3u},
      {0x80074c1cu, 0x800706e4u, 0u, 0x8003085cu, 3u},
      {0x80074c44u, 0x800706e4u, 0u, 0x80030860u, 3u},
      {0x80074c1cu, 0x800706e4u, 0u, 0x80030950u, 3u},
      {0x80074c44u, 0x800706e4u, 0u, 0x80030954u, 3u},
      {0x80074d30u, 0x800295c0u, 0u, 0u, 0u},
      {0x80075458u, 0x80072b70u, 0x8001edf4u, 0u, 1u},
  }};
  static constexpr std::array<ExpectedService, 13> Expected{{
      {0x80068c24u, 0x80066f88u, 0u, 0u},
      {0x80068c2cu, 0x80079664u, 0u, 1u},
      {0x80068c4cu, 0x80067468u, 0u, 0u},
      {0x80068cecu, 0x80067550u, 0u, 0u},
      {0x80068cf4u, 0x800675e4u, 0u, 0u},
      {0x80068d40u, 0x80067a60u, 1u, 1u},
      {0x80068d64u, 0x80067d38u, 1u, 1u},
      {0x80068d6cu, 0x80067664u, 0u, 0u},
      {0x80068d7cu, 0x8002de34u, 0u, 0u},
      {0x80068e00u, 0x80060ef8u, 0u, 0u},
      {0x80068e08u, 0x80060fbcu, 0u, 0u},
      {0x80068e28u, 0x800747b0u, 0u, 0u},
      {0x80068e30u, 0x8006817cu, 0u, 0u},
  }};

  NaturalFixture() {
    tick.access = access;
    tick.service = service;
    tick.player_update = player;
    tick.ball_simulation = ball;
    tick.net_transform = net;
    tick.match_frame = frame;
    tick.user = this;
    tick.operation_budget = 1000u;
    tick.incoming_s6 = {2u, 1u};
    binding.memory = {&region, 1u};
    binding.operation_budget = std::numeric_limits<std::size_t>::max();
    binding.entry_machine = machine(0x80068e30u);
    binding.entry_machine_ready = 1u;
    binding.io_user = this;

    /* These values only establish an explicit synthetic path through the
     * already-owned prerequisite portion of match tick. */
    put(0x8001edecu, 2u, 2u);
    put(0x800fdb92u, 2u, 2u);
    put(0x800fdb8au, 2u, 2u);
    put(0x80021d82u, 1u, 1u);
    put(0x800fdb7cu, 2u, 0u);
    put(0x800fe8ccu, 2u, 0u);
    put(0x800fe8c4u, 2u, 0u);
    put(0x800fdb68u, 2u, 0u);
    put(0x800fdb78u, 1u, 1u);
    put(0x800fdb6cu, 2u, 1u);
    put(0x800fdbaeu, 2u, 0u);
    put(0x800fdc48u, 4u, Ball);
    put(0x800fdbccu, 2u, 0u);
    put(0x800fdbd2u, 2u, 0u);
    put(0x800fdb90u, 2u, 0u);
    put(0x800fdb94u, 2u, 0xffffu);
  }

  static Nba97GameTeamTacticsMachine machine(std::uint32_t ra) {
    Nba97GameTeamTacticsMachine value{};
    for (unsigned index = 0u; index != NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
         ++index)
      value.registers.gpr[index] = {
          0x51000000u + index * 0x01010101u,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    value.registers.gpr[0] = {0u, 0x0fu};
    value.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x801ff000u, 0x0fu};
    value.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {ra, 0x0fu};
    value.hi = {0x13579bdfu, 0x03u};
    value.lo = {0x2468ace0u, 0x0cu};
    return value;
  }
  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }
  void put(std::uint32_t address, unsigned width, std::uint32_t value) {
    for (unsigned byte = 0u; byte != width; ++byte) {
      bytes[offset(address) + byte] =
          static_cast<std::uint8_t>(value >> (8u * byte));
      known[offset(address) + byte] = 1u;
    }
  }

  void prepare_geometry_boundary() {
    put(0x800fdb94u, 2u, 0u);
    put(0x800fe872u, 2u, 1u);
    put(0x800fdb6cu, 2u, 1u);
    put(0x8001edf8u, 4u, 0x80040000u);
    put(0x8001ee04u, 4u, 0x00000400u);
    put(0x8001ee08u, 2u, 0u);
    put(0x800fdc34u, 4u, 0x80050000u);
    put(0x80050008u, 4u, 1000u);
    put(0x8005000cu, 4u, 2000u);
    put(0x80020becu, 4u, 0x80030000u);
    put(0x80030008u, 4u, 100u);
    put(0x8003000cu, 4u, 200u);
    binding.io = child_refuse;
  }
  void prepare_complete_nontrivial() {
    put(0x800fdb68u, 2u, 0u);
    put(0x800fdbccu, 2u, 0u);
    put(0x800fdbd2u, 2u, 0u);
    put(0x800fdb94u, 2u, 0u);
    put(0x800fdb6cu, 2u, 1u);
    put(0x800fdba4u, 4u, 360u);
    put(0x800fdbc0u, 4u, 1000u);
    put(0x800fdbc4u, 4u, 2000u);
    put(0x800fdc34u, 4u, 0x80030000u);
    put(0x800fdc48u, 4u, Ball);
    put(0x800fe872u, 2u, 1u);
    put(0x800fe86eu, 2u, 0u);
    put(0x800fe866u, 2u, 0u);
    put(0x800fe868u, 2u, 0xffffu);
    put(0x800fe86au, 2u, 5u);
    put(0x800fe870u, 2u, 1u);
    put(0x800fe8a8u, 2u, 0u);
    put(0x800fe8a4u, 2u, 10u);
    put(0x8001edf8u, 4u, 0x8001eeb8u);
    put(0x8001ee04u, 4u, 500u);
    put(0x8001ee08u, 2u, 0u);
    put(0x8001ee68u, 2u, 100u);
    put(0x8001ee98u, 2u, 0u);
    put(0x8001ee60u, 4u, 0x80130000u);
    put(0x8001eebcu, 4u, 0x8001edf4u);
    put(0x8001eec8u, 4u, 500u);
    put(0x8001eeccu, 2u, 5u);
    put(0x8001ef2au, 2u, 20u);
    for (unsigned index = 0u; index != 10u; ++index) {
      const auto actor = 0x80030000u + index * 0xf4u;
      put(0x80020becu + index * 4u, 4u, actor);
      put(actor, 4u, index);
      put(actor + 4u, 2u, 0xffffu);
      put(actor + 8u, 4u, 100u + index * 10u);
      put(actor + 0xcu, 4u, 200u + index * 10u);
      put(actor + 0x1au, 1u, 1u);
      put(actor + 0xccu, 2u, index);
      put(actor + 0xd4u, 2u, 0xffffu);
      put(actor + 0xd6u, 2u, 0xffffu);
    }
    put(Ball + 8u, 4u, 600u);
    put(Ball + 0xcu, 4u, 0u);
    put(0x800bb7f8u, 4u, 0x80140000u);
    put(0x80140000u, 2u, 120u);
    put(0x80140002u, 2u, 0xffffu);
    put(0x80140004u, 2u, 0xffffu);
    put(0x80140006u, 2u, 0xffffu);
    binding.io = child_complete;
  }
  static int child_complete(void *opaque, const Nba97GameTextMemory *,
                            const Nba97GameTeamTacticsEvent *event,
                            Nba97GameTeamTacticsMachine *machine) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    std::size_t invocation = 1u;
    std::uint8_t kind = 0u;
    if (event == nullptr || machine == nullptr ||
        fixture.child_events.size() >= ExpectedChildren.size())
      return 0;
    const auto &expected = ExpectedChildren[fixture.child_events.size()];
    for (std::size_t index = 0u; index != fixture.child_events.size(); ++index)
      invocation += ExpectedChildren[index].entry == expected.entry;
    if (expected.entry == 0x800706e4u)
      kind = NBA97_GAME_TEAM_TACTICS_CHILD_800706E4;
    else if (expected.entry == 0x8007066cu)
      kind = NBA97_GAME_TEAM_TACTICS_CHILD_8007066C;
    else if (expected.entry == 0x800295c0u)
      kind = NBA97_GAME_TEAM_TACTICS_CHILD_800295C0;
    else if (expected.entry == 0x80072b70u)
      kind = NBA97_GAME_TEAM_TACTICS_CHILD_80072B70;
    if (event->pc != expected.pc || event->delay_slot_pc != expected.pc + 4u ||
        event->entry != expected.entry || event->kind != kind ||
        event->invocation != invocation ||
        event->argument_count != expected.argc ||
        machine->registers.gpr[31].known_mask != 0x0fu ||
        machine->registers.gpr[31].word != expected.pc + 8u ||
        (expected.a0 != 0u && machine->registers.gpr[4].word != expected.a0) ||
        (expected.a2 != 0u && machine->registers.gpr[6].word != expected.a2))
      return 0;
    ++fixture.child_calls;
    fixture.child_event = *event;
    fixture.child_events.push_back(*event);
    if (event->entry == 0x800706e4u) {
      fixture.put(machine->registers.gpr[6].word, 2u, 100u);
      machine->registers.gpr[2] = {100u << 8u, 0x0fu};
    } else if (event->entry == 0x8007066cu) {
      machine->registers.gpr[2] = {120u << 8u, 0x0fu};
    } else if (event->entry == 0x800295c0u || event->entry == 0x80072b70u) {
      machine->registers.gpr[2] = {1u, 0x0fu};
    } else
      return 0;
    return 1;
  }
  static int child_refuse(void *opaque, const Nba97GameTextMemory *,
                          const Nba97GameTeamTacticsEvent *event,
                          Nba97GameTeamTacticsMachine *) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    ++fixture.child_calls;
    fixture.child_event = *event;
    return 0;
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0u;
    for (unsigned byte = 0u; byte != width; ++byte)
      value |= static_cast<std::uint32_t>(bytes[offset(address) + byte])
               << (8u * byte);
    return value;
  }
  static int access(void *opaque, std::uint32_t, std::uint32_t address,
                    unsigned width, unsigned kind,
                    Nba97PlayerFrameValue *value) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    if (value == nullptr || address < Ram ||
        static_cast<std::uint64_t>(address - Ram) + width >
            fixture.bytes.size())
      return NBA97_BODY_UNKNOWN;
    const auto at = fixture.offset(address);
    if (kind == NBA97_FRAME_READ) {
      *value = {};
      for (unsigned byte = 0u; byte != width; ++byte) {
        value->word |= static_cast<std::uint32_t>(fixture.bytes[at + byte])
                       << (8u * byte);
        if (fixture.known[at + byte])
          value->known_mask =
              static_cast<std::uint8_t>(value->known_mask | (1u << byte));
      }
    } else {
      for (unsigned byte = 0u; byte != width; ++byte) {
        fixture.bytes[at + byte] =
            static_cast<std::uint8_t>(value->word >> (8u * byte));
        fixture.known[at + byte] =
            static_cast<std::uint8_t>((value->known_mask >> byte) & 1u);
      }
    }
    return NBA97_BODY_OK;
  }

  static int service(void *opaque, const Nba97MatchTickCall *call,
                     Nba97GamePeriodValue *result) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    /* Every prerequisite completion is an exact synthetic contract.  This is
     * not a generic successful service or an advancing-game claim. */
    if (call == nullptr || fixture.expected_index >= Expected.size()) {
      fixture.unexpected = true;
      return NBA97_BODY_ARGUMENT;
    }
    const auto &expected = Expected[fixture.expected_index];
    if (call->pc != expected.pc || call->entry != expected.entry ||
        call->args[0] != expected.a0 || call->args[1] != 0u ||
        call->count != expected.count) {
      fixture.unexpected = true;
      return NBA97_BODY_ARGUMENT;
    }
    ++fixture.expected_index;
    fixture.services.push_back(*call);
    if (call->pc == 0x80068d6cu) {
      if (result == nullptr)
        return NBA97_BODY_ARGUMENT;
      *result = {0u, 1u};
      return NBA97_BODY_OK;
    }
    if (call->pc == 0x80068e28u)
      return nba97_game_team_tactics_update_from_match_tick(&fixture.binding,
                                                            call, result)
                 ? NBA97_BODY_OK
                 : NBA97_BODY_ARGUMENT;
    if (call->pc == 0x80068e30u)
      return NBA97_BODY_ARGUMENT;
    return result == nullptr ? NBA97_BODY_OK : NBA97_BODY_ARGUMENT;
  }
  static int player(void *opaque, std::uint32_t pc) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    if (pc != 0x80068d84u)
      return NBA97_BODY_ARGUMENT;
    ++fixture.player_calls;
    return NBA97_BODY_OK;
  }
  static int ball(void *opaque, std::uint32_t pc, std::uint32_t pointer) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    if (pc != 0x80068d9cu || pointer != Ball)
      return NBA97_BODY_ARGUMENT;
    ++fixture.ball_calls;
    return NBA97_BODY_OK;
  }
  static int net(void *opaque, std::uint32_t) {
    static_cast<NaturalFixture *>(opaque)->unexpected = true;
    return NBA97_BODY_ARGUMENT;
  }
  static int frame(void *opaque, std::uint32_t) {
    static_cast<NaturalFixture *>(opaque)->unexpected = true;
    return NBA97_BODY_ARGUMENT;
  }
};

void actual_match_tick_path() {
  NaturalFixture fixture;
  check(nba97_game_match_tick(&fixture.tick, &fixture.tick_progress) ==
        NBA97_BODY_ARGUMENT);
  check(!fixture.unexpected &&
        fixture.expected_index == NaturalFixture::Expected.size());
  check(fixture.player_calls == 1u && fixture.ball_calls == 1u);
  check(fixture.binding.invocations == 1u &&
        fixture.binding.completions == 1u &&
        fixture.binding.progress.completed == 1u);
  check(fixture.binding.event.pc == 0x80068e28u &&
        fixture.binding.event.entry == 0x800747b0u &&
        fixture.binding.event.count == 0u);
  check(fixture.binding.entry_machine.registers.gpr[31].word == 0x80068e30u);
  check(fixture.tick_progress.stopped_pc == 0x80068e30u &&
        fixture.tick_progress.stopped_entry == 0x8006817cu);
}

void actual_match_tick_nontrivial_failure_prefix() {
  NaturalFixture fixture;
  fixture.prepare_geometry_boundary();
  check(nba97_game_match_tick(&fixture.tick, &fixture.tick_progress) ==
        NBA97_BODY_ARGUMENT);
  check(!fixture.unexpected && fixture.expected_index == 12u);
  check(fixture.binding.invocations == 1u &&
        fixture.binding.completions == 0u &&
        fixture.binding.result == NBA97_TEXT_IO_REFUSED);
  check(fixture.child_calls == 1u && fixture.child_event.pc == 0x800749ccu &&
        fixture.child_event.delay_slot_pc == 0x800749d0u &&
        fixture.child_event.entry == 0x800706e4u);
  check(fixture.binding.progress.machine.registers.gpr[4].word == 900u &&
        fixture.binding.progress.machine.registers.gpr[5].word == 1800u &&
        fixture.binding.progress.machine.registers.gpr[31].word == 0x800749d4u);
  check(fixture.tick_progress.stopped_pc == 0x80068e28u &&
        fixture.tick_progress.stopped_entry == 0x800747b0u);
}

void actual_match_tick_complete_nontrivial_path() {
  NaturalFixture fixture;
  fixture.prepare_complete_nontrivial();
  check(nba97_game_match_tick(&fixture.tick, &fixture.tick_progress) ==
        NBA97_BODY_ARGUMENT);
  check(!fixture.unexpected &&
        fixture.expected_index == NaturalFixture::Expected.size());
  check(fixture.binding.invocations == 1u &&
        fixture.binding.completions == 1u &&
        fixture.binding.progress.completed == 1u);
  check(fixture.binding.progress.actor_iterations == 5u &&
        fixture.binding.progress.opposing_actor_iterations == 5u);
  check(fixture.child_events.size() == 23u);
  check(fixture.child_events[0].pc == 0x800749ccu &&
        fixture.child_events[0].delay_slot_pc == 0x800749d0u &&
        fixture.child_events[0].entry == 0x800706e4u);
  check(fixture.child_events[10].pc == 0x80074ae8u &&
        fixture.child_events[10].entry == 0x8007066cu);
  check(fixture.get(0x800300beu, 2u) == 100u &&
        fixture.get(0x800300bau, 2u) == 100u &&
        fixture.get(0x800fdbe0u, 2u) == 100u);
  check(fixture.tick_progress.stopped_pc == 0x80068e30u &&
        fixture.tick_progress.stopped_entry == 0x8006817cu);
}

void exact_child_whitelist_rejects_unknown_boundaries() {
  NaturalFixture fixture;
  auto machine = NaturalFixture::machine(0x800749d4u);
  Nba97GameTeamTacticsEvent event{};
  event.pc = 0x800749d0u;
  event.delay_slot_pc = 0x800749d4u;
  event.entry = 0x800706e4u;
  event.kind = NBA97_GAME_TEAM_TACTICS_CHILD_800706E4;
  event.argument_count = 3u;
  event.invocation = 1u;
  machine.registers.gpr[6] = {0x800300c0u, 0x0fu};
  const auto before = machine;
  check(NaturalFixture::child_complete(&fixture, nullptr, &event, &machine) ==
        0);
  check(fixture.child_events.empty() && same_machine(machine, before));
}
void direct_reuse_and_guards() {
  NaturalFixture fixture;
  Nba97MatchTickCall call{0x80068e28u, 0x800747b0u, {0u, 0u}, 0u};
  check(nba97_game_team_tactics_update_from_match_tick(&fixture.binding, &call,
                                                       nullptr) == 1);
  check(nba97_game_team_tactics_update_from_match_tick(&fixture.binding, &call,
                                                       nullptr) == 1);
  check(fixture.binding.invocations == 2u && fixture.binding.completions == 2u);
  const auto before = fixture.binding.entry_machine;
  auto malformed = call;
  malformed.pc += 4u;
  check(nba97_game_team_tactics_update_from_match_tick(
            &fixture.binding, &malformed, nullptr) == 0 &&
        same_machine(fixture.binding.entry_machine, before));
  malformed = call;
  malformed.entry += 4u;
  check(nba97_game_team_tactics_update_from_match_tick(
            &fixture.binding, &malformed, nullptr) == 0 &&
        same_machine(fixture.binding.entry_machine, before));
  malformed = call;
  malformed.count = 1u;
  check(nba97_game_team_tactics_update_from_match_tick(
            &fixture.binding, &malformed, nullptr) == 0 &&
        same_machine(fixture.binding.entry_machine, before));
  malformed = call;
  malformed.args[0] = 1u;
  check(nba97_game_team_tactics_update_from_match_tick(
            &fixture.binding, &malformed, nullptr) == 0 &&
        same_machine(fixture.binding.entry_machine, before));
  Nba97GamePeriodValue stale{1u, 1u};
  check(nba97_game_team_tactics_update_from_match_tick(&fixture.binding, &call,
                                                       &stale) == 0);
  fixture.binding.entry_machine_ready = 0u;
  check(nba97_game_team_tactics_update_from_match_tick(&fixture.binding, &call,
                                                       nullptr) == 0);
  fixture.binding.entry_machine_ready = 1u;
  fixture.binding.entry_machine.registers.gpr[31].known_mask = 0x0eu;
  const auto bad_ra = fixture.binding.entry_machine;
  check(nba97_game_team_tactics_update_from_match_tick(&fixture.binding, &call,
                                                       nullptr) == 0);
  check(same_machine(fixture.binding.entry_machine, bad_ra));

  NaturalFixture limited;
  limited.binding.operation_budget = 1u;
  check(nba97_game_team_tactics_update_from_match_tick(&limited.binding, &call,
                                                       nullptr) == 0);
  check(limited.binding.result == NBA97_TEXT_LIMIT &&
        limited.binding.progress.stopped_pc == 0x800747bcu);
}
} // namespace

int main() {
  actual_match_tick_path();
  actual_match_tick_nontrivial_failure_prefix();
  actual_match_tick_complete_nontrivial_path();
  exact_child_whitelist_rejects_unknown_boundaries();
  direct_reuse_and_guards();
  std::printf("team tactics integration checks: %u\n", checks);
  return 0;
}
