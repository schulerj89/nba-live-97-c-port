#include "game_team_tactics_update_capture.h"

#include "game_team_tactics_update_adapter.h"

#include <array>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace nba97 {
namespace {
constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t Ball = 0x80012000u;
struct Expected {
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

struct CaptureFixture {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000u, 0u);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000u, 1u);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97MatchTickContext tick{};
  Nba97MatchTickProgress tick_progress{};
  Nba97GameTeamTacticsBinding binding{};
  std::vector<Nba97GameTeamTacticsEvent> children;
  std::size_t expected_index{};
  unsigned players{};
  unsigned balls{};

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

  static constexpr std::array<Expected, 13> Services{{
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

  CaptureFixture() {
    tick.access = access;
    tick.service = service;
    tick.player_update = player;
    tick.ball_simulation = ball;
    tick.net_transform = reject_pc;
    tick.match_frame = reject_pc;
    tick.user = this;
    tick.operation_budget = 1000u;
    tick.incoming_s6 = {2u, 1u};
    binding.memory = {&region, 1u};
    binding.operation_budget = std::numeric_limits<std::size_t>::max();
    binding.entry_machine = machine();
    binding.entry_machine_ready = 1u;
    binding.io = child;
    binding.io_user = this;
    prepare_parent();
    prepare_tactics();
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
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0u;
    for (unsigned byte = 0u; byte != width; ++byte)
      value |= static_cast<std::uint32_t>(bytes[offset(address) + byte])
               << (8u * byte);
    return value;
  }
  static Nba97GameTeamTacticsMachine machine() {
    Nba97GameTeamTacticsMachine value{};
    for (unsigned index = 0u; index != NBA97_MATCH_INITIALIZE_REGISTER_COUNT;
         ++index)
      value.registers.gpr[index] = {0x61000000u + index * 0x01010101u, 0x0fu};
    value.registers.gpr[0] = {0u, 0x0fu};
    value.registers.gpr[NBA97_MATCH_INITIALIZE_SP] = {0x801ff000u, 0x0fu};
    value.registers.gpr[NBA97_MATCH_INITIALIZE_RA] = {0x80068e30u, 0x0fu};
    value.hi = {0x13579bdfu, 0x0fu};
    value.lo = {0x2468ace0u, 0x0fu};
    return value;
  }
  void prepare_parent() {
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
  }
  void prepare_tactics() {
    put(0x800fdbccu, 2u, 0u);
    put(0x800fdbd2u, 2u, 0u);
    put(0x800fdb90u, 2u, 0u);
    put(0x800fdb94u, 2u, 0u);
    put(0x800fdba4u, 4u, 360u);
    put(0x800fdbc0u, 4u, 1000u);
    put(0x800fdbc4u, 4u, 2000u);
    put(0x800fdc34u, 4u, 0x80030000u);
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
  }

  static int access(void *opaque, std::uint32_t, std::uint32_t address,
                    unsigned width, unsigned kind,
                    Nba97PlayerFrameValue *value) {
    auto &f = *static_cast<CaptureFixture *>(opaque);
    if (value == nullptr || address < Ram ||
        static_cast<std::uint64_t>(address - Ram) + width > f.bytes.size())
      return NBA97_BODY_UNKNOWN;
    const auto at = f.offset(address);
    if (kind == NBA97_FRAME_READ) {
      *value = {};
      for (unsigned byte = 0u; byte != width; ++byte) {
        value->word |= static_cast<std::uint32_t>(f.bytes[at + byte])
                       << (8u * byte);
        if (f.known[at + byte])
          value->known_mask =
              static_cast<std::uint8_t>(value->known_mask | (1u << byte));
      }
    } else {
      for (unsigned byte = 0u; byte != width; ++byte) {
        f.bytes[at + byte] =
            static_cast<std::uint8_t>(value->word >> (8u * byte));
        f.known[at + byte] =
            static_cast<std::uint8_t>((value->known_mask >> byte) & 1u);
      }
    }
    return NBA97_BODY_OK;
  }
  static int child(void *opaque, const Nba97GameTextMemory *,
                   const Nba97GameTeamTacticsEvent *event,
                   Nba97GameTeamTacticsMachine *machine) {
    auto &f = *static_cast<CaptureFixture *>(opaque);
    std::size_t invocation = 1u;
    std::uint8_t kind = 0u;
    if (event == nullptr || machine == nullptr ||
        f.children.size() >= ExpectedChildren.size())
      return 0;
    const auto &expected = ExpectedChildren[f.children.size()];
    for (std::size_t index = 0u; index != f.children.size(); ++index)
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
    f.children.push_back(*event);
    if (event->entry == 0x800706e4u) {
      f.put(machine->registers.gpr[6].word, 2u, 100u);
      machine->registers.gpr[2] = {100u << 8u, 0x0fu};
    } else if (event->entry == 0x8007066cu) {
      machine->registers.gpr[2] = {120u << 8u, 0x0fu};
    } else if (event->entry == 0x800295c0u || event->entry == 0x80072b70u) {
      machine->registers.gpr[2] = {1u, 0x0fu};
    } else
      return 0;
    return 1;
  }
  static int service(void *opaque, const Nba97MatchTickCall *call,
                     Nba97GamePeriodValue *result) {
    auto &f = *static_cast<CaptureFixture *>(opaque);
    if (call == nullptr || f.expected_index >= Services.size())
      return NBA97_BODY_ARGUMENT;
    const auto &expected = Services[f.expected_index];
    if (call->pc != expected.pc || call->entry != expected.entry ||
        call->args[0] != expected.a0 || call->args[1] != 0u ||
        call->count != expected.count)
      return NBA97_BODY_ARGUMENT;
    ++f.expected_index;
    if (call->pc == 0x80068d6cu) {
      if (result == nullptr)
        return NBA97_BODY_ARGUMENT;
      *result = {0u, 1u};
      return NBA97_BODY_OK;
    }
    if (call->pc == 0x80068e28u)
      return nba97_game_team_tactics_update_from_match_tick(&f.binding, call,
                                                            result)
                 ? NBA97_BODY_OK
                 : NBA97_BODY_ARGUMENT;
    if (call->pc == 0x80068e30u)
      return NBA97_BODY_ARGUMENT;
    return result == nullptr ? NBA97_BODY_OK : NBA97_BODY_ARGUMENT;
  }
  static int player(void *opaque, std::uint32_t pc) {
    auto &f = *static_cast<CaptureFixture *>(opaque);
    if (pc != 0x80068d84u)
      return NBA97_BODY_ARGUMENT;
    ++f.players;
    return NBA97_BODY_OK;
  }
  static int ball(void *opaque, std::uint32_t pc, std::uint32_t pointer) {
    auto &f = *static_cast<CaptureFixture *>(opaque);
    if (pc != 0x80068d9cu || pointer != Ball)
      return NBA97_BODY_ARGUMENT;
    ++f.balls;
    return NBA97_BODY_OK;
  }
  static int reject_pc(void *, std::uint32_t) { return NBA97_BODY_ARGUMENT; }
};

void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
} // namespace

std::string captureGameTeamTacticsUpdate() {
  CaptureFixture fixture;
  const auto before_timer = fixture.get(0x800fe8a4u, 2u);
  const int tick_result =
      nba97_game_match_tick(&fixture.tick, &fixture.tick_progress);
  const auto &progress = fixture.binding.progress;
  require(tick_result == NBA97_BODY_ARGUMENT,
          "subsequent gate was not refused");
  require(fixture.expected_index == CaptureFixture::Services.size(),
          "match-tick service sequence diverged");
  require(fixture.players == 1u && fixture.balls == 1u,
          "synthetic player/ball prerequisite diverged");
  require(fixture.binding.invocations == 1u &&
              fixture.binding.completions == 1u && progress.completed == 1u,
          "team tactics owner did not complete");
  require(progress.actor_iterations == 5u &&
              progress.opposing_actor_iterations == 5u,
          "five-plus-five actor scans did not complete");
  require(fixture.children.size() == 23u,
          "typed child sequence ended too early");
  require(fixture.children.front().pc == 0x800749ccu &&
              fixture.children.front().delay_slot_pc == 0x800749d0u &&
              fixture.children.front().entry == 0x800706e4u,
          "first geometry boundary diverged");
  require(fixture.tick_progress.stopped_pc == 0x80068e30u &&
              fixture.tick_progress.stopped_entry == 0x8006817cu,
          "parent did not stop at the subsequent crossing-half service");
  require(fixture.get(0x800300beu, 2u) == 100u &&
              fixture.get(0x800300bau, 2u) == 100u &&
              fixture.get(0x800fdbe0u, 2u) == 100u,
          "geometry/minimum state diverged");

  std::ostringstream json;
  json << "{\"program\":\"GAMEONLY\",\"address\":\"0x800747B0\""
       << ",\"end\":\"0x80075D3F\""
       << ",\"range\":\"0x800747B0..0x80075D3F\""
       << ",\"bytes\":5520,\"instructions\":1380"
       << ",\"actual_call_pc\":\"0x80068E28\""
       << ",\"classification\":\"no direct visual effect\""
       << ",\"completed\":true,\"same_parent_memory\":true"
       << ",\"entry_machine\":\"explicit independent full-machine snapshot; "
          "legacy tick has no CPU ABI\""
       << ",\"operations\":" << progress.operations
       << ",\"reads\":" << progress.reads << ",\"stores\":" << progress.stores
       << ",\"callbacks\":" << progress.callbacks_completed
       << ",\"actor_iterations\":" << progress.actor_iterations
       << ",\"opposing_actor_iterations\":"
       << progress.opposing_actor_iterations
       << ",\"before\":{\"defense_timer\":" << before_timer << "}"
       << ",\"after\":{\"defense_timer\":" << fixture.get(0x800fe8a4u, 2u)
       << ",\"actor0_possession_distance\":" << fixture.get(0x800300beu, 2u)
       << ",\"actor0_basket_distance\":" << fixture.get(0x800300bau, 2u)
       << ",\"opposing_minimum\":" << fixture.get(0x800fdbe0u, 2u)
       << "},\"output_sp\":\"0x" << std::hex << std::uppercase
       << progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_SP].word
       << "\",\"output_ra\":\"0x"
       << progress.machine.registers.gpr[NBA97_MATCH_INITIALIZE_RA].word
       << "\",\"child_call_sites\":[";
  for (std::size_t index = 0u; index != fixture.children.size(); ++index) {
    if (index != 0u)
      json << ',';
    json << "\"0x" << std::hex << std::uppercase << fixture.children[index].pc
         << "\"";
  }
  json << "],\"parent_stop_pc\":\"0x80068E30\""
       << ",\"parent_stop_entry\":\"0x8006817C\"}" << '\n';
  return json.str();
}
} // namespace nba97
