#include "game_match_buffer_initialize_capture.h"
#include "game_match_buffer_initialize_adapter.h"
#include <sstream>
#include <stdexcept>
namespace nba97 { namespace {
std::uint32_t memoryWord(const Nba97GameTextMemory& memory,std::uint32_t address,unsigned width,bool write=false,std::uint32_t value=0) {
 for(std::size_t i=0;i<memory.count;++i){auto&r=memory.region[i];const auto offset=std::uint64_t(address)-r.base;
  if(address<r.base||offset>r.size||width>r.size-offset)continue;
  std::uint32_t result=0;
  for(unsigned b=0;b<width;++b){if(write){r.data[offset+b]=std::uint8_t(value>>(8*b));if(r.known)r.known[offset+b]=1;}
   else{if(r.known&&r.known[offset+b]!=1)throw std::runtime_error("match buffer capture unknown byte");result|=std::uint32_t(r.data[offset+b])<<(8*b);}}
  return result;
 }
 throw std::runtime_error("match buffer capture unmapped access");
}
}
namespace {
struct Child {
 unsigned calls=0; std::uint32_t pc=0,entry=0;
 static int service(void* opaque,const Nba97GameTextMemory* memory,const Nba97GameMatchBufferInitializeEvent* event,Nba97GameMatchBufferInitializeMachine* machine){
  auto&self=*static_cast<Child*>(opaque);++self.calls;self.pc=event->pc;self.entry=event->entry;
  if(memoryWord(*memory,0x800fa000u,2)!=0x76||memoryWord(*memory,0x800fa004u,4)!=0x800ccc00u||memoryWord(*memory,0x800fa008u,4)!=0x800d5734u)return 0;
  // Explicit diagnostic response until the cursor-reset owner is composed.
  machine->registers.gpr[2]={event->entry,15};return 1;
 }
};
}
bool GameMatchBufferInitializeCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GameMatchStateResetEvent* event,Nba97GameMatchStateResetMachine* machine){
 if(!memory||!event||!machine||!receipt.empty())return false;
 // Mark the clear range with a runtime-generated diagnostic sentinel.
 for(unsigned i=0;i<0x378;++i)memoryWord(*memory,0x800f9ffcu+i,1,true,0xa5);
 Child child;Nba97GameMatchBufferInitializeBinding binding{};binding.operation_budget=16;binding.zero_operation_budget=512;binding.io=Child::service;binding.user=&child;
 if(nba97_game_match_buffer_initialize_from_match_state_reset(&binding,memory,event,machine)!=1||binding.result!=NBA97_TEXT_COMPLETE||child.calls!=1||child.pc!=0x80064370u||child.entry!=0x80076ad0u)return false;
 bool bytes=true;for(unsigned i=0;i<0x378;++i){unsigned expected=0;
  if(i==4)expected=0x76;
  if(i>=8&&i<12)expected=(0x800ccc00u>>(8*(i-8)))&255u;
  if(i>=12&&i<16)expected=(0x800d5734u>>(8*(i-12)))&255u;
  bytes=bytes&&memoryWord(*memory,0x800f9ffcu+i,1)==expected;}
 if(!bytes)throw std::runtime_error("match buffer capture clear/header mismatch");
 const auto&p=binding.progress;std::ostringstream o;
 o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x8006432C\",\"inclusive_end\":\"0x80064387\",\"bytes\":92,\"instructions\":23,\"classification\":\"no direct visual effect\",\"scope\":\"actual initializer/reset/match-buffer/zero owners on same retained memory; runtime-generated clear sentinel, typed 80076AD0 service, no advancing match\",\"completed\":true,\"same_parent_memory\":true,\"buffer_verified\":true,\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"callbacks\":"<<p.callbacks_completed
 <<",\"zero_stores\":"<<binding.zero_progress.stores<<",\"zero_completed\":"<<unsigned(binding.zero_progress.completed)<<",\"child_pc\":"<<child.pc<<",\"child_entry\":"<<child.entry<<",\"return_address\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"]}";
 receipt=o.str();return true;
}
}
