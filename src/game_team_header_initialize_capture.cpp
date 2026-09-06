#include "game_team_header_initialize_capture.h"
#include "game_team_header_initialize_adapter.h"
#include <array>
#include <sstream>
#include <stdexcept>
namespace nba97 {
namespace {
std::uint32_t memoryWord(const Nba97GameTextMemory& memory,std::uint32_t address,unsigned width,bool write=false,std::uint32_t value=0) {
 for(std::size_t i=0;i<memory.count;++i){auto&r=memory.region[i];const auto offset=std::uint64_t(address)-r.base;
  if(address<r.base||offset>r.size||width>r.size-offset)continue;
  std::uint32_t result=0;
  for(unsigned b=0;b<width;++b){if(write){r.data[offset+b]=std::uint8_t(value>>(8*b));if(r.known)r.known[offset+b]=1;}
   else{if(r.known&&r.known[offset+b]!=1)throw std::runtime_error("team header capture unknown byte");result|=std::uint32_t(r.data[offset+b])<<(8*b);}}
  return result;
 }
 throw std::runtime_error("team header capture unmapped access");
}
}
bool GameTeamHeaderInitializeCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GameMatchStateResetEvent* event,Nba97GameMatchStateResetMachine* machine) {
 if(!memory||!event||!machine||calls.size()>=2)return false;
 const auto index=event->pc==0x80065a94u?1u:0u;if(index!=calls.size())return false;
 const auto team=machine->registers.gpr[4].word,opponent=machine->registers.gpr[5].word;
 const auto id=memoryWord(*memory,team,2),side=memoryWord(*memory,team+0x14,2),oppositeSide=memoryWord(*memory,opponent+0x14,2);
 const auto count=memoryWord(*memory,0x80023aecu+104*id,1),active=count<12?count:12;
 const auto injury=memoryWord(*memory,side?0x80021ed6u:0x80021ed5u,1),difficulty=memoryWord(*memory,0x80021d72u,1);
 const auto rank54=(id+3)&255u,rank57=(id+11)&255u,metadata=0x80110000u+id*0x100u;
 // These metadata pointers/ranks are runtime-generated diagnostic inputs.
 // IDs, counts, side words and lineups come from the real preceding owners.
 memoryWord(*memory,0x80020b0cu+4*id,4,true,metadata);
 memoryWord(*memory,metadata+0x54,1,true,rank54);memoryWord(*memory,metadata+0x57,1,true,rank57);
 std::array<std::uint32_t,5> lineup{};for(unsigned i=0;i<5;++i)lineup[i]=memoryWord(*memory,team+0x16+2*i,2);
 Nba97GameTeamHeaderInitializeBinding binding{};binding.operation_budget[0]=binding.operation_budget[1]=1000;
 if(nba97_game_team_header_initialize_from_match_state_reset(&binding,memory,event,machine)!=1||binding.result[index]!=NBA97_TEXT_COMPLETE)return false;
 const auto&p=binding.progress[index];bool statuses=true,actors=true,lineups=true;
 for(unsigned i=0;i<12;++i)statuses=statuses&&memoryWord(*memory,(side?0x8001f984u:0x8001f7ecu)+0x20+34*i,2)==(i<active&&i!=injury?0x7fff:0xfffe);
 for(unsigned i=0;i<5;++i){const auto actor=0x800fdcecu+(side+i)*244;
  actors=actors&&memoryWord(*memory,0x80020becu+4*(side+i),4)==actor&&memoryWord(*memory,actor+0xd6,2)==((oppositeSide+i)&65535u);
  lineups=lineups&&memoryWord(*memory,team+0x98+2*i,2)==lineup[i];}
 if(!statuses||!actors||!lineups||memoryWord(*memory,team+0x66,2)!=active||memoryWord(*memory,team+0x68,2)!=active)throw std::runtime_error("team header capture state mismatch");
 std::ostringstream o;o<<"{\"call_pc\":"<<event->pc<<",\"team\":"<<team<<",\"team_id\":"<<id<<",\"side\":"<<side<<",\"active_count\":"<<active<<",\"injury\":"<<injury<<",\"difficulty\":"<<difficulty
 <<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"status_iterations\":"<<p.status_iterations<<",\"unused_iterations\":"<<p.unused_iterations<<",\"actor_iterations\":"<<p.actor_iterations
 <<",\"direction\":"<<memoryWord(*memory,team+0x10,4)<<",\"thresholds\":["<<memoryWord(*memory,team+0x62,2)<<','<<memoryWord(*memory,team+0x72,2)<<','<<memoryWord(*memory,team+0x74,2)<<"],\"ranks\":["<<rank54<<','<<rank57<<"],\"statuses_verified\":true,\"actors_verified\":true,\"lineups_verified\":true,\"return_address\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"]}";
 calls.push_back(o.str());
 if(calls.size()==2)receipt="{\"program\":\"GAMEONLY\",\"address\":\"0x800655B0\",\"inclusive_end\":\"0x8006581F\",\"bytes\":624,\"instructions\":156,\"classification\":\"no direct visual effect\",\"scope\":\"actual initializer/reset/roster/team-header owners on same retained memory; runtime-generated metadata ranks, remaining typed controller/period services, no advancing match\",\"completed\":true,\"same_parent_memory\":true,\"calls\":["+calls[0]+","+calls[1]+"]}";
 return true;
}
}
