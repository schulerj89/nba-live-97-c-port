#include "game_match_buffer_compress_capture.h"
#include "game_match_buffer_record_capture.h"
#include <sstream>
#include <cstdint>
#include <stdexcept>
namespace nba97 { namespace {
std::uint32_t word(const Nba97GameTextMemory& memory,std::uint32_t address,unsigned width,bool write=false,std::uint32_t value=0) {
 for(std::size_t i=0;i<memory.count;++i){auto&r=memory.region[i];const auto offset=std::uint64_t(address)-r.base;
  if(address<r.base||offset>r.size||width>r.size-offset)continue;
  std::uint32_t result=0;
  for(unsigned b=0;b<width;++b){if(write){r.data[offset+b]=std::uint8_t(value>>(8*b));if(r.known)r.known[offset+b]=1;}
   else{if(r.known&&r.known[offset+b]!=1)throw std::runtime_error("buffer-record capture unknown byte");result|=std::uint32_t(r.data[offset+b])<<(8*b);}}
  return result;
 }
 throw std::runtime_error("buffer-record capture unmapped access");
}
struct Child {
 GameMatchBufferCompressCapture compressed;
 unsigned calls=0;std::uint32_t args[4]{};
 static int compress(void* u,const Nba97GameTextMemory* memory,const Nba97GameMatchBufferRecordEvent* e,Nba97GameMatchBufferRecordMachine* m) {
  auto& self=*static_cast<Child*>(u);++self.calls;
  if(e->entry!=0x800767fcu||e->pc!=0x80076e58u)return 0;
  for(unsigned i=0;i<4;++i)self.args[i]=m->registers.gpr[4+i].word;
  return self.compressed.dispatch(memory,e,m);
 }
};
}
bool GameMatchBufferRecordCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GamePeriodStartupEvent* event,Nba97GamePeriodStartupRegisters* registers) {
 if(!memory||!event||!registers||calls.size()>=2)return false;
 const auto index=calls.size();
 if(!index){
  // Explicit runtime fixtures for pointers/records absent from the older period
  // diagnostic. Preserve its live clocks, ball pointer and pending flag.
  word(*memory,0x8002148cu,2,true,1);word(*memory,0x800f9ffcu,2,true,0);
  for(unsigned i=0;i<8;++i){const auto pointer=0x80060000u+i*0x80u;word(*memory,0x800fdc50u+4*i,4,true,pointer);word(*memory,pointer+0x26,2,true,0x120u+i);}
  word(*memory,0x80020becu,4,true,0x80070000u);
  for(unsigned i=0;i<11;++i){const auto entity=0x80070000u+244*i;for(unsigned j=0;j<3;++j)word(*memory,entity+8+4*j,4,true,(i*17u+j*31u)<<8);for(unsigned j=0;j<0x30;j+=2)word(*memory,entity+0x84+j,2,true,0x140+i+j);}
  word(*memory,0x800fa004u,4,true,0x800ccc00u);word(*memory,0x800fa008u,4,true,0x800d5734u);word(*memory,0x800fa00cu,4,true,0x800ccc00u);word(*memory,0x800fa010u,4,true,0x800ccc00u);word(*memory,0x800fa014u,4,true,0);
 }
 const auto pendingBefore=word(*memory,0x800fe864u,1),clock=word(*memory,0x800fdb6cu,2),aux=word(*memory,0x800fdb94u,2);
 Child child;Nba97GameMatchBufferRecordBinding binding{};nba97_game_match_buffer_record_binding_init(&binding,1024,20,4);binding.io=Child::compress;binding.user=&child;
 if(nba97_game_match_buffer_record_from_period_startup(&binding,memory,event,registers)!=1)return false;
 const auto&p=binding.progress;const auto snapshot=child.args[0];
 bool fields=word(*memory,snapshot+9,1)==(clock&255u)&&word(*memory,snapshot+8,1)==((aux&0x8000u)?0:(aux&255u));
 for(unsigned i=0;i<8;++i)fields=fields&&word(*memory,snapshot+10+i,1)==0x20+i;
 for(unsigned i=0;i<11;++i)for(unsigned j=0;j<3;++j)fields=fields&&word(*memory,snapshot+0x28+i*20+j*2,2)==i*17+j*31;
 if(!p.completed||!fields||pendingBefore!=1||word(*memory,0x800fe864u,1)!=0||child.calls!=1)throw std::runtime_error("buffer-record capture drifted");
 std::ostringstream o;o<<"{\"call_pc\":"<<event->pc<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"callbacks\":"<<p.callbacks_completed<<",\"entity_iterations\":"<<p.entity_iterations<<",\"snapshot\":"<<snapshot<<",\"fields_verified\":true,\"pending_before\":"<<pendingBefore<<",\"pending_after\":0,\"cursor\":"<<word(*memory,0x800fa010u,4)<<",\"rewind_calls\":"<<binding.rewind_invocations<<",\"zero_stores\":"<<binding.rewind.zero_progress.stores<<",\"compression_args\":["<<child.args[0]<<','<<child.args[1]<<','<<child.args[2]<<','<<child.args[3]<<"],\"return_address\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"],\"compression\":"<<child.compressed.receipt<<"}";
 calls.push_back(o.str());return true;
}
std::string GameMatchBufferRecordCapture::receipt() const {
 if(calls.size()!=2)throw std::runtime_error("missing buffer-record calls");
 return "{\"program\":\"GAMEONLY\",\"address\":\"0x80076B3C\",\"inclusive_end\":\"0x80076FC7\",\"bytes\":1164,\"instructions\":291,\"classification\":\"no direct visual effect\",\"scope\":\"actual period/record/rewind/zero owners on same retained diagnostic memory; explicit pointer/entity fixtures and actual independently decoded compression, no advancing match\",\"completed\":true,\"same_parent_memory\":true,\"calls\":["+calls[0]+","+calls[1]+"]}";
}
}
