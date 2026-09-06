#include "game_frame_ui_service_capture.h"
#include "game_frame_ui_service_adapter.h"
#include <sstream>

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace nba97 {
namespace {
using U = std::uint32_t;

void checkAt(bool value, int line) {
  if (!value)
    throw std::runtime_error("frame UI native capture check failed at line " +
                             std::to_string(line));
}
#define check(value) checkAt((value), __LINE__)

struct Fixture {
  struct ExpectedService {
    U pc;
    U entry;
    U a0;
    unsigned count;
  };
  struct Trace {
    U pc;
    U entry;
    U address;
    unsigned kind;
  };
  static constexpr std::array<ExpectedService, 31> Expected{{
      {0x80068c24u, 0x80066f88u, 0, 0}, {0x80068c2cu, 0x80079664u, 0, 1},
      {0x80068c4cu, 0x80067468u, 0, 0}, {0x80068cecu, 0x80067550u, 0, 0},
      {0x80068cf4u, 0x800675e4u, 0, 0}, {0x80068d7cu, 0x8002de34u, 0, 0},
      {0x80068e00u, 0x80060ef8u, 0, 0}, {0x80068e08u, 0x80060fbcu, 0, 0},
      {0x80068e20u, 0x80060ef8u, 0, 0}, {0x80068e28u, 0x800747b0u, 0, 0},
      {0x80068e30u, 0x8006817cu, 0, 0}, {0x80068e38u, 0x8006830cu, 0, 0},
      {0x80068e78u, 0x80076b28u, 0, 0}, {0x80068e8cu, 0x800686b8u, 0, 0},
      {0x80068e94u, 0x80062bfcu, 0, 0}, {0x80068e9cu, 0x80066e84u, 0, 0},
      {0x80068ea4u, 0x80057b18u, 0, 0}, {0x8002dd8cu, 0x8007e26cu, 0, 1},
      {0x8002dd9cu, 0x800798b4u, 1, 1}, {0x80068fe0u, 0x80076b3cu, 0, 0},
      {0x8006902cu, 0x8008f224u, 0, 1}, {0x8006902cu, 0x8008f224u, 1, 1},
      {0x8006902cu, 0x8008f224u, 2, 1}, {0x8006902cu, 0x8008f224u, 3, 1},
      {0x8006902cu, 0x8008f224u, 4, 1}, {0x8006902cu, 0x8008f224u, 5, 1},
      {0x8006902cu, 0x8008f224u, 6, 1}, {0x8006902cu, 0x8008f224u, 7, 1},
      {0x800690b8u, 0x8006720cu, 0, 0}, {0x800690ccu, 0x800a584cu, 0, 0},
      {0x800691bcu, 0x80067930u, 0, 0},
  }};
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x200000);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{0x80000000u, bytes.data(), known.data(),
                             bytes.size()};
  Nba97GameFrameUiServiceMachine caller{};
  Nba97GameFrameUiServiceBinding binding{};
  Nba97MatchTickProgress parentProgress{};
  std::vector<Nba97MatchTickCall> services;
  std::vector<Trace> trace;
  std::size_t serviceIndex = 0;
  unsigned uiChildren = 0;
  unsigned framePumps = 0;
  U failServicePc = 0;

  Fixture() {
    for (unsigned i = 0; i < 32; ++i)
      caller.registers.gpr[i] = {0x10101010u + i, 15};
    caller.registers.gpr[0] = {0, 15};
    caller.registers.gpr[29] = {0x801ff000u, 15};
    caller.registers.gpr[31] = {0x8002ddb4u, 15};
    caller.hi = {0x12345678u, 7};
    caller.lo = {0x89abcdefu, 11};
    put(0x8001edecu, 1, 2);
    put(0x800fdb92u, 2, 2);
    put(0x800fdb8au, 1, 2);
    put(0x80021d82u, 1, 1);
    put(0x800fdb7cu, 0, 2);
    put(0x800fe8ccu, 1, 2);
    put(0x800fe8c4u, 3, 2);
    put(0x800fdc48u, 0x80130000u, 4);
    put(0x800fdbaeu, 5, 2);
    put(0x800fdb9cu, 0, 2);
    put(0x800fa038u, 0, 2);
    put(0x800eb680u, 1, 1);
    put(0x800fdb90u, 0, 2);
    put(0x800fdb68u, 0, 2);
    put(0x800fdb78u, 0, 1);
    put(0x800fdbdeu, 0, 2);
    binding.memory = {&region, 1};
    binding.explicit_caller_machine = &caller;
    binding.operation_budget = 32;
    binding.io = ui;
    binding.user = this;
  }

  std::size_t at(U address, unsigned width) const {
    if (address < 0x80000000u || std::uint64_t(address) + width > 0x80200000u)
      throw std::out_of_range("unmapped integration address");
    return address - 0x80000000u;
  }

  void put(U address, U value, unsigned width) {
    const auto offset = at(address, width);
    for (unsigned i = 0; i < width; ++i) {
      bytes[offset + i] = std::uint8_t(value >> (i * 8u));
      known[offset + i] = 1;
    }
  }

  static int access(void *opaque, U pc, U address, unsigned width,
                    unsigned kind, Nba97PlayerFrameValue *value) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.trace.push_back({pc, 0, address, 1});
    try {
      const auto offset = f.at(address, width);
      if (kind == NBA97_FRAME_READ) {
        *value = {};
        for (unsigned i = 0; i < width; ++i)
          if (f.known[offset + i]) {
            value->word |= U(f.bytes[offset + i]) << (i * 8u);
            value->known_mask |= std::uint8_t(1u << i);
          }
      } else {
        for (unsigned i = 0; i < width; ++i) {
          f.bytes[offset + i] = std::uint8_t(value->word >> (i * 8u));
          f.known[offset + i] = std::uint8_t((value->known_mask >> i) & 1u);
        }
      }
      return NBA97_BODY_OK;
    } catch (const std::out_of_range &) {
      return NBA97_BODY_BOUNDS;
    }
  }

  static int service(void *opaque, const Nba97MatchTickCall *call,
                     Nba97GamePeriodValue *value) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.services.push_back(*call);
    f.trace.push_back({call->pc, call->entry, 0, 2});
    if (f.serviceIndex >= Expected.size())
      return NBA97_BODY_BOUNDS;
    const auto expected = Expected[f.serviceIndex++];
    if (call->pc != expected.pc || call->entry != expected.entry ||
        call->args[0] != expected.a0 || call->args[1] != 0 ||
        call->count != expected.count)
      return NBA97_BODY_BOUNDS;
    if (call->pc == f.failServicePc)
      return NBA97_BODY_BOUNDS;
    /* Each accepted call is an explicit synthetic prerequisite contract for
     * this natural caller test. Only 60FBC and 8F224 have effects consumed by
     * the selected source path; the rest record typed synchronous completion.
     */
    if (call->entry == 0x80060fbcu)
      f.put(0x800fdb88u, 1, 2);
    if (value) {
      value->word = call->entry == 0x8008f224u && call->args[0] == 0 ? 1u : 0u;
      value->known = 1;
    }
    return NBA97_BODY_OK;
  }

  static int player(void *opaque, U) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.trace.push_back({0x80068d84u, 0x8006801cu, 0, 3});
    f.put(0x800fdc48u, 0x80140000u, 4);
    return NBA97_BODY_OK;
  }

  static int ball(void *opaque, U, U) {
    auto &f = *static_cast<Fixture *>(opaque);
    f.trace.push_back({0x80068d9cu, 0x8006ef60u, 0, 4});
    f.put(0x800fdc48u, 0x80140040u, 4);
    return NBA97_BODY_OK;
  }

  static int net(void *opaque, U pc) {
    auto &f = *static_cast<Fixture *>(opaque);
    check(pc == 0x8002dda4u);
    f.trace.push_back({pc, 0x8002dc88u, 0, 5});
    f.put(0x800fdb6cu, 0xffffu, 2);
    return NBA97_BODY_OK;
  }

  static int frame(void *opaque, U pc) {
    auto &f = *static_cast<Fixture *>(opaque);
    check(pc == 0x8002ddb4u);
    /* This typed synthetic completion is observable in trace/framePumps; it
     * does not claim a recovered renderer or a presented match frame. */
    f.trace.push_back({pc, 0x80049018u, 0, 6});
    ++f.framePumps;
    return NBA97_BODY_OK;
  }

  static int ui(void *opaque, const Nba97GameTextMemory *,
                const Nba97GameFrameUiServiceEvent *event,
                Nba97GameFrameUiServiceMachine *machine) {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.uiChildren;
    f.trace.push_back({event->pc, event->entry, 0, 7});
    check(event->pc == 0x80032b18u && event->entry == 0x8003287cu &&
          event->delay_slot_pc == 0x80032b1cu && event->argument_count == 0);
    machine->registers.gpr[2] = {0x76543210u, 15};
    return 1;
  }

  int run(bool withMachine = true) {
    serviceIndex = 0;
    services.clear();
    trace.clear();
    Nba97MatchTickContext context{};
    context.access = access;
    context.service = service;
    context.player_update = player;
    context.ball_simulation = ball;
    context.net_transform = net;
    context.match_frame = frame;
    context.user = this;
    context.operation_budget = 10000;
    context.incoming_s6 = {7, 1};
    binding.explicit_caller_machine = withMachine ? &caller : nullptr;
    return nba97_game_match_tick_with_frame_ui_service(&context, &binding,
                                                       &parentProgress);
  }
};

std::size_t traceIndex(const Fixture &f, unsigned kind, U pc) {
  for (std::size_t i = 0; i < f.trace.size(); ++i)
    if (f.trace[i].kind == kind && f.trace[i].pc == pc)
      return i;
  throw std::runtime_error("missing natural trace entry");
}

} // namespace
std::string captureGameFrameUiService() {
  Fixture f;
  check(f.run() == NBA97_BODY_OK && f.parentProgress.completed);
  const auto &p = f.binding.progress;
  check(p.completed && f.binding.invocations == 1 &&
        f.binding.completions == 1 && f.uiChildren == 1 && f.framePumps == 1);
  check(f.serviceIndex == Fixture::Expected.size() &&
        f.services.size() == Fixture::Expected.size());
  const std::array<std::size_t, 6> order{
      {traceIndex(f, 2, 0x8002dd8c), traceIndex(f, 1, 0x8002dd98),
       traceIndex(f, 2, 0x8002dd9c), traceIndex(f, 5, 0x8002dda4),
       traceIndex(f, 7, 0x80032b18), traceIndex(f, 6, 0x8002ddb4)}};
  for (std::size_t i = 1; i < order.size(); ++i)
    check(order[i - 1] < order[i]);
  check(p.machine.registers.gpr[29].word == 0x801ff000 &&
        p.machine.registers.gpr[31].word == 0x8002ddb4 &&
        p.machine.registers.gpr[2].word == 1);
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x80032B10\",\"inclusive_end\":"
       "\"0x80032BB7\",\"bytes\":168,\"instructions\":42,\"classification\":"
       "\"BLOCKED\",\"scope\":\"independent synthetic actual match-tick "
       "caller; explicit full caller machine; whitelisted typed prerequisite "
       "services; UI children and match-frame completion remain synthetic; no "
       "rendered match "
       "frame\",\"completed\":true,\"parent_completed\":true,\"same_parent_"
       "memory\":true,\"call_pc\":"
    << f.binding.event.pc << ",\"operations\":" << p.operations
    << ",\"instruction_count\":" << p.instruction_count
    << ",\"reads\":" << p.reads << ",\"stores\":" << p.stores
    << ",\"callbacks\":" << p.callbacks_completed
    << ",\"prerequisite_calls\":" << f.services.size()
    << ",\"synthetic_frame_completions\":" << f.framePumps
    << ",\"v0\":" << p.machine.registers.gpr[2].word
    << ",\"sp\":" << p.machine.registers.gpr[29].word
    << ",\"ra\":" << p.machine.registers.gpr[31].word
    << ",\"hilo_known_masks\":[" << unsigned(p.machine.hi.known_mask) << ','
    << unsigned(p.machine.lo.known_mask)
    << "],\"ordered_checkpoint_indices\":[";
  for (std::size_t i = 0; i < order.size(); ++i) {
    if (i)
      o << ',';
    o << order[i];
  }
  o << "],\"blocked_children\":[\"0x8003287C\",\"0x80031C5C\",\"0x8003066C\","
       "\"0x80032774\"]}";
  return o.str();
}
} // namespace nba97
