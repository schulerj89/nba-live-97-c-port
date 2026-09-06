#include "game_ball_actor_contact_capture.h"
#include "game_ball_actor_contact_adapter.h"
#include "game_ball_contact_gate_adapter.h"
#include "game_contact_dispatch_adapter.h"
#include "game_actor_contact_gate_adapter.h"
#include "game_ball_acquire_adapter.h"
#include "game_actor_contact_eligibility_adapter.h"
#include "game_opponent_contact_adapter.h"
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

std::string captureGameContactDispatch() {
  Fixture f;
  f.put32(Fixture::ball,10);
  f.put32(0x800fdc48,Fixture::ball);
  f.put16(0x800fdb90,0x81);
  f.put16(0x800fdbd2,0xffff);
  f.put32(0x80020bec,Fixture::actor);
  f.put32(0x80020c00,Fixture::actor);
  f.put16(Fixture::actor+0x46,0x27);
  for(unsigned i=1;i<=11;++i) {
    const uint32_t actor = i==1 ? Fixture::ball : (i==2 ? Fixture::actor : 0x80004000u+i*0x100u);
    f.put32(0x800fdcbc+i*4,actor);
    if(i>2) { f.put32(actor,i);f.put32(actor+8,100u*256u); }
  }
  Nba97GameBallContactGateBinding contact{};
  contact.child_operation_budget=1000;
  contact.io=Fixture::unresolved;contact.user=&f;contact.contact_binding=f.binding;
  Nba97GameContactDispatchChildren children{};
  children.ball_gate_operation_budget=32;
  children.contact_binding=&contact;
  children.child_8005FAA8=[](void*,const Nba97GameTextMemory*,const Nba97GameContactDispatchEvent*,Nba97GameContactDispatchMachine* m) {
    // Explicit actor-pair boundary: no contact, sorted row may stop.
    m->registers.gpr[2]={0,15};return 1;
  };
  Nba97GameContactDispatchContext c{};
  c.memory=f.context.memory;c.machine=f.context.machine;
  c.machine.registers.gpr[29]={0x801ff038,15};
  c.machine.registers.gpr[31]={0x80068e10,15};
  c.operation_budget=1000;c.io=nba97_game_contact_dispatch_compose_children;c.user=&children;
  Nba97GameContactDispatchProgress p{};
  const int rc=nba97_game_contact_dispatch(&c,&p);
  if(rc!=NBA97_TEXT_COMPLETE || !p.completed || f.get16(0x800fdb90)!=0x82 ||
     f.get16(0x800fe884)!=3 || !contact.contact_progress.completed)
    throw std::runtime_error("contact dispatch native CPU composition failed");
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x80060FBC\",\"inclusive_end\":\"0x800610FB\","
       "\"bytes\":320,\"instructions\":80,\"classification\":\"no direct visual effect\","
       "\"scope\":\"independent full-machine CPU fixture; actual coordinate gate and contact owners; typed actor-pair and contact dependencies\","
       "\"completed\":true,\"operations\":" << p.operations << ",\"reads\":" << p.reads << ",\"stores\":" << p.stores
    << ",\"callbacks\":" << p.callbacks_completed << ",\"coordinate_gates\":" << children.ball_gate_invocations
    << ",\"actor_pairs\":" << children.child_8005FAA8_invocations << ",\"phase_before\":129,\"phase_after\":" << f.get16(0x800fdb90)
    << ",\"phase_delay\":" << f.get16(0x800fe884) << ",\"contact_completed\":" << (contact.contact_progress.completed?"true":"false")
    << ",\"frame_stack_pointer\":" << p.frame_stack_pointer << ",\"returned_sp\":" << p.machine.registers.gpr[29].word
    << ",\"restored_ra\":" << p.restored_return_address.word << "}";
  return o.str();
}

std::string captureGameActorContactGate() {
  Fixture f;
  f.put32(0x800fdc48,Fixture::ball);
  f.put16(Fixture::ball+0xb4,1);
  for(unsigned i=1;i<=11;++i) {
    const uint32_t actor=0x80004000u+i*0x100u;
    f.put32(0x800fdcbc+i*4,i==11?Fixture::ball:actor);
    f.put32(actor+8,i*0x100u);
  }
  Nba97GameActorContactGateBinding binding{};
  binding.operation_budget=16;
  binding.io=[](void*,const Nba97GameTextMemory*,const Nba97GameActorContactGateEvent*,Nba97GameActorContactGateMachine* m) {
    // Explicit eligibility dependency. The source gate overwrites this zero.
    m->registers.gpr[2]={0,15};return 1;
  };
  Nba97GameContactDispatchContext c{};
  c.memory=f.context.memory;c.machine=f.context.machine;
  c.machine.registers.gpr[29]={0x801ff038,15};
  c.machine.registers.gpr[31]={0x80068e10,15};
  c.operation_budget=2000;
  c.io=nba97_game_actor_contact_gate_from_contact_dispatch;c.user=&binding;
  Nba97GameContactDispatchProgress parent{};
  const int rc=nba97_game_contact_dispatch(&c,&parent);
  const auto& p=binding.progress;
  if(rc!=NBA97_TEXT_COMPLETE || !parent.completed || !p.completed ||
     binding.invocations!=45 || p.returned_value.word!=1)
    throw std::runtime_error("actor contact gate native CPU composition failed");
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x8005FAA8\",\"inclusive_end\":\"0x8005FAE7\","
       "\"bytes\":64,\"instructions\":16,\"classification\":\"no direct visual effect\","
       "\"scope\":\"actual sorted dispatcher; independent CPU fixture; typed eligibility child returns zero\","
       "\"completed\":true,\"parent_completed\":true,\"invocations\":" << binding.invocations
    << ",\"call_pc\":" << binding.event.pc << ",\"operations\":" << p.operations
    << ",\"reads\":" << p.reads << ",\"stores\":" << p.stores << ",\"callbacks\":" << p.callbacks_completed
    << ",\"difference\":" << p.coordinate_difference.word << ",\"shifted_difference\":" << p.shifted_difference.word
    << ",\"returned_value\":" << p.returned_value.word << ",\"frame_stack_pointer\":" << p.frame_stack_pointer
    << ",\"returned_sp\":" << p.machine.registers.gpr[29].word << ",\"restored_ra\":" << p.restored_return_address.word << "}";
  return o.str();
}

std::string captureGameBallAcquire() {
  Fixture f;
  f.put16(0x800fdb90,0x81);f.put16(0x800fdbd2,0xffff);
  f.put32(0x80020bec,Fixture::actor);f.put32(0x80020c00,Fixture::actor);
  f.put16(Fixture::actor+0x46,0x27);
  f.put16(Fixture::actor+0xa0,385);
  f.put32(Fixture::actor+0x1c,0x80006000);
  f.put32(0x800fa034,0xffffffff);
  f.put32(0x8001edf8,0x8001eeb8);f.put32(0x8001eebc,0x8001edf4);
  Nba97GameBallAcquireNaturalProgress natural{};
  natural.acquisition_operation_budget=1000;
  natural.acquisition_io=[](void*,const Nba97GameTextMemory*,const Nba97GameBallAcquireEvent* e,Nba97GameBallAcquireMachine* m) {
    // Explicit acquisition dependencies; the real rule-delay leaf is composed.
    if(e->entry==0x8002ab70)m->registers.gpr[2]={0,15};
    return 1;
  };
  const auto owner_before=f.get16(0x800fdbcc);
  const int rc=nba97_game_ball_actor_contact_with_ball_acquire(&f.context,&f.progress,&f.binding,&natural);
  const auto& p=natural.acquisition;
  if(rc!=NBA97_TEXT_COMPLETE || !f.progress.completed || !p.completed || natural.acquisition_count!=1 ||
     f.get32(0x800fdc34)!=Fixture::actor || f.get32(0x800fdc38)!=0x8001edf4 || f.get16(0x800fdb90)!=0x82)
    throw std::runtime_error("ball acquisition native CPU composition failed");
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x8005D140\",\"inclusive_end\":\"0x8005D9EF\","
       "\"bytes\":2224,\"instructions\":556,\"classification\":\"no direct visual effect\","
       "\"scope\":\"actual complete ball contact caller and acquisition owner; independent phase81 fixture; typed geometry, release and acquisition dependencies\","
       "\"completed\":true,\"parent_completed\":true,\"invocations\":" << natural.acquisition_count
    << ",\"call_pc\":" << natural.acquisition_event.pc << ",\"operations\":" << p.operations
    << ",\"reads\":" << p.reads << ",\"stores\":" << p.stores << ",\"callbacks\":" << p.callbacks_completed
    << ",\"owner_before\":" << owner_before << ",\"owner_after\":" << f.get16(0x800fdbcc)
    << ",\"published_actor\":" << f.get32(0x800fdc34) << ",\"published_team\":" << f.get32(0x800fdc38)
    << ",\"phase_before\":129,\"phase_after\":" << f.get16(0x800fdb90) << ",\"phase_delay\":" << f.get16(0x800fe884)
    << ",\"frame_stack_pointer\":" << p.frame_stack_pointer << ",\"returned_sp\":" << p.machine.registers.gpr[29].word
    << ",\"restored_ra\":" << p.restored_return_address.word << "}";
  return o.str();
}

std::string captureGameActorContactEligibility() {
  Fixture f;
  const uint32_t first=0x80010000,second=0x80010200;
  f.put32(first,100);f.put32(second,200);
  f.put32(first+8,0);f.put32(second+8,3u<<8);
  f.put32(first+12,0);f.put32(second+12,4u<<8);
  f.put8(first+0xd9,1);f.put8(second+0xd9,2);
  Nba97GameActorContactEligibilityGeometryBinding geometry{};
  size_t actions=0;
  nba97_game_actor_contact_eligibility_geometry_binding_init(&geometry,
    [](void* user,const Nba97GameTextMemory*,const Nba97GameActorContactEligibilityEvent* e,
       Nba97GameActorContactEligibilityMachine* m) {
      if(e->pc!=0x8005fa2c || e->entry!=0x8005f888 ||
         m->registers.gpr[4].word!=0x80010000 || m->registers.gpr[5].word!=0x80010200) return 0;
      ++*static_cast<size_t*>(user);
      // Explicit unresolved action result; only the existing geometry is composed.
      m->registers.gpr[2]={0x123456cd,15};return 1;
    },&actions);
  Nba97GameActorContactEligibilityBinding binding{};
  nba97_game_actor_contact_eligibility_binding_init(&binding,1000,
    nba97_game_actor_contact_eligibility_geometry_child,&geometry,nullptr,0);
  Nba97GameActorContactGateContext c{};
  c.memory=f.context.memory;c.machine=f.context.machine;c.operation_budget=1000;
  c.machine.registers.gpr[4]={first,15};c.machine.registers.gpr[5]={second,15};
  c.machine.registers.gpr[31]={0x80061054,15};
  c.io=nba97_game_actor_contact_eligibility_from_actor_contact_gate;c.user=&binding;
  Nba97GameActorContactGateProgress parent{};
  const int rc=nba97_game_actor_contact_gate(&c,&parent);
  const auto& p=binding.progress;
  if(rc!=NBA97_TEXT_COMPLETE || !parent.completed || !p.completed ||
     geometry.geometry_invocations!=1 || actions!=1 || p.machine.registers.gpr[2].word!=0xcd ||
     parent.machine.registers.gpr[2].word!=1)
    throw std::runtime_error("actor contact eligibility native CPU composition failed");
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x8005F948\",\"inclusive_end\":\"0x8005FAA7\","
       "\"bytes\":352,\"instructions\":88,\"classification\":\"no direct visual effect\","
       "\"scope\":\"independent CPU fixture; actual coordinate gate and distance owner; typed action\","
       "\"completed\":true,\"parent_completed\":true,\"geometry_calls\":" << geometry.geometry_invocations
    << ",\"action_calls\":" << actions << ",\"operations\":" << p.operations
    << ",\"reads\":" << p.reads << ",\"stores\":" << p.stores << ",\"callbacks\":" << p.callbacks_completed
    << ",\"normalized_x\":3,\"normalized_y\":4,\"action_raw_return\":305419981,\"returned_value\":" << p.machine.registers.gpr[2].word
    << ",\"parent_returned_value\":" << parent.machine.registers.gpr[2].word
    << ",\"frame_stack_pointer\":" << p.frame_stack_pointer << ",\"returned_sp\":" << p.machine.registers.gpr[29].word
    << ",\"restored_ra\":" << p.restored_return_address.word << "}";
  return o.str();
}

std::string captureGameOpponentContact() {
  Fixture f;
  constexpr uint32_t first=0x80010000,second=0x80010200;
  f.put32(first,100);f.put32(second,200);
  f.put8(first+0xd9,1);f.put8(second+0xd9,2);
  f.put8(first+0xda,1);f.put8(second+0xda,1);
  f.put16(0x800fdbcc,7);
  Nba97GameOpponentContactBinding binding{};
  size_t calls=0;
  nba97_game_opponent_contact_binding_init(&binding,1000,
    [](void* user,const Nba97GameTextMemory*,const Nba97GameOpponentContactEvent* e,
       Nba97GameOpponentContactMachine* m) {
      if(e->pc!=0x8005f92c || e->entry!=0x8005f3bc ||
         m->registers.gpr[4].word!=second || m->registers.gpr[5].word!=first) return 0;
      ++*static_cast<size_t*>(user);
      // Explicit collision-response result; no actor physics is fabricated.
      m->registers.gpr[2]={0x123456cd,15};return 1;
    },&calls,nullptr,0);
  Nba97GameActorContactEligibilityGeometryBinding geometry{};
  nba97_game_actor_contact_eligibility_geometry_binding_init(&geometry,
    nba97_game_opponent_contact_from_actor_contact_eligibility,&binding);
  Nba97GameActorContactEligibilityContext c{};
  c.memory=f.context.memory;c.machine=f.context.machine;c.operation_budget=1000;
  c.machine.registers.gpr[4]={first,15};c.machine.registers.gpr[5]={second,15};
  c.machine.registers.gpr[6]={0,15};c.machine.registers.gpr[31]={0x8005fad4,15};
  c.io=nba97_game_actor_contact_eligibility_geometry_child;c.user=&geometry;
  Nba97GameActorContactEligibilityProgress parent{};
  const int rc=nba97_game_actor_contact_eligibility(&c,&parent);
  const auto& p=binding.progress;
  if(rc!=NBA97_TEXT_COMPLETE || !parent.completed || !p.completed ||
     geometry.geometry_invocations!=1 || calls!=1 || p.returned_value.word!=0xcd)
    throw std::runtime_error("opponent contact native CPU composition failed");
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x8005F888\",\"inclusive_end\":\"0x8005F947\","
       "\"bytes\":192,\"instructions\":48,\"classification\":\"no direct visual effect\","
       "\"scope\":\"independent CPU fixture; actual eligibility and distance owner; typed collision response\","
       "\"completed\":true,\"parent_completed\":true,\"geometry_calls\":" << geometry.geometry_invocations
    << ",\"action_calls\":" << calls << ",\"operations\":" << p.operations
    << ",\"reads\":" << p.reads << ",\"stores\":" << p.stores << ",\"callbacks\":" << p.callbacks_completed
    << ",\"input_pair\":[" << first << "," << second << "],\"dispatched_pair\":[" << second << "," << first << "]"
    << ",\"owner\":7,\"first_id\":100,\"returned_value\":" << p.returned_value.word
    << ",\"parent_returned_value\":" << parent.machine.registers.gpr[2].word
    << ",\"frame_stack_pointer\":" << p.frame_stack_pointer << ",\"returned_sp\":" << p.machine.registers.gpr[29].word
    << ",\"restored_ra\":" << p.restored_return_address.word << "}";
  return o.str();
}
}
