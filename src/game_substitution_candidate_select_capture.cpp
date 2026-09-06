#include "game_substitution_candidate_select_capture.h"
#include "game_substitution_candidate_select_adapter.h"
#include <sstream>
#include <stdexcept>
namespace nba97 { namespace {
std::uint32_t memoryWord(const Nba97GameTextMemory& memory,std::uint32_t address,unsigned width,bool write=false,std::uint32_t value=0) {
 for(std::size_t i=0;i<memory.count;++i){auto&r=memory.region[i];const auto offset=std::uint64_t(address)-r.base;
  if(address<r.base||offset>r.size||width>r.size-offset)continue;
  std::uint32_t result=0;
  for(unsigned b=0;b<width;++b){if(write){r.data[offset+b]=std::uint8_t(value>>(8*b));if(r.known)r.known[offset+b]=1;}
   else{if(r.known&&r.known[offset+b]!=1)throw std::runtime_error("substitution candidate capture unknown byte");result|=std::uint32_t(r.data[offset+b])<<(8*b);}}
  return result;
 }
 throw std::runtime_error("substitution candidate capture unmapped access");
}
}
namespace {
int unresolved(void*,const Nba97GameTextMemory*,const Nba97GameSubstitutionCandidateSelectEvent*,Nba97GameSubstitutionCandidateSelectMachine*) { return 0; }
}
bool GameSubstitutionCandidateSelectCapture::dispatch(const Nba97GameTextMemory* memory,const Nba97GameTeamStrategyApplyEvent* event,Nba97GameTeamStrategyApplyMachine* machine){
 if(!memory||!event||!machine||!receipt.empty())return false;
 const auto team=machine->registers.gpr[4].word,count=memoryWord(*memory,team+0x68,2),injury=memoryWord(*memory,machine->registers.gpr[6].word+0x20,2);
 // Retain all preceding owner state. The still-typed player-binding service
 // has not populated inverse-lineup entries; do not invent candidates here.
 Nba97GameSubstitutionCandidateSelectStrategyBinding binding{};binding.operation_budget=512;binding.io=unresolved;
 if(nba97_game_substitution_candidate_select_from_strategy(&binding,memory,event,machine)!=1||binding.result!=NBA97_TEXT_COMPLETE)return false;
 const auto&p=binding.progress;
 if(p.callbacks_completed||p.machine.registers.gpr[2].word!=0)throw std::runtime_error("candidate capture unexpected unbound candidate");
 std::ostringstream o;o<<"{\"program\":\"GAMEONLY\",\"address\":\"0x80064DBC\",\"inclusive_end\":\"0x8006506F\",\"bytes\":692,\"instructions\":173,\"classification\":\"no direct visual effect\",\"scope\":\"actual initializer/reset/team-header/strategy/candidate owners on same retained memory; inverse lineup remains preceding cleared state until typed player binding is replaced; no advancing match\",\"completed\":true,\"same_parent_memory\":true,\"call_pc\":"<<event->pc<<",\"team\":"<<team<<",\"count\":"<<count<<",\"injury_status\":"<<injury<<",\"operations\":"<<p.operations<<",\"reads\":"<<p.reads<<",\"stores\":"<<p.stores<<",\"callbacks\":"<<p.callbacks_completed<<",\"return_value\":"<<p.machine.registers.gpr[2].word<<",\"return_address\":"<<p.machine.registers.gpr[31].word<<",\"sp\":"<<p.machine.registers.gpr[29].word<<",\"hilo_known_masks\":["<<unsigned(p.machine.hi.known_mask)<<','<<unsigned(p.machine.lo.known_mask)<<"]}";
 receipt=o.str();return true;
}
}
