#include "game_actor_input_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks;
void check_at(bool value, unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr, "actor input integration check %u failed at %u\n",
                 checks, line);
    std::exit(1);
  }
}
#define check(value) check_at((value), __LINE__)

constexpr std::uint32_t Ram = 0x80000000u;

struct Composition {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x110000u, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x110000u, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97MatchTickContext tick{};
  Nba97MatchTickProgress tick_progress{};
  Nba97GameActorInputBinding actor_input{};
  std::vector<Nba97MatchTickCall> tick_calls;
  std::vector<Nba97GameActorInputEvent> actor_calls;
  Nba97MatchTickCall natural_call{};
  unsigned player_calls{};
  unsigned ball_calls{};
  unsigned frame_calls{};

  Composition() {
    tick.access = access;
    tick.service = service;
    tick.player_update = player;
    tick.ball_simulation = ball;
    tick.net_transform = net;
    tick.match_frame = frame;
    tick.user = this;
    tick.operation_budget = 500;
    tick.incoming_s6 = {0xfffffffeu, 1};
    actor_input.memory = {&region, 1};
    actor_input.operation_budget = 10000;
    actor_input.io = actor;
    actor_input.user = this;

    put(0x8001edecu, 1, 2);
    put(0x800fdb92u, 2, 2);
    put(0x800fdb8au, 1, 2);
    put(0x80021d82u, 1, 1);
    put(0x800fdb7cu, 0, 2);
    put(0x800fe8ccu, 0, 2);
    put(0x800fe8c4u, 0, 2);
    put(0x800fdb68u, 5, 2);
    put(0x800fdb78u, 0, 1);
    put(0x800fdb6cu, 7, 2);
    put(0x800fdc48u, 0x80022000u, 4);
    put(0x800f9ffeu, 0xbeefu, 2);
    put(0x800fdb90u, 0xff80u, 2);
    put(0x800275c4u, 0x80068a4cu, 4);
    for (unsigned i = 0; i < 10; ++i) {
      const auto actor_address = 0x80030000u + i * 0x1000u;
      put(0x80020becu + i * 4u, actor_address, 4);
      put(actor_address + 4u, 0xffffu, 2);
      put(actor_address + 0x1au, 0, 1);
      put(actor_address + 0x46u, 0x2bu, 2);
    }
  }

  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }
  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    auto at = offset(address);
    for (unsigned i = 0; i < width; ++i) {
      bytes[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
      known[at + i] = 1;
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    auto at = offset(address);
    std::uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[at + i]) << (8u * i);
    return value;
  }
  void prepareEntry(const Nba97MatchTickCall &call) {
    for (unsigned i = 0; i < 32; ++i)
      actor_input.entry_machine.registers.gpr[i] = {
          0x41000000u + i * 0x00010101u,
          static_cast<std::uint8_t>((i % 15u) + 1u)};
    actor_input.entry_machine.registers.gpr[0] = {0, 0x0f};
    actor_input.entry_machine.registers.gpr[29] = {0x800ff000u, 0x0f};
    actor_input.entry_machine.registers.gpr[31] = {call.pc + 8u, 0x0f};
    actor_input.entry_machine.hi = {0x12345678u, 3};
    actor_input.entry_machine.lo = {0x9abcdef0u, 0x0c};
    actor_input.entry_machine_ready = 1;
  }
  static int access(void *opaque, std::uint32_t, std::uint32_t address,
                    unsigned width, unsigned kind,
                    Nba97PlayerFrameValue *value) {
    auto &c = *static_cast<Composition *>(opaque);
    if (address < Ram ||
        std::uint64_t(address) + width > std::uint64_t(Ram) + c.bytes.size())
      return NBA97_BODY_BOUNDS;
    auto at = c.offset(address);
    if (kind == NBA97_FRAME_READ) {
      *value = {};
      for (unsigned i = 0; i < width; ++i)
        if (c.known[at + i]) {
          value->word |= std::uint32_t(c.bytes[at + i]) << (8u * i);
          value->known_mask =
              static_cast<std::uint8_t>(value->known_mask | (1u << i));
        }
    } else {
      for (unsigned i = 0; i < width; ++i) {
        c.bytes[at + i] = static_cast<std::uint8_t>(value->word >> (8u * i));
        c.known[at + i] =
            static_cast<std::uint8_t>((value->known_mask >> i) & 1u);
      }
    }
    return NBA97_BODY_OK;
  }
  static int actor(void *opaque, const Nba97GameTextMemory *,
                   const Nba97GameActorInputEvent *event,
                   Nba97GameActorInputMachine *) {
    static_cast<Composition *>(opaque)->actor_calls.push_back(*event);
    return 1;
  }
  static int service(void *opaque, const Nba97MatchTickCall *call,
                     Nba97GamePeriodValue *result) {
    auto &c = *static_cast<Composition *>(opaque);
    c.tick_calls.push_back(*call);
    if (call->entry == 0x800686b8u) {
      c.natural_call = *call;
      c.prepareEntry(*call);
      return nba97_game_actor_input_from_match_tick(&c.actor_input, call,
                                                    result);
    }
    if (result)
      *result = {0, 1};
    return NBA97_BODY_OK;
  }
  static int player(void *opaque, std::uint32_t) {
    ++static_cast<Composition *>(opaque)->player_calls;
    return NBA97_BODY_OK;
  }
  static int ball(void *opaque, std::uint32_t, std::uint32_t pointer) {
    auto &c = *static_cast<Composition *>(opaque);
    ++c.ball_calls;
    return pointer == 0x80022000u ? NBA97_BODY_OK : NBA97_BODY_ARGUMENT;
  }
  static int net(void *, std::uint32_t) { return NBA97_BODY_OK; }
  static int frame(void *opaque, std::uint32_t) {
    auto &c = *static_cast<Composition *>(opaque);
    ++c.frame_calls;
    c.put(0x800fdb78u, 1, 1);
    return NBA97_BODY_OK;
  }
  int run() { return nba97_game_match_tick(&tick, &tick_progress); }
};

void actual_natural_tick_call() {
  Composition c;
  check(c.run() == NBA97_BODY_OK && c.tick_progress.completed);
  check(c.natural_call.pc == 0x80068e8cu &&
        c.natural_call.entry == 0x800686b8u && c.natural_call.count == 0 &&
        c.natural_call.args[0] == 0 && c.natural_call.args[1] == 0);
  check(c.actor_input.invocations == 1 &&
        c.actor_input.result == NBA97_TEXT_COMPLETE &&
        c.actor_input.progress.completed && c.actor_calls.size() == 10);
  check(c.actor_calls.front().pc == 0x80068a4cu &&
        c.actor_calls.front().entry == 0x800670a8u &&
        c.actor_calls.front().argument_count == 0);
  check(c.actor_input.progress.restored_return_address.word == 0x80068e94u &&
        c.actor_input.progress.machine.registers.gpr[29].word == 0x800ff000u &&
        c.actor_input.progress.machine.hi.word == 0x12345678u &&
        c.actor_input.progress.machine.lo.known_mask == 0x0c);
  check(c.get(0x800fdb8au, 2) == 0 && c.player_calls == 1 &&
        c.ball_calls == 1 && c.frame_calls == 1);
}

void explicit_machine_guards_and_failure_prefix() {
  Composition refused;
  refused.actor_input.io = nullptr;
  check(refused.run() == NBA97_BODY_ARGUMENT &&
        refused.tick_progress.stopped_pc == 0x80068e8cu &&
        refused.tick_progress.stopped_entry == 0x800686b8u &&
        refused.actor_input.invocations == 1 &&
        refused.actor_input.result == NBA97_TEXT_IO_REFUSED);

  Composition guards;
  Nba97MatchTickCall call{0x80068e8cu, 0x800686b8u, {0, 0}, 0};
  guards.prepareEntry(call);
  auto before = guards.actor_input.entry_machine;
  call.pc ^= 4u;
  check(!nba97_game_actor_input_from_match_tick(&guards.actor_input, &call,
                                                nullptr));
  call.pc ^= 4u;
  call.entry ^= 4u;
  check(!nba97_game_actor_input_from_match_tick(&guards.actor_input, &call,
                                                nullptr));
  call.entry ^= 4u;
  call.count = 1;
  check(!nba97_game_actor_input_from_match_tick(&guards.actor_input, &call,
                                                nullptr));
  call.count = 0;
  call.args[0] = 1;
  check(!nba97_game_actor_input_from_match_tick(&guards.actor_input, &call,
                                                nullptr));
  call.args[0] = 0;
  Nba97GamePeriodValue result{};
  check(!nba97_game_actor_input_from_match_tick(&guards.actor_input, &call,
                                                &result));
  guards.actor_input.entry_machine = before;
  guards.actor_input.entry_machine_ready = 0;
  check(!nba97_game_actor_input_from_match_tick(&guards.actor_input, &call,
                                                nullptr));
  guards.actor_input.entry_machine_ready = 1;
  guards.actor_input.entry_machine.registers.gpr[31].known_mask = 7;
  check(!nba97_game_actor_input_from_match_tick(&guards.actor_input, &call,
                                                nullptr));
  guards.actor_input.entry_machine = before;
  guards.actor_input.entry_machine.registers.gpr[31].word ^= 4u;
  check(!nba97_game_actor_input_from_match_tick(&guards.actor_input, &call,
                                                nullptr));
  check(guards.actor_input.invocations == 0);
  check(!nba97_game_actor_input_from_match_tick(nullptr, &call, nullptr));
}
} // namespace

int main() {
  actual_natural_tick_call();
  explicit_machine_guards_and_failure_prefix();
  std::printf("game actor input integration: %u checks\n", checks);
}
