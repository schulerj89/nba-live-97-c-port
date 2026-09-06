#include "game_substitution_candidate_select_capture.h"
#include "game_team_strategy_apply_capture.h"
#include "game_team_strategy_apply_adapter.h"
#include <array>
#include <sstream>
#include <stdexcept>
namespace nba97 { namespace {
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
namespace {
struct Child {
 GameSubstitutionCandidateSelectCapture candidate;
 Nba97GameTeamStrategyApplyEvent event{}; std::array<std::uint32_t,4> args{}; unsigned calls=0;
 static int service(void* opaque,const Nba97GameTextMemory* memory,const Nba97GameTeamStrategyApplyEvent* event,Nba97GameTeamStrategyApplyMachine* machine){
  auto&self=*static_cast<Child*>(opaque);self.event=*event;++self.calls;
  for(unsigned i=0;i<4;++i)self.args[i]=machine->registers.gpr[4+i].word;
  if(event->entry==0x80064dbcu)return self.candidate.dispatch(memory,event,machine);
  // Explicit diagnostic result for the remaining binding service.
  machine->registers.gpr[2]={event->entry,15}; return 1;
 }
};
}
bool GameTeamStrategyApplyCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GameMatchStateResetEvent* event,Nba97GameMatchStateResetMachine* machine){
 if(!memory||!event||!machine||calls.size()>=2)return false;
 const auto index=event->pc==0x80065ac4u?1u:0u;if(index!=calls.size())return false;
 const auto team=machine->registers.gpr[4].word, side=memoryWord(*memory,team+0x14,2), before=memoryWord(*memory,team+0x66,2);
 // Controlled diagnostic strategy/injury inputs. Retain the preceding actual
 // roster/header counts, lineup entries and status records.
 memoryWord(*memory,team+0x42,2,true,index?0:1);
 memoryWord(*memory,side?0x80021ed6u:0x80021ed5u,1,true,index?5:0);
 const std::array<std::uint32_t,7> settings{0x80021deau,0x80021de8u,0x80021de6u,0x80021decu,0x80021deeu,0x80021df0u,0x80021df2u};
 const std::array<unsigned,7> offsets{0x78,0x77,0x76,0x38,0x39,0x36,0x37};
 if(!index)for(unsigned i=0;i<7;++i)memoryWord(*memory,settings[i],1,true,0x20+i);
 const auto lineup5=memoryWord(*memory,team+0x16+10,2),lineup11=memoryWord(*memory,team+0x16+22,2);
 Child child;Nba97GameTeamStrategyApplyResetBinding binding{};binding.operation_budget=128;binding.io=Child::service;binding.user=&child;
 if(nba97_game_team_strategy_apply_from_reset(&binding,memory,event,machine)!=1||binding.result!=NBA97_TEXT_COMPLETE||child.calls!=1)return false;
 const auto&p=binding.progress;bool fields=true;
 if(!index)for(unsigned i=0;i<7;++i)fields=fields&&memoryWord(*memory,team+offsets[i],1)==0x20+i;
 else fields=memoryWord(*memory,team+0x76,1)==1&&memoryWord(*memory,team+0x77,1)==1;
 const bool lineup=!index||(memoryWord(*memory,team+0x16+10,2)==lineup11&&memoryWord(*memory,team+0x16+22,2)==lineup5);
 const auto after=memoryWord(*memory,team+0x66,2);
 if(!fields||!lineup||after!=((before-1)&65535u)||child.event.pc!=(index?0x80065998u:0x800659c4u)||child.event.entry!=(index?0x800646a8u:0x80064dbcu))throw std::runtime_error("strategy capture source state mismatch");
 if(!index&&(child.args[0]!=team||child.args[1]!=0||child.args[2]!=0x8001f7ecu||child.args[3]!=0))throw std::runtime_error("strategy capture direct child args");
 if(index&&(child.args[0]!=lineup5||child.args[1]!=lineup11))throw std::runtime_error("strategy capture scan child args");
 std::ostringstream o;o<<"{\"call_pc\":"<<event->pc<<",\"team\":"<<team<<",\"side\":"<<side<<",\"injury\":"<<(index?5:0)<<",\"count_before\":"<<before<<",\"count_after\":"<<after
 <<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"fields_verified\":true,\"lineup_verified\":true,\"child_pc\":"<<child.event.pc<<",\"child_entry\":"<<child.event.entry<<",\"child_args\":["<<child.args[0]<<','<<child.args[1]<<','<<child.args[2]<<','<<child.args[3]<<"],\"return_address\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"],\"candidate_select\":"<<(child.candidate.receipt.empty()?"null":child.candidate.receipt)<<"}";
 calls.push_back(o.str());
 if(calls.size()==2)receipt="{\"program\":\"GAMEONLY\",\"address\":\"0x80065820\",\"inclusive_end\":\"0x800659EF\",\"bytes\":464,\"instructions\":116,\"classification\":\"no direct visual effect\",\"scope\":\"actual initializer/reset/roster/team-header/strategy owners on same retained memory; runtime-generated strategy/injury inputs, real candidate selection, typed binding service, no advancing match\",\"completed\":true,\"same_parent_memory\":true,\"calls\":["+calls[0]+","+calls[1]+"]}";
 return true;
}
}
