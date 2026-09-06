#include "game_actor_timers_adapter.h"

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
                 "actor timers integration check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Ball = 0x80012000u;

bool same_word(const Nba97GameActorTimersWord &left,
               const Nba97GameActorTimersWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}

bool same_machine(const Nba97GameActorTimersMachine &left,
                  const Nba97GameActorTimersMachine &right) {
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

struct NaturalFixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000u, 0u);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x110000u, 1u);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97MatchTickContext tick{};
  Nba97MatchTickProgress tick_progress{};
  Nba97GameActorTimersBinding binding{};
  std::vector<Nba97MatchTickCall> services;
  std::size_t expected_index{};
  unsigned player_calls{};
  unsigned ball_calls{};
  bool unexpected{};

  static constexpr std::array<ExpectedService, 15> Expected{{
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
      {0x80068e38u, 0x8006830cu, 0u, 0u},
      {0x80068e78u, 0x80076b28u, 0u, 0u},
  }};

  NaturalFixture() {
    tick.access = access;
    tick.service = service;
    tick.player_update = player;
    tick.ball_simulation = ball;
    tick.net_transform = net;
    tick.match_frame = frame;
    tick.user = this;
    tick.operation_budget = 800u;
    tick.incoming_s6 = {2u, 1u};

    binding.memory = {&region, 1u};
    binding.operation_budget = std::numeric_limits<std::size_t>::max();
    binding.entry_machine = entry_machine();
    binding.entry_machine_ready = 1u;

    put(0x8001edecu, 2u, 2u);
    put(0x800fdb92u, 2u, 2u);
    put(0x800fdb8au, 2u, 2u);
    put(0x80021d82u, 1u, 1u);
    put(0x800fdb7cu, 0u, 2u);
    put(0x800fe8ccu, 0u, 2u);
    put(0x800fe8c4u, 0u, 2u);
    put(0x800fdb68u, 5u, 2u);
    put(0x800fdb78u, 1u, 1u);
    put(0x800fdb6cu, 1u, 2u);
    put(0x800fdbaeu, 1u, 2u);
    put(0x800fdc48u, Ball, 4u);

    put(0x800fdb58u, 60u, 4u);
    put(0x800fdb74u, 0u, 2u);
    for (unsigned index = 0u; index != 11u; ++index) {
      const auto actor = 0x80030000u + index * 0x200u;
      put(0x80020becu + index * 4u, actor, 4u);
      put(actor + 0xe6u, 1u, 2u);
      put(actor + 0xe4u, 0u, 2u);
      put(actor + 0xb4u, 1u, 2u);
      put(actor + 4u, 0xffffu, 2u);
    }
    for (unsigned index = 0u; index != 10u; ++index) {
      const auto team = 0x80040000u + index * 0x40u;
      put(0x800fdc70u + index * 4u, team, 4u);
    }
  }

  static Nba97GameActorTimersMachine entry_machine() {
    Nba97GameActorTimersMachine machine{};
    for (unsigned index = 0u; index != NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
         ++index)
      machine.registers.gpr[index] = {
          0x42000000u + index * 0x01010101u,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0u, 0x0f};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ff000u, 0x0f};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80068e40u, 0x0f};
    machine.hi = {0x13579bdfu, 0x03};
    machine.lo = {0x2468ace0u, 0x0c};
    return machine;
  }

  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }
  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    const auto at = offset(address);
    for (unsigned byte = 0u; byte != width; ++byte) {
      bytes[at + byte] = static_cast<std::uint8_t>(value >> (8u * byte));
      known[at + byte] = 1u;
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0u;
    const auto at = offset(address);
    for (unsigned byte = 0u; byte != width; ++byte)
      value |= static_cast<std::uint32_t>(bytes[at + byte]) << (8u * byte);
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
        if (fixture.known[at + byte] != 0u)
          value->known_mask = static_cast<std::uint8_t>(
              value->known_mask | static_cast<std::uint8_t>(1u << byte));
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
    /* These exact prerequisite completions, the preceding crossing-half rule,
     * player update, and ball update are synthetic typed fixture contracts.
     * They do not claim an advancing native game. */
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
      if (result == nullptr) {
        fixture.unexpected = true;
        return NBA97_BODY_ARGUMENT;
      }
      *result = {0u, 1u};
      return NBA97_BODY_OK;
    }
    if (call->pc == 0x80068e30u)
      return result == nullptr ? NBA97_BODY_OK : NBA97_BODY_ARGUMENT;
    if (call->pc == 0x80068e38u)
      return nba97_game_actor_timers_from_match_tick(&fixture.binding, call,
                                                     result)
                 ? NBA97_BODY_OK
                 : NBA97_BODY_ARGUMENT;
    if (call->pc == 0x80068e78u)
      return NBA97_BODY_ARGUMENT;
    if (result != nullptr) {
      fixture.unexpected = true;
      return NBA97_BODY_ARGUMENT;
    }
    return NBA97_BODY_OK;
  }

  static int player(void *opaque, std::uint32_t pc) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    if (pc != 0x80068d84u) {
      fixture.unexpected = true;
      return NBA97_BODY_ARGUMENT;
    }
    ++fixture.player_calls;
    return NBA97_BODY_OK;
  }
  static int ball(void *opaque, std::uint32_t pc, std::uint32_t pointer) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    if (pc != 0x80068d9cu || pointer != Ball) {
      fixture.unexpected = true;
      return NBA97_BODY_ARGUMENT;
    }
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
  check(!fixture.unexpected && fixture.expected_index == 15u &&
        fixture.services.size() == 15u && fixture.player_calls == 1u &&
        fixture.ball_calls == 1u);
  for (std::size_t index = 0; index != NaturalFixture::Expected.size();
       ++index) {
    const auto &actual = fixture.services[index];
    const auto &expected = NaturalFixture::Expected[index];
    check(actual.pc == expected.pc && actual.entry == expected.entry &&
          actual.args[0] == expected.a0 && actual.args[1] == 0u &&
          actual.count == expected.count);
  }
  check(fixture.binding.invocations == 1u &&
        fixture.binding.completions == 1u &&
        fixture.binding.result == NBA97_TEXT_COMPLETE &&
        fixture.binding.progress.completed);
  check(fixture.binding.event.pc == 0x80068e38u &&
        fixture.binding.event.entry == 0x8006830cu &&
        fixture.binding.event.count == 0u);
  check(fixture.get(0x800300e6u, 2u) == 0u &&
        fixture.get(0x800300b4u, 2u) == 0u &&
        fixture.get(0x800fdb74u, 2u) == 1u);
  check(fixture.tick_progress.stopped_pc == 0x80068e78u &&
        fixture.tick_progress.stopped_entry == 0x80076b28u);
}

void binding_reuse_and_guards() {
  NaturalFixture fixture;
  Nba97MatchTickCall call{0x80068e38u, 0x8006830cu, {0u, 0u}, 0u};
  check(nba97_game_actor_timers_from_match_tick(&fixture.binding, &call,
                                                nullptr) == 1);
  check(fixture.binding.invocations == 1u && fixture.binding.completions == 1u);
  fixture.binding.entry_machine = NaturalFixture::entry_machine();
  check(nba97_game_actor_timers_from_match_tick(&fixture.binding, &call,
                                                nullptr) == 1);
  check(fixture.binding.invocations == 2u && fixture.binding.completions == 2u);

  const auto before = fixture.binding.entry_machine;
  auto malformed = call;
  malformed.pc += 4u;
  check(nba97_game_actor_timers_from_match_tick(&fixture.binding, &malformed,
                                                nullptr) == 0 &&
        same_machine(fixture.binding.entry_machine, before));
  malformed = call;
  malformed.entry += 4u;
  check(nba97_game_actor_timers_from_match_tick(&fixture.binding, &malformed,
                                                nullptr) == 0 &&
        same_machine(fixture.binding.entry_machine, before));
  malformed = call;
  malformed.count = 1u;
  check(nba97_game_actor_timers_from_match_tick(&fixture.binding, &malformed,
                                                nullptr) == 0 &&
        same_machine(fixture.binding.entry_machine, before));
  malformed = call;
  malformed.args[0] = 1u;
  check(nba97_game_actor_timers_from_match_tick(&fixture.binding, &malformed,
                                                nullptr) == 0 &&
        same_machine(fixture.binding.entry_machine, before));
  Nba97GamePeriodValue stale{1u, 1u};
  check(nba97_game_actor_timers_from_match_tick(&fixture.binding, &call,
                                                &stale) == 0 &&
        same_machine(fixture.binding.entry_machine, before));
  fixture.binding.entry_machine_ready = 0u;
  check(nba97_game_actor_timers_from_match_tick(&fixture.binding, &call,
                                                nullptr) == 0 &&
        same_machine(fixture.binding.entry_machine, before));
  fixture.binding.entry_machine_ready = 1u;
  fixture.binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
      0x80068e40u, 0x0eu};
  const auto bad_ra = fixture.binding.entry_machine;
  check(nba97_game_actor_timers_from_match_tick(&fixture.binding, &call,
                                                nullptr) == 0 &&
        same_machine(fixture.binding.entry_machine, bad_ra));
  check(fixture.binding.invocations == 2u);
  check(nba97_game_actor_timers_from_match_tick(nullptr, &call, nullptr) == 0 &&
        nba97_game_actor_timers_from_match_tick(&fixture.binding, nullptr,
                                                nullptr) == 0);
}
} // namespace

int main() {
  actual_match_tick_path();
  binding_reuse_and_guards();
  std::printf("%u actor timers integration checks passed\n", checks);
  return 0;
}
