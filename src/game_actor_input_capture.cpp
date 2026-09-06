#include "game_actor_input_capture.h"
#include "recovered/game_actor_input.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 {
std::string captureGameActorInput() {
  std::vector<std::uint8_t> bytes(0x200000),known(bytes.size(),1);
  Nba97GameTextRegion region{0x80000000,bytes.data(),known.data(),bytes.size()};
  auto put=[&](std::uint32_t a,std::uint32_t v,unsigned w=4) {
    for(unsigned i=0;i<w;++i)bytes.at(a-region.base+i)=std::uint8_t(v>>(8*i));
  };
  auto get=[&](std::uint32_t a,unsigned w=4) {
    std::uint32_t v=0;for(unsigned i=0;i<w;++i)v|=std::uint32_t(bytes.at(a-region.base+i))<<(8*i);return v;
  };
  put(0x80021d82,1,1);put(0x800fdb8a,1,2);
  put(0x800fdc50,0x80120000);
  for(unsigned i=0;i<21;++i)put(0x800275c4+i*4,0x80068a4c+i*16);
  for(unsigned i=0;i<10;++i) {
    const auto a=0x80110000u+i*0x100u;
    put(0x80020bec+i*4,a);put(a,i);put(a+4,i?0xffff:0,2);
    put(a+0x1a,3,1);put(a+0x46,0x2b,2);
  }
  Nba97GameActorInputContext c{};
  c.memory={&region,1};c.operation_budget=1000;
  for(auto& g:c.machine.registers.gpr)g={0,15};
  c.machine.hi.known_mask=c.machine.lo.known_mask=15;
  c.machine.registers.gpr[29]={0x801ff000,15};
  c.machine.registers.gpr[31]={0x80068e94,15};
  c.io=[](void*,const Nba97GameTextMemory*,const Nba97GameActorInputEvent* e,Nba97GameActorInputMachine* m) {
    // Explicit full-machine input/action fixtures; no physical pad or action implementation is implied.
    if(e->entry==0x8008f224)m->registers.gpr[2]={0x55667788,15};
    if(e->entry==0x8002d2dc)m->registers.gpr[2]={0x11223344,15};
    if(e->entry==0x800700e4)m->registers.gpr[2]={0x99aabbcc,15};
    return 1;
  };
  Nba97GameActorInputProgress p{};
  const int rc=nba97_game_actor_input(&c,&p);
  if(rc!=NBA97_TEXT_COMPLETE || !p.completed || p.callbacks_completed!=15 ||
     get(0x800fdb8a,2)!=0 || get(0x80120028,2)!=1 || get(0x800fdc3c)!=0x80110900)
    throw std::runtime_error("actor input native CPU fixture failed");
  std::ostringstream o;
  o << "{\"program\":\"GAMEONLY\",\"address\":\"0x800686B8\",\"inclusive_end\":\"0x80068BF7\","
       "\"bytes\":1344,\"instructions\":336,\"classification\":\"no direct visual effect\","
       "\"scope\":\"independent ten-actor full-machine fixture; typed input and action dependencies; no live tick bridge\","
       "\"completed\":true,\"operations\":" << p.operations << ",\"reads\":" << p.reads
    << ",\"stores\":" << p.stores << ",\"callbacks\":" << p.callbacks_completed
    << ",\"countdown_before\":1,\"countdown_after\":" << get(0x800fdb8a,2)
    << ",\"controller_flag\":" << get(0x80120028,2) << ",\"last_actor\":" << get(0x800fdc3c)
    << ",\"last_team\":" << get(0x800fdc40) << ",\"action_target\":" << p.computed_action_target
    << ",\"frame_stack_pointer\":" << p.frame_stack_pointer << ",\"returned_sp\":" << p.machine.registers.gpr[29].word
    << ",\"restored_ra\":" << p.restored_return_address.word << "}";
  return o.str();
}
}
