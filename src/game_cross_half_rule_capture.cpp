#include "game_cross_half_rule_capture.h"
#include "game_cross_half_rule_adapter.h"
#include "game_actor_timers_adapter.h"
#include <sstream>
#include <stdexcept>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace nba97 {
namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr,
                 "cross-half rule integration check %u failed at line %u\n",
                 checks, line);
    throw std::runtime_error("cross-half rule native capture failed");
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Actor1 = 0x80010000u;
constexpr std::uint32_t Actor2 = 0x80011000u;
constexpr std::uint32_t Ball = 0x80012000u;

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
  Nba97GameCrossHalfRuleBinding binding{};
  Nba97GameActorTimersBinding timers{};
  std::vector<Nba97MatchTickCall> services;
  std::vector<Nba97GameCrossHalfRuleEvent> children;
  std::vector<Nba97GameCrossHalfRuleMachine> child_machines;
  std::size_t expected_index{};
  unsigned player_calls{};
  unsigned ball_calls{};
  bool unexpected{};
  bool refuse_gate{};

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
    tick.operation_budget = 500u;
    tick.incoming_s6 = {2u, 1u};

    binding.memory = {&region, 1u};
    binding.operation_budget = 100u;
    binding.entry_machine = entry_machine();
    binding.entry_machine_ready = 1u;
    binding.io = child;
    binding.user = this;

    put(0x8001edecu, 1u, 2u);
    put(0x800fdb92u, 2u, 2u);
    put(0x800fdb8au, 1u, 2u);
    put(0x80021d82u, 1u, 1u);
    put(0x800fdb7cu, 0u, 2u);
    put(0x800fe8ccu, 0u, 2u);
    put(0x800fe8c4u, 0u, 2u);
    put(0x800fdb68u, 5u, 2u);
    put(0x800fdb78u, 1u, 1u);
    put(0x800fdb6cu, 1u, 2u);
    put(0x800fdc48u, Ball, 4u);

    put(0x800fdb90u, 100u, 2u);
    put(0x800fdbccu, 0u, 2u);
    put(0x800fdc38u, Actor1, 4u);
    put(0x800fdc34u, Actor2, 4u);
    put(Actor1 + 0x10u, 0x80000000u, 4u);
    put(Actor2 + 8u, 0u, 4u);
    put(Actor2 + 0x10u, 0u, 4u);
    put(0x80021d8bu, 1u, 1u);
    put(0x800fe8e0u, 1u, 2u);
    put(0x800fdbacu, 12u, 2u);
    put(0x800fdb94u, 0u, 2u);
    put(0x800fe882u, 0u, 2u);
    timers.memory = {&region, 1};
    timers.operation_budget = 1000;
    put(0x800fdbaeu, 1, 2);
    put(0x800fdb58u, 3600, 4);
    put(0x800fdb74u, 59, 2);
    for (unsigned i = 0; i < 11; ++i) {
      const auto actor = 0x80030000u + i * 0x200u;
      put(0x80020becu + i * 4u, actor, 4);
      put(actor + 0xe6u, 5, 2);
      put(actor + 0xe4u, 1, 2);
      put(actor + 0xb4u, 2, 2);
      put(actor + 4u, 0, 2);
      put(actor + 0xd8u, 0xa5, 1);
      put(actor + 0xf2u, 0xbeef, 2);
    }
    for (unsigned i = 0; i < 10; ++i)
      put(0x800fdc70u + i * 4u, 0x80040000u + i * 0x40u, 4);
    put(0x800fdc50u, 0x80050000u, 4);
    put(0x8005001eu, 5, 2);
  }

  static Nba97GameCrossHalfRuleMachine entry_machine() {
    Nba97GameCrossHalfRuleMachine machine{};
    for (unsigned index = 0u; index != NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
         ++index)
      machine.registers.gpr[index] = {
          0x51000000u + index * 0x01010101u,
          static_cast<std::uint8_t>((index % 15u) + 1u)};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_ZERO] = {0u, 0x0f};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x800ff000u, 0x0f};
    machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80068e38u, 0x0f};
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
    const auto at = offset(address);
    std::uint32_t value = 0u;
    for (unsigned byte = 0u; byte != width; ++byte)
      value |= std::uint32_t(bytes[at + byte]) << (8u * byte);
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
      value->word = 0u;
      value->known_mask = 0u;
      value->is_reference = 0u;
      value->reference = {};
      for (unsigned byte = 0u; byte != width; ++byte) {
        value->word |= std::uint32_t(fixture.bytes[at + byte]) << (8u * byte);
        if (fixture.known[at + byte] != 0u)
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
    /* Every whitelisted completion is a named synthetic fixture contract,
     * never a catch-all success or evidence of an advancing native match. */
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
    if (call->pc == 0x80068e30u) {
      if (result != nullptr) {
        fixture.unexpected = true;
        return NBA97_BODY_ARGUMENT;
      }
      return nba97_game_cross_half_rule_from_match_tick(&fixture.binding, call,
                                                        result);
    }
    if (call->pc == 0x80068e38u) {
      /* CG returns directly to the next JAL at 68E38. Its NOP delay changes
       * no state, so only the JAL-produced RA is projected into the full
       * machine returned by that actual owner on this retained memory. */
      if (!fixture.binding.progress.completed || result != nullptr)
        return NBA97_BODY_ARGUMENT;
      fixture.timers.entry_machine = fixture.binding.progress.machine;
      fixture.timers.entry_machine.registers.gpr[31] = {0x80068e40u, 15};
      fixture.timers.entry_machine_ready = 1;
      return nba97_game_actor_timers_from_match_tick(&fixture.timers, call,
                                                    result)
                 ? NBA97_BODY_OK : NBA97_BODY_ARGUMENT;
    }
    if (call->pc == 0x80068e78u)
      return NBA97_BODY_ARGUMENT;
    if (result != nullptr) {
      fixture.unexpected = true;
      return NBA97_BODY_ARGUMENT;
    }
    return NBA97_BODY_OK;
  }

  static int child(void *opaque, const Nba97GameTextMemory *,
                   const Nba97GameCrossHalfRuleEvent *event,
                   Nba97GameCrossHalfRuleMachine *machine) {
    auto &fixture = *static_cast<NaturalFixture *>(opaque);
    if (event == nullptr || machine == nullptr)
      return 0;
    fixture.children.push_back(*event);
    fixture.child_machines.push_back(*machine);
    switch (event->pc) {
    case 0x80068290u:
      if (event->entry != 0x80062d84u || event->argument_count != 0u ||
          event->delay_slot_pc != 0x80068294u)
        return 0;
      if (fixture.refuse_gate)
        return 0;
      machine->registers.gpr[NBA97_MATCH_INITIALIZE_V0] = {0u, 0x0f};
      return 1;
    case 0x800682b4u:
      return event->entry == 0x80029590u && event->argument_count == 1u &&
             machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 11u;
    case 0x800682d8u:
      return event->entry == 0x80062300u && event->argument_count == 1u &&
             machine->registers.gpr[NBA97_MATCH_INITIALIZE_A0].word == 5u;
    case 0x800682e0u:
      return event->entry == 0x80062660u && event->argument_count == 0u;
    default:
      return 0;
    }
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

} // namespace
std::string captureGameCrossHalfRule() {
  NaturalFixture f;
  check(nba97_game_match_tick(&f.tick, &f.tick_progress) ==
        NBA97_BODY_ARGUMENT);
  const auto &p = f.binding.progress;
  check(!f.unexpected && f.expected_index == 15 && f.player_calls == 1 &&
        f.ball_calls == 1 && p.completed && f.binding.completions == 1 &&
        f.binding.rule_delay_invocations == 1 &&
        f.binding.rule_delay_progress.completed);
  check(f.tick_progress.stopped_pc == 0x80068e78 &&
        f.tick_progress.stopped_entry == 0x80076b28 &&
        f.get(0x800fdbac, 2) == 13 && f.get(0x800fe8e0, 2) == 0 &&
        f.get(0x800fe882, 2) == 8 && f.children.size() == 4);
  const auto &t = f.timers.progress;
  check(t.completed && f.timers.completions == 1 &&
        t.entity_iterations == 11 && t.team_counter_updates == 10 &&
        t.participation_updates == 1 && f.get(0x800300e6u, 2) == 4 &&
        f.get(0x800300e4u, 2) == 0 && f.get(0x800300b4u, 2) == 1 &&
        f.get(0x800300ddu, 1) == 1 && f.get(0x800fdb74u, 2) == 60 &&
        f.get(0x8005001eu, 2) == 6);
  const std::uint32_t pcs[] = {0x80068290, 0x800682b4, 0x800682d8, 0x800682e0};
  for (unsigned i = 0; i < 4; ++i)
    check(f.children[i].pc == pcs[i] &&
          f.children[i].delay_slot_pc == pcs[i] + 4 &&
          f.child_machines[i].registers.gpr[31].word == pcs[i] + 8);
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x8006817C\",\"inclusive_end\":"
       "\"0x8006830B\",\"bytes\":400,\"instructions\":100,\"classification\":"
       "\"no direct visual effect\",\"scope\":\"independent synthetic actual "
       "match-tick caller with explicit full-machine snapshot; whitelisted "
       "typed prerequisites; actual duration no-op and actor timers; stops at next typed "
       "recorder service; no advancing "
       "match\",\"completed\":true,\"parent_completed\":false,\"same_parent_"
       "memory\":true,\"call_pc\":"
    << f.binding.event.pc << ",\"operations\":" << p.operations
    << ",\"reads\":" << p.reads << ",\"stores\":" << p.stores
    << ",\"callbacks\":" << p.callbacks_completed
    << ",\"prerequisite_events\":" << f.services.size()
    << ",\"duration_noop_calls\":" << f.binding.rule_delay_invocations
    << ",\"timer_before\":12,\"timer_after\":" << f.get(0x800fdbac, 2)
    << ",\"blocker_before\":1,\"blocker_after\":" << f.get(0x800fe8e0, 2)
    << ",\"rule_before\":0,\"rule_after\":" << f.get(0x800fe882, 2)
    << ",\"sp\":" << p.machine.registers.gpr[29].word
    << ",\"ra\":" << p.machine.registers.gpr[31].word
    << ",\"hilo_known_masks\":[" << unsigned(p.machine.hi.known_mask) << ','
    << unsigned(p.machine.lo.known_mask)
    << "],\"parent_stop_pc\":" << f.tick_progress.stopped_pc
    << ",\"parent_stop_entry\":" << f.tick_progress.stopped_entry
    << ",\"typed_child_pcs\":[";
  for (unsigned i = 0; i < 4; ++i) {
    if (i)
      o << ',';
    o << f.children[i].pc;
  }
  o << "],\"actor_timers\":{\"program\":\"GAMEONLY\",\"address\":\"0x8006830C\","
       "\"inclusive_end\":\"0x80068503\",\"bytes\":504,\"instructions\":126,"
       "\"classification\":\"no direct visual effect\",\"completed\":true,"
       "\"same_parent_memory\":true,\"machine_from_crossing_rule\":true,"
       "\"call_pc\":" << f.timers.event.pc << ",\"operations\":" << t.operations
    << ",\"reads\":" << t.reads << ",\"stores\":" << t.stores
    << ",\"actor_count\":" << t.entity_iterations
    << ",\"team_updates\":" << t.team_counter_updates
    << ",\"participation_updates\":" << t.participation_updates
    << ",\"multiply_count\":" << t.multiply_count
    << ",\"timers_before\":[5,1,2],\"timers_after\":[4,0,1],"
       "\"cache_before\":59,\"cache_after\":60,\"participation_before\":5,\"participation_after\":6"
    << ",\"sp\":" << t.machine.registers.gpr[29].word
    << ",\"ra\":" << t.machine.registers.gpr[31].word << "}}";
  return o.str();
}
} // namespace nba97
