#include "game_actor_collision_response_adapter.h"
#include "game_actor_collision_response_capture.h"
#include <sstream>
#include <stdexcept>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
constexpr std::uint32_t Ram = 0x80000000u;
constexpr std::uint32_t First = 0x80010000u;
constexpr std::uint32_t Second = 0x80010200u;
constexpr std::uint32_t FirstDescriptor = 0x80010400u;
constexpr std::uint32_t SecondDescriptor = 0x80010500u;

struct Composition {
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x100000u, 0);
  std::vector<std::uint8_t> known = std::vector<std::uint8_t>(0x100000u, 1);
  Nba97GameTextRegion region{Ram, bytes.data(), known.data(), bytes.size()};
  Nba97GameOpponentContactContext parent{};
  Nba97GameOpponentContactProgress parent_progress{};
  Nba97GameActorCollisionResponseBinding response{};
  Nba97GameActorCollisionResponseGeometryBinding geometry{};
  Nba97GameActorCollisionResponseEvent resolver_event{};
  Nba97GameActorCollisionResponseMachine resolver_machine{};
  unsigned resolver_calls{};
  bool refuse_resolver{};

  Composition() {
    for (unsigned i = 0; i < 32; ++i)
      parent.machine.registers.gpr[i] = {
          0x24000000u + i, static_cast<std::uint8_t>((i % 15u) + 1u)};
    parent.machine.registers.gpr[0] = {0, 15};
    parent.machine.registers.gpr[4] = {First, 15};
    parent.machine.registers.gpr[5] = {Second, 15};
    parent.machine.registers.gpr[29] = {0x800ff000u, 15};
    parent.machine.registers.gpr[31] = {0x81234568u, 15};
    parent.machine.hi = {0x11112222u, 7};
    parent.machine.lo = {0x33334444u, 11};
    parent.memory = {&region, 1};
    parent.operation_budget = 1000;
    parent.io = nba97_game_actor_collision_response_from_opponent_contact;
    parent.user = &response;
    put(First, 0x12345678u, 4);
    put(Second, 0xabcdef09u, 4);
    put(First + 0xc2u, 0, 2);
    put(Second + 0xc2u, 0, 2);
    put(First + 0xdau, 0, 1);
    put(Second + 0xdau, 0, 1);
    put(0x80021d8au, 0, 1);
    put(0x800fdb90u, 0, 2);
    put(0x800fdbccu, 0, 2);
    put(First + 8u, 0x1000, 4);
    put(Second + 8u, 0x0f00, 4);
    put(First + 0x0cu, 0x2000, 4);
    put(Second + 0x0cu, 0x2000, 4);
    put(First + 0x14u, 0, 2);
    put(Second + 0x14u, 64, 2);
    put(First + 0x16u, 0, 2);
    put(Second + 0x16u, 0, 2);
    put(First + 0x20u, FirstDescriptor, 4);
    put(Second + 0x20u, SecondDescriptor, 4);
    put(FirstDescriptor + 0x0au, 0, 1);
    put(SecondDescriptor + 0x0au, 0, 1);
    put(0x800b8324u, 1, 2);
    put(First + 0xdau, 1, 1);
    put(First + 0xe6u, 14, 2);
    put(Second + 0xe6u, 14, 2);
    nba97_game_actor_collision_response_geometry_binding_init(&geometry, leaf,
                                                              this);
    nba97_game_actor_collision_response_binding_init(
        &response, 1000, nba97_game_actor_collision_response_geometry_child,
        &geometry, nullptr, 0);
  }
  std::size_t offset(std::uint32_t address) const {
    return static_cast<std::size_t>(address - Ram);
  }
  void put(std::uint32_t address, std::uint32_t value, unsigned width) {
    const auto at = offset(address);
    for (unsigned i = 0; i < width; ++i) {
      bytes[at + i] = static_cast<std::uint8_t>(value >> (8u * i));
      known[at + i] = 1;
    }
  }
  std::uint32_t get(std::uint32_t address, unsigned width) const {
    std::uint32_t value = 0;
    const auto at = offset(address);
    for (unsigned i = 0; i < width; ++i)
      value |= std::uint32_t(bytes[at + i]) << (8u * i);
    return value;
  }
  static int leaf(void *opaque, const Nba97GameTextMemory *,
                  const Nba97GameActorCollisionResponseEvent *event,
                  Nba97GameActorCollisionResponseMachine *machine) {
    auto &c = *static_cast<Composition *>(opaque);
    ++c.resolver_calls;
    c.resolver_event = *event;
    c.resolver_machine = *machine;
    if (c.refuse_resolver)
      return 0;
    if (event->kind == NBA97_GAME_ACTOR_COLLISION_RESPONSE_RESOLVE_8005EA28) {
      c.put(machine->registers.gpr[4].word + 0xe2u, 1, 2);
      c.put(machine->registers.gpr[5].word + 0xe2u, 1, 2);
    }
    return 1;
  }
  int run() { return nba97_game_opponent_contact(&parent, &parent_progress); }
};

} // namespace
namespace nba97 {
std::string captureGameActorCollisionResponse() {
  Composition c;
  if(c.run()!=NBA97_TEXT_COMPLETE || !c.parent_progress.completed || !c.response.progress.completed || c.geometry.geometry_invocations!=1 || c.resolver_calls!=1)
    throw std::runtime_error("actor collision native composition failed");
  if(c.get(First+0xdc,1)!=9 || c.get(Second+0xdc,1)!=0x78 || c.get(0x800fdb88,2)!=1)
    throw std::runtime_error("actor collision native publication drifted");
  const auto& p=c.response.progress;
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x8005F3BC\",\"inclusive_end\":\"0x8005F887\","
       "\"bytes\":1228,\"instructions\":307,\"classification\":\"no direct visual effect\","
       "\"scope\":\"actual opponent-contact and geometry owners; independent CPU fixture; typed impulse service\","
       "\"completed\":true,\"parent_completed\":true,\"contact_before\":[0,0,0],\"contact_after\":[9,120,1],"
       "\"operations\":" << p.operations << ",\"reads\":" << p.reads << ",\"stores\":" << p.stores
    << ",\"callbacks\":" << p.callbacks_completed << ",\"normal\":[" << p.normal_x.word << ',' << p.normal_y.word << ']'
    << ",\"normal_velocity\":" << p.normal_velocity.word << ",\"tangent_velocity\":" << p.tangent_velocity.word
    << ",\"frame_stack_pointer\":" << p.frame_stack_pointer << ",\"returned_sp\":" << p.machine.registers.gpr[29].word
    << ",\"restored_ra\":" << p.restored_return_address.word << ",\"parent_restored_ra\":" << c.parent_progress.restored_return_address.word
    << ",\"parent_returned_value\":" << c.parent_progress.returned_value.word << ",\"resolver_pc\":" << c.resolver_event.pc
    << ",\"resolver_argument_count\":" << unsigned(c.resolver_event.argument_count) << "}";
  return o.str();
}
}
