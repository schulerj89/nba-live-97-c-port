#include "game_actor_timers_adapter.h"
#include "game_stamina_handicap_adapter.h"

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
                 "stamina handicap integration check %u failed at line %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Ball = 0x80012000u;

bool same_word(const Nba97GameStaminaHandicapWord &left,
               const Nba97GameStaminaHandicapWord &right) {
  return left.word == right.word && left.known_mask == right.known_mask;
}
bool same_machine(const Nba97GameStaminaHandicapMachine &left,
                  const Nba97GameStaminaHandicapMachine &right) {
  for (unsigned i = 0u; i != NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
    if (!same_word(left.registers.gpr[i], right.registers.gpr[i]))
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
  Nba97GameActorTimersBinding prior{};
  Nba97GameStaminaHandicapBinding binding{};
  std::vector<Nba97MatchTickCall> services;
  std::size_t expected_index{};
  unsigned player_calls{};
  unsigned ball_calls{};
  bool unexpected{};

  static constexpr std::array<ExpectedService, 16> Expected{{
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
      {0x80068e60u, 0x80068504u, 0u, 0u},
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
    tick.operation_budget = 1000u;
    tick.incoming_s6 = {2u, 1u};
    prior.memory = {&region, 1u};
    prior.operation_budget = std::numeric_limits<std::size_t>::max();
    prior.entry_machine = machine(0x80068e40u);
    prior.entry_machine_ready = 1u;
    binding.memory = {&region, 1u};
    binding.operation_budget = std::numeric_limits<std::size_t>::max();
    binding.entry_machine = machine(0x80068e68u);
    binding.entry_machine_ready = 1u;

    put(0x8001edecu, 2u, 2u);
    put(0x800fdb92u, 2u, 2u);
    put(0x800fdb8au, 2u, 2u);
    put(0x80021d82u, 1u, 1u);
    put(0x800fdb7cu, 2u, 0u);
    put(0x800fe8ccu, 2u, 0u);
    put(0x800fe8c4u, 2u, 0u);
    put(0x800fdb68u, 2u, 5u);
    put(0x800fdb78u, 1u, 1u);
    put(0x800fdb6cu, 2u, 1u);
    put(0x800fdbaeu, 2u, 0u);
    put(0x800fdc48u, 4u, Ball);

    put(0x80021d81u, 1u, 1u);
    put(0x80021d93u, 1u, 1u);
    put(0x800fdb58u, 4u, 60u);
    put(0x800fdb74u, 2u, 1u);
    put(0x800fdb7eu, 2u, 1u);
    put(0x8001ee22u, 2u, 4u);
    put(0x8001eee6u, 2u, 1u);
    for (unsigned i = 0u; i != 24u; ++i)
      put(0x8001f80cu + i * 0x22u, 2u, 10u);
    for (unsigned i = 0u; i != 11u; ++i) {
      const auto actor = 0x80030000u + i * 0x200u;
      const auto record = 0x80050000u + i * 0x40u;
      put(0x80020becu + i * 4u, 4u, actor);
      put(actor + 4u, 2u, 0xffffu);
      put(actor + 0xe6u, 2u, 1u);
      put(actor + 0xe4u, 2u, 0u);
      put(actor + 0xb4u, 2u, 1u);
      put(actor + 0x1cu, 4u, record);
      put(actor + 0xa0u, 2u, 1u);
      put(actor + 0xddu, 1u, 0u);
      put(actor + 0x44u, 2u, 1u);
      put(record + 0x20u, 2u, 20u);
    }
  }

  static Nba97GameStaminaHandicapMachine machine(std::uint32_t ra) {
    Nba97GameStaminaHandicapMachine value{};
    for (unsigned i = 0u; i != NBA97_MATCH_INITIALIZE_REGISTER_COUNT; ++i)
      value.registers.gpr[i] = {0x61000000u + i * 0x01010101u,
                                static_cast<std::uint8_t>((i % 15u) + 1u)};
    value.registers.gpr[0] = {0u, 0x0fu};
    value.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ff000u, 0x0fu};
    value.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {ra, 0x0fu};
    value.hi = {0x13579bdfu, 0x03u};
    value.lo = {0x2468ace0u, 0x0cu};
    return value;
  }
  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }
  void put(std::uint32_t address, unsigned width, std::uint32_t value) {
    for (unsigned i = 0u; i != width; ++i) {
      bytes[offset(address) + i] = static_cast<std::uint8_t>(value >> (8u * i));
      known[offset(address) + i] = 1u;
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0u;
    for (unsigned i = 0u; i != width; ++i)
      value |= static_cast<std::uint32_t>(bytes[offset(address) + i])
               << (8u * i);
    return value;
  }

  static int access(void *opaque, std::uint32_t, std::uint32_t address,
                    unsigned width, unsigned kind,
                    Nba97PlayerFrameValue *value) {
    auto &f = *static_cast<NaturalFixture *>(opaque);
    if (value == nullptr || address < Ram ||
        static_cast<std::uint64_t>(address - Ram) + width > f.bytes.size())
      return NBA97_BODY_UNKNOWN;
    const auto at = f.offset(address);
    if (kind == NBA97_FRAME_READ) {
      *value = {};
      for (unsigned i = 0u; i != width; ++i) {
        value->word |= static_cast<std::uint32_t>(f.bytes[at + i]) << (8u * i);
        if (f.known[at + i])
          value->known_mask =
              static_cast<std::uint8_t>(value->known_mask | (1u << i));
      }
    } else {
      for (unsigned i = 0u; i != width; ++i) {
        f.bytes[at + i] = static_cast<std::uint8_t>(value->word >> (8u * i));
        f.known[at + i] =
            static_cast<std::uint8_t>((value->known_mask >> i) & 1u);
      }
    }
    return NBA97_BODY_OK;
  }

  static int service(void *opaque, const Nba97MatchTickCall *call,
                     Nba97GamePeriodValue *result) {
    auto &f = *static_cast<NaturalFixture *>(opaque);
    /* Every prerequisite, the crossing-half gate, player update, and ball
     * update is an exact synthetic fixture contract, never catch-all success.
     */
    if (call == nullptr || f.expected_index >= Expected.size()) {
      f.unexpected = true;
      return NBA97_BODY_ARGUMENT;
    }
    const auto &expected = Expected[f.expected_index];
    if (call->pc != expected.pc || call->entry != expected.entry ||
        call->args[0] != expected.a0 || call->args[1] != 0u ||
        call->count != expected.count) {
      f.unexpected = true;
      return NBA97_BODY_ARGUMENT;
    }
    ++f.expected_index;
    f.services.push_back(*call);
    if (call->pc == 0x80068d6cu) {
      if (result == nullptr)
        return NBA97_BODY_ARGUMENT;
      *result = {0u, 1u};
      return NBA97_BODY_OK;
    }
    if (call->pc == 0x80068e30u)
      return result == nullptr ? NBA97_BODY_OK : NBA97_BODY_ARGUMENT;
    if (call->pc == 0x80068e38u)
      return nba97_game_actor_timers_from_match_tick(&f.prior, call, result)
                 ? NBA97_BODY_OK
                 : NBA97_BODY_ARGUMENT;
    if (call->pc == 0x80068e60u)
      return nba97_game_stamina_handicap_from_match_tick(&f.binding, call,
                                                         result)
                 ? NBA97_BODY_OK
                 : NBA97_BODY_ARGUMENT;
    if (call->pc == 0x80068e78u)
      return NBA97_BODY_ARGUMENT;
    return result == nullptr ? NBA97_BODY_OK : NBA97_BODY_ARGUMENT;
  }
  static int player(void *opaque, std::uint32_t pc) {
    auto &f = *static_cast<NaturalFixture *>(opaque);
    if (pc != 0x80068d84u)
      return NBA97_BODY_ARGUMENT;
    ++f.player_calls;
    return NBA97_BODY_OK;
  }
  static int ball(void *opaque, std::uint32_t pc, std::uint32_t pointer) {
    auto &f = *static_cast<NaturalFixture *>(opaque);
    if (pc != 0x80068d9cu || pointer != Ball)
      return NBA97_BODY_ARGUMENT;
    ++f.ball_calls;
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

void actual_tick_countdown_path() {
  NaturalFixture f;
  check(nba97_game_match_tick(&f.tick, &f.tick_progress) ==
        NBA97_BODY_ARGUMENT);
  check(!f.unexpected && f.expected_index == NaturalFixture::Expected.size() &&
        f.services.size() == NaturalFixture::Expected.size() &&
        f.player_calls == 1u && f.ball_calls == 1u);
  for (std::size_t i = 0u; i != NaturalFixture::Expected.size(); ++i) {
    const auto &actual = f.services[i];
    const auto &expected = NaturalFixture::Expected[i];
    check(actual.pc == expected.pc && actual.entry == expected.entry &&
          actual.args[0] == expected.a0 && actual.args[1] == 0u &&
          actual.count == expected.count);
  }
  check(f.prior.invocations == 1u && f.prior.completions == 1u &&
        f.binding.invocations == 1u && f.binding.completions == 1u);
  check(f.get(0x800fdbaeu, 2u) == 59u && f.get(0x800fdb98u, 2u) == 5u &&
        f.get(0x8001f80cu, 2u) == 11u && f.get(0x80050020u, 2u) == 18u);
  check(f.tick_progress.stopped_pc == 0x80068e78u &&
        f.tick_progress.stopped_entry == 0x80076b28u);
}

void direct_reuse_guards_and_budget() {
  NaturalFixture f;
  Nba97MatchTickCall call{0x80068e60u, 0x80068504u, {0u, 0u}, 0u};
  check(nba97_game_stamina_handicap_from_match_tick(&f.binding, &call,
                                                    nullptr) == 1);
  f.binding.entry_machine = NaturalFixture::machine(0x80068e68u);
  check(nba97_game_stamina_handicap_from_match_tick(&f.binding, &call,
                                                    nullptr) == 1 &&
        f.binding.invocations == 2u && f.binding.completions == 2u);
  const auto before = f.binding.entry_machine;
  auto malformed = call;
  malformed.pc += 4u;
  check(nba97_game_stamina_handicap_from_match_tick(&f.binding, &malformed,
                                                    nullptr) == 0 &&
        same_machine(f.binding.entry_machine, before));
  malformed = call;
  malformed.entry += 4u;
  check(nba97_game_stamina_handicap_from_match_tick(&f.binding, &malformed,
                                                    nullptr) == 0 &&
        same_machine(f.binding.entry_machine, before));
  malformed = call;
  malformed.count = 1u;
  check(nba97_game_stamina_handicap_from_match_tick(&f.binding, &malformed,
                                                    nullptr) == 0 &&
        same_machine(f.binding.entry_machine, before));
  malformed = call;
  malformed.args[1] = 1u;
  check(nba97_game_stamina_handicap_from_match_tick(&f.binding, &malformed,
                                                    nullptr) == 0 &&
        same_machine(f.binding.entry_machine, before));
  Nba97GamePeriodValue stale{1u, 1u};
  check(nba97_game_stamina_handicap_from_match_tick(&f.binding, &call,
                                                    &stale) == 0 &&
        same_machine(f.binding.entry_machine, before));
  f.binding.entry_machine_ready = 0u;
  check(nba97_game_stamina_handicap_from_match_tick(&f.binding, &call,
                                                    nullptr) == 0 &&
        same_machine(f.binding.entry_machine, before));
  f.binding.entry_machine_ready = 1u;
  f.binding.entry_machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {
      0x80068e68u, 0x0eu};
  const auto bad_ra = f.binding.entry_machine;
  check(nba97_game_stamina_handicap_from_match_tick(&f.binding, &call,
                                                    nullptr) == 0 &&
        f.binding.invocations == 2u &&
        same_machine(f.binding.entry_machine, bad_ra));

  NaturalFixture limited;
  limited.binding.operation_budget = 1u;
  check(nba97_game_stamina_handicap_from_match_tick(&limited.binding, &call,
                                                    nullptr) == 0 &&
        limited.binding.result == NBA97_TEXT_LIMIT &&
        limited.binding.progress.stopped_pc == 0x8006851cu &&
        limited.binding.invocations == 1u && limited.binding.completions == 0u);
}
} // namespace

int main() {
  actual_tick_countdown_path();
  direct_reuse_guards_and_budget();
  std::printf("%u stamina handicap integration checks passed\n", checks);
  return 0;
}
