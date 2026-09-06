#include "game_match_buffer_rewind_capture.h"
#include "game_match_buffer_rewind_adapter.h"
#include <sstream>
#include <stdexcept>
namespace nba97 { namespace {
std::uint32_t memoryWord(const Nba97GameTextMemory& memory,std::uint32_t address,unsigned width,bool write=false,std::uint32_t value=0) {
 for(std::size_t i=0;i<memory.count;++i){auto&r=memory.region[i];const auto offset=std::uint64_t(address)-r.base;
  if(address<r.base||offset>r.size||width>r.size-offset)continue;
  std::uint32_t result=0;
  for(unsigned b=0;b<width;++b){if(write){r.data[offset+b]=std::uint8_t(value>>(8*b));if(r.known)r.known[offset+b]=1;}
   else{if(r.known&&r.known[offset+b]!=1)throw std::runtime_error("buffer rewind capture unknown byte");result|=std::uint32_t(r.data[offset+b])<<(8*b);}}
  return result;
 }
 throw std::runtime_error("buffer rewind capture unmapped access");
}
}
namespace {
void seed(const Nba97GameTextMemory& memory){
 // Explicit diagnostic sentinels; buffer pointer remains the preceding owner output.
 memoryWord(memory,0x800f1918u,4,true,0xa5a5a5a5u);
 memoryWord(memory,0x800fe860u,4,true,0xa5a5a5a5u);
 memoryWord(memory,0x8002148cu,2,true,0xa5a5u);
 memoryWord(memory,0x800fe864u,1,true,0xa5u);
}
std::string proof(const Nba97GameTextMemory& memory,const Nba97GameMatchBufferRewindBinding& binding,std::uint32_t pc,std::uint32_t before){
 const auto&p=binding.progress;
 if(memoryWord(memory,0x800fa00cu,4)!=before||memoryWord(memory,0x800fa010u,4)!=before||memoryWord(memory,0x800f1918u,4)||memoryWord(memory,0x800fe860u,4)||memoryWord(memory,0x8002148cu,2)||memoryWord(memory,0x800fe864u,1))throw std::runtime_error("buffer rewind source state mismatch");
 std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80076AD0\",\"inclusive_end\":\"0x80076B27\",\"bytes\":88,\"instructions\":22,\"classification\":\"no direct visual effect\",\"scope\":\"actual initializer/reset/buffer/rewind/zero owners on same retained memory; runtime-generated flag sentinels, no advancing match\",\"completed\":true,\"same_parent_memory\":true,\"pointers_verified\":true,\"flags_verified\":true,\"call_pc\":"<<pc<<",\"pointer\":"<<before<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"callbacks\":"<<p.callbacks_completed
 <<",\"zero_stores\":"<<binding.zero_progress.stores<<",\"zero_bytes_stored\":"<<binding.zero_progress.bytes_stored<<",\"return_value\":"<<p.machine.registers.gpr[2].word<<",\"return_address\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"]}";return o.str();
}
}
bool GameMatchBufferRewindCapture::dispatchBuffer(const Nba97GameTextMemory* memory,const Nba97GameMatchBufferInitializeEvent* event,Nba97GameMatchBufferInitializeMachine* machine){
 if(!memory||!event||!machine||!receipt.empty())return false;
 const auto pointer=memoryWord(*memory,0x800fa004u,4);seed(*memory);
 Nba97GameMatchBufferRewindBinding binding{};binding.operation_budget=16;binding.zero_operation_budget=2;
 if(nba97_game_match_buffer_rewind_from_match_buffer_initialize(&binding,memory,event,machine)!=1||binding.result!=NBA97_TEXT_COMPLETE)return false;
 receipt=proof(*memory,binding,event->pc,pointer);return true;
}
bool GameMatchBufferRewindCapture::dispatchReset(const Nba97GameTextMemory* memory,const Nba97GameMatchStateResetEvent* event,Nba97GameMatchStateResetMachine* machine){
 if(!memory||!event||!machine||!receipt.empty())return false;
 const auto pointer=memoryWord(*memory,0x800fa004u,4);seed(*memory);
 Nba97GameMatchBufferRewindBinding binding{};binding.operation_budget=16;binding.zero_operation_budget=2;
 if(nba97_game_match_buffer_rewind_from_match_state_reset(&binding,memory,event,machine)!=1||binding.result!=NBA97_TEXT_COMPLETE)return false;
 receipt=proof(*memory,binding,event->pc,pointer);return true;
}
}
