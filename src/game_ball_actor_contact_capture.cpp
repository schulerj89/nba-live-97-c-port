#include "game_ball_actor_contact_capture.h"
#include "game_ball_actor_contact_adapter.h"
#include "game_ball_contact_gate_adapter.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 {
namespace {
struct Fixture {
  static constexpr uint32_t ball = 0x80001000, actor = 0x80002000;
  std::vector<uint8_t> bytes = std::vector<uint8_t>(0x200000),
                       known = std::vector<uint8_t>(0x200000, 1);
  Nba97GameTextRegion region{0x80000000, bytes.data(), known.data(),
                             bytes.size()};
  Nba97GameBallActorContactContext context{};
  Nba97GameBallActorContactProgress progress{};
  Nba97GameBallActorContactBinding binding{};
  std::vector<uint32_t> calls, resume_calls;
  int32_t contact = 1;
  uint32_t random_value = 0;
  bool resume_accept = true;
  Fixture() {
    context.memory.region = &region;
    context.memory.count = 1;
    context.operation_budget = 10000;
    context.io = unresolved;
    context.user = this;
    for (auto &g : context.machine.registers.gpr) {
      g.word = 0;
      g.known_mask = 15;
    }
    context.machine.hi.known_mask = context.machine.lo.known_mask = 15;
    context.machine.registers.gpr[29].word = 0x801ff000;
    context.machine.registers.gpr[31].word = 0x80060edc;
    context.machine.registers.gpr[4].word = ball;
    context.machine.registers.gpr[5].word = actor;
    put16(0x800fdbcc, 0xffff);
    put32(0x800fdb58, 1);
    put16(0x800fe8c4, 0);
    put16(0x800fe8cc, 0);
    put16(0x800fdb90, 0);
    put16(0x800fdb94, 0);
    put16(0x800fdbd4, 0);
    put16(0x800fdbd2, 0);
    put16(0x800fdbd0, 0xffff);
    put32(0x800fdc40, 0x8001edf4);
    put32(0x800fdc48, actor);
    put8(actor + 0xd9, 0);
    put16(actor + 4, 0xffff);
    put32(actor + 0x20, 0x80003000);
    put8(0x8000300d, 0);
    binding.actor_resume_io = resume;
    binding.actor_resume_user = this;
    binding.child_operation_budget = 1000;
  }
  void put8(uint32_t a, uint8_t v) { bytes[a - region.base] = v; }
  void put16(uint32_t a, uint16_t v) {
    put8(a, uint8_t(v));
    put8(a + 1, uint8_t(v >> 8));
  }
  void put32(uint32_t a, uint32_t v) {
    put16(a, uint16_t(v));
    put16(a + 2, uint16_t(v >> 16));
  }
  uint16_t get16(uint32_t a) const {
    return uint16_t(bytes[a - region.base] |
                    (uint16_t(bytes[a - region.base + 1]) << 8));
  }
  uint32_t get32(uint32_t a) const {
    return get16(a) | (uint32_t(get16(a + 2)) << 16);
  }
  int run() {
    return nba97_game_ball_actor_contact_run(&context, &progress, &binding);
  }
  static int unresolved(void *u, const Nba97GameTextMemory *,
                        const Nba97GameBallActorContactEvent *e,
                        Nba97GameBallActorContactMachine *m) {
    auto &f = *static_cast<Fixture *>(u);
    f.calls.push_back(e->pc);
    if (e->entry == 0x8007066c || e->entry == 0x8005d140) {
      m->registers.gpr[2].word = 0;
      m->registers.gpr[2].known_mask = 15;
    }
    if (e->entry == 0x8002ab70) {
      m->registers.gpr[2].word = f.random_value;
      m->registers.gpr[2].known_mask = 15;
    }
    if (e->entry == 0x800601b8 || e->entry == 0x80060240 ||
        e->entry == 0x80060008) {
      m->registers.gpr[2].word = uint32_t(f.contact);
      m->registers.gpr[2].known_mask = 15;
    }
    return 1;
  }
  static int resume(void *u, const Nba97GameTextMemory *,
                    const Nba97GameActorResumeEvent *e,
                    Nba97GameActorResumeMachine *) {
    auto &f = *static_cast<Fixture *>(u);
    f.resume_calls.push_back(e->pc);
    return f.resume_accept ? 1 : 0;
  }
};
}
std::string captureGameBallActorContact() {
  Fixture f;
  f.put16(0x800fdb90, 0x81);
  f.put16(0x800fdbd2, 0xffff);
  f.put32(0x80020bec, Fixture::actor);
  f.put32(0x80020c00, Fixture::actor);
  f.put16(Fixture::actor + 0x46, 0x27);
  // The gate now supplies the real contact entry, including the ID10 swap.
  f.put32(Fixture::ball, 10);
  Nba97GameBallContactGateContext gate{};
  gate.memory = f.context.memory;
  gate.machine = f.context.machine;
  gate.machine.registers.gpr[4] = {Fixture::actor,15};
  gate.machine.registers.gpr[5] = {Fixture::ball,15};
  gate.machine.registers.gpr[29] = {0x801ff018,15};
  gate.machine.registers.gpr[31] = {0x80061078,15};
  gate.operation_budget = 32;
  Nba97GameBallContactGateBinding binding{};
  binding.child_operation_budget = f.context.operation_budget;
  binding.io = Fixture::unresolved;
  binding.user = &f;
  binding.contact_binding = f.binding;
  Nba97GameBallContactGateProgress g{};
  const int result = nba97_game_ball_contact_gate_run(&gate,&g,&binding);
  f.progress = binding.contact_progress;
  f.binding = binding.contact_binding;
  const auto& p = f.progress;
  if (result != NBA97_TEXT_COMPLETE || !p.completed ||
      f.get16(0x800fdb90) != 0x82 || f.get16(0x800fe884) != 3 ||
      f.binding.actor_resume_count != 2 ||
      !f.binding.actor_resume[0].completed || !f.binding.actor_resume[1].completed)
    throw std::runtime_error("ball actor contact CPU probe failed");
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x800602CC\",\"inclusive_end\":\"0x80060E8B\","
       "\"bytes\":3008,\"instructions\":752,\"classification\":\"no direct visual effect\","
       "\"scope\":\"independent phase81 contact fixture; aliased jumper references; typed geometry, acquisition and release services; actual actor-reset owners\","
       "\"completed\":true,\"phase_before\":129,\"phase_after\":" << f.get16(0x800fdb90)
    << ",\"phase_delay\":" << f.get16(0x800fe884)
    << ",\"operations\":" << p.operations << ",\"reads\":" << p.reads << ",\"stores\":" << p.stores
    << ",\"callbacks\":" << p.callbacks_completed << ",\"actor_resets\":" << f.binding.actor_resume_count
    << ",\"frame_stack_pointer\":" << p.frame_stack_pointer << ",\"returned_sp\":" << p.machine.registers.gpr[29].word
    << ",\"restored_ra\":" << p.restored_return_address.word << ",\"typed_call_pcs\":[";
  for (size_t i=0;i<f.calls.size();++i) { if(i)o<<',';o<<f.calls[i]; }
  o << "],\"coordinate_gate\":{\"program\":\"GAMEONLY\",\"address\":\"0x80060E8C\","
       "\"inclusive_end\":\"0x80060EF7\",\"bytes\":108,\"instructions\":27,"
       "\"classification\":\"no direct visual effect\",\"scope\":\"actual complete contact child; independent CPU fixture\","
       "\"completed\":" << (g.completed?"true":"false") << ",\"operations\":" << g.operations
    << ",\"reads\":" << g.reads << ",\"stores\":" << g.stores << ",\"callbacks\":" << g.callbacks_completed
    << ",\"returned_value\":" << g.returned_value.word << ",\"call_pc\":" << binding.event.pc
    << ",\"child_arguments\":[" << binding.entry_machine.registers.gpr[4].word << ','
    << binding.entry_machine.registers.gpr[5].word << ',' << binding.entry_machine.registers.gpr[6].word
    << "],\"frame_stack_pointer\":" << g.frame_stack_pointer << ",\"returned_sp\":" << g.machine.registers.gpr[29].word
    << ",\"restored_ra\":" << g.restored_return_address.word << "}}";
  return o.str();
}
}
