#include "game_period_presentation_finish_capture.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace nba97 { namespace {
std::uint32_t word(const Nba97GameTextMemory& m,std::uint32_t a,unsigned width,bool store=false,std::uint32_t value=0) {
  for(std::size_t i=0;i<m.count;++i){const auto&r=m.region[i];const auto offset=std::uint64_t(a)-r.base;
    if(a<r.base || offset>r.size || width>r.size-offset)continue;
    std::uint32_t v=0;for(unsigned b=0;b<width;++b){
      if(store){r.data[offset+b]=std::uint8_t(value>>(8*b));if(r.known)r.known[offset+b]=1;}
      else if(r.known&&r.known[offset+b]!=1)throw std::runtime_error("presentation capture unknown byte");
      v|=std::uint32_t(r.data[offset+b])<<(8*b);
    }return v;
  }throw std::runtime_error("presentation capture unmapped byte");
}
struct Children {
  std::vector<std::uint32_t> pcs;
  static int call(void*u,const Nba97GameTextMemory* memory,const Nba97GamePeriodPresentationFinishEvent* e,Nba97GamePeriodPresentationFinishMachine* m){
    auto&self=*static_cast<Children*>(u);self.pcs.push_back(e->pc);
    if(word(*memory,0x800eb680u,1)!=0 || word(*memory,0x80109afcu,4)!=1 || word(*memory,0x80109ae4u,4)!=0x80170000u)return 0;
    // Explicit presentation-service response; it does not draw a match card.
    m->registers.gpr[2]={e->entry,15};return 1;
  }
};
}
bool GamePeriodPresentationFinishCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GameFirstPeriodStartupEvent* event,Nba97GameFirstPeriodStartupRegisters* registers){
  if(!memory||!event||!registers||receipt!="null")return false;
  const auto flag=word(*memory,0x800eb680u,1);
  // Explicit publication-word and gate inputs on the existing caller's RAM.
  word(*memory,0x8001ede8u,4,true,0x80170000u);word(*memory,0x800fdb78u,1,true,0);
  Children children;Nba97GamePeriodPresentationFinishBinding b{};b.operation_budget=16;b.io=Children::call;b.user=&children;
  if(nba97_game_period_presentation_finish_from_first_period_startup(&b,memory,event,registers)!=1)return false;
  const auto&p=b.progress;
  if(!p.completed||children.pcs!=std::vector<std::uint32_t>{0x8002ddf8,0x8002de14}||word(*memory,0x800eb680u,1)!=0||word(*memory,0x80109afcu,4)!=0||word(*memory,0x80109ae4u,4)!=0x80170000u)throw std::runtime_error("presentation capture state drifted");
  std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x8002DDCC\",\"inclusive_end\":\"0x8002DE33\",\"bytes\":104,\"instructions\":26,\"classification\":\"no direct visual effect\",\"scope\":\"actual first-period caller on same synthetic retained memory; typed presentation children, no advancing match\",\"completed\":true,\"same_parent_memory\":true,\"call_pc\":"<<event->pc<<",\"flag_before\":"<<flag<<",\"flag_after\":0,\"active_after\":0,\"published_word\":"<<word(*memory,0x80109ae4u,4)<<",\"gate\":"<<p.gate_flag.word<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"callbacks\":"<<p.callbacks_completed<<",\"return_address\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"returned_value\":"<<p.returned_value.word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"],\"call_pcs\":["<<children.pcs[0]<<','<<children.pcs[1]<<"]}";
  receipt=o.str();return true;
}
}
