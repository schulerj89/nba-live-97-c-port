#include "game_controller_profile_reset_capture.h"
#include "game_controller_profile_reset_adapter.h"
#include <sstream>
#include <stdexcept>
namespace nba97 {
namespace {
std::uint8_t* byte(const Nba97GameTextMemory& memory,std::uint32_t address,bool write) {
 for(std::size_t i=0;i<memory.count;++i) {auto&r=memory.region[i];const auto offset=std::uint64_t(address)-r.base;
  if(address<r.base||offset>=r.size)continue;
  if(r.known){if(write)r.known[offset]=1;else if(r.known[offset]!=1)throw std::runtime_error("profile capture unknown byte");}
  return r.data+offset;
 }
 throw std::runtime_error("profile capture unmapped byte");
}
}
bool GameControllerProfileResetCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GameMatchStateResetEvent* event,Nba97GameMatchStateResetMachine* machine) {
 if(!memory||!event||!machine||!receipt.empty())return false;
 // Runtime-generated input profiles are explicit diagnostic fixtures on the
 // parent's retained memory, not a claim about retail profile contents.
 for(unsigned i=0;i<8;++i){*byte(*memory,0x80021ddeu+i,true)=i%3==2?255:std::uint8_t(i%3);
  for(unsigned j=0;j<120;++j)*byte(*memory,0x8001ef7cu+120*i+j,true)=0xa5;}
 *byte(*memory,0x80020c87u,true)=1;*byte(*memory,0x80020cf3u,true)=0;
 for(unsigned j=0;j<59;++j){*byte(*memory,0x80020c3eu+j,true)=std::uint8_t(0x30+j);*byte(*memory,0x800bc94cu+j,true)=std::uint8_t(0xa0+j);}
 Nba97GameControllerProfileResetBinding binding{};
 nba97_game_controller_profile_reset_binding_init(&binding,2000,32);
 if(nba97_game_controller_profile_reset_from_match_state_reset(&binding,memory,event,machine)!=1||binding.result!=NBA97_TEXT_COMPLETE||binding.zero_invocations!=8)return false;
 const auto&p=binding.progress;bool content=true;
 for(unsigned i=0;i<8;++i)for(unsigned j=0;j<120;++j){unsigned expected=0xa5;
  if(j<36)expected=0;else if(j>=60&&j<119&&i%3!=2)expected=(i%3==0?0x30:0xa0)+j-60;
  content=content&&*byte(*memory,0x8001ef7cu+120*i+j,false)==expected;}
 if(!content||p.records_copied!=6||p.bytes_copied!=354)throw std::runtime_error("profile capture record mismatch");
 std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80083490\",\"inclusive_end\":\"0x800835C3\",\"bytes\":308,\"instructions\":77,\"call_pc\":\"0x80065A38\","
 "\"classification\":\"no direct visual effect\",\"scope\":\"actual initializer/reset/profile owners on same retained memory; runtime-generated selected/default/negative profile fixtures, actual zero owner, no advancing match\",\"completed\":true,\"same_parent_memory\":true,\"records_verified\":true,"
 "\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"zero_calls\":"<<binding.zero_invocations<<",\"records_started\":"<<p.records_started<<",\"records_copied\":"<<p.records_copied<<",\"bytes_copied\":"<<p.bytes_copied
 <<",\"return_address\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"]}";
 receipt=o.str();return true;
}
}
